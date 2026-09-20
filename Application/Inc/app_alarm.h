#ifndef __APP_ALARM_H
#define __APP_ALARM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ====== 报警策略: 依距离分区控制 RGB 颜色 + 蜂鸣器节奏 ======
   4 区: EMG(<danger) 红+60/60ms, DNG(<warn) 黄+80/120ms, WRN(<safe) 黄+150/350ms, SAFE 绿
   dist<0 时 (测距失败) 全灭 */
void alarm_update_config(float dist);

#ifdef __cplusplus
}
#endif
#endif /* __APP_ALARM_H */
