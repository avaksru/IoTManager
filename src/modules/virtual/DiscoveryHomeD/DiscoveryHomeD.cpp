#include "Global.h"
#include "classes/IoTDiscovery.h"
// #include "MqttDiscovery.h"
class DiscoveryHomeD : public IoTDiscovery
{
private:
    String _topic = "";
    //    bool sendOk = false;
    // bool topicOk = false;
    bool HOMEd = false;
    //    int _names = 0;
    int _toggle = 0;
    int _switch = 0;
    String ChipId = getChipId();
    //    String esp_id = ChipId;

public:
    DiscoveryHomeD(String parameters) : IoTDiscovery(parameters)
    {
        _topic = jsonReadStr(parameters, "topic");
        //        _names = jsonReadInt(parameters, "names");
        _toggle = jsonReadInt(parameters, "toggle");
        _switch = jsonReadInt(parameters, "switch");
        if (_topic && _topic != "" && _topic != "null")
        {
            HOMEd = true;
            HOMEdTopic = _topic;
        }
    }

    void onMqttRecive(String &topic, String &payloadStr)
    {
        if (!HOMEd)
            return;
    }

    void doByInterval()
    {
    }

    void publishStatusHOMEd(const String &topic, const String &data)
    {
    }

    void mqttSubscribeDiscovery()
    {
        if (mqttIsConnect() && HOMEd)
        {
            // deleteFromHOMEd();
            getlayoutHOMEd();
            publishRetain(mqttRootDevice + "/state", "{\"status\":\"online\"}");
            //            String HOMEdsubscribeTopic = HOMEdTopic + "/td/custom/" + esp_id;
            //            mqtt.subscribe(HOMEdsubscribeTopic.c_str());
        }
    }

    void getlayoutHOMEd()
    {
        if (mqttIsConnect() && HOMEd)
        {
            String devName = jsonReadStr(settingsFlashJson, F("name"));

            // auto file = seekFile("layout.json");
            auto file = seekFile("config.json");
            if (!file)
            {
                SerialPrint("E", F("MQTT"), F("no file config.json"));
                return;
            }
            size_t size = file.size();
            DynamicJsonDocument doc(size * 2);
            DeserializationError error = deserializeJson(doc, file);
            if (error)
            {
                SerialPrint("E", F("MQTT"), error.f_str());
                jsonWriteInt(errorsHeapJson, F("jse3"), 1); // Ошибка чтения json файла с виджетами при отправки в mqtt
            }
            int i = 0;
            // String path = jsonReadStr(settingsFlashJson, F("HOMEd"));
            JsonArray arr = doc.as<JsonArray>();
            String HOMEdJSON = "";
            HOMEdJSON = "{\"action\":\"updateDevice\",";
            HOMEdJSON = HOMEdJSON + "\"device\":\"" + ChipId + "\",";
            HOMEdJSON = HOMEdJSON + "\"data\":{";
            HOMEdJSON = HOMEdJSON + "\"active\": true,";
            HOMEdJSON = HOMEdJSON + "\"cloud\": false,";
            HOMEdJSON = HOMEdJSON + "\"discovery\": false,";
            HOMEdJSON = HOMEdJSON + "\"id\":\"" + ChipId + "\",";
            HOMEdJSON = HOMEdJSON + "\"name\":\"" + devName + "\",";
            HOMEdJSON = HOMEdJSON + "\"real\":true,";
            HOMEdJSON = HOMEdJSON + "\"exposes\": [";
            String options = "";
            String bindings = "";
            int switchCount = 0;
            for (JsonVariant value : arr)
            {
                // print value
                // SerialPrint("i", F("!!!!!!!"), String(i) + " " + value["widget"].as<String>() + " " + value["topic"].as<String>() + " " + value["descr"].as<String>());
                String name = value["descr"];
                // String device = selectToMarkerLast(value["topic"].as<String>(), "/");
                String device = value["id"];
                String expose = value["widget"];
                if (value["widget"].as<String>() == "toggle")
                {
                    switchCount++;
                    // Создаем toggle
                    if (_toggle)
                    {
                        HOMEdJSON = HOMEdJSON + "\"" + device + "\",";
                        options = options + "\"" + device + "\":{\"title\": \"" + name + "\",\"type\": \"toggle\"},";
                        bindings = bindings + "\"" + device + "\":{\"inTopic\": \"" + mqttRootDevice + "/" + device + "/status\", \"inPattern\": \"{{ true if json.status == 1 else false }}\", \"outTopic\": \"" + mqttRootDevice + "/" + device + "/control\", \"outPattern\": \"{{ 1 if value == true else 0 }}\"},";
                    }
                    if (_switch)
                    {
                        HOMEdJSON = HOMEdJSON + "\"switch_" + switchCount + "\",";
                        options = options + "\"switch_" + switchCount + "\":{\"title\": \"" + name + "\"},";
                        bindings = bindings + "\"status_" + switchCount + "\":{\"inTopic\": \"" + mqttRootDevice + "/" + device + "/status\", \"inPattern\": \"{{ on if json.status == 1 else off }}\", \"outTopic\": \"" + mqttRootDevice + "/" + device + "/control\", \"outPattern\": \"{{ 1 if value == on else 0 }}\"},";
                    }
                }
                else if (value["widget"].as<String>() == "inputDgt" || value["widget"].as<String>() == "inputTxt" || value["widget"].as<String>() == "inputTm" || value["widget"].as<String>() == "range")
                {
                    HOMEdJSON = HOMEdJSON + "\"" + device + "\",";
                    bindings = bindings + "\"" + device + "\":{\"inTopic\": \"" + mqttRootDevice + "/" + device + "/status\", \"inPattern\": \"{{ json.status }}\", \"outTopic\": \"" + mqttRootDevice + "/" + device + "/control\"},";
                    // options = options + "\"" + device + "\":{\"title\": \"" + name + "\"},";
                    options = options + "\"" + device + "\":{\"title\": \"" + name + "\",\"type\": \"number\", \"min\": -10000, \"max\": 100000, \"step\": 0.1, \"round\": " + value["round"].as<String>() + "},";
                }
                else if (value["widget"].as<String>() == "anydataTmp")
                {
                    HOMEdJSON = HOMEdJSON + "\"" + device + "\",";
                    bindings = bindings + "\"" + device + "\":{\"inTopic\": \"" + mqttRootDevice + "/" + device + "/status\", \"inPattern\": \"{{ json.status }}\"},";
                    options = options + "\"" + device + "\":{\"title\": \"" + name + "\",\"type\": \"sensor\", \"class\": \"temperature\", \"state\": \"measurement\", \"unit\": \"°C\", \"round\": " + value["round"].as<String>() + "},";
                }
                else if (value["widget"].as<String>() == "anydataHum")
                {
                    HOMEdJSON = HOMEdJSON + "\"" + device + "\",";
                    bindings = bindings + "\"" + device + "\":{\"inTopic\": \"" + mqttRootDevice + "/" + device + "/status\", \"inPattern\": \"{{ json.status }}\"},";
                    options = options + "\"" + device + "\":{\"title\": \"" + name + "\",\"type\": \"sensor\", \"class\": \"humidity\", \"state\": \"measurement\", \"unit\": \"%\", \"round\": " + value["round"].as<String>() + "},";
                }
                else if (value["widget"].as<String>() != "nil" && value["widget"].as<String>() != "" && value["widget"].as<String>())
                {
                    HOMEdJSON = HOMEdJSON + "\"" + device + "\",";
                    bindings = bindings + "\"" + device + "\":{\"inTopic\": \"" + mqttRootDevice + "/" + device + "/status\", \"inPattern\": \"{{ json.status }}\"},";
                    options = options + "\"" + device + "\":{\"title\": \"" + name + "\"},";
                }

                i++;
            }
            options = options.substring(0, options.length() - 1);
            bindings = bindings.substring(0, bindings.length() - 1);
            HOMEdJSON = HOMEdJSON.substring(0, HOMEdJSON.length() - 1);
            HOMEdJSON = HOMEdJSON + "],";
            HOMEdJSON = HOMEdJSON + " \"options\": {" + options + "},";
            HOMEdJSON = HOMEdJSON + " \"bindings\": {" + bindings + "},";
            HOMEdJSON = HOMEdJSON + " \"availabilityTopic\": \"" + mqttRootDevice + "/state\",";
            HOMEdJSON = HOMEdJSON + " \"availabilityPattern\": \"{{ json.status }}\" ";

            HOMEdJSON = HOMEdJSON + "}}";
            String topic = (HOMEdTopic + "/command/custom").c_str();
            if (!publish(topic, HOMEdJSON))
            {
                SerialPrint("E", F("MQTT"), F("Failed publish  data to HOMEd"));
            }

            file.close();

            publishRetain(mqttRootDevice + "/state", "{\"status\":\"online\"}");

            for (std::list<IoTItem *>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it)
            {
                if ((*it)->iAmLocal)
                {
                    publishStatusMqtt((*it)->getID(), (*it)->getValue());
                    (*it)->onMqttWsAppConnectEvent();
                }
            }
        }
    }
    void deleteFromHOMEd()
    {
        if (mqttIsConnect() && HOMEd)
        {
            for (std::list<IoTItem *>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it)
            {
                if (*it)
                {
                    String id_widget = (*it)->getID().c_str();
                    String HOMEdjson = "";
                    HOMEdjson = "{\"action\":\"removeDevice\",";
                    HOMEdjson = HOMEdjson + "\"device\":\"";
                    HOMEdjson = HOMEdjson + ChipId;
                    HOMEdjson = HOMEdjson + "\"}";
                    String topic = (HOMEdTopic + "/command/custom").c_str();
                    if (!publish(topic, HOMEdjson))
                    {
                        SerialPrint("E", F("MQTT"), F("Failed remove from HOMEd"));
                    }
                }
            }
        }
    }
    IoTDiscovery *getHOMEdDiscovery()
    {
        if (HOMEd)
            return this;
        else
            return nullptr;
    }
    ~DiscoveryHomeD() {};
};

void *getAPI_DiscoveryHomeD(String subtype, String param)
{
    if (subtype == F("DiscoveryHomeD"))
    {
        return new DiscoveryHomeD(param);
    }
    else
    {
        return nullptr;
    }
}
