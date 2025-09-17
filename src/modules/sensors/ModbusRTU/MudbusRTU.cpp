#include "Global.h"
#include "classes/IoTItem.h"
#include <map>
#include <HardwareSerial.h>

//  https://github.com/4-20ma/ModbusMaster

#include <ModbusMaster.h>

// class ModbusUart;
Stream *_modbusUART = nullptr;
int _DIR_PIN = 0;

#define UART_LINE 2

// Modbus stuff
// Данные Modbus по умолчанию

#define MODBUS_RX_PIN 18        // Rx pin
#define MODBUS_TX_PIN 19        // Tx pin
#define MODBUS_SERIAL_BAUD 9600 // Baud rate for esp32 and max485 communication

void modbusPreTransmission()
{
  //  delay(500);
  if (_DIR_PIN)
    digitalWrite(_DIR_PIN, HIGH);
}

// Pin 4 made low for Modbus receive mode
// Контакт 4 установлен на низком уровне для режима приема Modbus
void modbusPostTransmission()
{
  if (_DIR_PIN)
    digitalWrite(_DIR_PIN, LOW);
  //  delay(500);
}

float readFunctionModBus(const uint8_t &func, const uint8_t &addr, const uint16_t &reg, const uint8_t &count = 1, const bool &isFloat = 0)
{
  float retValue = 0;
  if (_modbusUART)
  {

    node.begin(addr, (Stream &)*_modbusUART);
    uint8_t result;
    // uint16_t data[2] = {0, 0};
    uint32_t reading;
    switch (func)
    {
    // Modbus function 0x01 Read Coils
    // Функция Modbus 0x01 Чтение Катушек
    case 1: // 0x01
      count = count > 16 ? 16 : count;
      count = count < 1 ? 1 : count;
      result = node.readCoils(reg, count);
      if (_debug)
      {
        SerialPrint("I", "ModbusNode", "readCoils, addr: " + String(addr, HEX) + ", reg: " + String(reg, HEX) + " = result: " + String(result, HEX));
      }
      break;

    // Modbus function 0x02 Read Discrete Inputs
    // Функция Modbus 0x02 Чтение дискретных входов
    case 2: // 0x02
      count = count > 16 ? 16 : count;
      count = count < 1 ? 1 : count;
      result = node.readDiscreteInputs(reg, count);
      if (_debug)
      {
        SerialPrint("I", "ModbusNode", "readDiscreteInputs, addr: " + String(addr, HEX) + ", reg: " + String(reg, HEX) + " = result: " + String(result, HEX));
      }
      break;

    // Modbus function 0x03 Read Holding Registers
    case 3: // 0x03
      count = count > 2 ? 2 : count;
      count = count < 1 ? 1 : count;
      result = node.readHoldingRegisters(reg, count);
      if (_debug)
      {
        SerialPrint("I", "ModbusNode", "readHoldingRegisters, addr: " + String(addr, HEX) + ", reg: " + String(reg, HEX) + " = result: " + String(result, HEX));
      }
      break;

    // Modbus function 0x04 Read Input Registers
    // Функция Modbus 0x04 Чтение входных регистров
    case 4: // 0x04
      count = count > 2 ? 2 : count;
      count = count < 1 ? 1 : count;
      result = node.readInputRegisters(reg, count);
      if (_debug)
      {
        SerialPrint("I", "ModbusNode", "readInputRegisters, addr: " + String(addr, HEX) ", reg: " + String(reg, HEX) + " = result: " + String(result, HEX));
      }
      break;

    default:
      SerialPrint("I", "ModbusNode", "Unknown function or not supported: " + String(func, HEX));

      break;
    }

    if (result == node.ku8MBSuccess)
    {
      if ((func == 3 || func == 4) && countR == 2)
      {
        reading = node.getResponseBuffer(0x00) |
                  node.getResponseBuffer(0x01) << 16;
      }
      else
      {
        reading = node.getResponseBuffer(0x00);
      }
      node.clearResponseBuffer();

      if (_debug)
      {
        SerialPrint("I", "ModbusMaster", "Success, Received data, register: " + String(reg) + " = " + String(reading, HEX));
      }
      if (isFloat)
      {
        retValue = *(float *)&reading;
      }
      retValue = reading;
    }
    else
    {

      if (_debug)
      {
        SerialPrint("I", "ModbusMaster", "Failed, Response Code: " + String(result, HEX));
      }
    }
  }
  return retValue;
}

ModbusMaster node;

class ModbusNode : public IoTItem
{
private:
  // Initialize the ModbusMaster object as node
  // Инициализируем объект ModbusMaster как узел

  uint8_t _addr = 0;    // Адрес слейва от 1 до 247
  String _regStr = "";  // Адрес регистра который будем дергать ( по коду от 0х0000 до 0х????)
  String _funcStr = ""; // Функция ModBUS
  uint8_t _func;
  uint16_t _reg = 0;
  uint8_t _countReg = 1;
  bool _isFloat = 0;
  bool _debug; // Дебаг

public:
  ModbusNode(String parameters) : IoTItem(parameters)
  {
    _addr = jsonReadInt(parameters, "addr"); // адреса slave прочитаем с веба
    jsonRead(parameters, "reg", _regStr);    // адреса регистров прочитаем с веба
    jsonRead(parameters, "func", _funcStr);  // Функция ModBUS
    _countReg = jsonReadInt(parameters, "count");
    jsonRead(parameters, "float", _isFloat);
    jsonRead(parameters, "debug", _debug);
    _func = hexStringToUint8(_funcStr);
    _reg = hexStringToUint16(_regStr);
  }

  void doByInterval()
  {
    regEvent(readFunctionModBus(_func, _addr, _reg, _countReg, _isFloat), "register");
  }

  ~ModbusNode(){};
};

class ModbusMaster : public IoTItem
{
private:
  int _rx = MODBUS_RX_PIN; // адреса прочитаем с веба
  int _tx = MODBUS_TX_PIN;
  int _baud = MODBUS_SERIAL_BAUD;
  String _prot = "SERIAL_8N1";
  int protocol = SERIAL_8N1;

  int _addr = 0;       // Адрес слейва от 1 до 247 ( вроде )
  String _regStr = ""; // Адрес регистра который будем дергать ( по коду от 0х0000 до 0х????)
  uint16_t _reg = 0;
  bool _debug; // Дебаг

public:
  ModbusMaster(String parameters) : IoTItem(parameters)
  {
    _rx = jsonReadInt(parameters, "RX"); // прочитаем с веба
    _tx = jsonReadInt(parameters, "TX");
    _DIR_PIN = jsonReadInt(parameters, "DIR_PIN");
    _baud = jsonReadInt(parameters, "baud");
    _prot = jsonReadStr(parameters, "protocol");
    jsonRead(parameters, "debug", _debug);

    if (_prot == "SERIAL_8N1")
    {
      protocol = SERIAL_8N1;
    }
    else if (_prot == "SERIAL_8N2")
    {
      protocol = SERIAL_8N2;
    }

    pinMode(_DIR_PIN, OUTPUT);
    digitalWrite(_DIR_PIN, LOW);

    // Serial2.begin(baud-rate, protocol, RX pin, TX pin);

    _modbusUART = new HardwareSerial(UART_LINE);

    if (_debug)
    {
      SerialPrint("I", "ModbusMaster", "baud: " + String(_baud) + ", protocol: " + String(protocol, HEX) + ", RX: " + String(_rx) + ", TX: " + String(_tx));
    }
    ((HardwareSerial *)_modbusUART)->begin(_baud, protocol, _rx, _tx); // выбираем тип протокола, скорость и все пины с веба
    ((HardwareSerial *)_modbusUART)->setTimeout(200);

    node.preTransmission(modbusPreTransmission);
    node.postTransmission(modbusPostTransmission);
  }

  // Комманды из сценария
  IoTValue execute(String command, std::vector<IoTValue> &param)
  {
    IoTValue val;
    uint8_t result;
    uint32_t reading;

    uint16_t _reg = 0;
    uint8_t count = 1;
    bool isFloat = 0;
    if (command == "readInputRegisters") // vout = mb.readInputRegisters(1, "0х0000", 1, 0) - "Адрес","Регистр","Кличество регистров","1-float, 0-long"
    {
      if (param.size())
      {
        _addr = param[0].valD;
        _reg = hexStringToUint16(param[1].valS);
        count = (uint8_t)param[2].valD;
        count = count > 2 ? 2 : count;
        count = count < 1 ? 1 : count;
        isFloat = (bool)param[3].valD;
        val.valD = readFunctionModBus(0x04, _addr, _reg, count, isFloat);
      }
      return val;
    }
    else if (command == "readHoldingRegisters") // vout = mb.readHoldingRegisters(1, "0х0000", 2, 1) - "Адрес","Регистр","Кличество регистров","1-float, 0-long"
    {
      if (param.size())
      {
        _addr = param[0].valD;
        _reg = hexStringToUint16(param[1].valS);
        count = (uint8_t)param[2].valD;
        count = count > 2 ? 2 : count;
        count = count < 1 ? 1 : count;
        isFloat = (bool)param[3].valD;
        val.valD = readFunctionModBus(0x03, _addr, _reg, count, isFloat);
      }
      return val;
    }
    else if (command == "readCoils") // vout = mb.readCoils(1, \"0х0000\", 1) - "Адрес","Регистр","Кличество бит"
    {
      if (param.size())
      {
        count = (uint8_t)param[2].valD;
        count = count > 16 ? 16 : count;
        count = count < 1 ? 1 : count;
        _addr = param[0].valD;
        _reg = hexStringToUint16(param[1].valS);
        node.begin(_addr, (Stream &)*_modbusUART);
        val.valD = readFunctionModBus(0x01, _addr, _reg, count);
      }
      return val;
    }
    else if (command == "readDiscreteInputs") // vout = mb.readDiscreteInputs(1, \"0х0000\", 1) - "Адрес","Регистр","Кличество бит"
    {
      if (param.size())
      {
        count = (uint8_t)param[2].valD;
        count = count > 16 ? 16 : count;
        count = count < 1 ? 1 : count;
        _addr = param[0].valD;
        _reg = hexStringToUint16(param[1].valS);
        node.begin(_addr, (Stream &)*_modbusUART);
        val.valD = readFunctionModBus(0x02, _addr, _reg, count);
      }
      return val;
    }
    else if (command == "writeSingleRegister") // vout = mb.writeSingleRegister(1,"0x0003", 1) - addr, register, state
    {
      if (param.size())
      {
        node.begin((uint8_t)param[0].valD, (Stream &)*_modbusUART);

        _addr = param[0].valD;
        _reg = hexStringToUint16(param[1].valS);
        // bool state = param[2].valD;
        uint16_t state = param[2].valD;
        result = node.writeSingleRegister(_reg, state);
        if (_debug)
        {
          SerialPrint("I", "ModbusMaster", "writeSingleRegister, addr: " + String((uint8_t)_addr, HEX) + ", regStr: " + _regStr + ", reg: " + String(_reg, HEX) + ", state: " + String(state) + " = result: " + String(result, HEX));
        }
      }
      // Что можно вернуть в ответ на запись ???
      return {};
    }
    else if (command == "writeSingleCoil") // vout = mb.writeSingleCoil(1,"0x0003", 1) - addr, register, state
    {
      if (param.size())
      {
        _addr = param[0].valD;
        _reg = hexStringToUint16(param[1].valS);
        node.begin(_addr, (Stream &)*_modbusUART);

        bool state = param[2].valD;
        result = node.writeSingleCoil(_reg, state);
        if (_debug)
        {
          SerialPrint("I", "ModbusMaster", "writeSingleCoil, addr: " + String((uint8_t)_addr, HEX) + ", regStr: " + _regStr + ", reg: " + String(_reg, HEX) + ", state: " + String(state) + " = result: " + String(result, HEX));
        }
      }
      // Что можно вернуть в ответ на запись койлов???
      return {};
    }
    else if (command == "writeMultipleCoils") // Пример: mb.writeMultipleCoils(1, \"0х0000\", 4, 3) - будут записаны в четыре бита 0011
    {
      if (param.size())
      {
        _addr = param[0].valD;
        _reg = hexStringToUint16(param[1].valS);
        count = (uint8_t)param[2].valD;
        count = count > 16 ? 16 : count;
        count = count < 1 ? 1 : count;
        node.begin(_addr, (Stream &)*_modbusUART);

        uint16_t state = param[3].valD;
        node.setTransmitBuffer(0, state);
        result = node.writeMultipleRegisters(_reg, count);
        node.clearTransmitBuffer();
        if (_debug)
        {
          SerialPrint("I", "ModbusMaster", "writeSingleCoil, addr: " + String((uint8_t)_addr, HEX) + ", regStr: " + _regStr + ", reg: " + String(_reg, HEX) + ", state: " + String(state) + " = result: " + String(result, HEX));
        }
      }
      return {};
    }
    // На данный момент записывает 2(два) регистра!!!!! Подходит для записи float?? Функция 0х10 протокола.
    else if (command == "writeMultipleRegisters") // mb.writeMultipleRegisters(1, \"0х0000\",  1234.987)
    {
      if (param.size())
      {
        _addr = param[0].valD;
        _reg = hexStringToUint16(param[1].valS);
        node.begin(_addr, (Stream &)*_modbusUART);

        float state = param[2].valD;

        node.setTransmitBuffer(0, lowWord(state));
        node.setTransmitBuffer(1, highWord(state));
        result = node.writeMultipleRegisters(_reg, 2);
        node.clearTransmitBuffer();
        if (_debug)
        {
          SerialPrint("I", "ModbusMaster", "writeMultipleRegisters, addr: " + String((uint8_t)_addr, HEX) + ", reg: " + String(_reg, HEX) + ", state: " + String(state) + " (" + String(state, HEX) + ")");
        }
      }
      return {};
    }
    return val;
  }

  ~ModbusMaster()
  {
    delete _modbusUART;
    _modbusUART = nullptr;
  };
};

void *getAPI_ModbusRTU(String subtype, String param)
{

  if (subtype == F("mbNode"))
  {
    return new ModbusNode(param);
  }
  else if (subtype == F("mbMaster"))
  {
    return new ModbusMaster(param);
  }
  {
    return nullptr;
  }
}
