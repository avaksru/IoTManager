#ifndef EEZ_LVGL_UI_IMAGES_H
#define EEZ_LVGL_UI_IMAGES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_img_dsc_t img_ok;
extern const lv_img_dsc_t img_ch_no_active;
extern const lv_img_dsc_t img_dhw_no_active;
extern const lv_img_dsc_t img_ch_active;
extern const lv_img_dsc_t img_ch_off;
extern const lv_img_dsc_t img_dhw_active;
extern const lv_img_dsc_t img_dhw_off;
extern const lv_img_dsc_t img_err;
extern const lv_img_dsc_t img_flame;
extern const lv_img_dsc_t img_hot;
extern const lv_img_dsc_t img_lock;
extern const lv_img_dsc_t img_settings;
extern const lv_img_dsc_t img_sos;
extern const lv_img_dsc_t img_temp;
extern const lv_img_dsc_t img_time;
extern const lv_img_dsc_t img_trable;
extern const lv_img_dsc_t img_w4;
extern const lv_img_dsc_t img_w0;
extern const lv_img_dsc_t img_w1;
extern const lv_img_dsc_t img_w2;
extern const lv_img_dsc_t img_w3;
extern const lv_img_dsc_t img_w5;
extern const lv_img_dsc_t img_w6;
extern const lv_img_dsc_t img_w7;
extern const lv_img_dsc_t img_q1;
extern const lv_img_dsc_t img_q2;
extern const lv_img_dsc_t img_q3;
extern const lv_img_dsc_t img_q7;
extern const lv_img_dsc_t img_w8;
extern const lv_img_dsc_t img_w9;

#ifndef EXT_IMG_DESC_T
#define EXT_IMG_DESC_T
typedef struct _ext_img_desc_t {
    const char *name;
    const lv_img_dsc_t *img_dsc;
} ext_img_desc_t;
#endif

extern const ext_img_desc_t images[30];


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_IMAGES_H*/