#include "Global.h"
#include "classes/IoTItem.h"
#include "JHM1200_I2C.h"

extern IoTGpio IoTgpio;

class JHM1200_Base : public IoTItem
{
protected:
    JHM1200 sensor;
    int _sdaPin;
    int _sclPin;
    long _i2cSpeed;
    String _addr;
    uint8_t _deviceAddress = 0x78;
    uint32_t ZERO_OFFSET = 0; // Значение при нулевом давлении
    int _max_val;

    int _rawData;
    static int _maxAge;

    // Статические переменные для обмена данными между экземплярами
    static double _lastTemp;
    static double _lastTempRaw;
    static unsigned long _lastMeasurementTime;

    bool initSensor()
    {
        if (_sdaPin != -1 && _sclPin != -1)
        {
            if (!sensor.begin(_sdaPin, _sclPin, _i2cSpeed))
            {
                SerialPrint("E", F(_id.c_str()), "Failed to initialize with custom pins");
                return false;
            }
        }
        else
        {
            if (!sensor.begin(_i2cSpeed))
            {
                SerialPrint("E", F(_id.c_str()), "Failed to initialize with default pins");
                return false;
            }
        }
        return true;
    }

public:
    JHM1200_Base(String parameters) : IoTItem(parameters)
    {
        int interval;
        if (jsonRead(parameters, "int", interval))
        {
            _maxAge = interval * 1100;
        }
        else
        {
            _maxAge = 600000; // Значение по умолчанию, если параметр не указан
        }
        _rawData = jsonReadInt(parameters, "rawData");
        _i2cSpeed = jsonReadInt(parameters, "i2cSpeed");

        ZERO_OFFSET = jsonReadInt(parameters, "ZERO_OFFSET");
        _max_val = jsonReadInt(parameters, "max_val");

        _sdaPin = jsonReadInt(parameters, "sdaPin");
        _sclPin = jsonReadInt(parameters, "sclPin");
        jsonRead(parameters, "addr", _addr);
        if (_addr == "")
        {
            scanI2C();
        }
        else
        {
            _deviceAddress = hexStringToUint8(_addr);
        }

        if (!initSensor())
        {
            while (1)
                ;
        }
    }

    // Метод для обновления температурных значений
    static void updateTemperatureValues(double temp, double tempRaw)
    {
        _lastTemp = temp;
        _lastTempRaw = tempRaw;
        _lastMeasurementTime = millis();
    }

    // Метод для получения температуры
    static double getTemperature(bool raw = false)
    {
        return raw ? _lastTempRaw : _lastTemp;
    }

    // Проверка актуальности данных
    static bool isDataFresh()
    {
        return (millis() - _lastMeasurementTime) < _maxAge;
    }
};

// Инициализация статических переменных
double JHM1200_Base::_lastTemp = 0;
double JHM1200_Base::_lastTempRaw = 0;
unsigned long JHM1200_Base::_lastMeasurementTime = 0;
int JHM1200_Base::_maxAge = 600000;

class JHM1200p : public JHM1200_Base
{
private:
    double pressRaw;
    double press;
    unsigned long prevmillis = 0;
    double sum = 0;
    int valid = 0;
    double last_pressure = 0;

public:
    JHM1200p(String parameters) : JHM1200_Base(parameters)
    {
        if (sensor.getPressure(&press, &pressRaw, &_lastTemp, &_lastTempRaw, _deviceAddress))
        {
            updateTemperatureValues(_lastTemp, _lastTempRaw);
            SerialPrint("i", F(_id.c_str()), "Pressure initialized. " + String(press) + " bar (" + String(pressRaw) + ") " + String(_lastTemp) + "°С (" + String(_lastTempRaw) + ") ");
        }
    }

    void doByInterval() override
    {
        double currentTemp, currentTempRaw;
        if (sensor.getPressure(&press, &pressRaw, &currentTemp, &currentTempRaw, _deviceAddress))
        {
            // SerialPrint("i", F(_id.c_str()), "Pressure: " + String(press) + " bar (" + String(pressRaw) + ") " + String(_lastTemp) + "°С (" + String(_lastTempRaw) + ") ");
            //  Обновляем значения температуры
            updateTemperatureValues(currentTemp, currentTempRaw);

            // Остальная логика обработки давления...
            const double MAX_RAW_DEVIATION = 400000.0;
            const double MAX_CALC_DEVIATION = 0.4;
            static double prevValues[3] = {0}; // Хранит 3 предыдущих значения + текущее = 4 значений
            static uint8_t idx = 0;            // Индекс для циклического заполнения

            if (_rawData == 1)
            {
                if (!pressRaw)
                    return;

                bool isDeviant = (last_pressure != 0) &&
                                 (fabs(last_pressure - pressRaw) > MAX_RAW_DEVIATION);

                if (!isDeviant)
                {
                    value.valD = pressRaw;
                    last_pressure = pressRaw;
                    for (int i = 0; i < 3; i++)
                    {
                        prevValues[i] = 0;
                    }
                }
                else
                {
                    SerialPrint("E", F(_id.c_str()), "Deviant: " + String(fabs(last_pressure - pressRaw)) + " last_pressure: " + String(last_pressure) + " pressRaw: " + String(pressRaw));
                    prevValues[idx] = pressRaw;
                    idx = (idx + 1) % 3;
                    bool bufferReady = true;
                    for (int i = 0; i < 3; i++)
                    {
                        if (prevValues[i] == 0)
                        {
                            bufferReady = false;
                            break;
                        }
                    }
                    if (bufferReady)
                    {
                        last_pressure = (last_pressure + prevValues[0] + prevValues[1] + prevValues[2]) / 4;
                        for (int i = 0; i < 3; i++)
                        {
                            prevValues[i] = 0;
                        }
                    }

                    value.valD = last_pressure;
                }
            }
            else
            {

                // Вычитаем смещение нуля и защищаемся от отрицательных значений
                unsigned long corrected_raw = (pressRaw > ZERO_OFFSET) ? (pressRaw - ZERO_OFFSET) : 0;
                double press = (double)corrected_raw / 16777216;
                float curent_press = press * _max_val;
                if (!curent_press)
                    return;

                bool isDeviant = (last_pressure != 0) &&
                                 (fabs(last_pressure - curent_press) > MAX_CALC_DEVIATION);

                if (!isDeviant)
                {
                    last_pressure = curent_press;
                    value.valD = last_pressure;
                    for (int i = 0; i < 3; i++)
                    {
                        prevValues[i] = 0;
                    }
                }
                else
                {
                    SerialPrint("E", F(_id.c_str()), "Deviant: " + String(fabs(last_pressure - curent_press)) + " last_pressure: " + String(last_pressure) + " curent_press: " + String(curent_press));

                    prevValues[idx] = curent_press;
                    idx = (idx + 1) % 3;
                    bool bufferReady = true;
                    for (int i = 0; i < 3; i++)
                    {
                        if (prevValues[i] == 0)
                        {
                            bufferReady = false;
                            break;
                        }
                    }
                    if (bufferReady)
                    {
                        last_pressure = (last_pressure + prevValues[0] + prevValues[1] + prevValues[2]) / 4;
                        for (int i = 0; i < 3; i++)
                        {
                            prevValues[i] = 0;
                        }
                    }
                    value.valD = last_pressure;
                    // const double ALPHA = 0.2;
                    // value.valD = (last_pressure * (1 - ALPHA) + curent_press * ALPHA);
                    // last_pressure = curent_press;
                }
            }

            regEvent(value.valD, "JHM1200p");
        }
    }
};

class JHM1200t : public JHM1200_Base
{
public:
    JHM1200t(String parameters) : JHM1200_Base(parameters)
    {
        SerialPrint("i", F(_id.c_str()), "Temp sensor initialized");
    }

    void doByInterval() override
    {
        if (isDataFresh())
        {
            value.valD = _rawData ? getTemperature(true) : getTemperature();
            regEvent(value.valD, "JHM1200t");
        }
        else
        {
            SerialPrint("E", F(_id.c_str()), "Temperature data is not fresh");
        }
    }
};

void *getAPI_JHM1200(String subtype, String param)
{
    if (subtype == F("JHM1200p"))
    {
        return new JHM1200p(param);
    }
    else if (subtype == F("JHM1200t"))
    {
        return new JHM1200t(param);
    }
    return nullptr;
}