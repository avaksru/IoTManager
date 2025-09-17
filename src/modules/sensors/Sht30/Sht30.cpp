/******************************************************************
  Simple library for the WEMOS SHT30 Shield.
  
  https://github.com/wemos/WEMOS_SHT3x_Arduino_Library

  adapted for version 4 @Serghei63
 ******************************************************************/

#include "Global.h"
#include "classes/IoTItem.h"


#include "Wire.h"
#include <WEMOS_SHT3X.h>

SHT3X sht30(0x44);

class Sht30t : public IoTItem {

    private:
    uint8_t _addr = 0;

   public:
    Sht30t(String parameters): IoTItem(parameters) {
    {
        String sAddr;
        jsonRead(parameters, "addr", sAddr);
        if (sAddr == "")
            scanI2C();
        else
            _addr = hexStringToUint8(sAddr);
    }

     }
    
    void doByInterval() {
        if(sht30.get()==0){
        value.valD = sht30.cTemp;

        SerialPrint("i", "Sensor Sht30t", "OK");

        if (value.valD > -46.85F) regEvent(value.valD, "Sht30t");     // TODO: найти способ понимания ошибки получения данных
            else SerialPrint("E", "Sensor Sht30t", "Error", _id);  
        }
    }
    ~Sht30t() {};
};

class Sht30h : public IoTItem {

    private:
    uint8_t _addr = 0;

   public:
    Sht30h(String parameters): IoTItem(parameters) {
            {
        String sAddr;
        jsonRead(parameters, "addr", sAddr);
        if (sAddr == "")
            scanI2C();
        else
            _addr = hexStringToUint8(sAddr);
    }
     }
    
    void doByInterval() {
        if(sht30.get()==0){
        value.valD = sht30.humidity;

        SerialPrint("i", "Sensor Sht30h", "OK");
        if (value.valD != -6) regEvent(value.valD, "Sht30h");    // TODO: найти способ понимания ошибки получения данных
            else SerialPrint("E", "Sensor Sht30h", "Error", _id);
        }   
    }
    ~Sht30h() {};
};


void* getAPI_Sht30(String subtype, String param) {
    if (subtype == F("Sht30t")){
        return new Sht30t(param);
        }
        if (subtype == F("Sht30h")) {
            return new Sht30h(param);
    } else {
        return nullptr;
    }
}
