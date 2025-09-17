#include "Global.h"
#include "classes/IoTItem.h"

extern IoTGpio IoTgpio;

class Impulse : public IoTItem
{
private:
    int _int;
    int _pin;
    bool _buttonState, _reading;
    bool _lastButtonState = LOW;
    unsigned long _lastDebounceTime = 0;
    int _debounceDelay = 50;
    int _timeORcount = 0;
    int _count = 0;
    int CNT = 0;
    unsigned long timing;

public:
    Impulse(String parameters) : IoTItem(parameters)
    {
        String _pinMode;
        jsonRead(parameters, F("pin"), _pin);
        jsonRead(parameters, F("pinMode"), _pinMode);
        jsonRead(parameters, F("debounceDelay"), _debounceDelay);
        jsonRead(parameters, F("count"), _count);
        jsonRead(parameters, F("timeORcount"), _timeORcount);
        jsonRead(parameters, "int", _int);
        _round = 0;

        if (_pinMode == F("INPUT"))
            IoTgpio.pinMode(_pin, INPUT);
        else if (_pinMode == F("INPUT_PULLUP"))
            IoTgpio.pinMode(_pin, INPUT_PULLUP);
        else if (_pinMode == F("INPUT_PULLDOWN"))
        {
            IoTgpio.pinMode(_pin, INPUT);
            IoTgpio.digitalWrite(_pin, LOW);
        }

        value.valD = _buttonState = IoTgpio.digitalRead(_pin);
        regEvent(_buttonState, "", false, false);
    }

    void loop()
    {
        _reading = IoTgpio.digitalRead(_pin);
        if (_reading != _lastButtonState)
        {

            _lastDebounceTime = millis();
        }

        if ((millis() - _lastDebounceTime) > _debounceDelay)
        {
            if (_reading != _buttonState)
            {
                _buttonState = _reading;
                CNT++;
            }
            if (CNT == 1)
            {
                timing = millis();
            }
            if (!_timeORcount)
            { // работаем по времени
                if (millis() - timing > _int * 1000 && CNT > 1)
                {
                    timing = millis();
                    value.valD = CNT;
                    regEvent(value.valD, F("Impulse"));
                    CNT = 0;
                }
            }
            else
            { // работаем по количеству импульсов
                if (_count && CNT == _count)
                {
                    value.valD = 1;
                    regEvent(value.valD, F("Impulse"));
                    CNT = 0;
                }
                
            }
        }

        _lastButtonState = _reading;
    }

    ~Impulse(){};
};

void *getAPI_Impulse(String subtype, String param)
{
    if (subtype == F("Impulse"))
    {
        return new Impulse(param);
    }
    else
    {
        return nullptr;
    }
}
