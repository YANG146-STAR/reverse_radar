#ifndef __MPU6050_H
#define __MPU6050_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

// MPU6050 I2C 地址（7 位格式，传给 HAL API 用）：
//   AD0 接 GND → 0x68（最常见）
//   AD0 接 VCC → 0x69（如果模块 AD0 上拉，改这里）
// HAL_I2C_Master_Transmit / Mem_Read 要求 DevAddress 参数 = 7 位地址，
// HAL 内部会自动在最低位拼 W/R 位。
#define MPU6050_ADDR_7BIT  0x68

#define MPU6050_TO     100   // HAL I2C 超时 (ms)

// ====== 姿态角输出（单位：度）======
typedef struct {
    float roll_deg;     // 横滚（左右倾斜）: +右倾 -左倾 范围 -90~+90
    float pitch_deg;    // 俯仰（前后倾斜）: +车头向上(上坡) -车头向下(下坡) 范围 -90~+90
    float acc_x_g;      // X 轴加速度 (g)
    float acc_y_g;      // Y 轴加速度 (g)
    float acc_z_g;      // Z 轴加速度 (g)
    float gyro_x_dps;   // X 轴角速度 (°/s)
    float gyro_y_dps;   // Y 轴角速度 (°/s)
    float gyro_z_dps;   // Z 轴角速度 (°/s)
    uint8_t init_ok;    // 1=初始化成功 0=失败（I2C无应答）
    // ---- 诊断字段（上电时记录，帮助排查 "NACK/ID?"）----
    uint8_t who_am_i;   // 实际读到的 WHO_AM_I 值（HEX）
    uint8_t used_addr;  // 实际使用的 7-bit 地址（0x68 或 0x69）
} Mpu6050Result;

/**
 * @brief 初始化 MPU6050（唤醒+设置量程+地址自动探测）
 *        自动尝试 AD0=GND(0x68) 和 AD0=VCC(0x69) 两个地址；
 *        对"国产替代/兼容芯片"(MPU6500=0x70 等)只要 I2C 能读写也放行。
 * @param out_diag  输出诊断信息(who_am_i / used_addr)，可为 NULL
 * @return 0=成功；1=两个地址全 NACK；2=地址有ACK但WHO_AM_I可疑(已继续使用)
 */
uint8_t MPU6050_Init(Mpu6050Result *out_diag);

/**
 * @brief 陀螺仪零偏校准（必须静止放置，耗时约 2 秒！）
 *        校准完成后，静止平放 Roll/Pitch 漂移 < 0.5°/min
 */
void MPU6050_Calibrate(void);

/**
 * @brief 读取一次原始传感器并更新姿态角（互补滤波）
 *        每 ~10~20ms 调用一次最佳；调用间隔会被内部记录用于积分
 * @param out  输出结果（包含 Roll/Pitch 与 6 轴原始值）
 * @note   Roll/Pitch 采用 加速度静态角度 + 陀螺仪积分 的互补滤波，
 *         0.98 陀螺权重保证短时间动态，0.02 加速度权重校正长时间漂移。
 */
void MPU6050_Update(Mpu6050Result *out);

/**
 * @brief 把俯仰角转换成距离修正系数 = cos(pitch°)
 *        超声波测的是"斜距"，真实水平距离 = 斜距 × cos(pitch)
 * @param pitch_deg 俯仰角（度）
 * @return 修正系数 k（<=1，0°时=1）
 */
float MPU6050_PitchCorrection(float pitch_deg);

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_H */
