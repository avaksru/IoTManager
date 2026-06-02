#include "Global.h"
#include "classes/IoTItem.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#define BLE_PART1
#define BLE_PART2
#include <decoder.h>
#include <vector>

// Создаем переменную для хранения данных с датчиков bluetooth
// StaticJsonDocument<JSON_BUFFER_SIZE * 4> BLEbuffer;
// DynamicJsonDocument extBLEdata(JSON_BUFFER_SIZE * 4);
// JsonObject extBLEdata = BLEbuffer.to<JsonObject>();
class BleSens;
std::vector<BleSens *> BleSensArray;

// Защита от множественной инициализации BLE на ESP32-C6
static bool bleInitialized = false;
static SemaphoreHandle_t bleInitMutex = NULL;

class BleSens : public IoTItem
{
private:
  // описание параметров передаваемых из настроек датчика из веба
  String _MAC;
  String _sensor;
  int timeRecv = 0;
  int _minutesPassed = 0;
  String json = "{}";
  int orange = 0;
  int red = 0;
  int offline = 0;
  int _int;
  bool dataFromNode = false;

public:
  String whoIAm(/*String &mac, String &sens*/)
  {
    // mac = _MAC;
    // sens = _sensor;
    return _MAC;
  }

  void setBLEdata(JsonObject extBLEdata)
  {
    if (_sensor == "last")
    {
      timeRecv = extBLEdata[_sensor].as<int>();
      char *s;
      s = TimeToString(millis() / 1000 - timeRecv / 1000);
      value.isDecimal = 0;
      if (timeRecv > 0)
      {
        value.valS = s;
        dataFromNode = true;
        _minutesPassed = 0;
        setNewWidgetAttributes();
      }
      else
      {
        value.valS = "";
      }
       regEvent(String(value.valS.c_str()), String(_id.c_str()));
    }
    else
    {
      String valStr = extBLEdata[_sensor].as<String>();
      if (valStr != "null")
      {
        if (value.isDecimal == isDigitDotCommaStr(valStr))
        {
          value.isDecimal = 1;
          value.valD = valStr.toFloat();
          regEvent(value.valD, String(_id.c_str()));
          dataFromNode = true;
          _minutesPassed = 0;
          setNewWidgetAttributes();
        }
        else
        {
          value.isDecimal = 0;
          value.valS = valStr.c_str();
          regEvent(String(value.valS.c_str()), String(_id.c_str()));
          dataFromNode = true;
          _minutesPassed = 0;
          setNewWidgetAttributes();
        }
      }
    }
  }
  char *TimeToString(unsigned long t)
  {
    static char str[12];
    long h = t / 3600;
    t = t % 3600;
    int m = t / 60;
    int s = t % 60;
    sprintf(str, "%02ld:%02d:%02d", h, m, s);
    return str;
  }

  void doByInterval()
  {
    if (_sensor == "last")
    {
      char *s;
      s = TimeToString(millis() / 1000 - timeRecv / 1000);
      value.isDecimal = 0;
      if (timeRecv > 0)
      {
        value.valS = s;
      }
      else
      {
        value.valS = "";
      }
       regEvent(String(value.valS.c_str()), String(_id.c_str()));
    }
    _minutesPassed++;
    setNewWidgetAttributes();
  }
  void onMqttWsAppConnectEvent()
  {
    setNewWidgetAttributes();
  }
  void setNewWidgetAttributes()
  {

    int minutes_ = _minutesPassed * _int / 60;
    jsonWriteStr(json, F("info"), prettyMinutsTimeout(minutes_));
    if (dataFromNode)
    {
      if (orange != 0 && red != 0 && offline != 0)
      {
        if (minutes_ < orange)
        {
          jsonWriteStr(json, F("color"), "");
        }
        if (minutes_ >= orange && minutes_ < red)
        {
          jsonWriteStr(json, F("color"), F("orange")); // сделаем виджет оранжевым
        }
        if (minutes_ >= red && minutes_ < offline)
        {
          jsonWriteStr(json, F("color"), F("red")); // сделаем виджет красным
        }
        if (minutes_ >= offline)
        {
          jsonWriteStr(json, F("info"), F("offline"));
        }
      }
    }
    else
    {
      jsonWriteStr(json, F("info"), F("awaiting"));
    }
    String idStr = String(_id.c_str());
    sendSubWidgetsValues(idStr, json);
  }

  BleSens(String parameters) : IoTItem(parameters)
  {
    _MAC = jsonReadStr(parameters, "MAC");
    _sensor = jsonReadStr(parameters, "sensor");
    jsonRead(parameters, F("orange"), orange);
    jsonRead(parameters, F("red"), red);
    jsonRead(parameters, F("offline"), offline);
    jsonRead(parameters, F("int"), _int);
    dataFromNode = false;
    BleSensArray.push_back(this);
  }

  ~BleSens() {};
};

//=======================================================================================================

class BleScan : public IoTItem, BLEAdvertisedDeviceCallbacks
{
private:
  // описание параметров передаваемых из настроек датчика из веба
  int _scanDuration;
  String _filter;
  bool _debug;

  JsonDocument doc;
  BLEScan *pBLEScan;
  TheengsDecoder decoder;

public:
  std::string convertServiceData(std::string deviceServiceData)
  {
    int serviceDataLength = (int)deviceServiceData.length();
    char spr[2 * serviceDataLength + 1];
    for (int i = 0; i < serviceDataLength; i++)
      sprintf(spr + 2 * i, "%.2x", (unsigned char)deviceServiceData[i]);
    spr[2 * serviceDataLength] = 0;
    return spr;
  }

  void onResult(BLEAdvertisedDevice *advertisedDevice)
  {
    // Защита от nullptr и валидация данных на ESP32-C6
    if (advertisedDevice == nullptr) {
      return;
    }
    
    // Проверяем, что BLE инициализирован
    if (!bleInitialized || pBLEScan == nullptr) {
      return;
    }
    
    JsonObject BLEdata = doc.to<JsonObject>();
    String mac_adress_ = "";
    
    // Безопасное получение MAC адреса
    try {
      BLEAddress addr = advertisedDevice->getAddress();
      if (addr.isNull()) {
        return;
      }
      mac_adress_ = addr.toString().c_str();
      mac_adress_.toUpperCase();
    } catch (...) {
      return;
    }
    
    BLEdata["id"] = (char *)mac_adress_.c_str();
    
    // Безопасное получение имени
    if (advertisedDevice->haveName()) {
      String devName = advertisedDevice->getName().c_str();
      if (devName.length() > 0 && devName.length() < 64) {
        BLEdata["name"] = (char *)devName.c_str();
      }
    }
    
    // Безопасное получение RSSI
    int rssi = 0;
    try {
      rssi = advertisedDevice->getRSSI();
      if (rssi != 0) {
        BLEdata["rssi"] = rssi;
      }
    } catch (...) {
      // RSSI недоступен
    }
    
    // Безопасное получение TX Power
    if (advertisedDevice->haveTXPower()) {
      try {
        BLEdata["txpower"] = (int8_t)advertisedDevice->getTXPower();
      } catch (...) {
        // TX Power недоступен
      }
    }
    
    // Безопасное получение Service Data
    if (advertisedDevice->haveServiceData()) {
      try {
        int serviceDataCount = advertisedDevice->getServiceDataCount();
        if (serviceDataCount > 0 && serviceDataCount < 10) {
          for (int j = 0; j < serviceDataCount; j++) {
            std::string service_data = convertServiceData(advertisedDevice->getServiceData(j));
            BLEdata["servicedata"] = (char *)service_data.c_str();
            std::string serviceDatauuid = advertisedDevice->getServiceDataUUID(j).toString();
            BLEdata["servicedatauuid"] = (char *)serviceDatauuid.c_str();
          }
        }
      } catch (...) {
        // Service Data недоступен
      }
    }
    
    if (decoder.decodeBLEJson(BLEdata))
    {
      String mac_address = BLEdata["mac"].as<const char *>();
      if (mac_address == "")
      {
        BLEdata["mac"] = BLEdata["id"];
        mac_address = BLEdata["id"].as<const char *>();
      }
      mac_address.replace(":", "");

      if (_debug < 2)
      {
        BLEdata.remove("manufacturerdata");
        BLEdata.remove("servicedata");
        BLEdata.remove("type");
        BLEdata.remove("cidc");
        BLEdata.remove("acts");
        BLEdata.remove("cont");
        BLEdata.remove("track");
        BLEdata.remove("id");
      }
      // дописываем время прихода пакета данных
      BLEdata["last"] = millis();
      if (_debug)
      {
        if ((_filter != "" && BLEdata[_filter]) || _filter == "")
        {
          //  for (JsonPair kv : BLEdata)
          // {
          // String val = BLEdata.as<String>();
          String output;
          if (_debug < 2)
          {
            BLEdata.remove("servicedatauuid");
          }
          serializeJson(BLEdata, output);
          SerialPrint("i", F("BLE"), mac_address + " " + output);
          //}
        }
        if (_debug > 1){
        SerialPrint("i", F("BLE"), "found: " + String(BLEdata["mac"].as<const char *>()));
        }
      }

      // Перебираем все зарегистрированные сенсоры BleSens
      for (std::vector<BleSens *>::iterator it = BleSensArray.begin();
           it != BleSensArray.end(); ++it)
      {
        // Если это данные для нужного сенсора (по его МАКУ)
        if ((*it)->whoIAm() == mac_address){
          // то передаем ему json, дальше он сам разберется
          (*it)->setBLEdata(BLEdata);
     
        }
      }
    }
  }
  /** Callback to process the results of the last scan or restart it */
  void onScanEnd(NimBLEScanResults results)
  {
    int count = results.getCount();
    SerialPrint("i", F("BLE"), "Scan done! "); // +"Devices found: " + String(count));
    SerialPrint("i", F("BLE"), "Devices found: " + String(count));
    // pBLEScan->clearResults();
  }
  BleScan(String parameters) : IoTItem(parameters)
  {
    _scanDuration = jsonReadInt(parameters, "scanDuration") * 1000;
    _filter = jsonReadStr(parameters, "filter");
    jsonRead(parameters, "debug", _debug);
    pBLEScan = nullptr;

    // Защита от множественной инициализации BLE на ESP32-C6
    // Используем мьютекс для предотвращения race condition
    if (bleInitMutex == NULL) {
      bleInitMutex = xSemaphoreCreateMutex();
    }
    
    if (bleInitMutex != NULL && xSemaphoreTake(bleInitMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      if (!bleInitialized) {
        // Инициализируем BLEDevice с именем устройства
        String devName = jsonReadStr(settingsFlashJson, F("name"));
        if (devName == "" || devName == "null") {
          devName = "IoTManager";
        }
        BLEDevice::init(devName.c_str());
        bleInitialized = true;
        SerialPrint("i", F("BLE"), "NimBLE initialized");
      }
      xSemaphoreGive(bleInitMutex);
    }

    // Создаем сканер
    pBLEScan = BLEDevice::getScan();
    if (pBLEScan != nullptr) {
      pBLEScan->setScanCallbacks(this, false); // false = не копировать callback'и
      pBLEScan->setActiveScan(false); // Пассивное сканирование стабильнее на ESP32-C6
      pBLEScan->setInterval(160); // 100ms в единицах 0.625ms
      pBLEScan->setWindow(80); // 50ms - меньше или равно interval
      pBLEScan->setMaxResults(0); // Не хранить результаты, только callback
    } else {
      SerialPrint("E", F("BLE"), "Failed to get BLE scan");
    }
  }

  // doByInterval()
  void doByInterval()
  {
    // Защита от nullptr на ESP32-C6
    if (pBLEScan == nullptr) {
      return;
    }
    
    if (pBLEScan->isScanning() == false)
    {
      if (_scanDuration > 0)
      {
        SerialPrint("i", F("BLE"), "Start Scanning...");
        pBLEScan->start(_scanDuration, false);
        // BLEScanResults foundDevices = pBLEScan->getResults(_scanDuration, false);
        // SerialPrint("i", F("BLE"), "Scan done!");
        // SerialPrint("i", F("BLE"), "Devices found: " + String(foundDevices.getCount()));
        // pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
      }
    }
  }

  ~BleScan() { BleSensArray.clear(); };
};

//=======================================================================================================

void *getAPI_Ble(String subtype, String param)
{
  if (subtype == F("BleScan"))
  {
    return new BleScan(param);
  }
  else if (subtype == F("BleSens"))
  {
    return new BleSens(param);
  }
  else
  {
    return nullptr;
  }
}