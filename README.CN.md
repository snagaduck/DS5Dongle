# Pico2W DualSense 5 Bridge

[English](./README.md)

> 将 Raspberry Pi Pico2W 变成 DualSense (DS5) 手柄的无线适配器。

## 概述

本项目使 Raspberry Pi Pico2W 能够作为 DualSense 手柄的蓝牙桥接器，实现无线连接并支持增强的震动反馈。

## 功能特点

- 🎮 通过 Pico2W 完整支持 DualSense 连接
- 🔊 支持 HD 震动（高级振动反馈）
- 📡 无线蓝牙桥接
- ⚙️ 通过麦克风音量调节震动增益
- 🔕 可配置 LED 和断连行为

## 快速开始

### 刷入固件

1. 按住 Pico2W 上的 BOOTSEL 按钮
2. 通过 USB 将 Pico2W 连接至电脑
3. 设备将以 USB 存储设备形式挂载
4. 将 .uf2 固件文件拖放至该设备

### 配对手柄

1. 将 DualSense 手柄进入蓝牙配对模式
2. 等待 Pico2W 检测并连接
3. 连接成功后，设备将出现在主机系统中

## 配置说明

以下控制器设置已被重新用途：

### 麦克风音量

控制震动增益倍数

范围：[1.0 – 2.0]

### 扬声器静音

禁用 LED 连接指示灯

手柄重连后生效

### 麦克风静音

禁用静默断连行为

## 注意事项

手柄连接到 Pico 后，系统才会显示设备

部分行为需要重连后才能生效

### 低电量 LED 指示

当已连接的 DualSense 手柄电量不超过 10%（且未在充电）时，Pico 板载 LED 将从常亮切换为 1Hz 闪烁，以便直观看到警告。一旦手柄插上充电线或电量恢复，LED 将恢复常亮。若已设置 `disable_pico_led`，低电量警告仍会触发闪烁（视为紧急提示，优先级高于 LED 关闭偏好）；电量恢复或开始充电后，LED 将回到关闭状态。

如需在编译时关闭此功能，请使用 `-DENABLE_BATT_LED=OFF`，默认为 ON。

## 已知问题

- ⚠️ 音频可能出现轻微卡顿

## 性能 / 超频

由于编码需求，Pico2W 必须超频：

当前参数：

- 电压：1.2V
- 频率：320 MHz

若设备无法启动：

- 适当提高电压，或降低 CPU 频率

## 编译说明

### 前置条件

- [Pico SDK 2.2.0](https://github.com/raspberrypi/pico-sdk/releases/tag/2.2.0)（其他版本未经测试）
- CMake 3.13+
- `arm-none-eabi-gcc` 工具链

### TinyUSB 版本

Pico SDK 2.2.0 自带 TinyUSB 0.16，0.16 和 0.20 均受支持：

**TinyUSB 0.16（默认）：** 编译系统在配置阶段自动检测版本，并对 `audio_device.c` 进行补丁以支持 UAC1 设备。无需手动操作，直接运行 cmake 即可。

**TinyUSB 0.20（推荐）：** 原生支持 UAC1，无需补丁。升级方法：

```bash
cd ~/.pico-sdk/sdk/2.2.0/lib/tinyusb
git fetch origin
git checkout 0.20.0
```

### 编译

```bash
git clone --recurse-submodules https://github.com/snagaduck/DS5Dongle.git
cd DS5Dongle
mkdir build && cd build
cmake ..
make -j$(nproc)
```

`.uf2` 文件将生成于 `build/ds5-bridge.uf2`。

### CMake 选项

| 选项 | 默认值 | 说明 |
|---|---|---|
| `ENABLE_WAKE_HID` | `OFF` | 添加 HID 键盘接口，按下 PS 键时从 S3 休眠唤醒 PC |
| `ENABLE_BATT_LED` | `ON` | 控制器电量 ≤ 10% 时闪烁板载 LED |
| `ENABLE_SERIAL` | `OFF` | 启用 USB CDC 串口用于调试输出 |
| `DISABLE_SPEAKER_PROC` | `OFF` | 禁用扬声器音频处理（仅震动模式） |
| `PICO_W_BUILD` | `OFF` | 为原版 Pico W (RP2040) 编译；禁用音频，降低时钟至 200 MHz |
| `SYS_CLOCK_KHZ` | `320000` | CPU 频率（kHz） |

示例：`cmake .. -DENABLE_WAKE_HID=ON`

## 震动音频滤波器（可选）

本适配器将 USB 音频通道 3/4 直接路由至 DualSense 的震动马达（LRA）。LRA 对 ~400Hz 以下的频率响应最强——高频内容在震动上感觉更像电气噪声而非震动感。

固件内置了一个可选的二阶 Butterworth 低通滤波器（截止频率 300Hz，-12dB/倍频程衰减），默认未启用。启用方法：

1. 将 `src/haptic_filter.cpp.disabled` 重命名为 `src/haptic_filter.cpp`
2. 在编译目录中重新运行 cmake：
   ```bash
   cmake ..
   cmake --build . --parallel
   ```

如需停用，将文件改回 `.disabled` 后重新运行 cmake。未启用时，编译结果与无滤波版本完全相同——没有任何运行时开销。

**将游戏音频用作震动：** 启用滤波器后，固件会自动读取扬声器音频（通道 1/2）并经低通滤波后送入震动马达。通过适配器播放的任何音频都将驱动手柄震动——无需外部路由软件。

## Wake-on-PS（可选）

使用 `-DENABLE_WAKE_HID=ON` 编译时，会添加第二个 HID 接口（启动键盘），当主机处于休眠状态时，按下任意手柄按键将注入 **F15** 按键，从 **S3 睡眠** 唤醒 PC。选用 F15 是因为它在 Windows 和常见应用中没有默认绑定——误触不会输入字符或触发快捷键。

适用范围：**仅限 S3。** 不支持现代待机（S0ix）。运行 `powercfg /a` 检查——需要"待机 (S3)"出现在可用睡眠状态列表中。

刷入 Wake 版固件后：

1. 打开设备管理器 → 找到新增的 **HID 键盘设备**（及其父级 **USB 复合设备**）→ 属性 → 电源管理 → 勾选"**允许此设备唤醒计算机**"
2. 用 `powercfg /devicequery wake_armed` 验证
3. 让 PC 进入睡眠；按手柄上的任意按键；PC 应在约 1 秒内唤醒
4. 唤醒后，`powercfg /lastwake` 应显示唤醒来源为 HID 键盘设备

## 开发路线图
- 请查看 [DS5Dongle plan](https://github.com/users/awalol/projects/5)

## 社区
- 加入 Discord 服务器：[Discord Server](https://discord.gg/hM4ntchGCa)
- 如遇 Bug，请提交 Issue 而非在 Discord 反馈。

## 参考资料

- [rafaelvaloto/Pico_W-Dualsense](https://github.com/rafaelvaloto/Pico_W-Dualsense) — 项目灵感来源
- [egormanga/SAxense](https://github.com/egormanga/SAxense) — 蓝牙震动 POC
- [https://controllers.fandom.com/wiki/Sony_DualSense](https://controllers.fandom.com/wiki/Sony_DualSense) — DualSense 数据报文结构文档
- [Paliverse/DualSenseX](https://github.com/Paliverse/DualSenseX) — 扬声器报文数据包
