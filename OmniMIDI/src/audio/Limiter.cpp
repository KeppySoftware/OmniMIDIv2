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
#include <exception>
#include <math.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

// Helper function to calculate smoothing coefficients from time in ms.
static float calculate_coeff(float time_ms, float sample_rate) {
    if (time_ms <= 0.0f)
        return 1.0f;
    return expf(-1.0f / (time_ms * 0.001f * sample_rate));
}

OmniMIDI::Compressor::Compressor(float sample_rate, float threshold,
                                 float ratio, float attack_ms, float release_ms,
                                 float lookahead_ms) {
    if (sample_rate <= 0) {
        throw std::runtime_error("Invalid sample rate");
    }

    // --- Set user parameters ---
    this->threshold = threshold;
    this->ratio = ratio;
    this->attack_ms = attack_ms;
    this->release_ms = release_ms;
    this->lookahead_ms = lookahead_ms;

    // --- Calculate internal parameters ---
    this->attack_coeff = calculate_coeff(attack_ms, sample_rate);
    this->release_coeff = calculate_coeff(release_ms, sample_rate);
    this->envelope = 0.0f;
    this->gain = 1.0f;

    // --- Initialize lookahead buffer ---
    this->buffer_size = (size_t)(lookahead_ms * 0.001f * sample_rate);
    if (this->buffer_size < 1) {
        this->buffer_size = 1;
    }

    this->delay_buffer = (float *)malloc(this->buffer_size * sizeof(float));
    if (!this->delay_buffer) {
        throw std::runtime_error("Failed to allocate memory for delay buffer");
    }

    this->write_index = 0;
    this->read_index = 1;
}

OmniMIDI::Compressor::~Compressor() {
    if (this->delay_buffer) {
        free(this->delay_buffer);
    }
}

float OmniMIDI::Compressor::process(float input) {
    float output;

    // --- Lookahead Delay Line ---
    delay_buffer[write_index] = input;
    float delayed_input = delay_buffer[read_index];
    float lookahead_sample = input;

    // --- Envelope Detection ---
    float rectified_sample = fabsf(lookahead_sample);
    if (rectified_sample > envelope) {
        envelope =
            attack_coeff * envelope + (1.0f - attack_coeff) * rectified_sample;
    } else {
        envelope = release_coeff * envelope +
                   (1.0f - release_coeff) * rectified_sample;
    }

    // --- Gain Computation ---
    // Calculate the desired gain reduction based on the smoothed envelope.
    float target_gain = 1.0f;
    if (envelope > threshold) {
        target_gain = (threshold + (envelope - threshold) / ratio) / envelope;
    }

    // --- Gain Application ---
    if (target_gain < gain) {
        gain = target_gain; // Instant attack
    } else {
        // Smooth release
        gain = release_coeff * gain + (1.0f - release_coeff) * target_gain;
    }

    // --- Apply Gain ---
    output = delayed_input * gain;

    // --- Update Buffer Indices ---
    write_index = (write_index + 1) % buffer_size;
    read_index = (read_index + 1) % buffer_size;

    return output;
}

OmniMIDI::AudioLimiter::AudioLimiter(uint16_t channels, uint32_t sample_rate) {
    num_channels = channels;
    for (uint16_t i = 0; i < channels; i++) {
        try {
            Compressor *c =
                new Compressor(sample_rate, 0.3, 1000.0, 10.0, 50.0, 10.0);
            compressors.push_back(c);
        } catch (std::exception &e) {
            throw std::runtime_error("Error initializing audio limiter");
        }
    }
}

OmniMIDI::AudioLimiter::~AudioLimiter() {
    for (auto c : compressors) {
        delete c;
    }
    compressors.clear();
}

void OmniMIDI::AudioLimiter::process(std::vector<float> &samples) {
    for (size_t i = 0; i < samples.size(); i++) {
        samples[i] = compressors[i % num_channels]->process(samples[i]);
    }
}