#include "MqttClient.h"
#include "classes/IoTDiscovery.h"

void mqttInit() {
    mqtt.setBufferSize(1024); // Увеличиваем буфер до 1024 байт
    mqtt.setCallback(mqttCallback);
    ts.add(
        WIFI_MQTT_CONNECTION_CHECK, MQTT_RECONNECT_INTERVAL,
        [&](void*) {
            if (isNetworkActive()) {
                SerialPrint("i", F("Net"), "http://" + jsonReadStr(settingsFlashJson, F("ip")));
                wifiUptimeCalc();
                if (mqtt.connected()) {
                    // SerialPrint("i", F("MQTT"), "OK");
                    mqttUptimeCalc();
                    handleMqttStatus(false);

                    // setLedStatus(LED_OFF);
                } else {
                    SerialPrint("E", F("Net"), F("✖ Connection lost"));
                    handleMqttStatus(false);
                    mqttUptime = 0;
                    mqttConnect();
                }
            } else {
                SerialPrint("E", F("Net"), F("✖ Lost connection"));
                ts.remove(WIFI_MQTT_CONNECTION_CHECK);
                wifiUptime = 0;
                startAPMode();
            }
        },
        nullptr, true);
}

void mqttLoop() {
    if (!isNetworkActive() || !mqtt.connected()) {
        return;
    }
    mqtt.loop();
}

boolean publishRetain(const String &topic, const String &data)
{
    if (mqtt.beginPublish(topic.c_str(), data.length(), true))
    {
        mqtt.print(data);
        return mqtt.endPublish();
    }
    return false;
}

boolean mqttConnect() {
    getMqttData();
    bool res = false;
    if (mqttServer == "") {
        SerialPrint("E", "MQTT", F("mqttServer empty"));

        handleMqttStatus(true, 6);

        return res;
    }
    SerialPrint("i", "MQTT", "connection started");

    SerialPrint("i", "MQTT", "broker " + mqttServer + ":" + String(mqttPort, DEC));
    SerialPrint("i", "MQTT", "topic " + mqttRootDevice);

    // setLedStatus(LED_FAST);

    mqtt.setServer(mqttServer.c_str(), mqttPort);

    if (!mqtt.connected()) {
        bool connected = false;
        if (mqttUser != "" && mqttPass != "") {
            if (HOMEdDiscovery)
            {
                connected = mqtt.connect(chipId.c_str(), mqttUser.c_str(), mqttPass.c_str(), (HOMEdDiscovery->HOMEdTopic + "/device/custom/" + nameId).c_str(), 1, true, "{\"status\":\"offline\"}");
            }
            else
            {
                connected = mqtt.connect(chipId.c_str(), mqttUser.c_str(), mqttPass.c_str(), (mqttRootDevice + "/state").c_str(), 1, true, "{\"status\":\"offline\"}");
            }
            SerialPrint("i", F("MQTT"), F("Go to connection with login and password"));
        } else if (mqttUser == "" && mqttPass == "") {
            if (HOMEdDiscovery)
            {
                connected = mqtt.connect(chipId.c_str(), (HOMEdDiscovery->HOMEdTopic + "/device/custom/" + nameId).c_str(), 1, true, "{\"status\":\"offline\"}");
            }
            else
            {
                connected = mqtt.connect(chipId.c_str(), (mqttRootDevice + "/state").c_str(), 1, true, "{\"status\":\"offline\"}");
            }
            SerialPrint("i", F("MQTT"), F("Go to connection without login and password"));
        } else {
            SerialPrint("E", F("MQTT"), F("✖ Login or password missed"));
            handleMqttStatus(true, 7);
            return res;
        }

        if (mqtt.state() == 0) {
            SerialPrint("i", F("MQTT"), F("✔ connected"));
            handleMqttStatus(true);
            //   setLedStatus(LED_OFF);
            mqttSubscribe();
             if (HOMEdDiscovery)
            {
                publishRetain((HOMEdDiscovery->HOMEdTopic + "/device/custom/" + nameId).c_str(), "{\"status\":\"online\"}");
            }
            publishRetain((mqttRootDevice + "/state").c_str(), "{\"status\":\"online\"}");

            res = true;
        } else {
            SerialPrint("E", F("MQTT"), "🡆 Could't connect, retry in " + String(MQTT_RECONNECT_INTERVAL / 1000) + "s");
            handleMqttStatus(true);
            //  setLedStatus(LED_FAST);
        }
    }
    return res;
}

void mqttDisconnect() {
    SerialPrint("i", F("MQTT"), F("disconnected"));
    mqtt.disconnect();
}

void mqttReconnect() {
    mqttDisconnect();
    mqttConnect();
}

bool mqttIsConnect(){
    return mqtt.connected();
}

void getMqttData() {
    mqttServer = jsonReadStr(settingsFlashJson, F("mqttServer"));
    mqttPort = jsonReadInt(settingsFlashJson, F("mqttPort"));
    mqttUser = jsonReadStr(settingsFlashJson, F("mqttUser"));
    mqttPass = jsonReadStr(settingsFlashJson, F("mqttPass"));
    mqttPrefix = jsonReadStr(settingsFlashJson, F("mqttPrefix"));
    if (jsonReadInt(settingsFlashJson, F("HOMEd_names"))){
    nameId = jsonReadStr(settingsFlashJson, F("name"));}
    else{nameId = getChipId();}
    mqttRootDevice = mqttPrefix + "/" + chipId;
}

void mqttSubscribe() {
    SerialPrint("i", F("MQTT"), F("subscribed"));
    SerialPrint("i", F("MQTT"), mqttRootDevice);
    mqtt.subscribe(mqttPrefix.c_str());
    mqtt.subscribe((mqttRootDevice + "/+/control").c_str());
    mqtt.subscribe((mqttRootDevice + "/update").c_str());

    if (jsonReadBool(settingsFlashJson, "mqttin")) {
        mqtt.subscribe((mqttPrefix + "/+/+/event/#").c_str());
        mqtt.subscribe((mqttPrefix + "/+/+/order/#").c_str());
        mqtt.subscribe((mqttPrefix + "/+/+/info").c_str());
    }
    for (std::list<IoTItem*>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
        if ((*it)->getSubtype() == "ExternalMQTT") {
            String tmps = (*it)->getMqttExterSub();
            if (tmps != ""){
                mqtt.subscribe(tmps.c_str());
                SerialPrint("i", F("MQTT"), ("subscribed external " + tmps).c_str());
            }
        }
    }
    if(HOMEdDiscovery)
        HOMEdDiscovery->mqttSubscribeDiscovery();
    if(HADiscovery)
        HADiscovery->mqttSubscribeDiscovery();
        // оттправляем все статусы
    if(HOMEdDiscovery || HADiscovery)
    {
        for (std::list<IoTItem *>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it)
        {
            if ((*it)->iAmLocal)
            {
                publishStatusMqtt(String((*it)->getID().c_str()), (*it)->getValue());
                (*it)->onMqttWsAppConnectEvent();
            }
        }
    }
}

void mqttSubscribeExternal(String topic, bool usePrefix) {

    // SerialPrint("i", F("MQTT"), mqttRootDevice);
    String _sb_topic = topic;
    if (usePrefix)
    {
        _sb_topic = mqttPrefix + "/" + topic;
    }
    mqtt.subscribe(_sb_topic.c_str());

    SerialPrint("i", F("MQTT"), ("subscribed external " + _sb_topic).c_str());
}

void mqttCallback(char* topic, uint8_t* payload, size_t length) {
    String topicStr = String(topic);
    // SerialPrint("i", "=>MQTT", topicStr);
    String payloadStr;
    payloadStr.reserve(length + 1);
    for (size_t i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }

    // генерация события прихода mqtt сообщения в модуле
    for (std::list<IoTItem*>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
        (*it)->onMqttRecive(topicStr, payloadStr);
    }

    if (payloadStr.startsWith("HELLO")) {
        SerialPrint("i", F("MQTT"), F("Full update"));

        // публикация всех виджетов
        publishWidgets();

        // публикация всех статус сообщений при подключении приложения и генерация события подключения приложения в модулях
        for (std::list<IoTItem*>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
            if ((*it)->iAmLocal) {
                publishStatusMqtt(String((*it)->getID().c_str()), (*it)->getValue());
                (*it)->onMqttWsAppConnectEvent();
            }
        }

        // отправка данных графиков - данный код будет оптимизирован после завершения написания приложения с новыми графиками
        for (std::list<IoTItem*>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
            if ((*it)->getSubtype() == "Loging" || "LogingDaily") {
                (*it)->setPublishDestination(TO_MQTT);
                (*it)->publishValue();
            }
        }

#ifdef GATE_MODE
        publishTimes();
#endif
#ifdef EnableLogging
        choose_log_date_and_send();
#endif
    }

    else if (topicStr.indexOf(F("control")) != -1) {
        String key = selectFromMarkerToMarker(topicStr, "/", 3);

        String valueIfJson = "";  // проверяем формат, если json то берем статус, иначе - как есть
        if (!jsonRead(payloadStr, F("status"), valueIfJson, false))
            generateOrder(key, payloadStr);
        else
            generateOrder(key, valueIfJson);

        SerialPrint("i", F("=>MQTT"), "Msg from iotmanager app: " + key + " " + payloadStr);
    }

    // здесь мы получаем события с других устройств, которые потом проверяются в сценариях этого устройства
    else if (topicStr.indexOf("event") != -1) {
        if (!jsonReadBool(settingsFlashJson, "mqttin")) {
            return;
        }
        if (topicStr.indexOf(chipId) == -1) {
            String devId = selectFromMarkerToMarker(topicStr, "/", 2);
            String id = selectFromMarkerToMarker(topicStr, "/", 3);
            analyzeMsgFromNet(payloadStr, id);
            // SerialPrint("i", F("=>MQTT"), "Received event from other device: '" + devId + "' " + id + " " + valAsStr);
        }
    }

    // здесь мы получаем прямые команды которые сразу выполнятся на этом устройстве
    // необходимо для тех кто хочет управлять своим устройством из mqtt
    else if (topicStr.indexOf("order") != -1) {
        if (!jsonReadBool(settingsFlashJson, "mqttin")) {
            return;
        }
        String devId = selectFromMarkerToMarker(topicStr, "/", 2);
        String id = selectFromMarkerToMarker(topicStr, "/", 3);
        generateOrder(id, payloadStr);
        SerialPrint("i", F("=>MQTT"), "Received direct order " + id + " " + payloadStr);
    }
}

boolean publish(const String& topic, const String& data) {
    if (mqtt.beginPublish(topic.c_str(), data.length(), false)) {
        mqtt.print(data);
        return mqtt.endPublish();
    }
    return false;
}

boolean publishData(const String& topic, const String& data) {
    String path = mqttRootDevice + "/" + topic;
    if (!publish(path, data)) {
        SerialPrint("E", F("MQTT"), F("on publish data"));
        return false;
    }
    return true;
}

boolean publishChartMqtt(const String& topic, const String& data) {
    String path = mqttRootDevice + "/" + topic + "/status";
    if (!publish(path, data)) {
        SerialPrint("E", F("MQTT"), F("on publish chart"));
        return false;
    }
    return true;
}

boolean publishStatusMqtt(const String& topic, const String& data) {
    if (HOMEdDiscovery)
    {
        HOMEdDiscovery->publishStatusHOMEd(topic, data);
    }
    String path = mqttRootDevice + "/" + topic + "/status";
    String json = "{}";
    jsonWriteStr(json, "status", data);
    return mqtt.publish(path.c_str(), json.c_str(), false);
}

boolean publishJsonMqtt(const String& topic, const String& json) {
    String path = mqttRootDevice + "/" + topic + "/status";
    return mqtt.publish(path.c_str(), json.c_str(), false);
}

boolean publishEvent(const String& topic, const String& data) {
    String path = mqttRootDevice + "/" + topic + "/event";
    return mqtt.publish(path.c_str(), data.c_str(), false);
}

void publishWidgets() {
    auto file = seekFile("layout.json");
    if (!file) {
        SerialPrint("E", F("MQTT"), F("no file layout.json"));
        return;
    }
    size_t size = file.size();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        SerialPrint("E", F("MQTT"), error.f_str());
        jsonWriteInt(errorsHeapJson, F("jse3"), 1);  // Ошибка чтения json файла с виджетами при отправки в mqtt
    }
    JsonArray arr = doc.as<JsonArray>();
    for (JsonVariant value : arr) {
        publishData("config", value.as<String>());
    }
    file.close();
}

bool publishChartFileToMqtt(String path, String id, int maxCount) {
    File configFile = FileFS.open(path, FILE_READ);
    if (!configFile) {
        SerialPrint("E", F("Loging"), path + " file reading error, json not created, return");
        return false;
    }
    String oneSingleJson = configFile.readString();
    configFile.close();
    String topic = mqttRootDevice + "/" + id;
    oneSingleJson = "{\"maxCount\":" + String(maxCount) + ",\"topic\":\"" + topic + "\",\"status\":[" + oneSingleJson + "]}";
    oneSingleJson.replace("},]}", "}]}");
    SerialPrint("i", "Loging", "json size: " + String(oneSingleJson.length()));
    publishChartMqtt(id, oneSingleJson);
    return true;
}

// void handleMqttStatus(bool send) {
//     String stateStr = getStateStr(mqtt.state());
//     // Serial.println(stateStr);
//     jsonWriteStr_(errorsHeapJson, F("mqtt"), stateStr);
//     if (!send) sendStringToWs("errors", errorsHeapJson, -1);
// }

void handleMqttStatus(bool send, int state) {
    if (state == -1) {
            state = mqtt.state();
    }
    String stateStr = getStateStr(state);
    // Serial.println(stateStr);
    jsonWriteStr_(errorsHeapJson, F("mqtt"), stateStr);
    if (!send) sendStringToWs("errors", errorsHeapJson, -1);
}

const String getStateStr(int e) {
    switch (e) {
        case -4:  // Нет ответа от сервера
        return F("e1");
        break;
        case -3:  // Соединение было разорвано
        return F("e2");
        break;
        case -2:  // Ошибка соединения. Обычно возникает когда неверно указано название сервера MQTT
        return F("e3");
        break;
        case -1:  // Клиент был отключен
        return F("e4");
        break;
        case 0:  // подключено
        return F("e5");
        break;
        case 1:  // Ошибка версии
        return F("e6");
        break;
        case 2:  // Отклонен идентификатор
        return F("e7");
        break;
        case 3:  // Не могу установить соединение
        return F("e8");
        break;
        case 4:  // Неправильное имя пользователя/пароль
        return F("e9");
        break;
        case 5:  // Не авторизован для подключения
        return F("e10");
        break;
        case 6:  // Название сервера пустое
        return F("e11");
        break;
        case 7:  // Имя пользователя или пароль пустые
        return F("e12");
        break;
        case 8:  // Подключение в процессе
        return F("e13");
        break;
    default:
        return F("unk");
        break;
    }
}

void mqttUptimeCalc() {
    static unsigned int prevMillis;
    mqttUptime = mqttUptime + (millis() - prevMillis);
    prevMillis = millis();
}

void wifiUptimeCalc() {
    static unsigned int prevMillis;
    wifiUptime = wifiUptime + (millis() - prevMillis);
    prevMillis = millis();
}
