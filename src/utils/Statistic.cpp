

#include "utils/Statistic.h"
#include <Arduino.h>

#include "esp_mac.h"

void stInit()
{
    if (TELEMETRY_UPDATE_INTERVAL_MIN)
    {
        ts.add(
            ST, TELEMETRY_UPDATE_INTERVAL_MIN * 60000, [&](void *)
            { updateDeviceStatus(); },
            nullptr, true);
    }
    SerialPrint("i", F("Stat"), F("Stat Init"));
}

void updateDeviceStatus()
{
    String ret;
    String serverIP = "http://live-control.com";
    jsonRead(settingsFlashJson, F("serverip"), serverIP);
    String url = serverIP + F("/projects/esprebootstat.php");
    // SerialPrint("i", "Stat", "url " + url);
    if ((isNetworkActive()))
    {
        uint8_t baseMac[6];
        char macStr[18]; // Буфер для MAC адреса (17 символов + null terminator)
// Базовый MAC адрес
#ifdef esp32c6_4mb
        esp_err_t err = esp_base_mac_addr_get(baseMac);
// else if
#elif defined(esp32c6_8mb)
        esp_err_t err = esp_base_mac_addr_get(baseMac);
#else
        esp_err_t err = esp_efuse_mac_get_default(baseMac);
#endif
        if (err == ESP_OK)
        {
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                     baseMac[0], baseMac[1], baseMac[2],
                     baseMac[3], baseMac[4], baseMac[5]);

            //            SerialPrint("i", "Stat", "Base MAC: " + String(macStr));
        }
        WiFiClient client;
        HTTPClient http;
        http.begin(client, url);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        String httpRequestData = "idesp=" + String(macStr) +
                                 "&nameesp=" + jsonReadStr(settingsFlashJson, F("name")) +
                                 "&firmwarename=" + String(FIRMWARE_NAME) +
                                 "&firmwarever=" + String(FIRMWARE_VERSION) +
                                 "&rebootreason=" + ESP_getResetReason() +
                                 "&uptime=" + jsonReadStr(errorsHeapJson, F("upt"));
        int httpResponseCode = http.POST(httpRequestData);

        if (httpResponseCode > 0)
        {
            ret = http.errorToString(httpResponseCode).c_str();
            if (httpResponseCode == HTTP_CODE_OK)
            {
#ifndef LIBRETINY
                String payload = http.getString();
#else
                String payload = httpGetString(http);
#endif
                // SerialPrint("i", "Stat", "Update device data: " + String(macStr) + " " + ret);
                if (payload == "{\"status\":\"falsification\"}")
                {
                    jsonWriteStr_(settingsFlashJson, "control", "fail");
                    SerialPrint("E", "License", "not found: " + String(macStr));
                }
                else
                {
                    jsonWriteStr_(settingsFlashJson, "control", "");
                }
                ret += " " + payload;
            }
        }
        else
        {
            ret = http.errorToString(httpResponseCode).c_str();
        }
        http.end();
    }
    // SerialPrint("i", "Stat", "Update device data: " + ret);
}