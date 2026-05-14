//
// Created by awalol on 2026/3/5.
//

#ifndef DS5_BRIDGE_AUDIO_H
#define DS5_BRIDGE_AUDIO_H

#include <cstdint>

void audio_init();
void audio_loop();
#if !DISABLE_SPEAKER_PROC
void core1_entry();
#endif
void set_headset(bool state);
// Called from the HID output path when a 0x02 report with rumble values is forwarded.
// Suppresses audio haptics for a short window while game rumble is active.
void notify_rumble(uint8_t left, uint8_t right);

#endif //DS5_BRIDGE_AUDIO_H