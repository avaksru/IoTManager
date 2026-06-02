// Licensed under the Cooperative Non-Violent Public License (CNPL)
// See: https://github.com/CHE77/IoTManager-Modules/blob/main/LICENSE

#include "Global.h"
#include "classes/IoTItem.h"
#include "NTP.h"
#include "WsServer.h"

// Подключаем нужные заголовочные файлы в зависимости от платформы
#ifdef ESP8266
#include <TZ.h>
#include <coredecls.h> // Для settimeofday_cb() в ESP8266
#else
#include <esp_sntp.h> // Для ESP32
#endif

typedef enum
{
    SoftRTC_SYNC_STATUS_NOT_SET = 0,    // Время не установлено
                                        //  SoftRTC_SYNC_STATUS_COULD_NOT_SET,  // Время не может быть установлено
    SoftRTC_SYNC_STATUS_BEFORE_SoftRTC, // Время было установлено до SoftRTC
    SoftRTC_SYNC_STATUS_RESTORED,       // Время восстановлено из памяти
    SoftRTC_SYNC_STATUS_MANUAL,         // Время установлено вручную
    SoftRTC_SYNC_STATUS_FROM_BROWSER_OR_NTP,
    SoftRTC_SYNC_STATUS_FROM_BROWSER, // Время (скоере всего) установлено с Браузера
    SoftRTC_SYNC_STATUS_NTP_JUST,     // Время только что синхронизировано через NTP
    SoftRTC_SYNC_STATUS_NTP           // Время синхронизировано через NTP

} SoftRTC_sync_status_t;

// Переменная для отслеживания источника времени
volatile int syncStatus = SoftRTC_SYNC_STATUS_NOT_SET;

// Функция-коллбэк для NTP (разные сигнатуры для разных платформ)
#ifdef ESP8266
/*
void timeSyncCallback()
{
    syncStatus = SoftRTC_SYNC_STATUS_NTP_JUST;
}
*/
#else
void timeSyncCallback(struct timeval *tv)
{
    (void)tv; // Неиспользуемый параметр
    syncStatus = SoftRTC_SYNC_STATUS_NTP_JUST;
}
#endif

time_t getUnixTimeFromYMDHMS(int year, int month, int day, int hour, int minute, int second)
{
    struct tm t;
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;
    t.tm_isdst = -1;
    return mktime(&t);
}

time_t getUnixTimeFromString(String datetime)
{
    struct tm t = {0};
    int d, m, y, h, min, s;

    const char *dt = datetime.c_str();

    if (sscanf(dt, "%d%*c%d%*c%d %d:%d:%d", &d, &m, &y, &h, &min, &s) == 6)
    {
        if (y < 100)
            y += (y >= 69) ? 1900 : 2000;
        if (d > 31)
        {
            int temp = d;
            d = y;
            y = temp;
        }
    }
    else
    {
        return -1;
    }

    t.tm_mday = d;
    t.tm_mon = m - 1;
    t.tm_year = y - 1900;
    t.tm_hour = h;
    t.tm_min = min;
    t.tm_sec = s;
    t.tm_isdst = -1;

    return mktime(&t);
}

String formatUnixTime(time_t unixTime)
{
    struct tm *timeInfo;
    timeInfo = localtime(&unixTime);
    char formattedTime[32];
    // char formattedTime[20];
    int year = timeInfo->tm_year + 1900;
    int shortYear = year % 100;

    snprintf(formattedTime, sizeof(formattedTime), "%02d.%02d.%02d %02d:%02d:%02d",
             timeInfo->tm_mday, timeInfo->tm_mon + 1, shortYear,
             timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);

    return String(formattedTime);
}

bool isDSTfunc(time_t utc)
{
    struct tm *timeinfo = gmtime(&utc); // UTC time → broken-down time
    int year = timeinfo->tm_year + 1900;

    // Найдём последнюю дату воскресенья марта и октября
    struct tm lastMarchSunday = {0};
    lastMarchSunday.tm_year = year - 1900;
    lastMarchSunday.tm_mon = 2; // Март (0-based)
    lastMarchSunday.tm_mday = 31;
    lastMarchSunday.tm_hour = 3; // Переход в 03:00 UTC

    mktime(&lastMarchSunday); // нормализация даты

    // Ищем последнее воскресенье марта
    while (lastMarchSunday.tm_wday != 0)
    { // 0 = воскресенье
        lastMarchSunday.tm_mday--;
        mktime(&lastMarchSunday);
    }

    time_t dstStart = mktime(&lastMarchSunday); // начало летнего времени

    struct tm lastOctoberSunday = {0};
    lastOctoberSunday.tm_year = year - 1900;
    lastOctoberSunday.tm_mon = 9; // Октябрь
    lastOctoberSunday.tm_mday = 31;
    lastOctoberSunday.tm_hour = 4; // Переход в 04:00 UTC

    mktime(&lastOctoberSunday);

    while (lastOctoberSunday.tm_wday != 0)
    {
        lastOctoberSunday.tm_mday--;
        mktime(&lastOctoberSunday);
    }

    time_t dstEnd = mktime(&lastOctoberSunday); // конец летнего времени

    return utc >= dstStart && utc < dstEnd;
}

int getWeekOfYear(time_t unixTime)
{
    struct tm *timeInfo = localtime(&unixTime);
    char buffer[3]; // 2 цифры + \0
    strftime(buffer, sizeof(buffer), "%W", timeInfo); // %W — номер недели (понедельник — первый день недели)
    return atoi(buffer);
}

int getDayOfWeekNumber(time_t unixTime)
{
    struct tm *timeInfo = localtime(&unixTime);
    int wday = timeInfo->tm_wday;
    return (wday == 0) ? 7 : wday; // воскресенье (0) → 7
}

int getDaysInMonth(time_t unixTime)
{
    struct tm t = *localtime(&unixTime);

    t.tm_mday = 1;         // Первый день текущего месяца
    t.tm_mon += 1;         // Следующий месяц
    t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0;

    if (t.tm_mon > 11) {   // Декабрь → январь
        t.tm_mon = 0;
        t.tm_year += 1;
    }

    time_t firstOfNextMonth = mktime(&t);
    t.tm_mon -= 1;         // Возврат к текущему
    t.tm_mday = 1;
    time_t firstOfThisMonth = mktime(&t);

    int days = (firstOfNextMonth - firstOfThisMonth) / (60 * 60 * 24);
    return days;
}

// Возвращает день и месяц Пасхи в Григорианском календаре
void getOrthodoxEasterDate(int year, int &day, int &month)
{
    // Meeus (Julian) algorithm
    int a = year % 4;
    int b = year % 7;
    int c = year % 19;
    int d = (19 * c + 15) % 30;
    int e = (2 * a + 4 * b - d + 34) % 7;

    int julianMonth = (d + e + 114) / 31;          // 3 = март, 4 = апрель
    int julianDay   = ((d + e + 114) % 31) + 1;

    // Переходим из юлианского в григоріанский:
    // * для 1900‑2099 разница = 13 дней
    struct tm t {};
    t.tm_year = year - 1900;
    t.tm_mon  = julianMonth - 1;
    t.tm_mday = julianDay + 13; // 13‑дневная разница
    mktime(&t);                 // нормализует переход между месяцами

    day   = t.tm_mday;
    month = t.tm_mon + 1;
}


class SoftRTC : public IoTItem
{
private:
    bool _ticker = false;
    bool _debug = false;
    long interval = 60;
    int _winterTime = 2;
    int _summerTime = 3;
    time_t recovered_unixTime = 0;
    time_t lastNTPtimeCorrection = 0;
    time_t lastUnixTimeMillis = 0;
    time_t lastUnixTime = 0;
    // time_t _timeZoneSeconds;
    int _timezone = 0;
    int lastSyncStatus = 0;
    time_t before = 0;
    unsigned long startMillis = 0; // Сохраняем текущее время в миллисекундах

public:
    SoftRTC(String parameters) : IoTItem(parameters)
    {
        _timezone = jsonReadInt(settingsFlashJson, F("timezone"));
// Устанавливаем коллбэк в зависимости от платформы
#ifdef ESP8266
        // settimeofday_cb(timeSyncCallback);
#else
        sntp_set_time_sync_notification_cb(timeSyncCallback);
#endif

        _needSave = true;
        _round = 0;
        jsonRead(parameters, F("ticker"), _ticker);
        jsonRead(parameters, F("int"), interval);
        jsonRead(parameters, F("winterTime"), _winterTime);
        jsonRead(parameters, F("summerTime"), _summerTime);
        jsonRead(parameters, F("debug"), _debug);

        before = time(nullptr);
        startMillis = millis(); // Сохраняем текущее время в миллисекундах
    }

    void loop()
    {

#if defined(ESP32)
        if (syncStatus == SoftRTC_SYNC_STATUS_NTP_JUST)
        {
            time_t ut = getSystemTime();
            String localDateTime = formatUnixTime(ut + _timezone * 60 * 60);
            if (_debug)
            SerialPrint("I", F("SoftRTC"), "✅ Время синхронизировано через NTP! " + (String)ut + " LT: " + localDateTime);

            lastNTPtimeCorrection = getSystemTime() - (lastUnixTime + (millis() - lastUnixTimeMillis) / 1000);
            if (_debug)
            SerialPrint("I", F("SoftRTC"), "lastNTPtimeCorrection: " + (String)lastNTPtimeCorrection + " сек.");
            syncStatus = SoftRTC_SYNC_STATUS_NTP;

            IoTValue valTmp;
            valTmp.isDecimal = false;
            valTmp.valS = String(ut).c_str();
            regEvent(String(valTmp.valS.c_str()), F("SoftRTC"), _debug, true);
        }
#endif

        if (syncStatus < 4) // если вреямя обновлено из надежного источника то уже не остлеживаем его изменение
        {

            time_t after = time(nullptr);
            startMillis = millis() - startMillis; 

            if ((abs(after - before) - int((startMillis + 500) / 1000)) > 2) // проверка на изменение вермени
            {
                int delta = abs(after - before - int((startMillis + 500) / 1000));

                if (_debug){
                    SerialPrint("I", F("SoftRTC"), "Замечено, что Время измененно!");
                    SerialPrint("I", F("SoftRTC"), "syncStatus = " + String(syncStatus));
                    SerialPrint("I", F("SoftRTC"), "lastSyncStatus = " + String(lastSyncStatus));
                }

                if (syncStatus == lastSyncStatus)
                {
                    if (_debug)
                    SerialPrint("I", F("SoftRTC"), "Замечено, что Время измененно без изменения статуса");

#ifdef ESP8266
                    syncStatus = SoftRTC_SYNC_STATUS_FROM_BROWSER_OR_NTP;
                    lastSyncStatus = SoftRTC_SYNC_STATUS_FROM_BROWSER_OR_NTP;

                    time_t ut = getSystemTime();
                    String localDateTime = formatUnixTime(ut + _timezone * 60 * 60);
                    if (_debug)
                    SerialPrint("I", F("SoftRTC"), "✅ Время синхронизировано через NTP или Browser! " + (String)ut + " LT: " + localDateTime);

                    lastNTPtimeCorrection = getSystemTime() - (lastUnixTime + (millis() - lastUnixTimeMillis) / 1000);
                    if (_debug)
                    SerialPrint("I", F("SoftRTC"), "lastNTPtimeCorrection: " + (String)lastNTPtimeCorrection + " сек.");

#else
                    syncStatus = SoftRTC_SYNC_STATUS_FROM_BROWSER;
                    lastSyncStatus = SoftRTC_SYNC_STATUS_FROM_BROWSER;
#endif
                }
                else
                {
                    lastSyncStatus = syncStatus;
                    if (_debug)
                    SerialPrint("I", F("SoftRTC"), "Замечено, что Время измененно c изменением статуса");
                }
            }
            before = time(nullptr);
            startMillis = millis();
        }

        IoTItem::loop();
    }

    void doByInterval()
    {
        time_t unixTime = 0;

        if (syncStatus == SoftRTC_SYNC_STATUS_RESTORED)
        {
            unixTime = getSystemTime();
            if (_debug)
                SerialPrint("I", F("SoftRTC"), "Время восстановлено из памяти: " + (String)unixTime);
            if (isNetworkActive())
            {
                if (_debug)
                    SerialPrint("I", F("SoftRTC"), "Пробуем синхронизировать через NTP");
                synchTime();
            }
        }
        else if (syncStatus == SoftRTC_SYNC_STATUS_MANUAL)
        {
            unixTime = getSystemTime();
            if (_debug)
                SerialPrint("I", F("SoftRTC"), "Время установлено вручную: " + (String)unixTime);
            if (isNetworkActive())
            {
                if (_debug)
                    SerialPrint("I", F("SoftRTC"), "Пробуем синхронизировать через NTP");
                synchTime();
            }
        }
#ifdef ESP8266
        else if (syncStatus == SoftRTC_SYNC_STATUS_FROM_BROWSER_OR_NTP)
        {
            unixTime = getSystemTime();
            if (_debug)
                SerialPrint("I", F("SoftRTC"), "Время синхронизировано через WEB или NTP: " + (String)unixTime);
        }
#else
        else if (syncStatus == SoftRTC_SYNC_STATUS_FROM_BROWSER)
        {
            unixTime = getSystemTime();
            if (_debug)
                SerialPrint("I", F("SoftRTC"), "Время синхронизировано через Браузер: " + (String)unixTime);
        }
        else if (syncStatus == SoftRTC_SYNC_STATUS_NTP || syncStatus == SoftRTC_SYNC_STATUS_NTP_JUST)
        {
            unixTime = getSystemTime();
            if (_debug)
                SerialPrint("I", F("SoftRTC"), "Время синхронизировано через NTP: " + (String)unixTime);
        }
#endif

        else if (time(nullptr) > 100000)
        {
            syncStatus = SoftRTC_SYNC_STATUS_BEFORE_SoftRTC;
            unixTime = getSystemTime();
            if (_debug)
                SerialPrint("I", F("SoftRTC"), "Время синхронизировано через NTP до загрузки SoftRTC: " + (String)unixTime);
        }
        else if (syncStatus == SoftRTC_SYNC_STATUS_NOT_SET)
        {
valuesFlashJson = readFile(F("values.json"), 4096 * 8);
            valuesFlashJson.replace("\r\n", "");
            String valAsStr = "";
            if (jsonRead(valuesFlashJson, String(_id.c_str()), valAsStr, _debug))
            {
                recovered_unixTime = valAsStr.toInt();
                if (_debug)
                    SerialPrint("I", F("SoftRTC"), "Время из энергонезависимой памяти: " + (String)recovered_unixTime);

                time_t nextSecond = millis() / 1000 + 1;
                while (millis() / 1000 < nextSecond)
                {
                }

                unixTime = recovered_unixTime + millis() / 1000 + interval / 2;

                struct timeval now = {.tv_sec = unixTime, .tv_usec = 0};
                settimeofday(&now, NULL);

                syncStatus = SoftRTC_SYNC_STATUS_RESTORED;

                String localDateTime = formatUnixTime(unixTime + _timezone * 60 * 60);
                SerialPrint("I", F("SoftRTC"), "Установлено восстановленное время: " + (String)unixTime + " LT: " + localDateTime);
            }
            else
            {
                //  syncStatus = SoftRTC_SYNC_STATUS_COULD_NOT_SET;
                if (_debug)
                    SerialPrint("I", F("SoftRTC"), "Время не может быть восстановлено из памяти");
            }
        }
        else
        {
            if (_debug)
                SerialPrint("I", F("SoftRTC"), "Время не установлено");
        }

        if (unixTime)
        {
            lastUnixTime = unixTime;
            lastUnixTimeMillis = millis();
            if (_debug)
                SerialPrint("I", F("SoftRTC"), "Сохраняем время: " + (String)unixTime);
            value.isDecimal = false;
            value.valS = String(unixTime).c_str();
            regEvent(String(value.valS.c_str()), F("SoftRTC"), _debug, _ticker);
        }
    }

    void onModuleOrder(String &key, String &value)
    {
        if (key == "setUTime")
        {
            if (_debug)
                SerialPrint("i", F("SoftRTC"), "Устанавливаем время: " + value);
            char *stopstring;
            time_t ut = strtoul(value.c_str(), &stopstring, 10);

            struct timeval now = {.tv_sec = ut, .tv_usec = 0};
            settimeofday(&now, NULL);

            syncStatus = SoftRTC_SYNC_STATUS_MANUAL;

            String localDateTime = formatUnixTime(getSystemTime() + _timezone * 60 * 60);
            if (_debug)
            SerialPrint("I", F("SoftRTC"), "LT: " + localDateTime);

            IoTValue valTmp;
            valTmp.isDecimal = false;
            valTmp.valS = String(ut).c_str();
            regEvent(String(valTmp.valS.c_str()), F("SoftRTC"), _debug, _ticker);
        }
        else if (key == "setSysTime")
        {
            if (_debug)
                SerialPrint("i", F("SoftRTC"), "Устанавливаем системное время: " + value);

            time_t ut = getUnixTimeFromString(value) - _timezone * 60 * 60;

            struct timeval now = {.tv_sec = ut, .tv_usec = 0};
            settimeofday(&now, NULL);

            String localDateTime = formatUnixTime(getSystemTime() + _timezone * 60 * 60);
            if (_debug)
            SerialPrint("I", F("SoftRTC"), (String)ut + " LT: " + localDateTime);

            syncStatus = SoftRTC_SYNC_STATUS_MANUAL;

            IoTValue valTmp;
            valTmp.isDecimal = false;
            valTmp.valS = String(ut).c_str();
            regEvent(String(valTmp.valS.c_str()), F("SoftRTC"), _debug, _ticker);
        }
    }

    IoTValue execute(String command, std::vector<IoTValue> &param)
    {
        if (command == "checkForSummer")
        {
            unixTime = getSystemTime();
            bool isDST = isDSTfunc(unixTime);

            int newTimeZone = isDST ? _summerTime : _winterTime;

            if (newTimeZone != _timezone)
            {
                jsonWriteInt_(settingsFlashJson, F("timezone"), isDST ? _summerTime : _winterTime);
                syncSettingsFlashJson();
                _timezone = newTimeZone;

                if (newTimeZone == _summerTime)
                {
                    SerialPrint("i", F("SoftRTC"), "Перешли на летнее время");
                }
                else
                {
                    SerialPrint("i", F("SoftRTC"), "Перешли на зименее время");
                }
            }

            return {};
        }
        else if (command == "getTime")
        {
            String localDateTime = formatUnixTime(getSystemTime() + _timezone * 60 * 60);

            IoTValue valTmp;
            valTmp.isDecimal = false;
            valTmp.valS = localDateTime.c_str();
            return valTmp;
        }
        else if (command == "getUTShort")
        {
            IoTValue valTmp;
            valTmp.isDecimal = true;
            unixTimeShort = getSystemTime() - START_DATETIME;
            valTmp.valD = unixTimeShort;
            return valTmp;
        }
        else if (command == "setUnixTime")
        {
            if (param.size() == 1)
            {
                time_t ut = strtoul(param[0].valS.c_str(), nullptr, 10);
                if (_debug)
                SerialPrint("i", F("SoftRTC"), "Устанавливаем время UT: " + (String)ut);

                struct timeval now = {.tv_sec = ut, .tv_usec = 0};
                settimeofday(&now, NULL);

                String localDateTime = formatUnixTime(ut + _timezone * 60 * 60);
                if (_debug)
                SerialPrint("I", F("SoftRTC"), "LT: " + localDateTime);
                syncStatus = SoftRTC_SYNC_STATUS_MANUAL;

                IoTValue valTmp;
                valTmp.isDecimal = false;
                valTmp.valS = String(ut).c_str();
                regEvent(String(valTmp.valS.c_str()), F("SoftRTC"), _debug, _ticker);

                return {};
            }
        }
        else if (command == "setTimeFromYMDHMS")
        {
            if (param.size() == 6)
            {
                time_t ut = getUnixTimeFromYMDHMS(param[0].valD, param[1].valD, param[2].valD,
                                                  param[3].valD, param[4].valD, param[5].valD);
                                                  if (_debug)
                SerialPrint("i", F("SoftRTC"), "Устанавливаем время YMDHMS: " + (String)ut);

                struct timeval now = {.tv_sec = ut, .tv_usec = 0};
                settimeofday(&now, NULL);

                String localDateTime = formatUnixTime(ut + _timezone * 60 * 60);
                if (_debug)
                SerialPrint("I", F("SoftRTC"), "LT: " + localDateTime);
                syncStatus = SoftRTC_SYNC_STATUS_MANUAL;

                IoTValue valTmp;
                valTmp.isDecimal = false;
                valTmp.valS = String(ut).c_str();
                regEvent(String(valTmp.valS.c_str()), F("SoftRTC"), _debug, _ticker);

                return {};
            }
        }
        else if (command == "lastNTPtimeCorrection")
        {
            IoTValue valTmp;
            valTmp.isDecimal = true;
            valTmp.valD = lastNTPtimeCorrection;
            return valTmp;
        }
        else if (command == "getWeekNumber")
        {
            time_t ut = getSystemTime() + _timezone * 60 * 60; // учитываем локальное смещение
            int week = getWeekOfYear(ut);
            IoTValue valTmp;
            valTmp.isDecimal = true;
            valTmp.valD = week;
            return valTmp;
        }
        else if (command == "getDayOfWeek")
        {
            time_t ut = getSystemTime() + _timezone * 60 * 60; // учёт смещения
            int dayNum = getDayOfWeekNumber(ut);
            IoTValue valTmp;
            valTmp.isDecimal = true;
            valTmp.valD = dayNum;
            return valTmp;
        }
        else if (command == "getYear")
        {
            time_t ut = getSystemTime() + _timezone * 60 * 60;   // локальное смещение
            struct tm *timeInfo = localtime(&ut);
            int year = timeInfo->tm_year + 1900;   // полный (4‑значный) год        
            IoTValue valTmp;
            valTmp.isDecimal = true;   // возвращаем число
            valTmp.valD = year;
            return valTmp;
        }
        else if (command == "getDayOfYear")
        {
            time_t ut = getSystemTime() + _timezone * 60 * 60;
            struct tm *timeInfo = localtime(&ut);
            int dayOfYear = timeInfo->tm_yday + 1; // tm_yday начинается с 0
            IoTValue valTmp;
            valTmp.isDecimal = true;
            valTmp.valD = dayOfYear;
            return valTmp;
        }
        else if (command == "getDaysInMonth")
        {
            time_t ut = getSystemTime() + _timezone * 60 * 60;
            int days = getDaysInMonth(ut);
        
            IoTValue valTmp;
            valTmp.isDecimal = true;
            valTmp.valD = days;
            return valTmp;
        }
        else if (command == "getOrthodoxEaster")
        {
            time_t ut = getSystemTime() + _timezone * 60 * 60;
            struct tm *t = localtime(&ut);
            int day, month;
            getOrthodoxEasterDate(t->tm_year + 1900, day, month);
        
            char buf[12];
            snprintf(buf, sizeof(buf), "%02d.%02d.%d", day, month, t->tm_year + 1900);
        
            IoTValue valTmp;
            valTmp.isDecimal = false;
            valTmp.valS = String(buf).c_str();   
            return valTmp;
        }
        else if (command == "isOrthodoxEaster")
        {
            time_t ut = getSystemTime() + _timezone * 60 * 60;
            struct tm *now = localtime(&ut);
        
            int dEaster, mEaster;
            getOrthodoxEasterDate(now->tm_year + 1900, dEaster, mEaster);
        
            bool todayIsEaster = (now->tm_mday == dEaster) && (now->tm_mon + 1 == mEaster);
        
            IoTValue valTmp;
            valTmp.isDecimal = true;
            valTmp.valD = todayIsEaster ? 1 : 0;
            return valTmp;
        }
               

        return {};
    }

    ~SoftRTC() {}
};

class SoftRTCsyncStatus : public IoTItem
{
private:
    int lastSyncStatus = -1;
    bool _debug = false;
    bool _ticker = true;

public:
    SoftRTCsyncStatus(String parameters) : IoTItem(parameters)
    {
        _round = 0;
        jsonRead(parameters, F("ticker"), _ticker);
        jsonRead(parameters, F("debug"), _debug);
    }

    void loop()
    {
        if (syncStatus != lastSyncStatus)
        {
            lastSyncStatus = syncStatus;
            if (_debug)
                SerialPrint("i", F("SoftRTC"), "Статус синхронизации: " + (String)lastSyncStatus);
            value.isDecimal = true;
            value.valD = lastSyncStatus;
            regEvent(value.valD, F("SoftRTCsyncStatus"), _debug, _ticker);
        }
        IoTItem::loop();
    }

    ~SoftRTCsyncStatus() {}
};

void *getAPI_SoftRTC(String subtype, String param)
{
    if (subtype == F("SoftRTC"))
    {
        return new SoftRTC(param);
    }
    else if (subtype == F("SoftRTCsyncStatus"))
    {
        return new SoftRTCsyncStatus(param);
    }
    return nullptr;
}