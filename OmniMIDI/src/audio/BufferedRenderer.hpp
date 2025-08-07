#ifndef BUFFERED_RENDERER_H
#define BUFFERED_RENDERER_H

#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <deque>
#include <shared_mutex>
#include <memory>

#include "../Common.hpp"
#include "../ThreadUtils.hpp"

// Forward declarations
class BufferedRenderer;
class BufferedRendererStatsReader;

// Holds the statistics, using shared pointers to allow shared ownership
// between the renderer, the reader, and the rendering thread.
struct BufferedRendererStats {
    std::shared_ptr<std::atomic<int64_t>> samples;
    std::shared_ptr<std::atomic<int64_t>> last_samples_after_read;
    std::shared_ptr<std::atomic<int64_t>> last_request_samples;
    std::shared_ptr<std::deque<double>> render_time_queue;
    std::shared_ptr<std::shared_mutex> render_time_mutex;
    std::shared_ptr<std::atomic<size_t>> render_size;
};

// Reads the statistics of a BufferedRenderer instance in a thread-safe way.
class BufferedRendererStatsReader {
public:
    explicit BufferedRendererStatsReader(BufferedRendererStats stats);

    // The number of samples currently buffered.
    int64_t samples() const;

    // The number of samples that were in the buffer after the last read.
    int64_t last_samples_after_read() const;

    // The last number of samples requested by the read command.
    int64_t last_request_samples() const;

    // The number of samples to render each iteration.
    size_t render_size() const;
    
    // The average load of the renderer (0.0 to 1.0).
    double average_renderer_load() const;

    // The most recent renderer load (0.0 to 1.0).
    double last_renderer_load() const;

private:
    BufferedRendererStats stats_;
};

// The main helper class for deferred sample rendering.
class BufferedRenderer {
public:
    // Represents the audio processing pipeline.
    using AudioPipe = std::function<void(std::vector<float>&)>;

    BufferedRenderer(AudioPipe render_function, AudioStreamParams params, size_t initial_render_size);
    
    // Destructor ensures the rendering thread is safely stopped and joined.
    ~BufferedRenderer();

    // Disable copy and move semantics to prevent issues with thread ownership.
    BufferedRenderer(const BufferedRenderer&) = delete;
    BufferedRenderer& operator=(const BufferedRenderer&) = delete;
    BufferedRenderer(BufferedRenderer&&) = delete;
    BufferedRenderer& operator=(BufferedRenderer&&) = delete;

    // Reads samples from the buffer into the destination vector.
    void read(std::vector<float>& dest);

    // Sets the number of samples to be rendered in each iteration of the render loop.
    void set_render_size(size_t size);

    // Returns a statistics reader.
    BufferedRendererStatsReader get_buffer_stats() const;

private:
    // The function that runs on the dedicated rendering thread.
    void render_loop();

    BufferedRendererStats stats_;
    ThreadSafeQueue<std::vector<float>> receive_queue_;
    std::vector<float> remainder_;
    std::shared_ptr<std::atomic<bool>> killed_;
    std::thread render_thread_;
    AudioStreamParams stream_params_;
    AudioPipe audio_pipe_;
};

#endif // BUFFERED_RENDERER_H