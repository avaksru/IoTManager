#include "Global.h"
#include "classes/IoTItem.h"
#include <esp32_smartdisplay.h>
#include "lvgl.h"
#include <Preferences.h>
// #include "conf/_config.h"
#include "ui/ui.h"
#include "ui/images.h"
#include "ui/actions.h"
#include "OTPanel.h"
#include "NTP.h"

struct Values values;
struct Settings panelSettings;
bool is_initialized = false;
unsigned long now;
class OTPanel : public IoTItem
{
private:
    //=======================================================================================================
    // Секция переменных.
    ulong next_millis = 0;
    ulong next_millis_display = 0;
    ulong lv_last_tick = millis();
    ulong oflineTimer = millis();
    //    String _controllerID;
    //    String _prefix;
    bool FaultStatus = 0;
    // bool isBoilerMQTTconnected = 0;
    int _rotation = 0;
    int dispMode = 0;
    int log = 1;

public:
    //=======================================================================================================
    // setup()

    OTPanel(String parameters) : IoTItem(parameters)
    {

        jsonRead(parameters, F("rotation"), _rotation);
        jsonRead(parameters, F("brightness_active"), values.brightness_active);
        jsonRead(parameters, F("brightness_idle"), values.brightness_idle);
        jsonRead(parameters, F("log"), log);

        panelSettings.displaymode = 0;

        if (is_initialized)
        {
            log_w("Smart Display already initialized");
        }
        else
        {
            smartdisplay_init();
            __attribute__((unused)) auto disp = lv_disp_get_default();

            if (_rotation == 90)
                lv_disp_set_rotation(disp, LV_DISP_ROTATION_90);
            if (_rotation == 180)
                lv_disp_set_rotation(disp, LV_DISP_ROTATION_180);
            if (_rotation == 270)
                lv_disp_set_rotation(disp, LV_DISP_ROTATION_270);
            smartdisplay_lcd_set_backlight(float(values.brightness_idle) / 100);
            ui_init();

            is_initialized = true;
        }
    }

    float map_value(float x, float x_min, float x_max, float y_min, float y_max)
    {
        float y = y_min + ((x - x_min) / (x_max - x_min)) * (y_max - y_min);
        // Округление до ближайшего значения, кратного 0.1
        y = round(y * 10) / 10;
        return y;
    }

    void WriteConfig()
    {
        Preferences preferences;
        preferences.begin("_config", false);
        preferences.putBytes("_config", &panelSettings, sizeof(panelSettings));
        preferences.end();
    }

    void VerifyVersion()
    {
        Preferences preferences;
        if (!preferences.begin("_config", false))
            return;
        preferences.end();
    }

    void LoadConfig()
    {
        VerifyVersion();
        Preferences preferences;
        preferences.begin("_config", true);
        size_t read = preferences.getBytes("_config", &panelSettings, sizeof(panelSettings));
        preferences.end();
        if (read != sizeof(panelSettings))
        {
            // Если blob не найден, инициализируем настройки по умолчанию и записываем их
            memset(&panelSettings, 0, sizeof(panelSettings));
            WriteConfig();
            log_w("Config not found, initialized defaults");
        }
    }

    void refreshDisplay()
    {

        //-------------------------------- отрисовываем информацию на экране----------------------------------------//

        String text_top_center = lv_label_get_text(objects.text_top_center);

        if ((unsigned long)(millis() - oflineTimer) > 61000UL)
        {
            // SerialPrint("i", "OTPanel", " oflineTimer: " + String(oflineTimer) + " millis: " + String(millis() - 61000));
            lv_label_set_text(objects.text_top_center, ("Ожидаем подключение котла"));
            lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
        }
        else if (panelSettings.displaymode == 0 && (text_top_center == "Установка температуры ГВС" || text_top_center == "Установка комнатной температуры" || text_top_center == "Установка температуры отопления" || text_top_center == "Котел подключен" || text_top_center == "Ожидаем подключение котла"))
        {
            lv_label_set_text(objects.text_top_center, (""));
            lv_obj_add_flag(objects.boiler_status, LV_OBJ_FLAG_HIDDEN);
        }

        else if (values.boiler && !panelSettings.FaultStatus && panelSettings.displaymode == 0 && text_top_center == "Потеря связи OpenTherm")
        {
            // lv_img_set_src(objects.boiler_status_image, &img_ok);
            // lv_obj_set_style_image_opa(objects.boiler_status_image, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
            // lv_obj_clear_flag(objects.boiler_status, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.boiler_status, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(objects.text_top_center, "");
        }
        if (!values.boiler && !panelSettings.FaultStatus)
        {
            lv_img_set_src(objects.boiler_status_image, &img_err);
            lv_obj_set_style_image_opa(objects.boiler_status_image, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(objects.boiler_status, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(objects.text_top_center, "Потеря связи OpenTherm");
        }
        // ошибка котла
        if (panelSettings.FaultStatus)
        {
            lv_img_set_src(objects.boiler_status_image, &img_sos);
            lv_obj_set_style_image_opa(objects.boiler_status_image, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(objects.boiler_status, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(objects.text_top_center, values.FaultText.c_str());
            lv_obj_set_style_text_color(objects.text_top_center, lv_color_hex(0xfff90000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        // Иконка CH
        // SerialPrint("i", "OTPanel", " panelSettings.ch_active: " + String(panelSettings.ch_active) + " panelSettings.ch_enabled: " + String(panelSettings.ch_enabled));
        if (panelSettings.ch_active && panelSettings.ch_enabled)
            lv_img_set_src(objects.ch_image, &img_ch_active);
        if (!panelSettings.ch_active && panelSettings.ch_enabled)
            lv_img_set_src(objects.ch_image, &img_ch_no_active);
        if (!panelSettings.ch_enabled)
            lv_img_set_src(objects.ch_image, &img_ch_off);

        // температура CH
        if (values.ch_temp)
        {
            char buffer[20];
            sprintf(buffer, "%d", values.ch_temp);
            lv_label_set_text(objects._h_curent_temp, buffer);
            lv_obj_clear_flag(objects._h_curent_temp, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.ch_c, LV_OBJ_FLAG_HIDDEN);
        }

        // Иконка ГВС
        if (panelSettings.dhw_active && panelSettings.dhw_enabled)
            lv_img_set_src(objects.dhw_image, &img_dhw_active);
        if (!panelSettings.dhw_active && panelSettings.dhw_enabled)
            lv_img_set_src(objects.dhw_image, &img_dhw_no_active);
        if (!panelSettings.dhw_enabled)
            lv_img_set_src(objects.dhw_image, &img_dhw_off);

        // температура ГВС
        if (values.dhw_temp)
        {
            char buffer[20];
            sprintf(buffer, "%d", values.dhw_temp);
            lv_label_set_text(objects.dhw_curent_temp, buffer);
            lv_obj_clear_flag(objects.dhw_curent_temp, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.dhw_c, LV_OBJ_FLAG_HIDDEN);
        }

        // модуляция
        if (values.flame_curent < 101)
        {
            char buffer[20];
            sprintf(buffer, "%d", values.flame_curent);
            lv_label_set_text(objects.flame_curent, buffer);
            lv_obj_clear_flag(objects.flame_curent, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.flame_status, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.flame_persent, LV_OBJ_FLAG_HIDDEN);
        }
        // Иконка горелки
        if (panelSettings.flame)
        {
            lv_img_set_src(objects.flame_image, &img_flame);
            lv_obj_clear_flag(objects.flame_image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.flame_status, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(objects.flame_image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.flame_status, LV_OBJ_FLAG_HIDDEN);
        }

        if (panelSettings.displaymode == 1)
        {
            // Режим экрана установка CH
            lv_label_set_text(objects.text_top_center, "Установка температуры отопления");
            // в центральный виджет установка CH
            char buffer[20];
            sprintf(buffer, "%d", panelSettings.ch_setpoint_temp);
            lv_label_set_text(objects.text_center, buffer);
            lv_obj_clear_flag(objects.text_center, LV_OBJ_FLAG_HIDDEN);
            // показываем цельсии центрального виджета
            lv_obj_clear_flag(objects.text_center_celsius, LV_OBJ_FLAG_HIDDEN);
            // показываем ARC
            lv_obj_clear_flag(objects.arc, LV_OBJ_FLAG_HIDDEN);
            lv_arc_set_value(objects.arc, panelSettings.ch_setpoint_temp);
            // прячем дополнительную температуру
            lv_obj_add_flag(objects.text_bottom, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.text_bottom_celsius, LV_OBJ_FLAG_HIDDEN);
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
            // прячем погоду
            lv_obj_add_flag(objects.weather, LV_OBJ_FLAG_HIDDEN);
        }
        else if (panelSettings.displaymode == 2)
        {
            // Режим экрана установка DHW
            lv_label_set_text(objects.text_top_center, "Установка температуры ГВС");
            // показываем ARC
            lv_obj_clear_flag(objects.arc, LV_OBJ_FLAG_HIDDEN);
            lv_arc_set_value(objects.arc, panelSettings.dhw_setpoint_temp);
            // в центральный виджет установка DHW
            char buffer[20];
            sprintf(buffer, "%d", panelSettings.dhw_setpoint_temp);
            lv_label_set_text(objects.text_center, buffer);
            // показываем цельсии центрального виджета
            lv_obj_clear_flag(objects.text_center_celsius, LV_OBJ_FLAG_HIDDEN);
            // прячем кнопку авто
            lv_obj_add_flag(objects.btn_auto, LV_OBJ_FLAG_HIDDEN);
            // прячем дополнительную температуру
            lv_obj_add_flag(objects.text_bottom, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.text_bottom_celsius, LV_OBJ_FLAG_HIDDEN);
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
        else if (panelSettings.displaymode == 3)
        {
            // Режим экрана установка комнатной температуры
            lv_label_set_text(objects.text_top_center, "Установка комнатной температуры");
            // показываем ARC
            lv_obj_clear_flag(objects.arc, LV_OBJ_FLAG_HIDDEN);
            lv_arc_set_value(objects.arc, panelSettings.room_setpoint);

            // в центральный виджет установка room
            char buffer[20];
            sprintf(buffer, "%.1f", panelSettings.room_setpoint_temp);
            lv_label_set_text(objects.text_center, buffer);
            lv_obj_clear_flag(objects.text_center, LV_OBJ_FLAG_HIDDEN);
            // показываем цельсии центрального виджета
            lv_obj_clear_flag(objects.text_center_celsius, LV_OBJ_FLAG_HIDDEN);
            // показываем кнопку авто
            lv_obj_clear_flag(objects.btn_auto, LV_OBJ_FLAG_HIDDEN);
            // прячем дополнительную температуру
            lv_obj_add_flag(objects.text_bottom, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.text_bottom_celsius, LV_OBJ_FLAG_HIDDEN);
            // прячем выключатель DHW
            lv_obj_add_flag(objects.switch_ch_dhw, LV_OBJ_FLAG_HIDDEN);
            // показываем часы
            lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
            lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
            // прячем погоду
            lv_obj_add_flag(objects.weather, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            // режим "Режим основной экран"
            String text_top_center = lv_label_get_text(objects.text_top_center);
            if (text_top_center == "Установка температуры ГВС" || text_top_center == "Установка комнатной температуры" || text_top_center == "Установка температуры отопления" || text_top_center == "Котел подключен")
            {
                lv_label_set_text(objects.text_top_center, "");
                lv_obj_add_flag(objects.boiler_status, LV_OBJ_FLAG_HIDDEN);
            }
            //  комнатная температура
            // SerialPrint("i", "OTPanel", String(values.room_temp));
            if (values.room_temp > 0)
            {
                char buffer[20];
                sprintf(buffer, "%.1f", values.room_temp);
                lv_label_set_text(objects.text_center, buffer);
                lv_obj_clear_flag(objects.text_center, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(objects.text_center_celsius, LV_OBJ_FLAG_HIDDEN);
            }
            else
            {
                lv_obj_add_flag(objects.text_center, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(objects.text_center_celsius, LV_OBJ_FLAG_HIDDEN);
            }

            // прячем ARC
            lv_obj_add_flag(objects.arc, LV_OBJ_FLAG_HIDDEN);
            // выключатель отопления
            lv_obj_add_flag(objects.switch_ch_dhw, LV_OBJ_FLAG_HIDDEN);

            // кнопка авто
            lv_obj_clear_flag(objects.btn_auto, LV_OBJ_FLAG_HIDDEN);
            if (panelSettings.room_thermostat)
            {
                lv_obj_set_style_border_color(objects.btn_auto, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.text_auto, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
                char buffer[20];
                sprintf(buffer, "%.1f", panelSettings.room_setpoint_temp);
                lv_label_set_text(objects.text_auto, buffer);
            }
            else
            {
                lv_obj_set_style_border_color(objects.btn_auto, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.text_auto, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_label_set_text(objects.text_auto, "AUTO");
            }
            // показываем часы
            lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
            lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
        }

        //-----------------------------------------------------------------------------------------------------------------
    }

    //=======================================================================================================
    // doByInterval()
    void
    doByInterval()
    {

        // Дополнительный виджет температуры
        IoTItem *customTemp = findIoTItem("customTemp");
        if (customTemp && panelSettings.displaymode == 0)
        {
            lv_label_set_text(objects.text_bottom, customTemp->getValue().c_str());
            lv_obj_clear_flag(objects.text_bottom, LV_OBJ_FLAG_HIDDEN);
            //  lv_obj_clear_flag(objects.text_bottom_celsius, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(objects.text_bottom, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.text_bottom_celsius, LV_OBJ_FLAG_HIDDEN);
        }

        // Обновляем погоду
        IoTItem *temp_C = findIoTItem("temp_C");
        if (!temp_C) temp_C = findIoTItem("tempC");
        if (temp_C && panelSettings.displaymode == 0)
        {
            String temperture;
            if (temp_C->getValue() != "...")
            {
                if (temp_C->getValue().toInt() > 0)
                {
                    temperture = "+" + String(temp_C->getValue());
                }
                else
                {
                    temperture = String(temp_C->getValue()); // для 0 и отрицательных значений
                }
                lv_label_set_text(objects.wez_temp, temperture.c_str());
                lv_obj_clear_flag(objects.wez_temp, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(objects.weather, LV_OBJ_FLAG_HIDDEN);
                if (temp_C->getValue().toInt() >= 18)
                    lv_obj_set_style_text_color(objects.wez_temp, lv_color_hex(0xffb6efc9), LV_PART_MAIN | LV_STATE_DEFAULT);
                if (temp_C->getValue().toInt() < 18)
                    lv_obj_set_style_text_color(objects.wez_temp, lv_color_hex(0xffc2e5f1), LV_PART_MAIN | LV_STATE_DEFAULT);
                if (temp_C->getValue().toInt() < 0)
                    lv_obj_set_style_text_color(objects.wez_temp, lv_color_hex(0xff7cdcff), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            else
            {
                lv_obj_add_flag(objects.wez_temp, LV_OBJ_FLAG_HIDDEN);
            }
        }
        else
        {
            lv_obj_add_flag(objects.wez_temp, LV_OBJ_FLAG_HIDDEN);
        }

        IoTItem *rangetempC = findIoTItem("rangetempC");
        if (rangetempC && panelSettings.displaymode == 0)
        {
            if (rangetempC->getValue() != "...")
            {
                lv_label_set_text(objects.wez_range, rangetempC->getValue().c_str());
                lv_obj_clear_flag(objects.wez_range, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(objects.weather, LV_OBJ_FLAG_HIDDEN);
            }
            else
            {
                lv_obj_add_flag(objects.wez_range, LV_OBJ_FLAG_HIDDEN);
            }
        }
        else
        {
            lv_obj_add_flag(objects.wez_range, LV_OBJ_FLAG_HIDDEN);
        }

        IoTItem *weatherCode = findIoTItem("weatherCode");
        if (weatherCode && panelSettings.displaymode == 0)
        {
            if (weatherCode->getValue() != "...")
            {
                // вычисляем время
                char buf[32];
                sprintf(buf, "%02d", _time_local.hour);
                int hour = atoi(buf);

                if (weatherCode->getValue().toInt() == 113)
                {
                    // ясное
                    if (hour > 18)
                    {
                        lv_image_set_src(objects.wez_cod, &img_q1);
                    }
                    else
                    {
                        lv_image_set_src(objects.wez_cod, &img_w1);
                    }
                }
                else

                    if (weatherCode->getValue().toInt() == 116)
                {
                    // солнышко с тучкой снизу
                    if (hour > 18)
                    {
                        lv_image_set_src(objects.wez_cod, &img_q2);
                    }
                    else
                    {
                        lv_image_set_src(objects.wez_cod, &img_w2);
                    }
                }
                else

                    if (weatherCode->getValue().toInt() == 119 || weatherCode->getValue().toInt() == 122 || weatherCode->getValue().toInt() == 143 || weatherCode->getValue().toInt() == 248 || weatherCode->getValue().toInt() == 260 || weatherCode->getValue().toInt() == 299 || weatherCode->getValue().toInt() == 302 || weatherCode->getValue().toInt() == 305 || weatherCode->getValue().toInt() == 308 || weatherCode->getValue().toInt() == 356 || weatherCode->getValue().toInt() == 359)
                {
                    // облачно
                    lv_image_set_src(objects.wez_cod, &img_w8);
                }
                else

                    if (weatherCode->getValue().toInt() == 179 || weatherCode->getValue().toInt() == 182 || weatherCode->getValue().toInt() == 185 || weatherCode->getValue().toInt() == 311 || weatherCode->getValue().toInt() == 314 || weatherCode->getValue().toInt() == 317 || weatherCode->getValue().toInt() == 320 || weatherCode->getValue().toInt() == 362 || weatherCode->getValue().toInt() == 365 || weatherCode->getValue().toInt() == 368 || weatherCode->getValue().toInt() == 377)
                {
                    // снег с дождем
                    lv_image_set_src(objects.wez_cod, &img_w6);
                }
                else

                    if (weatherCode->getValue().toInt() == 323 || weatherCode->getValue().toInt() == 326)
                {
                    // слабый снег
                    lv_image_set_src(objects.wez_cod, &img_w5);
                }
                else

                    if (weatherCode->getValue().toInt() == 176 || weatherCode->getValue().toInt() == 263 || weatherCode->getValue().toInt() == 266 || weatherCode->getValue().toInt() == 293 || weatherCode->getValue().toInt() == 296 || weatherCode->getValue().toInt() == 200 || weatherCode->getValue().toInt() == 227 || weatherCode->getValue().toInt() == 374 || weatherCode->getValue().toInt() == 386 || weatherCode->getValue().toInt() == 389 || weatherCode->getValue().toInt() == 392)
                {
                    // солнце дождь
                    if (hour > 18)
                    {
                        lv_image_set_src(objects.wez_cod, &img_q7);
                    }
                    else
                    {
                        lv_image_set_src(objects.wez_cod, &img_w7);
                    }
                }
                else
                {
                    // пустая картинка
                    lv_image_set_src(objects.wez_cod, &img_w0);
                }

                lv_obj_clear_flag(objects.wez_cod, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(objects.weather, LV_OBJ_FLAG_HIDDEN);
            }
            else
            {
                lv_obj_add_flag(objects.wez_cod, LV_OBJ_FLAG_HIDDEN);
            }
        }
        else
        {
            lv_obj_add_flag(objects.wez_cod, LV_OBJ_FLAG_HIDDEN);
        }

        // обновляем часы
        if (isNetworkActive())
        {
            lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
            lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
        }

        if (jsonReadStr(settingsFlashJson, "control") == "fail")
        {
            lv_label_set_text(objects.text_top_center, "Отсутствует лицензия");
            lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(objects.text_top_center, lv_color_hex(0xfff90000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    void getData()
    {
        // Если данные от котла не получены, то ждем
        IoTItem *tmp;
        String interim;
        tmp = findIoTItem("BoilerEnable");
        if (tmp)
        {
            interim = tmp->getValue();
            if (interim == "1")
                panelSettings.ch_enabled = true;
            if (interim == "0")
                panelSettings.ch_enabled = false;
        }
        tmp = findIoTItem("isHeatingEnabled");
        if (tmp)
        {
            interim = tmp->getValue();
            if (interim == "✅")
                panelSettings.ch_active = true;
            if (interim == "➖")
                panelSettings.ch_active = false;
        }
        // OTset1
        tmp = findIoTItem("OTset1");
        if (tmp)
        {
            interim = tmp->getValue();
            panelSettings.ch_setpoint_temp = interim.toInt();
        }
        // temperature
        tmp = findIoTItem("temperature");
        if (tmp)
        {
            interim = tmp->getValue();
            values.room_temp = atof(interim.c_str());
        }
        // setTemperature
        tmp = findIoTItem("setTemperature");
        if (tmp)
        {
            interim = tmp->getValue();
            panelSettings.room_setpoint_temp = atof(interim.c_str());
            // map для panelSettings.room_setpoint
            float x_min = 17, x_max = 27; // Входной диапазон
            float y_min = 0, y_max = 100; // Выходной диапазон
            // Вычисляем новое значение с шагом 0.5
            panelSettings.room_setpoint = map_value(panelSettings.room_setpoint_temp, x_min, x_max, y_min, y_max);
        }
        // auto
        tmp = findIoTItem("auto");
        if (tmp)
        {
            interim = tmp->getValue();
            if (interim == "1")
                panelSettings.room_thermostat = true;
            if (interim == "0")
                panelSettings.room_thermostat = false;
        }
        // DHWenable
        tmp = findIoTItem("DHWenable");
        if (tmp)
        {
            interim = tmp->getValue();
            if (interim == "1")
                panelSettings.dhw_enabled = true;
            if (interim == "0")
                panelSettings.dhw_enabled = false;
        }
        // isDHWenabled
        tmp = findIoTItem("isDHWenabled");
        if (tmp)
        {
            interim = tmp->getValue();
            if (interim == "✅")
                panelSettings.dhw_active = true;
            if (interim == "➖")
                panelSettings.dhw_active = false;
        }
        // OTset56
        tmp = findIoTItem("OTset56");
        if (tmp)
        {
            interim = tmp->getValue();
            panelSettings.dhw_setpoint_temp = interim.toInt();
        }
    }

    void onRegEvent(IoTItem *eventItem)
    {
        if (!eventItem)
            return;

        String printStr = "";
        printStr += eventItem->getID().c_str();

        if (printStr.indexOf(F("boiler")) != -1)
        {
            if (eventItem->getValue() == "➖")
            {
                lv_label_set_text(objects.text_top_center, ("Ожидаем подключение котла (" + jsonReadStr(settingsFlashJson, F("ip")) + ")").c_str());
                lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
            }
        }
        // подключение к котлу
        if (printStr.indexOf(F("boiler")) != -1)
        {
            oflineTimer = millis();
            //    SerialPrint("i", "OTPanel", " boilerTopic: " + values.boilerTopic);
            if (eventItem->getValue() == "✅")
            {
                values.boiler = 1;

                // Обновляем статусы
                getData();
                SerialPrint("i", "OTPanel", "!!!Запрос статусов котла!!!");
            }

            if (eventItem->getValue() == "❌")
            {
                values.boiler = 0;
            }
        }
        // ошибка котла
        if (printStr.indexOf(F("LastFaultType")) != -1)
        {
            if ((eventItem->getValue() == "нет") || eventItem->getValue() == "0")
                panelSettings.FaultStatus = 0;
            else
                panelSettings.FaultStatus = 1;
            values.FaultText = eventItem->getValue();
        }
        // CH разрешен
        if (printStr.indexOf(F("BoilerEnable")) != -1)
        {
            if (eventItem->getValue() == "1")
                panelSettings.ch_enabled = true;
            if (eventItem->getValue() == "0")
                panelSettings.ch_enabled = false;
        }
        // CH активен
        if (printStr.indexOf(F("isHeatingEnabled")) != -1)
        {
            if (eventItem->getValue() == "✅")
                panelSettings.ch_active = true;
            if (eventItem->getValue() == "➖")
                panelSettings.ch_active = false;
        }
        // температура CH
        if (printStr.indexOf(F("OTget25")) != -1)
        {
            values.ch_temp = eventItem->getValue().toInt();
            oflineTimer = millis();
        }
        // целевая температура CH
        if (printStr.indexOf(F("OTset1")) != -1)
            panelSettings.ch_setpoint_temp = eventItem->getValue().toInt();
        // DHW разрешен
        if (printStr.indexOf(F("DHWenable")) != -1)
        {
            if (eventItem->getValue() == "1")
                panelSettings.dhw_enabled = true;
            if (eventItem->getValue() == "0")
                panelSettings.dhw_enabled = false;
        }
        // DHW активен
        if (printStr.indexOf(F("isDHWenabled")) != -1)
        {
            if (eventItem->getValue() == "✅")
                panelSettings.dhw_active = true;
            if (eventItem->getValue() == "➖")
                panelSettings.dhw_active = false;
        }
        // температура ГВС
        if (printStr.indexOf(F("OTget26")) != -1)
        {
            values.dhw_temp = eventItem->getValue().toInt();
            oflineTimer = millis();
        }

        // целевая температура ГВС
        if (printStr.indexOf(F("OTset56")) != -1)
            panelSettings.dhw_setpoint_temp = eventItem->getValue().toInt();
        // Иконка горелки
        if (printStr.indexOf(F("isFlameOn")) != -1)
        {
            if (eventItem->getValue() == "🔥работает")
                panelSettings.flame = true;
            else
                panelSettings.flame = false;
        }
        // модуляция
        if (printStr.indexOf(F("OTget17")) != -1)
            values.flame_curent = eventItem->getValue().toInt();
        // комнатная температура
        if (printStr.indexOf(F("temperature")) != -1)
            values.room_temp = atof(eventItem->getValue().c_str());

        // комнатная целевая температура setTemperature
        if (printStr.indexOf(F("setTemperature")) != -1)
        {
            panelSettings.room_setpoint_temp = atof(eventItem->getValue().c_str());
            // Преобразуем значение для термостата
            float x_min = 17, x_max = 27; // Входной диапазон
            float y_min = 0, y_max = 100; // Выходной диапазон
            panelSettings.room_setpoint = map_value(panelSettings.room_setpoint_temp, x_min, x_max, y_min, y_max);
        }
        // AUTO
        if (printStr.indexOf(F("auto")) != -1)
            panelSettings.room_thermostat = eventItem->getValue().toInt();

        //<------------------------------LiveControl---------------------
        // refreshDisplay();
    }

    IoTValue execute(String command, std::vector<IoTValue> &param)
    {
        if (command == "brightness")
        {
            if (param.size())
            {
                smartdisplay_lcd_set_backlight(float(param[0].valD) / 100);
            }
        }
        else if (command == "display")
        {
            if (param.size())
            {
                panelSettings.displaymode = std::stoi(param[0].valS);
            }
        }

        return {};
    }
    //=======================================================================================================
    // loop()
    void loop()
    {

        if (panelSettings.displaymode != dispMode)
        {
            dispMode = panelSettings.displaymode;
            //    value.valD = dispMode;
            //    regEvent(value.valD, "OTPanel");
        }
        auto const now = millis();
        if (now > next_millis)
        {
            next_millis = now + 500;
            char text_buffer[32];
            sprintf(text_buffer, "%lu", now);
        }
        // обновление экрана
        if (now > next_millis_display)
        {
            next_millis_display = now + 2000;
            refreshDisplay();
        }
        // Update the ticker
        lv_tick_inc(now - lv_last_tick);
        lv_last_tick = now;
        // Update the UI
        lv_timer_handler();
        ui_tick();

        IoTItem::loop();
    }

    ~OTPanel() {};
};

void *getAPI_OTPanel(String subtype, String param)
{
    if (subtype == F("OTPanel"))
    {
        return new OTPanel(param);
    }
    else
    {
        return nullptr;
    }
}
