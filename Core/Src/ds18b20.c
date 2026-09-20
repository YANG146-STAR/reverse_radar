#include "main.h"

// main.h 里已经声明 delay_us(uint16_t us); 此处防止 ARMCC V5 头文件顺序警告
extern void delay_us(uint16_t us);

// 引脚操作宏
#define DS18B20_PORT  DS18B20_GPIO_Port
#define DS18B20_PIN   DS18B20_Pin

// 设置为输出模式
static void DS18B20_SetOutput(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DS18B20_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;  // 开漏输出
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DS18B20_PORT, &GPIO_InitStruct);
}

// 设置为输入模式
static void DS18B20_SetInput(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DS18B20_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;     // 输入模式
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DS18B20_PORT, &GPIO_InitStruct);
}

// 拉低
#define DS18B20_LOW()  HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET)
// 拉高（开漏模式实际是释放总线，靠上拉拉高）
#define DS18B20_HIGH() HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET)
// 读引脚
#define DS18B20_READ() HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN)

// 复位+存在脉冲
// 返回0：检测到设备  返回1：未检测到
uint8_t DS18B20_Reset(void) {
    DS18B20_SetOutput();
    DS18B20_LOW();
    delay_us(480);      // 拉低480us

    DS18B20_SetInput();  // 释放总线
    delay_us(60);       // 等60us后检测存在脉冲

    uint8_t presence = DS18B20_READ();
    delay_us(420);      // 等存在脉冲结束

    return presence;    // 0=存在, 1=不存在
}

// 写一个字节
void DS18B20_WriteByte(uint8_t data) {
    DS18B20_SetOutput();
    for (int i = 0; i < 8; i++) {
        DS18B20_LOW();
        delay_us(2);

        if (data & (1 << i)) {
            DS18B20_HIGH();   // 写1：释放总线
            delay_us(60);
        } else {
            // 写0：保持低电平
            delay_us(60);
        }
        DS18B20_HIGH();       // 释放总线
        delay_us(2);
    }
}

// 读一个字节
uint8_t DS18B20_ReadByte(void) {
    uint8_t data = 0;
    for (int i = 0; i < 8; i++) {
        DS18B20_SetOutput();
        DS18B20_LOW();
        delay_us(2);

        DS18B20_SetInput();
        delay_us(10);        // 等待15us内读

        if (DS18B20_READ()) {
            data |= (1 << i);
        }
        delay_us(50);        // 等待时隙结束
    }
    return data;
}

// 启动温度转换
void DS18B20_StartConvert(void) {
    DS18B20_Reset();
    DS18B20_WriteByte(0xCC);  // 跳过ROM
    DS18B20_WriteByte(0x44);  // 启动温度转换
}

// 读取温度值（摄氏度）—— 无 CRC 校验的旧接口，建议改用 DS18B20_ReadTempChecked()
float DS18B20_ReadTemp(void) {
    uint8_t temp_l, temp_h;
    int16_t raw;

    // 读取暂存器
    DS18B20_Reset();
    DS18B20_WriteByte(0xCC);  // 跳过ROM
    DS18B20_WriteByte(0xBE);  // 读暂存器

    temp_l = DS18B20_ReadByte();
    temp_h = DS18B20_ReadByte();
    raw = (temp_h << 8) | temp_l;

    // DS18B20默认12位精度，每位=0.0625°C
    float temp = raw * 0.0625f;
    return temp;
}

// ====== DS18B20 CRC-8（Dallas/Maxim 多项式 0x8C，LSB-first）======
static uint8_t ds18b20_crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x01) crc = (crc >> 1) ^ 0x8C;
            else            crc >>= 1;
        }
    }
    return crc;
}

// 读取温度值（摄氏度）+ CRC 校验（读 9 字节暂存器，第 9 字节为 CRC）
// 返回 1=成功（值写入 *out_temp）；0=CRC 校验失败（数据不可信，调用方保持旧值/重试）
uint8_t DS18B20_ReadTempChecked(float *out_temp) {
    uint8_t data[9];

    // 读取暂存器（9 字节）
    DS18B20_Reset();
    DS18B20_WriteByte(0xCC);  // 跳过ROM
    DS18B20_WriteByte(0xBE);  // 读暂存器

    for (uint8_t i = 0; i < 9; i++) {
        data[i] = DS18B20_ReadByte();
    }

    if (ds18b20_crc8(data, 9) != 0) return 0;   // CRC 错 → 数据不可信

    int16_t raw = (int16_t)((data[1] << 8) | data[0]);
    if (out_temp) *out_temp = raw * 0.0625f;    // 12 位精度，每位 = 0.0625°C
    return 1;
}

// 完整读温度流程（阻塞版：~750ms，仅保留用于上电首次读一次）
float ds18b20_get_temp(void) {
    // 1. 启动转换
    DS18B20_StartConvert();
    HAL_Delay(750);  // 12位精度需要750ms转换时间（上电初始化调用一次，不算主循环卡顿）

    // 2. 读取结果（带 CRC 校验，失败返回明显错误值便于上电诊断发现）
    float temp = 25.0f;
    if (!DS18B20_ReadTempChecked(&temp)) return -999.0f;
    return temp;
}

// ============================================================
//  非阻塞接口：去掉 HAL_Delay(750)，解决主循环"每隔几次卡住"的体验
// ============================================================
static uint8_t  s_convert_pending = 0;   // 1=已经发了StartConvert, 还没读结果
static uint32_t s_convert_start_tick = 0;  // StartConvert 的 HAL_GetTick()

// 立即返回：~1.5ms（复位+写两个字节），触发 DS18B20 后台做温度转换
// 关键：如果上次转换还在 pending（没读结果），直接 return 不重发 —— 否则 tick 永远被推前
void ds18b20_start(void) {
    if (s_convert_pending) return;       // ← 没这行就是之前温度永远 25.0 的根因
    DS18B20_StartConvert();
    s_convert_pending    = 1;
    s_convert_start_tick = HAL_GetTick();
}

// 非阻塞读取：距离 start >750ms 读结果写到 *out，返回1；否则返回 0（不改 *out）
uint8_t ds18b20_get_if_ready(float *out_temp_c) {
    if (!s_convert_pending) return 0;
    if ((HAL_GetTick() - s_convert_start_tick) < 750) return 0;

    // 转换完成：读 9 字节暂存器 + CRC 校验
    // CRC 失败 → 不清 pending、不更新温度（保持旧值），下一帧自动重试
    float temp;
    if (!DS18B20_ReadTempChecked(&temp)) return 0;
    s_convert_pending = 0;
    if (out_temp_c) *out_temp_c = temp;
    return 1;
}

