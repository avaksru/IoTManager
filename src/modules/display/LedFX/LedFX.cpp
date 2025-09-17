#include "Global.h"
#include "classes/IoTItem.h"
#include "ESPConfiguration.h"
#include <WS2812FX.h>

WS2812FX *_glob_strip = nullptr;      // глобальный указатель на WS2812FX для использования в функциях для кастомных эффектов
std::vector<IoTValue *> vuMeterBands; // массив указателей на элементы IoTValue, которые будут использоваться для передачи данных в эффект VU Meter

uint16_t vuMeter2(void)
{
    // функция взята из WS2812FX.cpp для демонстрации возможности создания своего алгоритма эффекта
    // в данном случае - VU Meter имеет тот же смысл отображения уровня сигнала, что и в WS2812FX с сохранением алгоритма вывода данных из нескольких источников
    // но вместо использования внешнего источника данных для встроенного эффекта, мы используем данные из других элементов конфигурации IoTM
    // Если данные не поступают (IoTValue.valS == "1"), то используется генерация случайных чисел для демонстрации работы эффекта

    if (_glob_strip == nullptr)
        return 0; // Проверяем, инициализирована ли библиотека WS2812FX

    uint16_t numBands = vuMeterBands.size(); // Получаем количество полос VU Meter из массива указателей
    if (numBands == 0)
        return 0; // Если нет полос, выходим из функции

    WS2812FX::Segment *seg = _glob_strip->getSegment();
    uint16_t seglen = seg->stop - seg->start + 1;
    uint16_t bandSize = seglen / numBands;

    if (vuMeterBands[0]->valS == "R")
        for (uint8_t i = 0; i < numBands; i++)
        {
            int randomData = vuMeterBands[i]->valD + _glob_strip->random8(32) - _glob_strip->random8(32);
            vuMeterBands[i]->valD = (randomData < 0 || randomData > 255) ? 128 : randomData;
        }

    for (uint8_t i = 0; i < numBands; i++)
    {
        // SerialPrint("i", "LedFX", String(vuMeterBands[i]->valD));
        uint8_t scaledBand = (vuMeterBands[i]->valD * bandSize) / 256;
        for (uint16_t j = 0; j < bandSize; j++)
        {
            uint16_t index = seg->start + (i * bandSize) + j;
            if (j <= scaledBand)
            {
                if (j < bandSize - 4)
                    _glob_strip->setPixelColor(index, GREEN);
                else if (j < bandSize - 2)
                    _glob_strip->setPixelColor(index, YELLOW);
                else
                    _glob_strip->setPixelColor(index, RED);
            }
            else
            {
                _glob_strip->setPixelColor(index, BLACK);
            }
        }
    }
    _glob_strip->setCycle();

    return seg->speed;
}

class LedFX : public IoTItem
{
private:
    WS2812FX *_strip;

    int _data_pin = 2;
    int _numLeds = 1;
    int _brightness = 100;
    int _speed = 15000;
    int _effectsMode = 0;
    int _valueMode = 0;
    int _color = 0xFF0000; // default color red

    uint8_t _BrightnessFadeOutStep = 0;
    uint8_t _BrightnessFadeOutMin = 1;
    uint8_t _BrightnessFadeInStep = 0;
    uint8_t _BrightnessFadeInMax = 50;

public:
    LedFX(String parameters) : IoTItem(parameters)
    {
        jsonRead(parameters, F("data_pin"), _data_pin);
        jsonRead(parameters, F("speed"), _speed);
        jsonRead(parameters, F("numLeds"), _numLeds);
        jsonRead(parameters, F("brightness"), _brightness);

        String tmpStr;
        jsonRead(parameters, F("color"), tmpStr);
        _color = hexStringToUint32(tmpStr);

        jsonRead(parameters, F("effectsMode"), _effectsMode);
        jsonRead(parameters, F("valueMode"), _valueMode);

        //_strip = new WS2812FX(_numLeds, _data_pin, NEO_BRG + NEO_KHZ400);  // SM16703
        _strip = new WS2812FX(_numLeds, _data_pin, NEO_GRB + NEO_KHZ800); // WS2812B

        if (_strip != nullptr)
        {
            _glob_strip = _strip; // Сохраняем указатель в глобальной переменной

            _strip->init();
            _strip->setBrightness(_brightness);
            _strip->setSpeed(_speed);
            _strip->setMode(_effectsMode);
            _strip->setColor(_color);
            if (_effectsMode >= 0)
                _strip->start();
        }
    }

    void fadeOutNonBlocking()
    {
    }

    void loop()
    {
        if (!_strip)
            return;

        static unsigned long lastUpdate = 0; // Время последнего обновления
        unsigned long now = millis();
        if (now - lastUpdate >= 70)
        { // Проверяем, прошло ли достаточно времени
            lastUpdate = now;

            if (_BrightnessFadeOutStep > 0)
            {
                int currentBrightness = _strip->getBrightness(); // Получаем текущую яркость
                currentBrightness -= _BrightnessFadeOutStep;
                if (currentBrightness < _BrightnessFadeOutMin)
                {
                    currentBrightness = _BrightnessFadeOutMin; // Убедимся, что яркость не уйдет в отрицательные значения
                    _BrightnessFadeOutStep = 0;                // Останавливаем затухание
                }
                _strip->setBrightness(currentBrightness);
                _strip->show();
            }

            if (_BrightnessFadeInStep > 0)
            {
                int currentBrightness = _strip->getBrightness(); // Получаем текущую яркость
                currentBrightness += _BrightnessFadeInStep;
                if (currentBrightness > _BrightnessFadeInMax)
                {
                    currentBrightness = _BrightnessFadeInMax; // Убедимся, что яркость не уйдет за пределы
                    _BrightnessFadeInStep = 0;                // Останавливаем затухание
                }
                _strip->setBrightness(currentBrightness);
                _strip->show();
            }
        }

        _strip->service();
        IoTItem::loop();
    }

    void doByInterval()
    {
    }

    IoTValue execute(String command, std::vector<IoTValue> &param)
    {
        if (!_strip)
            return {};
        if (command == "fadeOut")
        {
            if (param.size() == 2)
            {
                _BrightnessFadeOutMin = param[0].valD;
                _BrightnessFadeOutStep = param[1].valD;
                SerialPrint("E", "Strip LedFX", "BrightnessFadeOut");
            }
        }
        else if (command == "fadeIn")
        {
            if (param.size() == 2)
            {
                _BrightnessFadeInMax = param[0].valD;
                _BrightnessFadeInStep = param[1].valD;
                SerialPrint("E", "Strip LedFX", "BrightnessFadeIn");
            }
        }
        else if (command == "setColor")
        {
            if (param.size() == 1)
            {
                _color = hexStringToUint32(param[0].valS);
                _strip->setColor(_color);
                _strip->show();
                SerialPrint("E", "Strip LedFX", "setColor:" + param[0].valS);
            }
            else if (param.size() == 3)
            {
                uint8_t _r = (param[0].valD);
                uint8_t _g = (param[1].valD);
                uint8_t _b = (param[2].valD);
                _strip->setColor(_r, _g, _b);
                _strip->show();
                
            }
        }
        else if (command == "setEffect")
        {
            if (param.size() == 1)
            {
                if (param[0].valD < 0 | param[0].valD > 79)
                    _effectsMode = random(0, 79);
                else
                    _effectsMode = param[0].valD;
                _strip->setMode(_effectsMode);
                _strip->show();
                _strip->start();
                SerialPrint("E", "Strip LedFX", "setEffect:" + param[0].valS);
            }
        }
        else if (command == "setSpeed")
        {
            if (param.size() == 1)
            {
                _speed = param[0].valD;
                _strip->setSpeed(_speed);
                _strip->show();
                SerialPrint("E", "Strip LedFX", "setSpeed:" + param[0].valS);
            }
        }
        else if (command == "setBrightness")
        {
            if (param.size() == 1)
            {
                _brightness = param[0].valD;
                _strip->setBrightness(_brightness);
                _strip->show();
                SerialPrint("E", "Strip LedFX", "setBrightness:" + param[0].valS);
            }
        }
        else if (command == "stop")
        {
            _strip->stop();
            SerialPrint("E", "Strip LedFX", "stop");
        }
        else if (command == "start")
        {
            _strip->start();
            SerialPrint("E", "Strip LedFX", "start");
        }
        else if (command == "pause")
        {
            _strip->pause();
            SerialPrint("E", "Strip LedFX", "pause");
        }
        else if (command == "resume")
        {
            _strip->resume();
            SerialPrint("E", "Strip LedFX", "resume");
        }
        else if (command == "setSegment")
        {
            if (param.size() == 6)
            {
                _strip->setSegment(param[0].valD, param[1].valD, param[2].valD, param[3].valD, hexStringToUint32(param[4].valS), param[5].valD);
                _strip->show();
                _strip->start();
                SerialPrint("E", "Strip LedFX", "setSegment:" + param[0].valS + " start:" + param[1].valS + " stop:" + param[2].valS + " mode:" + param[3].valS, " color:" + param[4].valS + " speed:" + param[5].valS);
            }
        }
        else if (command == "noShowOne")
        {
            if (param.size() == 1)
            {
                _strip->setPixelColor(param[0].valD, _strip->Color(0, 0, 0));
                _strip->show();
                SerialPrint("E", "Strip LedFX", "noShowOne");
            }
        }
        else if (command == "showLed")
        {
            if (param.size() == 2)
            {
                uint32_t color = hexStringToUint32(param[1].valS);
                _strip->setPixelColor(param[0].valD, color);
                _strip->show();
                _strip->start();
                SerialPrint("E", "Strip LedFX", "showLed:" + param[0].valS + " color:" + param[1].valS);
            }
        }
        else if (command == "vuMeter")
        {
            if (param.size() == 2)
            {
                int bandCnt = param[0].valD;
                if (param[1].valS == "")
                {
                    for (int i = 0; i < vuMeterBands.size(); i++)
                    {
                        delete vuMeterBands[i];
                    }
                    vuMeterBands.clear();

                    for (uint8_t i = 0; i < bandCnt; i++)
                    {
                        IoTValue *band = new IoTValue(); // создаем новый элемент IoTValue для полос VU Meter
                        band->valD = 0;
                        band->valS = "R";
                        vuMeterBands.push_back(band); // добавляем указатель в массив
                    }
                }
                else
                {
                    // Очищаем массив vuMeterBands перед заполнением
                    vuMeterBands.clear();

                    String id;
                    String idsStr = param[1].valS;
                    // Разделяем строку idsStr на идентификаторы, используя запятую как разделитель
                    while (idsStr.length() > 0)
                    {
                        // Извлекаем идентификатор до первой запятой
                        id = selectToMarker(idsStr, ",");

                        // Ищем элемент IoTItem по идентификатору
                        IoTItem *item = findIoTItem(id);
                        if (item != nullptr)
                        {
                            // Добавляем указатель на поле value найденного элемента в vuMeterBands
                            vuMeterBands.push_back(&(item->value));
                            SerialPrint("E", "LedFX", "Добавлен элемент в vuMeterBands: " + id);
                        }
                        else
                        {
                            SerialPrint("E", "LedFX", "Элемент не найден: " + id);
                        }

                        int8_t oldSize = idsStr.length();
                        // Удаляем обработанный идентификатор из строки
                        idsStr = deleteBeforeDelimiter(idsStr, ",");
                        if (idsStr.length() == oldSize)
                        {
                            // Если длина строки не изменилась, значит, больше нет запятых и это был последний идентификатор
                            break;
                        }
                    }
                }
                _strip->setCustomMode(vuMeter2);
                _strip->setMode(FX_MODE_CUSTOM);
                _strip->start();
                SerialPrint("E", "Strip LedFX", "vuMeter bands:" + param[0].valS + " IDs to show:" + param[1].valS);
            }
        }

        return {};
    }

    void setValue(const IoTValue &Value, bool genEvent = true)
    {
        if (!_strip)
            return;

        if (_valueMode == 0)
        {
            _strip->setMode(Value.valD);
            _effectsMode = Value.valD;
        }
        else if (_valueMode == 1)
        {
            _strip->setBrightness(Value.valD);
            _brightness = Value.valD;
        }
        else if (_valueMode == 2)
        {
            _color = hexStringToUint32(Value.valS);
            _strip->setColor(_color);
        }
        else if (_valueMode == 3)
        {
            _strip->setSpeed(Value.valD);
            _speed = Value.valD;
        }

        value = Value;
        regEvent(value.valD, "LedFX", false, genEvent);
    }

    ~LedFX()
    {
        if (_strip != nullptr)
        {
            delete _strip;
            _strip = nullptr;
            _glob_strip = nullptr; // Обнуляем глобальный указатель
        }
    };
};

void *getAPI_LedFX(String subtype, String param)
{
    if (subtype == F("LedFX"))
    {
        return new LedFX(param);
    }
    else
    {
        return nullptr;
    }
}
