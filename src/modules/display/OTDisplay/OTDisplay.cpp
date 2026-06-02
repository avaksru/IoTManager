#include "Global.h"
#include "classes/IoTItem.h"
#include <esp32_smartdisplay.h>
#include "lvgl.h"
#include <Preferences.h>
// #include "conf/_config.h"
#include "ui/ui.h"
#include "ui/images.h"
#include "ui/actions.h"
#include "OTDisplay.h"
#include "NTP.h"

struct Values values;
struct Settings panelSettings;
bool is_initialized = false;
unsigned long now;
class OTDisplay : public IoTItem
{
private:
    //=======================================================================================================
    // Секция переменных.
    ulong next_millis = 0;
    ulong next_millis_display = 0;
    ulong lv_last_tick = millis();
    ulong oflineTimer = millis();
    int first_start = 0;
    String _controllerID;
    String _prefix;
    String _relay1;
    String _relay2;
    String _relay3;
    bool FaultStatus = 0;
    // bool isBoilerMQTTconnected = 0;
    int _rotation = 0;
    int dispMode = 0;
    int log = 1;

public:
    //=======================================================================================================
    // setup()

    OTDisplay(String parameters) : IoTItem(parameters)
    {
        jsonRead(parameters, F("controllerID"), _controllerID);
        jsonRead(parameters, F("rotation"), _rotation);
        jsonRead(parameters, F("brightness_active"), values.brightness_active);
        jsonRead(parameters, F("brightness_idle"), values.brightness_idle);
        jsonRead(parameters, F("firmware"), values.firmware);
        jsonRead(parameters, F("prefix"), _prefix);
        jsonRead(parameters, F("relay1"), _relay1);
        jsonRead(parameters, F("relay2"), _relay2);
        jsonRead(parameters, F("relay3"), _relay3);
        jsonRead(parameters, F("log"), log);

        values.isBoilerMQTTconnected = 0;

        if (values.firmware == "zigbee2mqtt")
            values.boilerTopic = _prefix + "/" + _controllerID;
        else if (values.firmware == "HOMEd")
            values.boilerTopic = _prefix + "/td/zigbee/" + _controllerID;
        else
            values.boilerTopic = _prefix;

        if (values.firmware == "SmartTherm") // || values.firmware == "HOMEd")
        {
            LoadConfig();
        }

        panelSettings.displaymode = 0;

        if (is_initialized)
        {
            log_w("Smart Display already initialized");
        }
        else
        {
            smartdisplay_init();
            __attribute__((unused)) auto disp = lv_disp_get_default();
            // lv_disp_set_brightness(disp, values.brightness);

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

        // Показываем реле если они есть
        IoTItem *tmp;
        tmp = findIoTItem(_relay1);
        if (tmp)
        {
            values.relay1 = tmp->getValue().toDouble();
            lv_obj_clear_flag(objects.relay1, LV_OBJ_FLAG_HIDDEN);
            if (values.relay1)
            {
                lv_obj_set_style_border_color(objects.relay1, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.btn1_text, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            else
            {
                lv_obj_set_style_border_color(objects.relay1, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.btn1_text, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
        tmp = findIoTItem(_relay2);
        if (tmp)
        {
            values.relay2 = tmp->getValue().toDouble();
            lv_obj_clear_flag(objects.relay2, LV_OBJ_FLAG_HIDDEN);
            if (values.relay2)
            {
                lv_obj_set_style_border_color(objects.relay2, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.btn2_text, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            else
            {
                lv_obj_set_style_border_color(objects.relay2, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.btn2_text, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
        tmp = findIoTItem(_relay3);
        if (tmp)
        {
            values.relay3 = tmp->getValue().toDouble();
            lv_obj_clear_flag(objects.relay3, LV_OBJ_FLAG_HIDDEN);
            if (values.relay3)
            {
                lv_obj_set_style_border_color(objects.relay3, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.btn3_text, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            else
            {
                lv_obj_set_style_border_color(objects.relay3, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.btn3_text, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
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

        if (values.isBoilerMQTTconnected)
        {

            String text_top_center = lv_label_get_text(objects.text_top_center);

            if ((unsigned long)(millis() - oflineTimer) > 61000UL)
            {
                // SerialPrint("i", "OTDisplay", " oflineTimer: " + String(oflineTimer) + " millis: " + String(millis() - 61000));
                lv_label_set_text(objects.text_top_center, ("Ожидаем подключение котла"));
                lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
            }
            else if (panelSettings.displaymode == 0 && (text_top_center == "Установка температуры ГВС" || text_top_center == "Установка комнатной температуры" || text_top_center == "Установка температуры отопления" || text_top_center == "Котел подключен" || text_top_center == "Ожидаем подключение котла"))
            {
                lv_label_set_text(objects.text_top_center, (""));
                lv_obj_add_flag(objects.boiler_status, LV_OBJ_FLAG_HIDDEN);
            }
            // иконка подключения котла ()
            if (values.boiler && !panelSettings.FaultStatus && text_top_center == "Потеря связи OpenTherm")
            {
                // lv_img_set_src(objects.boiler_status_image, &img_ok);
                // lv_obj_set_style_image_opa(objects.boiler_status_image, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                // lv_obj_clear_flag(objects.boiler_status, LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(objects.text_top_center, "");
                lv_obj_add_flag(objects.boiler_status, LV_OBJ_FLAG_HIDDEN);
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
        }
        // Иконка CH
        // SerialPrint("i", "OTDisplay", " panelSettings.ch_active: " + String(panelSettings.ch_active) + " panelSettings.ch_enabled: " + String(panelSettings.ch_enabled));
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
            // SerialPrint("i", "OTDisplay", String(values.room_temp));
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

    // Публикуем комнатную температуру с экрана в mqtt для контроллера
    // TO DOO !!!! доделать для всех систем
    void sendTemperatere(float roomTemperature)
    {
        if (values.firmware == "LiveControl")
        {
            publish(values.boilerTopic + "temperature/control", String(roomTemperature));
        }
        if (values.firmware == "OTGateway")
        {
            //    publish(values.boilerTopic + "/settings/set", "{\"heating\": {\"enabled\": false}}");
        }
        if (values.firmware == "SmartTherm")
        {
            // publish(values.boilerTopic + "Boiler/mode_cmd_t", "off");
        }
        if (values.firmware == "HOMEd")
        {
            // publish(values.boilerTopic + "/1", "{\"systemMode\":\"off\"}");
        }
        if (values.firmware == "zigbee2mqtt")
        {
            // publish(values.boilerTopic + "/thermostat1/set/system_mode", "off");
        }
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

        // если есть локальный виджет температуры то берем из него данные
        IoTItem *disp_indoor_temp = findIoTItem("disp_indoor_temp");
        if (disp_indoor_temp)
        {
            SerialPrint("i", "OTDisplay", "Локальный сенсор температуры");
            values.room_temp = atof(disp_indoor_temp->getValue().c_str());
            values.disp_indoor = true;
            // Передаем комнатнуютемпературу на контроллер
            sendTemperatere(values.room_temp);
        }
        // если есть локальный виджет заданной температуры то берем из него данные
        /*
         IoTItem *disp_setpoint_temp = findIoTItem("disp_setpoint_temp");
         if (disp_setpoint_temp)
         {
             panelSettings.room_setpoint_temp = atof(disp_setpoint_temp->getValue().c_str());
             values.disp_setpoint = true;
             //  Преобразуем значение для термостата
             float x_min = 17, x_max = 27; // Входной диапазон
             float y_min = 0, y_max = 100; // Выходной диапазон
             // Вычисляем новое значение с шагом 0.5
             panelSettings.room_setpoint = map_value(panelSettings.room_setpoint_temp, x_min, x_max, y_min, y_max);
         }
             */
        // обновляем часы

        if (first_start == 1 && isNetworkActive())
        {
            lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
            lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
        }

        // Если отсутствуют данные от котла
        if (first_start == 1 && !values.isBoilerMQTTconnected)
        {
            lv_label_set_text(objects.text_top_center, ("Ожидаем подключение котла (" + jsonReadStr(settingsFlashJson, F("ip")) + ")").c_str());
            lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
        }

        // выводим статус подключения котла, WiFi и MQTT
        if (!mqttIsConnect())
        {
            // lv_label_set_text(objects.text_top_center, "Отсутствует подключение MQTT");
            lv_label_set_text(objects.text_top_center, ("Отсутствует подключение MQTT (" + jsonReadStr(settingsFlashJson, F("ip")) + ")").c_str());
            lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
            values.isBoilerMQTTconnected = 0;
            first_start = 0;
        }

        if (!isNetworkActive())
        {
            lv_label_set_text(objects.text_top_center, "Отсутствует подключение WiFi");
            lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
            values.isBoilerMQTTconnected = 0;
            first_start = 0;
        }
        if (jsonReadStr(settingsFlashJson, "control") == "fail")
        {
            lv_label_set_text(objects.text_top_center, "Отсутствует лицензия");
            lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(objects.text_top_center, lv_color_hex(0xfff90000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    // Обрабатываем входящие mqtt пакеты
    void onMqttRecive(String &topic, String &payloadStr)
    {
        if (log > 0)
        {
            SerialPrint("i", "OTDisplay", " topic: " + topic + " msg: " + payloadStr);
        }
        //------------------------------LiveControl--------------------->
        if (values.firmware == "LiveControl")
        {
            if (payloadStr.indexOf("HELLO") == -1)
            {
                // payloadStr.replace(",", ".");
                // SerialPrint("i", "OTDisplay", " topic: " + topic + " msg: " + payloadStr);
                // статус офлайн
                if (topic.indexOf(F("/state")) != -1)
                {
                    // if (jsonReadStr(payloadStr, "status") == "online")
                    // values.isBoilerMQTTconnected = 1;
                    // пока закомментирую так как может из за этого показывает ложную надпись
                    if (jsonReadStr(payloadStr, "status") == "offline")
                    {
                        //    values.isBoilerMQTTconnected = 0;
                        //    lv_label_set_text(objects.text_top_center, ("Ожидаем подключение котла (" + jsonReadStr(settingsFlashJson, F("ip")) + ")").c_str());
                        //    lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
                    }
                }
                // подключение к котлу
                if (topic.indexOf(F("boiler/status")) != -1)
                {
                    values.boilerTopic = "/" + selectFromMarkerToMarker(topic, "/", 1) + "/" + selectFromMarkerToMarker(topic, "/", 2) + "/";
                    //    SerialPrint("i", "OTDisplay", " boilerTopic: " + values.boilerTopic);
                    if (jsonReadStr(payloadStr, "status") == "✅")
                        values.boiler = 1;
                    if (jsonReadStr(payloadStr, "status") == "❌")
                        values.boiler = 0;

                    // убираем надпись "ожидание котла"
                    if (values.isBoilerMQTTconnected == 0)
                    {
                        lv_label_set_text(objects.text_top_center, "");
                        lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
                        lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
                    }
                    values.isBoilerMQTTconnected = 1;
                    oflineTimer = millis();
                }
                // ошибка котла
                if (topic.indexOf(F("LastFaultType/status")) != -1)
                {
                    if ((jsonReadStr(payloadStr, "status") == "нет") || String(jsonReadInt(payloadStr, "status")) == "0")
                        panelSettings.FaultStatus = 0;
                    else
                        panelSettings.FaultStatus = 1;
                    values.FaultText = jsonReadStr(payloadStr, "status");
                }
                // CH разрешен
                if (topic.indexOf(F("BoilerEnable/status")) != -1)
                {
                    if (jsonReadStr(payloadStr, "status") == "1")
                        panelSettings.ch_enabled = true;
                    if (jsonReadStr(payloadStr, "status") == "0")
                        panelSettings.ch_enabled = false;
                }
                // CH активен
                if (topic.indexOf(F("isHeatingEnabled/status")) != -1)
                {
                    if (jsonReadStr(payloadStr, "status") == "✅")
                        panelSettings.ch_active = true;
                    if (jsonReadStr(payloadStr, "status") == "➖")
                        panelSettings.ch_active = false;
                }
                // температура CH
                if (topic.indexOf(F("OTget25/status")) != -1)
                {
                    values.ch_temp = jsonReadInt(payloadStr, "status");
                    oflineTimer = millis();
                }
                // целевая температура CH
                if (topic.indexOf(F("OTset1/status")) != -1)
                    panelSettings.ch_setpoint_temp = jsonReadInt(payloadStr, "status");
                // DHW разрешен
                if (topic.indexOf(F("DHWenable/status")) != -1)
                {
                    if (jsonReadStr(payloadStr, "status") == "1")
                        panelSettings.dhw_enabled = true;
                    if (jsonReadStr(payloadStr, "status") == "0")
                        panelSettings.dhw_enabled = false;
                }
                // DHW активен
                if (topic.indexOf(F("isDHWenabled/status")) != -1)
                {
                    if (jsonReadStr(payloadStr, "status") == "✅")
                        panelSettings.dhw_active = true;
                    if (jsonReadStr(payloadStr, "status") == "➖")
                        panelSettings.dhw_active = false;
                }
                // температура ГВС
                if (topic.indexOf(F("OTget26/status")) != -1)
                {
                    values.dhw_temp = jsonReadInt(payloadStr, "status");
                    oflineTimer = millis();
                }
                // целевая температура ГВС
                if (topic.indexOf(F("OTset56/status")) != -1)
                    panelSettings.dhw_setpoint_temp = jsonReadInt(payloadStr, "status");
                // Иконка горелки
                if (topic.indexOf(F("isFlameOn/status")) != -1)
                {
                    if (jsonReadStr(payloadStr, "status") == "🔥работает")
                        panelSettings.flame = true;
                    else
                        panelSettings.flame = false;
                    oflineTimer = millis();
                }
                // модуляция
                if (topic.indexOf(F("OTget17/status")) != -1)
                {
                    values.flame_curent = jsonReadInt(payloadStr, "status");
                    oflineTimer = millis();
                }
                // комнатная температура
                if (topic.indexOf(F("/temperature/status")) != -1 && !values.disp_indoor)
                    values.room_temp = atof(jsonReadStr(payloadStr, "status").c_str());

                // комнатная целевая температура
                if (topic.indexOf(F("setTemperature/status")) != -1)
                {
                    panelSettings.room_setpoint_temp = atof(jsonReadStr(payloadStr, "status").c_str());
                    // Преобразуем значение для термостата
                    float x_min = 17, x_max = 27; // Входной диапазон
                    float y_min = 0, y_max = 100; // Выходной диапазон
                    panelSettings.room_setpoint = map_value(panelSettings.room_setpoint_temp, x_min, x_max, y_min, y_max);
                }
                // AUTO
                if (topic.indexOf(F("auto/status")) != -1)
                    panelSettings.room_thermostat = jsonReadInt(payloadStr, "status");
            }
        }

        //<------------------------------LiveControl---------------------
        //------------------------------SmartTherm--------------------->
        if (values.firmware == "SmartTherm")
        {
            payloadStr.replace(",", ".");

            // подключение к котлу
            if (topic.indexOf(F("_OT/stat_t")) != -1)
            {
                // mqttPrefix
                values.boilerTopic = "aha/" + _prefix + "/" + _prefix + "_";
                if (payloadStr == "ON")
                    values.boiler = 1;
                if (payloadStr == "OFF")
                    values.boiler = 0;

                // убираем надпись "ожидание котла"
                if (values.isBoilerMQTTconnected == 0)
                {
                    lv_label_set_text(objects.text_top_center, "");
                    lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
                    lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
                }
                values.isBoilerMQTTconnected = 1;
                oflineTimer = millis();
                WriteConfig();
            }
            // костыль для ST определяем доступность котла по топику online
            if (topic.indexOf(F("/avty_t")) != -1)
            {
                // mqttPrefix
                values.boilerTopic = "aha/" + _prefix + "/" + _prefix + "_";
                if (payloadStr == "online")
                    // убираем надпись "ожидание котла"
                    if (values.isBoilerMQTTconnected == 0)
                    {
                        lv_label_set_text(objects.text_top_center, "");
                        lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
                        lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
                    }
                values.isBoilerMQTTconnected = 1;
                oflineTimer = millis();
                if (payloadStr == "offline")
                {
                    values.isBoilerMQTTconnected = 0;
                    lv_label_set_text(objects.text_top_center, ("Ожидаем подключение котла (" + jsonReadStr(settingsFlashJson, F("ip")) + ")").c_str());
                }
            }
            // ошибка котла
            if (topic.indexOf(F("_Err/stat_t")) != -1)
            {
                if (payloadStr == "нет")
                    panelSettings.FaultStatus = 0;
                else
                    panelSettings.FaultStatus = 1;

                WriteConfig();
            }
            // CH разрешен
            // if (topic.indexOf(F("_CH/stat_t")) != -1)
            if (topic.indexOf(F("_Boiler/mode_stat_t")) != -1)
            {
                if (payloadStr == "heat")
                    panelSettings.ch_enabled = true;
                if (payloadStr == "off")
                    panelSettings.ch_enabled = false;

                WriteConfig();
            }
            // CH активен
            if (topic.indexOf(F("_CH/stat_t")) != -1)
            {
                if (payloadStr == "ON")
                    panelSettings.ch_active = true;
                if (payloadStr == "OFF")
                    panelSettings.ch_active = false;

                // WriteConfig();
            }
            // температура CH
            if (topic.indexOf(F("_Boiler/curr_temp_t")) != -1)
            {
                values.ch_temp = atof(payloadStr.c_str());
                // values.ch_temp = payloadStr.toInt();

                // костыль для ST
                // доступность котла
                values.boiler = 1;
                // mqttPrefix
                values.boilerTopic = "aha/" + _prefix + "/" + _prefix + "_";
                // убираем надпись "ожидание котла"
                if (values.isBoilerMQTTconnected == 0)
                {
                    lv_label_set_text(objects.text_top_center, "");
                    lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
                    lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
                }
                values.isBoilerMQTTconnected = 1;
                oflineTimer = millis();
            }
            // целевая температура CH
            if (topic.indexOf(F("_Boiler/temp_stat_t")) != -1)
            {
                //    panelSettings.ch_setpoint_temp = payloadStr.toInt();
                panelSettings.ch_setpoint_temp = atof(payloadStr.c_str());

                WriteConfig();
            }
            // DHW разрешен
            if (topic.indexOf(F("_DHW/mode_stat_t")) != -1)
            {
                if (payloadStr == "heat")
                    panelSettings.dhw_enabled = true;
                if (payloadStr == "off")
                    panelSettings.dhw_enabled = false;
                WriteConfig();
            }
            // DHW активен
            if (topic.indexOf(F("_HW/stat_t")) != -1)
            {
                if (payloadStr == "ON")
                    panelSettings.dhw_active = true;
                if (payloadStr == "OFF")
                    panelSettings.dhw_active = false;

                // WriteConfig();
            }
            // температура ГВС
            if (topic.indexOf(F("_DHW/curr_temp_")) != -1)
                // values.dhw_temp = payloadStr.toInt();
                values.dhw_temp = atof(payloadStr.c_str());

            // целевая температура ГВС
            if (topic.indexOf(F("_DHW/temp_stat_t")) != -1)
            {
                // panelSettings.dhw_setpoint_temp = payloadStr.toInt();
                panelSettings.dhw_setpoint_temp = atof(payloadStr.c_str());
                WriteConfig();
            }
            // Иконка горелки
            if (topic.indexOf(F("_Flame/stat_t")) != -1)
            {
                if (payloadStr == "ON")
                    panelSettings.flame = true;
                else
                    panelSettings.flame = false;

                // WriteConfig();
            }
            // модуляция
            if (topic.indexOf(F("_Modulation/stat_t")) != -1)
                // values.flame_curent = payloadStr.toInt();
                values.flame_curent = atof(payloadStr.c_str());

            // комнатная температура
            if (topic.indexOf(F("_Tindoor/stat_t")) != -1 && !values.disp_indoor)
                values.room_temp = atof(payloadStr.c_str());
            // values.room_temp     = payloadStr.toInt();
            // комнатная целевая температура
            if (topic.indexOf(F("_PID/temp_stat_t")) != -1)
            {
                panelSettings.room_setpoint_temp = atof(payloadStr.c_str());
                // Преобразуем значение для термостата
                float x_min = 17, x_max = 27; // Входной диапазон
                float y_min = 0, y_max = 100; // Выходной диапазон
                panelSettings.room_setpoint = map_value(panelSettings.room_setpoint_temp, x_min, x_max, y_min, y_max);

                WriteConfig();
            }
            // AUTO
            if (topic.indexOf(F("_PID/mode_stat_t")) != -1)
            {
                if (payloadStr == "auto")
                    panelSettings.room_thermostat = true;
                else
                    panelSettings.room_thermostat = false;

                WriteConfig();
            }
        }

        //<-----------------------------------------SmartTherm------------------------------------------------------

        //-----------------------------------------OTGateway------------------------------------------------------>
        if (values.firmware == "OTGateway")
        {
            if (payloadStr.indexOf("HELLO") == -1)
            {
                // payloadStr.replace(",", ".");
                // SerialPrint("i", "OTDisplay", " topic: " + topic + " msg: " + payloadStr);

                // статус офлайн
                if (topic.indexOf(F("/status")) != -1)
                {
                    if (payloadStr == "offline")
                    {
                        values.isBoilerMQTTconnected = 0;
                        lv_label_set_text(objects.text_top_center, ("Ожидаем подключение котла (" + jsonReadStr(settingsFlashJson, F("ip")) + ")").c_str());
                        lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
                        // lv_obj_add_flag(objects.boiler_status_image, LV_OBJ_FLAG_HIDDEN);
                    }
                }

                // температура DHW
                if (topic.indexOf(F("/sensors/dhw_temp")) != -1)
                {
                    values.dhw_temp = atof(jsonReadStr(payloadStr, "value").c_str());
                }
                // температура CH
                if (topic.indexOf(F("/sensors/heating_temp")) != -1)
                {
                    values.ch_temp = atof(jsonReadStr(payloadStr, "value").c_str());
                }

                // целевая CH
                if (topic.indexOf(F("/sensors/heating_setpoint_temp")) != -1)
                {
                    panelSettings.ch_setpoint_temp = atof(jsonReadStr(payloadStr, "value").c_str());
                }

                // комнатная температура
                if (topic.indexOf(F("/sensors/indoor_temp")) != -1 && !values.disp_indoor)
                {
                    values.room_temp = atof(jsonReadStr(payloadStr, "value").c_str());
                }
                // мдуляция
                if (topic.indexOf(F("/sensors/modulation_level")) != -1)
                {
                    values.flame_curent = atof(jsonReadStr(payloadStr, "value").c_str());
                }

                // получены статусы
                if (topic.indexOf(F("/state")) != -1)
                {
                    // убираем надпись "ожидание котла"
                    if (values.isBoilerMQTTconnected == 0)
                    {
                        lv_label_set_text(objects.text_top_center, "");
                        lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
                        lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
                    }
                    values.isBoilerMQTTconnected = 1;
                    oflineTimer = millis();
                    String slave = jsonReadStr(payloadStr, "slave");
                    // DHW активен
                    String dhw = jsonReadStr(slave, "dhw");
                    panelSettings.dhw_active = jsonReadBool(dhw, "active");
                    // CH активен
                    String heating = jsonReadStr(slave, "heating");
                    panelSettings.ch_active = jsonReadBool(heating, "active");
                    // OT conect
                    values.boiler = jsonReadBool(slave, "connected");
                    // ошибка котла
                    String fault = jsonReadStr(slave, "fault");
                    panelSettings.FaultStatus = jsonReadBool(fault, "active");
                    // slave.fault.code
                    values.FaultText = jsonReadStr(fault, "code");
                    // flameState
                    panelSettings.flame = jsonReadBool(slave, "flame");
                }

                // получены настройки
                if (topic.indexOf(F("/settings")) != -1)
                {

                    String pid = jsonReadStr(payloadStr, "pid");
                    // PID разрешен
                    panelSettings.pid = jsonReadBool(pid, "enabled");

                    String equitherm = jsonReadStr(payloadStr, "equitherm");
                    // Эквитермия разрешена
                    panelSettings.equitherm = jsonReadBool(equitherm, "enabled");

                    //    SerialPrint("i", "OTDisplay", " topic: " + topic + " msg: " + payloadStr);
                    String heating = jsonReadStr(payloadStr, "heating");
                    // CH разрешен
                    panelSettings.ch_enabled = jsonReadBool(heating, "enabled");

                    // целевая температура CH или PID
                    if (panelSettings.pid || panelSettings.equitherm)
                    {
                        panelSettings.room_setpoint_temp = atof(jsonReadStr(heating, "target").c_str());
                        // Преобразуем значение для термостата
                        float x_min = 17, x_max = 27; // Входной диапазон
                        float y_min = 0, y_max = 100; // Выходной диапазон
                        panelSettings.room_setpoint = map_value(panelSettings.room_setpoint_temp, x_min, x_max, y_min, y_max);
                        //   lv_arc_set_value(objects.arc, panelSettings.room_setpoint);
                    }
                    else
                    {
                        panelSettings.ch_setpoint_temp = jsonReadInt(heating, "target");
                    }

                    String dhw = jsonReadStr(payloadStr, "dhw");
                    // ГВС разрешен
                    panelSettings.dhw_enabled = jsonReadBool(dhw, "enabled");
                    // целевая температура ГВС
                    panelSettings.dhw_setpoint_temp = jsonReadInt(dhw, "target");
                }
            }
        }

        //<-----------------------------------------OTGateway------------------------------------------------------
        //_-----------------------------------------HOMEd------------------------------------------------------>
        if (values.firmware == "HOMEd")
        {

            if (payloadStr.indexOf("HELLO") == -1)
            {
                // payloadStr.replace(",", ".");

                // статус офлайн
                if (topic.indexOf(F("/device/zigbee")) != -1)
                {
                    if (jsonReadStr(payloadStr, "status") && jsonReadStr(payloadStr, "status") == "offline")
                    {
                        values.isBoilerMQTTconnected = 0;
                        lv_label_set_text(objects.text_top_center, ("Ожидаем подключение котла (" + jsonReadStr(settingsFlashJson, F("ip")) + ")").c_str());
                        lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
                        // lv_obj_add_flag(objects.boiler_status_image, LV_OBJ_FLAG_HIDDEN);
                    }
                }

                // если EP 4
                if (topic.indexOf("fd/zigbee/" + _controllerID + "/4") != -1)
                {
                    // комнатная температура
                    if (jsonReadInt(payloadStr, "temperature") && !values.disp_indoor)
                        values.room_temp = atof(jsonReadStr(payloadStr, "value").c_str());
                }
                else if (topic.indexOf("fd/zigbee/" + _controllerID + "/3") != -1)
                {
                    // комнатный термостат
                    if (jsonReadInt(payloadStr, "temperature"))
                        values.room_temp = atof(jsonReadStr(payloadStr, "temperature").c_str());
                    // целевая комнатная температура
                    if (jsonReadInt(payloadStr, "targetTemperature"))
                    {
                        panelSettings.room_setpoint_temp = atof(jsonReadStr(payloadStr, "targetTemperature").c_str());
                        // Преобразуем значение для термостата
                        float x_min = 17, x_max = 27; // Входной диапазон
                        float y_min = 0, y_max = 100; // Выходной диапазон
                        panelSettings.room_setpoint = map_value(panelSettings.room_setpoint_temp, x_min, x_max, y_min, y_max);
                        //  WriteConfig();
                    }
                    // статус термостата
                    if (jsonReadStr(payloadStr, "systemMode"))
                    {
                        if (jsonReadStr(payloadStr, "systemMode") == "off")
                            panelSettings.room_thermostat = false;
                        else
                            panelSettings.room_thermostat = true;

                        //  WriteConfig();
                    }
                }
                else if (topic.indexOf("fd/zigbee/" + _controllerID + "/2") != -1)
                {
                    // ГВС термостат
                    if (jsonReadInt(payloadStr, "temperature"))
                        values.dhw_temp = atof(jsonReadStr(payloadStr, "temperature").c_str());
                    // целевая DHW температура
                    if (jsonReadInt(payloadStr, "targetTemperature"))
                    {
                        panelSettings.dhw_setpoint_temp = atof(jsonReadStr(payloadStr, "targetTemperature").c_str());
                        //  WriteConfig();
                    }
                    // статус термостата
                    if (jsonReadStr(payloadStr, "system_mode_thermostat2"))
                    {
                        if (jsonReadStr(payloadStr, "system_mode_thermostat2") == "off")
                        {
                            panelSettings.dhw_enabled = 0;
                            panelSettings.dhw_active = 0;
                        }
                        else
                        {
                            panelSettings.dhw_enabled = 1;
                        }

                        //  WriteConfig();
                    }
                }
                else if (topic.indexOf("fd/zigbee/" + _controllerID + "/1") != -1)
                {
                    // убираем надпись "ожидание котла"
                    if (values.isBoilerMQTTconnected == 0)
                    {
                        lv_label_set_text(objects.text_top_center, "");
                        lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
                        lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
                    }
                    values.isBoilerMQTTconnected = 1;
                    values.boiler = 1;
                    oflineTimer = millis();
                    // CH термостат
                    if (jsonReadInt(payloadStr, "temperature"))
                        values.ch_temp = atof(jsonReadStr(payloadStr, "temperature").c_str());
                    // целевая CH температура
                    if (jsonReadInt(payloadStr, "targetTemperature"))
                    {
                        //   panelSettings.ch_setpoint_temp = jsonReadInt(payloadStr, "targetTemperature");
                        panelSettings.ch_setpoint_temp = atof(jsonReadStr(payloadStr, "targetTemperature").c_str());
                        //  WriteConfig();
                    }
                    // статус термостата
                    if (jsonReadStr(payloadStr, "system_mode_thermostat1"))
                    {
                        if (jsonReadStr(payloadStr, "system_mode_thermostat1") == "off")
                        {
                            panelSettings.ch_enabled = 0;
                            panelSettings.ch_active = 0;
                        }
                        else
                        {
                            panelSettings.ch_enabled = 1;
                        }

                        //  WriteConfig();
                    }
                }
            }
        }
        //_-----------------------------------------HOMEd------------------------------------------------------>

        //_-----------------------------------------zigbee2mqtt------------------------------------------------------>
        if (values.firmware == "zigbee2mqtt")
        {

            if (payloadStr.indexOf("HELLO") == -1)
            {
                // payloadStr.replace(",", ".");

                // статус офлайн
                /*
                if (topic.indexOf(F("/device/zigbee")) != -1)
                {
                    if (jsonReadStr(payloadStr, "status") && jsonReadStr(payloadStr, "status") == "offline")
                    {
                        values.isBoilerMQTTconnected = 0;
                           lv_label_set_text(objects.text_top_center, ("Ожидаем подключение котла (" + jsonReadStr(settingsFlashJson, F("ip")) + ")").c_str());
                        lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
                        lv_obj_add_flag(objects.boiler_status_image, LV_OBJ_FLAG_HIDDEN);
                    }
                }
                */

                // если EP 4
                // комнатная температура
                if (jsonReadInt(payloadStr, "temperature_4") && !values.disp_indoor)
                    values.room_temp = atof(jsonReadStr(payloadStr, "temperature_4").c_str());

                // комнатный термостат
                if (jsonReadInt(payloadStr, "local_temperature_thermostat3"))
                    values.room_temp = atof(jsonReadStr(payloadStr, "local_temperature_thermostat3").c_str());
                // целевая комнатная температура
                if (jsonReadInt(payloadStr, "occupied_heating_setpoint_thermostat3"))
                {
                    panelSettings.room_setpoint_temp = atof(jsonReadStr(payloadStr, "occupied_heating_setpoint_thermostat3").c_str());
                    // Преобразуем значение для термостата
                    float x_min = 17, x_max = 27; // Входной диапазон
                    float y_min = 0, y_max = 100; // Выходной диапазон
                    panelSettings.room_setpoint = map_value(panelSettings.room_setpoint_temp, x_min, x_max, y_min, y_max);
                    // WriteConfig();
                }
                // статус термостата комнатного
                if (jsonReadStr(payloadStr, "system_mode_thermostat3"))
                {
                    if (jsonReadStr(payloadStr, "system_mode_thermostat3") == "off")
                        panelSettings.room_thermostat = false;
                    else
                        panelSettings.room_thermostat = true;

                    //   WriteConfig();
                }

                // ГВС термостат
                if (jsonReadInt(payloadStr, "local_temperature_thermostat2"))
                    values.dhw_temp = atof(jsonReadStr(payloadStr, "local_temperature_thermostat2").c_str());
                // целевая DHW температура
                if (jsonReadInt(payloadStr, "occupied_heating_setpoint_thermostat2"))
                {
                    panelSettings.dhw_setpoint_temp = atof(jsonReadStr(payloadStr, "occupied_heating_setpoint_thermostat2").c_str());
                    //  WriteConfig();
                }
                // статус термостата ГВС
                if (jsonReadStr(payloadStr, "system_mode_thermostat2"))
                {
                    if (jsonReadStr(payloadStr, "system_mode_thermostat2") == "off")
                    {
                        panelSettings.dhw_enabled = 0;
                        panelSettings.dhw_active = 0;
                    }
                    else
                    {
                        panelSettings.dhw_enabled = 1;
                    }

                    //  WriteConfig();
                }

                // убираем надпись "ожидание котла"
                if (values.isBoilerMQTTconnected == 0)
                {
                    lv_label_set_text(objects.text_top_center, "");
                    lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
                    lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
                }
                values.isBoilerMQTTconnected = 1;
                values.boiler = 1;
                oflineTimer = millis();
                // CH термостат
                if (jsonReadInt(payloadStr, "local_temperature_thermostat1"))
                    values.ch_temp = atof(jsonReadStr(payloadStr, "local_temperature_thermostat1").c_str());
                // целевая CH температура
                if (jsonReadInt(payloadStr, "occupied_heating_setpoint_thermostat1"))
                {
                    //   panelSettings.ch_setpoint_temp = jsonReadInt(payloadStr, "targetTemperature");
                    panelSettings.ch_setpoint_temp = atof(jsonReadStr(payloadStr, "occupied_heating_setpoint_thermostat1").c_str());
                    //   WriteConfig();
                }
                // статус термостата CH
                /*
                if (jsonReadStr(payloadStr, "system_mode_thermostat1"))
                {
                    if (jsonReadStr(payloadStr, "system_mode_thermostat1") == "off")
                    {
                        panelSettings.ch_enabled = 0;
                        panelSettings.ch_active = 0;
                    }
                    else
                    {
                        panelSettings.ch_enabled = 1;
                    }

                    // WriteConfig();
                }
                    */
                // CH активен
                if (jsonReadStr(payloadStr, "state_ep4") == "OFF")
                {
                    panelSettings.ch_enabled = 0;
                    panelSettings.ch_active = 0;
                }
                else
                {
                    panelSettings.ch_enabled = 1;
                }
            }
        }
        //_-----------------------------------------zigbee2mqtt------------------------------------------------------>
        // refreshDisplay();
        // SerialPrint("i", "OTDisplay", "isBoilerMQTTconnected: " + String(values.isBoilerMQTTconnected) + " values.boiler: " + String(values.boiler));
    }

    void onRegEvent(IoTItem *eventItem)
    {
        if (!eventItem)
            return;
        if (eventItem->getID() == _relay1.c_str())
        {
            values.relay1 = eventItem->getValue().toDouble();
            if (values.relay1)
            {
                lv_obj_set_style_border_color(objects.relay1, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.btn1_text, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            else
            {
                lv_obj_set_style_border_color(objects.relay1, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.btn1_text, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            return;
        }
        else if (eventItem->getID() == _relay2.c_str())
        {
            values.relay2 = eventItem->getValue().toDouble();

            if (values.relay2)
            {
                lv_obj_set_style_border_color(objects.relay2, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.btn2_text, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            else
            {
                lv_obj_set_style_border_color(objects.relay2, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.btn2_text, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            return;
        }
        else if (eventItem->getID() == _relay3.c_str())
        {
            values.relay3 = eventItem->getValue().toDouble();
            if (values.relay3)
            {
                lv_obj_set_style_border_color(objects.relay3, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.btn3_text, lv_color_hex(0xff169223), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            else
            {
                lv_obj_set_style_border_color(objects.relay3, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(objects.btn3_text, lv_color_hex(0x184d81), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            return;
        }
        else
        {
            return;
        }
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
        panelSettings.displaymode = (int)std::stoi(param[0].valS);
            }
        }
        else if (command == "relay1")
        {
            if (param.size())
            {
                lv_label_set_text(objects.btn1_text, param[0].valS.c_str());
            }
        }
        else if (command == "relay2")
        {
            if (param.size())
            {
                lv_label_set_text(objects.btn2_text, param[0].valS.c_str());
            }
        }
        else if (command == "relay3")
        {
            if (param.size())
            {
                lv_label_set_text(objects.btn3_text, param[0].valS.c_str());
            }
        }

        return {};
    }
    //=======================================================================================================
    // loop()
    void loop()
    {
        // выводим статус подключения котла, WiFi и MQTT
        if (!mqttIsConnect())
        {
            // lv_label_set_text(objects.text_top_center, "Отсутствует подключение MQTT");
            lv_label_set_text(objects.text_top_center, ("Отсутствует подключение MQTT (" + jsonReadStr(settingsFlashJson, F("ip")) + ")").c_str());
            lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
            values.isBoilerMQTTconnected = 0;
            first_start = 0;
        }

        if (!isNetworkActive())
        {
            lv_label_set_text(objects.text_top_center, "Отсутствует подключение WiFi");
            lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
            values.isBoilerMQTTconnected = 0;
            first_start = 0;
        }

        if (panelSettings.displaymode != dispMode)
        {
            dispMode = panelSettings.displaymode;
            //    value.valD = dispMode;
            //    regEvent(value.valD, "OTDisplay");
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

        if (first_start == 0 && mqttIsConnect())
        {
            first_start = 1;
            if (values.firmware == "LiveControl")
            {
                if (_controllerID && _controllerID != "")
                {
                    mqtt.subscribe((_prefix + "/" + _controllerID + "/state").c_str());
                    mqtt.subscribe((_prefix + "/" + _controllerID + "/+/status").c_str());
                    SerialPrint("i", "OTDisplay subscribe: ", _prefix + "/" + _controllerID);
                }
                else
                {
                    mqtt.subscribe((_prefix + "/+/state").c_str());
                    mqtt.subscribe((_prefix + "/+/+/status").c_str());
                    SerialPrint("i", "OTDisplay subscribe: ", _prefix);
                }
                // Обновляем статусы
                publish(_prefix, "HELLO");
            }
            if (values.firmware == "SmartTherm")
            {
                mqtt.subscribe(("aha/" + _prefix + "/#").c_str());
                SerialPrint("i", "OTDisplay subscribe: ", "aha/" + _prefix + "/#");
            }
            if (values.firmware == "OTGateway")
            {
                // mqtt.subscribe((_prefix + "/#").c_str());
                mqtt.subscribe((_prefix + "/status").c_str());
                mqtt.subscribe((_prefix + "/state").c_str());
                mqtt.subscribe((_prefix + "/settings").c_str());
                mqtt.subscribe((_prefix + "/sensors/#").c_str());

                SerialPrint("i", "OTDisplay subscribe: ", _prefix);
            }

            if (values.firmware == "HOMEd")
            {

                mqtt.subscribe((_prefix + "/device/zigbee/" + _controllerID).c_str());
                mqtt.subscribe((_prefix + "/fd/zigbee/" + _controllerID).c_str());
                mqtt.subscribe((_prefix + "/fd/zigbee/" + _controllerID + "/#").c_str());
                SerialPrint("i", "OTDisplay subscribe: ", _prefix + "/fd/zigbee/" + _controllerID);

                // Прячем модуляцию
                lv_obj_add_flag(objects.flame_status, LV_OBJ_FLAG_HIDDEN);
            }
            if (values.firmware == "zigbee2mqtt")
            {

                mqtt.subscribe((_prefix + "/" + _controllerID).c_str());
                SerialPrint("i", "OTDisplay subscribe: ", _prefix + "/" + _controllerID);

                // Прячем модуляцию
                lv_obj_add_flag(objects.flame_status, LV_OBJ_FLAG_HIDDEN);
            }
            // выводим время на дисплей
            // lv_label_set_text(objects.text_top_right, getTimeLocal_hhmm().c_str());
            // lv_obj_clear_flag(objects.text_top_right, LV_OBJ_FLAG_HIDDEN);
            // Меняем надпись из лого
            if (!values.isBoilerMQTTconnected)
            {
                lv_label_set_text(objects.text_top_center, ("Ожидаем подключение котла (" + jsonReadStr(settingsFlashJson, F("ip")) + ")").c_str());
                lv_obj_clear_flag(objects.text_top_center, LV_OBJ_FLAG_HIDDEN);
            }
            else
            {
                lv_label_set_text(objects.text_top_center, "");
            }
        }

        IoTItem::loop();
    }

    ~OTDisplay() {};
};

void *getAPI_OTDisplay(String subtype, String param)
{
    if (subtype == F("OTDisplay"))
    {
        return new OTDisplay(param);
    }
    else
    {
        return nullptr;
    }
}
