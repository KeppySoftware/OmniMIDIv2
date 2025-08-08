#include "BASSThreadMgr.hpp"
#include "bass/bass.h"
#include <cstdint>
#include <stdexcept>
#include <sys/types.h>

void ThreadFunc(OmniMIDI::BASSThreadManager::ThreadInfo *info);

static size_t calc_render_size(uint32_t sample_rate, uint32_t buffer_ms) {
  float val = (float)sample_rate * (float)buffer_ms / 1000.0;
  val = fmax(val, 1.0);
  return (size_t)val;
}

OmniMIDI::BASSInstance::BASSInstance(BASSSettings *bassConfig,
                                     uint32_t channels, bool decoding) {
  DWORD flags = BASS_MIDI_DECAYEND | BASS_SAMPLE_FLOAT;

  if (decoding)
    flags |= BASS_STREAM_DECODE;
  if (bassConfig->DisableEffects)
    flags |= BASS_MIDI_NOFX;
  if (bassConfig->FollowOverlaps)
    flags |= BASS_MIDI_NOTEOFF1;

  stream = BASS_MIDI_StreamCreate(channels, flags, bassConfig->SampleRate);
  if (stream == 0) {
    throw BASS_ErrorGetCode();
  }

  evbuf = (uint32_t *)malloc(bassConfig->EvBufSize * sizeof(uint32_t));
  if (!evbuf) {
    throw std::runtime_error("");
  }
  evbuf_len = 0;
  evbuf_capacity = bassConfig->EvBufSize;

  BASS_ChannelSetAttribute(stream, BASS_ATTRIB_BUFFER, 0);
  BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_VOICES,
                           (float)bassConfig->VoiceLimit);

  BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_SRC, 0.0);

  BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_KILL, 1.0);

  BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_CPU,
                           (float)bassConfig->RenderTimeLimit);
}

OmniMIDI::BASSInstance::~BASSInstance() {
  free(evbuf);
  BASS_StreamFree(stream);
}

void OmniMIDI::BASSInstance::SendEvent(uint32_t event) {
  if (evbuf_len == evbuf_capacity)
    FlushEvents();

  evbuf[evbuf_len++] = event;
}

int OmniMIDI::BASSInstance::ReadSamples(float *buffer, size_t num_samples) {
  FlushEvents();
  return BASS_ChannelGetData(stream, buffer, num_samples * sizeof(float));
}

uint64_t OmniMIDI::BASSInstance::GetVoiceCount() {
  float val;
  BASS_ChannelGetAttribute(stream, BASS_ATTRIB_MIDI_VOICES_ACTIVE, &val);
  return (uint64_t)val;
}

float OmniMIDI::BASSInstance::GetRenderingTime() {
  float val;
  BASS_ChannelGetAttribute(stream, BASS_ATTRIB_CPU, &val);
  return val;
}

int OmniMIDI::BASSInstance::SetSoundFonts(
    const std::vector<BASS_MIDI_FONTEX> &sfs) {
  return BASS_MIDI_StreamSetFonts(stream, sfs.data(),
                                  (uint32_t)sfs.size() | BASS_MIDI_FONT_EX);
}

void OmniMIDI::BASSInstance::SetDrums(bool isDrumsChan) {
  BASS_MIDI_StreamEvent(stream, 0, MIDI_EVENT_DEFDRUMS, (float)isDrumsChan);
}

void OmniMIDI::BASSInstance::ResetStream() {
  BASS_MIDI_StreamEvent(stream, 0, MIDI_EVENT_SYSTEM, MIDI_SYSTEM_DEFAULT);
}

void OmniMIDI::BASSInstance::FlushEvents() {
  BASS_MIDI_StreamEvents(stream,
                         BASS_MIDI_EVENTS_RAW | BASS_MIDI_EVENTS_NORSTATUS,
                         evbuf, evbuf_len * sizeof(uint32_t));
  evbuf_len = 0;
}

OmniMIDI::BASSThreadManager::BASSThreadManager(ErrorSystem::Logger *PErr,
                                               BASSSettings *bassConfig) {
  ErrLog = PErr;

  Message("Starting.");

  uint32_t sample_rate = audio_player.GetSampleRate();
  uint16_t audio_channels = audio_player.GetChannelCount();

  BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);
  if (!BASS_Init(0, sample_rate, 0, NULL, NULL))
    throw std::runtime_error("Cannot start BASS");

  shared.num_threads = bassConfig->ThreadCount;
  shared.num_instances = bassConfig->ExperimentalAudioMultiplier;
  uint32_t buffer_ms = bassConfig->AudioBuf;
  kbdiv = (uint32_t)bassConfig->ExpMTKeyboardDiv;

  uint32_t buffer_len = (uint32_t)((float)sample_rate * (float)audio_channels *
                                   (float)buffer_ms / 1000.0);
  buffer_len = buffer_len < 1 ? 1 : buffer_len;

  shared.instance_buffers =
      (float **)malloc(shared.num_instances * sizeof(float *));

  Message("Creating BASSMIDI instances.");

  shared.instances = new BASSInstance *[shared.num_instances];
  shared.nps_insts = new NpsInstance[shared.num_instances];

  for (uint32_t i = 0; i < shared.num_instances; i++) {
    shared.instances[i] = new BASSInstance(bassConfig, 1, true);

    NpsInstance *newnpsi = &shared.nps_insts[i];
    newnpsi->nps = new NpsLimiter();
    newnpsi->max_nps = bassConfig->MaxInstanceNPS;
    memset(newnpsi->skipped_notes_, 0, sizeof(newnpsi->skipped_notes_));

    shared.instance_buffers[i] = (float *)malloc(buffer_len * sizeof(float));
  }

  Message("Creating threads.");

  shared.should_exit = 0;
  shared.work_in_progress = 0;
  shared.active_thread_count = 0;

  threads = new ThreadInfo[shared.num_threads];
  shared.thread_is_working = new uint8_t[shared.num_threads];

  for (uint32_t i = 0; i < shared.num_threads; i++) {
    ThreadInfo *t = &threads[i];
    t->thread_idx = i;
    t->shared = &shared;

    t->thread = std::jthread(ThreadFunc, t);
  }

  for (uint32_t i = 0; i < kbdiv; i++) {
    shared.instances[(9 * kbdiv) + i]->SetDrums(true);
  }

  Message("Creating renderer.");

  AudioStreamParams stream_params{sample_rate, audio_channels};
  size_t render_size = calc_render_size(sample_rate, buffer_ms);
  auto render_func = [self = this](std::vector<float> &buffer) mutable {
    self->ReadSamples(buffer.data(), buffer.size());
  };
  buffered = new BufferedRenderer(render_func, stream_params, render_size);

  Message("Creating audio player.");

  auto audio_pipe = [self = buffered](std::vector<float> &buffer) mutable {
    self->read(buffer);
  };
  audio_player.SetUpStream(audio_pipe, render_size, bassConfig->LoudMax);
  audio_player.Start();

  Message("Finished.");
}

OmniMIDI::BASSThreadManager::~BASSThreadManager() {
  audio_player.Stop();

  {
    std::unique_lock<std::mutex> lck(shared.mutex);

    shared.should_exit = 1;
    for (uint32_t i = 0; i < shared.num_threads; i++)
      shared.thread_is_working[i] = 1;

    shared.work_available.notify_all();
  }

  for (uint32_t i = 0; i < shared.num_threads; i++) {
    threads[i].thread.join();
  }

  for (uint32_t i = 0; i < shared.num_instances; i++) {
    delete shared.instances[i];
    delete shared.nps_insts[i].nps;
    free(shared.instance_buffers[i]);
  }

  delete shared.instances;
  delete shared.nps_insts;

  free(shared.instance_buffers);
  delete shared.thread_is_working;
  delete buffered;

  // delete threads;

  BASS_Free();
}

void OmniMIDI::BASSThreadManager::SendEvent(uint32_t event) {
  uint32_t head = event & 0xFF;
  uint32_t channel = head & 0xF;
  uint32_t code = head >> 4;
  uint32_t ev;

  switch (code) {
  case 0x8: {
    ev = event & 0xFFFFF0;
    uint32_t key = (ev >> 8) & 0xFF;
    uint32_t idx = (channel * kbdiv) + (key % kbdiv);

    NpsInstance *npsi = &shared.nps_insts[idx];

    if (npsi->skipped_notes_[channel * 128 + key] > 0) {
      npsi->skipped_notes_[channel * 128 + key]--;
    } else {
      ev = event & 0xFFFFF0;
      BASSInstance *instance = shared.instances[idx];
      instance->SendEvent(ev);
    }
    break;
  }

  case 0x9: {
    ev = event & 0xFFFFF0;
    uint32_t key = (ev >> 8) & 0xFF;
    uint32_t vel = (ev >> 16) & 0xFF;
    uint32_t idx = (channel * kbdiv) + (key % kbdiv);

    NpsInstance *npsi = &shared.nps_insts[idx];
    uint64_t curr_nps = npsi->nps->calculate_nps();

    if (NpsLimiter::should_send_for_vel_and_nps(vel, curr_nps, npsi->max_nps)) {
      BASSInstance *instance = shared.instances[idx];
      instance->SendEvent(ev);
      npsi->nps->add_note();
    } else {
      npsi->skipped_notes_[channel * 128 + key]++;
    }

    break;
  }

  case 0xF: {
    for (uint32_t i = 0; i < channel * kbdiv; i++) {
      BASSInstance *instance = shared.instances[i];
      NpsInstance *npsi = &shared.nps_insts[i];

      instance->SendEvent(event);

      if (event == 0xFF) {
        memset(npsi->skipped_notes_, 0, sizeof(npsi->skipped_notes_));
      }
    }

    break;
  }

  default: {
    ev = event & 0xFFFFF0;
    for (uint32_t i = 0; i < kbdiv; i++) {
      uint32_t idx = (channel * kbdiv) + i;
      BASSInstance *instance = shared.instances[idx];
      instance->SendEvent(ev);
    }
  }
  }
}

void OmniMIDI::BASSThreadManager::ReadSamples(float *buffer,
                                              size_t num_samples) {
  {
    std::unique_lock<std::mutex> lck(shared.mutex);

    shared.active_voices = 0;
    shared.render_time = 0.0;

    shared.should_exit = 0;
    shared.num_samples = num_samples;
    shared.active_thread_count = shared.num_threads;
    shared.work_in_progress = 1;
    for (uint32_t i = 0; i < shared.num_threads; i++)
      shared.thread_is_working[i] = 1;

    shared.work_available.notify_all();
    shared.work_done.wait(lck, [&] { return !shared.work_in_progress; });
  }

  memset(buffer, 0, num_samples * sizeof(float));
  for (uint32_t i = 0; i < shared.num_instances; i++) {
    float *curr = shared.instance_buffers[i];
    for (uint32_t s = 0; s < num_samples; s++) {
      buffer[s] += curr[s];
    }
  }

  ActiveVoices = shared.active_voices;
  RenderTime = shared.render_time / (float)shared.num_instances;
}

int OmniMIDI::BASSThreadManager::SetSoundFonts(
    const std::vector<BASS_MIDI_FONTEX> &sfs) {
  for (uint32_t i = 0; i < shared.num_instances; i++) {
    int err = shared.instances[i]->SetSoundFonts(sfs);
    if (err < 0) {
      return BASS_ErrorGetCode();
    }
  }

  return 0;
}

uint64_t OmniMIDI::BASSThreadManager::GetActiveVoices() { return ActiveVoices; }

float OmniMIDI::BASSThreadManager::GetRenderingTime() { return RenderTime; }

void ThreadFunc(OmniMIDI::BASSThreadManager::ThreadInfo *info) {
  using namespace OmniMIDI;

  OmniMIDI::BASSThreadManager::ThreadSharedInfo *shared = info->shared;
  uint32_t thread_idx = info->thread_idx;

  while (1) {
    {
      std::unique_lock<std::mutex> lck(shared->mutex);
      shared->work_available.wait(lck, [&] {
        return (shared->work_in_progress &&
                shared->thread_is_working[thread_idx]) ||
               shared->should_exit;
      });

      if (shared->should_exit) {
        break;
      }
    }

    uint64_t thread_active_voices = 0;
    float thread_render_time = 0.0;

    for (uint32_t i = thread_idx; i < shared->num_instances;
         i += shared->num_threads) {
      BASSInstance *instance = shared->instances[i];

      memset(shared->instance_buffers[i], 0,
             shared->num_samples * sizeof(float));
      int a = instance->ReadSamples(shared->instance_buffers[i],
                                    shared->num_samples);
      if (a < 0) {
        printf("RE%d\n", BASS_ErrorGetCode());
      }
      thread_active_voices += instance->GetVoiceCount();
      thread_render_time += instance->GetRenderingTime();
    }

    {
      std::unique_lock<std::mutex> lck(shared->mutex);
      shared->thread_is_working[thread_idx] = 0;
      shared->active_voices += thread_active_voices;
      shared->render_time += thread_render_time;
      if (--shared->active_thread_count == 0) {
        shared->work_in_progress = 0;
        shared->work_done.notify_one();
      }
    }
  }
}