/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#include <ksynth.h>

struct KSynth* (*ksynth_new)(const char* ksmpf, unsigned int sample_rate, unsigned int num_channel, unsigned long max_polyphony);
void (*ksynth_note_on)(struct KSynth* ksynth_instance, unsigned char channel, unsigned char note, unsigned char velocity);
void (*ksynth_note_off)(struct KSynth* ksynth_instance, unsigned char channel, unsigned char note);
void (*ksynth_cc)(struct KSynth* ksynth_instance, unsigned char channel, unsigned char param1, unsigned char param2);
void (*ksynth_note_off_all)(struct KSynth* ksynth_instance);
void (*ksynth_fill_buffer)(struct KSynth* ksynth_instance, float* buffer, unsigned int buffer_size);
void (*ksynth_buffer_free)(float* buffer);
void (*ksynth_free)(struct KSynth* ksynth_instance);