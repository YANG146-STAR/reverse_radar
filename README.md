# STM32F103 倒车雷达系统

基于 STM32F103C8T6 + FreeRTOS 的车载倒车雷达系统，集成超声波测距、OLED 显示、姿态检测、多级报警和菜单配置功能。

## 功能概览

| 功能模块 | 传感器/外设 | 说明 |
|---------|-----------|------|
| 超声波测距 | HC-SR04 | 5 点中值 + 5 点滑动平均滤波，测量范围 2-400cm |
| OLED 显示 | SSD1306 0.96" | I2C 接口，显示距离/温度/姿态/报警状态 |
| 姿态检测 | MPU6050 | Roll/Pitch 解耦，倾斜检测 |
| 温度监测 | DS18B20 | 非阻塞读取，750ms 转换周期 |
| 四区报警 | RGB LED + 蜂鸣器 | EMG/DNG/WRN/SAFE 四级，颜色 + 节奏 |
| 菜单系统 | 旋转编码器 | 阈值/亮度/校准参数配置 |
| 红外避障 | IR 模块 | 近距离辅助检测 |

## 硬件连接

```
STM32F103C8T6
├── PA2  ─── RGB LED Red   (GPIO, 低电平有效)
├── PA8  ─── 蜂鸣器         (TIM1_CH1 PWM, 低电平触发)
├── PA9  ─── USART1 TX     (115200 8N1, 调试串口)
├── PA10 ─── USART1 RX
├── PA15 ─── RGB LED Green (GPIO, 低电平有效)
├── PB0  ─── Encoder CLK   (GPIO_PULLUP)
├── PB1  ─── Encoder SW    (GPIO_PULLUP)
├── PB3  ─── RGB LED Blue  (GPIO, 低电平有效)
├── PB6  ─── I2C1 SCL      (OLED + MPU6050)
├── PB7  ─── I2C1 SDA      (OLED + MPU6050)
├── PB8  ─── Ultrasonic Echo (输入)
├── PB9  ─── Ultrasonic Trig(输出)
├── PB12 ─── Encoder DT    (GPIO_PULLUP)
└── PC13 ─── 板载 LED      (心跳指示)
```

## 项目结构

```
reverse_radar/
├── Application/              # 应用层 (模块化封装)
│   ├── Inc/
│   │   ├── app_config.h      # 系统配置 + 共享数据声明
│   │   ├── app_tasks.h       # RTOS 对象 + app_init/app_start
│   │   ├── app_ultrasonic.h  # 超声波测距接口
│   │   ├── app_alarm.h       # 报警逻辑接口
│   │   ├── app_menu.h        # 菜单状态机接口
│   │   └── app_outputs.h     # RGB/蜂鸣器/延时接口
│   └── Src/
│       ├── app_tasks.c       # 5 任务 + 2 hook + 共享数据 + RTOS 对象
│       ├── app_ultrasonic.c  # 超声波测距 + 滤波
│       ├── app_alarm.c       # 四区报警逻辑
│       ├── app_menu.c        # 旋转编码器菜单
│       └── app_outputs.c     # RGB/蜂鸣器/IR 检测
├── Core/                     # CubeMX 管理
│   ├── Inc/                  # main.h + 外设头文件 + BSP 驱动头文件
│   └── Src/                  # main.c + 外设初始化 + BSP 驱动 (OLED/DS18B20/MPU6050/filter)
├── Drivers/                  # STM32 HAL + CMSIS
├── Middlewares/              # FreeRTOS 源码
└── MDK-ARM/
    ├── reverse_radar.uvprojx  # Keil 工程文件
    └── startup_stm32f103xb.s # 启动文件
```

## 系统架构图

![Architecture](docs/architecture.svg)

## FreeRTOS 任务架构

| 任务 | 优先级 | 栈大小 | 周期 | 职责 |
|------|--------|-------|------|------|
| InitTask | 6 | 512 word | 单次 | 硬件初始化 + MPU6050 校准，完成后自删除 |
| SensorTask | 5 | 512 word | 10ms | 超声波测距 + MPU6050 读取 + DS18B20 非阻塞 + 共享数据更新 |
| AlarmTask | 4 | 256 word | 50ms | 四区报警判断 + RGB 颜色 + 蜂鸣器节奏 |
| EncoderTask | 4 | 256 word | 2ms | 旋转编码器扫描 + 按键检测 + 事件入队 |
| DisplayTask | 2 | 384 word | 50ms | OLED 显存更新 + 刷新 + 菜单渲染 |

### 互斥锁

| 互斥锁 | 保护对象 |
|--------|---------|
| xMutexI2C | I2C1 总线 (OLED + MPU6050 共用) |
| xMutexSensor | 共享传感器数据 (g_distance/g_temp/g_mpu/g_zone/g_tilt) |
| xMutexSysConfig | 系统配置参数 (sys_config) |

## 报警分区

| 区间 | 距离范围 | LED 颜色 | 蜂鸣器节奏 | 状态 |
|------|---------|---------|-----------|------|
| EMG | < 15cm | 红色 | 60/60ms 快响 | 紧急 |
| DNG | 15 ~ 30cm | 紫色 | 80/120ms 中速 | 危险 |
| WRN | 30 ~ 50cm | 黄色 | 150/350ms 慢响 | 警告 |
| SAFE | ≥ 50cm | 绿色 | 静音 | 安全 |

## 关键设计

- **I2C 互斥保护**: DisplayTask 和 SensorTask 共用 hi2c1，所有 I2C 操作通过 xMutexI2C 串行化，避免总线冲突
- **DS18B20 非阻塞**: `ds18b20_start()` 启动转换后立即返回，750ms 后 `ds18b20_get_if_ready()` 读取结果，不阻塞主循环
- **OLED 双缓冲**: `OLED_ShowString()` 只写本地显存，`OLED_Refresh()` 统一刷新到硬件，减少 I2C 持锁时间
- **超声波 DWT 计时**: 使用 `DWT->CYCCNT` 硬件计数器实现 14ns 精度计时，避免 TIM 通道冲突
- **MPU6050 姿态解耦**: 原始 X/Y 轴数据旋转 45° 投影到车体坐标轴，实现 Roll/Pitch 解耦
- **编码器轮询**: 2ms 周期扫描替代 EXTI 中断，避免与 FreeRTOS 临界区冲突

## 开发环境

- **MCU**: STM32F103C8T6 (Cortex-M3, 72MHz, 64KB Flash, 20KB SRAM)
- **IDE**: Keil MDK-ARM 5.26 (ARMCC V5.06)
- **RTOS**: FreeRTOS (Cortex-M3 RVDS port, heap_4)
- **代码生成**: STM32CubeMX (外设初始化)
- **烧录**: J-Link / ST-Link

## 编译与烧录

1. 用 Keil 打开 `MDK-ARM/reverse_radar.uvprojx`
2. 点击 Build (F7) 编译
3. 连接 J-Link/ST-Link，点击 Download 烧录
4. 上电后 OLED 显示启动画面，MPU6050 校准期间请勿移动设备

## 资源占用

| 资源 | 占用 | 说明 |
|------|------|------|
| Flash | ~26KB / 64KB | 代码 + RO-data |
| RAM | ~13KB / 20KB | RW-data + ZI-data (含 FreeRTOS heap 10KB) |
| 任务数 | 5 + IDLE + Timer | 5 个应用任务 |

## License

MIT
