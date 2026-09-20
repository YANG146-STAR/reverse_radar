#include "mpu6050.h"
#include "i2c.h"
#include <math.h>
#include <string.h>  // memset
#include "filter.h"  // 公共中值滤波 + 滑动平均

// ====== MPU6050 寄存器定义 ======
#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CFG     0x1B
#define MPU6050_REG_ACCEL_CFG    0x1C
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_WHO_AM_I     0x75
#define MPU6050_WHO_AM_I_VAL     0x68

// 量程设置（保持与寄存器配置严格一致，Experience 434841 警告）
// 加速度 ±2g → 16384 LSB/g；陀螺仪 ±500°/s → 65.5 LSB/(°/s)
#define ACCEL_LSB_PER_G   16384.0f
#define GYRO_LSB_PER_DPS  65.5f

#define DEG_TO_RAD        (3.14159265358979f / 180.0f)
#define RAD_TO_DEG        (180.0f / 3.14159265358979f)

// ============ 防抖 / 抗漂移 参数 ============
// 零偏校准：初始化静止采集次数（每 2ms 一次 → 2 秒校准完）
#define CALIB_SAMPLES     1000

// ============================================================
//  姿态角两层滤波统一走 Core/Inc/filter.h（filter_median / FilterMovingAvg）
//  GYRO_FILTER_SIZE = 3：无 FPU 的 M3 上每帧省 2 次 I2C 读 + 2 次软浮点 atan2（原 5 次）
// ============================================================
#define GYRO_FILTER_SIZE 3

// ============ 全局持久状态 ============
static uint8_t  s_ready = 0;
static uint8_t  s_addr_7bit = 0x68;
static float    s_gyro_bias_x = 0.0f;   // 陀螺仪零偏（°/s，校准后扣除）
static float    s_gyro_bias_y = 0.0f;
static float    s_gyro_bias_z = 0.0f;
static float    s_last_roll  = 0.0f;   // 最近一次有效解角（I2C 失败帧填补用）
static float    s_last_pitch = 0.0f;

static inline uint16_t HAL_ADDR(void) { return (uint16_t)s_addr_7bit << 1; }

// ================================================================
//  F1 硬件 I2C BUSY 死锁恢复（和 oled.c 对称）
//  每次调用前 ForceReady (State=READY + SWRST 清 BUSY 残位)，
//  失败 ≥ 2 次关 I2C1 时钟 + MX_I2C1_Init 全量重初始化。
// ================================================================
extern void MX_I2C1_Init(void);
// 和 oled.c 对称：只强制清 HAL 状态机，不瞎做 SWRST（那会引发 GPIO 重初始化死循环）
static void MPU_I2C1_ForceReady(void) {
    hi2c1.State = HAL_I2C_STATE_READY;
    hi2c1.Mode  = HAL_I2C_MODE_NONE;
}

static void MPU_I2C1_Recover(void) {
    MPU_I2C1_ForceReady();
    static uint8_t s_fail_cnt = 0;
    s_fail_cnt++;
    if (s_fail_cnt >= 2) {
        s_fail_cnt = 0;
        __HAL_RCC_I2C1_CLK_DISABLE();
        for (volatile uint32_t i = 0; i < 24; i++) __NOP();
        MX_I2C1_Init();
        NVIC_DisableIRQ(I2C1_EV_IRQn);
        NVIC_DisableIRQ(I2C1_ER_IRQn);
    }
}

static uint8_t MPU_WriteReg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    MPU_I2C1_ForceReady();
    HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(&hi2c1, HAL_ADDR(), buf, 2, MPU6050_TO);
    if (st != HAL_OK) { MPU_I2C1_Recover(); return HAL_ERROR; }
    return HAL_OK;
}

static uint8_t MPU_ReadRegs(uint8_t reg, uint8_t len, uint8_t *buf)
{
    MPU_I2C1_ForceReady();
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c1, HAL_ADDR(),
                            reg, I2C_MEMADD_SIZE_8BIT, buf, len, MPU6050_TO);
    if (st != HAL_OK) { MPU_I2C1_Recover(); return HAL_ERROR; }
    return HAL_OK;
}

static uint8_t probe_addr(uint8_t addr7, uint8_t *out_who)
{
    s_addr_7bit = addr7;
    if (MPU_ReadRegs(MPU6050_REG_WHO_AM_I, 1, out_who) != HAL_OK) return 0;
    if (*out_who == 0x00 || *out_who == 0xFF) return 0;
    return 1;
}

// ====== 上电静止校准陀螺仪零偏（Experience 434841 核心修复：消除积分漂移根因）======
static void gyro_calibrate_zerobias(void)
{
    float sx = 0, sy = 0, sz = 0;
    uint16_t ok_count = 0;
    for (uint16_t i = 0; i < CALIB_SAMPLES; i++) {
        uint8_t raw[14];
        int16_t gx, gy, gz;
        if (MPU_ReadRegs(MPU6050_REG_ACCEL_XOUT_H, 14, raw) != HAL_OK) continue;
        gx = (int16_t)((raw[8]  << 8) | raw[9]);
        gy = (int16_t)((raw[10] << 8) | raw[11]);
        gz = (int16_t)((raw[12] << 8) | raw[13]);
        sx += (float)gx;
        sy += (float)gy;
        sz += (float)gz;
        ok_count++;
        HAL_Delay(2);  // 每 2ms 采一次，总时长≈2s
    }
    if (ok_count == 0) { s_gyro_bias_x = 0; return; }
    s_gyro_bias_x = (sx / (float)ok_count) / GYRO_LSB_PER_DPS;  // 换算为 °/s
    s_gyro_bias_y = (sy / (float)ok_count) / GYRO_LSB_PER_DPS;
    s_gyro_bias_z = (sz / (float)ok_count) / GYRO_LSB_PER_DPS;
}

uint8_t MPU6050_Init(Mpu6050Result *out_diag)
{
    uint8_t who = 0;
    const uint8_t candidates[] = {0x68, 0x69};
    uint8_t found = 0;

    for (uint8_t i = 0; i < sizeof(candidates); i++) {
        if (probe_addr(candidates[i], &who)) { found = 1; break; }
    }
    if (!found) {
        s_ready = 0;
        s_addr_7bit = 0x68;
        if (out_diag) {
            out_diag->who_am_i = who;
            out_diag->used_addr = 0x68;
            out_diag->init_ok = 0;
        }
        return 1;
    }

    uint8_t suspicious = (who != 0x68 && who != 0x70) ? 1 : 0;

    MPU_WriteReg(MPU6050_REG_PWR_MGMT_1, 0x00);
    HAL_Delay(50);
    MPU_WriteReg(MPU6050_REG_SMPLRT_DIV, 1);              // 采样率 500Hz (1kHz / (1+1))，配合 2ms 校准
    MPU_WriteReg(MPU6050_REG_CONFIG, 0x03);               // DLPF 42Hz
    MPU_WriteReg(MPU6050_REG_GYRO_CFG, 0x08);             // ±500°/s
    MPU_WriteReg(MPU6050_REG_ACCEL_CFG, 0x00);            // ±2g

    s_ready = 1;

    if (out_diag) {
        out_diag->who_am_i = who;
        out_diag->used_addr = s_addr_7bit;
        out_diag->init_ok = 1;
    }

    // 在校准前清零已有的零偏缓存
    s_gyro_bias_x = s_gyro_bias_y = s_gyro_bias_z = 0.0f;

    return suspicious ? 2 : 0;
}

// ============ 安装方向映射（实测 4 组对角线动作反推）============
// 实物（板子平放，用指南针测方位）：
//   MPU6050 在板的正北方位，超声波在板的东南方位
//   车头方向 = 西北，车尾方向 = 东南（超声波发射方向）
//
// 实测反推 MPU 模块朝向：X+ 朝南，Y+ 朝东（不是 X+ 东 Y+ 南）
//   南 = +X_MPU, 东 = +Y_MPU, 北 = -X_MPU, 西 = -Y_MPU
//   西北(车头) = (-X,-Y)/√2 = (-1,-1)/√2  (MPU 坐标)
//   东南(车尾) = (+1,+1)/√2
//   东北(车右) = (-1,+1)/√2
//   西南(车左) = (+1,-1)/√2
//
// 车头方向 h = (-1,-1)/√2，车右方向 r = (-1,+1)/√2（右手定则验证 r×h=+Z）
//
// 把加速度投影到车右(r)/车头(h)方向，解耦 R/P：
//   a_车右 = (ay-ax)/√2     a_车头 = -(ax+ay)/√2
//
//   Roll+ = 车右倾(东北下沉) → 重力沿 +a_车右 → atan2(ay-ax, ...)
//   Pitch+ = 上坡(西北抬高) → 重力沿 -a_车头 = +(ax+ay)/√2 → atan2(ax+ay, ...)
// ==========================================================================

/**
 * 加速度算静态角度（车头在(-1,-1)/√2 对角线方向，坐标系旋转 45°）
 *   roll_acc  = atan2( ay - ax, sqrt((ax+ay)^2 + 2*az^2) )
 *   pitch_acc = atan2( ax + ay, sqrt((ay-ax)^2 + 2*az^2) )
 */
static void Accel_To_RollPitch(float ax, float ay, float az,
                               float *roll_deg, float *pitch_deg)
{
    float sum  = ax + ay;    // Pitch 轴向分量 ×√2
    float diff = ax - ay;    // Roll 轴向分量 ×√2  (原 ay-ax 符号反了→改为 ax-ay，让 Roll 正=右下沉)
    float roll_r  = atan2f( diff, sqrtf(sum*sum   + 2.0f*az*az) );
    float pitch_r = atan2f( sum,  sqrtf(diff*diff + 2.0f*az*az) );
    *roll_deg  = roll_r  * RAD_TO_DEG;
    *pitch_deg = pitch_r * RAD_TO_DEG;
}

/**
 * @brief 校准陀螺仪零偏：在上电初始化显示后调用（静止时！双手别碰板子 2 秒）
 *        这样 init_ok 状态和零偏解耦，零偏校准可在用户确认静止后做一次，
 *        避免在 MPU_Init 中阻塞 2 秒导致 OLED 诊断画面被用户跳过看不到。
 */
void MPU6050_Calibrate(void)
{
    if (!s_ready) return;
    gyro_calibrate_zerobias();
}

void MPU6050_Update(Mpu6050Result *out)
{
    uint8_t raw[14];
    int16_t ax, ay, az;
    int16_t gx, gy, gz;

    if (!s_ready) { out->init_ok = 0; return; }
    out->init_ok = 1;

    // --- 第一次调用：初始化 roll/pitch 各一条两层滤波器 ---
    static float s_avg_roll_buf[GYRO_FILTER_SIZE];
    static float s_avg_pitch_buf[GYRO_FILTER_SIZE];
    static FilterMovingAvg s_avg_roll, s_avg_pitch;
    static uint8_t s_filt_inited = 0;
    if (!s_filt_inited) {
        filter_avg_init(&s_avg_roll, s_avg_roll_buf, GYRO_FILTER_SIZE);
        filter_avg_init(&s_avg_pitch, s_avg_pitch_buf, GYRO_FILTER_SIZE);
        s_filt_inited = 1;
    }

    // --- GYRO_FILTER_SIZE 次连续 I2C 读取 → 每次独立解角 → 凑齐样本 ---
    float roll_buf[GYRO_FILTER_SIZE];
    float pitch_buf[GYRO_FILTER_SIZE];
    uint8_t ok = 0;               // 至少 1 次成功
    for (int i = 0; i < GYRO_FILTER_SIZE; i++) {
        if (MPU_ReadRegs(MPU6050_REG_ACCEL_XOUT_H, 14, raw) != 0) {
            // 本次 I2C 失败 → 用上次有效值填补（和超声波的 s_last_valid 逻辑一致）
            roll_buf[i]  = s_last_roll;
            pitch_buf[i] = s_last_pitch;
            continue;
        }
        ax = (int16_t)((raw[0] << 8) | raw[1]);
        ay = (int16_t)((raw[2] << 8) | raw[3]);
        az = (int16_t)((raw[4] << 8) | raw[5]);
        gx = (int16_t)((raw[8]  << 8) | raw[9]);
        gy = (int16_t)((raw[10] << 8) | raw[11]);
        gz = (int16_t)((raw[12] << 8) | raw[13]);

        float axg = (float)ax / ACCEL_LSB_PER_G;
        float ayg = (float)ay / ACCEL_LSB_PER_G;
        float azg = (float)az / ACCEL_LSB_PER_G;
        float gxd = (float)gx / GYRO_LSB_PER_DPS - s_gyro_bias_x;
        float gyd = (float)gy / GYRO_LSB_PER_DPS - s_gyro_bias_y;
        float gzd = (float)gz / GYRO_LSB_PER_DPS - s_gyro_bias_z;

        out->acc_x_g    = axg;
        out->acc_y_g    = ayg;
        out->acc_z_g    = azg;
        out->gyro_x_dps = gxd;
        out->gyro_y_dps = gyd;
        out->gyro_z_dps = gzd;

        float r, p;
        Accel_To_RollPitch(axg, ayg, azg, &r, &p);
        roll_buf[i]  = r;
        pitch_buf[i] = p;
        s_last_roll  = r;
        s_last_pitch = p;
        ok = 1;
    }

    // 本帧全部 I2C 失败 → out->init_ok 置 0，返回上一次有效输出
    if (!ok) {
        out->init_ok = 0;
        out->roll_deg  = s_last_roll;   // 滑窗里仍会填填补值，但至少返回值合理
        out->pitch_deg = s_last_pitch;
        return;
    }

    // --- 第一层：中值滤波（roll / pitch 各跑一次独立冒泡排序）---
    float med_roll  = filter_median(roll_buf, GYRO_FILTER_SIZE);
    float med_pitch = filter_median(pitch_buf, GYRO_FILTER_SIZE);

    // --- 第二层：滑动平均（环形 buffer[3]，各自 count 未满按 count 平均）---
    out->roll_deg  = filter_avg_update(&s_avg_roll,  med_roll);
    out->pitch_deg = filter_avg_update(&s_avg_pitch, med_pitch);
}

float MPU6050_PitchCorrection(float pitch_deg)
{
    if (pitch_deg >  89.0f) pitch_deg =  89.0f;
    if (pitch_deg < -89.0f) pitch_deg = -89.0f;
    return cosf(pitch_deg * DEG_TO_RAD);
}
