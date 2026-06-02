#pragma once

#include "Global.h"

#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
// Заглушка - Ethernet не поддерживается на ESP32-S3
#elif defined(ESP32)
#include <ETH.h>

// Для SPI Ethernet W5500 используем ручные настройки
#ifndef ETH_PHY_ADDR
#define ETH_PHY_ADDR        1
#endif
#ifndef ETH_PHY_POWER_PIN
#define ETH_PHY_POWER_PIN   -1
#endif
#ifndef ETH_PHY_MDC_PIN
#define ETH_PHY_MDC_PIN     23
#endif
#ifndef ETH_PHY_MDIO_PIN
#define ETH_PHY_MDIO_PIN    18
#endif

// Определяем W5500 через SPI - используем enum значение
#ifndef ETH_PHY_W5500
#define ETH_PHY_W5500 (eth_phy_type_t)5  //Значение по enum eth_phy_type_t для W5500
#endif

#ifndef ETH_PHY_TYPE
#define ETH_PHY_TYPE        ETH_PHY_W5500
#endif

#elif defined(ESP8266)
#include <Ethernet.h>
#endif

// Статус подключения Ethernet
enum EthernetStatus {
    ETH_DISCONNECTED = 0,
    ETH_CONNECTING = 1,
    ETH_CONNECTED = 2,
    ETH_GOT_IP = 3
};

// Инициализация Ethernet
void ethernetInit();

// Проверка подключения Ethernet
bool isEthernetConnected();

// Получение IP адреса Ethernet
String getEthernetIP();

// Получение MAC адреса Ethernet
String getEthernetMAC();

// Получение статуса Ethernet
EthernetStatus getEthernetStatus();

// Обработчик событий Ethernet
void onEthEvent(WiFiEvent_t event);

// Проверка подключения Ethernet (внутренняя функция)
void checkEthernetConnection();
