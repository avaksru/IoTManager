#include "Global.h"
#include "classes/IoTItem.h"
#include "Arduino.h"
#include "esp32-hal-ledc.h"

extern IoTGpio IoTgpio;

class Pwm32 : public IoTItem
{
private:
    int _pin;
    int _freq;
    int _apin, _oldValue;
    bool _freezVal = true;
    int _ledChannel;
    int _resolution;

public:
    Pwm32(String parameters) : IoTItem(parameters)
    {
        _interval = _interval / 1000;

        jsonRead(parameters, "pin", _pin);
        jsonRead(parameters, "freq", _freq);
        jsonRead(parameters, "ledChannel", _ledChannel);
        jsonRead(parameters, "PWM_resolution", _resolution);

        pinMode(_pin, OUTPUT);

// Универсальная инициализация PWM для ESP32 и ESP32-C6
#if defined(ARDUINO_ARCH_ESP32) && !defined(esp32c6_4mb) && !defined(esp32c6_8mb)
        // Для классического ESP32
        ledcSetup(_ledChannel, _freq, _resolution);
        ledcAttachPin(_pin, _ledChannel);
#else
        // Для ESP32-C6
        ledcAttachChannel(_pin, _freq, _resolution, _ledChannel);
#endif

// Универсальная запись PWM
#if defined(ARDUINO_ARCH_ESP32) && !defined(esp32c6_4mb) && !defined(esp32c6_8mb)
        ledcWrite(_ledChannel, value.valD);
#else
        ledcWrite(_pin, value.valD);
#endif

        _resolution = pow(2, _resolution);

        jsonRead(parameters, "apin", _apin);
        if (_apin >= 0)
            IoTgpio.pinMode(_apin, INPUT);
    }

    void doByInterval()
    {
        if (_apin >= 0)
        {
            value.valD = map(IoTgpio.analogRead(_apin), 0, 4095, 0, _resolution);
            if (value.valD > _resolution - 6)
                value.valD = _resolution;
            else if (value.valD < 9)
                value.valD = 0;

            if (abs(_oldValue - value.valD) > 20)
            {
                _oldValue = value.valD;
#if defined(ARDUINO_ARCH_ESP32) && !defined(esp32c6_4mb) && !defined(esp32c6_8mb)
                ledcWrite(_ledChannel, value.valD);
#else
                ledcWrite(_pin, value.valD);
#endif
                _freezVal = false;
            }
            else if (!_freezVal)
            {
                regEvent(value.valD, "Pwm32");
                _freezVal = true;
            }
        }
    }

    void setValue(const IoTValue &Value, bool genEvent = true)
    {
        value = Value;
#if defined(ARDUINO_ARCH_ESP32) && !defined(esp32c6_4mb) && !defined(esp32c6_8mb)
        ledcWrite(_ledChannel, value.valD);
#else
        ledcWrite(_pin, value.valD);
#endif
        regEvent(value.valD, "Pwm32", false, genEvent);
    }

    ~Pwm32() {}
};

void *getAPI_Pwm32(String subtype, String param)
{
    if (subtype == F("Pwm32"))
    {
        return new Pwm32(param);
    }
    return nullptr;
}