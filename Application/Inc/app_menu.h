#ifndef __APP_MENU_H
#define __APP_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ====== 菜单状态机 (3 级: 主菜单 → Alarm Setup / Display Set → 参数修改) ====== */
typedef enum {
    STATE_MAIN_DISPLAY,
    STATE_MENU,
    STATE_ALARM_SET,
    STATE_DISPLAY_SET,
    STATE_SET_SAFE_DIST,
    STATE_SET_WARN_DIST,
    STATE_SET_DANGER_DIST,
    STATE_SET_BUZZER,
    STATE_SET_BRIGHTNESS,
} MenuState;

/* ====== 编码器事件 ====== */
typedef enum {
    ENC_NONE,
    ENC_CW,
    ENC_CCW,
    ENC_CLICK,
    ENC_LONG_PRESS,
} EncoderEvent;

/* 菜单当前状态 (DisplayTask 需读它分支显示, 故暴露为 extern) */
extern MenuState current_state;

/* 菜单显示 (含 OLED_Refresh, 由 DisplayTask 持 xMutexI2C 调用) */
void menu_display(void);

/* 菜单事件处理 (无 I2C 调用, 不需持锁) */
void menu_handle_event(EncoderEvent event);

/* 编码器读取 (HAL_GetTick 非阻塞消抖, 不阻塞主循环) */
EncoderEvent encoder_read(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_MENU_H */
