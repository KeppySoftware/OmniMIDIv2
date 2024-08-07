/*

    OmniMIDI v15+ (Rewrite) for Windows NT

    This file contains the required code to run the driver under Windows 7 SP1 and later.
    This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef KSYNTH_H
#define KSYNTH_H

struct KSynth {
	struct Sample** samples;
	unsigned int sample_rate;
	unsigned char num_channel;
	unsigned long polyphony;
	unsigned char polyphony_per_channel[16];
	unsigned long max_polyphony;
	float rendering_time;
	struct Chan* channels;
	struct Voice** voices;
};

extern void (*ksynth_free_samples)(struct Sample** samples, unsigned long count);
extern struct Sample** (*ksynth_allocate_samples)(const char* path, unsigned char keys, int sample_rate);
extern struct KSynth* (*ksynth_new)(const char* ksmpf, unsigned int sample_rate, unsigned int num_channel, unsigned long max_polyphony);
extern void (*ksynth_note_on)(struct KSynth* ksynth_instance, unsigned char channel, unsigned char note, unsigned char velocity);
extern void (*ksynth_note_off)(struct KSynth* ksynth_instance, unsigned char channel, unsigned char note);
extern void (*ksynth_cc)(struct KSynth* ksynth_instance, unsigned char channel, unsigned char param1, unsigned char param2);
extern void (*ksynth_note_off_all)(struct KSynth* ksynth_instance);
extern void (*ksynth_fill_buffer)(struct KSynth* ksynth_instance, float* buffer, unsigned int buffer_size);
extern void (*ksynth_buffer_free)(float* buffer);
extern void (*ksynth_free)(struct KSynth* ksynth_instance);

#endif