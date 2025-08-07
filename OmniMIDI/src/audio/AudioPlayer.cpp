#include "AudioPlayer.hpp"
#include <portaudio.h>
#include <stdexcept>
#include <vector>

using namespace OmniMIDI;

static int paCallback(const void *inputBuffer, void *outputBuffer,
                      unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo *timeInfo,
                      PaStreamCallbackFlags statusFlags, void *userData) {
  OmniMIDI::MIDIAudioPlayer::AudioPlayerArgument *argument = (OmniMIDI::MIDIAudioPlayer::AudioPlayerArgument *)userData;
  OmniMIDI::MIDIAudioPlayer::AudioPipe audio_pipe = argument->audio_pipe;
  OmniMIDI::AudioLimiter *limiter = argument->limiter;

  float *out = (float *)outputBuffer;
  
  // Todo, *2 is for stereo, we need mono support too
  std::vector<float> outVec(framesPerBuffer * 2);
  audio_pipe(outVec);
  if (limiter) {
    limiter->process(outVec);
  }
  std::copy(outVec.begin(), outVec.end(), out);
  return 0;
}

OmniMIDI::AudioLimiter::AudioLimiter(uint16_t channels) {
  num_channels = channels;
  for (uint16_t i = 0; i < channels; i++) {
    SingleChannelLimiter *limiter = new SingleChannelLimiter();
    limiters.push_back(limiter);
  }
}

OmniMIDI::AudioLimiter::~AudioLimiter() {
  for (uint16_t i = 0; i < num_channels; i++) {
    delete limiters[i];
  }
}

void OmniMIDI::AudioLimiter::process(std::vector<float>&samples) {
  for (size_t i = 0; i < samples.size(); i++) {
    samples[i] = limiters[i % num_channels]->process(samples[i]);
  }
}

OmniMIDI::MIDIAudioPlayer::MIDIAudioPlayer() {
  PaError err;

  err = Pa_Initialize();
  if (err != paNoError) {
    throw std::runtime_error(Pa_GetErrorText(err));
  }

  outputParameters.device = Pa_GetDefaultOutputDevice();
  if (outputParameters.device == paNoDevice) {
    throw std::runtime_error("Error: No default output device.");
  }

  const PaDeviceInfo *devInfo = Pa_GetDeviceInfo(outputParameters.device);
  outputParameters.channelCount = devInfo->maxOutputChannels;
  outputParameters.sampleFormat = paFloat32;
  outputParameters.suggestedLatency = devInfo->defaultLowOutputLatency;
  outputParameters.hostApiSpecificStreamInfo = NULL;
  sample_rate = devInfo->defaultSampleRate;
  channels = devInfo->maxOutputChannels;

  limiter = new AudioLimiter(outputParameters.channelCount);
}

OmniMIDI::MIDIAudioPlayer::~MIDIAudioPlayer() {
  if (stream) {
    Stop();
    Pa_CloseStream(stream);
  }

  Pa_Terminate();

  delete limiter;
}

uint32_t OmniMIDI::MIDIAudioPlayer::GetSampleRate() { return sample_rate; }

uint16_t OmniMIDI::MIDIAudioPlayer::GetChannelCount() { return channels; }

void OmniMIDI::MIDIAudioPlayer::SetUpStream(AudioPipe audio_pipe, size_t render_size,
                                            bool limiter) {
  PaError err;

  if (stream) {
    Stop();
    Pa_CloseStream(stream);
  }

  if (limiter)
    arg = {audio_pipe, this->limiter};
  else
    arg = {audio_pipe, NULL};

  err =
      Pa_OpenStream(&stream, NULL, &outputParameters, sample_rate,
                    1024, paClipOff, paCallback, &arg);
  if (err != paNoError) {
    throw std::runtime_error(Pa_GetErrorText(err));
  }
}

void OmniMIDI::MIDIAudioPlayer::Start() {
  if (stream) {
    Pa_StartStream(stream);
  }
}

void OmniMIDI::MIDIAudioPlayer::Stop() {
  if (stream) {
    Pa_StopStream(stream);
  }
}