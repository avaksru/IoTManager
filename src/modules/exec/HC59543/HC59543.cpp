#include "Global.h"
#include "classes/IoTItem.h"
#include "classes/IoTGpio.h"

// Класс драйвера для 74HC595 (сдвиговый регистр Serial-In Parallel-Out)
class Hc59543Driver : public IoTGpio {
   private:
    uint8_t* _state;       // Буфер состояния для каждого чипа (бит на пин)
    int _numChips;         // Количество каскадированных микросхем (всего пинов = _numChips * 8)
    
    uint8_t _pinDS;        // Data Serial (DS) - пин данных
    uint8_t _pinSHCP;      // Shift Register Clock (SH_CP) - пин тактов сдвига
    uint8_t _pinSTCP;      // Storage / Latch Clock (ST_CP) - пин защёлки

    uint8_t _pinOE;         // Output Enable (OE) - пин разрешения выхода (active LOW), 0 = не используется

    // Сдвигает данные во все регистры
    void writeState() {
        // Выставляем данные, начиная с последнего чипа в каскаде
        for (int i = _numChips - 1; i >= 0; i--) {
            shiftOut(_pinDS, _pinSHCP, MSBFIRST, _state[i]);
        }
        // Сигнал защёлки — данные появляются на выходах
        ::digitalWrite(_pinSTCP, HIGH);
        delayMicroseconds(5);
        ::digitalWrite(_pinSTCP, LOW);
    }

   public:
    Hc59543Driver(int index, uint8_t pinDS, uint8_t pinSHCP, uint8_t pinSTCP, int numChips, uint8_t pinOE = 0) 
        : IoTGpio(index) {
        _pinDS = pinDS;
        _pinSHCP = pinSHCP;
        _pinSTCP = pinSTCP;
        _pinOE = pinOE;
        _numChips = numChips;

        if (_numChips < 1) _numChips = 1;
        if (_numChips > 4) _numChips = 4;  // Ограничим каскад 4 чипами (32 выхода)

        // Выделяем память под состояние
        _state = new uint8_t[_numChips];
        for (int i = 0; i < _numChips; i++) {
            _state[i] = 0x00;  // Все выходы в LOW
        }

        // Настраиваем пины управления (:: - глобальная Arduino функция, не виртуальная IoTGpio)
        ::pinMode(_pinDS, OUTPUT);
        ::pinMode(_pinSHCP, OUTPUT);
        ::pinMode(_pinSTCP, OUTPUT);

        // Устанавливаем начальные состояния
        ::digitalWrite(_pinDS, LOW);
        ::digitalWrite(_pinSHCP, LOW);
        ::digitalWrite(_pinSTCP, LOW);

        // Если указан OE пин — настраиваем и включаем выходы (LOW = active)
        if (_pinOE > 0) {
            ::pinMode(_pinOE, OUTPUT);
            ::digitalWrite(_pinOE, LOW);  // Включаем выходы
        }

        // Инициализируем регистры нулями
        writeState();
    }

    void pinMode(int pin, uint8_t mode) {
        // 74HC595 — только выходы, игнорируем другие режимы
        if (mode != OUTPUT) {
            Serial.printf("HC59543: pin %d может быть только OUTPUT.\n", pin);
        }
    }

    void digitalWrite(int pin, uint8_t val) {
        int totalPins = _numChips * 8;
        if (pin < 0 || pin >= totalPins) {
            Serial.printf("HC59543: пин %d вне диапазона (0-%d).\n", pin, totalPins - 1);
            return;
        }

        int chipIndex = pin / 8;       // Номер чипа в каскаде
        int bitIndex = pin % 8;        // Номер пина внутри чипа

        if (val) {
            _state[chipIndex] |= (1 << bitIndex);
        } else {
            _state[chipIndex] &= ~(1 << bitIndex);
        }

        writeState();
    }

    int digitalRead(int pin) {
        int totalPins = _numChips * 8;
        if (pin < 0 || pin >= totalPins) {
            Serial.printf("HC59543: пин %d вне диапазона (0-%d).\n", pin, totalPins - 1);
            return LOW;
        }

        int chipIndex = pin / 8;
        int bitIndex = pin % 8;

        // Читаем из внутреннего буфера (74HC595 не поддерживает чтение)
        return (_state[chipIndex] >> bitIndex) & 0x01;
    }

    void digitalInvert(int pin) {
        int totalPins = _numChips * 8;
        if (pin < 0 || pin >= totalPins) {
            Serial.printf("HC59543: пин %d вне диапазона (0-%d).\n", pin, totalPins - 1);
            return;
        }

        int chipIndex = pin / 8;
        int bitIndex = pin % 8;

        _state[chipIndex] ^= (1 << bitIndex);  // Инвертируем бит

        writeState();
    }

    ~Hc59543Driver() {
        if (_state) {
            delete[] _state;
            _state = nullptr;
        }
    }
};


class HC59543 : public IoTItem {
   private:
    Hc59543Driver* _driver;

   public:
    HC59543(String parameters) : IoTItem(parameters) {
        _driver = nullptr;

        // Параметры пинов
        int dataPin = -1, clockPin = -1, latchPin = -1;
        jsonRead(parameters, "data", dataPin);
        jsonRead(parameters, "clock", clockPin);
        jsonRead(parameters, "latch", latchPin);

        if (dataPin < 0 || clockPin < 0 || latchPin < 0) {
            Serial.println("HC59543: необходимо указать пины data, clock и latch.");
            return;
        }

        int index = 0;
        jsonRead(parameters, "index", index);
        if (index < 1 || index > 4) {
            Serial.printf("HC59543: неправильный индекс (%d). Должен быть 1 - 4.\n", index);
            return;
        }

        int numChips = 1;
        jsonRead(parameters, "chips", numChips);
        if (numChips < 1) numChips = 1;
        if (numChips > 4) {
            Serial.println("HC59543: количество чипов не может превышать 4.");
            return;
        }

        int oePin = 0;
        jsonRead(parameters, "oepin", oePin);

        _driver = new Hc59543Driver(index, dataPin, clockPin, latchPin, numChips, oePin);
    }

    void doByInterval() {
        // Ничего не делаем по таймеру — драйвер работает по запросу
    }

    IoTGpio* getGpioDriver() {
        return _driver;
    }

    ~HC59543() {
        delete _driver;
    }
};

void* getAPI_HC59543(String subtype, String param) {
    if (subtype == F("HC59543")) {
        return new HC59543(param);
    } else {
        return nullptr;
    }
}
