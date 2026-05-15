//
// Created by awalol on 2026/3/4.
//

#include <cstdio>
#include "bsp/board_api.h"
#include "bt.h"
#include "utils.h"
#include "audio.h"
#include "wake.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"
#if ENABLE_SERIAL
#include "pico/stdio_usb.h"
#endif
#include "config.h"
#include "cmd.h"
#if ENABLE_BATT_LED
#include "battery_led.h"
#endif

// Pico SDK specifically for waiting on conditions
#include "pico/critical_section.h"

static_assert(sizeof(USBGetStateData) == 63,
              "USBGetStateData must match the 63-byte BT 0x31 payload");

uint8_t interrupt_in_data[63] = {
    0x7f, 0x7d, 0x7f, 0x7e, 0x00, 0x00, 0xa7,
    0x08, 0x00, 0x00, 0x00, 0x52, 0x43, 0x30, 0x41,
    0x01, 0x00, 0x0e, 0x00, 0xef, 0xff, 0x03, 0x03,
    0x7b, 0x1b, 0x18, 0xf0, 0xcc, 0x9c, 0x60, 0x00,
    0xfc, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x09, 0x09, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xa7, 0xad, 0x60, 0x00, 0x29, 0x18, 0x00,
    0x53, 0x9f, 0x28, 0x35, 0xa5, 0xa8, 0x0c, 0x8b
};

critical_section_t report_cs;
volatile bool report_pending = false;

void interrupt_loop() {
    if (!tud_hid_ready()) return;

    if (get_config().polling_rate_mode != 2) {
        if (!tud_hid_report(0x01, interrupt_in_data, 63)) {
            printf("[USBHID] tud_hid_report error\n");
        }
        return;
    }

    bool should_send = false;
    // Local buffer to hold the report data while we prepare it to send. 
    uint8_t safe_report[63];


    critical_section_enter_blocking(&report_cs);
    if (report_pending) {
        memcpy(safe_report, interrupt_in_data, 63);
        report_pending = false;
        should_send = true;
    }
    critical_section_exit(&report_cs);

    // Only send to TinyUSB if we actually grabbed fresh data
    if (should_send) {
        if (!tud_hid_report(0x01, safe_report, 63)) {
            printf("[USBHID] tud_hid_report error\n");

            // If the report failed to queue, restore the dirty flag 
            // so we try again on the next loop iteration.
            critical_section_enter_blocking(&report_cs);
            report_pending = true;
            critical_section_exit(&report_cs);
        }
    }
}

void on_bt_data(CHANNEL_TYPE channel, uint8_t *data, uint16_t len) {
    // BT 0x01 simplified input: brief window before controller switches to 0x31 mode.
    // Map available fields so the USB report isn't stale during connection.
    if (channel == INTERRUPT && data[1] == 0x01 && len >= 11) {
        const uint8_t *s = data + 2; // BTSimpleGetStateData
        interrupt_in_data[0] = s[0]; // LeftStickX
        interrupt_in_data[1] = s[1]; // LeftStickY
        interrupt_in_data[2] = s[2]; // RightStickX
        interrupt_in_data[3] = s[3]; // RightStickY
        interrupt_in_data[4] = s[7]; // TriggerLeft
        interrupt_in_data[5] = s[8]; // TriggerRight
        interrupt_in_data[7] = s[4]; // DPad + face buttons (same layout)
        interrupt_in_data[8] = s[5]; // shoulder/action buttons (same layout)
        // BTSimple byte6: bit1=ButtonHome, bit2=ButtonPad → USB byte9: bit0=ButtonHome, bit1=ButtonPad
        interrupt_in_data[9] = (interrupt_in_data[9] & 0xFC) | ((s[6] >> 1) & 0x03);
        return;
    }

    if (channel == INTERRUPT && data[1] == 0x31) {
        const auto *state = reinterpret_cast<const USBGetStateData *>(data + 3);

        if (state->PluggedHeadphones != (interrupt_in_data[53] & 1)) {
            set_headset(state->PluggedHeadphones);
            bt_update_output_path(state->PluggedHeadphones);
        }

        if (state->MicMuted != ((interrupt_in_data[53] >> 2) & 1)) {
            bt_update_mute_light(state->MicMuted);
        }

        // Wake-on-PS must observe every BT input report regardless of polling
        // mode: the wake feature has its own state to maintain (button-byte
        // diff for edge detection) and short-circuiting it on non-2 polling
        // modes silently breaks wake while the host is suspended.
        wake_on_bt_input(data + 3, len - 3);

        // Pack power byte: PowerPercent in bits 0-3, PowerState in bits 4-7.
        const uint8_t power_byte = static_cast<uint8_t>(
            (static_cast<uint8_t>(state->Power) << 4) | state->PowerPercent);

        if (get_config().polling_rate_mode != 2) {
            memcpy(interrupt_in_data, data + 3, 63);
#if ENABLE_BATT_LED
            battery_led_note_report(power_byte);
#endif
            return;
        }

        critical_section_enter_blocking(&report_cs);
        memcpy(interrupt_in_data, data + 3, 63);
        report_pending = true;
        critical_section_exit(&report_cs);
#if ENABLE_BATT_LED
        battery_led_note_report(power_byte);
#endif
    }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
#ifdef ENABLE_WAKE_HID
    if (itf == 1) {
        if (reqlen >= 8) {
            memset(buffer, 0, 8);
            return 8;
        }
        return 0;
    }
#endif
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    if (is_pico_cmd(report_id)) {
        return pico_cmd_get(report_id, buffer, reqlen);
    }

    std::vector<uint8_t> feature_data = get_feature_data(report_id, reqlen);
    if (!feature_data.empty()) {
        uint16_t copy_len = std::min((uint16_t)(feature_data.size() - 1), reqlen);
        memcpy(buffer, feature_data.data() + 1, copy_len);
        return copy_len;
    }

    return 0;
}

bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void) rhport;
    uint8_t const itf = tu_u16_low(p_request->wIndex); // wInterface
    uint8_t const alt = tu_u16_low(p_request->wValue); // bAlternateSetting

    if (itf == 1) {
        printf("[AUDIO] Set interface Speaker to alternate setting %d\n", alt);
    }

    return true;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
#ifdef ENABLE_WAKE_HID
    if (itf == 1) {
        // Drop keyboard SET_REPORT (host LED state).
        return;
    }
#endif
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;

    if (is_pico_cmd(report_id)) {
        printf("[HID] Receive 0xf6 setting config, funcid:0x%02X\n", buffer[0]);
        pico_cmd_set(report_id, buffer, bufsize);
        return;
    }

    // INTERRUPT OUT
    if (report_id == 0) {
        switch (buffer[0]) {
            case 0x02: {
                static int out_report_seq = 0;
                uint8_t outputData[78]{};
                outputData[0] = 0x31;
                outputData[1] = (out_report_seq & 0x0F) << 4;
                if (++out_report_seq >= 16) out_report_seq = 0;
                memcpy(outputData + 2, buffer + 1, bufsize - 1);
                bt_write(outputData, sizeof(outputData));
                // SetStateData[2/3] = RumbleEmulationRight/Left (buffer[3/4])
                if (bufsize > 4)
                    notify_rumble(buffer[4], buffer[3]);
                break;
            }
        }
    }
    if (report_id == 0x21 || // Set Audio Control — proxy volume/mic commands to BT
        report_id == 0x80 ||
        // DSE: Write Profile Block
        report_id == 0x60 ||
        report_id == 0x62 ||
        report_id == 0x61) {
        set_feature_data(report_id, const_cast<uint8_t *>(buffer), bufsize);
        return;
    }
}

int main() {
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(1000);
    set_sys_clock_khz(SYS_CLOCK_KHZ, true);

    board_init();
    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL
    };
    tusb_init(BOARD_TUD_RHPORT, &dev_init);
#if !ENABLE_SERIAL
    sleep_ms(150);
    tud_disconnect();
#endif
    board_init_after_tusb();
#if ENABLE_SERIAL
    stdio_usb_init();
#endif

    if (cyw43_arch_init()) {
        printf("Failed to initialize CYW43\n");
        return 1;
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);

#if ENABLE_BATT_LED
    battery_led_init();
#endif

#if !ENABLE_SERIAL
    if (watchdog_caused_reboot()) {
        printf("Rebooted by Watchdog!\n");
        // flash LED three times to signal a watchdog-triggered reboot
        for (int i = 0; i < 6; i++) {
            if (i % 2 == 0) {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
            } else {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
            }
            sleep_ms(500);
        }
    } else {
        printf("Clean boot\n");
    }
#endif

    // Initialize the critical section for the report buffer
    critical_section_init(&report_cs);
    wake_init();

    config_load();

    bt_init();
    bt_register_data_callback(on_bt_data);

    audio_init();

#if !ENABLE_SERIAL
    watchdog_enable(1000, true);
#endif

    while (1) {
#if !ENABLE_SERIAL
        watchdog_update();
#endif
        cyw43_arch_poll();
        tud_task();
        cmd_task();
        wake_task();
        audio_loop();
        interrupt_loop();
#if ENABLE_BATT_LED
        battery_led_tick();
#endif
    }
}
