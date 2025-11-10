#include "ESPConfiguration.h"

void* getAPI_Cron(String subtype, String params);
void* getAPI_DiscoveryHA(String subtype, String params);
void* getAPI_DiscoveryHomeD(String subtype, String params);
void* getAPI_Loging(String subtype, String params);
void* getAPI_LogingDaily(String subtype, String params);
void* getAPI_LogingHourly(String subtype, String params);
void* getAPI_Timer(String subtype, String params);
void* getAPI_UpdateServer(String subtype, String params);
void* getAPI_Variable(String subtype, String params);
void* getAPI_VariableColor(String subtype, String params);
void* getAPI_VButton(String subtype, String params);
void* getAPI_Weather(String subtype, String params);
void* getAPI_AnalogAdc(String subtype, String params);
void* getAPI_Ble_part1(String subtype, String params);
void* getAPI_Dht1122(String subtype, String params);
void* getAPI_Ds18b20(String subtype, String params);
void* getAPI_ExternalMQTT(String subtype, String params);
void* getAPI_Ntc(String subtype, String params);
void* getAPI_RTC(String subtype, String params);
void* getAPI_ButtonIn(String subtype, String params);
void* getAPI_ButtonOut(String subtype, String params);
void* getAPI_TelegramLT(String subtype, String params);
void* getAPI_Thermostat(String subtype, String params);

void* getAPI(String subtype, String params) {
void* tmpAPI;
if ((tmpAPI = getAPI_Cron(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_DiscoveryHA(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_DiscoveryHomeD(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_Loging(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_LogingDaily(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_LogingHourly(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_Timer(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_UpdateServer(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_Variable(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_VariableColor(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_VButton(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_Weather(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_AnalogAdc(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_Ble_part1(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_Dht1122(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_Ds18b20(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_ExternalMQTT(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_Ntc(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_RTC(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_ButtonIn(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_ButtonOut(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_TelegramLT(subtype, params)) != nullptr) return tmpAPI;
if ((tmpAPI = getAPI_Thermostat(subtype, params)) != nullptr) return tmpAPI;
return nullptr;
}