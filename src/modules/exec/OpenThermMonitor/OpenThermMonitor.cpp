#include "OpenThermMonitor.h"

// ============================================================================
// Функции из IoTManager (объявлены в Global.h)
// ============================================================================
extern bool isNetworkActive();
extern bool mqttIsConnect();

// ============================================================================
// Статический указатель экземпляра для callback-обёртки
// ============================================================================
OpenThermMonitor* OpenThermMonitor::_currentInstance = nullptr;

void OpenThermMonitor::_responseCallbackWrapper(unsigned long result, OpenThermResponseStatus status) {
    if (_currentInstance) {
        _currentInstance->responseCallback(result, status);
    }
}

// ============================================================================
// ISR-совместимая обёртка handleInterrupt
// ============================================================================
void handleInterrupt()
{
    if (OpenThermMonitor::_currentInstance != nullptr && OpenThermMonitor::_currentInstance->_libOT != nullptr)
        OpenThermMonitor::_currentInstance->_libOT->handleInterrupt();
}

// =======================================================================================================
// Конструктор
// =======================================================================================================
OpenThermMonitor::OpenThermMonitor(String parameters) : IoTItem(parameters)
{
    jsonRead(parameters, "RX_pin", _RX_pin);
    jsonRead(parameters, "TX_pin", _TX_pin);
    _debug = jsonReadInt(parameters, "LogLevel");
    settings.heating.enable = bool(jsonReadInt(parameters, "centralHeating"));
    settings.opentherm.dhwPresent = bool(jsonReadInt(parameters, "hotWater"));
    settings.opentherm.summerWinterMode = bool(jsonReadInt(parameters, "summerMode"));
    settings.opentherm.heatingCh2Enabled = bool(jsonReadInt(parameters, "centralHeating2"));
    settings.opentherm.nativeHeatingControl = bool(jsonReadInt(parameters, "weatherThermostat"));
    _offlineCHTemp = jsonReadInt(parameters, "offlineCHTemp");
    _useOfflineMode = bool(jsonReadInt(parameters, "useOfflineMode"));
    vars.parameters.dhwMinTemp = jsonReadInt(parameters, "tempDHWmin");
    vars.parameters.dhwMaxTemp = jsonReadInt(parameters, "tempDHWmax");
    vars.parameters.heatingMinTemp = jsonReadInt(parameters, "tempCHmin");
    vars.parameters.heatingMaxTemp = jsonReadInt(parameters, "tempCHmax");
    _settingsDelay = jsonReadInt(parameters, "libOTfreq");
    _telegramInfo = bool(jsonReadInt(parameters, "telegram"));
    settings.opentherm.async = jsonReadInt(parameters, "async");
    settings.opentherm.inGpio = byte(_RX_pin);
    settings.opentherm.outGpio = byte(_TX_pin);
    _instanceId = String(_id.c_str());

    SerialPrint("i", F("ot"), " LogLevel " + String(_debug));

    IoTItem *item = findIoTItem("BoilerEnable");
    if (item)
    {
        settings.heating.enable = atof(item->getValue().c_str());
    }
    item = findIoTItem("DHWenable");
    if (item)
    {
        settings.dhw.enable = atof(item->getValue().c_str());
    }

    // Удаляем старый экземпляр библиотеки
    if (_currentInstance != nullptr)
    {
        delete _libOT;
        _libOT = nullptr;
        SerialPrint("i", F("ot"), F("Stopped"));
    }

    if (!GPIO_IS_VALID(settings.opentherm.inGpio) || !GPIO_IS_VALID(settings.opentherm.outGpio))
    {
        SerialPrint("i", F("ot"), "NlibOT started. GPIO IN: " + String(settings.opentherm.inGpio) + " or GPIO OUT: " + String(settings.opentherm.outGpio) + " is not valid");
        return;
    }

    if (settings.system.unitSystem != UnitSystem::METRIC)
    {
        vars.parameters.heatingMinTemp = convertTemp(vars.parameters.heatingMinTemp, UnitSystem::METRIC, settings.system.unitSystem);
        vars.parameters.heatingMaxTemp = convertTemp(vars.parameters.heatingMaxTemp, UnitSystem::METRIC, settings.system.unitSystem);
        vars.parameters.dhwMinTemp = convertTemp(vars.parameters.dhwMinTemp, UnitSystem::METRIC, settings.system.unitSystem);
        vars.parameters.dhwMaxTemp = convertTemp(vars.parameters.dhwMaxTemp, UnitSystem::METRIC, settings.system.unitSystem);
    }

    this->_instanceCreatedTime = millis();
    this->_instanceInGpio = settings.opentherm.inGpio;
    this->_instanceOutGpio = settings.opentherm.outGpio;
    this->_isInitialized = false;

    SerialPrint("i", F("ot"), "Started. GPIO IN: " + String(settings.opentherm.inGpio) + "  GPIO OUT: " + String(settings.opentherm.outGpio));
    checkNew(String(_id.c_str()), "➖");
    checkNew("isHeatingEnabled", "➖");
    checkNew("isDHWenabled", "➖");
    checkNew("isFlameOn", "➖");
    checkNew("OTget25", String(0.00f));
    checkNew("OTget26", String(0.00f));
    checkNew("OTget17", String(0.00f));

    SerialPrint("i", F("ot"), " Start OpenTherm.... ");

    // Создаём экземпляр OpenTherm
    _libOT = new OpenTherm(settings.opentherm.inGpio, settings.opentherm.outGpio);
    _currentInstance = this;

    // Колбэк через статическую обёртку + указатель на экземпляр
    _libOT->begin(handleInterrupt, _responseCallbackWrapper);
}

// =======================================================================================================
// Деструктор
// =======================================================================================================
OpenThermMonitor::~OpenThermMonitor()
{
    if (_libOT != nullptr)
    {
        delete _libOT;
        _libOT = nullptr;
    }
    if (_currentInstance == this)
        _currentInstance = nullptr;
}

// =======================================================================================================
// doByInterval() — периодические задачи
// =======================================================================================================
void OpenThermMonitor::doByInterval()
{
    if (_oldOTinit == true)
    {
        SerialPrint("i", F("ot"), "setBoilerStatus: CH = " + String(bool(settings.heating.enable)) +
            ", DHW = " + String(bool(settings.opentherm.dhwPresent && settings.dhw.enable)) +
            ", Cooling = " + String(false) +
            ", nativeThermostat = " + String(bool(settings.opentherm.nativeHeatingControl)) +
            ", CH2 = " + String(bool(settings.opentherm.heatingCh2Enabled)));
    }
    else
    {
        SerialPrint("i", F("ot"), "setBoilerStatus: CH = " + String(bool(settings.heating.enable)) +
            ", DHW = " + String(bool(settings.opentherm.dhwPresent && settings.dhw.enable)) +
            ", Cooling = " + String(false) +
            ", nativeThermostat = " + String(bool(settings.opentherm.nativeHeatingControl)) +
            ", CH2 = " + String(bool(settings.opentherm.heatingCh2Enabled)) +
            ", summerMode = " + String(bool(settings.opentherm.summerWinterMode)) +
            ", dhwBlocking = " + String(bool(settings.opentherm.dhwBlocking)) +
            ", immergasFix = " + String(bool(settings.opentherm.immergasFix)));
    }

    // Обработка OTset виджетов
    processOTsetWidgets();

    if (_debug > 0)
    {
        SerialPrint("i", F("ot"), "memoryUsage: " + String(_knownValues.size()));
    }
    _knownValues.clear();
}

// =======================================================================================================
// processOTsetWidgets() — вынесенная обработка OTset виджетов
// =======================================================================================================
void OpenThermMonitor::processOTsetWidgets()
{
    for (std::list<IoTItem *>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it)
    {
        if ((*it)->isStrInID("OTset"))
        {
            String widgetID = String((*it)->getID().c_str());
            widgetID.replace("OTset", "");
            int id = widgetID.toInt();
            unsigned int data;

            if (id == 56 || id == 57 || id == 58 || id == 1 || id == 7 || id == 8 || id == 14 || id == 16 || id == 23 || id == 24 || id == 124)
            {
                data = _libOT->temperatureToData(atoi((*it)->getValue().c_str()));
            }
            else
            {
                data = atoi((*it)->getValue().c_str());
            }

            // Offline режим
            if (vars.states.emergency && id == 1)
            {
                data = _libOT->temperatureToData(_offlineCHTemp);
                SerialPrint("i", F("ot"), "Offline режим. Установлена температура CH: " + String(_offlineCHTemp));
                checkNew("OTset1", String(_offlineCHTemp));
            }

            unsigned long request = _libOT->buildRequestID(OpenThermRequestType::WRITE_DATA, id, data);
            addRequestQueue(request);
        }
    }
}

// =======================================================================================================
// processOTgetWidgets() — вынесенная обработка OTget виджетов
// =======================================================================================================
void OpenThermMonitor::processOTgetWidgets()
{
    if (_otgetWidgets.empty())
    {
        for (std::list<IoTItem *>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it)
        {
            if ((*it)->isStrInID("OTget"))
            {
                _otgetWidgets.push_back(*it);
            }
        }
    }

    if (!_otgetWidgets.empty())
    {
        IoTItem *item = _otgetWidgets[_currentWidgetIndex];
        String widgetID = String(item->getID().c_str());
        widgetID.replace("OTget", "");
        int id = widgetID.toInt();

        unsigned long request = _libOT->buildRequestID(OpenThermRequestType::READ_DATA, id, 0x0000);
        addRequestQueue(request);

        _currentWidgetIndex++;
        if (_currentWidgetIndex >= _otgetWidgets.size())
        {
            _currentWidgetIndex = 0;
        }
    }
}

// =======================================================================================================
// buildBoilerStatusRequest() — объединение buildRequestBoilerStatus + buildSetBoilerStatusRequest
// =======================================================================================================
unsigned long OpenThermMonitor::buildBoilerStatusRequest(
    bool enableCentralHeating, bool enableHotWater, bool enableCooling,
    bool enableOutsideTemperatureCompensation, bool enableCentralHeating2,
    bool summerWinterMode, bool dhwBlocking, uint8_t lb)
{
    unsigned int data = enableCentralHeating | (enableHotWater << 1) | (enableCooling << 2) |
                        (enableOutsideTemperatureCompensation << 3) | (enableCentralHeating2 << 4) |
                        (summerWinterMode << 5) | (dhwBlocking << 6);
    data <<= 8;
    if (settings.opentherm.immergasFix)
    {
        data |= lb;
    }
    return _libOT->buildRequest(
        OpenThermMessageType::READ_DATA,
        OpenThermMessageID::Status,
        data);
}

// =======================================================================================================
// sendRemoteRequest() — объединение sendBoilerReset + sendServiceReset
// =======================================================================================================
bool OpenThermMonitor::sendRemoteRequest(uint8_t code)
{
    unsigned int data = code;
    data <<= 8;
    unsigned long response = _libOT->sendRequest(_libOT->buildRequest(
        OpenThermMessageType::WRITE_DATA,
        OpenThermMessageID::RemoteRequest,
        data));

    return _libOT->isValidResponse(response) && _libOT->isValidResponseID(response, OpenThermMessageID::RemoteRequest);
}

// =======================================================================================================
// Вспомогательные методы инициализации OpenTherm
// =======================================================================================================
bool OpenThermMonitor::updateSlaveVersion()
{
    unsigned long response = _libOT->sendRequest(_libOT->buildRequest(
        OpenThermRequestType::READ_DATA,
        OpenThermMessageID::SlaveVersion, 0));

    if (!_libOT->isValidResponse(response)) return false;
    if (!_libOT->isValidResponseID(response, OpenThermMessageID::SlaveVersion)) return false;

    vars.parameters.slaveVersion = response & 0xFF;
    vars.parameters.slaveType = (response & 0xFFFF) >> 8;
    return true;
}

bool OpenThermMonitor::setMasterVersion(uint8_t version, uint8_t type)
{
    unsigned long response = _libOT->sendRequest(_libOT->buildRequest(
        OpenThermRequestType::WRITE_DATA,
        OpenThermMessageID::MasterVersion,
        (unsigned int)version | (unsigned int)type << 8));

    if (!_libOT->isValidResponse(response)) return false;
    if (!_libOT->isValidResponseID(response, OpenThermMessageID::MasterVersion)) return false;

    vars.parameters.masterVersion = response & 0xFF;
    vars.parameters.masterType = (response & 0xFFFF) >> 8;
    return true;
}

bool OpenThermMonitor::updateSlaveOtVersion()
{
    unsigned long response = _libOT->sendRequest(_libOT->buildRequest(
        OpenThermRequestType::READ_DATA,
        OpenThermMessageID::OpenThermVersionSlave, 0));

    if (!_libOT->isValidResponse(response)) return false;
    if (!_libOT->isValidResponseID(response, OpenThermMessageID::OpenThermVersionSlave)) return false;

    vars.parameters.slaveOtVersion = _libOT->getFloat(response);
    return true;
}

bool OpenThermMonitor::setMasterOtVersion(float version)
{
    unsigned long response = _libOT->sendRequest(_libOT->buildRequest(
        OpenThermRequestType::WRITE_DATA,
        OpenThermMessageID::OpenThermVersionMaster,
        toFloat(version)));

    if (!_libOT->isValidResponse(response)) return false;
    if (!_libOT->isValidResponseID(response, OpenThermMessageID::OpenThermVersionMaster)) return false;

    vars.parameters.masterOtVersion = _libOT->getFloat(response);
    return true;
}

bool OpenThermMonitor::updateSlaveConfig()
{
    unsigned long response = _libOT->sendRequest(_libOT->buildRequest(
        OpenThermRequestType::READ_DATA,
        OpenThermMessageID::SConfigSMemberIDcode, 0));

    if (!_libOT->isValidResponse(response)) return false;
    if (!_libOT->isValidResponseID(response, OpenThermMessageID::SConfigSMemberIDcode)) return false;

    vars.parameters.slaveMemberId = response & 0xFF;
    vars.parameters.slaveFlags = (response & 0xFFFF) >> 8;
    return true;
}

bool OpenThermMonitor::setMasterConfig(uint8_t id, uint8_t flags, bool force)
{
    vars.parameters.masterMemberId = (force || id || settings.opentherm.memberIdCode > 65535)
                                         ? id
                                         : vars.parameters.slaveMemberId;

    vars.parameters.masterFlags = (force || flags || settings.opentherm.memberIdCode > 65535)
                                       ? flags
                                       : vars.parameters.slaveFlags;

    unsigned int request = (unsigned int)vars.parameters.masterMemberId | (unsigned int)vars.parameters.masterFlags << 8;
    if (!request) return true;

    unsigned long response = _libOT->sendRequest(_libOT->buildRequest(
        OpenThermRequestType::WRITE_DATA,
        OpenThermMessageID::MConfigMMemberIDcode, request));

    return _libOT->isValidResponse(response) && _libOT->isValidResponseID(response, OpenThermMessageID::MConfigMMemberIDcode);
}

void OpenThermMonitor::initialize()
{
    if (updateSlaveVersion())
    {
        if (_debug > 0)
            SerialPrint("i", F("ot"), "Received slave version: " + String(vars.parameters.slaveVersion) + ", type: " + String(vars.parameters.slaveType));
    }
    else
    {
        if (_debug > 0)
            SerialPrint("E", F("ot"), F("Failed receive slave version"));
    }

    if (setMasterVersion(0x3F, 0x01))
    {
        if (_debug > 0)
            SerialPrint("i", F("ot"), "Set master version: " + String(vars.parameters.masterVersion) + ", type: " + String(vars.parameters.masterType));
    }
    else
    {
        if (_debug > 0)
            SerialPrint("E", F("ot"), F("Failed set master version"));
    }

    if (updateSlaveOtVersion())
    {
        if (_debug > 0)
            SerialPrint("i", F("ot"), "Received slave OT version: " + String(vars.parameters.slaveOtVersion));
    }
    else
    {
        if (_debug > 0)
            SerialPrint("E", F("ot"), F("Failed receive slave OT version"));
    }

    if (setMasterOtVersion(2.2f))
    {
        if (_debug > 0)
            SerialPrint("i", F("ot"), "Set master OT version: " + String(vars.parameters.masterOtVersion));
    }
    else
    {
        if (_debug > 0)
            SerialPrint("E", F("ot"), F("Failed set master OT version"));
    }

    if (updateSlaveConfig())
    {
        if (_debug > 0)
            SerialPrint("i", F("ot"), "Received slave member id: " + String(vars.parameters.slaveMemberId) + ", flags: " + String(vars.parameters.slaveFlags));
    }
    else
    {
        if (_debug > 0)
            SerialPrint("E", F("ot"), F("Failed receive slave config"));
    }

    if (setMasterConfig(settings.opentherm.memberIdCode & 0xFF, (settings.opentherm.memberIdCode & 0xFFFF) >> 8))
    {
        if (_debug > 0)
            SerialPrint("i", F("ot"), "Set master member id: " + String(vars.parameters.masterMemberId) + ", flags: " + String(vars.parameters.masterFlags));
    }
    else
    {
        if (_debug > 0)
            SerialPrint("E", F("ot"), F("Failed set master config"));
    }
}

// =======================================================================================================
// Основной цикл
// =======================================================================================================
void OpenThermMonitor::loop()
{
    _libOT->process();

#ifndef ESP32_FREERTOS
    if (_libOT->isReady())
        sendRequest2Channel();
#endif

    unsigned long new_ts = millis();
    if (_delay < 800)
        _delay = 800;
    if (new_ts - _ts > _delay)
    {
        _netActive = isNetworkActive();
        _mqttIsConnect = mqttIsConnect();

        if (settings.opentherm.async == 0)
            _async = false;
        if (settings.opentherm.async == 1)
            _async = true;

        if (_debug > 0)
        {
            String asyncStr = _async ? "async" : "";
            if (new_ts - _ts < 1100 && _debug > 2)
            {
                SerialPrint("i", F("ot"), "delay: " + String(new_ts - _ts) + " Queue.size: " + String(_requestQueue.size()) + " " + asyncStr);
            }
            else if (_debug > 2)
            {
                SerialPrint("E", F("ot"), "delay: " + String(new_ts - _ts) + " Queue.size: " + String(_requestQueue.size()) + " " + asyncStr);
            }
        }
        _ts = new_ts;

        if (_instanceInGpio != settings.opentherm.inGpio || _instanceOutGpio != settings.opentherm.outGpio)
        {
            // Пересоздаём экземпляр при смене пинов
            if (_libOT != nullptr) {
                delete _libOT;
            }
            _libOT = new OpenTherm(settings.opentherm.inGpio, settings.opentherm.outGpio);
            _currentInstance = this;
            _libOT->begin(handleInterrupt, _responseCallbackWrapper);
        }
        else if ((_initializedMemberIdCode != settings.opentherm.memberIdCode || millis() - _initializedTime > _initializingInterval) && _isInitialized == true)
        {
            _isInitialized = false;
            if (_debug > 0 && _mqttIsConnect)
            {
                publishData("OTlogs", "Переводим котел в Slave режим по расписанию");
            }
        }

        if (_libOT == nullptr)
            return;

        if (_useOfflineMode && (!_netActive || !_mqttIsConnect))
            vars.states.emergency = true;
        else
            vars.states.emergency = false;

        bool heatingEnabled = (vars.states.emergency || settings.heating.enable);
        bool heatingCh2Enabled = settings.opentherm.heatingCh2Enabled;
        if (settings.opentherm.heatingCh1ToCh2)
        {
            heatingCh2Enabled = heatingEnabled;
        }
        else if (settings.opentherm.dhwToCh2)
        {
            heatingCh2Enabled = settings.opentherm.dhwPresent && settings.dhw.enable;
        }

        uint8_t statusLb = 0;
        if (settings.opentherm.immergasFix)
        {
            statusLb = 0xCA;
        }

        // OTget виджеты — собираем один раз (перенесено в processOTgetWidgets)
        // processOTgetWidgets вызывается ниже

        // Устанавливаем статус котла и OTget запросы
        if (_nextIsBoilerRequest)
        {
            // Используем общий buildBoilerStatusRequest
            unsigned long request = buildBoilerStatusRequest(
                heatingEnabled,
                settings.opentherm.dhwPresent && settings.dhw.enable,
                false,
                settings.opentherm.nativeHeatingControl,
                heatingCh2Enabled,
                settings.opentherm.summerWinterMode,
                settings.opentherm.dhwBlocking,
                statusLb);

            if (_debug > 2)
            {
                if (_oldOTinit)
                {
                    SerialPrint("i", F("ot"), "setBoilerStatus: CH = " + String(bool(settings.heating.enable)) +
                                 ", DHW = " + String(bool(settings.opentherm.dhwPresent && settings.dhw.enable)) +
                                 ", Cooling = " + String(false) +
                                 ", nativeThermostat = " + String(bool(settings.opentherm.nativeHeatingControl)) +
                                 ", CH2 = " + String(bool(settings.opentherm.heatingCh2Enabled)));
                }
                else
                {
                    SerialPrint("i", F("ot"), "setBoilerStatus: CH = " + String(bool(settings.heating.enable)) +
                                 ", DHW = " + String(bool(settings.opentherm.dhwPresent && settings.dhw.enable)) +
                                 ", Cooling = " + String(false) +
                                 ", nativeThermostat = " + String(bool(settings.opentherm.nativeHeatingControl)) +
                                 ", CH2 = " + String(bool(settings.opentherm.heatingCh2Enabled)) +
                                 ", summerMode = " + String(bool(settings.opentherm.summerWinterMode)) +
                                 ", dhwBlocking = " + String(bool(settings.opentherm.dhwBlocking)) +
                                 ", immergasFix = " + String(bool(settings.opentherm.immergasFix)));
                }
            }
            addRequestQueue(request);
            _nextIsBoilerRequest = false;
        }
        else
        {
            processOTgetWidgets();
            _nextIsBoilerRequest = true;
        }

        // (аварийный режим || отопление разрешено) && каскад && isReady() && !heatingBlocking
        heatingEnabled = (vars.states.emergency || settings.heating.enable) && vars.cascadeControl.input && _libOT->isReady() && !_heatingBlocking;
        heatingCh2Enabled = settings.opentherm.heatingCh2Enabled;
        if (settings.opentherm.heatingCh1ToCh2)
        {
            heatingCh2Enabled = heatingEnabled;
        }
        else if (settings.opentherm.dhwToCh2)
        {
            heatingCh2Enabled = settings.opentherm.dhwPresent && settings.dhw.enable;
        }

        // Проверка статуса подключения
        if (!vars.states.otStatus && millis() - _lastSuccessResponse < 1150)
        {
            if (_debug > 0 && _mqttIsConnect)
            {
                SerialPrint("i", F("ot"), F("Connected!"));
                publishData("OTlogs", "Connected!");
            }
            _delay = _settingsDelay;
            vars.states.otStatus = true;
        }
        else if (vars.states.otStatus && millis() - _lastSuccessResponse > 1150)
        {
            if (_debug > 0 && _mqttIsConnect)
            {
                SerialPrint("E", F("ot"), "Timeout " + String(millis() - _lastSuccessResponse));
                publishData("OTlogs", "Timeout " + String(millis() - _lastSuccessResponse));
            }

            if (settings.sensors.outdoor.type == SensorType::BOILER_OUTDOOR)
                vars.sensors.outdoor.connected = false;
            if (settings.sensors.indoor.type == SensorType::BOILER_RETURN)
                vars.sensors.indoor.connected = false;

            vars.states.otStatus = false;
            _isInitialized = false;
        }

        // Если котел отключен
        if (!vars.states.otStatus)
        {
            vars.states.heating = false;
            vars.states.dhw = false;
            vars.states.flame = false;
            vars.states.fault = false;
            vars.states.diagnostic = false;
            _delay = 1100;
            return;
        }

        // Переинициализация
        if (!_isInitialized && _oldOTinit == false)
        {
            SerialPrint("i", F("ot"), F("Initializing..."));
            if (_debug > 0 && _mqttIsConnect)
                publishData("OTlogs", "Initializing...");

            _requestQueue.resize(1);
            _isInitialized = true;
            _initializedTime = millis();
            _initializedMemberIdCode = settings.opentherm.memberIdCode;
            initialize();
        }

        // Вкл/выкл отопления
        if (vars.parameters.heatingEnabled != heatingEnabled)
        {
            vars.parameters.heatingEnabled = heatingEnabled;
            SerialPrint("i", F("ot"), "heatingEnabled: " + String(heatingEnabled ? F("Enabled") : F("Disabled")));
        }
    }

    // Fault reset action (sendRemoteRequest(1))
    if (vars.actions.resetFault)
    {
        if (vars.states.fault)
        {
            if (sendRemoteRequest(1))
                SerialPrint("i", F("ot"), F("Boiler fault reset successfully"));
            else
                SerialPrint("E", F("ot"), F("Boiler fault reset failed"));
        }
        vars.actions.resetFault = false;
    }

    // Diag reset action (sendRemoteRequest(10))
    if (vars.actions.resetDiagnostic)
    {
        if (vars.states.diagnostic)
        {
            if (sendRemoteRequest(10))
                SerialPrint("i", F("ot"), F("Boiler diagnostic reset successfully"));
            else
                SerialPrint("E", F("ot"), F("Boiler diagnostic reset failed"));
        }
        vars.actions.resetDiagnostic = false;
    }

    IoTItem::loop();
}

// =======================================================================================================
// Исполнительные команды
// =======================================================================================================
IoTValue OpenThermMonitor::execute(String command, std::vector<IoTValue> &param)
{
    unsigned long request;

    if (command == "lockOutReset")
    {
        vars.actions.resetFault = true;
        vars.actions.resetDiagnostic = true;
        SerialPrint("i", F("ot"), "boiler.lockReset");
        return {};
    }
    else if (command == "ot_imitation")
    {
        SerialPrint("i", F("ot"), "!!! ot->imitation !!! ");
        if (_libOT != nullptr)
            _libOT->imitation(true);
        return {};
    }
    else if (command == "command")
    {
        unsigned int id = 0;
        if (!param[0].isDecimal)
            SerialPrint("E", "ot", "Используйте номер OpenTherm ID ");
        if (param[0].isDecimal)
            id = param[0].valD;

        if (_debug > 0)
        {
            if (param.size() == 2 && param[1].isDecimal)
                SerialPrint("i", F("ot"), "Запрос OTset: " + String(id) + " = " + String(param[1].valD));
            else
                SerialPrint("i", F("ot"), "Запрос OTget: " + String(id));
        }

        if (id > 0)
        {
            if (param.size() == 2 && param[1].isDecimal)
            {
                unsigned int data;
                if (id == 56 || id == 57 || id == 58 || id == 1 || id == 7 || id == 8 || id == 14 || id == 16 || id == 23 || id == 24 || id == 124)
                    data = _libOT->temperatureToData(param[1].valD);
                else
                    data = param[1].valD;

                if (vars.states.emergency && id == 1)
                {
                    data = _libOT->temperatureToData(_offlineCHTemp);
                    SerialPrint("i", F("ot"), "Offline режим. Установлена температура CH: " + String(_offlineCHTemp));
                    checkNew("OTset1", String(_offlineCHTemp));
                }
                request = _libOT->buildRequestID(OpenThermRequestType::WRITE_DATA, id, data);
                addRequestQueue(request);
            }
            else
            {
                request = _libOT->buildRequestID(OpenThermRequestType::READ_DATA, id, 0x0000);
                addRequestQueue(request);
            }
        }
        return {};
    }
    // --------------- Установка режима работы котла ---------------
    else if (command == "SetBoilerStatus")
    {
        if (_useOfflineMode && (!_netActive || !_mqttIsConnect))
        {
            if (_debug > 0)
                SerialPrint("i", F("ot"), "Offline-mode: Режим работы котла из сценария не установлен");
        }
        else
        {
            settings.heating.enable = (bool)param[0].valD;
            settings.heating.recirculation = (bool)1;
            if (param.size() > 2)
            {
                settings.dhw.enable = (bool)param[2].valD;
                settings.heating.cooling = (bool)0;

                if (param[4].isDecimal)
                    settings.opentherm.nativeHeatingControl = (bool)param[4].valD;
                else
                    settings.opentherm.nativeHeatingControl = (bool)0;

                if (param[5].isDecimal)
                    settings.opentherm.heatingCh2Enabled = (bool)param[5].valD;
                else
                    settings.opentherm.heatingCh2Enabled = (bool)0;

                if (param[6].isDecimal)
                    settings.opentherm.summerWinterMode = (bool)param[6].valD;
                else
                    settings.opentherm.summerWinterMode = (bool)0;

                if (param[7].isDecimal)
                    settings.opentherm.dhwBlocking = (bool)param[7].valD;
                else
                    settings.opentherm.dhwBlocking = (bool)0;

                if (param[8].isDecimal)
                    settings.opentherm.immergasFix = (bool)param[8].valD;
                else
                    settings.opentherm.immergasFix = (bool)0;
            }
            else
            {
                settings.dhw.enable = (bool)param[1].valD;
                settings.heating.cooling = (bool)0;
                settings.opentherm.dhwBlocking = (bool)0;
                settings.opentherm.immergasFix = (bool)0;
            }

            if (_debug > 0)
                SerialPrint("i", F("ot"), "Записываем Режим работы котла (SetBoilerStatus) ");

            if (_oldOTinit == true)
            {
                SerialPrint("i", F("ot"), "setBoilerStatus: CH = " + String(bool(settings.heating.enable)) +
                    ", DHW = " + String(bool(settings.opentherm.dhwPresent && settings.dhw.enable)) +
                    ", Cooling = " + String(false) +
                    ", nativeThermostat = " + String(bool(settings.opentherm.nativeHeatingControl)) +
                    ", CH2 = " + String(bool(settings.opentherm.heatingCh2Enabled)));
            }
            else
            {
                SerialPrint("i", F("ot"), "setBoilerStatus: CH = " + String(bool(settings.heating.enable)) +
                    ", DHW = " + String(bool(settings.opentherm.dhwPresent && settings.dhw.enable)) +
                    ", Cooling = " + String(false) +
                    ", nativeThermostat = " + String(bool(settings.opentherm.nativeHeatingControl)) +
                    ", CH2 = " + String(bool(settings.opentherm.heatingCh2Enabled)) +
                    ", summerMode = " + String(bool(settings.opentherm.summerWinterMode)) +
                    ", dhwBlocking = " + String(bool(settings.opentherm.dhwBlocking)) +
                    ", immergasFix = " + String(bool(settings.opentherm.immergasFix)));
            }
        }
        return {};
    }
    // --------------- Slave режим котла ---------------
    else if (command == "setSlave")
    {
        unsigned long req = _libOT->buildRequest(OpenThermRequestType::READ_DATA, OpenThermMessageID::SConfigSMemberIDcode, 0xFFFF);
        _libOT->sendRequest(req);
        SerialPrint("i", F("ot"), "boiler.setSlave ID: 3");
        if (param[0].isDecimal)
        {
            vars.parameters.slaveMemberId = (int)param[0].valD;
        }
        if (vars.parameters.slaveMemberId >= 0)
        {
            req = _libOT->buildRequest(OpenThermRequestType::WRITE_DATA, OpenThermMessageID::MConfigMMemberIDcode, (int)vars.parameters.slaveMemberId);
            _libOT->sendRequest(req);
            SerialPrint("i", F("ot"), "boiler.setSlave ID: 2 = " + String(vars.parameters.slaveMemberId));
        }
        return {};
    }
    else if (command == "oldOTinit")
    {
        _oldOTinit = true;
        return {};
    }
    return {};
}

// =======================================================================================================
// Колбэк ответа от OpenTherm (нестатический, через лямбду)
// =======================================================================================================
void OpenThermMonitor::responseCallback(unsigned long result, OpenThermResponseStatus status)
{
    switch (status)
    {
    case OpenThermResponseStatus::NONE:
        if (_debug > 0)
        {
            SerialPrint("E", "ot", "Error: OpenTherm не инициализирован");
            if (_mqttIsConnect)
                publishData("OTlogs", "Error: OpenTherm не инициализирован");
        }
        break;
    case OpenThermResponseStatus::INVALID:
        if (_debug > 0)
        {
            byte msgType = (result << 1) >> 29;
            if (static_cast<OpenThermMessageType>(msgType) == OpenThermMessageType::UNKNOWN_DATA_ID)
            {
                SerialPrint("E", "ot", "ID:" + String(static_cast<int>(_libOT->getDataID(result))) +
                    "  Error: Команда не поддерживается: " + String(result, HEX));
                if (_mqttIsConnect)
                    publishData("OTlogs", "ID:" + String(static_cast<int>(_libOT->getDataID(result))) +
                        "  Error: Команда не поддерживается: " + String(result, HEX));
            }
            else
            {
                SerialPrint("E", "ot", "ID:" + String(static_cast<int>(_libOT->getDataID(result))) +
                    "  Error: Ошибочный ответ от котла: " + String(result, HEX));
                if (_mqttIsConnect)
                    publishData("OTlogs", "ID:" + String(static_cast<int>(_libOT->getDataID(result))) +
                        "  Error: Ошибочный ответ от котла: " + String(result, HEX));
            }
        }
        break;
    case OpenThermResponseStatus::TIMEOUT:
        _timeoutCount++;
        if (_debug > 0 && _async == true)
        {
            SerialPrint("E", "ot", " ID: " + String(static_cast<int>(_libOT->getDataID(result))) +
                "  / Error: async Таймаут ответа от котла " + String(millis() - _lastSuccessResponse) +
                ". Пропусков: " + String(_timeoutCount));
            if (_mqttIsConnect)
                publishData("OTlogs", " ID: " + String(static_cast<int>(_libOT->getDataID(result))) +
                    " / Error: async Таймаут ответа от котла " + String(millis() - _lastSuccessResponse) +
                    ". Пропусков: " + String(_timeoutCount));
        }
        else if (_debug > 0)
        {
            SerialPrint("E", "ot", " ID: " + String(static_cast<int>(_libOT->getDataID(result))) +
                " / Error: Таймаут ответа от котла " + String(millis() - _lastSuccessResponse) +
                ". Пропусков: " + String(_timeoutCount));
            if (_mqttIsConnect)
                publishData("OTlogs", " ID: " + String(static_cast<int>(_libOT->getDataID(result))) +
                    " / Error: Таймаут ответа от котла " + String(millis() - _lastSuccessResponse) +
                    ". Пропусков: " + String(_timeoutCount));
        }

        if (_timeoutCount > TIMEOUT_TRESHOLD)
        {
            if (_debug > 0 && _async == false)
            {
                SerialPrint("E", "ot", " ID: " + String(static_cast<int>(_libOT->getDataID(result))) +
                    " / OpenTherm: потеря связи с котлом " + String(millis() - _lastSuccessResponse));
            }
            checkNew(_instanceId, "❌");
            checkNew("isHeatingEnabled", "➖");
            checkNew("isDHWenabled", "➖");
            checkNew("isFlameOn", "➖");
            checkNew("OTget25", String(0.00f));
            checkNew("OTget26", String(0.00f));
            checkNew("OTget17", String(0.00f));
            _timeoutCount = TIMEOUT_TRESHOLD;
            sendTelegramm(("OpenTherm: потеря связи с котлом ❌"));
            if (_debug > 0 && _mqttIsConnect)
                publishData("OTlogs", "OpenTherm: потеря связи с котлом ❌");
            if (settings.opentherm.async == 2)
                _async = true;
        }
        break;
    case OpenThermResponseStatus::SUCCESS:
        if (settings.opentherm.async == 2)
            _async = false;

        _timeoutCount = 0;

        // Проверяем восстановление связи (используем _knownValues вместо OpenThemDataGW)
        if (_knownValues[_instanceId] == "❌")
        {
            sendTelegramm("OpenTherm: котёл подключен ✅");
            if (_debug > 0 && _mqttIsConnect)
                publishData("OTlogs", "OpenTherm: котёл подключен ✅");
        }
        checkNew(_instanceId, "✅");
        handleReply(result);
        _lastSuccessResponse = millis();
        break;
    default:
        break;
    }
}

// =======================================================================================================
// sendTelegramm() — отправка Telegram сообщения (исправлено логическое условие)
// =======================================================================================================
void OpenThermMonitor::sendTelegramm(String msg)
{
    if (_telegramInfo == 1)
    {
        for (std::list<IoTItem *>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it)
        {
            String st = String((*it)->getSubtype().c_str());
            if (st == "TelegramLT" || st == "Telegram" || st == "Telegram_v2")
            {
                (*it)->sendTelegramMsg(false, msg);
            }
        }
    }
}

// =======================================================================================================
// handleReply() — обработка ответов котла
// =======================================================================================================
void OpenThermMonitor::handleReply(unsigned long response)
{
    byte id = static_cast<int>(_libOT->getDataID(response));
    uint8_t flags;
    static uint8_t failMoreOne = 0;
    static uint8_t failMoreOne_ = 0;

    switch (id)
    {
    case 0:
        // Статус котла
        if (_debug > 2)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) +
                " Fault:" + String(bool((response & 0x01)) ? "❗️🆘❗️" : "➖") +
                ", CH:" + String(bool((response & 0x02)) ? "✅" : "➖") +
                ", DHW:" + String(bool((response & 0x04)) ? "✅" : "➖") +
                ", Flame:" + String(bool((response & 0x08)) ? "🔥" : "➖") +
                ", Cooling:" + String(bool((response & 0x10)) ? "✅" : "➖") +
                ", CH2:" + String(bool((response & 0x20)) ? "✅" : "➖") +
                ", Diagnostic:" + String(bool((response & 0x40)) ? "✅" : "➖"));
        }
        checkNew("isHeatingEnabled", String(bool(_libOT->isCentralHeatingActive(response)) ? "✅" : "➖"));
        checkNew("isDHWenabled", String(bool(response & 0x04) ? "✅" : "➖"));
        checkNew("isFlameOn", String(bool(response & 0x08) ? "🔥работает" : "➖"));
        checkNew("isFault", String(bool(response & 0x01) ? "❗️🆘❗️" : "нет"));
        checkNew("isDiagnostic", String(bool(response & 0x40) ? "❗️🆘❗️" : "➖"));
        checkNew("CH_mode", String(bool(response & 0x02) ? "✅" : "➖"));
        checkNew("CH2_mode", String(bool(response & 0x20) ? "✅" : "➖"));

        if ((vars.states.fault != _libOT->isFault(response)) || (vars.states.diagnostic != _libOT->isDiagnostic(response)))
        {
            _requestQueue.resize(1);
            unsigned long request = _libOT->buildRequest(OpenThermRequestType::READ_DATA, OpenThermMessageID::ASFflags, 0x0000);
            addRequestQueue(request);
        }

        vars.states.heating = _libOT->isCentralHeatingActive(response);
        vars.states.dhw = settings.opentherm.dhwPresent ? _libOT->isHotWaterActive(response) : false;
        vars.states.flame = _libOT->isFlameOn(response);
        vars.states.fault = _libOT->isFault(response);
        vars.states.diagnostic = _libOT->isDiagnostic(response);
        if (!vars.states.flame)
            checkNew("OTget17", String(0.00f));
        if (!vars.states.fault && !vars.states.diagnostic)
        {
            checkNew("isFault", "нет");
            checkNew("isDiagnostic", "➖");
            checkNew("LastFaultType", "нет");
        }
        break;

    case 5:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " ASFflags (Получены флаги ошибок)");
            SerialPrint("i", F("ot"), " - fault_code HEX " + String(response & 0xFF, HEX));
            SerialPrint("i", F("ot"), " - fault_code " + String(response & 0xFF));
            SerialPrint("i", F("ot"), "Application-specific fault flags:");
            flags = response >> 8;
            SerialPrint("i", F("ot"), " - service_required " + String(bool((flags & 0x01)) ? "❗️🆘❗️" : "➖"));
            SerialPrint("i", F("ot"), " - lockout_reset " + String(bool((flags & 0x02)) ? "❗️🆘❗️" : "➖"));
            SerialPrint("i", F("ot"), " - low_water_pressure " + String(bool((flags & 0x04)) ? "❗️🆘❗️" : "➖"));
            SerialPrint("i", F("ot"), " - gas_fault " + String(bool((flags & 0x08)) ? "❗️🆘❗️" : "➖"));
            SerialPrint("i", F("ot"), " - air_fault " + String(bool((flags & 0x10)) ? "❗️🆘❗️" : "➖"));
            SerialPrint("i", F("ot"), " - water_overtemp " + String(bool((flags & 0x20)) ? "❗️🆘❗️" : "➖"));
        }
        flags = (response & 0xFFFF) >> 8;
        checkNew("service_required", String(bool(flags & 0x01)));
        checkNew("lockout_reset", String(bool(flags & 0x02)));
        checkNew("low_water_pressure", String(bool(flags & 0x04)));
        checkNew("gas_fault", String(bool(flags & 0x08)));
        checkNew("air_fault", String(bool(flags & 0x10)));
        checkNew("water_overtemp", String(bool(flags & 0x20)));
        checkNew("fault_code", String((int)response & 0xFF));

        if (bool(flags & 0x01)) { sendTelegramm("OpenTherm: запрос обслуживания!"); checkNew("LastFaultType", "Требуется обслуживание"); }
        if (bool(flags & 0x02)) { sendTelegramm("OpenTherm: сброс блокировки!"); checkNew("LastFaultType", "Сброс блокировки!"); }
        if (bool(flags & 0x04)) { sendTelegramm("OpenTherm: низкое давление воды!"); checkNew("LastFaultType", "Давление воды!"); }
        if (bool(flags & 0x08)) { sendTelegramm("OpenTherm: ошибка газа/пламени!"); checkNew("LastFaultType", "Ошибка газа/пламени!"); }
        if (bool(flags & 0x10)) { sendTelegramm("OpenTherm: ошибка давления воздуха!"); checkNew("LastFaultType", "Ошибка давления воздуха!"); }
        if (bool(flags & 0x20)) { sendTelegramm("OpenTherm: перегрев!"); checkNew("LastFaultType", "Перегрев котла!"); }
        if ((int)response & 0xFF)
            sendTelegramm("OpenTherm: код ошибки: " + String((int)response & 0xFF));
        break;

    case 3:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " SConfigSMemberIDcode (Получены установки котла)");
            SerialPrint("i", F("ot"), " - SlaveMemberIDcode HEX " + String(response >> 0 & 0xFF, HEX));
            SerialPrint("i", F("ot"), " - SlaveMemberIDcode " + String(response >> 0 & 0xFF));
            flags = response >> 8;
            SerialPrint("i", F("ot"), " - dhw:" + String(bool((flags & 0x01)) ? "✅" : "➖") +
                ", control_type: " + String(bool((flags & 0x02)) ? String("On/Off") : String("Модуляция")) +
                ", cooling:" + String(bool((flags & 0x04)) ? "✅" : "➖") +
                ", dhw: " + String(bool((flags & 0x08)) ? String("бак") : String("проточная")) +
                ", pump_control:" + String(bool((flags & 0x10)) ? "✅" : "➖") +
                ", ch2:" + String(bool((flags & 0x20)) ? "✅" : "➖"));
        }
        checkNew("SlaveMemberIDcode", String(response >> 0 & 0xFF));
        flags = (response & 0xFFFF) >> 8 & 0xFF;
        checkNew("dhw_present", String(bool(flags & 0x01)));
        checkNew("control_type", String(bool((flags & 0x02)) ? String("On/Off") : String("Модуляция")));
        checkNew("cooling_present", String(bool(flags & 0x04)));
        checkNew("dhw_tank_present", String(bool((flags & 0x08)) ? String("бак") : String("проточная")));
        checkNew("pump_control_present", String(bool(flags & 0x10)));
        checkNew("ch2_present", String(bool(flags & 0x20)));
        break;

    case 127:
        if (_debug > 2)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " SlaveVersion ");
            SerialPrint("i", F("ot"), " - SlaveType " + String(uint8_t((response >> 8) & 0xFF)));
            SerialPrint("i", F("ot"), " - SlaveVersion " + String(response & 0xFF));
        }
        checkNew("SlaveType", String(uint8_t((response & 0xFFFF) >> 8)));
        checkNew("SlaveVersion", String(uint8_t(response & 0xFF)));
        checkNew("OTget" + String(id), String(uint8_t((response & 0xFFFF) >> 8)) + " " + String(uint8_t(response & 0xFF)));
        break;

    case 126:
        if (_debug > 2)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + "  Slave product version Master product version ");
            SerialPrint("i", F("ot"), " - MasterType " + String((response & 0xFFFF) >> 8));
            SerialPrint("i", F("ot"), " - MasterVersion " + String(response & 0xFF));
        }
        checkNew("MasterType", String((response & 0xFFFF) >> 8));
        checkNew("MasterVersion", String(response & 0xFF));
        checkNew("OTget" + String(id), String((response & 0xFFFF) >> 8) + " " + String(response & 0xFF));
        break;

    // Float-типы (f8.8): сгруппированы
    case 17: case 25: case 26: case 27: case 28: case 29:
    case 31: case 32: case 56: case 57: case 58: case 125:
    case 18: case 19:
    {
        String name = getFloatIDName(id);
        float val = _libOT->getFloat(response);
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " " + name + ": " + String(val));
        }
        checkNew(name, String(val));
        break;
    }

    // u16-типы
    case 115: case 116: case 117: case 118: case 119:
    case 120: case 121: case 122: case 123:
    {
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " = " + String(response & 0xFFFF));
        }
        checkNew("OTget" + String(id), String(response & 0xFFFF));
        break;
    }

    // Специальные форматы
    case 48:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " TdhwSetUBTdhwSetLB (Диапазон изменения °C ГВС )");
            SerialPrint("i", F("ot"), " - DHWsetpLow " + String(response & 0xFF));
            SerialPrint("i", F("ot"), " - DHWsetpUpp " + String((response >> 8) & 0xFF));
        }
        checkNew("DHWsetpUpp", String((int)(response & 0xFFFF) >> 8));
        checkNew("DHWsetpLow", String((int)response & 0xFF));
        checkNew("OTget" + String(id), String((int)(response & 0xFFFF) >> 8) + " " + String((int)response & 0xFF));
        break;

    case 49:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " MaxTSetUBMaxTSetLB (Диапазон изменения °C отопления )");
            SerialPrint("i", F("ot"), " - MaxCHsetpLow " + String(response & 0xFF));
            SerialPrint("i", F("ot"), " - MaxCHsetpUpp " + String((response >> 8) & 0xFF));
        }
        checkNew("MaxCHsetpUpp", String((int)(response & 0xFFFF) >> 8));
        checkNew("MaxCHsetpLow", String((int)response & 0xFF));
        checkNew("OTget" + String(id), String((int)(response & 0xFFFF) >> 8) + " " + String((int)response & 0xFF));
        break;

    case 20:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " Day of Week & Time of Day");
            SerialPrint("i", F("ot"), " - Day_of_week " + String((response >> 8) & 0xE0));
            SerialPrint("i", F("ot"), " - Hours " + String((response >> 8) & 0x1F));
            SerialPrint("i", F("ot"), " - Minutes " + String((response) & 0xFF));
        }
        checkNew("Day_of_week", String((response >> 8) & 0xE0));
        checkNew("Hours", String((response >> 8) & 0x1F));
        checkNew("Minutes", String((response) & 0xFF));
        checkNew("OTget" + String(id), String((response >> 8) & 0xE0) + " " + String((response >> 8) & 0x1F) + ":" + String((response) & 0xFF));
        break;

    case 21:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " Date");
            SerialPrint("i", F("ot"), " - Month " + String((response >> 8) & 0xFF));
            SerialPrint("i", F("ot"), " - Day " + String((response) & 0xFF));
        }
        checkNew("Month", String((response >> 8) & 0xFF));
        checkNew("Day", String((response) & 0xFF));
        checkNew("OTget" + String(id), String((response >> 8) & 0xFF) + " " + String((response) & 0xFF));
        break;

    case 22:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " Year");
            SerialPrint("i", F("ot"), " - Year " + String(response & 0xFFFF));
        }
        checkNew("OTget" + String(id), String(response & 0xFFFF));
        break;

    case 33:
    {
        // s16 — signed 16-bit
        int16_t exhaustVal = (int16_t)(response & 0xFFFF);
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " Boiler exhaust temperature (°C)");
            SerialPrint("i", F("ot"), " - OTget33: " + String(exhaustVal));
        }
        checkNew("OTget" + String(id), String(exhaustVal));
        break;
    }

    case 34:
    {
        float val34 = _libOT->getFloat(response & 0xFFFF);
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " Boiler heat exchanger temperature (°C)");
            SerialPrint("i", F("ot"), " - OTget34: " + String(val34));
        }
        checkNew("OTget" + String(id), String(val34));
        break;
    }

    case 35:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " Boiler fan speed Setpoint and actual value");
            SerialPrint("i", F("ot"), " - OTget35: " + String((response >> 8) & 0xFF));
            SerialPrint("i", F("ot"), " - Fan Speed Setpoint " + String(response & 0xFF));
            SerialPrint("i", F("ot"), " - Fan Speed " + String((response >> 8) & 0xFF));
        }
        checkNew("FanSpeedSetpoint", String((int)(response & 0xFFFF) >> 8));
        checkNew("FanSpeed", String((int)response & 0xFF));
        checkNew("OTget" + String(id), String((int)response & 0xFF));
        break;

    case 36:
    {
        float val36 = _libOT->getFloat(response);
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " Electric current through burner flame (μA)");
            SerialPrint("i", F("ot"), " - OTget36: " + String(val36));
        }
        checkNew("OTget" + String(id), String(val36));
        break;
    }

    case 6:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " Remote boiler parameter transfer-enable & read/write flags");
            SerialPrint("i", F("ot"), " - DHW_setpoint_write " + String(bool((response & 0x01)) ? "✅" : "❌"));
            SerialPrint("i", F("ot"), " - max_CHsetpoint_write: " + String(bool((response & 0x02)) ? "✅" : "❌"));
            flags = response >> 8;
            SerialPrint("i", F("ot"), " - DHW_setpoint_transfer_enable: " + String(bool((flags & 0x01)) ? "✅" : "❌"));
            SerialPrint("i", F("ot"), " - max_CHsetpoint_transfer_enable: " + String(bool((flags & 0x02)) ? "✅" : "❌"));
        }
        checkNew("DHW_setpoint_write", String(bool(response & 0x01)));
        checkNew("max_CHsetpoint_write", String(bool(response & 0x02)));
        checkNew("DHW_setpoint_transfer_enable", String(bool(flags & 0x01)));
        checkNew("max_CHsetpoint_transfer_enable", String(bool(flags & 0x02)));
        break;

    case 10:
    case 12:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " = " + String((response >> 8) & 0xFF));
        }
        checkNew("OTget" + String(id), String((response >> 8) & 0xFF));
        break;

    case 15:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " Maximum boiler capacity (kW) / Minimum boiler modulation level(%)");
            SerialPrint("i", F("ot"), " - max_boiler_capacity: " + String((response >> 8) & 0xFF));
            SerialPrint("i", F("ot"), " - min_modulation_level: " + String((response) & 0xFF));
        }
        checkNew("max_boiler_capacity", String((response >> 8) & 0xFF));
        checkNew("min_modulation_level", String((response) & 0xFF));
        checkNew("OTget" + String(id), String((response >> 8) & 0xFF) + " " + String((response) & 0xFF));
        break;

    case 100:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " Remote override function");
            SerialPrint("i", F("ot"), " - Manual_change_priority: " + String(bool((response & 0x01)) ? "✅" : "❌"));
            SerialPrint("i", F("ot"), " - Program_change_priority: " + String(bool((response & 0x02)) ? "✅" : "❌"));
        }
        checkNew("Manual_change_priority", String(bool(response & 0x01)));
        checkNew("Program_change_priority", String(bool(response & 0x02)));
        checkNew("OTget" + String(id), String(bool(response & 0x01)) + " " + String(bool(response & 0x02)));
        break;

    case 1:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " CH temperature setpoint (°C) TSet");
            SerialPrint("i", F("ot"), " - OTget1: " + String(_libOT->getFloat(response)));
        }
        checkNew("OTget" + String(id), String(_libOT->getFloat(response)));
        break;

    case 30:
    {
        // Solar collector temperature — float
        float val30 = _libOT->getFloat(response);
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id) + " Solar collector temperature (°C)");
            SerialPrint("i", F("ot"), " - OTget30: " + String(val30));
        }
        checkNew("OTget" + String(id), String(val30));
        break;
    }

    default:
        if (_debug > 1)
        {
            SerialPrint("i", F("ot"), " 🔙  ID: " + String(id));
        }
        checkNew("OTget" + String(id), String(response));
        break;
    }
}

// =======================================================================================================
// checkNew() — проверка изменения значения, уведомление подписчиков
// =======================================================================================================
void OpenThermMonitor::checkNew(String openthermID, String value)
{
    String oldVal = _knownValues[openthermID];

    // Округление для OTget25, OTget26, OTget17
    if (openthermID == "OTget25" || openthermID == "OTget26" || openthermID == "OTget17")
    {
        if (value.indexOf('.') != -1)
        {
            float valF = value.toFloat();
            int valI = round(valF);
            value = String(valI);
        }
        if (oldVal.indexOf('.') != -1)
        {
            float valF = oldVal.toFloat();
            int valI = round(valF);
            oldVal = String(valI);
        }
    }

    if (oldVal != value)
    {
        IoTItem *item = findIoTItem(openthermID);
        if (item)
        {
            item->setValue(value.c_str());
        }
        _knownValues[openthermID] = value;
    }
}

// =======================================================================================================
// addRequestQueue() — постановка запроса в очередь
// =======================================================================================================
void OpenThermMonitor::addRequestQueue(unsigned long request)
{
    if (_debug > 3)
    {
        SerialPrint("i", F("ot"), "addRequestQueue ID:" + String(static_cast<int>(_libOT->getDataID(request))));
    }
#ifndef ESP32_FREERTOS
    if (_requestQueue.size() >= 10)
    {
        _requestQueue.resize(9);
    }
    if (_requestQueue.size() >= 45)
    {
        _requestQueue.resize(44);
    }
    _requestQueue.push_back(request);
#else
    addQueueRequest(request, (TickType_t)0);
#endif
}

// =======================================================================================================
// sendRequest2Channel() — отправка асинхронных запросов
// =======================================================================================================
bool OpenThermMonitor::sendRequest2Channel()
{
    bool result = false;
    if (!_requestQueue.empty())
    {
        int msg_id = static_cast<int>(_libOT->getDataID(_requestQueue.front()));
        if (_debug > 2 && msg_id == 0)
        {
            String asyncStr = _async ? "async" : "";
            SerialPrint("i", F("ot"), " ➡️ " + asyncStr + " ID: " + String(msg_id) + " / request " + String(_requestQueue.front(), HEX));
        }
        if (_debug > 1 && msg_id != 0)
        {
            String asyncStr = _async ? "async" : "";
            SerialPrint("i", F("ot"), " ➡️ " + asyncStr + " ID: " + String(msg_id) + " / request " + String(_requestQueue.front(), HEX));
        }

        if (_async)
            result = _libOT->sendRequestAsync(_requestQueue.front());
        else
            result = _libOT->sendRequest(_requestQueue.front());

        if (result)
            _requestQueue.pop_front();
    }
    return result;
}

// =======================================================================================================
// onModuleOrder() — обработка заказов из конфигурации
// =======================================================================================================
void OpenThermMonitor::onModuleOrder(String &key, String &value)
{
    // Зарезервировано для будущих кнопок
}

// =======================================================================================================
// Фабричная функция для создания экземпляра
// =======================================================================================================
void *getAPI_OpenThermMonitor(String subtype, String param)
{
    if (subtype == F("OpenThermMonitor"))
    {
        return new OpenThermMonitor(param);
    }
    else
    {
        return nullptr;
    }
}