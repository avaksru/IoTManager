/*
 * OpenThermMonitor.h - Оптимизированная версия
 * Модуль управления OpenTherm для IoTManager
 */

#pragma once

// =======================================================================================================
// Конфигурация из OpenThermMonitor_config.h (объединено в один файл)
// =======================================================================================================
#include <Arduino.h>
#include <cstdint>

#define GPIO_IS_NOT_CONFIGURED 0xff

enum class SensorType : uint8_t { BOILER_OUTDOOR = 0, BOILER_RETURN = 4, MANUAL = 1, DS18B20 = 2, BLUETOOTH = 3 };

enum class UnitSystem : uint8_t { METRIC, IMPERIAL };

struct Settings
{
    struct
    {
        UnitSystem unitSystem = UnitSystem::METRIC;
    } system;

    struct
    {
        UnitSystem unitSystem = UnitSystem::METRIC;
        uint8_t inGpio = 14;
        uint8_t outGpio = 15;
        unsigned int memberIdCode = 0;
        bool dhwPresent = true;
        bool summerWinterMode = false;
        bool heatingCh2Enabled = true;
        bool heatingCh1ToCh2 = false;
        bool dhwToCh2 = false;
        bool dhwBlocking = false;
        bool nativeHeatingControl = false;
        bool immergasFix = false;
        uint8_t async = 3;
    } opentherm;

    struct
    {
        bool enable = true;
        bool recirculation = true;
        bool cooling = false;
    } heating;

    struct
    {
        bool enable = true;
    } dhw;

    struct
    {
        struct { SensorType type = SensorType::BOILER_OUTDOOR; uint8_t gpio = GPIO_IS_NOT_CONFIGURED; } outdoor;
        struct { SensorType type = SensorType::MANUAL; uint8_t gpio = GPIO_IS_NOT_CONFIGURED; } indoor;
    } sensors;

    char validationValue[8] = "stvalid";
} settings;

struct Variables
{
    struct
    {
        bool emergency = false;
        bool otStatus = false;
        bool heating = false;
        bool dhw = false;
        bool flame = false;
        bool fault = false;
        bool diagnostic = false;
    } states;

    struct
    {
        bool connected = false;
    } outdoor;

    struct
    {
        bool connected = false;
    } indoor;

    struct
    {
        bool heatingEnabled = false;
        uint8_t heatingMinTemp = 20;
        uint8_t heatingMaxTemp = 90;
        uint8_t dhwMinTemp = 30;
        uint8_t dhwMaxTemp = 60;
        uint8_t slaveMemberId = 0;
        uint8_t slaveFlags = 0;
        uint8_t slaveType = 0;
        uint8_t slaveVersion = 0;
        float slaveOtVersion = 0.0f;
        uint8_t masterMemberId = 0;
        uint8_t masterFlags = 0;
        uint8_t masterType = 0;
        uint8_t masterVersion = 0;
        float masterOtVersion = 0;
    } parameters;

    struct
    {
        bool resetFault = false;
        bool resetDiagnostic = false;
    } actions;

    struct
    {
        bool input = false;
    } cascadeControl;

    struct
    {
        struct { bool connected = false; } outdoor;
        struct { bool connected = false; } indoor;
    } sensors;
} vars;

inline float convertTemp(float value, const UnitSystem unitFrom, const UnitSystem unitTo)
{
    if (unitFrom == UnitSystem::METRIC && unitTo == UnitSystem::IMPERIAL)
        return (value * 9.0f / 5.0f) + 32.0f;
    else if (unitFrom == UnitSystem::IMPERIAL && unitTo == UnitSystem::METRIC)
        return (value - 32.0f) * 5.0f / 9.0f;
    return value;
}

#ifdef ARDUINO_ARCH_ESP32
#include <driver/gpio.h>
#else
#define GPIO_IS_VALID_GPIO(gpioNum) (gpioNum >= 0 && gpioNum <= 16)
#endif

#define GPIO_IS_VALID(gpioNum) (gpioNum != GPIO_IS_NOT_CONFIGURED && GPIO_IS_VALID_GPIO(gpioNum))

// =======================================================================================================
// Основной код модуля
// =======================================================================================================
// Предварительное объявление для friend
void handleInterrupt();

#include "Global.h"
#include "classes/IoTItem.h"
#include "OpenTherm.h"
#include <queue>
#include <map>

#define TIMEOUT_TRESHOLD 5

// Статическая функция для получения имени float-ID (замена designated initializers)
static String getFloatIDName(uint8_t id) {
    switch (id) {
        case 1:   return "OTget1";
        case 9:   return "OTget9";
        case 17:  return "OTget17";
        case 18:  return "OTget18";
        case 19:  return "OTget19";
        case 25:  return "OTget25";
        case 26:  return "OTget26";
        case 27:  return "OTget27";
        case 28:  return "OTget28";
        case 29:  return "OTget29";
        case 30:  return "OTget30";
        case 31:  return "OTget31";
        case 32:  return "OTget32";
        case 33:  return "OTget33";
        case 34:  return "OTget34";
        case 56:  return "OTget56";
        case 57:  return "OTget57";
        case 58:  return "OTget58";
        case 125: return "OTget125";
        default:  return "OTget" + String(id);
    }
}

class OpenThermMonitor : public IoTItem
{
private:
    // Переменные экземпляра (раньше были глобальными)
    int _debug = 0;
    bool _telegramInfo = false;
    std::map<String, String> _knownValues; // замена JsonDocument OpenThemDataGW
    std::deque<unsigned long> _requestQueue;
    unsigned long _lastSuccessResponse = 0;
    bool _async = true;
    unsigned long _timeoutCount = 0;
    bool _mqttIsConnect = false;
    String _instanceId;

    OpenTherm *_libOT = nullptr;

    // Константы
    const unsigned int _initializingInterval = 3600000;

    // Переменные состояния
    unsigned long _instanceCreatedTime = 0;
    uint8_t _instanceInGpio = 0;
    uint8_t _instanceOutGpio = 0;
    bool _isInitialized = false;
    unsigned long _initializedTime = 0;
    unsigned int _initializedMemberIdCode = 0;
    bool _heatingBlocking = false;
    int _ts = 0;
    uint8_t _offlineCHTemp = 50;
    bool _useOfflineMode = false;
    bool _oldOTinit = false;
    int _settingsDelay = 800;
    int _delay = 800;
    int _RX_pin;
    int _TX_pin;
    bool _netActive = false; // переименовано из _isNetworkActive (конфликт с isNetworkActive())

    int _currentWidgetIndex = 0;
    bool _nextIsBoilerRequest = true;
    std::vector<IoTItem *> _otgetWidgets;

    // Приватные методы
    void processOTsetWidgets();
    void processOTgetWidgets();
    unsigned long buildBoilerStatusRequest(bool enableCentralHeating, bool enableHotWater, bool enableCooling, 
                                           bool enableOutsideTemperatureCompensation, bool enableCentralHeating2, 
                                           bool summerWinterMode = false, bool dhwBlocking = false, uint8_t lb = 0);
    bool sendRemoteRequest(uint8_t code);
    void responseCallback(unsigned long result, OpenThermResponseStatus status);
    void handleReply(unsigned long response);
    void checkNew(String openthermID, String value);
    void addRequestQueue(unsigned long request);
    bool sendRequest2Channel();
    void sendTelegramm(String msg);

    // Инициализация OpenTherm
    void initialize();
    bool updateSlaveVersion();
    bool setMasterVersion(uint8_t version, uint8_t type);
    bool updateSlaveOtVersion();
    bool setMasterOtVersion(float version);
    bool updateSlaveConfig();
    bool setMasterConfig(uint8_t id, uint8_t flags, bool force = false);

    // Конвертеры
    template <class T>
    static unsigned int toFloat(const T val) { return (unsigned int)(val * 256); }
    static short getInt(const unsigned long response) { return response & 0xffff; }

    // Статический указатель для callback-обёртка
    static OpenThermMonitor* _currentInstance;
    static void _responseCallbackWrapper(unsigned long result, OpenThermResponseStatus status);

    friend void handleInterrupt();

public:
    OpenThermMonitor(String parameters);
    ~OpenThermMonitor();

    void doByInterval() override;
    void loop() override;
    void onModuleOrder(String &key, String &value) override;
    IoTValue execute(String command, std::vector<IoTValue> &param) override;
};