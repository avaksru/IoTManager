#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *switch_ch_dhw;
    lv_obj_t *main_panel;
    lv_obj_t *weather;
    lv_obj_t *wez_cod;
    lv_obj_t *wez_temp;
    lv_obj_t *wez_range;
    lv_obj_t *arc;
    lv_obj_t *main_minus;
    lv_obj_t *main_plus;
    lv_obj_t *text_top_right;
    lv_obj_t *text_top_center;
    lv_obj_t *text_center;
    lv_obj_t *text_center_celsius;
    lv_obj_t *boiler_status;
    lv_obj_t *boiler_status_image;
    lv_obj_t *ch_status;
    lv_obj_t *_h_curent_temp;
    lv_obj_t *ch_c;
    lv_obj_t *ch_image;
    lv_obj_t *dhw_status;
    lv_obj_t *dhw_curent_temp;
    lv_obj_t *dhw_image;
    lv_obj_t *dhw_c;
    lv_obj_t *flame_status;
    lv_obj_t *flame_curent;
    lv_obj_t *flame_image;
    lv_obj_t *flame_persent;
    lv_obj_t *text_bottom;
    lv_obj_t *text_bottom_celsius;
    lv_obj_t *btn_auto;
    lv_obj_t *text_auto;
    lv_obj_t *lock;
    lv_obj_t *relay1;
    lv_obj_t *btn1_text;
    lv_obj_t *relay2;
    lv_obj_t *btn2_text;
    lv_obj_t *relay3;
    lv_obj_t *btn3_text;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
};

void create_screen_main();
void delete_screen_main();
void tick_screen_main();

void create_screen_by_id(enum ScreensEnum screenId);
void delete_screen_by_id(enum ScreensEnum screenId);
void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/