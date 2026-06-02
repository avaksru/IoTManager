#pragma once

#include "ModbusEC.h"
#include "AdapterCommon.h"
// #include "Stream.h"
#include <vector>

struct IoTValue;

// static void publishData(String widget, String status);
static void (*_publishData)(String, String);

// static void sendTelegramm(String msg);
static void (*_sendTelegramm)(String);

static void (*_SerialPrint)(const String &, const String &, const String &); //, const String& itemId = ""

class RsEctoControl //: public ModbusMaster
{
private:
    ModbusMaster node;
    String _license;
    uint8_t _addr;
    uint8_t _debug;
    Stream *_modbusUART;

    BoilerInfo info;
    BoilerStatus status;

    uint16_t code;
    uint16_t codeExt;
    uint8_t flagErr;
    float flow;
    float maxSetCH;
    float maxSetDHW;
    float minSetCH;
    float minSetDHW;
    float modLevel;
    float press;
    float tCH;
    float tDHW;
    float tOut;
    bool enableCH;
    bool enableDHW;
    bool enableCH2;
    bool _isNetworkActive;
    bool _mqttIsConnect;
    int md = 0;
    int md2 = 0;

    uint32_t getFlashChipIdNew();

    uint32_t ESP_getChipId(void);

    const String getChipId();

    bool readFunctionModBus(const uint16_t &reg, uint16_t &reading);

    bool writeFunctionModBus(const uint16_t &reg, uint16_t &data);

public:
    uint8_t _DIR_PIN;
    //=======================================================================================================
    // setup()
    RsEctoControl(String parameters); //: ModbusMaster();

    ~RsEctoControl();

    void begin(uint8_t slave, Stream &serial);

    static void initFunction(void (*_publishData_)(String, String), void (*_sendTelegramm_)(String), void (*_SerialPrint_)(const String &, const String &, const String &)); //, const String&

    void doByInterval();

    // Основной цикл программы
    void loop(bool isNetworkActive, bool mqttIsConnect);

    // Исполнительные комманды
    void execute(String command, std::vector<IoTValue> &param);

    bool getModelVersion();
    bool getBoilerInfo();
    bool getBoilerStatus();
    bool getCodeError();
    bool getCodeErrorExt();
    bool getFlagErrorOT();
    bool getFlowRate();
    bool getMaxSetCH();
    bool getMaxSetDHW();
    bool getMinSetCH();
    bool getMinSetDHW();
    bool getModLevel();
    bool getPressure();
    bool getTempCH();
    bool getTempDHW();
    bool getTempOutside();

    bool setTypeConnect(float &data);
    bool setTCH(float &data);
    bool setTCHFaultConn(float &data);
    bool setMinCH(float &data);
    bool setMaxCH(float &data);
    bool setMinDHW(float &data);
    bool setMaxDHW(float &data);
    bool setTDHW(float &data);
    bool setMaxModLevel(float &data);
    bool setStatusCH(bool data);
    bool setStatusDHW(bool data);
    bool setStatusCH2(bool data);

    bool lockOutReset();
    bool rebootAdapter();
};
