# CODEBUDDY.md

This file provides guidance to CodeBuddy Code when working with code in this repository.

## 项目概述

FreeRTOS 骑行码表旗舰版（4G），基于 STM32F103C8 (ARM Cortex-M3, 72MHz, 64KB Flash, 20KB SRAM)，使用 Keil MDK 构建。实现骑行数据采集、GPS 定位、4G 云端上传、OLED 显示等功能。

## 构建方式

本项目使用 **Keil MDK (uVision5)** 构建，没有 Makefile 或 CMake 构建系统。

- **项目文件**: `Project.uvprojx`（用 Keil uVision5 打开）
- **工具链**: ARM Compiler 5.06 update 7 (ARM-ADS)
- **清理输出**: 运行 `keilkill.bat` 清理编译产物（`Listings/` 和 `Objects/` 目录）
- **编译输出目录**: `Objects/`（.axf, .hex, .map 文件）

命令行构建需要安装 Keil MDK 并使用 `UV4.exe`：
```
UV4.exe -build Project.uvprojx
```

## 目录结构

```
├── User/          # 应用层（骑行核心逻辑）
├── Drivers/       # 外设驱动（传感器、显示、通信）
├── System/        # OS 抽象层和日志
├── FreeRTOS/      # FreeRTOS 内核
│   ├── inc/       # 内核头文件及 FreeRTOSConfig.h
│   ├── src/       # 内核源文件
│   └── portable/  # Cortex-M3 移植（port.c, heap_4.c）
├── RTE/           # Keil 运行时环境（STM32 标准外设库）
├── Objects/       # 编译输出（.gitignore 这些）
└── Listings/      # 编译列表输出
```

## 架构说明

### 分层架构

```
[User Layer]    main.c, bike.c, screen.c, flash_data.c, flash_index.c
      ↓
[System Layer]  system.h (OS抽象: os_delay_ms, os_malloc, os_millis)
      ↓
[Driver Layer]  Drivers/ (OLED, AHT10, GPS, W25Qxx, AIR780E, key, led, usart)
      ↓
[RTOS Layer]    FreeRTOS kernel (tasks, queues, timers, event groups)
      ↓
[HAL Layer]     RTE/Device/STM32F103C8 (STM32 标准外设库)
```

### 主要任务结构

`User/main.c` 创建任务并启动调度器：
- `bike_start()` — 骑行主任务（含状态机：IDEL → RUNNING → STOP）
- GPS 数据通过 USART3 中断接收，解析 GPRMC 帧后向骑行任务发通知
- 按键通过 EXTI 中断检测，发任务通知位给骑行主任务

### 骑行主任务通知位（`User/bike.h`）

```c
NOTIFY_KEY1_PRESS       (1 << 0)   // KEY1 短按/长按
NOTIFY_KEY2_PRESS       (1 << 1)   // KEY2 短按/长按
NOTIFY_GPRMC_RECV       (1 << 2)   // GPS GPRMC 帧到达
NOTIFY_CONNECTED_SERVER (1 << 3)   // 4G 连接服务器成功
NOTIFY_TIME_UPDATE      (1 << 4)   // 时间同步
NOTIFY_DATA_LIST        (1 << 5)   // 数据列表请求
NOTIFY_DATA_EXPORT      (1 << 6)   // 数据导出请求
```

### 外设与串口分配

| 串口   | 用途            | 驱动              |
|--------|-----------------|-------------------|
| USART1 | 调试日志输出    | `System/log.c`    |
| USART2 | AIR780E 4G 模块 | `Drivers/at_air780e.c` |
| USART3 | GPS 接收        | `Drivers/GPS.c`   |

| 总线 | 设备                    |
|------|-------------------------|
| I2C1 | AHT10 温湿度 + OLED 显示屏 |
| SPI1 | W25Qxx SPI Flash        |

| GPIO       | 功能         |
|------------|--------------|
| PA0（绿）  | LED 指示灯   |
| PC13（红） | LED 指示灯   |
| PB0        | KEY1 (EXTI0) |
| PC14       | KEY2 (EXTI15_10) |

### FreeRTOS 关键配置（`FreeRTOS/inc/FreeRTOSConfig.h`）

- Tick 频率：1000Hz（1ms）
- 最大优先级：5
- 堆大小：5120 字节（heap_4 算法）
- 栈溢出检测：模式 2
- 禁用时间分片（configUSE_TIME_SLICING = 0）

### OS 抽象层（`System/system.h`）

所有 FreeRTOS 调用应通过此抽象层进行，不要在应用层直接调用 `vTaskDelay` 等：

```c
os_delay_ms(ms)      // 替代 vTaskDelay
os_delay_s(s)        // 秒级延迟
os_millis()          // 获取毫秒时间戳
os_millisFromISR()   // 在 ISR 中获取时间
os_malloc(size)      // 动态内存分配
os_free(ptr)         // 释放内存
os_mem_info()        // 打印堆使用情况
```

### 数据持久化

骑行记录存储在 W25Qxx SPI Flash 中：
- `User/flash_data.c` — 单条骑行记录的读写
- `User/flash_index.c` — 记录索引管理（类似文件系统索引）

### 4G 通信

`Drivers/at_air780e.c` 封装 AIR780E 模块 AT 指令，通过 `Drivers/at_interface.c` 提供通用 AT 命令发送框架，使用环形缓冲区（`Drivers/ringbuffer.c`）处理串口数据流。

## 关键文件索引

| 文件 | 作用 |
|------|------|
| `User/main.c` | 系统初始化，任务创建，调度器启动 |
| `User/bike.c` | 骑行数据处理核心（距离、速度、卡路里、状态机） |
| `User/screen.c` | OLED 多屏显示管理 |
| `User/flash_data.c` | Flash 骑行数据存储 |
| `Drivers/GPS.c` | GPS GPRMC 帧解析 |
| `Drivers/at_air780e.c` | AIR780E 4G 模块驱动 |
| `Drivers/OLED.c` | OLED 屏幕驱动 |
| `Drivers/AHT10.c` | 温湿度传感器驱动 |
| `System/system.c` | FreeRTOS OS 抽象层初始化 |
| `FreeRTOS/inc/FreeRTOSConfig.h` | FreeRTOS 系统配置 |
