#include "ESPConfiguration.h"

void* getAPI_Cron(String subtype, String params);
void* getAPI_DiscoveryHomeD(String subtype, String params);
void* getAPI_Loging(String subtype, String params);
void* getAPI_Timer(String subtype, String params);
void* getAPI_Variable(String subtype, String params);
void* getAPI_VariableColor(String subtype, String params);
void* getAPI_VButton(String subtype, String params);
void* getAPI_AnalogAdc(String subtype, String params);
void* getAPI_Ds18b20(String subtype, String params);
void* getAPI_ExternalMQTT(String subtype, String params);
void* getAPI_SoftRTC(String subtype, String params);
void* getAPI_ButtonIn(String subtype, String params);
void* getAPI_ButtonOut(String subtype, String params);
void* getAPI_HC59543(String subtype, String params);
void* getAPI_TelegramLT(String subtype, String params);
void* getAPI_Thermostat(String subtype, String params);

void* getAPI(String subtype, String params) {
void* tmpAPI; void* foundAPI = nullptr;
if ((tmpAPI = getAPI_Cron(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_DiscoveryHomeD(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_Loging(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_Timer(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_Variable(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_VariableColor(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_VButton(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_AnalogAdc(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_Ds18b20(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_ExternalMQTT(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_SoftRTC(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_ButtonIn(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_ButtonOut(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_HC59543(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_TelegramLT(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_Thermostat(subtype, params)) != nullptr) foundAPI = tmpAPI;
return foundAPI;
}