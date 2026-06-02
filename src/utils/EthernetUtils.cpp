#include <SPI.h>
#include "driver/spi_master.h"
#include "utils/EthernetUtils.h"
#include "utils/SerialPrint.h"
#include "utils/JsonUtils.h"

// Для ESP32-S3 и esp32_4mb3f используем заглушки (несовместимость с версией платформы)
#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(esp32_4mb3f)

static EthernetStatus ethStatus = ETH_DISCONNECTED;

void ethernetInit() {
    SerialPrint("w", "ETH", "Ethernet not supported on this platform");
}

bool isEthernetConnected() {
    return false;
}

String getEthernetIP() {
    return "";
}

String getEthernetMAC() {
    return "";
}

EthernetStatus getEthernetStatus() {
    return ETH_DISCONNECTED;
}

void checkEthernetConnection() {
    // Не поддерживается
}

void onEthEvent(WiFiEvent_t event) {
    // Не поддерживается
}

#elif defined(ESP32)

static EthernetStatus ethStatus = ETH_DISCONNECTED;
static bool ethInitialized = false;
static bool ethConnected = false;
static String ethIP = "";
static String ethMAC = "";

// Обработчик событий Ethernet
void onEthEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START: {
            SerialPrint("i", "ETH", "Ethernet started");
            ethStatus = ETH_CONNECTING;
            // Устанавливаем hostname
            String deviceName = jsonReadStr(settingsFlashJson, "name");  
            deviceName.trim();
            ETH.setHostname(deviceName.c_str());
            break;
        }
            
        case ARDUINO_EVENT_ETH_CONNECTED:
            SerialPrint("i", "ETH", "Ethernet connected");
            ethConnected = true;
            ethStatus = ETH_CONNECTED;
            break;
            
        case ARDUINO_EVENT_ETH_GOT_IP:
            ethIP = ETH.localIP().toString();
            ethMAC = ETH.macAddress();
            ethStatus = ETH_GOT_IP;
            SerialPrint("i", "ETH", "Got IP: " + ethIP);
            SerialPrint("i", "ETH", "MAC: " + ethMAC);
            
            // Сохраняем IP в настройки
            jsonWriteStr(settingsFlashJson, "ip", ethIP);
            jsonWriteStr_(settingsFlashJson, "ethIP", ethIP);
            jsonWriteStr_(settingsFlashJson, "ethMAC", ethMAC);
            
            // Запускаем MQTT если еще не запущен
            if (!mqttIsConnect()) {
                mqttInit();
            }
            break;
            
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            SerialPrint("e", "ETH", "Ethernet disconnected");
            ethConnected = false;
            ethStatus = ETH_DISCONNECTED;
            ethIP = "";
            break;
            
        case ARDUINO_EVENT_ETH_STOP:
            SerialPrint("e", "ETH", "Ethernet stopped");
            ethConnected = false;
            ethStatus = ETH_DISCONNECTED;
            ethIP = "";
            break;
            
        default:
            break;
    }
}

void ethernetInit() {
    if (ethInitialized) {
        SerialPrint("w", "ETH", "Ethernet already initialized");
        return;
    }
    
    SerialPrint("i", "ETH", "Initializing W5500 Ethernet...");
    
    // Получаем пины из конфигурации
    int csPin = 5;      // CS пин для W5500
    int intPin = 4;     // INT пин для W5500
    int rstPin = -1;    // RST пин (не используется)
    
    jsonRead(settingsFlashJson, "ethCsPin", csPin);
    jsonRead(settingsFlashJson, "ethIntPin", intPin);
    jsonRead(settingsFlashJson, "ethRstPin", rstPin);
    
    SerialPrint("i", "ETH", "CS Pin: " + String(csPin));
    SerialPrint("i", "ETH", "INT Pin: " + String(intPin));
    
    // Инициализация SPI для W5500
    #if defined(esp32c6_8mb)
        // ESP32-C6: SCK=GPIO6, MISO=GPIO5, MOSI=GPIO7
        SPI.begin(18, 19, 20, csPin);  // SCK, MISO, MOSI, CS
    #else
        // Для других ESP32
        SPI.begin(18, 19, 23, csPin);  // SCK, MISO, MOSI, CS
    #endif
    
    // Запускаем Ethernet
    // Для ESP32-C6 функция ETH.begin() требует 7 аргументов: type, phy_addr, cs, irq, rst, spi, spi_freq_mhz
    #ifdef ETH_PHY_W5500
        ETH.begin(ETH_PHY_W5500, 1, csPin, intPin, rstPin, SPI, ETH_PHY_SPI_FREQ_MHZ);
    #endif
    
    ethInitialized = true;
    SerialPrint("i", "ETH", "Ethernet initialization complete");
}

bool isEthernetConnected() {
    return ethConnected && ethStatus == ETH_GOT_IP;
}

String getEthernetIP() {
    return ethIP;
}

String getEthernetMAC() {
    return ethMAC;
}

EthernetStatus getEthernetStatus() {
    return ethStatus;
}

void checkEthernetConnection() {
    // Проверяем статус подключения
    if (ETH.linkUp()) {
        // Кабель подключен
        if (ethStatus == ETH_DISCONNECTED) {
            ethStatus = ETH_CONNECTING;
            SerialPrint("i", "ETH", "Ethernet connecting...");
            
            // Устанавливаем hostname
            String deviceName = jsonReadStr(settingsFlashJson, "name");  
            ETH.setHostname(deviceName.c_str());
        }
        
        // Проверяем IP
        IPAddress currentIP = ETH.localIP();
        
        if (currentIP != INADDR_NONE && currentIP.toString() != "0.0.0.0") {
            // IP получен
            if (!ethConnected || ethIP != currentIP.toString()) {
                ethIP = currentIP.toString();
                ethMAC = ETH.macAddress();
                ethConnected = true;
                ethStatus = ETH_GOT_IP;
                SerialPrint("i", "ETH", "Got IP: " + ethIP);
                SerialPrint("i", "ETH", "MAC: " + ethMAC);
                
                // Сохраняем IP в настройки
                jsonWriteStr(settingsFlashJson, "ip", ethIP);
                jsonWriteStr_(settingsFlashJson, "ethIP", ethIP);
                jsonWriteStr_(settingsFlashJson, "ethMAC", ethMAC);
                
                // Запускаем MQTT если еще не запущен
                if (!mqttIsConnect()) {
                    mqttInit();
                }
            }
        } else if (ethConnected) {
            // Связь есть но IP еще не получен
            SerialPrint("i", "ETH", "Link up, waiting for IP...");
        }
    } else {
        // Кабель отключен
        if (ethConnected) {
            SerialPrint("e", "ETH", "Link down");
            ethConnected = false;
            ethStatus = ETH_DISCONNECTED;
            ethIP = "";
        }
    }
}

#elif defined(ESP8266)

// Заглушки для ESP8266
static EthernetStatus ethStatus = ETH_DISCONNECTED;

void ethernetInit() {
    SerialPrint("w", "ETH", "Ethernet not supported on this platform");
}

bool isEthernetConnected() {
    return false;
}

String getEthernetIP() {
    return "";
}

String getEthernetMAC() {
    return "";
}

EthernetStatus getEthernetStatus() {
    return ETH_DISCONNECTED;
}

void checkEthernetConnection() {
    // Не поддерживается
}

void onEthEvent(WiFiEvent_t event) {
    // Не поддерживается
}

#endif
