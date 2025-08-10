#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <portaudio.h>

namespace OmniMIDI {
class AudioLimiter {
private:
  struct SingleChannelLimiter {
    float loudness;
    float attack;
    float falloff;
    float strength;
    float min_thres;

    SingleChannelLimiter() {
      loudness = 1.0;
      attack = 100.0;
      falloff = 16000.0;
      strength = 1.0;
      min_thres = 1.0;
    }

    float process(float sample) {
      float sabs = std::abs(sample);
      if (loudness > sabs) {
        loudness = (loudness * falloff + sabs) / (falloff + 1.0);
      } else {
        loudness = (loudness * attack + sabs) / (attack + 1.0);
      }

      if (loudness < min_thres) {
        loudness = min_thres;
      }

      return sample / (loudness * strength + 2.0 * (1.0 - strength)) / 2.0;
    }
  };

  std::vector<SingleChannelLimiter *> limiters;
  uint16_t num_channels;
public:
  AudioLimiter(uint16_t channels);
  ~AudioLimiter();
  void process(std::vector<float>&samples);
};

class MIDIAudioPlayer {
public:
  using AudioPipe = std::function<void(std::vector<float>&)>;

  struct AudioPlayerArgument {
    AudioPipe audio_pipe;
    AudioLimiter *limiter;
  };

  MIDIAudioPlayer();
  ~MIDIAudioPlayer();

  void SetUpStream(AudioPipe audio_pipe, size_t render_size, bool limiter);

  void SetSampleRate(uint32_t sampleRate);
  uint32_t GetSampleRate();
  uint16_t GetChannelCount();

  void Start();
  void Stop();

private:
  PaStreamParameters outputParameters;
  PaStream *stream = nullptr;
  uint32_t sample_rate;
  uint16_t channels;
  AudioLimiter *limiter = nullptr;
  AudioPlayerArgument arg;
};
} // namespace OmniMIDI

#endif