#include "BufferedRenderer.hpp"
#include <numeric>
#include <chrono>

BufferedRendererStatsReader::BufferedRendererStatsReader(BufferedRendererStats stats)
    : stats_(std::move(stats)) {}

int64_t BufferedRendererStatsReader::samples() const {
    return stats_.samples->load(std::memory_order_relaxed);
}

int64_t BufferedRendererStatsReader::last_samples_after_read() const {
    return stats_.last_samples_after_read->load(std::memory_order_relaxed);
}

int64_t BufferedRendererStatsReader::last_request_samples() const {
    return stats_.last_request_samples->load(std::memory_order_relaxed);
}

size_t BufferedRendererStatsReader::render_size() const {
    return stats_.render_size->load(std::memory_order_relaxed);
}

double BufferedRendererStatsReader::average_renderer_load() const {
    std::shared_lock<std::shared_mutex> lock(*stats_.render_time_mutex);
    if (stats_.render_time_queue->empty()) {
        return 0.0;
    }
    double sum = std::accumulate(stats_.render_time_queue->begin(), stats_.render_time_queue->end(), 0.0);
    return sum / stats_.render_time_queue->size();
}

double BufferedRendererStatsReader::last_renderer_load() const {
    std::shared_lock<std::shared_mutex> lock(*stats_.render_time_mutex);
    if (stats_.render_time_queue->empty()) {
        return 0.0;
    }
    return stats_.render_time_queue->front();
}


// --- BufferedRenderer Implementation ---

BufferedRenderer::BufferedRenderer(AudioPipe render_function, AudioStreamParams params, size_t initial_render_size)
    : stream_params_(params), audio_pipe_(std::move(render_function)) {
    
    // Initialize the shared statistics
    stats_.samples = std::make_shared<std::atomic<int64_t>>(0);
    stats_.last_samples_after_read = std::make_shared<std::atomic<int64_t>>(0);
    stats_.last_request_samples = std::make_shared<std::atomic<int64_t>>(0);
    stats_.render_size = std::make_shared<std::atomic<size_t>>(initial_render_size);
    stats_.render_time_queue = std::make_shared<std::deque<double>>();
    stats_.render_time_mutex = std::make_shared<std::shared_mutex>();
    
    killed_ = std::make_shared<std::atomic<bool>>(false);

    // Start the rendering thread
    render_thread_ = std::thread(&BufferedRenderer::render_loop, this);
}

BufferedRenderer::~BufferedRenderer() {
    // Signal the thread to stop and wait for it to finish.
    killed_->store(true, std::memory_order_relaxed);
    if (render_thread_.joinable()) {
        render_thread_.join();
    }
}

void BufferedRenderer::read(std::vector<float>& dest) {
    std::fill(dest.begin(), dest.end(), 0.0f);

    stats_.samples->fetch_sub(dest.size(), std::memory_order_seq_cst);
    stats_.last_request_samples->store(dest.size(), std::memory_order_seq_cst);

    size_t filled_count = 0;

    // 1. Read from the remainder buffer first
    size_t to_copy_from_remainder = std::min(dest.size(), remainder_.size());
    if (to_copy_from_remainder > 0) {
        std::copy(remainder_.begin(), remainder_.begin() + to_copy_from_remainder, dest.begin());
        remainder_.erase(remainder_.begin(), remainder_.begin() + to_copy_from_remainder);
        filled_count += to_copy_from_remainder;
    }

    // 2. Read from the queue until the destination is full
    while (filled_count < dest.size()) {
        std::vector<float> received_buf = receive_queue_.pop();
        
        size_t needed = dest.size() - filled_count;
        size_t to_copy_from_new = std::min(needed, received_buf.size());

        std::copy(received_buf.begin(), received_buf.begin() + to_copy_from_new, dest.begin() + filled_count);
        filled_count += to_copy_from_new;

        // If there are leftover samples, save them as the new remainder
        if (to_copy_from_new < received_buf.size()) {
            remainder_.assign(received_buf.begin() + to_copy_from_new, received_buf.end());
        }
    }

    stats_.last_samples_after_read->store(stats_.samples->load(), std::memory_order_relaxed);
}

void BufferedRenderer::set_render_size(size_t size) {
    stats_.render_size->store(size, std::memory_order_seq_cst);
}

BufferedRendererStatsReader BufferedRenderer::get_buffer_stats() const {
    return BufferedRendererStatsReader(stats_);
}

void BufferedRenderer::render_loop() {
    while (!killed_->load(std::memory_order_relaxed)) {
        size_t size = stats_.render_size->load(std::memory_order_seq_cst);
        if (size == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // The expected render time per iteration, with a 10% buffer.
        auto delay_micros = (1000000LL * size / stream_params_.sample_rate) * 90 / 100;
        auto delay = std::chrono::microseconds(delay_micros);

        // If the render thread is too far ahead, wait a bit.
        while (!killed_->load(std::memory_order_relaxed)) {
            auto current_samples = stats_.samples->load(std::memory_order_seq_cst);
            auto last_requested = stats_.last_request_samples->load(std::memory_order_seq_cst);
            if (current_samples > last_requested * 110 / 100) {
                std::this_thread::sleep_for(delay / 10);
            } else {
                break;
            }
        }
        
        if (killed_->load(std::memory_order_relaxed)) break;

        auto start_time = std::chrono::steady_clock::now();
        
        // Create the buffer and render samples into it
        std::vector<float> buffer(size * stream_params_.channels, 0.0f);
        audio_pipe_(buffer);

        // Send the rendered samples to the main thread
        stats_.samples->fetch_add(buffer.size(), std::memory_order_seq_cst);
        receive_queue_.push(std::move(buffer));

        // Record the render time statistic
        {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            double elapsed_f64 = std::chrono::duration<double>(elapsed).count();
            double total_f64 = std::chrono::duration<double>(delay).count();
            
            std::unique_lock<std::shared_mutex> lock(*stats_.render_time_mutex);
            stats_.render_time_queue->push_front(elapsed_f64 / total_f64);
            if (stats_.render_time_queue->size() > 100) {
                stats_.render_time_queue->pop_back();
            }
        }

        // Sleep until the next iteration is due
        auto end_time = start_time + delay;
        auto now = std::chrono::steady_clock::now();
        if (end_time > now) {
            std::this_thread::sleep_for(end_time - now);
        }
    }
}
