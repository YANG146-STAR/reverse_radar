#include "app_alarm.h"
#include "app_outputs.h"
#include "app_config.h"

// ====== 依距离分区控制 RGB + 蜂鸣器节奏 ======
//   zone 0 EMG  (dist<danger)  红   +  60/60ms 紧急快响
//   zone 1 DNG  (dist<warn)    紫   +  80/120ms 危险中速
//   zone 2 WRN  (dist<safe)    黄   + 150/350ms 警告慢响
//   zone 3 SAFE (>=safe)       绿   蜂鸣器关
//   zone 4 ERR  (dist<0)       灭   蜂鸣器关
// 调用方: AlarmTask (周期 10ms), 单线程
void alarm_update_config(float dist) {
    static uint8_t  last_zone = 0xFF;
    static uint8_t  buzzer_on = 0;
    static uint32_t last_toggle = 0;

    uint8_t zone;
    uint16_t on_ms = 0, off_ms = 0;

    if (dist < 0) {
        zone = 4;
    } else if (dist >= sys_config.safe_dist) {
        zone = 3;
    } else if (dist >= sys_config.warn_dist) {
        zone = 2; on_ms = 150; off_ms = 350;  // 警告: 慢响
    } else if (dist >= sys_config.danger_dist) {
        zone = 1; on_ms = 80;  off_ms = 120;  // 危险: 中速
    } else {
        zone = 0; on_ms = 60;  off_ms = 60;   // 紧急: 快响
    }

    if (zone != last_zone) {
        last_zone = zone;
        buzzer_on = 0;
        last_toggle = HAL_GetTick();
    }

    // RGB 颜色: 绿(SAFE) → 黄(WRN) → 紫(DNG) → 红(EMG)
    switch (zone) {
        case 0:  RGB_SetColor(1, 0, 0); break;   // 红
        case 1:  RGB_SetColor(1, 0, 1); break;   // 紫
        case 2:  RGB_SetColor(1, 1, 0); break;   // 黄
        case 3:  RGB_SetColor(0, 1, 0); break;   // 绿
        default: RGB_SetColor(0, 0, 0); break;   // 灭
    }

    if (zone >= 3 || !sys_config.buzzer_enable) {
        Buzzer_Off();
        return;
    }

    uint32_t now = HAL_GetTick();
    uint16_t period = buzzer_on ? on_ms : off_ms;
    if (now - last_toggle >= period) {
        buzzer_on = !buzzer_on;
        last_toggle = now;
        if (buzzer_on) Buzzer_On();
        else           Buzzer_Off();
    }
}
