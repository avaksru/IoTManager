#include "Global.h"
#include "classes/IoTItem.h"
#include <Wire.h>

typedef unsigned short int WORD;
typedef unsigned char BYTE;
typedef unsigned long int DWORD;

class JHM1200Base : public IoTItem
{
protected:
    uint8_t _sdaPin;
    uint8_t _sclPin;
    uint8_t _i2cAddr;
    bool _initialized;
    unsigned long _lastReadTime;

    void initI2C()
    {
        Wire.end();
        delay(10);
        Wire.begin(_sdaPin, _sclPin);
        Wire.setClock(40000); // Пониженная частота для надежности
        delay(50);

        Wire.beginTransmission(_i2cAddr);
        _initialized = (Wire.endTransmission() == 0);

        if (_initialized)
        {
            SerialPrint("i", F(_id.c_str()), "I2C initialized on SDA: " + String(_sdaPin) + ", SCL:" + String(_sclPin));
        }
        else
        {

            SerialPrint("E", F(_id.c_str()), "I2C init failed on SDA: " + String(_sdaPin) + ", SCL:" + String(_sclPin));
        }
    }

    bool readSensorData(BYTE *buffer)
    {
        // Освобождаем и переинициализируем шину перед каждым опросом
        // initI2C();
        if (!_initialized)
            return false;

        // Защита от слишком частых опросов
        if (millis() - _lastReadTime < 500)
        {
            delay(500 - (millis() - _lastReadTime));
        }

        // 1. Отправка команды измерения
        Wire.beginTransmission(_i2cAddr);
        Wire.write(0xAC);
        if (Wire.endTransmission() != 0)
        {

            SerialPrint("E", F(_id.c_str()), "Command send failed");
            return false;
        }

        // 2. Ожидание готовности (увеличенное время)
        delay(100);

        // 3. Чтение данных с проверкой
        if (Wire.requestFrom(_i2cAddr, 6) != 6)
        {
            SerialPrint("E", F(_id.c_str()), "Read failed");
            return false;
        }

        for (int i = 0; i < 6; i++)
        {
            buffer[i] = Wire.read();
        }

        _lastReadTime = millis();
        return true;
    }

public:
    JHM1200Base(String parameters) : IoTItem(parameters), _initialized(false)
    {
        _sdaPin = jsonReadInt(parameters, "sdaPin");
        _sclPin = jsonReadInt(parameters, "sclPin");

        String addrStr = jsonReadStr(parameters, "addr");
        _i2cAddr = addrStr.startsWith("0x") ? strtol(addrStr.c_str(), NULL, 16) : addrStr.toInt();

        //  initI2C();
    }
    void loop() override
    {
        IoTItem::loop();
    }
};

class JHM1200p : public JHM1200Base
{
private:
    int _PRES_MIN;
    int _PRES_MAX;
    float _lastGoodValue;
    float _filteredValue = 0;
    int _raw_corection = 16777216;
    double press = 0;

public:
    JHM1200p(String parameters) : JHM1200Base(parameters), _lastGoodValue(0)
    {
        _PRES_MIN = jsonReadInt(parameters, "PRES_MIN");
        _PRES_MAX = jsonReadInt(parameters, "PRES_MAX");
        _raw_corection = jsonReadInt(parameters, "raw_corection");
        initI2C();
    }

    void doByInterval() override
    {
        BYTE buffer[6] = {0};
        if (readSensorData(buffer))
        {
            DWORD press_raw = ((DWORD)buffer[1] << 16) | ((WORD)buffer[2] << 8) | buffer[3];
            if (_raw_corection)
            {
                press = (double)press_raw / _raw_corection;
            }
            else
            {
                press = (double)press_raw / 16777216;
            }
            float curent_press = press * (_PRES_MAX - _PRES_MIN) + _PRES_MIN;

            // Экспоненциальная фильтрация (коэффициент 0.6)
            //            if (_filteredValue == 0)
            //                _filteredValue = _lastGoodValue;
            //            _filteredValue = 0.6 * _filteredValue + 0.4 * _lastGoodValue;
            //            _lastGoodValue = _filteredValue;
            if (curent_press - _lastGoodValue > 0.5 || curent_press - _lastGoodValue < 0.5)
            {
                value.valD = _lastGoodValue;
            }
            else
            {
                value.valD = curent_press;
            }
            _lastGoodValue = curent_press;

            SerialPrint("i", F(_id.c_str()), "Pressure: " + String(_lastGoodValue) + ", (raw: " + String(press_raw) + ")");
        }
        else
        {
            value.valD = _lastGoodValue;

            SerialPrint("E", F(_id.c_str()), "Using last good value : " + String(_lastGoodValue));
        }

        regEvent(value.valD, "JHM1200p");
    }
    ~JHM1200p() {};
};

class JHM1200t : public JHM1200Base
{
private:
    int _TEMP_MIN;
    int _TEMP_MAX;
    float _lastGoodValue;
    float _filteredValue; // Для фильтрации показаний
    int _raw_corection = 65536;

    bool isValidTemperature(WORD raw)
    {
        // Проверка на недопустимые значения
        return raw != 0 && raw != 0xFFFF;
    }

public:
    JHM1200t(String parameters) : JHM1200Base(parameters),
                                  _lastGoodValue(0), // Начальное значение 25°C
                                  _filteredValue(0)
    {
        _TEMP_MIN = jsonReadInt(parameters, "TEMP_MIN");
        _TEMP_MAX = jsonReadInt(parameters, "TEMP_MAX");
        _raw_corection = jsonReadInt(parameters, "raw_corection");

        SerialPrint("i", F(_id.c_str()), "Temperature sensor initialized ( " + String(_TEMP_MIN) + ".." + String(_TEMP_MAX) + ")");
    }

    void doByInterval() override
    {
        BYTE buffer[6] = {0};
        if (readSensorData(buffer))
        {
            WORD temp_raw = ((WORD)buffer[4] << 8) | buffer[5];

            if (isValidTemperature(temp_raw))
            {
                double temp = (double)temp_raw / _raw_corection;
                float temperature = (1 - temp) * (_TEMP_MAX - _TEMP_MIN) + _TEMP_MIN;

                // Экспоненциальная фильтрация (коэффициент 0.6)
                _filteredValue = 0.6 * _filteredValue + 0.4 * temperature;
                _lastGoodValue = _filteredValue;

                if (temperature - _filteredValue < 2)
                {
                    value.valD = _filteredValue;
                }
                else
                {
                    value.valD = temperature;
                }

                SerialPrint("i", F(_id.c_str()), "Temperature: " + String(_filteredValue) + "°C (raw: " + String(temp_raw) + ")");
            }
            else
            {
                value.valD = _lastGoodValue;

                SerialPrint("i", F(_id.c_str()), "Invalid raw value: " + String(temp_raw) + " using " + String(_lastGoodValue) + "°C");
            }
        }
        else
        {
            value.valD = _lastGoodValue;
            SerialPrint("i", F(_id.c_str()), "Read error, using last value: " + String(_lastGoodValue) + "°C");
        }

        regEvent(value.valD, "JHM1200t");
    }
    ~JHM1200t() {};
};

void *getAPI_JHM1200(String subtype, String param)
{
    if (subtype == F("JHM1200t") || subtype == F("JHM1200p"))
    {
        String addr;
        jsonRead(param, "addr", addr);
        if (addr == "")
        {
            scanI2C();
            return nullptr;
        }

        if (subtype == F("JHM1200t"))
        {
            return new JHM1200t(param);
        }
        else if (subtype == F("JHM1200p"))
        {
            return new JHM1200p(param);
        }
    }
    return nullptr;
}