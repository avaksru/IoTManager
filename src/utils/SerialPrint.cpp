#include "utils/SerialPrint.h"

// Защита от бесконечной рекурсии при ошибках файловой системы
static bool serialPrintInProgress = false;

void SerialPrint(const String& errorLevel, const String& module, const String& msg, const String& itemId) {
    // Защита от рекурсии - если уже внутри SerialPrint, просто выводим в Serial
    if (serialPrintInProgress) {
        String tosend = prettyMillis(millis());
        tosend += " [R] [";
        tosend += module;
        tosend += "] ";
        tosend += msg;
        Serial.println(tosend);
        return;
    }
    
    serialPrintInProgress = true;
    
    String tosend = prettyMillis(millis());
    tosend += " [";
    tosend += errorLevel;
    tosend += "] [";
    tosend += module;
    tosend += "] ";
    tosend += msg;
    Serial.println(tosend);

    // Проверяем валидность settingsFlashJson перед чтением log level
    // settingsFlashJson может быть "failed" или пустым при ошибках ФС
    if (settingsFlashJson.length() > 2 && 
        settingsFlashJson != "failed" &&
        settingsFlashJson != "{}") {
        if (jsonReadInt(settingsFlashJson, F("log")) != 0) {
            sendStringToWs(F("corelg"), tosend, -1);
        }
    }
    
    serialPrintInProgress = false;

    if (errorLevel == "E") {
        cleanString(tosend);
        // создаем событие об ошибке для возможной реакции в сценарии
        if (itemId != "") {
            createItemFromNet(itemId + F("_onError"), tosend, 1);
        } else {
            // createItemFromNet("onError", tosend, -4);
        }
    }
}
