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

#ifndef LIMITER_H
#define LIMITER_H

#include <cstdint>
#include <stddef.h>
#include <vector>

namespace OmniMIDI {

class Compressor {
  public:
    Compressor(float sample_rate, float threshold, float ratio, float attack_ms,
               float release_ms, float lookahead_ms);
    ~Compressor();
    float process(float input);

  private:
    float threshold;    // Threshold in linear amplitude
    float ratio;        // Compression ratio (e.g., 4.0 for 4:1)
    float attack_ms;    // Attack time in milliseconds
    float release_ms;   // Release time in milliseconds
    float lookahead_ms; // Lookahead time in milliseconds

    float attack_coeff;
    float release_coeff;
    float envelope;
    float gain;

    float *delay_buffer;
    size_t buffer_size;
    size_t write_index;
    size_t read_index;
};

class AudioLimiter {
  private:
    std::vector<Compressor *> compressors;
    uint16_t num_channels;

  public:
    AudioLimiter(uint16_t channels, uint32_t sample_rate);
    ~AudioLimiter();
    void process(std::vector<float> &samples);
};

} // namespace OmniMIDI

#endif