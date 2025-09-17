#include "Global.h"
#include "classes/IoTItem.h"
#include <ArduinoJson.h>

// long prevSaveConfigMillis = millis() - 60001;
// StaticJsonDocument<JSON_BUFFER_SIZE * 2> SaveConfigdoc;

extern IoTGpio IoTgpio;
class SaveConfig : public IoTItem
{
private:
public:
    SaveConfig(String parameters) : IoTItem(parameters)
    {
        getSaveConfig();
    }
    uint8_t crc8(const uint8_t *addr, uint8_t len)
    {
        uint8_t crc = 0;
        while (len--)
        {
            uint8_t inbyte = *addr++;
            for (uint8_t i = 8; i; i--)
            {
                uint8_t mix = (crc ^ inbyte) & 0x01;
                crc >>= 1;
                if (mix)
                    crc ^= 0x8C;
                inbyte >>= 1;
            }
        }
        return crc;
    }
    uint8_t getUserID()
    {

        String md = jsonReadStr(settingsFlashJson, F("mqttServer")) + jsonReadStr(settingsFlashJson, F("mqttUser")) + jsonReadStr(settingsFlashJson, F("mqttPass"));
        uint8_t myStrCrc = crc8((const uint8_t *)md.c_str(), md.length());
        return myStrCrc;
    }

    void getSaveConfig()
    {
        String ret;

        if (WiFi.status() == WL_CONNECTED)
        {
            // char c;
            String payload;
            WiFiClient client;
            HTTPClient http;
            http.begin(client, "http://live-control.com/iotm/saveESPsettings.php");
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            String httpRequestData = "idesp=" + getChipId() +
                                     "&userID=" + String(getUserID()) +
                                     "&nameesp=" + jsonReadStr(settingsFlashJson, F("name")) +
                                     "&settings=" + settingsFlashJson;
            int httpResponseCode = http.POST(httpRequestData);
            if (httpResponseCode > 0)
            {
                ret = http.errorToString(httpResponseCode).c_str();

                if (httpResponseCode == HTTP_CODE_OK)
                {
                    payload = http.getString();

                    //  deserializeJson(SaveConfigdoc, payload);
                    ret += payload;
                }
            }
            else
            {
                ret = http.errorToString(httpResponseCode).c_str();
            }
            SerialPrint("<-", F("settingsSaveConfig"), ret);

            http.end();
            httpRequestData = "";
            ret = "";
            // 2
            http.begin(client, "http://live-control.com/iotm/saveESPsettings.php");
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            httpRequestData = "idesp=" + getChipId() +
                              "&userID=" + String(getUserID()) +
                              "&nameesp=" + jsonReadStr(settingsFlashJson, F("name")) +
                              "&scenario=" + readFile("scenario.txt", 4096 * 4);

            httpResponseCode = http.POST(httpRequestData);
            if (httpResponseCode > 0)
            {
                ret = http.errorToString(httpResponseCode).c_str();

                if (httpResponseCode == HTTP_CODE_OK)
                {
                    payload = http.getString();

                    //   deserializeJson(SaveConfigdoc, payload);
                    ret += payload;
                }
            }
            else
            {
                ret = http.errorToString(httpResponseCode).c_str();
            }
            SerialPrint("<-", F("scenarioSaveConfig"), ret);

            http.end();
            httpRequestData = "";
            ret = "";
            // 3
            http.begin(client, "http://live-control.com/iotm/saveESPsettings.php");
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            httpRequestData = "idesp=" + getChipId() +
                              "&userID=" + String(getUserID()) +
                              "&nameesp=" + jsonReadStr(settingsFlashJson, F("name")) +
                              "&config=" + readFile("config.json", 4096 * 4);

            httpResponseCode = http.POST(httpRequestData);
            if (httpResponseCode > 0)
            {
                ret = http.errorToString(httpResponseCode).c_str();

                if (httpResponseCode == HTTP_CODE_OK)
                {
                    payload = http.getString();

                    //  deserializeJson(SaveConfigdoc, payload);
                    ret += payload;
                }
            }
            else
            {
                ret = http.errorToString(httpResponseCode).c_str();
            }
            // SerialPrint("<-", F("configSaveConfig"), httpRequestData);
            SerialPrint("->", F("configSaveConfig"), ret);

            http.end();
            httpRequestData = "";
            ret = "";

            // 4
            http.begin(client, "http://live-control.com/iotm/saveESPsettings.php");
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            httpRequestData = "idesp=" + getChipId() +
                              "&userID=" + String(getUserID()) +
                              "&nameesp=" + jsonReadStr(settingsFlashJson, F("name")) +
                              "&myProfile=" + readFile("myProfile.json", 4096 * 4);

            httpResponseCode = http.POST(httpRequestData);
            if (httpResponseCode > 0)
            {
                ret = http.errorToString(httpResponseCode).c_str();

                if (httpResponseCode == HTTP_CODE_OK)
                {
                    payload = http.getString();

                    //  deserializeJson(SaveConfigdoc, payload);
                    ret += payload;
                }
            }
            else
            {
                ret = http.errorToString(httpResponseCode).c_str();
            }
            // SerialPrint("<-", F("configSaveConfig"), httpRequestData);
            SerialPrint("->", F("myProfileSaveConfig"), ret);

            http.end();
            httpRequestData = "";
            ret = "";
        }
    }

    void doByInterval()
    {
    }

    ~SaveConfig(){};
};

void *getAPI_SaveConfig(String subtype, String param)
{
    if (subtype == F("SaveConfig"))
    {
        return new SaveConfig(param);
    }
    else
    {
        return nullptr;
    }
}
