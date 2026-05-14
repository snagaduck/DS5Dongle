//
// Created by awalol on 2026/3/5.
//

#include "audio.h"
#include "bt.h"
#include "resample.h"
#include "haptic_filter.h"
#include "tusb.h"
#include "opus.h"
#include "utils.h"
#include "config.h"
#include "usb.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

__attribute__((weak)) void haptic_filter_process(const int16_t*, int, int, WDL_ResampleSample*, int, int) {}

#define INPUT_CHANNELS    4
#define OUTPUT_CHANNELS   2
#define SAMPLE_SIZE       64
#define REPORT_SIZE       398
#define REPORT_ID         0x36
// #define VOLUME_GAIN       2
// #define BUFFER_LENGTH     48

using std::clamp;
using std::max;

static WDL_Resampler resampler;
static volatile bool core1_healthy = false;
static uint8_t reportSeqCounter = 0;
static uint8_t packetCounter = 0;
static bool plug_headset = false;
alignas(8) static uint32_t audio_core1_stack[8192];
queue_t audio_fifo;
static uint8_t opus_buf[200];
critical_section_t opus_cs;

struct audio_raw_element {
    float data[512 * 2];
};

void set_headset(bool state) {
    plug_headset = state;
}

void audio_loop() {
    // 1. read USB audio data
    if (!tud_audio_available()) return;

    int16_t raw[192];
    uint32_t bytes_read = tud_audio_read(raw, sizeof(raw)); // 384 bytes per call
    int frames = bytes_read / (INPUT_CHANNELS * sizeof(int16_t));
    if (frames == 0) {
        return;
    }

    static float audio_buf[512 * 2];
    static uint audio_buf_pos = 0;
    // 2. extract ch3/ch4 from 4-ch input and convert to float for the resampler
    WDL_ResampleSample *in_buf;
    int nframes = resampler.ResamplePrepare(frames, OUTPUT_CHANNELS, &in_buf);

    static uint8_t last_mute = 0xFF;
    static float last_vol = 0.0f;
    static float audio_gain = 0.0f;
    const uint8_t cur_mute = mute[0];
    const float cur_vol = get_config().speaker_volume;
    if (cur_mute != last_mute || cur_vol != last_vol) {
        last_mute = cur_mute;
        last_vol = cur_vol;
        audio_gain = cur_mute ? 0.0f : powf(10.0f, cur_vol / 20.0f);
    }
    const float haptics_gain = get_config().haptics_gain;
    for (int i = 0; i < nframes; i++) {
 #if !DISABLE_SPEAKER_PROC       
        audio_buf[audio_buf_pos++] = raw[i * INPUT_CHANNELS] / 32768.0f * audio_gain;
        audio_buf[audio_buf_pos++] = raw[i * INPUT_CHANNELS + 1] / 32768.0f * audio_gain;
        if (audio_buf_pos == 512 * 2) {
            static audio_raw_element element{};
            memcpy(element.data, audio_buf, 512 * 2 * 4);
            if (queue_is_full(&audio_fifo)) {
                static audio_raw_element discard{};
                queue_try_remove(&audio_fifo, &discard);
                static uint32_t drop_count = 0;
                if ((++drop_count % 500) == 1)
                    printf("[Audio] Warning: FIFO overflow (total drops: %lu)\n", drop_count);
            }
            if (!queue_try_add(&audio_fifo, &element)) {
                printf("[Audio] Warning: audio_fifo add failed\n");
            }
            audio_buf_pos = 0;
        }
#endif
        in_buf[i * 2] = static_cast<WDL_ResampleSample>(clamp(raw[i * INPUT_CHANNELS + 2] / 32768.0f * haptics_gain,
                                                              -1.0f, 1.0f));
        in_buf[i * 2 + 1] = static_cast<WDL_ResampleSample>(clamp(raw[i * INPUT_CHANNELS + 3] / 32768.0f * haptics_gain,
                                                                  -1.0f, 1.0f));
    }

    haptic_filter_process(raw, frames, INPUT_CHANNELS, in_buf, nframes, OUTPUT_CHANNELS);

    // 3. resample 48kHz -> 3kHz (16:1 ratio)
    static WDL_ResampleSample out_buf[SAMPLE_SIZE]; // 64 floats = 32 frames × 2ch
    const int out_frames = resampler.ResampleOut(out_buf, nframes, nframes / 16, OUTPUT_CHANNELS);

    static int8_t haptic_buf[SAMPLE_SIZE];
    static int haptic_buf_pos = 0;

    // 4. convert to int8 and accumulate; emit a packet once the buffer is full
    for (int i = 0; i < out_frames; i++) {
        int val_l = static_cast<int>(out_buf[i * 2] * 127.0f);
        int val_r = static_cast<int>(out_buf[i * 2 + 1] * 127.0f);
        haptic_buf[haptic_buf_pos++] = (int8_t) clamp(val_l, -128, 127);
        haptic_buf[haptic_buf_pos++] = (int8_t) clamp(val_r, -128, 127);

        if (haptic_buf_pos != SAMPLE_SIZE) {
            continue;
        }
        // Silence gate: treat haptic payload as silent when all samples are
        // within ±1/127 (~0.8%) to filter USB audio dithering noise.
        // Without this, the constant 0x36 stream (started by our USB audio
        // enumeration fix) overrides HID haptic commands (0x31) from games
        // even when no haptic audio is routed to the device.
        // DISABLE_SPEAKER_PROC: drop the packet so controller-native haptics fire.
        // Full build: send with haptic length = 0 so the controller skips the
        // haptic payload but still receives the Opus speaker audio.
        bool haptic_silent = true;
        for (int j = 0; j < SAMPLE_SIZE && haptic_silent; j++)
            haptic_silent = (haptic_buf[j] >= -1 && haptic_buf[j] <= 1);
#if DISABLE_SPEAKER_PROC
        if (haptic_silent) {
            haptic_buf_pos = 0;
            continue;
        }
#endif
        uint8_t pkt[REPORT_SIZE]{};
        pkt[0] = REPORT_ID;
        pkt[1] = reportSeqCounter << 4;
        reportSeqCounter = (reportSeqCounter + 1) & 0x0F;
        pkt[2] = 0x11 | 0 << 6 | 1 << 7;
        pkt[3] = 7;
        pkt[4] = 0b11111110;
        const auto buf_len = get_config().audio_buffer_length;
        pkt[5] = buf_len;
        pkt[6] = buf_len;
        pkt[7] = buf_len;
        pkt[8] = buf_len; // bytes 5-8: purpose unknown; adjusting has no observable effect
        pkt[9] = buf_len; // byte 9: audio buffer length — only this byte has an observable effect
        pkt[10] = packetCounter++;
        pkt[11] = 0x12 | 0 << 6 | 1 << 7;
        pkt[12] = haptic_silent ? 0 : SAMPLE_SIZE;
        if (!haptic_silent)
            memcpy(pkt + 13, haptic_buf, SAMPLE_SIZE);
#if !DISABLE_SPEAKER_PROC
        pkt[77] = (plug_headset ? 0x16 : 0x13) | 0 << 6 | 1 << 7; // Speaker: 0x13
        // L Headset Mono: 0x14
        // L Headset R Speaker: 0x15
        // Headset: 0x16
        pkt[78] = 200;
        if (core1_healthy) {
            critical_section_enter_blocking(&opus_cs);
            memcpy(pkt + 79, opus_buf, 200);
            critical_section_exit(&opus_cs);
        } else {
            static uint32_t warn_count = 0;
            if ((++warn_count % 500) == 1)
                printf("[Audio] Warning: Core 1 not healthy, speaker audio muted (count: %lu)\n", warn_count);
        }
#endif
        bt_write(pkt, sizeof(pkt), true);
        haptic_buf_pos = 0;
    }
}

void audio_init() {
    resampler.SetMode(true, 0, false);
    resampler.SetRates(48000, 3000);
    resampler.SetFeedMode(true);
    resampler.Prealloc(2, 24, 6);
 #if !DISABLE_SPEAKER_PROC
    queue_init(&audio_fifo, sizeof(audio_raw_element), 2);
    critical_section_init(&opus_cs);
    multicore_launch_core1_with_stack(core1_entry, audio_core1_stack, sizeof(audio_core1_stack));
#endif
}

static OpusEncoder *encoder;
static WDL_Resampler resampler_audio;

void core1_entry() {
    int error = 0;
    encoder = opus_encoder_create(48000, 2, OPUS_APPLICATION_AUDIO, &error);
    if (error != 0) {
        printf("[Audio] OpusEncoder create failed (error %d) — speaker audio disabled\n", error);
        return;
    }
    core1_healthy = true;
    opus_encoder_ctl(encoder,OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_10_MS));
    opus_encoder_ctl(encoder,OPUS_SET_BITRATE(200 * 8 * 100));
    opus_encoder_ctl(encoder,OPUS_SET_VBR(false));
    opus_encoder_ctl(encoder,OPUS_SET_COMPLEXITY(0)); // max 4
    resampler_audio.SetMode(true, 0, false);
    resampler_audio.SetRates(51200, 48000);
    resampler_audio.SetFeedMode(true);
    resampler_audio.Prealloc(2, 512, 480);

    while (true) {
        static audio_raw_element audio_element{};
        queue_remove_blocking(&audio_fifo, &audio_element);
        // resample 512 frames → 480 frames to eliminate noise. credit: @Junhoo
        WDL_ResampleSample *in_buf;
        int nframes = resampler_audio.ResamplePrepare(512, 2, &in_buf);
        for (int i = 0; i < nframes * 2; i++) {
            in_buf[i] = audio_element.data[i];
        }
        static WDL_ResampleSample out_buf[480 * 2];
        resampler_audio.ResampleOut(out_buf, nframes, 480, 2);

        static uint8_t out[200];
        opus_int32 encoded_len = opus_encode_float(encoder, out_buf, 480, out, 200);
        if (encoded_len < 0) continue;
        critical_section_enter_blocking(&opus_cs);
        memcpy(opus_buf, out, 200);
        critical_section_exit(&opus_cs);
    }
}
