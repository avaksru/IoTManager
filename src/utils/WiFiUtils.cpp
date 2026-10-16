#include "utils/WiFiUtils.h"
#include "WsServer.h"
#include <vector>
#include "DebugTrace.h"
#if defined(ESP32)
#include <esp_wifi.h> // esp_wifi_set_country_code / esp_wifi_set_protocol (страна и fallback протокола)
#endif
#if defined(ESP32)
#include <ESPmDNS.h>
#elif defined(ESP8266)
#include <ESP8266mDNS.h>
#endif
#define TRIESONE 20 // количество секунд ожидания подключения к одной сети из несколких
#define TRIES 30    // количество секунд ожидания подключения сети если она одна

// Расшифровка причин разрыва WiFi (коды из esp_wifi_types.h / esp_wifi_types_nan.h).
// Нужно, чтобы в логе сразу было видно: проблема с паролем/авторизацией (15, 202),
// нет такой сети (201) или это "обычный" разрыв (1, 2, 8)
static String wifiDisconnectReasonText(uint8_t reason)
{
  switch (reason)
  {
  case 1: return "UNSPECIFIED";
  case 2: return "AUTH_EXPIRE";
  case 3: return "AUTH_LEAVE";
  case 4: return "ASSOC_EXPIRE";
  case 5: return "ASSOC_TOOMANY";
  case 6: return "NOT_AUTHED";
  case 7: return "NOT_ASSOCED";
  case 8: return "ASSOC_LEAVE";
  case 10: return "DEAUTH_LEAVING";
  case 15: return "4WAY_HANDSHAKE_TIMEOUT (wrong password?)";
  case 16: return "GROUP_KEY_UPDATE_TIMEOUT";
  case 18: return "ASSOC_NOT_AUTHED";
  case 20: return "IEEE8021X_AUTH_FAIL";
  case 201: return "NO_AP_FOUND";
  case 202: return "AUTH_FAIL (wrong password?)";
  case 203: return "ASSOC_FAIL";
  case 204: return "HANDSHAKE_TIMEOUT";
  case 39: return "TIMEOUT (assoc exchange failed)";
  case 200: return "BEACON_TIMEOUT";
  case 205: return "CONNECTION_FAIL";
  case 206: return "AP_TSF_RESET";
  case 207: return "ROAMING";
  case 209: return "SA_QUERY_TIMEOUT (PMF?)";
  default: return "unknown (see esp_wifi_types.h)";
  }
}

// Расшифровка режимов аутентификации точки (esp_wifi_types.h, полезно видеть
// в логе, чем защищена сеть - WPA2 / WPA3 / переходный режим)
static String wifiAuthModeText(uint8_t authmode)
{
  switch (authmode)
  {
  case 0: return "OPEN";
  case 1: return "WEP";
  case 2: return "WPA_PSK";
  case 3: return "WPA2_PSK";
  case 4: return "WPA_WPA2_PSK";
  case 5: return "WPA2_ENTERPRISE";
  case 6: return "WPA3_PSK";
  case 7: return "WPA2_WPA3_PSK";
  case 8: return "WAPI_PSK";
  case 9: return "OWE";
  default: return "unknown (" + String(authmode) + ")";
  }
}

#if defined(esp32_wifirep)
#include "lwip/lwip_napt.h"
// #include "lwip/ip_route.h"
#define PROTO_TCP 6
#define PROTO_UDP 17

IPAddress stringToIp(String strIp)
{
  IPAddress ip;
  ip.fromString(strIp);
  return ip;
}
#endif
void addPortMap(String TCP_UDP, String maddr, u16_t mport, String daddr, u16_t dport)
{
#if defined(esp32_wifirep)
  uint8_t tcp_udp;
  if (TCP_UDP == "TCP")
    tcp_udp = PROTO_TCP;
  else if (TCP_UDP == "UDP")
    tcp_udp = PROTO_UDP;
  else
    SerialPrint("E", "WIFI", "Add port map: ERROR, Must be 'TCP' or 'UDP'");

  ip_portmap_add(tcp_udp, stringToIp(maddr), mport, stringToIp(daddr), dport);
  SerialPrint("i", "WIFI", "Add port map: " + String(tcp_udp) + ", " + maddr + ":" + String(mport) + " -> " + daddr + ":" + String(dport));
#else
  SerialPrint("E", "WIFI", "Add port map: ERROR, change board to esp32_wifirep");
#endif
}



#ifdef WIFI_ASYNC
std::vector<String> _ssidList;
std::vector<String> _passwordList;
// номер сети, для перебирания в момент подключения к сетям из массива
uint8_t currentNetwork = 0;
bool wifiConnecting = false;
uint8_t connectionAttempts = 0;
// диагностика проблем ассоциации: счётчик подряд идущих "подозрительных" разрывов
// (reason 39/201/203) и флаг принудительного ограничения протокола (обход 11ax)
uint8_t wifiConnFailStreak = 0;
// канал/BSSID последней известной точки из скана
uint8_t knownChannel = 0;
uint8_t knownBssid[6] = {0, 0, 0, 0, 0, 0};
bool knownBssidValid = false;
bool wifiProtocolPinned = false;
//------------------------------------------
// Обработчики событий Wi-Fi
//------------------------------------------
void WiFiEvent(arduino_event_t *event)
{
  switch (event->event_id)
  {
#if defined(LIBRETINY) || defined(esp32c6_4mb) || defined(esp32c6_8mb)
  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
#else
  case SYSTEM_EVENT_STA_CONNECTED:
#endif
    // Подключились к STA. Канал и режим аутентификации точки сразу видны в логе:
    // канал 12/13 => проблема страны, WPA3 => PMF/SAE нюансы
#if defined(ESP32)
    SerialPrint("i", "WIFI", "Connected to AP: " + WiFi.SSID() + " (channel " + String(event->event_info.wifi_sta_connected.channel) + ", authmode " + wifiAuthModeText(event->event_info.wifi_sta_connected.authmode) + ")");
#else
    SerialPrint("i", "WIFI", "Connected to AP: " + WiFi.SSID());
#endif
    // TODO если подключились, но не получили IP что будет?
    break;
#if defined(LIBRETINY) || defined(esp32c6_4mb) || defined(esp32c6_8mb)
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
#else
  case SYSTEM_EVENT_STA_GOT_IP:
#endif
    // Получили IP от роутера
    wifiConnFailStreak = 0; // соединение состоялось - сбрасываем счётчик провалов
    // wifiReconnectTicker.detach();
    ts.remove(WIFI_SCAN);
    ts.remove(WIFI_CONN);
#ifdef LIBRETINY
    SerialPrint("i", "WIFI", "http://" + ipToString(WiFi.localIP()));
    jsonWriteStr(settingsFlashJson, "ip", ipToString(WiFi.localIP()));
#else
    //SerialPrint("i", "WIFI", "http://" + WiFi.localIP().toString());
    jsonWriteStr(settingsFlashJson, "ip", WiFi.localIP().toString());
#endif
    createItemFromNet("onWifi", "1", 1);
    // запускаем MQTT
    mqttInit();
    SerialPrint("i", F("WIFI"), F("Network Init"));

    bool postMsgTelegram;
    if (!jsonRead(settingsFlashJson, "debugTraceMsgTlgrm", postMsgTelegram, false)) postMsgTelegram = 1;
    sendDebugTraceAndFreeMemory(postMsgTelegram);

    // Отключаем AP при успешном подключении
    WiFi.softAPdisconnect(true);
    break;
#if defined(LIBRETINY) || defined(esp32c6_4mb) || defined(esp32c6_8mb)
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
#else
  case SYSTEM_EVENT_STA_DISCONNECTED:
#endif
    // Отключились от STA. Логируем причину и уровень сигнала: неверный пароль (15/202),
    // нет сети (201), таймаут обмена (39 - типичен для ESP32-C6 и WiFi6 (11ax) точек,
    // а также для каналов 12/13 без настроек страны)
#if defined(ESP32)
    {
      uint8_t wlReason = event->event_info.wifi_sta_disconnected.reason;
      char bssidStr[18];
      snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               event->event_info.wifi_sta_disconnected.bssid[0], event->event_info.wifi_sta_disconnected.bssid[1],
               event->event_info.wifi_sta_disconnected.bssid[2], event->event_info.wifi_sta_disconnected.bssid[3],
               event->event_info.wifi_sta_disconnected.bssid[4], event->event_info.wifi_sta_disconnected.bssid[5]);
      SerialPrint("i", "WIFI", "Disconnected from STA (reason " + String(wlReason) + " - " + wifiDisconnectReasonText(wlReason) + ", rssi " + String(event->event_info.wifi_sta_disconnected.rssi) + ", bssid " + String(bssidStr) + ")");
      // считаем подряд идущие проблемные разрывы - по ним включаем fallback
      if ((wlReason == 39 || wlReason == 201 || wlReason == 203) && !isNetworkActive())
      {
        if (wifiConnFailStreak < 250) wifiConnFailStreak++;
        if (wifiConnFailStreak >= 3 && !wifiProtocolPinned)
        {
          // известный нюанс ESP32-C6 (IDF 5.1.x) с WiFi-6 (802.11ax) точками доступа:
          // ограничиваем протокол станции до 11B/11G/11N и продолжаем попытки
          if (esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N) == ESP_OK)
          {
            wifiProtocolPinned = true;
            SerialPrint("i", "WIFI", "Protocol pinned to 11B/11G/11N (11ax off) after repeated failures");
            WiFi.setSleep(false); // приглушаем модем-сон - иногда мешает HE/TWT-режимам
          }
        }
      }
    }
#else
    SerialPrint("i", "WIFI", "Disconnected from STA");
#endif
    // Завершаем задачу проверки сети
    ts.remove(WIFI_CONN);
    if (wifiConnecting)
    { // если у нас ещё не закончились попытки подключения, то перезапускаем задачу
      Serial.print(".");
      checkConnection();
      // wifiReconnectTicker.once_ms(WIFI_CHECK_INTERVAL, checkConnection);
    }
    else
    { // если попытки подключения исчерпаны, то переходим в AP
      sendDebugTraceAndFreeMemory(false);
      startAPMode();
    }
    break;
#if defined(LIBRETINY) || defined(esp32c6_4mb) || defined(esp32c6_8mb)
  case ARDUINO_EVENT_WIFI_SCAN_DONE:
#else
  case SYSTEM_EVENT_SCAN_DONE:
#endif
    if (WiFi.scanComplete() >= 0) {
      Serial.println("Valid Scan completed");
      handleScanResults();
      WiFi.scanDelete(); // Очищаем только при успешном сканировании
    } else {
      //Serial.println("Empty scan or error");
      //WiFi.scanDelete(); // Принудительная очистка буфера
    }
    // Завершилось сканирование сетей
    //Serial.println("Scan completed");
    //handleScanResults();
    break;
  }
}

//------------------------------------------
// Обработка результатов сканирования
//------------------------------------------
void handleScanResults()
{
  // скан завершен — возвращаем серверный heartbeat (см. ScanAsync / wsSuspendClientWatchdog)
  wsResumeClientWatchdog();
  ssidListHeapJson = "{}";
  _ssidList.clear();
  _passwordList.clear();
  jsonReadArray(settingsFlashJson, "routerssid", _ssidList);
  jsonReadArray(settingsFlashJson, "routerpass", _passwordList);
  int16_t numNetworks = WiFi.scanComplete();
  if (numNetworks <= 0)
  {
    SerialPrint("i", "WIFI", "no networks found");
    return;
  }

  // Ищем известные сети
  int connectNumNet = -1;
  // SerialPrint("i", "WIFI", "Count found: "+numNetworks);
  for (int n = 0; n < numNetworks; ++n)
  {
    String ssid = WiFi.SSID(n);
    jsonWriteStr_(ssidListHeapJson, String(n), ssid);
    for (size_t i = 0; i < _ssidList.size(); i++)
    {
      if (ssid == _ssidList[i])
      {
        Serial.printf("Found known network: %s (channel %d, RSSI %d dBm)\n", _ssidList[i].c_str(), (int)WiFi.channel(n), (int)WiFi.RSSI(n));
        knownChannel = (uint8_t)WiFi.channel(n);
        const uint8_t *kbss = WiFi.BSSID(n);
        if (kbss)
        {
          memcpy(knownBssid, kbss, 6);
          knownBssidValid = true;
        }
        if (connectNumNet < 0)
          connectNumNet = i;
      }
    }
    // if
  }
  sendStringToWs("ssidli", ssidListHeapJson, -1);  
  SerialPrint("i", "WIFI", "Scan Found: " + ssidListHeapJson);
  if (connectNumNet >= 0 && !isNetworkActive())
  {
    // ts.remove(WIFI_SCAN);
    connectToNextNetwork();
  }
  // checkConnection();
  // connectToSTA(_ssidList[connectNumNet], _passwordList[connectNumNet]);
  WiFi.scanDelete();
}

void WiFiUtilsItit()
{
#if !defined LIBRETINY
#if defined(esp32c6_4mb) || defined(esp32c6_8mb)
  WiFi.setAutoReconnect(false);
#else
  WiFi.setAutoConnect(false);
#endif
  WiFi.persistent(false); // Не загружать старые параметры из flash
  WiFi.setSleep(true);
#endif
  WiFi.disconnect(true);  // Очистить сохранённые WiFi параметры
  WiFi.mode(WIFI_STA);
  // Страна/каналы: по умолчанию RU (каналы 1-13). Без явной страны точка на канале
  // 12/13 видна в скане, но ассоциация с ней не проходит (в логе это reason 39/203).
  // Переопределяется ключом "wificountry" в settings.json
  String wifiCountry = jsonReadStr(settingsFlashJson, "wificountry");
  wifiCountry.trim();
  if (wifiCountry == "")
    wifiCountry = "RU";
#if defined(ESP32)
  if (wifiCountry.length() == 2)
  {
    if (esp_wifi_set_country_code(wifiCountry.c_str(), false) == ESP_OK)
      SerialPrint("i", "WIFI", "Country set to " + wifiCountry);
    else
      SerialPrint("E", "WIFI", "Country set ERROR: " + wifiCountry);
  }
#endif
  //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);  // сброс DHCP-имени
  String deviceName = jsonReadStr(settingsFlashJson, "name");  // Получаем имя в String
  Serial.print("[WiFi] Setting hostname: ");
  Serial.println(deviceName);
  deviceName.trim();
  WiFi.setHostname(deviceName.c_str());
  Serial.print("[WiFi] Actual hostname: ");
  Serial.println(WiFi.getHostname());
  WiFi.onEvent(WiFiEvent);
  _ssidList.clear();
  _passwordList.clear();
  jsonReadArray(settingsFlashJson, "routerssid", _ssidList);
  jsonReadArray(settingsFlashJson, "routerpass", _passwordList);

  if (_ssidList.empty() || _passwordList.empty())
  {
    SerialPrint("E", "WIFI", "No networks configured");
    startAPMode();
    return;
  }
  currentNetwork = 0;
  connectionAttempts = 0;
  connectToNextNetwork();
}

void connectToNextNetwork()
{
  // все сети перебрали
  if (currentNetwork >= _ssidList.size())
  {
    SerialPrint("i", "WIFI", "All networks tried");
    ts.remove(WIFI_CONN);
    startAPMode();
    return;
  }

  wifiConnecting = true;
  // connectionAttempts++;

  const char *ssid = _ssidList[currentNetwork].c_str();
  // защищаемся от рассинхрона массивов routerssid/routerpass (разные количества элементов)
  String passStr = (currentNetwork < _passwordList.size()) ? _passwordList[currentNetwork] : String("");
  const char *pass = passStr.c_str();
  // Пробуем подключиться к сети
  SerialPrint("i", "WIFI", "Connecting to: " + String(ssid));
  // КАКОЙ пароль реально идёт в эфир - видно сразу, так ловится рассинхрон массивов
  SerialPrint("i", "WIFI", "Using pass[" + String(currentNetwork) + ", pass count " + String(_passwordList.size()) + "]: \"" + passStr + "\" (" + String(passStr.length()) + " chars)");
  String deviceName = jsonReadStr(settingsFlashJson, "name");  // Получаем имя в String
  Serial.print("[WiFi] Setting hostname: ");
  Serial.println(deviceName);
  deviceName.trim();
  WiFi.setHostname(deviceName.c_str());
  Serial.print("[WiFi] Actual hostname: ");
  Serial.println(WiFi.getHostname());
  
  // Устанавливаем мощность передатчика ДО WiFi.begin (после начала подключения
  // смена мощности отрабатывается ненадёжно, esp_task_wdt_reset() из контекста
  // wifi-событий шумел "task not found" и убран - WDT следит за loopTask)
#if defined(ESP32)
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
#elif defined(ESP8266)
  WiFi.setOutputPower(20.5);
#endif

#if defined(ESP32)
  if (knownBssidValid)
  {
    WiFi.begin(ssid, pass, (int32_t)knownChannel, knownBssid);
  }
  else
#endif
  {
  WiFi.begin(ssid, pass);
  }

if (!MDNS.begin(deviceName.c_str())) {
        Serial.println("Error starting mDNS!");
    } else {
        Serial.println("mDNS started: " + deviceName + ".local");
    }
  // wifiReconnectTicker.once_ms(WIFI_CHECK_INTERVAL, checkConnection);
}

void checkConnection()
{
  ts.add(
      WIFI_CONN, 1000,
      [&](void *)
      {
        // esp_task_wdt_reset() убран: WDT следит за loopTask, который живёт независимо

        connectionAttempts++;
        if (WiFi.status() == WL_CONNECTED)
        {
          connectionAttempts = 0;
          wifiConnecting = false;
          return;
        }

        if (connectionAttempts >= (_ssidList.size() > 1 ? TRIESONE : TRIES))
        {
          SerialPrint("i", "WIFI", "Max attempts reached");
          currentNetwork++;
          connectionAttempts = 0;
          wifiConnecting = false;
        }

        if (wifiConnecting)
        {
#ifdef ESP8266
          if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_WRONG_PASSWORD)
#else
          if (WiFi.status() == WL_CONNECT_FAILED)
#endif
          {
            SerialPrint("E", "WIFI", "Connection failed, wrong password?");
            jsonWriteInt(errorsHeapJson, "passer", 1);
            wifiConnecting = false;
            currentNetwork++;
            connectionAttempts = 0;
          }

          // wifiReconnectTicker.once_ms(WIFI_CHECK_INTERVAL, checkConnection);
        }

        if (!wifiConnecting)
        {
          connectToNextNetwork();
        }
      },
      nullptr, true);
}

//------------------------------------------
// Неблокирующее подключение к STA
//------------------------------------------
void connectToSTA(const char *ssid, const char *pass)
{
  if (isNetworkActive())
    return;
  SerialPrint("i", "WIFI", "Connecting to ... " + String(ssid));
  // SerialPrint("i", "WIFI", "pass connect: " + _passwordList[i]);
  String deviceName = jsonReadStr(settingsFlashJson, "name");  // Получаем имя в String
  deviceName.trim();
  WiFi.setHostname(deviceName.c_str());
#if defined(ESP32)
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
#elif defined(ESP8266)
  WiFi.setOutputPower(20.5);
#endif
  WiFi.begin(ssid, pass); 

if (!MDNS.begin(deviceName.c_str())) {
        Serial.println("Error starting mDNS!");
    } else {
        Serial.println("mDNS started: " + deviceName + ".local");
    }
}

void ScanAsync()
{
  // bool res = false;
  int n = WiFi.scanComplete();
  SerialPrint("i", "WIFI", "scan result: " + String(n, DEC));

  if (n == -1)
  { // Сканирование все еще выполняется
    SerialPrint("i", "WIFI", "scanning in progress");
  }
  else
  {
    // радиомодуль уходит с домашнего канала, pong-и клиентов не доходят -
    // серверный heartbeat на время скана приостанавливаем (иначе он режет
    // живые соединения), вернём в handleScanResults
    wsSuspendClientWatchdog();
    SerialPrint("i", "WIFI", "start scanning");
    WiFi.scanNetworks(true, false);
  }
}
#else //WIFI_ASYNC

void routerConnect()
{
#if defined(esp32_wifirep)
//  Set custom dns server address for dhcp server
#define MY_DNS_IP_ADDR 0xC0A80401 // 192.168.4.1 // 0x08080808 // 8.8.8.8
  ip_addr_t dnsserver;

  String _ssidAP = jsonReadStr(settingsFlashJson, "apssid");
  String _passwordAP = jsonReadStr(settingsFlashJson, "appass");
  int _chanelAP = 0;
  jsonRead(settingsFlashJson, "wifirep_apchanel", _chanelAP);
  if (_chanelAP == 0)
    _chanelAP = 7;

  // WiFi.begin(ssid, password);
  WiFi.mode(WIFI_AP_STA);

  String s_apip = "";
  bool ap_ip = jsonRead(settingsFlashJson, "wifirep_apip", s_apip);
  if (ap_ip && s_apip != "")
  {
    WiFi.softAPConfig(stringToIp(s_apip), stringToIp(s_apip), stringToIp("255.255.255.0"));
    // bool softAPConfig(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dhcp_lease_start = (uint32_t) 0);
    dnsserver.u_addr.ip4.addr = stringToIp(s_apip);
  }
  else
    dnsserver.u_addr.ip4.addr = htonl(MY_DNS_IP_ADDR);

  dnsserver.type = IPADDR_TYPE_V4;
  dhcps_dns_setserver(&dnsserver);

  WiFi.softAP(_ssidAP.c_str(), _passwordAP.c_str(), _chanelAP, 0, 5);
  jsonWriteStr(settingsFlashJson, "ip", WiFi.softAPIP().toString());
  SerialPrint("i", "WIFI", "AP SSID: " + WiFi.softAPSSID());
  SerialPrint("i", "WIFI", "AP IP: " + WiFi.softAPIP().toString());
  SerialPrint("i", "WIFI", "AP pass: " + _passwordAP);

  String s_staip = "";
  bool static_ip = jsonRead(settingsFlashJson, "wifirep_staip", s_staip);
  String s_gateway = jsonReadStr(settingsFlashJson, "wifirep_gateway");
  String s_netmask = jsonReadStr(settingsFlashJson, "wifirep_netmask");
  String s_dns = jsonReadStr(settingsFlashJson, "wifirep_dns");

  if (static_ip == true && s_staip != "")
  {
    SerialPrint("i", "WIFI", "Use static IP");
    WiFi.config(stringToIp(s_staip), stringToIp(s_gateway), stringToIp(s_netmask), stringToIp(s_dns));
    // bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000);
    SerialPrint("i", "WIFI", "Static IP: " + s_staip);
    SerialPrint("i", "WIFI", "Gateway: " + s_gateway);
    SerialPrint("i", "WIFI", "Netmask: " + s_netmask);
    SerialPrint("i", "WIFI", "DNS: " + s_dns);
  }
#else
  WiFi.mode(WIFI_STA);
#endif

#if  !defined LIBRETINY  
#if defined(esp32c6_4mb) || defined(esp32c6_8mb)
  WiFi.setAutoReconnect(false);
#else
  WiFi.setAutoConnect(false);
#endif
  WiFi.persistent(false);
#endif
/*     String s_staip = "192.168.2.62";
    String s_gateway = "192.168.2.1";
    String s_netmask = "255.255.255.0";
    String s_dns = "192.168.2.1";
    SerialPrint("i", "WIFI", "Use static IP");
    WiFi.config(stringToIp(s_staip), stringToIp(s_gateway), stringToIp(s_netmask), stringToIp(s_dns));
    // bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000);
    SerialPrint("i", "WIFI", "Static IP: " + s_staip);
    SerialPrint("i", "WIFI", "Gateway: " + s_gateway);
    SerialPrint("i", "WIFI", "Netmask: " + s_netmask);
    SerialPrint("i", "WIFI", "DNS: " + s_dns); */
  //WiFi.mode(WIFI_STA);
  byte triesOne = TRIESONE;

  std::vector<String> _ssidList;
  std::vector<String> _passwordList;
  jsonReadArray(settingsFlashJson, "routerssid", _ssidList);
  jsonReadArray(settingsFlashJson, "routerpass", _passwordList);
  if (_ssidList.size() > 1)
    triesOne = TRIES;

  if (_passwordList.size() == 0 && _ssidList[0] == "" && _passwordList[0] == "")
  {
    #ifndef LIBRETINY
    WiFi.begin();
    #endif
  }
  else
  {
    WiFi.begin(_ssidList[0].c_str(), _passwordList[0].c_str());
#if defined (ESP32)
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
#elif defined (ESP8266)
    WiFi.setOutputPower(20.5);
#endif
    String _ssid;
    String _password;
    for (int8_t i = 0; i < _ssidList.size(); i++)
    {
      _ssid = _ssid + _ssidList[i] + "; ";
    }
    for (int8_t i = 0; i < _passwordList.size(); i++)
    {
      _password = _password + _passwordList[i] + "; ";
    }
    SerialPrint("i", "WIFI", "ssid list: " + _ssid);
    SerialPrint("i", "WIFI", "pass list: " + _password);
  }
  for (size_t i = 0; i < _ssidList.size(); i++)
  {
    triesOne = TRIESONE;
    if (WiFi.status() == WL_CONNECTED)
      break;
    WiFi.begin(_ssidList[i].c_str(), _passwordList[i].c_str());
    SerialPrint("i", "WIFI", "ssid connect: " + _ssidList[i]);
    SerialPrint("i", "WIFI", "pass connect: " + _passwordList[i]);
    while (--triesOne && WiFi.status() != WL_CONNECTED)
    {
//            SerialPrint("i", "WIFI", ": " + String((int)WiFi.status()));
#ifdef ESP8266
      if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_WRONG_PASSWORD)
#else
      if (WiFi.status() == WL_CONNECT_FAILED)
#endif
      {
        SerialPrint("E", "WIFI", "password is not correct");
        triesOne = 1;
        jsonWriteInt(errorsHeapJson, "passer", 1);
        break;
      }
      Serial.print(".");
      delay(1000);
    }
    Serial.println("");
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("");
    startAPMode();
  }
  else
  {
    Serial.println("");
#ifdef LIBRETINY
    SerialPrint("i", "WIFI", "http://" + ipToString(WiFi.localIP()));
    jsonWriteStr(settingsFlashJson, "ip", ipToString(WiFi.localIP()));
#else
    SerialPrint("i", "WIFI", "http://" + WiFi.localIP().toString());
    jsonWriteStr(settingsFlashJson, "ip", WiFi.localIP().toString());
#endif
    createItemFromNet("onWifi", "1", 1);

#if defined(esp32_wifirep)
    // Enable DNS (offer) for dhcp server
    dhcps_offer_t dhcps_dns_value = OFFER_DNS;
    dhcps_set_option_info(6, &dhcps_dns_value, sizeof(dhcps_dns_value));
    u32_t napt_netif_ip;
    if (ap_ip && s_apip != "")
      napt_netif_ip = stringToIp(s_apip);
    else
    {
      napt_netif_ip = 0xC0A80401; // Set to ip address of softAP netif (Default is 192.168.4.1)
      napt_netif_ip = htonl(napt_netif_ip);
    }
    // get_esp_interface_netif(ESP_IF_WIFI_AP)
    ip_napt_enable(napt_netif_ip, 1);
    // ip_napt_enable_no(ESP_IF_WIFI_AP, 1);

#endif

    mqttInit();
  }
  SerialPrint("i", F("WIFI"), F("Network Init"));
}
#endif
bool startAPMode()
{
#ifdef WIFI_ASYNC
  wifiConnecting = false;
  currentNetwork = 0;
  connectionAttempts = 0;
#endif
  SerialPrint("i", "WIFI", "AP Mode");

  WiFi.disconnect();
  WiFi.mode(WIFI_AP);

  String _ssidAP = jsonReadStr(settingsFlashJson, "apssid");
  String _passwordAP = jsonReadStr(settingsFlashJson, "appass");
  if (_passwordAP == "")
    WiFi.softAP(_ssidAP.c_str(), NULL, 6);
  else
    WiFi.softAP(_ssidAP.c_str(), _passwordAP.c_str(), 6);
  IPAddress myIP = WiFi.softAPIP();
#ifdef LIBRETINY
  SerialPrint("i", "WIFI", "AP IP: " + ipToString(myIP));
  jsonWriteStr(settingsFlashJson, "ip", ipToString(myIP));
#else
  SerialPrint("i", "WIFI", "AP IP: " + myIP.toString());
  jsonWriteStr(settingsFlashJson, "ip", myIP.toString());
#endif
  if (jsonReadInt(errorsHeapJson, "passer") != 1)
  {
    ts.add(
        WIFI_SCAN, 30 * 1000,
        [&](void *)
        {
#ifndef WIFI_ASYNC
          std::vector<String> jArray;
          jsonReadArray(settingsFlashJson, "routerssid", jArray);
          for (int8_t i = 0; i < jArray.size(); i++)
          {
            SerialPrint("i", "WIFI", "scanning for " + jArray[i]);
          }
          if (RouterFind(jArray))
          {
            ts.remove(WIFI_SCAN);
            WiFi.scanDelete();
            routerConnect();
          }
#else
          ScanAsync();
#endif
        },
        nullptr, true);
  }
  return true;
}

#ifndef WIFI_ASYNC
boolean RouterFind(std::vector<String> jArray)
{
  bool res = false;
  int n = WiFi.scanComplete();
  SerialPrint("i", "WIFI", "scan result: " + String(n, DEC));

  if (n == -1)
  { // Сканирование все еще выполняется. Безусловный scanDelete() в конце
    // функции стирал результаты только что пришедшего скана, поэтому список
    // сетей никогда не отдавался и скан приходилось запускать заново.
    SerialPrint("i", "WIFI", "scanning in progress");
    return res;
  }

  if (n == -2 || n == 0)
  { // Сканирование не было запущено (или найдено 0 сетей), запускаем
    // Скан уводит радиомодуль с домашнего канала, pong-и клиентов не доходят -
    // серверный heartbeat на время скана приостанавливаем (иначе сервер рвёт
    // живые соединения), вернём его когда соберем результаты ниже
    wsSuspendClientWatchdog();
    SerialPrint("i", "WIFI", "start scanning");
    WiFi.scanNetworks(true, false); // async, show_hidden
    return res;
  }

  // Сканирование завершено, есть результаты - собираем их и возвращаем heartbeat
  for (int8_t i = 0; i < n; i++)
  {
    for (size_t k = 0; k < jArray.size(); k++)
    {
      if (WiFi.SSID(i) == jArray[k])
      {
        res = true;
      }
    }
    jsonWriteStr_(ssidListHeapJson, String(i), WiFi.SSID(i));

    // String(WiFi.RSSI(i)
  }
  SerialPrint("i", "WIFI", ssidListHeapJson);
  wsResumeClientWatchdog();
  WiFi.scanDelete();
  return res;
}
#endif

boolean isNetworkActive()
{
  // Проверяем WiFi
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  
  // Проверяем Ethernet если доступен
#if defined(ESP32) || defined(ESP8266)
  if (isEthernetConnected()) {
    return true;
  }
#endif
  
  return false;
}

uint8_t getNumAPClients()
{
  return WiFi.softAPgetStationNum();
}

uint8_t RSSIquality()
{
  uint8_t res = 0;
  if (isNetworkActive())
  {
    int rssi = WiFi.RSSI();
    if (rssi >= -50)
    {
      res = 6; //"Excellent";
    }
    else if (rssi < -50 && rssi >= -60)
    {
      res = 5; //"Very good";
    }
    else if (rssi < -60 && rssi >= -70)
    {
      res = 4; //"Good";
    }
    else if (rssi < -70 && rssi >= -80)
    {
      res = 3; //"Low";
    }
    else if (rssi < -80 && rssi > -100)
    {
      res = 2; //"Very low";
    }
    else if (rssi <= -100)
    {
      res = 1; //"No signal";
    }
  }
  return res;
}

#ifdef LIBRETINY
String httpGetString(HTTPClient &http)
{
  String payload = "";
  int len = http.getSize();
  uint8_t buff[128] = {0};
  WiFiClient *stream = http.getStreamPtr();

  // read all data from server
  while (http.connected() && (len > 0 || len == -1))
  {
    // get available data size
    size_t size = stream->available();

    if (size)
    {
      // read up to 128 byte
      int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

      // write it to Serial
      //   Serial.write(buff,c);

      // payload += String((char*)buff);
      char charBuff[c + 1];        // Create a character array with space for null terminator
      memcpy(charBuff, buff, c);   // Copy the data to the character array
      charBuff[c] = '\0';          // Null-terminate the character array
      payload += String(charBuff); // Append the character array to the payload

      if (len > 0)
      {
        len -= c;
      }
    }
    delay(1);
  }
  return payload;
}
#endif