#include "filter.h"

// ====== 中值滤波（冒泡排序取中值，不修改原数组） ======
float filter_median(float *samples, uint8_t n) {
    float tmp[FILTER_MEDIAN_MAX];

    // 复制到临时数组
    for (uint8_t i = 0; i < n; i++) {
        tmp[i] = samples[i];
    }

    // 冒泡排序（从小到大）
    for (uint8_t i = 0; i < n - 1; i++) {
        for (uint8_t j = 0; j < n - 1 - i; j++) {
            if (tmp[j] > tmp[j + 1]) {
                float t = tmp[j];
                tmp[j] = tmp[j + 1];
                tmp[j + 1] = t;
            }
        }
    }

    // 取中间值，自动去掉最大和最小的异常值
    return tmp[n / 2];
}

// ====== 滑动平均滤波器（环形缓冲区） ======
void filter_avg_init(FilterMovingAvg *f, float *buf, int size) {
    f->buffer = buf;
    f->size   = size;
    f->index  = 0;
    f->count  = 0;
    for (int i = 0; i < size; i++) {
        f->buffer[i] = 0.0f;
    }
}

float filter_avg_update(FilterMovingAvg *f, float value) {
    // 1. 新数据写入当前位置（覆盖最老的数据）
    f->buffer[f->index] = value;

    // 2. 索引前移（环形循环）
    f->index = (f->index + 1) % f->size;

    // 3. 统计有效数据个数
    if (f->count < f->size) {
        f->count++;
    }

    // 4. 计算平均值
    float sum = 0.0f;
    for (int i = 0; i < f->count; i++) {
        sum += f->buffer[i];
    }
    return sum / (float)f->count;
}
