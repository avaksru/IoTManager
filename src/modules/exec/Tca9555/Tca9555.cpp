#include "Global.h"
#include "classes/IoTItem.h"
#include "classes/IoTGpio.h"
#include "Wire.h"
#include "TCA9555.h"


class Tca9555Driver : public IoTGpio {
   private:
    TCA9555* _tca;

   public:
    Tca9555Driver(int index, TCA9555* tca) : IoTGpio(index) {
        _tca = tca;
    }

    void pinMode(int pin, uint8_t mode) {
        _tca->pinMode1(pin, mode);
    }

    void digitalWrite(int pin, uint8_t val) {
        _tca->write1(pin, val);
    }

    int digitalRead(int pin) {
        return _tca->read1(pin);
    }

    void digitalInvert(int pin) {
        _tca->write1(pin, 1 - _tca->read1(pin));
    }

    ~Tca9555Driver() {};
};


class Tca9555 : public IoTItem {
   private:
    String _addr;
    Tca9555Driver* _driver;

   public:
    Tca9555(String parameters) : IoTItem(parameters) {
        _driver = nullptr;
        Wire.begin();

        jsonRead(parameters, "addr", _addr);
        if (_addr == "") {
            scanI2C();
            return;
        }

        int index;
        jsonRead(parameters, "index", index);
        if (index > 4) {
            Serial.println(F("TCA9555 wrong index. Must be 0 - 4"));
            return;
        }

        int mktype;
        TCA9555* tca;
        jsonRead(parameters, "mktype", mktype);
        if (mktype == 35) {
            tca = new TCA9535(hexStringToUint8(_addr));
        } else if (mktype == 55) {
            tca = new TCA9555(hexStringToUint8(_addr));
        } else {
            Serial.println(F("TCA9555 wrong type. Must be 35 or 55"));
            return;     
        }

        _driver = new Tca9555Driver(index, tca);
    }

    void doByInterval() {
        if (_addr == "") {
            scanI2C();
            return;
        }
    }

    //возвращает ссылку на экземпляр класса Tca9555Driver
    IoTGpio* getGpioDriver() {
        return _driver;
    }

    ~Tca9555() {
        delete _driver;
    };
};

void* getAPI_Tca9555(String subtype, String param) {
    if (subtype == F("Tca9555")) {
        return new Tca9555(param);
    } else {
        return nullptr;
    }
}
