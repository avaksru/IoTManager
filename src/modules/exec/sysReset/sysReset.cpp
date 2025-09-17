#include "Global.h"
#include "classes/IoTItem.h"

extern IoTGpio IoTgpio;

class sysReset : public IoTItem
{
private:
    int _WiFI_led;
    int _MQTT_led;
    int _OT_led;
    int _RESET_pin;
    int _count = 0;

    int _lastWiFIState = false;
    bool _lastMQTTState = false;
    unsigned long timing;
    int duration = 0;
    bool _reading;
    bool _lastButtonState = LOW;
    unsigned long _lastDebounceTime = 0;
    int _debounceDelay = 50;

public:
    sysReset(String parameters) : IoTItem(parameters)
    {
        String _pinMode;
        jsonRead(parameters, "WiFI_led", _WiFI_led);
        jsonRead(parameters, "MQTT_led", _MQTT_led);
        jsonRead(parameters, "OT_led", _OT_led);
        jsonRead(parameters, "RESET_pin", _RESET_pin);
        jsonRead(parameters, "pinMode", _pinMode);
        jsonRead(parameters, "debounceDelay", _debounceDelay);

        pinMode(_WiFI_led, OUTPUT);
        digitalWrite(_WiFI_led, HIGH);
        pinMode(_MQTT_led, OUTPUT);
        digitalWrite(_MQTT_led, HIGH);
        pinMode(_OT_led, OUTPUT);
        digitalWrite(_OT_led, HIGH);

        if (_pinMode == F("INPUT"))
            IoTgpio.pinMode(_RESET_pin, INPUT);
        else if (_pinMode == F("INPUT_PULLUP"))
            IoTgpio.pinMode(_RESET_pin, INPUT_PULLUP);
        else if (_pinMode == F("INPUT_PULLDOWN"))
        {
            IoTgpio.pinMode(_RESET_pin, INPUT);
            IoTgpio.digitalWrite(_RESET_pin, LOW);
        }

        _lastButtonState = IoTgpio.digitalRead(_RESET_pin);
        SerialPrint("i", F("sysReset"), "RESET_pin " + String(_RESET_pin));
    }

    void doByInterval()
    {

        // LEDs
        if (_lastMQTTState != mqttIsConnect())
        {
            _lastMQTTState = mqttIsConnect();
            if (mqttIsConnect())
            {
                digitalWrite(_MQTT_led, LOW);
                //    SerialPrint("i", F("sysReset"), "mqttIsConnect: " + String(mqttIsConnect()));
            }
            else
            {
                digitalWrite(_MQTT_led, HIGH);
                //    SerialPrint("i", F("sysReset"), "mqttDisConnect: " + String(mqttIsConnect()));
            }
        }
        if (_lastWiFIState != WiFi.getMode())
        {
            _lastWiFIState = WiFi.getMode();
            if (_lastWiFIState == 3 || _lastWiFIState == 2)
            {
                digitalWrite(_WiFI_led, HIGH);
                //    SerialPrint("i", F("sysReset"), "WiFi.AP: " + String(_lastWiFIState));
            }
            else if (_lastWiFIState == 1)
            {

                digitalWrite(_WiFI_led, LOW);
                //    SerialPrint("i", F("sysReset"), ".WiFi.STA: " + String(_lastWiFIState));
            }
            else
            {
                // digitalWrite(_WiFI_led, LOW);
                SerialPrint("i", F("sysReset"), "WiFi.???: " + String(_lastWiFIState));
            }
        }

        for (std::list<IoTItem *>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it)
        {
            if ((*it)->getSubtype() == "OpenThermMonitor")
            {
                IoTItem *item = findIoTItem((*it)->getID());
                if (item && item->getValue() == "✅")
                {
                    digitalWrite(_OT_led, LOW);
                    //      SerialPrint("i", F("sysReset"), "OT Найден: " + String(item->getValue()));
                }
                else
                {
                    digitalWrite(_OT_led, HIGH);
                }
            }
        }
    }
    void SysReset()
    {
        writeFile("/config.json", "[]");
        String ssJSON = "{\"name\": \"IoTmanager\",\"apssid\": \"IoTmanager\",\"appass\": \"\",\"routerssid\": \"Mikro\",\"routerpass\": \"4455667788\",\"timezone\": 3,\"ntp\": \"pool.ntp.org\",\"weblogin\": \"admin\",\"webpass\": \"admin\",\"mqttServer\": \"\",\"mqttPort\": 1883,\"mqttPrefix\": \"/demo\",\"mqttUser\": \"\",\"mqttPass\": \"\",\"serverip\": \"http://live-control.com\",\"serverlocal\": \"http://192.168.1.2:5500\",\"log\": 0,\"mqttin\": 0,\"pinSCL\": 0,\"pinSDA\": 0,\"i2cFreq\": 100000,\"wg\": \"group1\",\"debugTraceMsgTlgrm\": 0}";
        // jsonWriteStr(settingsFlashJson, "routerssid", "");
        writeFile("/settings.json", ssJSON);
        writeFile("/layout.json", "");
        writeFile("/scenario.txt", "");
        restartEsp();
    }

    void
    loop()
    {
        _reading = IoTgpio.digitalRead(_RESET_pin);

        if (_reading != _lastButtonState)
        {
            if ((millis() - _lastDebounceTime) > 100)
            {
                duration = millis() - _lastDebounceTime;
                SerialPrint("I", F("sysReset"), "duration " + String(duration / 1000));
                if (duration > 4000 && duration < 8000)
                {
                    SerialPrint("i", F("sysReset"), "RESET ESP");
                    SysReset();
                }
            }
            _lastDebounceTime = millis();
            _lastButtonState = _reading;
        }
        /*
        if (_reading != _lastButtonState)
        {
            _lastDebounceTime = millis();
            SerialPrint("i", F("sysReset"), "_RESET_pin: " + String(_lastDebounceTime));
        }

        if ((millis() - _lastDebounceTime) > _debounceDelay)
        {
            //  SerialPrint("i", F("sysReset"), "_RESET_pin: " + String((millis() - _lastDebounceTime)) + " " + String(_debounceDelay));
            if (_reading != _lastButtonState)
            {
                _count++;
                SerialPrint("i", F("sysReset"), "_count: " + String(_count));
                //_lastButtonState = _reading;
            }
            if (1 < _count && millis() > _lastDebounceTime + 2000)
            {
                SerialPrint("i", F("sysReset"), "_count " + String(_count));
                if (_count == 6)
                {
                    SerialPrint("i", F("sysReset"), "sysReset");
                    SysReset();
                }
                //  value.valD = _count / 2;
                //  regEvent(value.valD, F("sysReset"));
                _count = 0;
            }
        }
        _lastButtonState = _reading;
        */

        IoTItem::loop();
    }
    ~sysReset() {};
};

void *getAPI_sysReset(String subtype, String param)
{
    if (subtype == F("sysReset"))
    {
        return new sysReset(param);
    }
    else
    {
        return nullptr;
    }
}