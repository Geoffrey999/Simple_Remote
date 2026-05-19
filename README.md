# F103 航模遥控器

基于 STM32F103C8 的开源航模遥控器固件，使用 STM32CubeMX + Keil MDK-ARM 开发，集成 FreeRTOS 实时操作系统。

## 硬件平台

- **主控芯片**: STM32F103C8 (Cortex-M3, 72MHz)
- **显示屏**: 0.96" OLED 128x64 (SSD1306, SPI 接口)
- **通信**: 2.4G 无线模块 (USART1, 通过 DMA 收发)
- **摇杆**: 4 路 ADC (Roll / Pitch / Throttle / 电池电压)
- **按键**: UP / DOWN / ENTER / BACK
- **开关**: ARM (3 段) / MODE (3 段) / Yaw+ / Yaw-
- **USB**: USB Device (预留)

## 固件架构

基于 FreeRTOS (CMSIS-RTOS V2)，划分为 3 个任务：

| 任务 | 优先级 | 频率 | 职责 |
|------|--------|------|------|
| **InputTask** | Normal | ~60Hz | 读取 ADC 摇杆值、物理开关、按键扫描，组装 TX 数据帧 |
| **UartTxTask** | High | 按需触发 | 通过 USART1 DMA 发送控制帧，接收并解析飞控回传数据 |
| **DisplayTask** | Low | ~100Hz | OLED 显示驱动、菜单系统交互 |

### 任务间通信

- `TxFrameQueue` (size 3): InputTask → UartTxTask，传递待发送的控制帧
- `RxFrameQueue` (size 8): UART 空闲中断 → UartTxTask，传递飞控回传帧
- `KeyEventQueue` (size 8): InputTask → DisplayTask，传递按键事件
- `settings_mutex`: 保护菜单设置参数 (aux 值、偏移量) 的并发访问

## 通信协议

### 上行帧 (遥控器 → 飞控) — 16 通道 x 16bit

| 字段 | 说明 |
|------|------|
| head | 帧头 `0x4020` |
| roll / pitch / throttle / yaw | 摇杆通道 (1000~2000) |
| arm | 解锁状态 (1000=Disarm / 1500=Disarm / 2000=Armed) |
| mode | 飞行模式 (1000=Angle+Alt / 1500=Horizon / 2000=Acro) |
| aux3 ~ aux8 | 辅助通道 (可通过菜单调节) |
| roll_offset / pitch_offset | 摇杆微调偏移 |
| checksum | 累加和校验 (0xFFFF - sum) |

### 下行帧 (飞控 → 遥控器) — 10 字段

| 字段 | 说明 |
|------|------|
| head | 帧头`0x4020`  |
| bat | 飞控电池电压 |
| roll / pitch / yaw | 飞行器姿态角 |
| altitude | 高度 |
| rssi | 信号强度 |
| checksum | 校验和 |

## 菜单系统

```
主页 (Roll/Pitch/Yaw/Throttle/飞行状态/电池)
  └─ ENTER → 菜单
       ├─ Attitude → 飞控姿态数据显示
       ├─ Offset  → Roll / Pitch 摇杆微调
       ├─ AUXs
       │   ├─ Aux3-5 → 辅助通道 3~5 数值调节
       │   └─ Aux6-8 → 辅助通道 6~8 数值调节
       └─ Info    → 固件版本、运行时间
```

- UP/DOWN: 移动光标 / 编辑数值
- UP/DOWN 长按: 快速增减数值 (500ms 触发, 100ms 重复)
- ENTER: 进入子菜单 / 确认 / 切换编辑模式
- BACK: 返回上级 / 退出编辑 / 返回主页

## 硬件引脚定义

| 功能 | 引脚 |
|------|------|
| OLED CS | PA15 |
| OLED DC | PA0 |
| OLED RST | PB4 |
| OLED SPI | SPI1 (PA5 SCK, PA7 MOSI) |
| KEY_UP | PB8 |
| KEY_DOWN | PA10 |
| KEY_ENTER | PB9 |
| KEY_BACK | PA9 |
| USART1 TX/RX | PA9/PA10 (连接无线模块) |
| USART3 TX/RX | (预留调试) |
| Yaw- / Yaw+ | PB13 / PB14 |
| ARM 3段开关 | PA3, PA4 |
| MODE 3段开关 | PC13, PC14 |
| ADC 摇杆 | PA0~PA4 (Roll/Pitch/Throttle/Bat) |

## 开发环境

- **IDE**: Keil MDK-ARM (UVision)
- **代码生成**: STM32CubeMX 
- **RTOS**: FreeRTOS V10.3.1 (CMSIS-RTOS V2)
- **HAL 库**: STM32F1xx HAL Driver

### 编译说明

1. 安装 Keil MDK-ARM (推荐 5.x 版本)
2. 安装 STM32F1xx 器件支持包 (DFP)
3. 双击打开 `MDK-ARM/F103.uvprojx`
4. 编译并下载到目标板

## 许可

本项目基于 MIT 协议开源。
