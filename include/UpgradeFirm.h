#pragma once
#include "Global.h"
// #include "Upgrade.h"
#ifdef ESP8266
// #include "ESP8266.h"
#elif ESP32
#include <HTTPUpdate.h>
#endif

struct updateFirm {
    String settingsFlashJson;
    String configJson;
    String layoutJson;
    String scenarioTxt;
    String chartsData;
};

extern void upgradeInit();
extern void getLastVersion();
extern void upgrade_firmware(int type, String path);
extern bool upgradeFS(String path);
extern bool upgradeBuild(String path);

// Перемонтировать LittleFS после перезаписи раздела ФС (updateSpiffs/updateFS/Update).
// После записи раздела через esp_partition_write "старое" монтирование в RAM содержит
// устаревшую таблицу размещения: записи/чтения через него теряются после перезагрузки
// и могут повредить новую ФС. Возвращает true, если перемонтирование прошло успешно.
extern bool remountFS();

extern void restartEsp();

extern const String getBinPath();
extern void putUserDataToRam();
extern void saveUserDataToFlash();
extern void saveUpdeteStatus(String key, int val);