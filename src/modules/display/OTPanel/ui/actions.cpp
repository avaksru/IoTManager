#include "actions.h"
#include "../OTPanel.h"
#include "screens.h"
#include <esp32_smartdisplay.h>
#include "NTP.h"
#include "Global.h"
#include "classes/IoTItem.h"
bool lock = false;
// -------------------ations---------------------------
float map_value(float x, float x_min, float x_max, float y_min, float y_max)
{
    float y = y_min + ((x - x_min) / (x_max - x_min)) * (y_max - y_min);
    // Округление до ближайшего значения, кратного 0.1
    y = round(y * 10) / 10;
    return y;
}
void action_callback(lv_event_t *e)
{
    // TODO: Implement action main_panel here
    log_i("action_callback");
    lv_event_code_t event_code = lv_event_get_code(e);     // Получаем код события
    lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e); // Получаем объект, который вызвал событие
    // Получаем пользовательские данные (ID или имя объекта)
    const char *name = (const char *)lv_obj_get_user_data(target);

    log_i("press 0x%p\n", (void *)target);
    log_i("press ", name);
}
void update_local_widget()
{
    //    установка целево комнатной для локального термометра
    // Дергается экран
    /*
    if (values.disp_setpoint)
    {
        IoTItem *tmp;
        tmp = findIoTItem("disp_setpoint_temp");
        if (tmp)
        {
            if (String(panelSettings.room_setpoint_temp) != tmp->value.valS)
                tmp->setValue(String(panelSettings.room_setpoint_temp), true);
        }
    }
    */
}
void display_active()
{
    // SerialPrint("i", "OTPanel", "Активирован дисплей: " + String(panelSettings.displaymode));
    IoTItem *tmp;
    tmp = findIoTItem("OTPanel");
    if (tmp)
    {
        tmp->setValue(String(panelSettings.displaymode), true);
        // SerialPrint("i", "OTPanel", "Активирован дисплей: " + String(panelSettings.displaymode));
    }

    smartdisplay_lcd_set_backlight(float(values.brightness_active) / 100);
}
IoTItem *tmp;
void action_btn_auto_press(lv_event_t *e)
{
    if (!lock)
    {
        display_active();
        if (panelSettings.room_thermostat)
        {
            // выключаем термостат
            panelSettings.room_thermostat = false;
            lv_obj_set_style_border_color(objects.btn_auto, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(objects.text_auto, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        else
        {
            // включаем термостат
            panelSettings.room_thermostat = true;
            lv_obj_set_style_border_color(objects.btn_auto, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(objects.text_auto, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        tmp = findIoTItem("auto");
        if (tmp)
        {
            tmp->setValue(String(panelSettings.room_thermostat), true);
        }

        // отключаем отопление если отключен комнатныйтермостат
        if (!panelSettings.room_thermostat)
        {
            panelSettings.ch_enabled = false;
            tmp = findIoTItem("BoilerEnable");
            if (tmp)
            {
                tmp->setValue("0", true);
            }
        }
    }
}
// режим установки комнатной температуры
void room_setpoint()
{
    panelSettings.displaymode = 3;
    SerialPrint("i", "OTPanel", "Режим установки комнатной температуры");
    lv_label_set_text(objects.text_top_center, "Установка комнатной температуры");
    // показываем ARC
    lv_obj_clear_flag(objects.arc, LV_OBJ_FLAG_HIDDEN);
    lv_arc_set_value(objects.arc, panelSettings.room_setpoint);
    // в центральный виджет установка room
    char buffer[20];
    sprintf(buffer, "%.1f", panelSettings.room_setpoint_temp);
    lv_label_set_text(objects.text_center, buffer);
    // показываем цельсии центрального виджета
    lv_obj_clear_flag(objects.text_center_celsius, LV_OBJ_FLAG_HIDDEN);
    // показываем кнопку авто
    lv_obj_clear_flag(objects.btn_auto, LV_OBJ_FLAG_HIDDEN);
    // прячем нижнюю температуру
    //    lv_obj_add_flag(objects.text_bottom, LV_OBJ_FLAG_HIDDEN);
    //    lv_obj_add_flag(objects.text_bottom_celsius, LV_OBJ_FLAG_HIDDEN);
    // прячем выключатель DHW
    lv_obj_add_flag(objects.switch_ch_dhw, LV_OBJ_FLAG_HIDDEN);
    // показываем часы
    lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
    lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
    // прячем погоду
    lv_obj_add_flag(objects.weather, LV_OBJ_FLAG_HIDDEN);
}
void action_main_plus_clic(lv_event_t *e)
{
    if (!lock)
    {
        display_active();
        // нажали кнопку плюс
        if (panelSettings.displaymode == 1)
        {
            panelSettings.ch_setpoint_temp = panelSettings.ch_setpoint_temp + 1;
            char buffer[20];
            sprintf(buffer, "%d", panelSettings.ch_setpoint_temp);
            lv_label_set_text(objects.text_center, buffer);
            lv_arc_set_value(objects.arc, panelSettings.ch_setpoint_temp);

            tmp = findIoTItem("OTset1");
            if (tmp)
            {
                tmp->setValue(String(int(panelSettings.ch_setpoint_temp)), true);
            }
        }
        else if (panelSettings.displaymode == 2)
        {
            panelSettings.dhw_setpoint_temp = panelSettings.dhw_setpoint_temp + 1;
            char buffer[20];
            sprintf(buffer, "%d", panelSettings.dhw_setpoint_temp);
            lv_label_set_text(objects.text_center, buffer);
            lv_arc_set_value(objects.arc, panelSettings.dhw_setpoint_temp);
            tmp = findIoTItem("OTset56");
            if (tmp)
            {
                tmp->setValue(String(int(panelSettings.dhw_setpoint_temp)), true);
            }
        }
        else if (panelSettings.displaymode == 3)
        {
            panelSettings.room_setpoint_temp = panelSettings.room_setpoint_temp + 0.5;
            char buffer[20];
            sprintf(buffer, "%.1f", panelSettings.room_setpoint_temp);
            lv_label_set_text(objects.text_center, buffer);
            // Преобразуем значение для термостата
            float x_min = 17, x_max = 27; // Входной диапазон
            float y_min = 0, y_max = 100; // Выходной диапазон
            // Вычисляем новое значение с шагом 0.1
            panelSettings.room_setpoint = map_value(panelSettings.room_setpoint_temp, x_min, x_max, y_min, y_max);
            lv_arc_set_value(objects.arc, panelSettings.room_setpoint);

            tmp = findIoTItem("setTemperature");
            if (tmp)
            {
                tmp->setValue(String(float(panelSettings.room_setpoint_temp)), true);
            }
        }
    }
}

void action_main_minus_clic(lv_event_t *e)
{
    if (!lock)
    {
        display_active();
        // нажали кнопку минус

        if (panelSettings.displaymode == 1)
        {
            panelSettings.ch_setpoint_temp = panelSettings.ch_setpoint_temp - 1;
            char buffer[20];
            sprintf(buffer, "%d", panelSettings.ch_setpoint_temp);
            lv_label_set_text(objects.text_center, buffer);
            lv_arc_set_value(objects.arc, panelSettings.ch_setpoint_temp);
            tmp = findIoTItem("OTset1");
            if (tmp)
            {
                tmp->setValue(String(int(panelSettings.ch_setpoint_temp)), true);
            }
        }
        else if (panelSettings.displaymode == 2)
        {
            panelSettings.dhw_setpoint_temp = panelSettings.dhw_setpoint_temp - 1;
            char buffer[20];
            sprintf(buffer, "%d", panelSettings.dhw_setpoint_temp);
            lv_label_set_text(objects.text_center, buffer);
            lv_arc_set_value(objects.arc, panelSettings.dhw_setpoint_temp);
            tmp = findIoTItem("OTset56");
            if (tmp)
            {
                tmp->setValue(String(int(panelSettings.dhw_setpoint_temp)), true);
            }
        }
        else if (panelSettings.displaymode == 3)
        {
            panelSettings.room_setpoint_temp = panelSettings.room_setpoint_temp - 0.5;
            char buffer[20];
            sprintf(buffer, "%.1f", panelSettings.room_setpoint_temp);
            lv_label_set_text(objects.text_center, buffer);
            // Преобразуем значение для термостата
            float x_min = 17, x_max = 27; // Входной диапазон
            float y_min = 0, y_max = 100; // Выходной диапазон
            // Вычисляем новое значение с шагом 0.5
            panelSettings.room_setpoint = map_value(panelSettings.room_setpoint_temp, x_min, x_max, y_min, y_max);
            lv_arc_set_value(objects.arc, panelSettings.room_setpoint);

            tmp = findIoTItem("setTemperature");
            if (tmp)
            {
                tmp->setValue(String(float(panelSettings.room_setpoint_temp)), true);
            }
        }
    }
}

void action_main_plus_hold(lv_event_t *e)
{
    log_i("BTN plus_hold");
}

void action_main_minus_hold(lv_event_t *e)
{
    log_i("BTN minus_hold");
}

// режим основной дисплей. показываем комнатную температуру
void action_boiler_click(lv_event_t *e)
{
    if (!lock)
    {
        panelSettings.displaymode = 0; // "Режим основной экран"
        display_active();

        SerialPrint("i", "OTPanel", "Режим основной экран");
        lv_label_set_text(objects.text_top_center, "");
        // прячем ARC
        lv_obj_add_flag(objects.arc, LV_OBJ_FLAG_HIDDEN);
        // в центральный виджет комнатная температура
        char buffer[20];
        sprintf(buffer, "%.1f", values.room_temp);
        lv_label_set_text(objects.text_center, buffer);
        // показываем цельсии центрального виджета
        lv_obj_clear_flag(objects.text_center_celsius, LV_OBJ_FLAG_HIDDEN);
        // кнопка авто
        lv_obj_clear_flag(objects.btn_auto, LV_OBJ_FLAG_HIDDEN);
        // нижня температура
        //        lv_obj_clear_flag(objects.text_bottom, LV_OBJ_FLAG_HIDDEN);
        //        lv_obj_clear_flag(objects.text_bottom_celsius, LV_OBJ_FLAG_HIDDEN);
        // прячем выключатель CH
        lv_obj_add_flag(objects.switch_ch_dhw, LV_OBJ_FLAG_HIDDEN);
        // показываем часы
        lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
        lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
    }
}
// режим установки температуры CH
void action_ch_click(lv_event_t *e)
{
    if (!lock)
    {
        panelSettings.displaymode = 1;
        display_active();

        SerialPrint("i", "OTPanel", "Режим установки температуры CH ");
        lv_label_set_text(objects.text_top_center, "Установка температуры отопления");
        // показываем ARC
        lv_obj_clear_flag(objects.arc, LV_OBJ_FLAG_HIDDEN);

        lv_arc_set_value(objects.arc, panelSettings.ch_setpoint_temp);
        // в центральный виджет установка CH
        // SerialPrint("i", "OTPanel", " panelSettings.ch_setpoint_temp: " + String(panelSettings.ch_setpoint_temp));
        char buffer[20];
        sprintf(buffer, "%d", panelSettings.ch_setpoint_temp);
        // SerialPrint("i", "OTPanel", " buffer: " + String(buffer));
        lv_label_set_text(objects.text_center, buffer);
        // показываем цельсии центрального виджета
        lv_obj_clear_flag(objects.text_center, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.text_center_celsius, LV_OBJ_FLAG_HIDDEN);
        // прячем кнопку авто
        lv_obj_add_flag(objects.btn_auto, LV_OBJ_FLAG_HIDDEN);
        // прячем нижнюю температуру
        //        lv_obj_add_flag(objects.text_bottom, LV_OBJ_FLAG_HIDDEN);
        //        lv_obj_add_flag(objects.text_bottom_celsius, LV_OBJ_FLAG_HIDDEN);
        // показываем выключатель CH
        if (panelSettings.ch_enabled)
        {
            lv_obj_add_state(objects.switch_ch_dhw, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_remove_state(objects.switch_ch_dhw, LV_STATE_CHECKED);
        }
        lv_obj_clear_flag(objects.switch_ch_dhw, LV_OBJ_FLAG_HIDDEN);
        // показываем часы
        lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
        lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
        // прячем погоду
        lv_obj_add_flag(objects.weather, LV_OBJ_FLAG_HIDDEN);
    }
}

// режим установки температуры DHW
void action_dhw_click(lv_event_t *e)
{
    if (!lock)
    {
        panelSettings.displaymode = 2;
        display_active();

        SerialPrint("i", "OTPanel", "Режим установки температуры DHW ");
        lv_label_set_text(objects.text_top_center, "Установка температуры ГВС");
        // показываем ARC
        lv_obj_clear_flag(objects.arc, LV_OBJ_FLAG_HIDDEN);

        lv_arc_set_value(objects.arc, panelSettings.dhw_setpoint_temp);
        // в центральный виджет установка DHW
        char buffer[20];
        sprintf(buffer, "%d", panelSettings.dhw_setpoint_temp);
        lv_label_set_text(objects.text_center, buffer);
        // показываем цельсии центрального виджета
        lv_obj_clear_flag(objects.text_center, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.text_center_celsius, LV_OBJ_FLAG_HIDDEN);
        // прячем кнопку авто
        lv_obj_add_flag(objects.btn_auto, LV_OBJ_FLAG_HIDDEN);
        // прячем нижнюю температуру
        //        lv_obj_add_flag(objects.text_bottom, LV_OBJ_FLAG_HIDDEN);
        //        lv_obj_add_flag(objects.text_bottom_celsius, LV_OBJ_FLAG_HIDDEN);
        // показываем выключатель DHW
        if (panelSettings.dhw_enabled)
        {
            lv_obj_add_state(objects.switch_ch_dhw, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_remove_state(objects.switch_ch_dhw, LV_STATE_CHECKED);
        }
        lv_obj_clear_flag(objects.switch_ch_dhw, LV_OBJ_FLAG_HIDDEN);
        // показываем часы
        lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
        lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
        // прячем погоду
        lv_obj_add_flag(objects.weather, LV_OBJ_FLAG_HIDDEN);
    }
}
void action_text_top_right_pressed(lv_event_t *e)
{
    if (!lock)
    {
        panelSettings.displaymode = 0; // "Режим основной экран"
        display_active();

        SerialPrint("i", "OTDisplay", "Режим основной экран");
        lv_label_set_text(objects.text_top_center, "");
        // прячем ARC
        lv_obj_add_flag(objects.arc, LV_OBJ_FLAG_HIDDEN);
        // в центральный виджет комнатная температура
        char buffer[20];
        sprintf(buffer, "%.1f", values.room_temp);
        lv_label_set_text(objects.text_center, buffer);
        // показываем цельсии центрального виджета
        lv_obj_clear_flag(objects.text_center_celsius, LV_OBJ_FLAG_HIDDEN);
        // кнопка авто
        lv_obj_clear_flag(objects.btn_auto, LV_OBJ_FLAG_HIDDEN);
        // нижня температура
        //        lv_obj_clear_flag(objects.text_bottom, LV_OBJ_FLAG_HIDDEN);
        //        lv_obj_clear_flag(objects.text_bottom_celsius, LV_OBJ_FLAG_HIDDEN);
        // прячем выключатель CH
        lv_obj_add_flag(objects.switch_ch_dhw, LV_OBJ_FLAG_HIDDEN);
        // показываем часы
        lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
        lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
    }

    /*
    // режим  часов
    if (panelSettings.displaymode != 4)
    {
        panelSettings.displaymode = 4;
        SerialPrint("i", "OTPanel", "Режим часов ");
        // в центральный виджет установка часы
        lv_label_set_text(objects.text_center, getTimeLocal_hhmm().c_str());
        // прячем цельсии центрального виджета
        lv_obj_add_flag(objects.text_center_celsius, LV_OBJ_FLAG_HIDDEN);
        // в центральный виджет установка room
        char buffer[20];
        sprintf(buffer, "%.1f", values.room_temp);
        lv_label_set_text(objects.text_top_right, buffer);
    }
    else
    {
        panelSettings.displaymode = 0;
        SerialPrint("i", "OTPanel", "Режим основной экран ");
        lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
        // в центральный виджет комнатная температура
        char buffer[20];
        sprintf(buffer, "%.1f", values.room_temp);
        lv_label_set_text(objects.text_center, buffer);
        // показываем цельсии центрального виджета
        lv_obj_clear_flag(objects.text_center_celsius, LV_OBJ_FLAG_HIDDEN);
    }
    display_active();
    /*
    lv_label_set_text(objects.text_top_center, "");
    // прячем ARC
    lv_obj_add_flag(objects.arc, LV_OBJ_FLAG_HIDDEN);
    // в центральный виджет установка часы
    lv_label_set_text(objects.text_center, getTimeLocal_hhmm().c_str());
    // прячем цельсии центрального виджета
    lv_obj_add_flag(objects.text_center_celsius, LV_OBJ_FLAG_HIDDEN);
    // показываем кнопку авто
    lv_obj_clear_flag(objects.btn_auto, LV_OBJ_FLAG_HIDDEN);
    // нижня температура
    if (panelSettings.room_thermostat == 1)
    {
        lv_obj_clear_flag(objects.text_bottom, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.text_bottom_celsius, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(objects.text_bottom, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.text_bottom_celsius, LV_OBJ_FLAG_HIDDEN);
    }
    // прячем выключатель DHW
    lv_obj_add_flag(objects.switch_ch_dhw, LV_OBJ_FLAG_HIDDEN);
    // прячем часы
    lv_obj_add_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
    */
}

void action_btn_ch_dhw_on(lv_event_t *e)
{
    display_active();
    if (panelSettings.displaymode == 1)
    {
        panelSettings.ch_enabled = true;
        tmp = findIoTItem("BoilerEnable");
        if (tmp)
        {
            tmp->setValue("1", true);
        }
    }
    else if (panelSettings.displaymode == 2)
    {
        panelSettings.dhw_enabled = true;

        tmp = findIoTItem("DHWenable");
        if (tmp)
        {
            tmp->setValue("1", true);
        }
    }
}

void action_btn_ch_dhw_off(lv_event_t *e)
{
    display_active();
    if (panelSettings.displaymode == 1)
    {
        panelSettings.ch_enabled = false;

        tmp = findIoTItem("BoilerEnable");
        if (tmp)
        {
            tmp->setValue("0", true);
        }
    }
    else if (panelSettings.displaymode == 2)
    {
        panelSettings.dhw_enabled = false;

        tmp = findIoTItem("DHWenable");
        if (tmp)
        {
            tmp->setValue("0", true);
        }
    }
}

void action_arc_value_released(lv_event_t *e)
{
    display_active();
    if (panelSettings.displaymode == 1)
    {
        tmp = findIoTItem("OTset1");
        if (tmp)
        {
            tmp->setValue(String(int(panelSettings.ch_setpoint_temp)), true);
        }
    }
    else if (panelSettings.displaymode == 2)
    {

        tmp = findIoTItem("OTset56");
        if (tmp)
        {
            tmp->setValue(String(int(panelSettings.dhw_setpoint_temp)), true);
        }
    }
    else
    {

        tmp = findIoTItem("setTemperature");
        if (tmp)
        {
            tmp->setValue(String(float(panelSettings.room_setpoint_temp)), true);
        }
    }
}

void action_arc_value_change(lv_event_t *e)
{
    // режим: установка CH
    if (panelSettings.displaymode == 1)
    {
        // lv_obj_t *arc = lv_event_get_target(e);
        //  Получаем текущее значение дуги
        int16_t value = lv_arc_get_value(objects.arc);
        // Устанавливаем текст метки
        char buffer[20];
        sprintf(buffer, "%d", value);
        lv_label_set_text(objects.text_center, buffer);
        panelSettings.ch_setpoint_temp = value;
    }

    else if (panelSettings.displaymode == 2)
    {
        // режим: установка dhw
        // lv_obj_t *arc = lv_event_get_target(e);
        //  Получаем текущее значение дуги
        int16_t value = lv_arc_get_value(objects.arc);
        // Устанавливаем текст метки
        char buffer[20];
        sprintf(buffer, "%d", value);
        lv_label_set_text(objects.text_center, buffer);
        panelSettings.dhw_setpoint_temp = value;
        // публикуем в mqtt
    }
    else
    {
        // если режим: комнатный термометр
        float x_min = 0, x_max = 100; // Входной диапазон
        float y_min = 17, y_max = 27; // Выходной диапазон
        //  Получаем текущее значение дуги
        int16_t value = lv_arc_get_value(objects.arc);
        // Вычисляем новое значение с шагом 0.1

        float new_value = map_value(value, x_min, x_max, y_min, y_max);
        // Буфер для хранения строки
        char buffer[16]; // Достаточно для хранения числа и символа завершения строки
        // Преобразуем число в строку
        snprintf(buffer, sizeof(buffer), "%.1f", new_value); // "%.1f" — формат для одного знака после запятой
        // Устанавливаем текст метки
        lv_label_set_text(objects.text_center, buffer);
        panelSettings.room_setpoint_temp = new_value;
        panelSettings.room_setpoint = value;
    }
}
void action_text_centr_click(lv_event_t *e)
{
    if (!lock)
    {
        // TODO: Implement action text_centr_click here
        room_setpoint();
        display_active();
        //        log_i("BTN action_text_centr_click");
    }
}

void action_text_centr_hold(lv_event_t *e)
{
    display_active();
    if (lock)
    {
        lock = false;
        lv_obj_add_flag(objects.lock, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lock = true;
        lv_obj_clear_flag(objects.lock, LV_OBJ_FLAG_HIDDEN);
    }
    // TODO: Implement action text_centr_hold here
    // log_i("BTN action_text_centr_hold");
}

void action_relay1_press(lv_event_t *e)
{
    if (!lock)
    {
        //  log_i("BTN action_relay1_press");
        display_active();

        if (values.relay1)
        {
            // выключаем термостат
            values.relay1 = false;
            lv_obj_set_style_border_color(objects.relay1, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(objects.btn1_text, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        else
        {
            // включаем термостат
            values.relay1 = true;
            lv_obj_set_style_border_color(objects.relay1, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(objects.btn1_text, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        IoTItem *tmp;
        tmp = findIoTItem("relay1");
        if (tmp)
        {
            tmp->setValue(String(values.relay1), false);
        }
    }
}

void action_relay2_press(lv_event_t *e)
{
    if (!lock)
    {
        // log_i("BTN action_relay2_press");
        display_active();

        if (values.relay2)
        {
            // выключаем термостат
            values.relay2 = false;
            lv_obj_set_style_border_color(objects.relay2, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(objects.btn2_text, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        else
        {
            // включаем термостат
            values.relay2 = true;
            lv_obj_set_style_border_color(objects.relay2, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(objects.btn2_text, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        IoTItem *tmp;
        tmp = findIoTItem("relay2");
        if (tmp)
        {

            // tmp->regEvent((String)values.relay2, "", false, false);
            tmp->setValue(String(values.relay2), false);
        }
    }
}

void action_relay3_press(lv_event_t *e)
{
    if (!lock)
    {
        //    log_i("BTN action_relay3_press");
        display_active();

        if (values.relay3)
        {
            // выключаем термостат
            values.relay3 = false;
            lv_obj_set_style_border_color(objects.relay3, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(objects.btn3_text, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        else
        {
            // включаем термостат
            values.relay3 = true;
            lv_obj_set_style_border_color(objects.relay3, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(objects.btn3_text, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        IoTItem *tmp;
        tmp = findIoTItem("relay3");
        if (tmp)
        {
            tmp->setValue(String(values.relay3), false);
        }
    }
}
// -------------------ations---------------------------
