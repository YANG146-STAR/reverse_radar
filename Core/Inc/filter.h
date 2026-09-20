#ifndef __FILTER_H
#define __FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// 中值滤波最大支持点数（当前超声 5 点、MPU6050 3 点）
#define FILTER_MEDIAN_MAX  9

/**
 * @brief 中值滤波：对 samples[0..n-1] 冒泡排序后取中值
 * @param samples 输入样本（不会被修改，内部复制后排序）
 * @param n       样本个数（要求为奇数且 <= FILTER_MEDIAN_MAX）
 * @return 中值（第 n/2 大的值，自动剔除最大/最小异常值）
 */
float filter_median(float *samples, uint8_t n);

// 滑动平均滤波器（环形缓冲区，缓冲内存由调用者提供）
typedef struct {
    float *buffer;   // 环形缓冲区指针
    int    size;     // 缓冲区长度
    int    index;    // 当前写入位置
    int    count;    // 已存数据个数（未满时按 count 平均）
} FilterMovingAvg;

/**
 * @brief 初始化滑动平均滤波器
 * @param f    滤波器对象
 * @param buf  外部提供的 float 缓冲区
 * @param size 缓冲区长度
 */
void  filter_avg_init(FilterMovingAvg *f, float *buf, int size);

/**
 * @brief 写入一个新值，返回当前滑动平均值
 */
float filter_avg_update(FilterMovingAvg *f, float value);

#ifdef __cplusplus
}
#endif

#endif /* __FILTER_H */
