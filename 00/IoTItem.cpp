#include "utils/JsonUtils.h"
#include "utils/SerialPrint.h"
#include "classes/IoTItem.h"
#include "WsServer.h"
#include "ESPConfiguration.h"
#include "EventsAndOrders.h"

IoTItem::IoTItem(const String& parameters) {

    String subtype;
    jsonRead(parameters.c_str(), F("int"), _interval, false);
    setInterval(_interval);
    jsonRead(parameters.c_str(), F("subtype"), subtype, false);
    jsonRead(parameters.c_str(), F("id"), _id);

    // НАШ КУСОК: Считаем числовой хэш от ID один раз при создании
    _idHash = hashFNV1a(_id);
    _subHash = hashFNV1a(subtype);
    //subtype = "";
    lastMillis = millis();

    if (!jsonRead(parameters.c_str(), F("multiply"), _multiply, false)) _multiply = 1;
    if (!jsonRead(parameters.c_str(), F("plus"), _plus, false)) _plus = 0;
    if (!jsonRead(parameters.c_str(), F("round"), _round, false)) _round = 0;
    if (!jsonRead(parameters.c_str(), F("numDigits"), _numDigits, false)) _numDigits = 1;
    if (!jsonRead(parameters.c_str(), F("global"), _global, false)) _global = false;

    String map;

    //SerialPrint("IoTitem", "IoTitem", subtype, _id);
    jsonRead(parameters.c_str(), F("map"), map, false);
    if (map[0] != '\0') {
        
        sscanf(map.c_str(), "%d,%d,%d,%d", &_map1, &_map2, &_map3, &_map4);

    } else
        _map1 = _map2 = _map3 = _map4 = 0;

    map = "";
    subtype = "";

    // String valAsStr;
    // if (jsonRead(parameters.c_str(), F("val"), valAsStr, false))  // значение переменной или датчика при инициализации если есть в конфигурации
    //     setValue(valAsStr, false);

    // valAsStr = "";
        // --- УМНАЯ ИНИЦИАЛИЗАЦИЯ ИЗ JSON С ИСПОЛЬЗОВАНИЕМ ШАБЛОНОВ ---
    // Сначала пробуем прочитать значение как чистый double. 
    // Если в JSON лежит число, ваша шаблонная jsonRead запишет его напрямую в value.valD, минуя строки!
    double tmpValD = 0.0;
    if (jsonRead(parameters.c_str(), F("val"), tmpValD, false)) {
        setValue(tmpValD, false); // Автоматически вызовет числовую перегрузку setValue(double, bool)
        //SerialPrint("IoTitem", "DOUBLE", String(_id), String(tmpValD));
    } else {
        // Если как число прочитать не удалось, значит там строка (например, время "11:30")
        String tmpValS;
        if (jsonRead(parameters.c_str(), F("val"), tmpValS, false)) {
            setValue(tmpValS, false); // Автоматически вызовет строковую перегрузку setValue(const String&, bool)
            //SerialPrint("IoTitem", "STRING", String(_id), String(tmpValS));
        }
    }
    // -------------------------------------------------------------


    jsonRead(parameters.c_str(), F("needSave"), _needSave, false);
    //if (_needSave && jsonRead(valuesFlashJson, _id, valAsStr, false))  // пробуем достать из сохранения значение элемента, если указано, что нужно сохранять
    //    setValue(valAsStr, false);
    // if (_needSave && valuesDoc.containsKey(_id)) {  
    //     // Извлекаем значение напрямую в виде String и передаем в setValue
    //     setValue(valuesDoc[_id].as<String>(), false);
    // }

    // проверяем нужно ли отслеживать значение другого элемента
    String trackingID = "";
    IoTItem* item = nullptr;
    if (jsonRead(parameters.c_str(), F("trackingID"), trackingID, false) && (item = findIoTItem(trackingID)) != nullptr) {
        _trackingValue = &(item->value);
    }
}

void IoTItem::suspendNextDoByInt(unsigned long _delay) { // 0 - force
    lastMillis = millis() + _delay; 
}

void IoTItem::loop() {     
    if (enableDoByInt) {         
        unsigned long currentMillis = millis(); 
        
        // Правильная обработка переполнения (rollover)
        if (currentMillis - lastMillis  >= _interval) {            
            lastMillis = currentMillis; // Сбрасываем таймер
            //SerialPrint(F("i"), _id, "this->doByInterval", String(lastMillis));             
            this->doByInterval();         
        }     
    } 
}

String IoTItem::getValue() {
    if (value.isDecimal) {
        return getRoundValue();
    } else
        return value.valS;
}

long IoTItem::getInterval() { return _interval; }

bool IoTItem::isGlobal() { return _global; }

// void IoTItem::setValue(const String& valStrr, bool genEvent) {

//     String valStr = valStrr;
//     valStr.trim();
//     value.isDecimal = isDigitDotCommaStr(valStr);
    
//     if (value.isDecimal) {

//         value.valD = valStr.toFloat();               
//         getRoundValue();

//     } else {

//         value.valS = valStr;
//     }

//     setValue(value, genEvent);
// }
// 1. Числовая перегрузка (Чистая математика, 0 байт аллокаций строк)
void IoTItem::setValue(double val, bool genEvent) {
    value.valD = val;
    value.isDecimal = true;
    value.valS.clear(); // Сбрасываем длину, но сохраняем capacity буфера!

    // Наш оптимизированный метод округления (который теперь не портит оригинал)
    getRoundValue(); 

    // Вызываем ваш базовый исполнитель модулей
    setValue(value, genEvent);
}

// 2. Строковая перегрузка (Включается только для истинного текста вроде "11:30")
void IoTItem::setValue(const String& valStr, bool genEvent) {
    String trimmed = valStr;
    trimmed.trim();
    
    // Защита: если из веб-интерфейса или MQTT прилетело число в виде строки
    if (isDigitDotCommaStr(trimmed)) {
        setValue(trimmed.toFloat(), genEvent); // Уходим в чистые числа
    } else {
        // Если это реальный текст
        value.isDecimal = false;
        value.valS = std::move(trimmed); // Перемещаем буфер без копирования
        value.valD = 0.0;
        
        setValue(value, genEvent);
    }
}

void IoTItem::setValue(const IoTValue& Value, bool genEvent) {
    value = Value; 

    if (value.isDecimal) {
        regEvent(value.valD, "", false, genEvent);
    } else {
        regEvent(value.valS, "", false, genEvent);
    }
}

// метод отправки из модуля дополнительных json полей виджета в приложение и веб интерфейс,
// необходимый для изменения виджетов "на лету" из модуля
void IoTItem::sendSubWidgetsValues(String& id, String& json) {
    publishJsonMqtt(id, json);
    publishJsonWs(id, json);
}

// когда событие случилось
void IoTItem::regEvent(const String& value, const String& consoleInfo, bool error, bool genEvent) {
    if (_needSave) {
        //jsonWriteStr_(valuesFlashJson, _id, value);
        //needSaveValues = true;
        // Проверяем, изменилось ли значение в документе
        // if (valuesDoc[_id] != value.c_str()) {
        //     valuesDoc[_id] = value.c_str(); // Записываем/обновляем ключ напрямую
        //     needSaveValues = true;  // Выставляем флаг для сохранения в loop()
        // }
    }
    publishStatusMqtt(_id, value);
    publishStatusWs(_id, value);
    // SerialPrint("i", "Sensor", consoleInfo + " '" + _id + "' data: " + value + "'");

    if (genEvent) {
        generateEvent(_idHash, value);

        // отправка события другим устройствам в сети если не было ошибки
        if (_global && !error) {
            String json; // = F("{}");
            jsonWrite(json, F("id"), _id);
            jsonWrite(json, F("val"), value);
            jsonWrite(json, F("int"), _interval / 1000);
            publishEvent(_id, json);
            SerialPrint(F("i"), F("<=MQTT"), F("Broadcast event: "), json);
        }
    }
}

String IoTItem::getRoundValue() {
    if (!value.isDecimal) return value.valS;

    if (_round >= 0 && _round <= 6) {
        // Убираем pow(). Используем быстрый массив степеней десятки
        static const uint32_t powers[] = {1, 10, 100, 1000, 10000, 100000, 1000000};
        uint32_t sot = powers[_round];
        
        // Математическое округление
        value.valD = ::round(value.valD * sot) / (double)sot;

        // Выделяем буферы на стеке, а не в куче. 16 байт с запасом хватит для float/double
        char fmt[12];
        char buf[16];
        
        // Безопасно формируем строку формата, например "%05.2f"
        // Точка в формате экранируется как %%, чтобы sprintf воспринял её как символ
        snprintf(fmt, sizeof(fmt), "%%0%d.%df", (_numDigits + _round), _round);
        
        // Форматируем число с ведущими нулями
        snprintf(buf, sizeof(buf), fmt, value.valD);
        
        value.valS = String(buf);
    } else {
        value.valS = String(value.valD);
    }
    
    return value.valS;
}

void IoTItem::regEvent(float regvalue, const String& consoleInfo, bool error, bool genEvent) {
        value.valD = regvalue;

    if (_multiply) value.valD = regvalue * _multiply;
    if (_plus) value.valD = value.valD + _plus;
    if (_map1 != _map2) value.valD = map(value.valD, _map1, _map2, _map3, _map4);

    regEvent(getRoundValue(), consoleInfo, error, genEvent);
}

void IoTItem::doByInterval() {}

IoTValue IoTItem::execute(String command, std::vector<IoTValue>& param) { return {}; }

//String IoTItem::getSubtype() {
//    return _subtype;
//}

int IoTItem::getIntFromNet() {
    return _intFromNet;
}

void IoTItem::getNetEvent(String& event) {
    //event = "";
    jsonWrite(event, F("id"), _id);
    jsonWrite(event, F("val"), getValue());
    jsonWrite(event, F("int"), _interval / 1000);
}

void IoTItem::setIntFromNet(int interval) {
    _intFromNet = interval;
}

void IoTItem::checkIntFromNet() {
    // проверяем элемент на доверие данным.   
    if (_intFromNet >= 0) {
        // если время жизни истекло, то удаляем элемент чуть позже на следующем такте loop
        // если это было уведомление не об ошибке или начале работы, то сообщаем, что сетевое событие давно не приходило
        if (_intFromNet == 0 && /*_id.indexOf(F("onError")) == -1 &&*/ _idHash != 486281996 && _idHash != 4046869490 && _idHash != 3558363773) {
            SerialPrint(F("E"), _id, F("The new data did not come from the network. The level of trust is low."), _id);
        }
        _intFromNet--;
        //SerialPrint(F("E"), _id, String(_intFromNet));
    }
}

String IoTItem::getMqttExterSub() {
    return "";
}

// хуки для системных событий (должны начинаться с "on")
void IoTItem::onRegEvent(IoTItem* item) {}
void IoTItem::onMqttRecive(String& topic, String& msg) {}
void IoTItem::onMqttWsAppConnectEvent() {}
void IoTItem::onModuleOrder(String& key, String& value) {}
void IoTItem::onTrackingValue(IoTItem* item) {
    setValue(item->getValue(), false);
}

bool IoTItem::isTracking(IoTItem* item) {
    return &(item->value) == _trackingValue;
}

// делаем доступным модулям отправку сообщений в телеграм
void IoTItem::sendTelegramMsg(bool often, String msg) {}
void IoTItem::sendFoto(uint8_t *buf, uint32_t length, const String &name) {}
void IoTItem::editFoto(uint8_t *buf, uint32_t length, const String &name) {}
// для обновления экрана Nextion из телеграм
void IoTItem::uploadNextionTlgrm(String &url) {}

// методы для графиков (будет упрощено)
void IoTItem::publishValue() {}
void IoTItem::clearValue() {}
void IoTItem::setPublishDestination(int publishType, int wsNum){};
void IoTItem::clearHistory() {}
void IoTItem::setTodayDate() {}

String IoTItem::getID() {
    return _id;
};

bool IoTItem::isStrInID(const String& str) {
    return _id.indexOf(str) != -1; 
}

void IoTItem::setInterval(unsigned long interval) {
    if (interval == 0) enableDoByInt = false;           // выключаем использование периодического выполнения в модуле
    else {
        enableDoByInt = true;
        if (interval > 0) _interval = interval * 1000;    // если int положителен, то считаем, что получены секунды
        else if (interval < 0) _interval = interval * -1;      // если int отрицательный, то миллисекунды
    }
    // SerialPrint(F("i"), F("IoTItem"), "setInterval: " + _interval.toString);
}

IoTGpio* IoTItem::getGpioDriver() {
    return nullptr;
}

#ifdef mod_RtcDriver
IoTItem* IoTItem::getRtcDriver() {
    return nullptr;
}
#endif

/*
IoTItem* IoTItem::getCAMDriver() {
    return nullptr;
}
*/
IoTItem* IoTItem::getTlgrmDriver() {
    return nullptr;
}

// IoTDiscovery *IoTItem::getHOMEdDiscovery()
// {
//     return nullptr;
// }
// IoTDiscovery *IoTItem::getHADiscovery()
// {
//     return nullptr;
// }
#ifdef mod_RtcDriver
unsigned long IoTItem::getRtcUnixTime()
{
    return 0;
}
#endif

// сетевое общение====================================================================================================================================

// externalVariable::externalVariable(const String& parameters) : IoTItem(parameters) {
//     prevMillis = millis();  // запоминаем текущее значение таймера для выполения doByInterval после int сек
//     iAmLocal = false;       // указываем, что это сущность прилетела из сети
//     //Serial.printf("Call from  externalVariable: parameters %s %d\n", parameters.c_str(), _interval);
// }

// externalVariable::~externalVariable() {
//     Serial.printf("Call from  ~externalVariable: Im dead\n");
// }

// void externalVariable::doByInterval() {  // для данного класса doByInterval+int выполняет роль счетчика обратного отсчета до уничтожения
//     iAmDead = true;
// }

//=========================================================================================================================================
// IoTItem* myIoTItem; // экономим память, используется в одном месте

// Старый строковый поиск считает хэш и вызывает числовой
IoTItem* findIoTItem(const String& name) {
    if (name.length() == 0) return nullptr;
    return findIoTItem(hashFNV1a(name));
}

// НОВАЯ СВЕРХБЫСТРАЯ ФУНКЦИЯ ПОИСКА (По числу):
IoTItem* findIoTItem(uint32_t nameHash) {
    for (auto item : IoTItems) {
        if (item->getIDHash() == nameHash) return item;
    }
    return nullptr;
}

// // поиск элемента модуля в существующей конфигурации
// IoTItem* findIoTItem(const String& name) {
//     if (name[0] == '\0') return nullptr;
//     for (std::list<IoTItem*>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
//         if ((*it)->getID() == name) return *it;
//     }

//     return nullptr;
// }

// поиск элемента модуля в существующей конфигурации по части имени
IoTItem* findIoTItemByPartOfName(const String& partName) {
    if (partName[0] == '\0') return nullptr;
    for (std::vector<IoTItem*>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
        if ((*it)->isStrInID(partName)) return *it;
    }

    return nullptr;
}

// поиск плюс получение значения
String getItemValue(const String& name) {
    IoTItem* tmp = findIoTItem(name);
    if (tmp)
        return tmp->getValue();
    else
        return "";
}

// существует ли айтем
bool isItemExist(const String& name) {
    IoTItem* tmp = findIoTItem(name);
    if (tmp)
        return true;
    else
        return false;
}

// создаем временную копию элемента из сети на основе события

IoTItem* createItemFromNet(const String& itemId, const String& value, int interval) {
    String jsonStr;
    
    jsonWrite(jsonStr, F("id"), itemId);
    jsonWrite(jsonStr, F("val"), value);
    jsonWrite(jsonStr, F("int"), interval);

    //Serial.println(String("CREATE : ") + jsonStr);
    
    return createItemFromNet(jsonStr);
}

// IoTItem* createItemFromNet(const String& itemId, const String& value, int interval) {
//     char buffer[256]; // Размер с запасом под вашу задачу
//     snprintf(buffer, sizeof(buffer), "{\"id\":\"%s\",\"val\":\"%s\",\"int\":%d}", 
//              itemId.c_str(), value.c_str(), interval);
    
//     // Передаем как String (создастся один раз при вызове)
//     return createItemFromNet(String(buffer));
// }

// создаем временную копию элемента из сети на основе события
IoTItem* createItemFromNet(const String& msgFromNet) {
    IoTItem* tmpp = new IoTItem(msgFromNet);

    if (tmpp->getInterval()) tmpp->setIntFromNet(tmpp->getInterval() / 1000 + 5);
    tmpp->iAmLocal = false;
    IoTItems.push_back(tmpp);
    generateEvent(tmpp->getIDHash(), tmpp->getValue());
    return tmpp;
}

void analyzeMsgFromNet(const String& msg, String altId) {
    //if (msg.length() == 0) return;
    if (!jsonRead(msg.c_str(), F("id"), altId)) return;  // ничего не предпринимаем, если ошибка и altId = "", вообще данная конструкция нужна для совместимости с форматом данных 3 версией
    if (altId.length() == 0) return;
    IoTItem* itemExist = findIoTItem(altId);
    if (itemExist) {
        String valAsStr; // = msg;
        jsonRead(msg.c_str(), F("val"), valAsStr, false);
        long interval = 0;
        jsonRead(msg.c_str(), F("int"), interval, false);

        itemExist->setInterval(interval);                      // устанавливаем такой же интервал как на источнике события
        itemExist->setValue(valAsStr, false);                  // только регистрируем изменения в интерфейсе без создания цикла сетевых событий
        if (interval) itemExist->setIntFromNet(interval + 5);  // если пришедший интервал =0, значит не нужно контролировать доверие, иначе даем фору в 5 сек
        generateEvent(altId, valAsStr);
    } else {
        // временно зафиксируем данные в базе, если локально элемент отсутствует
        createItemFromNet(msg);
        //Serial.println("ffffffffff " + msg + " altId=" + altId);
    }
}
