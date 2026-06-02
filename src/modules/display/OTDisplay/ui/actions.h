#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_btn_auto_press(lv_event_t * e);
extern void action_main_minus_clic(lv_event_t * e);
extern void action_main_minus_hold(lv_event_t * e);
extern void action_main_plus_hold(lv_event_t * e);
extern void action_main_plus_clic(lv_event_t * e);
extern void action_ch_click(lv_event_t * e);
extern void action_dhw_click(lv_event_t * e);
extern void action_btn_ch_dhw_on(lv_event_t * e);
extern void action_btn_ch_dhw_off(lv_event_t * e);
extern void action_text_top_right_pressed(lv_event_t * e);
extern void action_callback(lv_event_t * e);
extern void action_arc_value_change(lv_event_t * e);
extern void action_boiler_click(lv_event_t * e);
extern void action_arc_value_released(lv_event_t * e);
extern void action_text_centr_click(lv_event_t * e);
extern void action_text_centr_hold(lv_event_t * e);
extern void action_relay1_press(lv_event_t * e);
extern void action_relay2_press(lv_event_t * e);
extern void action_relay3_press(lv_event_t * e);
extern void action_weather_pressed(lv_event_t * e);


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/