#include "UpgradeFirm.h"
#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <HTTPUpdate.h>
#include <esp_task_wdt.h>
#endif

updateFirm update;

// Добавляем структуру аргументов для OTA task
#ifdef ESP32
struct OTAArgs
{
    String path;
    int result; // HTTP_UPDATE_OK / HTTP_UPDATE_FAILED / HTTP_UPDATE_NO_UPDATES
    TaskHandle_t callerTask;
};

static SemaphoreHandle_t otaSemaphore = nullptr;
static volatile int otaLastResult = HTTP_UPDATE_FAILED; // <- added: последний результат OTA
static void otaBuildTask(void *pvParameters)
{
    OTAArgs *args = static_cast<OTAArgs *>(pvParameters);
    if (!args)
    {
        if (otaSemaphore)
            xSemaphoreGive(otaSemaphore);
        vTaskDelete(NULL);
        return;
    }

    // Удаляем текущий task из WDT, чтобы долгий сетевой обмен не вызывал watchdog
    // (удаляем только этот task)
    esp_task_wdt_delete(xTaskGetCurrentTaskHandle());

    WiFiClient wifiClient;
    String fullPath = args->path + "/firmware.bin";
    SerialPrint("i", F("Update"), "Updating build from: " + fullPath);
    HTTPUpdateResult retBuild = httpUpdate.update(wifiClient, fullPath);

    if (retBuild == HTTP_UPDATE_OK)
        args->result = HTTP_UPDATE_OK;
    else if (retBuild == HTTP_UPDATE_NO_UPDATES)
        args->result = HTTP_UPDATE_NO_UPDATES;
    else
        args->result = HTTP_UPDATE_FAILED;

    // Оповещаем вызвавший task
    otaLastResult = args->result; // сохранить результат глобально
    if (otaSemaphore)
        xSemaphoreGive(otaSemaphore);

    // освобождаем память аргументов
    delete args;
    vTaskDelete(NULL);
}
// Добавляем отдельный таск для обновления FS (ESP32)
static void otaFsTask(void *pvParameters)
{
    OTAArgs *args = static_cast<OTAArgs *>(pvParameters);
    if (!args)
    {
        if (otaSemaphore)
            xSemaphoreGive(otaSemaphore);
        vTaskDelete(NULL);
        return;
    }

    // удаляем текущий task из WDT
    esp_task_wdt_delete(xTaskGetCurrentTaskHandle());

    WiFiClient wifiClient;
    httpUpdate.rebootOnUpdate(false);
    String fullPath = args->path + "/littlefs.bin";
    SerialPrint("i", F("Update"), "Updating FS from: " + fullPath);
    HTTPUpdateResult retFS = httpUpdate.updateSpiffs(wifiClient, fullPath);

    if (retFS == HTTP_UPDATE_OK)
    {
        args->result = HTTP_UPDATE_OK;
        SerialPrint("!!!", F("Update"), F("FS upgrade done!"));
        saveUpdeteStatus("fs", UPDATE_COMPLETED);
        // HTTP.send(200, "text/plain", "FS upgrade done!");
    }
    else
    {
        args->result = HTTP_UPDATE_FAILED;
        saveUpdeteStatus("fs", UPDATE_FAILED);
        if (retFS == HTTP_UPDATE_FAILED)
        {
            SerialPrint("E", F("Update"), "HTTP_UPDATE_FAILED");
            String page = "<html><body>Ошибка обновления FS!<br>FS Update failed!<br><a href='/'>Home</a></body></html>";
            HTTP.send(200, "text/html; charset=UTF-8", page);
        }
        else if (retFS == HTTP_UPDATE_NO_UPDATES)
        {
            args->result = HTTP_UPDATE_NO_UPDATES;
            SerialPrint("E", F("Update"), "HTTP_UPDATE_NO_UPDATES! DELETE /localota !!!");
        }
    }

    otaLastResult = args->result; // сохранить результат
    if (otaSemaphore)
        xSemaphoreGive(otaSemaphore);

    delete args;
    vTaskDelete(NULL);
}
#endif

void upgrade_firmware(int type, String path)
{
    if (path == "")
    {
        path = getBinPath();
    }
    putUserDataToRam();
    // сбросим файл статуса последнего обновления
    writeFile("ota.json", "{}");

    // only build
    if (type == 1) {
        if (upgradeBuild(path)) {
            saveUserDataToFlash();
            restartEsp();
        }
    }

    // only littlefs
    else if (type == 2) {
        if (upgradeFS(path)) {
            saveUserDataToFlash();
            restartEsp();
        }
    }

    // littlefs and build
    else if (type == 3) {
        if (upgradeFS(path)) {
            saveUserDataToFlash();
            if (upgradeBuild(path))
            {
                restartEsp();
            }
        }
    }
}

bool upgradeFS(String path)
{
    bool ret = false;
#ifndef LIBRETINY
    WiFiClient wifiClient;
    SerialPrint("!!!", F("Update"), "Start upgrade FS... " + path);

    if (path == "")
    {
        SerialPrint("E", F("Update"), F("FS Path error"));
        saveUpdeteStatus("fs", PATH_ERROR);
        return ret;
    }
#ifdef ESP8266
    ESPhttpUpdate.rebootOnUpdate(false);
    t_httpUpdate_return retFS = ESPhttpUpdate.updateFS(wifiClient, path + "/littlefs.bin");
    if (retFS == HTTP_UPDATE_OK)
    {
        SerialPrint("!!!", F("Update"), F("FS upgrade done!"));
        saveUpdeteStatus("fs", UPDATE_COMPLETED);
        ret = true;
    }
    else
    {
        saveUpdeteStatus("fs", UPDATE_FAILED);
        if (retFS == HTTP_UPDATE_FAILED)
        {
            SerialPrint("E", F("Update"), "HTTP_UPDATE_FAILED");
            String page = "<html><body>Ошибка обновления FS!<br>FS Update failed!<br><a href='/'>Home</a></body></html>";
            HTTP.send(200, "text/html; charset=UTF-8", page);
        }
        else if (retFS == HTTP_UPDATE_NO_UPDATES)
        {
            SerialPrint("E", F("Update"), "HTTP_UPDATE_NO_UPDATES! DELETE /localota !!!");
        }
    }
#endif

#ifdef ESP32
    // создаём семафор один раз
    if (!otaSemaphore)
        otaSemaphore = xSemaphoreCreateBinary();

    // создаём аргументы для task
    OTAArgs *args = new OTAArgs();
    args->path = path;
    args->result = HTTP_UPDATE_FAILED;
    otaLastResult = HTTP_UPDATE_FAILED;

    // создаём task на другом ядре (core 0) чтобы не блокировать loopTask (обычно core 1)
    BaseType_t created = xTaskCreatePinnedToCore(
        otaFsTask,
        "otaFS",
        8192, // стек в байтах (при необходимости увеличить)
        args,
        0, // lower priority to let loopTask run and reset WDT
        nullptr,
        0 // pin to core 0
    );

    if (created != pdPASS)
    {
        SerialPrint("E", F("Update"), F("Failed to create OTA FS task"));
        saveUpdeteStatus("fs", UPDATE_FAILED);
        delete args;
        return ret;
    }

    // ждём завершения задачи с разумным таймаутом (например 5 минут)
    // Удаляем текущий task из WDT, чтобы ожидание семафора не вызвало перезагрузку
    esp_task_wdt_delete(NULL);
    if (xSemaphoreTake(otaSemaphore, pdMS_TO_TICKS(300000)) == pdTRUE)
    {
        // смотрим глобальную переменную с результатом
        if (otaLastResult == HTTP_UPDATE_OK)
        {
            ret = true;
        }
        else
        {
            ret = false;
        }
    }
    else
    {
        SerialPrint("E", F("Update"), F("OTA FS task timeout"));
        saveUpdeteStatus("fs", UPDATE_FAILED);
        ret = false;
    }
    // Возвращаем task в WDT
    esp_task_wdt_add(NULL);
#endif

#endif
    return ret;
}

bool upgradeBuild(String path) {
    bool ret = false;
#ifndef LIBRETINY
    WiFiClient wifiClient;
    SerialPrint("!!!", F("Update"), "Start upgrade BUILD... " + path);

    if (path == "") {
        SerialPrint("E", F("Update"), F("Build Path error"));
        saveUpdeteStatus("build", PATH_ERROR);
        return ret;
    }

#ifdef ESP32
    // создаём семафор один раз
    if (!otaSemaphore)
        otaSemaphore = xSemaphoreCreateBinary();

    // создаём аргументы для task
    OTAArgs *args = new OTAArgs();
    args->path = path;
    args->result = HTTP_UPDATE_FAILED;

    // создаём task на другом ядре (core 0) чтобы не блокировать loopTask (обычно core 1)
    BaseType_t created = xTaskCreatePinnedToCore(
        otaBuildTask,
        "otaBuild",
        8192, // стек в байтах (при необходимости увеличить)
        args,
        0, // lower priority to let loopTask run and reset WDT
        nullptr,
        0 // pin to core 0
    );

    if (created != pdPASS)
    {
        SerialPrint("E", F("Update"), F("Failed to create OTA task"));
        saveUpdeteStatus("build", UPDATE_FAILED);
        delete args;
        return ret;
    }

    // ждём завершения задачи с разумным таймаутом (например 5 минут)
    // Удаляем текущий task из WDT, чтобы ожидание семафора не вызвало перезагрузку
    esp_task_wdt_delete(NULL);
    if (xSemaphoreTake(otaSemaphore, pdMS_TO_TICKS(300000)) == pdTRUE)
    {
        ret = true;
    }
    else
    {
        SerialPrint("E", F("Update"), F("OTA task timeout"));
        saveUpdeteStatus("build", UPDATE_FAILED);
        ret = false;
    }
    // Возвращаем task в WDT
    esp_task_wdt_add(NULL);
#endif

#if defined(esp8266_4mb) || defined(esp8266_16mb) || defined(esp8266_1mb) || defined(esp8266_1mb_ota) || defined(esp8266_2mb) || defined(esp8266_2mb_ota)
    ESPhttpUpdate.rebootOnUpdate(false);
    t_httpUpdate_return retBuild = ESPhttpUpdate.update(wifiClient, path + "/firmware.bin");
#endif

#endif
    return ret;
}

void restartEsp() {
    Serial.println(F("Restart ESP...."));
    delay(1000);
    ESP.restart();
}

const String getBinPath() {
    String path = "error";
    int targetVersion = 400; //HACKFUCK local OTA version in PrepareServer.py
    String serverip;
    if (jsonRead(errorsHeapJson, F("chver"), targetVersion)) {
        if (targetVersion >= 400)
        {
            if (jsonRead(settingsFlashJson, F("serverip"), serverip))
            {
                if (serverip != "")
                {
                    path = jsonReadStr(settingsFlashJson, F("serverip")) + "/iotm/" + String(FIRMWARE_NAME) + "/" + String(targetVersion) + "/" ;
                }
            }
        }
    }
    else if (targetVersion >= 400)
    {
            if (jsonRead(settingsFlashJson, F("serverlocal"), serverip)) {
            if (serverip != "")
            {
                path = jsonReadStr(settingsFlashJson, F("serverlocal")) + "/iotm/" + String(FIRMWARE_NAME) + "/" + String(targetVersion) + "/";
            }
        }
    }

    SerialPrint("i", F("Update"), "server local: " + path);
    return path;
}

void putUserDataToRam() {
    update.configJson = readFile("config.json", 4096 * 8);
    update.settingsFlashJson = readFile("settings.json", 4096 * 8);
    update.layoutJson = readFile("layout.json", 4096 * 8);
    update.scenarioTxt = readFile("scenario.txt", 4096 * 8);
}

void saveUserDataToFlash() {
    writeFile("/config.json", update.configJson);
    writeFile("/settings.json", update.settingsFlashJson);
    writeFile("/layout.json", update.layoutJson);
    writeFile("/scenario.txt", update.scenarioTxt);
}

void saveUpdeteStatus(String key, int val) {
    const String path = "ota.json";
    String json = readFile(path, 1024);
    if (json == "failed") json = "{}";
    jsonWriteInt_(json, key, val);
    writeFile(path, json);
}