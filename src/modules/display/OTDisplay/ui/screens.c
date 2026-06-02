#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;
lv_obj_t *tick_value_change_obj;
uint32_t active_theme_index = 0;

static void event_handler_unchecked_cb_main_switch_ch_dhw(lv_event_t *e) {
    lv_obj_t *ta = lv_event_get_target(e);
    if (!lv_obj_has_state(ta, LV_STATE_CHECKED)) {
        action_btn_ch_dhw_off(e);
    }
}

static void event_handler_checked_cb_main_switch_ch_dhw(lv_event_t *e) {
    lv_obj_t *ta = lv_event_get_target(e);
    if (lv_obj_has_state(ta, LV_STATE_CHECKED)) {
        action_btn_ch_dhw_on(e);
    }
}

void create_screen_main() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 480);
    {
        lv_obj_t *parent_obj = obj;
        {
            // main_panel
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.main_panel = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 480, 480);
            lv_obj_add_event_cb(obj, action_callback, LV_EVENT_PRESSED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff0e1621), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(obj, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // weather
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.weather = obj;
                    lv_obj_set_pos(obj, -18, 81);
                    lv_obj_set_size(obj, 200, 77);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // wez_cod
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.wez_cod = obj;
                            lv_obj_set_pos(obj, 86, -2);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_w0);
                            lv_image_set_scale(obj, 470);
                        }
                        {
                            // wez_temp
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.wez_temp = obj;
                            lv_obj_set_pos(obj, -15, -17);
                            lv_obj_set_size(obj, 80, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff7cdcff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                        {
                            // wez_range
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.wez_range = obj;
                            lv_obj_set_pos(obj, -17, 23);
                            lv_obj_set_size(obj, 80, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
                {
                    // ARC
                    lv_obj_t *obj = lv_arc_create(parent_obj);
                    objects.arc = obj;
                    lv_obj_set_pos(obj, 72, 100);
                    lv_obj_set_size(obj, 300, 300);
                    lv_arc_set_value(obj, 0);
                    lv_arc_set_bg_start_angle(obj, 145);
                    lv_arc_set_bg_end_angle(obj, 35);
                    lv_obj_add_event_cb(obj, action_arc_value_released, LV_EVENT_RELEASED, (void *)0);
                    lv_obj_add_event_cb(obj, action_arc_value_change, LV_EVENT_VALUE_CHANGED, (void *)0);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_style_arc_width(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_opa(obj, 150, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                }
                {
                    // main_minus
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.main_minus = obj;
                    lv_obj_set_pos(obj, -18, 153);
                    lv_obj_set_size(obj, 58, 236);
                    lv_obj_add_event_cb(obj, action_main_minus_clic, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_add_event_cb(obj, action_main_minus_hold, LV_EVENT_LONG_PRESSED, (void *)0);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // main_plus
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.main_plus = obj;
                    lv_obj_set_pos(obj, 397, 153);
                    lv_obj_set_size(obj, 68, 236);
                    lv_obj_add_event_cb(obj, action_main_plus_clic, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_add_event_cb(obj, action_main_plus_hold, LV_EVENT_LONG_PRESSED, (void *)0);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // text_top_right
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.text_top_right = obj;
                    lv_obj_set_pos(obj, 316, -10);
                    lv_obj_set_size(obj, 140, LV_SIZE_CONTENT);
                    lv_obj_add_event_cb(obj, action_text_top_right_pressed, LV_EVENT_PRESSED, (void *)1);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN|LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // text_top_center
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.text_top_center = obj;
                    lv_obj_set_pos(obj, 0, 57);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_opa(obj, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "live-control.com");
                }
                {
                    // text_center
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.text_center = obj;
                    lv_obj_set_pos(obj, -5, 12);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_event_cb(obj, action_text_centr_hold, LV_EVENT_LONG_PRESSED, (void *)0);
                    lv_obj_add_event_cb(obj, action_text_centr_click, LV_EVENT_SHORT_CLICKED, (void *)0);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN|LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_helvetica_bold_r72, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_opa(obj, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "0");
                }
                {
                    // text_center_celsius
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.text_center_celsius = obj;
                    lv_obj_set_pos(obj, 293, 191);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_opa(obj, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "°C");
                }
                {
                    // switch_ch_dhw
                    lv_obj_t *obj = lv_switch_create(parent_obj);
                    objects.switch_ch_dhw = obj;
                    lv_obj_set_pos(obj, -1, 331);
                    lv_obj_set_size(obj, 50, 25);
                    lv_obj_add_event_cb(obj, event_handler_unchecked_cb_main_switch_ch_dhw, LV_EVENT_VALUE_CHANGED, (void *)0);
                    lv_obj_add_event_cb(obj, event_handler_checked_cb_main_switch_ch_dhw, LV_EVENT_VALUE_CHANGED, (void *)0);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_state(obj, LV_STATE_CHECKED);
                    lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // boiler_status
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.boiler_status = obj;
                    lv_obj_set_pos(obj, -18, 50);
                    lv_obj_set_size(obj, 35, 35);
                    lv_obj_add_event_cb(obj, action_boiler_click, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff17212b), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // boiler_status_image
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.boiler_status_image = obj;
                            lv_obj_set_pos(obj, -10, -12);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_err);
                            lv_image_set_scale(obj, 350);
                            lv_obj_set_style_image_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    // ch_status
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.ch_status = obj;
                    lv_obj_set_pos(obj, -10, -10);
                    lv_obj_set_size(obj, 100, 55);
                    lv_obj_add_event_cb(obj, action_ch_click, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff17212b), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // сh_curent_temp
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects._h_curent_temp = obj;
                            lv_obj_set_pos(obj, 19, -10);
                            lv_obj_set_size(obj, 48, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                        {
                            // ch_c
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.ch_c = obj;
                            lv_obj_set_pos(obj, 64, -10);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_text_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "°C");
                        }
                        {
                            // ch_image
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.ch_image = obj;
                            lv_obj_set_pos(obj, -16, -8);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_ch_no_active);
                            lv_obj_set_style_image_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    // dhw_status
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.dhw_status = obj;
                    lv_obj_set_pos(obj, 93, -10);
                    lv_obj_set_size(obj, 100, 55);
                    lv_obj_add_event_cb(obj, action_dhw_click, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff17212b), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // dhw_curent_temp
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.dhw_curent_temp = obj;
                            lv_obj_set_pos(obj, 20, -10);
                            lv_obj_set_size(obj, 48, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                        {
                            // dhw_image
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.dhw_image = obj;
                            lv_obj_set_pos(obj, -16, -8);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_dhw_no_active);
                            lv_obj_set_style_image_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // dhw_c
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.dhw_c = obj;
                            lv_obj_set_pos(obj, 64, -10);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_text_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "°C");
                        }
                    }
                }
                {
                    // flame_status
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.flame_status = obj;
                    lv_obj_set_pos(obj, 196, -11);
                    lv_obj_set_size(obj, 100, 55);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff182533), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // flame_curent
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.flame_curent = obj;
                            lv_obj_set_pos(obj, 12, -8);
                            lv_obj_set_size(obj, 54, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                        {
                            // flame_image
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.flame_image = obj;
                            lv_obj_set_pos(obj, -16, -8);
                            lv_obj_set_size(obj, 35, 35);
                            lv_image_set_src(obj, &img_flame);
                            lv_image_set_scale(obj, 350);
                            lv_obj_set_style_image_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // flame_persent
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.flame_persent = obj;
                            lv_obj_set_pos(obj, 67, -10);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_letter_space(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "%");
                        }
                    }
                }
                {
                    // text_bottom
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.text_bottom = obj;
                    lv_obj_set_pos(obj, 193, 79);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // text_bottom_celsius
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.text_bottom_celsius = obj;
                    lv_obj_set_pos(obj, 438, 81);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_style_text_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "°C");
                }
                {
                    // btn_auto
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.btn_auto = obj;
                    lv_obj_set_pos(obj, 157, 320);
                    lv_obj_set_size(obj, 130, 70);
                    lv_obj_add_event_cb(obj, action_btn_auto_press, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(0xff184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // text_auto
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.text_auto = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_roboto20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
                {
                    // lock
                    lv_obj_t *obj = lv_image_create(parent_obj);
                    objects.lock = obj;
                    lv_obj_set_pos(obj, 211, 77);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_image_set_src(obj, &img_lock);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_MOMENTUM);
                }
                {
                    // relay1
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.relay1 = obj;
                    lv_obj_set_pos(obj, -10, 415);
                    lv_obj_set_size(obj, 150, 40);
                    lv_obj_add_event_cb(obj, action_relay1_press, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(0xff184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn1_text
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn1_text = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_roboto20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
                {
                    // relay2
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.relay2 = obj;
                    lv_obj_set_pos(obj, 147, 415);
                    lv_obj_set_size(obj, 150, 40);
                    lv_obj_add_event_cb(obj, action_relay2_press, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(0xff184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn2_text
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn2_text = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_roboto20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
                {
                    // relay3
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.relay3 = obj;
                    lv_obj_set_pos(obj, 305, 415);
                    lv_obj_set_size(obj, 150, 40);
                    lv_obj_add_event_cb(obj, action_relay3_press, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(0xff184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn3_text
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn3_text = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_roboto20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
            }
        }
    }
    
    tick_screen_main();
}

void delete_screen_main() {
    lv_obj_delete(objects.main);
    objects.main = 0;
    objects.main_panel = 0;
    objects.weather = 0;
    objects.wez_cod = 0;
    objects.wez_temp = 0;
    objects.wez_range = 0;
    objects.arc = 0;
    objects.main_minus = 0;
    objects.main_plus = 0;
    objects.text_top_right = 0;
    objects.text_top_center = 0;
    objects.text_center = 0;
    objects.text_center_celsius = 0;
    objects.switch_ch_dhw = 0;
    objects.boiler_status = 0;
    objects.boiler_status_image = 0;
    objects.ch_status = 0;
    objects._h_curent_temp = 0;
    objects.ch_c = 0;
    objects.ch_image = 0;
    objects.dhw_status = 0;
    objects.dhw_curent_temp = 0;
    objects.dhw_image = 0;
    objects.dhw_c = 0;
    objects.flame_status = 0;
    objects.flame_curent = 0;
    objects.flame_image = 0;
    objects.flame_persent = 0;
    objects.text_bottom = 0;
    objects.text_bottom_celsius = 0;
    objects.btn_auto = 0;
    objects.text_auto = 0;
    objects.lock = 0;
    objects.relay1 = 0;
    objects.btn1_text = 0;
    objects.relay2 = 0;
    objects.btn2_text = 0;
    objects.relay3 = 0;
    objects.btn3_text = 0;
}

void tick_screen_main() {
}



typedef void (*create_screen_func_t)();
create_screen_func_t create_screen_funcs[] = {
    create_screen_main,
};
void create_screen(int screen_index) {
    create_screen_funcs[screen_index]();
}
void create_screen_by_id(enum ScreensEnum screenId) {
    create_screen_funcs[screenId - 1]();
}

typedef void (*delete_screen_func_t)();
delete_screen_func_t delete_screen_funcs[] = {
    delete_screen_main,
};
void delete_screen(int screen_index) {
    delete_screen_funcs[screen_index]();
}
void delete_screen_by_id(enum ScreensEnum screenId) {
    delete_screen_funcs[screenId - 1]();
}

typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
};
void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}

void create_screens() {
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    create_screen_main();
}
