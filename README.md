# Pico2W DualSense 5 Bridge

[中文](./README.CN.md)

> Turn a Raspberry Pi Pico2W into a wireless adapter for the DualSense (DS5) controller.

## Overview

This project enables the Raspberry Pi Pico2W to function as a Bluetooth bridge for the DualSense controller, allowing wireless connectivity with enhanced haptics support.

## Features

- 🎮 Full DualSense connectivity via Pico2W
- 🔊 Supports HD haptics (advanced vibration feedback)
- 📡 Wireless Bluetooth bridging
- ⚙️ Adjustable haptic gain via microphone volume
- 🔕 Configurable LED and disconnection behaviors

## Getting Started

### Flashing Firmware

1. Hold the BOOTSEL button on the Pico2W
2. Connect the Pico2W to your computer via USB
3. The device will mount as a USB storage device
4. Drag and drop the .uf2 firmware file onto the device

### Pairing the Controller

1. Put the DualSense controller into Bluetooth pairing mode
2. Wait for the Pico2W to detect and connect
3. Once connected, the device will appear on the host system

## Configuration

The following controller settings are repurposed:

### Microphone volume

Controls haptic gain multiplier

Range: [1.0 – 2.0]

### Speaker mute

Disables LED connection indicator

Takes effect after controller reconnects

### Microphone mute

Disables silent disconnection behavior

## Notes

The Pico device will only be visible to the system after the controller is connected

Some behaviors depend on reconnection cycles to take effect

### Low-battery LED indicator

When the connected DualSense reports its battery at or below 10% (and it is not charging), the Pico onboard LED switches from solid-on to a 1 Hz blink so you can see the warning at a glance. The LED returns to solid-on as soon as the controller is plugged in or its reported level rises again. The blink also fires when `disable_pico_led` is set — the warning is treated as critical and overrides the LED-off preference; the LED returns to its disabled (off) state once the battery recovers or the controller starts charging.

To opt out at build time, configure with `-DENABLE_BATT_LED=OFF`. Default is ON.

## Known Issues

- ⚠️ Audio may experience slight stuttering

## Performance / Overclocking

Due to encoding requirements, the Pico2W must be overclocked:

Current settings:

- Voltage: 1.2V
- Frequency: 320 MHz

If your device fails to boot:

- Increase voltage slightly or Reduce CPU frequency

## Build Instructions

### Prerequisites

- [Pico SDK 2.2.0](https://github.com/raspberrypi/pico-sdk/releases/tag/2.2.0) (other versions not tested)
- CMake 3.13+
- `arm-none-eabi-gcc` toolchain

### TinyUSB version

Pico SDK 2.2.0 ships with TinyUSB 0.16, which does not support UAC1 and will cause USB audio enumeration to fail on Windows. Update TinyUSB to 0.20.0 before building:

```bash
cd ~/.pico-sdk/sdk/2.2.0/lib/tinyusb
git fetch origin
git checkout 0.20.0
```

### Build

```bash
git clone --recurse-submodules https://github.com/snagaduck/DS5Dongle.git
cd DS5Dongle
mkdir build && cd build
cmake ..
make -j$(nproc)
```

The `.uf2` file will be in `build/ds5-bridge.uf2`.

### CMake options

| Option | Default | Description |
|---|---|---|
| `ENABLE_WAKE_HID` | `OFF` | Add HID keyboard interface to wake PC from S3 sleep on button press |
| `ENABLE_BATT_LED` | `ON` | Blink onboard LED when controller battery ≤ 10% |
| `ENABLE_SERIAL` | `OFF` | Enable USB CDC serial port for debug output |
| `DISABLE_SPEAKER_PROC` | `OFF` | Disable speaker audio processing (haptics only) |
| `SYS_CLOCK_KHZ` | `320000` | CPU frequency in kHz |

Example: `cmake .. -DENABLE_WAKE_HID=ON`

## Wake-on-PS (optional)

A `-DENABLE_WAKE_HID=ON` build adds a second HID interface (a boot keyboard) that injects an **F15** keypress when any controller button is pressed while the host is suspended, waking the PC from **S3 sleep**. F15 was chosen because it has no default Windows or app binding — a stray fire never inserts characters or triggers shortcuts.

Scope: **S3 only.** Modern Standby (S0ix) is not supported. To check your machine, run `powercfg /a` — you need "Standby (S3)" listed under available sleep states.

After flashing the wake build:

1. Open Device Manager → the new **HID Keyboard Device** (and its parent **USB Composite Device**) → Properties → Power Management → tick **"Allow this device to wake the computer."**
2. Verify with `powercfg /devicequery wake_armed`.
3. Sleep the PC; press any button on the controller; the PC should wake within ~1 s.
4. After a wake, `powercfg /lastwake` should attribute the wake to the HID Keyboard Device.

## Roadmap
- Please check out [DS5Dongle plan](https://github.com/users/awalol/projects/5)

## Community
- Join the Discord server: [Discord Server](https://discord.gg/hM4ntchGCa)
- If you have a bug, please open an issue instead.

## References

- [rafaelvaloto/Pico_W-Dualsense](https://github.com/rafaelvaloto/Pico_W-Dualsense) — Project inspiration
- [egormanga/SAxense](https://github.com/egormanga/SAxense) — Bluetooth Haptics POC
- [https://controllers.fandom.com/wiki/Sony_DualSense](https://controllers.fandom.com/wiki/Sony_DualSense) - DualSense data report structure documentation
- [Paliverse/DualSenseX](https://github.com/Paliverse/DualSenseX) — Speaker report packet
