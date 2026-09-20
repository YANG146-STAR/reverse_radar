#include "app_menu.h"
#include "app_config.h"     /* sys_config */
#include "oled.h"            /* OLED_ShowString/OLED_Refresh/OLED_Clear */
#include <stdio.h>

#define MENU_MAIN_ITEMS    2   // 1.Alarm Setup  2.Display Set
#define MENU_ALARM_ITEMS   3
#define MENU_DISPLAY_ITEMS 2

// ====== 菜单状态机变量 (current_state 暴露给 DisplayTask, menu_index 内部) ======
MenuState current_state = STATE_MAIN_DISPLAY;
static uint8_t  menu_index = 0;

// ====== 编码器读取 (HAL_GetTick 非阻塞消抖, 不阻塞主循环) ======
EncoderEvent encoder_read(void) {
    static uint8_t  last_clk = 1;
    static uint8_t  last_sw = 1;
    static uint32_t clk_debounce_tick = 0;   // 旋转消抖时间戳
    static uint32_t sw_press_time = 0;
    static uint8_t  sw_handled = 0;

    EncoderEvent event = ENC_NONE;

    // 旋转检测 (HAL_GetTick 非阻塞消抖 2ms)
    uint8_t clk = HAL_GPIO_ReadPin(ENC_CLK_GPIO_Port, ENC_CLK_Pin);
    if (clk != last_clk) {
        if (HAL_GetTick() - clk_debounce_tick >= 2) {
            uint8_t dt = HAL_GPIO_ReadPin(ENC_DT_GPIO_Port, ENC_DT_Pin);
            if (clk == 0) {
                event = (dt == 1) ? ENC_CW : ENC_CCW;
            }
        }
        clk_debounce_tick = HAL_GetTick();
        last_clk = clk;
    }

    // 按压检测
    uint8_t sw = HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin);
    if (sw == 0 && last_sw == 1) {
        sw_press_time = HAL_GetTick();
        sw_handled = 0;
    }
    if (sw == 0 && !sw_handled) {
        if (HAL_GetTick() - sw_press_time > 1000) {
            event = ENC_LONG_PRESS;
            sw_handled = 1;
        }
    }
    if (sw == 1 && last_sw == 0) {
        if (!sw_handled) {
            event = ENC_CLICK;
        }
        sw_handled = 0;
    }
    last_sw = sw;
    return event;
}

// ====== 菜单显示 (英文, ASCII 5x7, 每行 1 页高) ======
// 注意: 由 DisplayTask 持 xMutexI2C 调用, 内部 OLED_Refresh 也安全
void menu_display(void) {
    char buf[22];

    switch (current_state) {

        case STATE_MENU:
            OLED_ShowString(0, 0, "== Menu ==");
            if (menu_index == 0) OLED_ShowString(0, 2, ">1.Alarm Setup ");
            else                 OLED_ShowString(0, 2, " 1.Alarm Setup ");
            if (menu_index == 1) OLED_ShowString(0, 3, ">2.Display Set ");
            else                 OLED_ShowString(0, 3, " 2.Display Set ");
            OLED_ShowString(0, 5, "                ");
            OLED_ShowString(0, 7, "                ");
            OLED_Refresh();
            break;

        case STATE_ALARM_SET:
            OLED_ShowString(0, 0, "== Alarm Setup ");
            if (menu_index == 0) OLED_ShowString(0, 2, "> Safe Dist  ");
            else                 OLED_ShowString(0, 2, "  Safe Dist  ");
            if (menu_index == 1) OLED_ShowString(0, 4, "> Warn Dist  ");
            else                 OLED_ShowString(0, 4, "  Warn Dist  ");
            if (menu_index == 2) OLED_ShowString(0, 6, "> Danger Dist");
            else                 OLED_ShowString(0, 6, "  Danger Dist");
            OLED_Refresh();
            break;

        case STATE_SET_SAFE_DIST:
        case STATE_SET_WARN_DIST:
        case STATE_SET_DANGER_DIST: {
            const char *title;
            float val;
            if (current_state == STATE_SET_SAFE_DIST)       { title = "Safe Dist";   val = sys_config.safe_dist; }
            else if (current_state == STATE_SET_WARN_DIST)   { title = "Warn Dist";   val = sys_config.warn_dist; }
            else                                              { title = "Danger Dist"; val = sys_config.danger_dist; }
            OLED_ShowString(0, 0, title);
            OLED_ShowString(60, 0, "          ");
            OLED_ShowString(0, 2, "  Value: ");
            sprintf(buf, "%4.0fcm      ", val);
            OLED_ShowString(48, 2, buf);
            OLED_ShowString(0, 4, "  Adjust: Turn ");
            OLED_ShowString(0, 6, "  Click: Confirm");
            OLED_Refresh();
            break;
        }

        case STATE_DISPLAY_SET:
            OLED_ShowString(0, 0, "== Display Set ");
            if (menu_index == 0) OLED_ShowString(0, 2, "> Buzzer    ");
            else                 OLED_ShowString(0, 2, "  Buzzer    ");
            if (menu_index == 1) OLED_ShowString(0, 4, "> Brightness");
            else                 OLED_ShowString(0, 4, "  Brightness");
            OLED_ShowString(0, 6, "                ");
            OLED_Refresh();
            break;

        case STATE_SET_BUZZER:
            OLED_ShowString(0, 0, "Buzzer        ");
            OLED_ShowString(0, 2, "  Now: ");
            if (sys_config.buzzer_enable) OLED_ShowString(48, 2, "ON  ");
            else                          OLED_ShowString(48, 2, "OFF ");
            OLED_ShowString(0, 4, "  Adjust: Turn ");
            OLED_ShowString(0, 6, "  Click: Confirm");
            OLED_Refresh();
            break;

        case STATE_SET_BRIGHTNESS:
            OLED_ShowString(0, 0, "Brightness    ");
            OLED_ShowString(0, 2, "  Now: ");
            sprintf(buf, "%3d            ", sys_config.brightness);
            OLED_ShowString(48, 2, buf);
            OLED_ShowString(0, 4, "  Adjust: Turn ");
            OLED_ShowString(0, 6, "  Click: Confirm");
            OLED_Refresh();
            break;

        default:
            break;
    }
}

// ====== 菜单事件处理 (无 I2C 调用, 不需持锁) ======
void menu_handle_event(EncoderEvent event) {
    if (event == ENC_NONE) return;

    switch (current_state) {

        case STATE_MAIN_DISPLAY:
            if (event == ENC_LONG_PRESS || event == ENC_CLICK) {
                current_state = STATE_MENU;
                menu_index = 0;
                OLED_Clear();
            }
            break;

        case STATE_MENU:
            if (event == ENC_CW)       menu_index = (menu_index + 1) % MENU_MAIN_ITEMS;
            else if (event == ENC_CCW) menu_index = (menu_index + MENU_MAIN_ITEMS - 1) % MENU_MAIN_ITEMS;
            else if (event == ENC_CLICK) {
                if      (menu_index == 0) { current_state = STATE_ALARM_SET;   menu_index = 0; }
                else                       { current_state = STATE_DISPLAY_SET; menu_index = 0; }
                OLED_Clear();
            } else if (event == ENC_LONG_PRESS) {
                current_state = STATE_MAIN_DISPLAY;
                OLED_Clear();
            }
            break;

        case STATE_ALARM_SET:
            if (event == ENC_CW) menu_index = (menu_index + 1) % 3;
            else if (event == ENC_CCW) menu_index = (menu_index + 2) % 3;
            else if (event == ENC_CLICK) {
                if (menu_index == 0) current_state = STATE_SET_SAFE_DIST;
                else if (menu_index == 1) current_state = STATE_SET_WARN_DIST;
                else current_state = STATE_SET_DANGER_DIST;
                OLED_Clear();
            } else if (event == ENC_LONG_PRESS) {
                current_state = STATE_MENU; menu_index = 0;
                OLED_Clear();
            }
            break;

        case STATE_SET_SAFE_DIST:
            if (event == ENC_CW) { sys_config.safe_dist += 1; if (sys_config.safe_dist > 200) sys_config.safe_dist = 200; }
            else if (event == ENC_CCW) { sys_config.safe_dist -= 1; if (sys_config.safe_dist < sys_config.warn_dist) sys_config.safe_dist = sys_config.warn_dist; }
            else if (event == ENC_CLICK || event == ENC_LONG_PRESS) {
                current_state = STATE_ALARM_SET; menu_index = 0; OLED_Clear();
            }
            break;

        case STATE_SET_WARN_DIST:
            if (event == ENC_CW) { sys_config.warn_dist += 1; if (sys_config.warn_dist > sys_config.safe_dist) sys_config.warn_dist = sys_config.safe_dist; }
            else if (event == ENC_CCW) { sys_config.warn_dist -= 1; if (sys_config.warn_dist < sys_config.danger_dist) sys_config.warn_dist = sys_config.danger_dist; }
            else if (event == ENC_CLICK || event == ENC_LONG_PRESS) {
                current_state = STATE_ALARM_SET; menu_index = 1; OLED_Clear();
            }
            break;

        case STATE_SET_DANGER_DIST:
            if (event == ENC_CW) { sys_config.danger_dist += 1; if (sys_config.danger_dist > sys_config.warn_dist) sys_config.danger_dist = sys_config.warn_dist; }
            else if (event == ENC_CCW) { sys_config.danger_dist -= 1; if (sys_config.danger_dist < 5) sys_config.danger_dist = 5; }
            else if (event == ENC_CLICK || event == ENC_LONG_PRESS) {
                current_state = STATE_ALARM_SET; menu_index = 2; OLED_Clear();
            }
            break;

        case STATE_DISPLAY_SET:
            if (event == ENC_CW)       menu_index = (menu_index + 1) % MENU_DISPLAY_ITEMS;
            else if (event == ENC_CCW) menu_index = (menu_index + MENU_DISPLAY_ITEMS - 1) % MENU_DISPLAY_ITEMS;
            else if (event == ENC_CLICK) {
                if (menu_index == 0) current_state = STATE_SET_BUZZER;
                else current_state = STATE_SET_BRIGHTNESS;
                OLED_Clear();
            } else if (event == ENC_LONG_PRESS) { current_state = STATE_MENU; menu_index = 1; OLED_Clear(); }
            break;

        case STATE_SET_BUZZER:
            if (event == ENC_CW || event == ENC_CCW) sys_config.buzzer_enable = !sys_config.buzzer_enable;
            else if (event == ENC_CLICK || event == ENC_LONG_PRESS) {
                current_state = STATE_DISPLAY_SET; menu_index = 0; OLED_Clear();
            }
            break;

        case STATE_SET_BRIGHTNESS:
            if (event == ENC_CW) { sys_config.brightness += 32; if (sys_config.brightness > 255) sys_config.brightness = 255; }
            else if (event == ENC_CCW) { if (sys_config.brightness > 32) sys_config.brightness -= 32; }
            else if (event == ENC_CLICK || event == ENC_LONG_PRESS) {
                current_state = STATE_DISPLAY_SET; menu_index = 1; OLED_Clear();
            }
            break;

        default:
            current_state = STATE_MAIN_DISPLAY;
            break;
    }
}
