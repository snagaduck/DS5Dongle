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

constexpr float   DEFAULT_HAPTICS_GAIN       = 1.0f;    // [1.0, 2.0]
constexpr float   DEFAULT_SPEAKER_VOLUME     = -100.0f; // [-100, 0] dB (-100 = silent)
constexpr uint8_t DEFAULT_INACTIVE_TIME      = 30;      // [5, 60] minutes before auto-disconnect
constexpr uint8_t DEFAULT_DISABLE_DISCONNECT = 0;       // 0 = auto-disconnect enabled
constexpr uint8_t DEFAULT_DISABLE_PICO_LED   = 0;       // 0 = Pico LED on when connected
constexpr uint8_t DEFAULT_POLLING_RATE       = 0;       // 0=250Hz, 1=500Hz, 2=real-time
constexpr uint8_t DEFAULT_AUDIO_BUFFER       = 64;      // [16, 128] haptic buffer length
constexpr uint8_t DEFAULT_CONTROLLER_MODE    = 2;       // 0=DS5, 1=DSE, 2=Auto
constexpr uint8_t DEFAULT_HAPTIC_YIELD_RUMBLE = 1;     // 1=mute audio haptics while game rumble is active

// ─── BT init: lightbar ─────────────────────────────────────────────────────
// RGB color applied to the controller lightbar on every BT connect (0x00–0xFF each).
constexpr uint8_t BT_LED_R = 0x00;
constexpr uint8_t BT_LED_G = 0x40;
constexpr uint8_t BT_LED_B = 0xFF; // #0040FF Blue

// Animation played when the LED color is applied on connect:
//   0 = Nothing  (instant, color appears immediately)
//   1 = FadeIn   (fades from black to the set color)
//   2 = FadeOut  (fades from the set color to black — avoid)
constexpr uint8_t BT_LIGHT_FADE = 0x00;

// Lightbar brightness: 0=Bright, 1=Mid, 2=Dim
constexpr uint8_t BT_LIGHT_BRIGHTNESS = 0x00;

// ─── BT init: player indicator LEDs ────────────────────────────────────────
// White LEDs under the touchpad. Standard PS5 layout:
//   0x00 = off
//   0x04 = Player 1 (center LED only)
//   0x06 = Player 2
//   0x15 = Player 3
//   0x1B = Player 4
// Note: Gen 0x04 hardware mirrors PlayerLight1+5 and PlayerLight2+4,
// so only symmetric layouts work on newer controllers.
constexpr uint8_t BT_PLAYER_INDICATORS = 0x04; // Player 1

// ─── BT init: audio volumes ────────────────────────────────────────────────
// Volumes sent to controller audio hardware on connect (0x00–0x7F).
// Headphone and speaker max is 0x7F. Mic effective max is ~0x40 (64).
constexpr uint8_t BT_VOLUME_HEADPHONES = 0x7F;
constexpr uint8_t BT_VOLUME_SPEAKER    = 0x7F;
constexpr uint8_t BT_VOLUME_MIC        = 0x40; // 64 = documented effective max

// ─── BT init: mic DSP ──────────────────────────────────────────────────────
// Controller onboard mic processing. 0=off, 1=on.
constexpr uint8_t BT_ECHO_CANCEL  = 0;
constexpr uint8_t BT_NOISE_CANCEL = 0;

// ─── BT init: speaker DSP ──────────────────────────────────────────────────
// Additional hardware speaker volume boost beyond the main volume setting.
// SpeakerCompPreGain: [0, 7] — 0 = no boost, 7 = maximum boost
constexpr uint8_t BT_SPEAKER_COMP_PREGAIN = 2;
// BeamformingEnable: directional mic filtering. 0=off, 1=on.
constexpr uint8_t BT_BEAMFORMING = 1;

// ─── BT init: mute indicator LED ───────────────────────────────────────────
// Amber mute indicator on the controller (not the lightbar).
// 0=Off, 1=On, 2=Breathing
constexpr uint8_t BT_MUTE_LIGHT = 0x00;

// ─── Controller-type BT init ───────────────────────────────────────────────
// Per-type overrides applied after controller identity (DS5 vs DSE) is confirmed.
// Both inherit the shared values above by default; change here to diverge.

// DS5 (standard DualSense)
constexpr uint8_t BT_DS5_LED_R             = BT_LED_R;
constexpr uint8_t BT_DS5_LED_G             = BT_LED_G;
constexpr uint8_t BT_DS5_LED_B             = BT_LED_B;
constexpr uint8_t BT_DS5_PLAYER_INDICATORS = BT_PLAYER_INDICATORS;
constexpr uint8_t BT_DS5_LIGHT_FADE        = BT_LIGHT_FADE;
constexpr uint8_t BT_DS5_LIGHT_BRIGHTNESS  = BT_LIGHT_BRIGHTNESS;

// DSE (DualSense Edge)
// Gen 0x04 hardware mirrors PlayerLight1+5 and PlayerLight2+4; 0x04 (center
// LED only) is the only layout guaranteed symmetric on all DSE revisions.
constexpr uint8_t BT_DSE_LED_R             = BT_LED_R;
constexpr uint8_t BT_DSE_LED_G             = BT_LED_G;
constexpr uint8_t BT_DSE_LED_B             = BT_LED_B;
constexpr uint8_t BT_DSE_PLAYER_INDICATORS = 0x04; // center LED — symmetric on all DSE hardware
constexpr uint8_t BT_DSE_LIGHT_FADE        = BT_LIGHT_FADE;
constexpr uint8_t BT_DSE_LIGHT_BRIGHTNESS  = BT_LIGHT_BRIGHTNESS;

#endif // DS5_BRIDGE_SETTINGS_H
