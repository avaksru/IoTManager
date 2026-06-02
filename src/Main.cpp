#include "Main.h"
#include <time.h>
#include <SPI.h>
#include "classes/IoTDB.h"
#include "classes/IoTBench.h"
#ifndef LIBRETINY
#include <Wire.h>
#endif
#include "DebugTrace.h"
#if defined(ESP32)
#include <esp_task_wdt.h>
#include <esp_log.h>
#endif
#if defined(esp32s2_4mb) || defined(esp32s3_16mb)
//#include <USB.h>
#endif

IoTScenario iotScen;  // объект управления сценарием

String volStrForSave = "";
// unsigned long currentMillis; // это сдесь лишнее
// unsigned long prevMillis;

void elementsLoop() {
    // передаем управление каждому элементу конфигурации для выполнения своих функций
    for (std::list<IoTItem *>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
        if (benchTaskItem) benchTaskItem->preTaskFunction(String((*it)->getID().c_str()));
        (*it)->loop();
        if (benchTaskItem) benchTaskItem->postTaskFunction(String((*it)->getID().c_str()));
        // if ((*it)->iAmDead) {
        if (!((*it)->iAmLocal) && (*it)->getIntFromNet() == -1) {
            if ((*it)->fromPool()) {
                iotItemPool.destroy(*it);
            } else {
                delete *it;
            }
            IoTItems.erase(it);
            break;
        }
    }

    handleOrder();
    handleEvent();
}

#define SETUPBASE_ERRORMARKER 0
#define SETUPCONF_ERRORMARKER 1
#define SETUPSCEN_ERRORMARKER 2
#define SETUPINET_ERRORMARKER 3
#define SETUPLAST_ERRORMARKER 4
#define TICKER_ERRORMARKER 5
#define HTTP_ERRORMARKER 6
#define SOCKETS_ERRORMARKER 7
#define MQTT_ERRORMARKER 8
#define MODULES_ERRORMARKER 9

#define COUNTER_ERRORMARKER 4       // количество шагов счетчика
#define STEPPER_ERRORMARKER 100000  // размер шага счетчика интервала доверия выполнения блока кода мкс

#if defined(esp32_4mb) || defined(esp32_4mb3f) || defined(esp32_16mb) || defined(esp32cam_4mb)

static int IRAM_ATTR initErrorMarkerId = 0;  // ИД маркера
static int IRAM_ATTR errorMarkerId = 0;
static int IRAM_ATTR errorMarkerCounter = 0;

hw_timer_t *My_timer = NULL;
void IRAM_ATTR onTimer() {
    if (errorMarkerCounter >= 0) {
        if (errorMarkerCounter >= COUNTER_ERRORMARKER) {
            errorMarkerId = initErrorMarkerId;
            errorMarkerCounter = -1;
        } else
            errorMarkerCounter++;
    }
}
#endif

void initErrorMarker(int id) {
#if defined(esp32_4mb) || defined(esp32_4mb3f) || defined(esp32_16mb) || defined(esp32cam_4mb)
    initErrorMarkerId = id;
    errorMarkerCounter = 0;
#endif
}

void stopErrorMarker(int id) {
#if defined(esp32_4mb) || defined(esp32_4mb3f) || defined(esp32_16mb) || defined(esp32cam_4mb)
    errorMarkerCounter = -1;
    if (errorMarkerId)
        SerialPrint("I", "WARNING!", "A lazy (freezing loop more than " + (String)(COUNTER_ERRORMARKER * STEPPER_ERRORMARKER / 1000) + " ms) section has been found! With ID=" + (String)errorMarkerId);
    errorMarkerId = 0;
    initErrorMarkerId = 0;
#endif
}

void setup() {
    // Подавляем логи task_wdt и отключаем Task Watchdog для ESP32-C6 и ESP32-S3
    #if defined(ESP32)
    esp_log_level_set("task_wdt", ESP_LOG_NONE);  // Отключаем логи watchdog
    #endif
    
    #if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32S3)
    esp_task_wdt_deinit();
    Serial.println("[WDT] esp_task_wdt_deinit() called for ESP32-C6/S3");
    #endif

#if defined(esp32s2_4mb) || defined(esp32s3_16mb)
 //   USB.begin();
#endif
#if defined(esp32_4mb) || defined(esp32_4mb3f) || defined(esp32_16mb) || defined(esp32cam_4mb)
    My_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(My_timer, &onTimer, true);
    timerAlarmWrite(My_timer, STEPPER_ERRORMARKER, true);
    timerAlarmEnable(My_timer);
    // timerAlarmDisable(My_timer);

    initErrorMarker(SETUPBASE_ERRORMARKER);
#endif

    Serial.begin(115200);
    Serial.flush();
    //----------- Отладка EXCEPTION (функции с заглушками для отключения) ---------
#if defined(RESTART_DEBUG_INFO)  
    //Привязка коллбэк функции для вызова при перезагрузке
    esp_register_shutdown_handler(debugUpdate);
#endif // RESTART_DEBUG_INFO
    // Печать или оправка отладочной информации
    printDebugTrace();
    startWatchDog();
    Serial.println();
    Serial.println(F("--------------started----------------"));

    // создание экземпляров классов
    // myNotAsyncActions = new NotAsync(do_LAST);

    // инициализация файловой системы
    fileSystemInit();
    Serial.println(F("------------------------"));
    Serial.println("FIRMWARE NAME     " + String(FIRMWARE_NAME));
    Serial.println("FIRMWARE VERSION  " + String(FIRMWARE_VERSION));
    Serial.println("WEB VERSION       " + getWebVersion());
    const String buildTime = String(BUILD_DAY) + "." + String(BUILD_MONTH) + "." + String(BUILD_YEAR) + " " + String(BUILD_HOUR) + ":" + String(BUILD_MIN) + ":" + String(BUILD_SEC);
    Serial.println("BUILD TIME        " + buildTime);
    Serial.println("ETH SUPPORT       W5500 auto-detect enabled");
    jsonWriteStr_(errorsHeapJson, F("bt"), buildTime);
    Serial.println(F("------------------------"));

    // получение chip id
    setChipId();

    verifyFirmware();

    // синхронизация глобальных переменных с flash
    globalVarsSync();

    stopErrorMarker(SETUPBASE_ERRORMARKER);

    initErrorMarker(SETUPCONF_ERRORMARKER);

    // настраиваем i2c шину
    int i2c, pinSCL, pinSDA, i2cFreq;
    jsonRead(settingsFlashJson, "pinSCL", pinSCL, false);
    jsonRead(settingsFlashJson, "pinSDA", pinSDA, false);
    jsonRead(settingsFlashJson, "i2cFreq", i2cFreq, false);
    jsonRead(settingsFlashJson, "i2c", i2c, false);
    //jsonWriteStr_(ssidListHeapJson, "0", "Scaning...");
    if (i2c != 0) {
#ifdef ESP32
        Wire.end();
        Wire.begin(pinSDA, pinSCL, (uint32_t)i2cFreq);
#elif defined(ESP8266)
        Wire.begin(pinSDA, pinSCL);
        Wire.setClock(i2cFreq);
#endif
        SerialPrint("i", "i2c", F("i2c pins overriding done"));
    }
#if defined(RESTART_DEBUG_INFO)
  esp_reset_reason_t esp_reason = esp_reset_reason();
  if (esp_reason == ESP_RST_UNKNOWN || esp_reason == ESP_RST_POWERON) 
    bootloop_panic_count = 0;
/*   else if (bootloop_panic_count == 3 || bootloop_panic_count == 0 ) bootloop_panic_count = 1;
  else if (bootloop_panic_count == 2) bootloop_panic_count = 3;
  else if (bootloop_panic_count == 1) bootloop_panic_count = 2;
  else bootloop_panic_count = 0; */
     Serial.println("bootloop_panic_count     " + String(bootloop_panic_count));
    if (bootloop_panic_count >3 )
    {
        //resetSettingsFlashByPanic();
        bootloop_panic_count = 0;
    }      
    if (bootloop_panic_count >= 3)
    {
        resetSettingsFlashByPanic();
        bootloop_panic_count = -1;
    } 
    if (bootloop_panic_count == -1)
    {
        SerialPrint("E", "CORE", F("CONFIG and SCENARIO reset !!!"));
        bootloop_panic_count = 0;
        ESP.restart();
    } 
#endif // RESTART_DEBUG_INFO
    // настраиваем микроконтроллер
    configure("/config.json");

    stopErrorMarker(SETUPCONF_ERRORMARKER);

    initErrorMarker(SETUPSCEN_ERRORMARKER);

    // подготавливаем сценарии
    iotScen.loadScenario("/scenario.txt");
    // создаем событие завершения инициализации основных моментов для возможности выполнения блока кода при загрузке
    createItemFromNet("onInit", "1", 1);
//    elementsLoop(); //Для работы MQTT Брокера перенес ниже, иначе брокер падает если вызван до routerConnect();

    stopErrorMarker(SETUPSCEN_ERRORMARKER);

    initErrorMarker(SETUPINET_ERRORMARKER);

// подключаемся к роутеру или Ethernet
#ifdef ESP8266
    routerConnect();
#else
    // Проверяем, использовать ли Ethernet вместо WiFi
    bool useEthernet = jsonReadBool(settingsFlashJson, "useEthernet");
    if (useEthernet) {
        SerialPrint("i", "NET", "Using Ethernet connection");
        ethernetInit();
    } else {
        // Используем WiFi по умолчанию
        // Примечание: SPI Ethernet (W5500) поддерживается только для ESP32 классический
        // Для ESP32-S3 и ESP32-C6 используется только WiFi
        #if defined(esp32_4mb) || defined(esp32_4mb3f) || defined(esp32_16mb) || defined(esp32cam_4mb)
        // Для классических ESP32 попробуем обнаружить W5500 через SPI
        SerialPrint("i", "NET", "Attempting auto-detect W5500 Ethernet...");
        int csPin = 10;      // CS пин для W5500 по умолчанию
        int intPin = 4;     // INT пин для W5500 по умолчанию
        
        jsonRead(settingsFlashJson, "ethCsPin", csPin);
        jsonRead(settingsFlashJson, "ethIntPin", intPin);
        
        // Инициализируем SPI
        SPI.begin(18, 19, 23, csPin);  // SCK, MISO, MOSI, CS
        
        // Пытаемся прочитать версионный регистр W5500 для обнаружения
        // W5500 имеет регистр версии по адресу 0x0039, который должен содержать 0x04
        pinMode(csPin, OUTPUT);
        digitalWrite(csPin, LOW);
        SPI.transfer(0x00);  // Адрес high byte
        SPI.transfer(0x39);  // Адрес low byte  
        SPI.transfer(0x00);  // Контрольный байт (чтение)
        uint8_t version = SPI.transfer(0x00);  // Читаем версию
        digitalWrite(csPin, HIGH);
        
        SerialPrint("i", "NET", "W5500 version register: 0x" + String(version, HEX));
        
        if (version == 0x04) {
            // W5500 обнаружен!
            SerialPrint("i", "NET", "W5500 detected! Using Ethernet connection");
            // Регистрируем обработчик событий Ethernet
            WiFi.onEvent(onEthEvent);
            // Инициализируем Ethernet
            ETH.begin(ETH_PHY_W5500);
            ETH.setHostname("IoTManager");
            // Отключаем WiFi
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
        } else {
            SerialPrint("i", "NET", "W5500 not detected, using WiFi connection");
            WiFiUtilsItit();
        }
        #else
        // Для ESP32-S3, ESP32-C6 и других используем только WiFi
        SerialPrint("i", "NET", "Using WiFi connection (SPI Ethernet not available for this platform)");
        WiFiUtilsItit();
        #endif
    }
#endif
// инициализация асинхронного веб сервера и веб сокетов
#ifdef ASYNC_WEB_SERVER
    asyncWebServerInit();
#endif
#ifdef ASYNC_WEB_SOCKETS
    asyncWebSocketsInit();
#endif

// инициализация стандартного веб сервера и веб сокетов
#ifdef STANDARD_WEB_SERVER
    standWebServerInit();
#endif
#ifdef STANDARD_WEB_SOCKETS
    standWebSocketsInit();
#endif

    stopErrorMarker(SETUPINET_ERRORMARKER);

    // bool postMsgTelegram;
    // if (!jsonRead(settingsFlashJson, "debugTraceMsgTlgrm", postMsgTelegram, false)) postMsgTelegram = 1;
    // sendDebugTraceAndFreeMemory(postMsgTelegram);
    
    initErrorMarker(SETUPLAST_ERRORMARKER);

    elementsLoop();
    // NTP
    ntpInit();

    // инициализация задач переодического выполнения
    periodicTasksInit();

#if defined(ESP8266)
    // Перенесли после получения IP, так как теперь работа WiFi асинхронная
    // запуск работы udp
    addThisDeviceToList();
    #ifdef UDP_ENABLED
    udpListningInit();
    udpBroadcastInit();
    #endif
#endif    
    // создаем событие завершения конфигурирования для возможности выполнения блока кода при загрузке
    createItemFromNet("onStart", "1", 1);

    // stInit();

    // настраиваем секундные обслуживания системы
    ts.add(
        TIMES, 1000, [&](void *) {
            // сохраняем значения IoTItems в файл каждую секунду, если были изменения (установлены маркеры на сохранение)
            if (needSaveValues) {
                syncValuesFlashJson();
                needSaveValues = false;
            }

            // проверяем все элементы на тухлость
            for (std::list<IoTItem *>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
                (*it)->checkIntFromNet();

                // Serial.printf("[ITEM] size: %d, id: %s, int: %d, intnet: %d\n", sizeof(**it), (*it)->getID(), (*it)->getInterval(), (*it)->getIntFromNet());
            }
        },
        nullptr, true);

    // ловим пинги от WS (2сек) и дисконнектим если их нет 3 раза 6сек*2прохода = 12сек
    ts.add(
        PiWS, 12000, [&](void*) {
            if (isNetworkActive()) {
                for (size_t i = 0; i < WEBSOCKETS_CLIENT_MAX; i++)
                {
                    if (ws_clients[i] == 0) {
                        disconnectWSClient(i);
                        ws_clients[i]=-1;
                    }
                    if (ws_clients[i] > 0) { 
                        ws_clients[i]=0;
                    }

                }
            }
        },
        nullptr, true);

    // test
    //Serial.println("-------test start--------");
    //Serial.println("--------test end---------");

    stopErrorMarker(SETUPLAST_ERRORMARKER);
#if defined(RESTART_DEBUG_INFO)    
    bootloop_panic_count = 0;
#endif // RESTART_DEBUG_INFO
}

void loop() {
#if !defined(ESP8266)
    static bool udpFirstFlag = true;
    // Перенесли после получения IP, так как теперь работа WiFi асинхронная
    if (isNetworkActive() && udpFirstFlag) {
        udpFirstFlag = false;
        // запуск работы udp
        addThisDeviceToList();
        #ifdef UDP_ENABLED
        udpListningInit();
        udpBroadcastInit();
        #endif
    }
#endif    

#ifdef LOOP_DEBUG
    unsigned long st = millis();
#endif
    
    if (benchLoadItem) benchLoadItem->preLoadFunction();
    if (benchTaskItem) benchTaskItem->preTaskFunction("TickerScheduler");
    initErrorMarker(TICKER_ERRORMARKER);
    ts.update();
    stopErrorMarker(TICKER_ERRORMARKER);
    if (benchTaskItem) benchTaskItem->postTaskFunction("TickerScheduler");
    if (benchTaskItem) benchTaskItem->preTaskFunction("webServer");
#ifdef STANDARD_WEB_SERVER
    initErrorMarker(HTTP_ERRORMARKER);
    HTTP.handleClient();
    stopErrorMarker(HTTP_ERRORMARKER);
#endif
    if (benchTaskItem) benchTaskItem->postTaskFunction("webServer");
    if (benchTaskItem) benchTaskItem->preTaskFunction("webSocket");
#ifdef STANDARD_WEB_SOCKETS
    initErrorMarker(SOCKETS_ERRORMARKER);
    standWebSocket.loop();
    stopErrorMarker(SOCKETS_ERRORMARKER);
#endif
    if (benchTaskItem) benchTaskItem->postTaskFunction("webSocket");
    if (benchTaskItem) benchTaskItem->preTaskFunction("mqtt");
    initErrorMarker(MQTT_ERRORMARKER);
    mqttLoop();
    stopErrorMarker(MQTT_ERRORMARKER);
    if (benchTaskItem) benchTaskItem->postTaskFunction("mqtt");
    initErrorMarker(MODULES_ERRORMARKER);
    elementsLoop();
    stopErrorMarker(MODULES_ERRORMARKER);
    if (benchLoadItem) benchLoadItem->postLoadFunction();
    // #ifdef LOOP_DEBUG
    //     loopPeriod = millis() - st;
    //     if (loopPeriod > 2) Serial.println(loopPeriod);
    // #endif
}

// отправка json
// #ifdef QUEUE_FROM_STR
//     if (sendJsonFiles) sendJsonFiles->loop();
// #endif

// if(millis()%2000==0){
//     //watch->settimeUnix(time(&iotTimeNow));
//     Serial.println(watch->gettime("d-m-Y, H:i:s, M"));
//     delay(1);
// }

// File dir = FileFS.open("/", "r");
// String out;
// printDirectory(dir, out);
// Serial.println(out);

//=======проверка очереди из структур=================

// myDB = new IoTDB;
// QueueItems myItem;
// myItem.myword = "word1";
// myDB->push(myItem);
// myItem.myword = "word2";
// myDB->push(myItem);
// myItem.myword = "word3";
// myDB->push(myItem);
// Serial.println(myDB->front().myword);
// Serial.println(myDB->front().myword);
// Serial.println(myDB->front().myword);

// Serial.println(FileList("lg"));
