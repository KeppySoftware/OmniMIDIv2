/*
 * SPDX-License-Identifier: MIT
 *
 * OmniMIDI
 *
 * Copyright (c) 2024 Keppy's Software
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the MIT License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * MIT License for more details.
 *
 * You should have received a copy of the MIT License along with this
 * program.  If not, see <https://opensource.org/license/mit/>.
 */

#include "Limiter.hpp"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Helper function to calculate smoothing coefficients from time in ms.
static float calculate_coeff(float time_ms, float sample_rate) {
    if (time_ms <= 0.0f)
        return 1.0f;
    return expf(-1.0f / (time_ms * 0.001f * sample_rate));
}

int compressor_create(Compressor *limiter, float sample_rate, float threshold,
                      float ratio, float attack_ms, float release_ms,
                      float lookahead_ms) {
    if (sample_rate <= 0) {
        return 0;
    }

    if (!limiter) {
        return 0;
    }

    // --- Set user parameters ---
    limiter->threshold = threshold;
    limiter->ratio = ratio;
    limiter->attack_ms = attack_ms;
    limiter->release_ms = release_ms;
    limiter->lookahead_ms = lookahead_ms;

    // --- Calculate internal parameters ---
    limiter->attack_coeff = calculate_coeff(attack_ms, sample_rate);
    limiter->release_coeff = calculate_coeff(release_ms, sample_rate);
    limiter->envelope = 0.0f;
    limiter->gain = 1.0f;

    // --- Initialize lookahead buffer ---
    limiter->buffer_size = (size_t)(lookahead_ms * 0.001f * sample_rate);
    if (limiter->buffer_size < 1) {
        limiter->buffer_size = 1;
    }

    limiter->delay_buffer =
        (float *)malloc(limiter->buffer_size * sizeof(float));
    if (!limiter->delay_buffer) {
        perror("Failed to allocate memory for delay buffer");
        free(limiter);
        return 0;
    }

    limiter->write_index = 0;
    limiter->read_index = 1;

    return 1;
}

void compressor_destroy(Compressor *limiter) {
    if (limiter) {
        if (limiter->delay_buffer) {
            free(limiter->delay_buffer);
        }
    }
}

float compressor_process(Compressor *limiter, float input) {
    if (!limiter)
        return 0.0;

    float output;

    // --- Lookahead Delay Line ---
    limiter->delay_buffer[limiter->write_index] = input;
    float delayed_input = limiter->delay_buffer[limiter->read_index];
    float lookahead_sample = input;

    // --- Envelope Detection ---
    float rectified_sample = fabsf(lookahead_sample);
    if (rectified_sample > limiter->envelope) {
        limiter->envelope = limiter->attack_coeff * limiter->envelope +
                            (1.0f - limiter->attack_coeff) * rectified_sample;
    } else {
        limiter->envelope = limiter->release_coeff * limiter->envelope +
                            (1.0f - limiter->release_coeff) * rectified_sample;
    }

    // --- Gain Computation ---
    // Calculate the desired gain reduction based on the smoothed envelope.
    float target_gain = 1.0f;
    if (limiter->envelope > limiter->threshold) {
        target_gain =
            (limiter->threshold +
             (limiter->envelope - limiter->threshold) / limiter->ratio) /
            limiter->envelope;
    }

    // --- Gain Application ---
    if (target_gain < limiter->gain) {
        limiter->gain = target_gain; // Instant attack
    } else {
        // Smooth release
        limiter->gain = limiter->release_coeff * limiter->gain +
                        (1.0f - limiter->release_coeff) * target_gain;
    }

    // --- Apply Gain ---
    output = delayed_input * limiter->gain;

    // --- Update Buffer Indices ---
    limiter->write_index = (limiter->write_index + 1) % limiter->buffer_size;
    limiter->read_index = (limiter->read_index + 1) % limiter->buffer_size;

    return output;
}
