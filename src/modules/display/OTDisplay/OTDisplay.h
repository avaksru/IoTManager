#ifndef VALUES_H // Защита от повторного включения
#define VALUES_H
#include "Global.h"
struct Values
{
    int brightness_active;
    int brightness_idle;
    float room_temp;
    int ch_temp;
    int dhw_temp;
    bool disp_indoor;
    bool disp_setpoint;
    int flame_curent;
    bool boiler;
    bool isBoilerMQTTconnected;
    String firmware;
    String boilerTopic;
    String FaultText;
    bool relay1;
    bool relay2;
    bool relay3;
};

extern struct Values values;

struct Settings
{
    int displaymode;
    float room_setpoint_temp;
    int room_setpoint;
    int ch_setpoint_temp;
    bool ch_enabled;
    bool ch_active;
    int dhw_setpoint_temp;
    bool dhw_enabled;
    bool dhw_active;
    bool room_thermostat;
    bool flame;
    bool FaultStatus;
    bool equitherm;
    bool pid;
};

extern struct Settings panelSettings;

#endif // VALUES_H