#pragma once
#include "resample.h"

// Called on the haptic in_buf after channel extraction, before the resampler.
// Default implementation is a weak no-op in audio.cpp.
// Rename haptic_filter.cpp.disabled -> haptic_filter.cpp to activate.
void haptic_filter_process(WDL_ResampleSample* buf, int nframes, int nch);
