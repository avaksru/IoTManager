#include "UpgradeFirm.h"
#include "EspFileSystem.h" // FileFS (LittleFS) - нужен для перемонтирования ФС после updateSpiffs
#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <HTTPUpdate.h>
#include <esp_task_wdt.h>
#endif

updateFirm update;

// Склеивает базовый путь и имя файла, убирая лишние завершающие '/' (serverip часто уже содержит '/')
static String otaJoinPath(const String &base, const String &file)
{
    String b = base;
    while (b.length() && b.endsWith("/"))
    {
        b.remove(b.length() - 1);
    }
    return b + "/" + file;
}

// После updateSpiffs раздел ФС перезаписан целиком, а "старое" монтирование LittleFS
// остаётся в памяти со своей таблицей размещения. Все записи через него
// (saveUserDataToFlash, фоновые ts-задачи) теряются после перезагрузки и могут
// испортить новую файловую систему. Поэтому ФС нужно перемонтировать и только
// затем восстанавливать пользовательские данные.
// Используется и из других модулей (StandWebServer, Telegram_v2) после их
// собственных FS-обновлений - поэтому не static.
bool remountFS()
{
    SerialPrint("i", F("Update"), F("Remounting FS after flash..."));
    FileFS.end();
    if (FileFS.begin())
    {
        SerialPrint("i", F("Update"), F("FS remounted OK"));
        return true;
    }
    SerialPrint("E", F("Update"), F("FS remount FAILED"));
    return false;
}

// Восстановление одного файла пользователя с логом и проверкой записи.
// Если файл не удалось прочитать до обновления ("failed"/"large"), данные терять
// нечего - пропускаем запись, чтобы не затирать файлы заводской FS мусором.
static void restoreOneFile(const String &name, const String &content)
{
    if (content == "failed")
    {
        SerialPrint("E", F("Update"), name + " not saved before update (read failed), skip restore");
        return;
    }
    if (content == "large")
    {
        SerialPrint("E", F("Update"), name + " not saved before update (too large for RAM), skip restore");
        return;
    }

    String res = writeFile(name, content);
    if (res != "success")
    {
        SerialPrint("E", F("Update"), "Write " + name + " failed: " + res + ", retry once");
        delay(100);
        res = writeFile(name, content);
        if (res != "success")
        {
            SerialPrint("E", F("Update"), "RESTORE " + name + " FAILED: " + res);
            return;
        }
    }

    // проверяем запись повторным чтением
    String check = readFile(name, content.length() + 128);
    if (check == content)
    {
        SerialPrint("i", F("Update"), "Restored " + name + " (" + String(content.length()) + " bytes)");
    }
    else
    {
        SerialPrint("E", F("Update"), "RESTORE VERIFY " + name + " FAILED!");
    }
}
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

    // Задача НЕ зарегистрирована в WDT (watchdog следит только за loopTask),
    // esp_task_wdt_delete() здесь лишь шумел "task not found" - убрано

    WiFiClient wifiClient;
    String fullPath = otaJoinPath(args->path, "firmware.bin");
    SerialPrint("i", F("Update"), "Updating build from: " + fullPath);
    HTTPUpdateResult retBuild = httpUpdate.update(wifiClient, fullPath);

    if (retBuild == HTTP_UPDATE_OK)
        args->result = HTTP_UPDATE_OK;
    else if (retBuild == HTTP_UPDATE_NO_UPDATES)
        args->result = HTTP_UPDATE_NO_UPDATES;
    else
    {
        args->result = HTTP_UPDATE_FAILED;
        saveUpdeteStatus("build", UPDATE_FAILED);
        SerialPrint("E", F("Update"), "BUILD HTTP_UPDATE_FAILED, err=" + String(httpUpdate.getLastError()) + " (" + httpUpdate.getLastErrorString() + ")");
    }

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

    // Задача не зарегистрирована в WDT, esp_task_wdt_delete() шумел "task not found" - убрано

    WiFiClient wifiClient;
    httpUpdate.rebootOnUpdate(false);
    String fullPath = otaJoinPath(args->path, "littlefs.bin");
    SerialPrint("i", F("Update"), "Updating FS from: " + fullPath);
    HTTPUpdateResult retFS = httpUpdate.updateSpiffs(wifiClient, fullPath);

    if (retFS == HTTP_UPDATE_OK)
    {
        args->result = HTTP_UPDATE_OK;
        SerialPrint("!!!", F("Update"), F("FS upgrade done!"));
        // ЗДЕСЬ НЕ ПИШЕМ ota.json: раздел ФС уже перезаписан, а монтирование устарело.
        // Статус запишет upgradeFS()/upgrade_firmware() после remountFS()
        // HTTP.send(200, "text/plain", "FS upgrade done!");
    }
    else
    {
        args->result = HTTP_UPDATE_FAILED;
        if (retFS == HTTP_UPDATE_FAILED)
        {
            SerialPrint("E", F("Update"), "FS HTTP_UPDATE_FAILED, err=" + String(httpUpdate.getLastError()) + " (" + httpUpdate.getLastErrorString() + ")");
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
    // приостанавливаем фоновые ts-задачи (TIMES пишет values.json и т.п.),
    // чтобы во время перезаписи раздела никто не писал в ФС через "зависшее" монтирование
    ts.disableAll();

    if (path == "")
    {
        path = getBinPath();
    }
    putUserDataToRam();
    // сбросим файл статуса последнего обновления
    writeFile("ota.json", "{}");

    // only build
    if (type == 1)
    {
        if (upgradeBuild(path))
        {
            saveUserDataToFlash();
            restartEsp();
        }
        else
        {
            SerialPrint("E", F("Update"), F("BUILD update failed, device not rebooted"));
            ts.enableAll();
        }
    }

    // only littlefs
    else if (type == 2)
    {
        if (upgradeFS(path))
        {
            // раздел FS перезаписан - ПЕРЕМОНТИРОВАТЬ, иначе восстановление потеряется
            if (remountFS())
            {
                saveUserDataToFlash();
                // статус уже писался из otaFsTask в старое монтирование - перезапишем в новую FS
                saveUpdeteStatus("fs", UPDATE_COMPLETED);
            }
            else
            {
                SerialPrint("E", F("Update"), F("User data restore skipped (FS remount failed)"));
            }
            restartEsp();
        }
        else
        {
            SerialPrint("E", F("Update"), F("FS update failed, device not rebooted"));
            ts.enableAll();
        }
    }

    // littlefs and build
    else if (type == 3)
    {
        if (upgradeFS(path))
        {
            if (remountFS()) // раздел FS перезаписан - перемонтировать, иначе restore потеряется
            {
                saveUserDataToFlash();
                // статус уже писался из otaFsTask в старое монтирование - перезапишем в новую FS
                saveUpdeteStatus("fs", UPDATE_COMPLETED);
            }
            else
            {
                SerialPrint("E", F("Update"), F("User data restore skipped (FS remount failed)"));
            }

            if (upgradeBuild(path))
            {
                restartEsp();
            }
            else
            {
                // ФС уже заменена, но прошивка НЕ скачалась/не записалась.
                // НЕ перезагружаем "молча" (раньше upgradeBuild() возвращал true без
                // проверки результата, и устройство уходило в перезагрузку со старой
                // прошивкой и новой ФС, теряя при этом пользовательские данные).
                SerialPrint("E", F("Update"), F("BUILD update failed, FS updated, device not rebooted"));
                ts.enableAll();
            }
        }
        else
        {
            SerialPrint("E", F("Update"), F("FS update failed, upgrade aborted"));
            ts.enableAll();
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
    t_httpUpdate_return retFS = ESPhttpUpdate.updateFS(wifiClient, otaJoinPath(path, "littlefs.bin"));
    if (retFS == HTTP_UPDATE_OK)
    {
        SerialPrint("!!!", F("Update"), F("FS upgrade done!"));
        // ЗДЕСЬ НЕ ПИШЕМ ota.json: раздел ФС перезаписан, монтирование устарело.
        // Статус запишет upgrade_firmware() после remountFS()
        ret = true;
    }
    else
    {
        saveUpdeteStatus("fs", UPDATE_FAILED);
        if (retFS == HTTP_UPDATE_FAILED)
        {
            SerialPrint("E", F("Update"), "FS HTTP_UPDATE_FAILED, err=" + String(ESPhttpUpdate.getLastError()) + " (" + ESPhttpUpdate.getLastErrorString() + ")");
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
            // статус FAILURE пишем здесь (а не из otaFsTask): раздел ФС не перезаписан,
            // монтирование осталось валидным, запись ota.json безопасна
            saveUpdeteStatus("fs", UPDATE_FAILED);
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
        // смотрим глобальную переменную с результатом (как в upgradeFS).
        // Раньше здесь стоял безусловный ret = true: прошивка могла НЕ скачаться,
        // а устройство всё равно перезагружалось (баг, оставлявший новую ФС без
        // пользовательских данных и со старой прошивкой)
        if (otaLastResult == HTTP_UPDATE_OK)
        {
            ret = true;
        }
        else
        {
            saveUpdeteStatus("build", UPDATE_FAILED);
            ret = false;
        }
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
    t_httpUpdate_return retBuild = ESPhttpUpdate.update(wifiClient, otaJoinPath(path, "firmware.bin"));
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

    // readFile возвращает "failed"/"large" (а для config.json - "[]") при проблемах,
    // логируем, чтобы потеря пользовательских данных была видна в логе обновления
    if (update.configJson == "failed" || update.configJson == "large")
        SerialPrint("E", F("Update"), "config.json: " + update.configJson + " - data will be lost after FS update!");
    if (update.settingsFlashJson == "failed" || update.settingsFlashJson == "large")
        SerialPrint("E", F("Update"), "settings.json: " + update.settingsFlashJson + " - data will be lost after FS update!");
    if (update.layoutJson == "failed" || update.layoutJson == "large")
        SerialPrint("E", F("Update"), "layout.json: " + update.layoutJson + " - data will be lost after FS update!");
    if (update.scenarioTxt == "failed" || update.scenarioTxt == "large")
        SerialPrint("E", F("Update"), "scenario.txt: " + update.scenarioTxt + " - data will be lost after FS update!");
}

void saveUserDataToFlash() {
    restoreOneFile("config.json", update.configJson);
    restoreOneFile("settings.json", update.settingsFlashJson);
    restoreOneFile("layout.json", update.layoutJson);
    restoreOneFile("scenario.txt", update.scenarioTxt);
}

void saveUpdeteStatus(String key, int val) {
    const String path = "ota.json";
    String json = readFile(path, 1024);
    if (json == "failed") json = "{}";
    jsonWriteInt_(json, key, val);
    writeFile(path, json);
}