#pragma once
#include <vector>
#include "classes/IoTGpio.h"

// Единственный эталонный хэш FNV-1a для всего проекта
inline uint32_t hashFNV1a(const char* str, size_t len) {
    uint32_t hash = 2166136261U;
    for (size_t i = 0; i < len; ++i) {
        hash ^= (uint8_t)str[i];
        hash *= 16777619U;
    }
    //Serial.print("[HASH] "); Serial.print( str); Serial.print(" = "); Serial.println(hash);
    return hash;
}

// Перегрузка для удобной работы с типом String
inline uint32_t hashFNV1a(const String& str) {
    return hashFNV1a(str.c_str(), str.length());
}

// Перегрузка для работы со строками-константами "строка"
inline uint32_t hashFNV1a(const char* str) {
    return hashFNV1a(str, strlen(str));
}

//-------------------------------------------------------------------------

struct IoTValue {
    float valD = 0;
    String valS;

    bool isDecimal = true;

     //  очистка БЕЗ удаления буфера строки из памяти
    void clear() {
        valD = 0.0f;
        isDecimal = true;
        valS = ""; // Сбрасывает длину в 0, но СОХРАНЯЕТ capacity (память в куче)
    }
};

class IoTItem {
   public:
    IoTItem(const String& parameters);
    virtual ~IoTItem() {};
    virtual void loop();
    virtual void doByInterval();
    virtual IoTValue execute(String command, std::vector<IoTValue>& param);

    void checkIntFromNet();

    virtual void regEvent(const String& value, const String& consoleInfo, bool error = false, bool genEvent = true);
    virtual void regEvent(float value, const String& consoleInfo, bool error = false, bool genEvent = true);

    //String getSubtype();

    String getID();
    bool isStrInID(const String& str);
    int getIntFromNet();
    virtual String getValue();
    long getInterval();
    bool isGlobal();
    
    void sendSubWidgetsValues(String& id, String& json);

    void setInterval(unsigned long interval);
    void setIntFromNet(int interval);
  
    unsigned long lastMillis ; // достаточно 1 переменной, надо экономить память
    // задержка следующего вызова, не изменяет текущий _interval
    void suspendNextDoByInt(unsigned long _delay); // 0 - force

    IoTValue value;  // хранение основного значения, которое обновляется из сценария, execute(), loop() или doByInterval()

    // bool iAmDead = false;  // признак необходимости удалить объект из базы
    bool iAmLocal = true;  // признак того, что айтем был создан локально

    bool enableDoByInt = true;

    virtual IoTGpio* getGpioDriver();
    //virtual IoTItem* getCAMDriver();
    virtual IoTItem* getTlgrmDriver();
    
#ifdef mod_RtcDriver
    virtual IoTItem* getRtcDriver();
    virtual unsigned long getRtcUnixTime();
#endif
        // делаем доступным модулям отправку сообщений в телеграм
    virtual void sendTelegramMsg(bool often, String msg);
    virtual void sendFoto(uint8_t *buf, uint32_t length, const String &name);
    virtual void editFoto(uint8_t *buf, uint32_t length, const String &name);


    virtual void setValue(const IoTValue& Value, bool genEvent = true);
    virtual void setValue(const String& valStr, bool genEvent = true);
    virtual void setValue(double val, bool genEvent);
    
    String getRoundValue();
    void getNetEvent(String& event);
    virtual String getMqttExterSub();

    // хуки для системных событий (должны начинаться с "on")
    virtual void onRegEvent(IoTItem* item);
    virtual void onMqttRecive(String& topic, String& msg);
    virtual void onMqttWsAppConnectEvent();
    virtual void onModuleOrder(String& key, String& value);
    virtual void onTrackingValue(IoTItem* item);  // момент, когда ядро заметило изменение отслеживаемого значения

    // для обновления экрана Nextion из телеграм
    virtual void uploadNextionTlgrm(String &url);

    // методы для графиков (будет упрощено)
    virtual void publishValue();
    virtual void clearValue();
    virtual void setPublishDestination(int type, int wsNum = -1);
    virtual void clearHistory();
    virtual void setTodayDate();

    // Получить числовой хэш ID
    uint32_t getIDHash() const { return _idHash; }   
    uint32_t getSubHash() const { return _subHash; }  
    uint32_t getSubtype() const { return _subHash; }  
     
    // Получить указатель на внутреннюю структуру с тремя полями значений (int/float/string)
    IoTValue* getIoTValue() { return &value; }

    bool isTracking(IoTItem* item);    // проверка на отслеживание

protected:
    bool _needSave = false;  // признак необходимости сохранять и загружать значение элемента на flash
    //String _subtype = "";
    String _id = "errorId";  // если будет попытка создания Item без указания id, то элемент оставит это значение
    // НАШЕ НОВОЕ ПОЛЕ:
    uint32_t _idHash = 0; // Числовой хэш от _id для мгновенного поиска
    uint32_t _subHash = 0; // Числовой хэш от _id для мгновенного поиска
    unsigned long _interval = 0;
    int _intFromNet = -2;  // количество секунд доверия, пришедших из сети вместе с данными для текущего ИД
                           // -2 - данные не приходили, скорее всего, элемент локальный, доверие есть, в случае прихода сетевого значения с int=0, будет выключен механизм проверки доверия
                           // -1 - данные приходили и обратный отсчет дошел до нуля, значит доверия нет и элемент будет удален при следующем такте loop

    float _multiply;  // умножаем на значение
    float _plus;      // увеличиваем на значение
    int _map1 = 0;
    int _map2 = 0;
    int _map3 = 0;
    int _map4 = 0;
    int _round = 1;  // 1, 10, 100, 1000, 10000
    int _numDigits = 1;     // количество целых значений, не значимые позиции заменяются нулем в строковом формате

    bool _global = false;  // характеристика айтема, что ему нужно слать и принимать события из внешнего мира

    IoTValue* _trackingValue = nullptr;   // указатель на значение родительского элемента изменение которого отслеживается
};

IoTItem* findIoTItem(const String& name);                      // поиск экземпляра элемента модуля по имени
IoTItem* findIoTItem(uint32_t nameHash);                       // поиск экземпляра элемента модуля по хешу
IoTItem* findIoTItemByPartOfName(const String& partName);      // поиск элемента модуля в существующей конфигурации по части имени
String getItemValue(const String& name);                       // поиск плюс получение значения
bool isItemExist(const String& name);                          // существует ли айтем

IoTItem* createItemFromNet(const String& itemId, const String& value, int interval);
IoTItem* createItemFromNet(const String& msgFromNet);
void analyzeMsgFromNet(const String& msg, String altId = "");