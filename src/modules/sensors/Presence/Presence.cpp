// Licensed under the Cooperative Non-Violent Public License (CNPL)
// See: https://github.com/CHE77/IoTManager-Modules/blob/main/LICENSE


#define MQTT_MAX_PACKET_SIZE 512  // или 1024
#include "Global.h"
#include "classes/IoTItem.h"

#include <math.h>
#define EARTH_RADIUS_KM 6371.0 // Радиус Земли в километрах

#include "NTP.h"
#include <PubSubClient.h> // чтобы знать тип
extern PubSubClient mqtt; // объявляем глобальный объект

class Presence : public IoTItem
{
private:
    String _MAC;
    String _parameter;
    IoTItem *tmp;
    int _minutesPassed = 0;
    String json = "{}";
    int orange = 0;
    int red = 0;
    int offline = 0;
    bool dataFromNode = false;
    String _topic = "";
    bool _isJson;
    bool _ticker = true;
    bool _debug = false;
    bool sendOk = false;
    float _lat_A = 0;
    float _lon_A = 0;


    struct PresenceData
    {
        String chargingState;
        String plugState;
        String connectedWifi;
        String geoLocation;
        float lat = 0.0;
        float lon = 0.0;
        unsigned long geoTimestamp = 0;
        String geoTime;
        String deviceName;
        int batteryLevel = -1;
        unsigned long currentTimestamp = 0;
        String currentTime;
        unsigned long nextScheduledTimestamp = 0;
        String nextScheduledTime;
        unsigned long nextAlarmclockTimestamp = 0;
        String nextAlarmclockTime;
        std::vector<String> conditionContent;
        String conditionContentString;
    };
    PresenceData pdata;

public:
    Presence(String parameters) : IoTItem(parameters)
    {
        _parameter = jsonReadStr(parameters, "parameter");
        jsonRead(parameters, F("orange"), orange);
        jsonRead(parameters, F("red"), red);
        jsonRead(parameters, F("offline"), offline);
        _topic = jsonReadStr(parameters, "topic");
        if (_debug)
            SerialPrint("i", "Presence topic :  ",  _topic);
        jsonRead(parameters, F("isJson"), _isJson);
        // jsonRead(parameters, "addPrefix", _addPrefix);
        jsonRead(parameters, F("Lat. A"), _lat_A);
        jsonRead(parameters, F("Long. A"), _lon_A);
        jsonRead(parameters, F("ticker"), _ticker);
        jsonRead(parameters, F("debug"), _debug);
        dataFromNode = false;
        if (mqttIsConnect())
        {
            mqtt.setBufferSize(512); 
            sendOk = true;
            mqttSubscribeExternal(_topic);
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

    double toRadians(double degrees)
    {
        return degrees * M_PI / 180.0;
    }

    double toDegrees(double radians)
    {
        return radians * 180.0 / M_PI;
    }

    // Возвращает пеленг в градусах: от 0 до 360
    double calculateInitialBearing(double lat1, double lon1, double lat2, double lon2)
    {
        lat1 = toRadians(lat1);
        lon1 = toRadians(lon1);
        lat2 = toRadians(lat2);
        lon2 = toRadians(lon2);

        double deltaLon = lon2 - lon1;

        double y = sin(deltaLon) * cos(lat2);
        double x = cos(lat1) * sin(lat2) -
                   sin(lat1) * cos(lat2) * cos(deltaLon);

        double bearing = atan2(y, x);
        bearing = toDegrees(bearing);

        // Приводим к диапазону 0–360
        return fmod((bearing + 360.0), 360.0);
    }

    // lat и lon — в градусах
    double haversineDistance(double lat1, double lon1, double lat2, double lon2)
    {
        double dLat = toRadians(lat2 - lat1);
        double dLon = toRadians(lon2 - lon1);

        lat1 = toRadians(lat1);
        lat2 = toRadians(lat2);

        double a = sin(dLat / 2) * sin(dLat / 2) +
                   cos(lat1) * cos(lat2) *
                       sin(dLon / 2) * sin(dLon / 2);

        double c = 2 * atan2(sqrt(a), sqrt(1 - a));

        return EARTH_RADIUS_KM * c * 1000.0;
    }

    void onMqttRecive(String &topic, String &msg)
    {
        Serial.printf("[MQTT] Topic: %s\nPayload size: %d bytes\n", topic.c_str(), msg.length());
        msg.trim(); // Убираем пробелы и переносы строк

        if (msg.indexOf("HELLO") == -1)
        {
            if (_debug)
                SerialPrint("i", "Presence HELLO", "  _1d: " + _id + "  topic: " + topic + " msg: " + msg);
            String dev = selectToMarkerLast(topic, "/");
            dev.toUpperCase();
            dev.replace(":", "");
            if (_topic != topic)
            {
                if (_debug)
                {
                    SerialPrint("i", "Presence", topic + " not equal: " + _topic + " msg: " + msg);
                }
                return;
            }

            if (_isJson)
            {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, msg);

                if (err)
                {
                    SerialPrint("E", F("Presence"), err.f_str());
                    return;
                }

                JsonObject obj = doc.as<JsonObject>();

                if (obj.containsKey("chargingState"))
                {
                    pdata.chargingState = obj["chargingState"].as<String>();
                }
                if (obj.containsKey("plugState"))
                {
                    pdata.plugState = obj["plugState"].as<String>();
                }

                if (obj.containsKey("connectedWifi"))
                    pdata.connectedWifi = obj["connectedWifi"].as<String>();
                if (obj.containsKey("geoLocation"))
                {
                    pdata.geoLocation = obj["geoLocation"].as<String>();
                    // Разбор геолокации после получения
                    parseGeo(pdata.geoLocation);
                    pdata.geoTime = getDateTimeDotFormatedFromUnix(pdata.geoTimestamp);
                    SerialPrint("i", "Presence", "GeoTime : " + pdata.geoTime);
                }

                if (obj.containsKey("deviceName"))
                    pdata.deviceName = obj["deviceName"].as<String>();
                if (obj.containsKey("batteryLevel"))
                    pdata.batteryLevel = obj["batteryLevel"].as<int>();
                if (obj.containsKey("currentTimestamp"))
                {
                    pdata.currentTimestamp = obj["currentTimestamp"].as<unsigned long>();
                    pdata.currentTime = getDateTimeDotFormatedFromUnix(pdata.currentTimestamp);
                }

                if (obj.containsKey("nextScheduledTimestamp"))
                {
                    pdata.nextScheduledTimestamp = obj["nextScheduledTimestamp"].as<unsigned long>();
                    pdata.nextScheduledTime = getDateTimeDotFormatedFromUnix(pdata.nextScheduledTimestamp);
                }

                if (obj.containsKey("nextAlarmclockTimestamp"))
                {
                    pdata.nextAlarmclockTimestamp = obj["nextAlarmclockTimestamp"].as<unsigned long>();
                    pdata.nextAlarmclockTime = getDateTimeDotFormatedFromUnix(pdata.nextAlarmclockTimestamp);
                }

                if (obj.containsKey("conditionContent"))
                {
                    JsonArray arr = obj["conditionContent"].as<JsonArray>();
                    pdata.conditionContent.clear();
                    for (String s : arr)
                    {
                        pdata.conditionContent.push_back(s);
                    }

                    String conditionStr;
                    for (size_t i = 0; i < pdata.conditionContent.size(); ++i)
                    {
                        conditionStr += pdata.conditionContent[i];
                        if (i < pdata.conditionContent.size() - 1)
                            conditionStr += ", ";
                    }
                    pdata.conditionContentString = conditionStr;
                }

                dataFromNode = true;
                _minutesPassed = 0;

                String sensorVal;

                if (_parameter == "latitude")
                {
                    value.isDecimal = true;
                    value.valD = pdata.lat;
                }
                else if (_parameter == "longitude")
                {
                    value.isDecimal = true;
                    value.valD = pdata.lon;
                }
                else if (_parameter == "azimuth")
                {
                    value.isDecimal = true;
                    value.valD = calculateInitialBearing(_lat_A, _lon_A, pdata.lat, pdata.lon);
                }
                else if (_parameter == "distance")
                {
                    value.isDecimal = true;
                    value.valD = haversineDistance(_lat_A, _lon_A, pdata.lat, pdata.lon);
                }
                else if (_parameter == "batteryLevel")
                {
                    value.isDecimal = true;
                    value.valD = pdata.batteryLevel;
                }
                else if (_parameter == "geoTime")
                {
                    value.isDecimal = false;
                    value.valS = pdata.geoTime;
                }
                else if (_parameter == "geoTimestamp")
                {
                    value.isDecimal = false;
                    value.valS = pdata.geoTimestamp;
                }
                else if (_parameter == "currentTime")
                {
                    value.isDecimal = false;
                    value.valS = pdata.currentTime;
                }
                else if (_parameter == "nextScheduledTime")
                {
                    value.isDecimal = false;
                    value.valS = pdata.nextScheduledTime;
                }
                else if (_parameter == "nextAlarmclockTime")
                {
                    value.isDecimal = false;
                    value.valS = pdata.nextAlarmclockTime;
                }
                else if (_parameter == "conditionContent")
                {
                    value.isDecimal = false;
                    value.valS = pdata.conditionContentString;
                }
                else if (obj.containsKey(_parameter) && obj[_parameter].is<const char *>())
                {
                    sensorVal = obj[_parameter].as<const char *>();
                    value.isDecimal = false;
                    value.valS = sensorVal;
                }
                else
                {
                    value.isDecimal = false;
                    value.valS = "parameter mismatch";
                }

                if (value.isDecimal)
                {
                    regEvent(value.valD, F("Presence"), _debug, _ticker);
                }
                else
                {
                    regEvent(value.valS, F("Presence"), _debug, _ticker);
                }

            }
            else
            {
                if (_debug)
                {
                    SerialPrint("i", "Presence", "Received MAC: " + dev + " val=" + msg);
                }
                dataFromNode = true;
                _minutesPassed = 0;
                setValue(msg);
            }
        }
    }

    IoTValue execute(String command, std::vector<IoTValue> &param)
    {
        IoTValue valTmp;

        if (command == "latitude")
        {
            valTmp.isDecimal = true;
            valTmp.valD = pdata.lat;
            return valTmp;
        }
        else if (command == "longitude")
        {
            valTmp.isDecimal = true;
            valTmp.valD = pdata.lon;
            return valTmp;
        }
        else if (command == "azimuth")
        {
            if (param.size() == 2 && param[0].isDecimal && param[1].isDecimal)
            {
                valTmp.isDecimal = true;
                valTmp.valD = calculateInitialBearing(param[0].valD, param[1].valD, pdata.lat, pdata.lon);
            }
            else
            {
                valTmp.isDecimal = false;
                valTmp.valS = "wrong parameters";
            }
            return valTmp;
        }
        else if (command == "distance")
        {
            if (param.size() == 2 && param[1].isDecimal && param[1].isDecimal)
            {
                valTmp.isDecimal = true;
                valTmp.valD = haversineDistance(param[0].valD, param[1].valD, pdata.lat, pdata.lon);
            }
            else
            {
                valTmp.isDecimal = false;
                valTmp.valS = "wrong parameters";
            }
            return valTmp;
        }
        else if (command == "batteryLevel")
        {
            valTmp.isDecimal = true;
            valTmp.valD = pdata.batteryLevel;
            return valTmp;
        }
        else if (command == "geoTime")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.geoTime;
            return valTmp;
        }
        else if (command == "geoTimestamp")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.geoTimestamp;
            return valTmp;
        }
        else if (command == "currentTime")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.currentTime;
            return valTmp;
        }
        else if (command == "currentTimestamp")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.currentTimestamp;
            return valTmp;
        }
        else if (command == "nextScheduledTime")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.nextScheduledTime;
            return valTmp;
        }
        else if (command == "nextScheduledTimestamp")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.nextScheduledTimestamp;
            return valTmp;
        }
        else if (command == "nextAlarmclockTime")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.nextAlarmclockTime;
            return valTmp;
        }
        else if (command == "nextAlarmclockTimestamp")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.nextAlarmclockTimestamp;
            return valTmp;
        }
        else if (command == "conditionContent")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.conditionContentString;
            return valTmp;
        }
        else if (command == "chargingState")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.chargingState;
            return valTmp;
        }
        else if (command == "plugState")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.plugState;
            return valTmp;
        }
        else if (command == "connectedWifi")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.connectedWifi;
            return valTmp;
        }
        else if (command == "deviceName")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.deviceName;
            return valTmp;
        }
        else if (command == "geoLocation")
        {
            valTmp.isDecimal = false;
            valTmp.valS = pdata.geoLocation;
            return valTmp;
        }
        else
        {
            valTmp.isDecimal = false;
            valTmp.valS = "wrong command";
            return valTmp;
        }

        return {};
    }

    String getMqttExterSub()
    {
        return _topic;
    }

    void doByInterval()
    {
        _minutesPassed++;
        setNewWidgetAttributes();
        if (mqttIsConnect() && !sendOk)
        {
            mqtt.setBufferSize(512); // ← ДО подписи
            sendOk = true;
            mqttSubscribeExternal(_topic);
        }
    }
    void onMqttWsAppConnectEvent()
    {
        setNewWidgetAttributes();
    }

    void setNewWidgetAttributes()
    {

        jsonWriteStr(json, F("info"), prettyMinutsTimeout(_minutesPassed));
        if (dataFromNode)
        {
            if (orange != 0 && red != 0 && offline != 0)
            {
                if (_minutesPassed < orange)
                {
                    jsonWriteStr(json, F("color"), "");
                }
                if (_minutesPassed >= orange && _minutesPassed < red)
                {
                    jsonWriteStr(json, F("color"), F("orange")); // сделаем виджет оранжевым
                }
                if (_minutesPassed >= red && _minutesPassed < offline)
                {
                    jsonWriteStr(json, F("color"), F("red")); // сделаем виджет красным
                }
                if (_minutesPassed >= offline)
                {
                    jsonWriteStr(json, F("info"), F("offline"));
                    SerialPrint("i", "Presence", _id + " - offline");
                }
            }
        }
        else
        {
            jsonWriteStr(json, F("info"), F("awaiting"));
        }
        // SerialPrint("i", "JSON", json);
        sendSubWidgetsValues(_id, json);
    }

    void parseGeo(const String &geo)
    {
        if (!geo.startsWith("geo:"))
            return;

        int commaIndex = geo.indexOf(',', 4);
        int semicolonIndex = geo.indexOf(';', commaIndex);

        if (commaIndex == -1 || semicolonIndex == -1)
            return;

        String latStr = geo.substring(4, commaIndex);
        String lonStr = geo.substring(commaIndex + 1, semicolonIndex);

        // timestamp
        String tsTag = "timestamp=";
        int tsIndex = geo.indexOf(tsTag);
        String tsStr = (tsIndex > 0) ? geo.substring(tsIndex + tsTag.length()) : "";

        pdata.lat = latStr.toFloat();
        pdata.lon = lonStr.toFloat();
        pdata.geoTimestamp = tsStr.toInt();

        if (_debug)
        {
            SerialPrint("i", "Presence", "Lat: " + String(pdata.lat, 6));
            SerialPrint("i", "Presence", "Lon: " + String(pdata.lon, 6));
            SerialPrint("i", "Presence", "Geo timestamp: " + String(pdata.geoTimestamp));
        }
    }

    ~Presence() {};
};

void *getAPI_Presence(String subtype, String param)
{
    if (subtype == F("Presence"))
    {
        return new Presence(param);
    }
    else
    {
        return nullptr;
    }
}
