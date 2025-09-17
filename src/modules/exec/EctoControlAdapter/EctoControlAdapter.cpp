#include "Global.h"
#include "classes/IoTItem.h"
#include <map>
#include <HardwareSerial.h>
#include "ModbusEC.h"
#include <vector>
#include <functional>

// Константы
constexpr uint8_t DEFAULT_DIR_PIN = 0;
constexpr uint8_t DEFAULT_MODBUS_ADDR = 0xF0;
constexpr uint8_t DEFAULT_DEVICE_TYPE = 0x14;
constexpr uint32_t DEFAULT_SERIAL_TIMEOUT = 500;
constexpr uint16_t INVALID_MODBUS_VALUE = 0x7FFF;
constexpr uint8_t MAX_RETRY_ATTEMPTS = 3;
constexpr uint16_t RETRY_DELAY_MS = 100;
constexpr uint16_t MAX_REGISTERS_PER_READ = 16;

// Команды адаптера
constexpr uint16_t COMMAND_REBOOT = 2;
constexpr uint16_t COMMAND_RESET_ERRORS = 3;

// Битовая маска для регистра статуса котла
struct BoilerStatusBits {
    uint8_t heatingEnabled : 1;
    uint8_t dhwEnabled : 1;
    uint8_t secondaryCircuit : 1;
    uint8_t reserved : 5;
    
    uint16_t toUint16() const {
        return (heatingEnabled ? 0x0001 : 0) | 
               (dhwEnabled ? 0x0002 : 0) | 
               (secondaryCircuit ? 0x0004 : 0);
    }
};

// Оптимизированные структуры данных с упакованными полями
struct __attribute__((packed)) BoilerInfo {
    uint8_t adapterType : 3;
    bool boilerConnected : 1;
    uint8_t rebootCode;
    uint8_t hardwareVersion;
    uint8_t softwareVersion;
    uint32_t uptime;
    uint16_t memberCode;
    uint16_t modelCode;
};

struct __attribute__((packed)) BoilerStatus {
    bool burnerActive : 1;
    bool heatingActive : 1;
    bool dhwActive : 1;
    uint8_t errorFlags;
    BoilerStatusBits writtenStatus;
};

// Регистры Modbus
enum ReadRegisters {
    ecR_AdapterInfo = 0x0010,
    ecR_AdapterVersion = 0x0011,
    ecR_Uptime = 0x0012,
    ecR_MinSetCH = 0x0014,
    ecR_MaxSetCH = 0x0015,
    ecR_MinSetDHW = 0x0016,
    ecR_MaxSetDHW = 0x0017,
    ecR_TempCH = 0x0018,
    ecR_TempDHW = 0x0019,
    ecR_Pressure = 0x001A,
    ecR_FlowRate = 0x001B,
    ecR_ModLevel = 0x001C,
    ecR_BoilerStatus = 0x001D,
    ecR_CodeError = 0x001E,
    ecR_CodeErrorExt = 0x001F,
    ecR_TempOutside = 0x0020,
    ecR_MemberCode = 0x0021,
    ecR_ModelCode = 0x0022,
    ecR_FlagErrorOT = 0x0023,
    ecR_ReadSetStatusBoiler = 0x003F,
    // Регистры для чтения записанных значений (+6 от регистров записи)
    ecR_ReadSetTypeConnect = 0x0036,
    ecR_ReadTSetCH = 0x0037,
    ecR_ReadTSetCHFaultConn = 0x0038,
    ecR_ReadTSetMinCH = 0x0039,
    ecR_ReadTSetMaxCH = 0x003A,
    ecR_ReadTSetMinDHW = 0x003B,
    ecR_ReadTSetMaxDHW = 0x003C,
    ecR_ReadTSetDHW = 0x003D,
    ecR_ReadSetMaxModLevel = 0x003E
};

enum WriteRegisters {
    ecW_SetTypeConnect = 0x0030,
    ecW_TSetCH = 0x0031,
    ecW_TSetCHFaultConn = 0x0032,
    ecW_TSetMinCH = 0x0033,
    ecW_TSetMaxCH = 0x0034,
    ecW_TSetMinDHW = 0x0035,
    ecW_TSetMaxDHW = 0x0036,
    ecW_TSetDHW = 0x0037,
    ecW_SetMaxModLevel = 0x0038,
    ecW_SetStatusBoiler = 0x0039,
    ecW_Command = 0x0080
};

Stream* _modbusUART = nullptr;
uint8_t _DIR_PIN = DEFAULT_DIR_PIN;

// Оптимизированные функции управления направлением передачи
inline void modbusPreTransmission() {
    if (_DIR_PIN) digitalWrite(_DIR_PIN, HIGH);
}

inline void modbusPostTransmission() {
    if (_DIR_PIN) digitalWrite(_DIR_PIN, LOW);
}

class EctoControlAdapter : public IoTItem {
private:
    int _rx, _tx, _baud, _uartLine;
    String _prot;
    uint8_t _addr;
    uint8_t _debugLevel;
    ModbusMaster node;
    BoilerInfo info = {};
    BoilerStatus status = {};
    float tCH = 0, tDHW = 0, tOut = 0;
    float press = 0, flow = 0;
    uint8_t modLevel = 0;
    uint16_t codeError = 0, codeErrorExt = 0;
    uint8_t errorFlags = 0;

    // Кэш для часто запрашиваемых значений
    uint32_t _lastUpdate = 0;
    static constexpr uint32_t UPDATE_INTERVAL = 10000; // 10 секунд

    // Оптимизированное чтение группы регистров
    bool readRegisterBlock(uint16_t startReg, uint16_t* data, uint8_t count) {
        if (count == 0 || count > MAX_REGISTERS_PER_READ) return false;
        
        uint8_t result = node.readHoldingRegisters(startReg, count);
        if (result != node.ku8MBSuccess) {
            if (_debugLevel > 0) {
                SerialPrint("E", "Modbus", "Read block 0x" + String(startReg, HEX) + 
                          "-0x" + String(startReg + count - 1, HEX) + 
                          " failed, code: " + String(result, HEX));
            }
            return false;
        }
        
        for (uint8_t i = 0; i < count; i++) {
            data[i] = node.getResponseBuffer(i);
            if (_debugLevel > 2) {
                SerialPrint("I", "Modbus", "Read 0x" + String(startReg + i, HEX) + 
                          " = " + String(data[i]));
            }
        }
        
        node.clearResponseBuffer();
        return true;
    }

    // Оптимизированное чтение с повторными попытками
    bool readWithRetry(uint16_t reg, uint16_t &reading) {
        uint8_t attempts = 0;
        while (attempts < MAX_RETRY_ATTEMPTS) {
            if (readRegisterBlock(reg, &reading, 1)) {
                if (reading != INVALID_MODBUS_VALUE) {
                    return true;
                }
            }
            attempts++;
            delay(RETRY_DELAY_MS);
        }
        
        if (_debugLevel > 0) {
            SerialPrint("E", "Modbus", "Read reg 0x" + String(reg, HEX) + 
                      " failed after " + String(MAX_RETRY_ATTEMPTS) + " attempts");
        }
        return false;
    }

    // Оптимизированная запись с повторными попытками
    bool writeWithRetry(uint16_t reg, uint16_t data) {
        uint8_t attempts = 0;
        while (attempts < MAX_RETRY_ATTEMPTS) {
            node.setTransmitBuffer(0, data);
            uint8_t result = node.writeMultipleRegisters(reg, 1);
            node.clearTransmitBuffer();
            
            if (result == node.ku8MBSuccess) {
                if (_debugLevel > 2) {
                    SerialPrint("I", "Modbus", "Write 0x" + String(reg, HEX) + 
                              " = " + String(data));
                }
                return true;
            }
            attempts++;
            delay(RETRY_DELAY_MS);
        }
        
        if (_debugLevel > 0) {
            SerialPrint("E", "Modbus", "Write reg 0x" + String(reg, HEX) + 
                      " failed after " + String(MAX_RETRY_ATTEMPTS) + " attempts");
        }
        return false;
    }

    // Оптимизированная публикация данных
    void publishData(const __FlashStringHelper* name, const String& value) {
        IoTItem* item = findIoTItem(name);
        if (item) {
            item->setValue(value, true);
        } else if (_debugLevel > 0) {
            SerialPrint("I", "EctoControl", String(name) + " = " + value);
        }
    }

    // Чтение и обработка ошибок котла
    bool readBoilerErrors() {
        uint16_t registers[3];
        if (!readRegisterBlock(ecR_CodeError, registers, 3)) {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to read boiler errors");
            return false;
        }

        codeError = registers[0];
        codeErrorExt = registers[1];
        errorFlags = registers[2] & 0xFF;

        publishData(F("codeError"), String(codeError));
        publishData(F("codeErrorExt"), String(codeErrorExt));
        publishData(F("errorFlags"), String(errorFlags));

        if (_debugLevel > 1) {
            SerialPrint("I", "EctoControl", String("Boiler errors: ") + 
                      "Main=" + String(codeError, HEX) + 
                      ", Ext=" + String(codeErrorExt, HEX) + 
                      ", Flags=" + String(errorFlags, HEX));
        }

        return true;
    }

    // Групповое чтение основных данных
    bool readCommonData() {
        uint16_t registers[9]; // Увеличено до 9 регистров
        if (!readRegisterBlock(ecR_TempCH, registers, 9)) {
            return false;
        }

        // Обработка температуры отопления
        int16_t tempCH = registers[0];
        if (tempCH != INVALID_MODBUS_VALUE) {
            tCH = tempCH / 10.0f;
            publishData(F("tempCH"), String(tCH));
        }

        // Обработка температуры ГВС
        uint16_t tempDHW = registers[1];
        if (tempDHW != INVALID_MODBUS_VALUE) {
            tDHW = tempDHW / 10.0f;
            publishData(F("tempDHW"), String(tDHW));
        }

        // Обработка давления
        uint16_t pressure = registers[2];
        if (pressure != INVALID_MODBUS_VALUE) {
            press = (pressure & 0xFF) / 10.0f;
            publishData(F("pressure"), String(press));
        }

        // Обработка расхода ГВС
        uint16_t flowRate = registers[3];
        if (flowRate != INVALID_MODBUS_VALUE) {
            flow = (flowRate & 0xFF) / 10.0f;
            publishData(F("flowRate"), String(flow));
        }

        // Обработка уровня модуляции
        uint16_t modulation = registers[4];
        if (modulation != INVALID_MODBUS_VALUE) {
            modLevel = modulation & 0xFF;
            publishData(F("modLevel"), String(modLevel));
        }

        // Обработка статуса котла
        uint16_t statusReg = registers[5];
        status.burnerActive = statusReg & 0x01;         // Бит 0: горелка
        status.heatingActive = (statusReg >> 1) & 0x01; // Бит 1: отопление
        status.dhwActive = (statusReg >> 2) & 0x01;     // Бит 2: ГВС
        
        publishData(F("burner"), String(status.burnerActive));
        publishData(F("heating"), String(status.heatingActive));
        publishData(F("dhw"), String(status.dhwActive));

        // Обработка кодов ошибок
        codeError = registers[6];
        codeErrorExt = registers[7];

        // Обработка температуры наружного воздуха
        int8_t tempOut = (int8_t)(registers[8] & 0xFF);
        if (tempOut != 0x7F) {
            tOut = tempOut;
            publishData(F("tempOut"), String(tOut));
        }

        // Чтение ошибок котла
        readBoilerErrors();

        return true;
    }

    // Групповое чтение настроек
    bool readSettings() {
        uint16_t registers[6];
        if (!readRegisterBlock(ecR_ReadSetTypeConnect, registers, 6)) {
            return false;
        }

        // Обработка записанных значений
        publishData(F("setTypeConnect"), String(registers[0]));
        publishData(F("tSetCH"), String(registers[1] / 10.0f));
        publishData(F("tSetCHFaultConn"), String(registers[2] / 10.0f));
        publishData(F("tSetMinCH"), String(registers[3] / 10.0f));
        publishData(F("tSetMaxCH"), String(registers[4] / 10.0f));
        publishData(F("tSetMinDHW"), String(registers[5]));

        // Чтение оставшихся настроек
        uint16_t settings[3];
        if (readRegisterBlock(ecR_ReadTSetMaxDHW, settings, 3)) {
            publishData(F("tSetMaxDHW"), String(settings[0]));
            publishData(F("tSetDHW"), String(settings[1]));
            publishData(F("setMaxModLevel"), String(settings[2]));
        }

        return true;
    }

public:
    EctoControlAdapter(String parameters) : IoTItem(parameters) {
        _addr = jsonReadInt(parameters, "addr", DEFAULT_MODBUS_ADDR);
        _rx = jsonReadInt(parameters, "RX", 16);
        _tx = jsonReadInt(parameters, "TX", 17);
        _DIR_PIN = jsonReadInt(parameters, "DIR_PIN", DEFAULT_DIR_PIN);
        _baud = jsonReadInt(parameters, "baud", 19200);
        _prot = jsonReadStr(parameters, "protocol", "SERIAL_8N1");
        _debugLevel = jsonReadInt(parameters, "debug", 0);
        _uartLine = jsonReadInt(parameters, "UARTLine", 1);  // Новый параметр выбора порта

        // Инициализация UART
        if (!_modbusUART) {
            _modbusUART = new HardwareSerial(_uartLine);  // Используем выбранный порт
            if (_modbusUART) {
                ((HardwareSerial*)_modbusUART)->begin(_baud, SERIAL_8N1, _rx, _tx);
                ((HardwareSerial*)_modbusUART)->setTimeout(DEFAULT_SERIAL_TIMEOUT);
            } else {
                SerialPrint("E", "EctoControl", "Failed to initialize UART");
            }
        }
        
        // Инициализация Modbus
        node.begin(_addr, _modbusUART);
        node.preTransmission(modbusPreTransmission);
        node.postTransmission(modbusPostTransmission);

        if (_DIR_PIN) {
            pinMode(_DIR_PIN, OUTPUT);
            digitalWrite(_DIR_PIN, LOW);
        }

        if (_addr >= 0) {
            initializeDevice();
        }
    }

    // Оптимизированная инициализация устройства
    void initializeDevice() {
        if (_addr > 0) {
            uint16_t type = 0;
            if (readWithRetry(0x0003, type)) {
                type = type >> 8;
                if (type != 0x14 && type != 0x15 && type != 0x16) {
                    SerialPrint("E", "EctoControl", "Unsupported device type: " + String(type, HEX));
                }
            } else {
                SerialPrint("E", "EctoControl", "Failed to read device type");
            }
        }
        else if (_addr == 0) {
            // если адреса нет, то шлем широковещательный запрос адреса
            uint8_t addr = node.readAddresEctoControl();
            SerialPrint("I", "EctoControlAdapter", "readAddresEctoControl, addr: " + String(addr) + " - Enter to configuration");
        }
    }

    // Получение информации о котле
    bool getBoilerInfo() {
        uint16_t data[3];
        if (!readRegisterBlock(ecR_AdapterInfo, data, 3)) {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to read adapter info");
            return false;
        }
        
        info.adapterType = (data[0] >> 8) & 0x07;
        info.boilerConnected = (data[0] >> 11) & 0x01;
        info.rebootCode = data[0] & 0xFF;
        info.hardwareVersion = data[1] >> 8;
        info.softwareVersion = data[1] & 0xFF;
        info.uptime = (uint32_t)data[2] << 16 | data[3];
        
        uint16_t tempMemberCode;
        if (!readWithRetry(ecR_MemberCode, tempMemberCode)) {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to read member code");
        } else {
            info.memberCode = tempMemberCode;
        }
        
        uint16_t tempModelCode;
        if (!readWithRetry(ecR_ModelCode, tempModelCode)) {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to read model code");
        } else {
            info.modelCode = tempModelCode;
        }
        
        publishData(F("adapterType"), String(info.adapterType));
        publishData(F("boilerConnected"), String(info.boilerConnected));
        return true;
    }

    // Получение записанного статуса котла
    bool getWrittenBoilerStatus() {
        uint16_t regValue;
        if (!readWithRetry(ecR_ReadSetStatusBoiler, regValue)) {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to read written boiler status");
            return false;
        }

        status.writtenStatus.heatingEnabled = (regValue & 0x0001) != 0;
        status.writtenStatus.dhwEnabled = (regValue & 0x0002) != 0;
        status.writtenStatus.secondaryCircuit = (regValue & 0x0004) != 0;

        publishData(F("writtenHeatingEnabled"), String(status.writtenStatus.heatingEnabled));
        publishData(F("writtenDHWEnabled"), String(status.writtenStatus.dhwEnabled));
        publishData(F("writtenSecondaryCircuit"), String(status.writtenStatus.secondaryCircuit));

        if (_debugLevel > 1) {
            SerialPrint("I", "EctoControl", String("Status 0x003F: ") + 
                      "Heating=" + status.writtenStatus.heatingEnabled + 
                      ", DHW=" + status.writtenStatus.dhwEnabled + 
                      ", Secondary=" + status.writtenStatus.secondaryCircuit);
        }

        return true;
    }

    // Установка статуса отопления
    bool setHeating(bool enable) {
        uint16_t currentStatus;
        if (!readWithRetry(ecR_ReadSetStatusBoiler, currentStatus)) {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to read boiler status");
            return false;
        }

        uint16_t newStatus = currentStatus & 0xFFFE;
        if (enable) {
            newStatus |= 0x0001;
        }

        bool success = writeWithRetry(ecW_SetStatusBoiler, newStatus);
        if (success) {
            getWrittenBoilerStatus();
        } else {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to set heating status");
        }
        return success;
    }

    // Установка статуса ГВС
    bool setDHW(bool enable) {
        uint16_t currentStatus;
        if (!readWithRetry(ecR_ReadSetStatusBoiler, currentStatus)) {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to read boiler status");
            return false;
        }

        uint16_t newStatus = currentStatus & 0xFFFD;
        if (enable) {
            newStatus |= 0x0002;
        }

        bool success = writeWithRetry(ecW_SetStatusBoiler, newStatus);
        if (success) {
            getWrittenBoilerStatus();
        } else {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to set DHW status");
        }
        return success;
    }

    // Установка статуса вторичного контура
    bool setSecondaryCircuit(bool enable) {
        uint16_t currentStatus;
        if (!readWithRetry(ecR_ReadSetStatusBoiler, currentStatus)) {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to read boiler status");
            return false;
        }

        uint16_t newStatus = currentStatus & 0xFFFB;
        if (enable) {
            newStatus |= 0x0004;
        }

        bool success = writeWithRetry(ecW_SetStatusBoiler, newStatus);
        if (success) {
            getWrittenBoilerStatus();
        } else {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to set secondary circuit status");
        }
        return success;
    }

    // Установка общего статуса котла
    bool setBoilerStatus(bool heating, bool dhw, bool secondary) {
        uint16_t newStatus = 0;
        if (heating) newStatus |= 0x0001;
        if (dhw) newStatus |= 0x0002;
        if (secondary) newStatus |= 0x0004;

        if (!writeWithRetry(ecW_SetStatusBoiler, newStatus)) {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to set boiler status");
            return false;
        }

        // Проверяем, что статус обновился
        uint16_t currentStatus;
        if (readWithRetry(ecR_ReadSetStatusBoiler, currentStatus)) {
            return (currentStatus & 0x0007) == newStatus;
        }
        return false;
    }

    // Установка температуры отопления
    bool setTCH(float temp) {
        int16_t value = temp * 10;
        if (!writeWithRetry(ecW_TSetCH, (uint16_t)value)) {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to set CH temperature");
            return false;
        }
        return true;
    }

    // Установка температуры ГВС
    bool setTDHW(float temp) {
        uint16_t value = temp;
        if (!writeWithRetry(ecW_TSetDHW, value)) {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to set DHW temperature");
            return false;
        }
        return true;
    }

    // Перезагрузка адаптера
    bool rebootAdapter() {
        if (!writeWithRetry(ecW_Command, COMMAND_REBOOT)) {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to send reboot command");
            return false;
        }
        
        uint16_t response;
        if (readWithRetry(0x0081, response)) {
            return response == 0;
        }
        return false;
    }

    // Сброс ошибок котла
    bool resetBoilerErrors() {
        if (!writeWithRetry(ecW_Command, COMMAND_RESET_ERRORS)) {
            if (_debugLevel > 0) SerialPrint("E", "EctoControl", "Failed to send reset errors command");
            return false;
        }
        
        uint16_t response;
        if (readWithRetry(0x0081, response)) {
            return response == 0;
        }
        return false;
    }

    // Оптимизированный основной цикл
    void doByInterval() override {
        if (_addr <= 0) return;

        uint32_t currentTime = millis();
        if (currentTime - _lastUpdate < UPDATE_INTERVAL) {
            return;
        }
        _lastUpdate = currentTime;

        // Чтение основных данных одним запросом
        if (!readCommonData()) {
            return;
        }

        // Чтение информации о котле
        getBoilerInfo();
        
        // Чтение записанных настроек
        readSettings();
        
        // Чтение записанного статуса
        getWrittenBoilerStatus();
    }

    // Обработка команд
    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (command == "setHeating") {
            if (param.size() && param[0].isDecimal) {
                setHeating(param[0].valD);
            }
        }
        else if (command == "setDHW") {
            if (param.size() && param[0].isDecimal) {
                setDHW(param[0].valD);
            }
        }
        else if (command == "setSecondaryCircuit") {
            if (param.size() && param[0].isDecimal) {
                setSecondaryCircuit(param[0].valD);
            }
        }
        else if (command == "setBoilerStatus") {
            if (param.size() >= 3) {
                setBoilerStatus(param[0].valD, param[1].valD, param[2].valD);
            }
        }
        else if (command == "getBoilerInfo") {
            getBoilerInfo();
        }
        else if (command == "getBoilerStatus") {
            // Уже читается в readCommonData()
        }
        else if (command == "getWrittenBoilerStatus") {
            getWrittenBoilerStatus();
        }
        else if (command == "getSettings") {
            readSettings();
        }
        else if (command == "setTCH") {
            if (param.size() && param[0].isDecimal) {
                setTCH(param[0].valD);
            }
        }
        else if (command == "setTDHW") {
            if (param.size() && param[0].isDecimal) {
                setTDHW(param[0].valD);
            }
        }
        else if (command == "reboot") {
            rebootAdapter();
        }
        else if (command == "resetErrors") {
            resetBoilerErrors();
        }
        else if (command == "setSetTypeConnect") {
            if (param.size() && param[0].isDecimal) {
                writeWithRetry(ecW_SetTypeConnect, static_cast<uint16_t>(param[0].valD));
            }
        }
        else if (command == "setTSetCHFaultConn") {
            if (param.size() && param[0].isDecimal) {
                writeWithRetry(ecW_TSetCHFaultConn, static_cast<uint16_t>(param[0].valD * 10));
            }
        }
        else if (command == "setTSetMinCH") {
            if (param.size() && param[0].isDecimal) {
                writeWithRetry(ecW_TSetMinCH, static_cast<uint16_t>(param[0].valD * 10));
            }
        }
        else if (command == "setTSetMaxCH") {
            if (param.size() && param[0].isDecimal) {
                writeWithRetry(ecW_TSetMaxCH, static_cast<uint16_t>(param[0].valD * 10));
            }
        }
        else if (command == "setTSetMinDHW") {
            if (param.size() && param[0].isDecimal) {
                writeWithRetry(ecW_TSetMinDHW, static_cast<uint16_t>(param[0].valD));
            }
        }
        else if (command == "setTSetMaxDHW") {
            if (param.size() && param[0].isDecimal) {
                writeWithRetry(ecW_TSetMaxDHW, static_cast<uint16_t>(param[0].valD));
            }
        }
        else if (command == "setSetMaxModLevel") {
            if (param.size() && param[0].isDecimal) {
                writeWithRetry(ecW_SetMaxModLevel, static_cast<uint16_t>(param[0].valD));
            }
        }
        else if (command == "getTempCH") {
            uint16_t regValue;
            if (readWithRetry(ecR_TempCH, regValue)) {
                IoTValue val;
                val.valD = regValue / 10.0;
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getTempDHW") {
            uint16_t regValue;
            if (readWithRetry(ecR_TempDHW, regValue)) {
                IoTValue val;
                val.valD = regValue / 10.0;
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getPressure") {
            uint16_t regValue;
            if (readWithRetry(ecR_Pressure, regValue)) {
                IoTValue val;
                val.valD = (regValue & 0xFF) / 10.0;
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getFlowRate") {
            uint16_t regValue;
            if (readWithRetry(ecR_FlowRate, regValue)) {
                IoTValue val;
                val.valD = (regValue & 0xFF) / 10.0;
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getModLevel") {
            uint16_t regValue;
            if (readWithRetry(ecR_ModLevel, regValue)) {
                IoTValue val;
                val.valD = static_cast<uint8_t>(regValue & 0xFF);
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getBoilerStatusRaw") {
            uint16_t regValue;
            if (readWithRetry(ecR_BoilerStatus, regValue)) {
                IoTValue val;
                val.valD = regValue;
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getTempOutside") {
            uint16_t regValue;
            if (readWithRetry(ecR_TempOutside, regValue)) {
                int8_t temp = static_cast<int8_t>(regValue & 0xFF);
                IoTValue val;
                val.valD = (temp != 0x7F) ? temp : 0;
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getFlagErrorOT") {
            uint16_t regValue;
            if (readWithRetry(ecR_FlagErrorOT, regValue)) {
                IoTValue val;
                val.valD = regValue;
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getMinSetCH") {
            uint16_t regValue;
            if (readWithRetry(ecR_MinSetCH, regValue)) {
                IoTValue val;
                val.valD = regValue / 10.0;
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getMaxSetCH") {
            uint16_t regValue;
            if (readWithRetry(ecR_MaxSetCH, regValue)) {
                IoTValue val;
                val.valD = regValue / 10.0;
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getMinSetDHW") {
            uint16_t regValue;
            if (readWithRetry(ecR_MinSetDHW, regValue)) {
                IoTValue val;
                val.valD = regValue;
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getMaxSetDHW") {
            uint16_t regValue;
            if (readWithRetry(ecR_MaxSetDHW, regValue)) {
                IoTValue val;
                val.valD = regValue;
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getAdapterInfo") {
            uint16_t regValue;
            if (readWithRetry(ecR_AdapterInfo, regValue)) {
                IoTValue val;
                val.valD = regValue;
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getAdapterVersion") {
            uint16_t regValue;
            if (readWithRetry(ecR_AdapterVersion, regValue)) {
                IoTValue val;
                val.valD = regValue;
                val.isDecimal = true;
                return val;
            }
        }
        else if (command == "getUptime") {
            uint16_t regValue;
            if (readWithRetry(ecR_Uptime, regValue)) {
                IoTValue val;
                val.valD = regValue;
                val.isDecimal = true;
                return val;
            }
        }
        else {
            SerialPrint("E", "EctoControl", "Unknown command: " + command);
        }

        return {};
    }

    ~EctoControlAdapter() {
        if (_modbusUART) {
            ((HardwareSerial*)_modbusUART)->end();
            delete _modbusUART;
            _modbusUART = nullptr;
        }
    }
};

void* getAPI_EctoControlAdapter(String subtype, String param) {
    if (subtype == F("ecAdapter")) {
        return new EctoControlAdapter(param);
    }
    return nullptr;
}