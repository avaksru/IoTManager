#include "Global.h"
#include "classes/IoTDiscovery.h"
class DiscoverySprut : public IoTDiscovery
{
private:
    String _topic = "";
    bool Sprut = false;
    String ChipId = getChipId();
    String SprutTopic;

public:
    DiscoverySprut(String parameters) : IoTDiscovery(parameters)
    {
        _topic = jsonReadStr(parameters, "topic");

        if (_topic && _topic != "" && _topic != "null")
        {
            Sprut = true;
            SprutTopic = _topic;
            //     SerialPrint("i", "Sprut", "topic " + mqttRootDevice);
            getlayoutSprut();
        }
    }

    void onMqttRecive(String &topic, String &payloadStr)
    {
        if (!Sprut)
            return;
    }

    void doByInterval()
    {
    }

    void mqttSubscribeDiscovery()
    {
        if (mqttIsConnect() && Sprut)
        {
            getlayoutSprut();
            publishRetain(mqttRootDevice + "/state", "{\"status\":\"online\"}");
        }
    }

    String removeSpaces(String str)
    {
        int startIndex = 0;
        // Пропускаем все пробелы в начале строки
        while (startIndex < str.length() && str[startIndex] == ' ')
        {
            startIndex++;
        }
        // Возвращаем строку, начиная с первого не пробельного символа
        return str.substring(startIndex);
    }

    void getlayoutSprut()
    {
        if (mqttIsConnect() && Sprut)
        {
            String devName = jsonReadStr(settingsFlashJson, F("name"));

            auto file = seekFile("layout.json");
            if (!file)
            {
                SerialPrint("E", F("MQTT"), F("no file layout.json"));
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
            // String path = jsonReadStr(settingsFlashJson, F("Sprut"));
            JsonArray arr = doc.as<JsonArray>();
            String SprutJSON = "";
            SprutJSON = "{\"manufacturer\":\"SmartKot\",";
            SprutJSON = SprutJSON + "\"model\":\"" + devName + "\",";
            SprutJSON = SprutJSON + "\"modelId\":\"" + ChipId + "\"";

            String devises = "";
            String options = "";
            //-------------------термостаты opentherm----------------//

            // devises = devises + "{\"type\" : \"Thermostat\",\"name\" : \"Отопление\", \"logics\" : [{\"type\" : \"CurrentHeatingCoolingFromTarget\"}],";
            devises = devises + "{\"type\" : \"Thermostat\",\"name\" : \"Отопление\",";
            devises = devises + "\"characteristics\" : [";
            devises = devises + "{\"type\" : \"CurrentTemperature\",";
            devises = devises + "\"link\" : { \"type\": \"Integer\",";
            devises = devises + "\"topicGet\": \"" + mqttRootDevice + "/OTget25/status\",";
            devises = devises + "\"inFunc\": \"JSON.parse(value).status\"}},";

            devises = devises + "{\"type\" : \"TargetTemperature\",";
            devises = devises + "\"link\" : { \"type\": \"Integer\",";
            devises = devises + "\"topicGet\": \"" + mqttRootDevice + "/OTset1/status\",";
            devises = devises + "\"inFunc\": \"JSON.parse(value).status\",";
            devises = devises + "\"topicSet\": \"" + mqttRootDevice + "/OTset1/control\"},";
            devises = devises + "\"minValue\": 0.0,";
            devises = devises + "\"maxValue\": 82.0,";
            devises = devises + "\"minStep\": 1.0},";

            devises = devises + "{\"type\" : \"CurrentHeatingCoolingState\",";
            devises = devises + "\"link\" : { \"type\": \"String\",";
            devises = devises + "\"topicGet\": \"" + mqttRootDevice + "/isHeatingEnabled/status\",";
            devises = devises + "\"inFunc\": \"JSON.parse(value).status\",";
            devises = devises + "\"topicSet\": \"" + mqttRootDevice + "/isHeatingEnabled/control\",";
            //            devises = devises + "\"map\": {\"OFF\": \"0\",\"HEAT\": \"1\"}, \"validValues\": \"OFF, HEAT\"}, ";
            devises = devises + "\"map\": {\"OFF\": \"➖\",\"HEAT\": \"✅\"},\"outMap\": {\"OFF\": \"0\",\"HEAT\": \"1\"}}, \"validValues\": \"OFF, HEAT\"}, ";
            devises = devises + "{\"type\": \"TargetHeatingCoolingState\",\"validValues\": \"HEAT\",\"option\": true},";
            devises = devises + "{\"type\": \"TemperatureDisplayUnits\",\"validValues\": \"CELSIUS\",\"option\": true}]},";

            // devises = devises + "{\"type\" : \"Thermostat\",\"name\" : \"ГВС\", \"logics\" : [{\"type\" : \"CurrentHeatingCoolingFromTarget\"}],";
            devises = devises + "{\"type\" : \"Thermostat\",\"name\" : \"ГВС\", ";
            devises = devises + "\"characteristics\" : [";
            devises = devises + "{\"type\" : \"CurrentTemperature\",";
            devises = devises + "\"link\" : { \"type\": \"Integer\",";
            devises = devises + "\"topicGet\": \"" + mqttRootDevice + "/OTget26/status\",";
            devises = devises + "\"inFunc\": \"JSON.parse(value).status\"}},";

            devises = devises + "{\"type\" : \"TargetTemperature\",";
            devises = devises + "\"link\" : { \"type\": \"Integer\",";
            devises = devises + "\"topicGet\": \"" + mqttRootDevice + "/OTset56/status\",";
            devises = devises + "\"inFunc\": \"JSON.parse(value).status\",";
            devises = devises + "\"topicSet\": \"" + mqttRootDevice + "/OTset56/control\"},";
            devises = devises + "\"minValue\": 0.0,";
            devises = devises + "\"maxValue\": 82.0,";
            devises = devises + "\"minStep\": 1.0},";

            devises = devises + "{\"type\" : \"CurrentHeatingCoolingState\",";
            devises = devises + "\"link\" : { \"type\": \"String\",";
            devises = devises + "\"topicGet\": \"" + mqttRootDevice + "/isDHWenabled/status\",";
            devises = devises + "\"inFunc\": \"JSON.parse(value).status\",";
            devises = devises + "\"topicSet\": \"" + mqttRootDevice + "/isDHWenabled/control\",";
            devises = devises + "\"map\": {\"OFF\": \"➖\",\"HEAT\": \"✅\"},\"outMap\": {\"OFF\": \"0\",\"HEAT\": \"1\"}}, \"validValues\": \"OFF, HEAT\"}, ";
            devises = devises + "{\"type\": \"TargetHeatingCoolingState\",\"validValues\": \"HEAT\",\"option\": true},";
            devises = devises + "{\"type\": \"TemperatureDisplayUnits\",\"validValues\": \"CELSIUS\",\"option\": true}]},";

            // devises = devises + "{\"type\" : \"Thermostat\",\"name\" : \"Комнатный термостат\", \"logics\" : [{\"type\" : \"CurrentHeatingCoolingFromTarget\"}],";
            devises = devises + "{\"type\" : \"Thermostat\",\"name\" : \"Комнатный термостат\", ";
            devises = devises + "\"characteristics\" : [";
            devises = devises + "{\"type\" : \"CurrentTemperature\",";
            devises = devises + "\"link\" : { \"type\": \"Double\",";
            devises = devises + "\"topicGet\": \"" + mqttRootDevice + "/DS18B20room/status\",";
            devises = devises + "\"inFunc\": \"JSON.parse(value).status\"}},";

            devises = devises + "{\"type\" : \"TargetTemperature\",";
            devises = devises + "\"link\" : { \"type\": \"Double\",";
            devises = devises + "\"topicGet\": \"" + mqttRootDevice + "/setTemperature/status\",";
            devises = devises + "\"inFunc\": \"JSON.parse(value).status\",";
            devises = devises + "\"topicSet\": \"" + mqttRootDevice + "/setTemperature/control\"},";
            devises = devises + "\"minValue\": 0.0,";
            devises = devises + "\"maxValue\": 35.0,";
            devises = devises + "\"minStep\": 0.1},";

            devises = devises + "{\"type\" : \"CurrentHeatingCoolingState\",";
            devises = devises + "\"link\" : { \"type\": \"Double\",";
            devises = devises + "\"topicGet\": \"" + mqttRootDevice + "/auto/status\",";
            devises = devises + "\"inFunc\": \"JSON.parse(value).status\",";
            devises = devises + "\"topicSet\": \"" + mqttRootDevice + "/auto/control\",";
            devises = devises + "\"map\": {\"OFF\": \"0\",\"HEAT\": \"1\"}}, \"validValues\": \"OFF, HEAT\"}, ";
            devises = devises + "{\"type\": \"TargetHeatingCoolingState\",\"validValues\": \"HEAT\",\"option\": true},";
            devises = devises + "{\"type\": \"TemperatureDisplayUnits\",\"validValues\": \"CELSIUS\",\"option\": true}]},";

            //-------------------термостаты--------------------------//

            for (JsonVariant value : arr)
            {

                String name = value["descr"];
                name.replace("/", "_");
                name.replace("\\", "_");
                name = removeSpaces(name);
                String device = "";
                String option = "";
                String id = selectToMarkerLast(value["topic"].as<String>(), "/");
                String characteristics = "";
                String link = "";
                // String expose = value["name"];

                device = device + "\"name\": \"" + name + "\",";
                if (value["name"].as<String>() == "toggle")
                {
                    device = device + "\"type\": \"Switch\",";
                    characteristics = characteristics + "\"type\": \"On\",";
                    link = link + "\"type\": \"String\",";
                    link = link + "\"topicGet\": \"" + mqttRootDevice + "/" + id + "/status\",";
                    link = link + "\"inFunc\": \"JSON.parse(value).status\",";
                    link = link + "\"topicSet\": \"" + mqttRootDevice + "/" + id + "/control\",";
                    link = link + "\"map\": {\"false\": 0,\"true\": 1},";

                    link = link.substring(0, link.length() - 1);
                    devises = devises + "{" + device + "\"characteristics\": [{" + characteristics + "\"link\": {" + link + "}}]},";
                }

                else if (value["name"].as<String>() == "inputDgt")
                {
                    option = option + "\"type\": \"Double\",";
                    // option = option + "\"write\": true,";
                    option = option + "\"inputType\": \"STATUS\",";
                    option = option + "\"name\": \"" + name + "\",";
                    link = link + "\"type\": \"String\",";
                    link = link + "\"topicGet\": \"" + mqttRootDevice + "/" + id + "/status\",";
                    link = link + "\"inFunc\": \"JSON.parse(value).status\",";
                    link = link + "\"topicSet\": \"" + mqttRootDevice + "/" + id + "/control\",";
                    options = options + "{" + option + "\"link\": {" + link + "}},";
                }
                else if (value["name"].as<String>() == "inputTxt")
                {
                    option = option + "\"type\": \"String\",";
                    // option = option + "\"write\": true,";
                    option = option + "\"inputType\": \"STATUS\",";
                    option = option + "\"name\": \"" + name + "\",";
                    link = link + "\"type\": \"String\",";
                    link = link + "\"topicGet\": \"" + mqttRootDevice + "/" + id + "/status\",";
                    link = link + "\"inFunc\": \"JSON.parse(value).status\",";
                    link = link + "\"topicSet\": \"" + mqttRootDevice + "/" + id + "/control\",";
                    options = options + "{" + option + "\"link\": {" + link + "}},";
                }
                else if (value["name"].as<String>() == "anydataTmp")
                {
                    device = device + "\"type\": \"TemperatureSensor\",";
                    characteristics = characteristics + "\"type\": \"CurrentTemperature\",";
                    link = link + "\"type\": \"Double\",";
                    link = link + "\"topicGet\": \"" + mqttRootDevice + "/" + id + "/status\",";
                    link = link + "\"inFunc\": \"JSON.parse(value).status\",";
                    link = link.substring(0, link.length() - 1);
                    devises = devises + "{" + device + "\"characteristics\": [{" + characteristics + "\"link\": {" + link + "}}]},";
                }
                else if (value["name"].as<String>() == "anydataHum")
                {
                    device = device + "\"type\": \"HumiditySensor\",";
                    characteristics = characteristics + "\"type\": \"CurrentRelativeHumidity\",";
                    link = link + "\"type\": \"Double\",";
                    link = link + "\"topicGet\": \"" + mqttRootDevice + "/" + id + "/status\",";
                    link = link + "\"inFunc\": \"JSON.parse(value).status\",";
                    link = link.substring(0, link.length() - 1);
                    devises = devises + "{" + device + "\"characteristics\": [{" + characteristics + "\"link\": {" + link + "}}]},";
                }
                else
                {
                    option = option + "\"type\": \"String\",";
                    option = option + "\"write\": false,";
                    option = option + "\"inputType\": \"STATUS\",";
                    option = option + "\"name\": \"" + name + "\",";
                    link = link + "\"type\": \"String\",";
                    link = link + "\"topicGet\": \"" + mqttRootDevice + "/" + id + "/status\",";
                    link = link + "\"inFunc\": \"JSON.parse(value).status\"";
                    options = options + "{" + option + "\"link\": {" + link + "}},";
                }

                i++;
            }

            devises = devises.substring(0, devises.length() - 1);
            if (devises.length() > 2)
                SprutJSON = SprutJSON + ", \"services\":[" + devises + "]";

            options = options.substring(0, options.length() - 1);
            if (options.length() > 2)
                SprutJSON = SprutJSON + ", \"options\":[" + options + "]";

            SprutJSON = SprutJSON + "}";

            String topic = (SprutTopic + "/command/custom").c_str();
            if (!publish(topic, SprutJSON))
            {
                SerialPrint("E", F("MQTT"), F("Failed publish  data to Sprut"));
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

    IoTDiscovery *getSprutDiscovery()
    {
        if (Sprut)
            return this;
        else
            return nullptr;
    }
    ~DiscoverySprut() {};
};

void *getAPI_DiscoverySprut(String subtype, String param)
{
    if (subtype == F("DiscoverySprut"))
    {
        return new DiscoverySprut(param);
    }
    else
    {
        return nullptr;
    }
}
