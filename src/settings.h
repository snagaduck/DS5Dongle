//
// settings.h — single place to change before a custom build.
// All user-facing defaults and BT init values live here.
//

#ifndef DS5_BRIDGE_SETTINGS_H
#define DS5_BRIDGE_SETTINGS_H

#include <cstdint>

// ─── Default config values ─────────────────────────────────────────────────
// Applied when flash config is absent or a field fails validation.
// Range constraints are enforced by config_valid(); values here must satisfy them.

constexpr float   DEFAULT_HAPTICS_GAIN       = 1.0f;   // [1.0, 2.0]
constexpr float   DEFAULT_SPEAKER_VOLUME     = -100.0f; // [-100, 0] dB (−100 = silent)
constexpr uint8_t DEFAULT_INACTIVE_TIME      = 30;      // [5, 60] minutes before auto-disconnect
constexpr uint8_t DEFAULT_DISABLE_DISCONNECT = 0;       // 0 = auto-disconnect active
constexpr uint8_t DEFAULT_DISABLE_PICO_LED   = 0;       // 0 = Pico LED on when connected
constexpr uint8_t DEFAULT_POLLING_RATE       = 0;       // 0 = 250 Hz, 1 = 500 Hz, 2 = real-time
constexpr uint8_t DEFAULT_AUDIO_BUFFER       = 64;      // [16, 128] haptic buffer length
constexpr uint8_t DEFAULT_CONTROLLER_MODE    = 2;       // 0 = DS5, 1 = DSE, 2 = Auto

// ─── BT init packet values ─────────────────────────────────────────────────
// Sent to the controller once on every BT connection.

// Lightbar color (RGB, 0x00–0xFF each)
constexpr uint8_t BT_LED_R = 0xFF;
constexpr uint8_t BT_LED_G = 0xD7;
constexpr uint8_t BT_LED_B = 0x00; // #FFD700 Nijika Yellow

// Audio volumes sent to controller hardware (0x00–0x7F)
constexpr uint8_t BT_VOLUME_HEADPHONES = 0x7F; // max
constexpr uint8_t BT_VOLUME_SPEAKER    = 0x7F; // max
constexpr uint8_t BT_VOLUME_MIC        = 0xFF; // clamped by firmware

#endif // DS5_BRIDGE_SETTINGS_H
