#pragma once
#include "resample.h"
#include <cstdint>

// Called after the haptic in_buf is filled from ch3/4, before the resampler.
// raw      : full USB audio frame buffer (all INPUT_CHANNELS interleaved)
// raw_nch  : number of channels in raw (INPUT_CHANNELS = 4)
// raw_nframes: number of frames in raw
// in_buf   : haptic resampler input buffer (OUTPUT_CHANNELS = 2 interleaved)
// nframes  : frames to fill in in_buf
//
// Default implementation is a weak no-op in audio.cpp.
// Rename haptic_filter.cpp.disabled -> haptic_filter.cpp to activate.
// When active, bridges speaker audio (ch1/2) into in_buf and applies a
// 300Hz low-pass filter so only bass/impact reaches the haptic actuators.
void haptic_filter_process(const int16_t* raw, int raw_nframes, int raw_nch,
                           WDL_ResampleSample* in_buf, int nframes, int out_nch);
