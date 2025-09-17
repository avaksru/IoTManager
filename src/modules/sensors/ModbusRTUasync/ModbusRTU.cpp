#include "Global.h"
#include "classes/IoTItem.h"
#include <map>
#include <HardwareSerial.h>

#include "Logging.h"
#include "ModbusClientRTU.h"
#include "CoilData.h"

// class ModbusUart;
Stream *_modbusUART = nullptr;

// Данные Modbus по умолчанию
int8_t MODBUS_DIR_PIN = 0;
#define MODBUS_UART_LINE 2
#define MODBUS_RX_PIN 18        // Rx pin
#define MODBUS_TX_PIN 19        // Tx pin
#define MODBUS_SERIAL_BAUD 9600 // Baud rate for esp32 and max485 communication

// bool modBus_data_ready = false;
uint32_t modBus_Token_count = 0; // Счетчик токенов для Нод, 0 - всегду у главного класса, дальше по порядку
class ModbusNode;
std::map<uint32_t, ModbusNode *> MBNoneMap;
ModbusClientRTU *MB = nullptr;

ModbusClientRTU *instanceModBus(int8_t _DR)
{
  if (!MB)
  { // Если библиотека ранее инициализировалась, т о просто вернем указатель
    // Инициализируем библиотеку
    if (_DR)
      MB = new ModbusClientRTU(_DR);
    else
      MB = new ModbusClientRTU();
  }
  return MB;
}
// ModbusClientRTU MB(_DIR_PIN);
// ModbusClientRTU MB();

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
  uint32_t _token = 0;
  bool _isFloat = 0;
  CoilData _respCoil;

public:
  ModbusNode(String parameters) : IoTItem(parameters)
  {
    _addr = jsonReadInt(parameters, "addr"); // адреса slave прочитаем с веба
    jsonRead(parameters, "reg", _regStr);    // адреса регистров прочитаем с веба
    jsonRead(parameters, "func", _funcStr);  // Функция ModBUS
    jsonRead(parameters, "isFloat", _isFloat);
    _countReg = jsonReadInt(parameters, "count");
    _func = hexStringToUint8(_funcStr);
    _reg = hexStringToUint16(_regStr);
    modBus_Token_count++;
    _token = modBus_Token_count;
    MBNoneMap[_token] = this;
    Serial.printf("Добавлен нода/токен: %s - %d\n", getID(), _token);
  }

  void doByInterval()
  {
    if (!MB)
    {
      Serial.printf("ModbusNode: ModbusClientAsync is NULL\n");
      return;
    }
    if (_func == 0x04) // vout = mb.readInputRegisters(1, "0х0000", 1, 0) - "Адрес","Регистр","Кличество регистров"
    {
      // val.valD = readFunctionModBus(0x04, _addr, _reg, count, isFloat);
      Serial.printf("sending request with token %d\n", _token);
      Error err;
      err = MB->addRequest(_token, _addr, READ_INPUT_REGISTER, _reg, _countReg);
      if (err != SUCCESS)
      {
        ModbusError e(err);
        Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
      }
    }
    else if (_func == 0x03) // vout = mb.readHoldingRegisters(1, "0х0000", 2, 1) - "Адрес","Регистр","Кличество регистров"
    {
      Serial.printf("sending request with token %d\n", _token);
      Error err;
      err = MB->addRequest(_token, _addr, READ_HOLD_REGISTER, _reg, _countReg);
      if (err != SUCCESS)
      {
        ModbusError e(err);
        Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
      }
    }
    else if (_func == 0x01) // vout = mb.readCoils(1, \"0х0000\", 1) - "Адрес","Регистр","Кличество бит"
    {
      Serial.printf("sending request with token %d\n", _token);
      Error err;
      err = MB->addRequest(_token, _addr, READ_COIL, _reg, _countReg);
      if (err != SUCCESS)
      {
        ModbusError e(err);
        Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
      }
    }
    else if (_func == 0x02) // vout = mb.readDiscreteInputs(1, \"0х0000\", 1) - "Адрес","Регистр","Кличество бит"
    {
      Serial.printf("sending request with token %d\n", _token);
      Error err;
      err = MB->addRequest(_token, _addr, READ_DISCR_INPUT, _reg, _countReg);
      if (err != SUCCESS)
      {
        ModbusError e(err);
        Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
      }
    }
  }

  void parseMB(ModbusMessage response)
  {
    if (MB)
    {
      if (_func == 0x02 || _func == 0x01) // coil
      {
        uint16_t val;
        // response.get(3, val);
        // regEvent((float)val, "ModbusNode");
        CoilData cd(_countReg);
        cd.set(0, _countReg, (uint8_t *)response.data() + 3);
        _respCoil = cd;
        cd.print("Received                          : ", Serial);
        val = cd[0];
        regEvent(val, "ModbusNode");
      }
      else
      {
        if (_countReg == 2 && _isFloat)
        {
          float val;
          response.get(3, val);
          regEvent(val, "ModbusNode");
        }
        else
        {
          if (_countReg == 2)
          {
            uint16_t val1, val2;
            response.get(3, val1);
            response.get(5, val2);
            Serial.printf("COUNT 2: %02X - %02X\n", (int)val1, (int)val2);
            long val = val1 | val2 << 16;
            regEvent((float)val, "ModbusNode");
          }
          else
          {
            uint16_t val;
            response.get(3, val);
            regEvent((float)val, "ModbusNode");
          }
        }
      }
    }
  }

  // Комманды из сценария
  IoTValue execute(String command, std::vector<IoTValue> &param)
  {
    IoTValue val;
    // uint8_t result;
    // uint32_t reading;

    uint16_t _index = 0;

    if (command == "getBits") 
    {
      if (param.size())
      {
        if (_respCoil.size() > _index)
        {
          _index = param[0].valD;
          val.valD = _respCoil[_index];
          return val;
        }
      }
    }
    return {};
  }

  ~ModbusNode()
  {
    //MBNoneMap.erase(_token);
  };
};

// Define an onData handler function to receive the regular responses
// Arguments are received response message and the request's token
void handleModBusData(ModbusMessage response, uint32_t token)
{
  printf("Response --- Token:%d FC:%02X Server:%d Length:%d\n",
         token,
         response.getFunctionCode(),
         response.getServerID(),
         response.size());
  HEXDUMP_N("Data dump", response.data(), response.size());

  // // First value is on pos 3, after server ID, function code and length byte
  // uint16_t offs = 3;
  // // The device has values all as IEEE754 float32 in two consecutive registers
  // offs = response.get(offs, values[i]);
  // uint16_t val;
  // response.get(3, val);
  if (MBNoneMap[token])
  {
    MBNoneMap[token]->parseMB(response);
  }
  else
  {
    Serial.printf("Токен/Нода не найден: %d\n", token);
  }
  // modBus_data_ready = true;
}

// Define an onError handler function to receive error responses
// Arguments are the error code returned and a user-supplied token to identify the causing request
void handleModBusError(Error error, uint32_t token)
{
  // ModbusError wraps the error code and provides a readable error message for it
  ModbusError me(error);
  // LOG_E("Error response: %02X - %s\n", (int)me, (const char *)me);
  Serial.printf("Error response: %02X - %s\n", (int)me, (const char *)me);
}

class ModbusClientAsync : public IoTItem
{
private:
  int8_t _rx = MODBUS_RX_PIN; // адреса прочитаем с веба
  int8_t _tx = MODBUS_TX_PIN;
  int _baud = MODBUS_SERIAL_BAUD;
  String _prot = "SERIAL_8N1";
  int protocol = SERIAL_8N1;

  int _addr = 0;       // Адрес слейва от 1 до 247 ( вроде )
  String _regStr = ""; // Адрес регистра который будем дергать ( по коду от 0х0000 до 0х????)
  uint16_t _reg = 0;
  bool _debug;         // Дебаг
  uint32_t _token = 0; // Токен у главного класса весгда 0

public:
  ModbusClientAsync(String parameters) : IoTItem(parameters)
  {
    _rx = (int8_t)jsonReadInt(parameters, "RX"); // прочитаем с веба
    _tx = (int8_t)jsonReadInt(parameters, "TX");
    MODBUS_DIR_PIN = (int8_t)jsonReadInt(parameters, "DIR_PIN");
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

    pinMode(MODBUS_DIR_PIN, OUTPUT);
    digitalWrite(MODBUS_DIR_PIN, LOW);

    // Serial2.begin(baud-rate, protocol, RX pin, TX pin);
    instanceModBus(MODBUS_DIR_PIN);
    _modbusUART = new HardwareSerial(MODBUS_UART_LINE);

    if (_debug)
    {
      SerialPrint("I", "ModbusClientAsync", "baud: " + String(_baud) + ", protocol: " + String(protocol, HEX) + ", RX: " + String(_rx) + ", TX: " + String(_tx));
    }
    RTUutils::prepareHardwareSerial((HardwareSerial &)*_modbusUART);
    // Serial2.begin(BAUDRATE, SERIAL_8N1, RXPIN, TXPIN);
    ((HardwareSerial *)_modbusUART)->begin(_baud, protocol, _rx, _tx); // выбираем тип протокола, скорость и все пины с веба
    ((HardwareSerial *)_modbusUART)->setTimeout(200);

    // Set up ModbusRTU client.
    // - provide onData handler function
    MB->onDataHandler(&handleModBusData);
    // - provide onError handler function
    MB->onErrorHandler(&handleModBusError);
    // Set message timeout to 2000ms
    MB->setTimeout(2000);
    // Start ModbusRTU background task
    MB->begin((HardwareSerial &)*_modbusUART);
    // MBNoneMap[_token] = this;
  }

  // Комманды из сценария
  IoTValue execute(String command, std::vector<IoTValue> &param)
  {
    IoTValue val;
    // uint8_t result;
    // uint32_t reading;

    uint16_t _reg = 0;
    uint8_t count = 1;
    if (command == "writeSingleRegister") // vout = mb.writeSingleRegister(1,"0x0003", 1) - addr, register, state
    {
      if (param.size())
      {
        // node.begin((uint8_t)param[0].valD, (Stream &)*_modbusUART);

        _addr = param[0].valD;
        _reg = hexStringToUint16(param[1].valS);
        // bool state = param[2].valD;
        uint16_t state = param[2].valD;
        // result = node.writeSingleRegister(_reg, state);
        if (_debug)
        {
          SerialPrint("I", "ModbusClientAsync", "writeSingleRegister, addr: " + String((uint8_t)_addr, HEX) + ", regStr: " + _regStr + ", reg: " + String(_reg, HEX) + ", state: " + String(state));
        }

        // We will first set the register to a known state, read the register,
        // then write to it and finally read it again to verify the change

        // Set defined conditions first - write 0x1234 to the register
        // The Token value is used in handleData to avoid the output for this first preparation request!
        // uint32_t Token = 1111;
        Serial.printf("sending request with token %d\n", _token);
        Error err;
        err = MB->addRequest(_token, _addr, WRITE_HOLD_REGISTER, _reg, state);
        if (err != SUCCESS)
        {
          ModbusError e(err);
          Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
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
        // node.begin(_addr, (Stream &)*_modbusUART);

        bool state = param[2].valD;
        // result = node.writeSingleCoil(_reg, state);
        if (_debug)
        {
          SerialPrint("I", "ModbusClientAsync", "writeSingleCoil, addr: " + String((uint8_t)_addr, HEX) + ", regStr: " + _regStr + ", reg: " + String(_reg, HEX) + ", state: " + String(state));
        }

        // next set a single coil at 8
        Serial.printf("sending request with token %d\n", _token);
        Error err;
        ModbusMessage msg;
        if (state)
        {
          msg.setMessage(_addr, WRITE_COIL, _reg, 0xFF00);
          err = MB->addRequest(msg, _token);
        }
        else
        {
          // msg.setMessage(_addr, WRITE_COIL, _reg, 0x0000);
          err = MB->addRequest(_token, _addr, WRITE_COIL, _reg, 0);
        }
        if (err != SUCCESS)
        {
          ModbusError e(err);
          Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
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
        // node.begin(_addr, (Stream &)*_modbusUART);

        uint16_t state = param[3].valD;
        // node.setTransmitBuffer(0, state);
        // result = node.writeMultipleRegisters(_reg, count);
        // node.clearTransmitBuffer();
        Serial.printf("NOT SUPPORTED!\n");
        if (_debug)
        {
          SerialPrint("I", "ModbusClientAsync", "writeSingleCoil, addr: " + String((uint8_t)_addr, HEX) + ", regStr: " + _regStr + ", reg: " + String(_reg, HEX) + ", state: " + String(state));
        }

        CoilData cd(12);
        // Finally set a a bunch of coils starting at 20
        cd = "011010010110";
        Serial.printf("sending request with token %d\n", _token);
        Error err;
        err = MB->addRequest(_token, _addr, WRITE_MULT_COILS, _reg, cd.coils(), cd.size(), cd.data());
        if (err != SUCCESS)
        {
          ModbusError e(err);
          Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
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
        // node.begin(_addr, (Stream &)*_modbusUART);

        float state = param[2].valD;

        // node.setTransmitBuffer(0, lowWord(state));
        // node.setTransmitBuffer(1, highWord(state));
        // result = node.writeMultipleRegisters(_reg, 2);
        // node.clearTransmitBuffer();
        Serial.printf("NOT SUPPORTED!\n");
        if (_debug)
        {
          SerialPrint("I", "ModbusClientAsync", "writeMultipleRegisters, addr: " + String((uint8_t)_addr, HEX) + ", reg: " + String(_reg, HEX) + ", state: " + String(state) + " (" + String(state, HEX) + ")");
        }
      }
      return {};
    }
    return val;
  }

  ~ModbusClientAsync()
  {
    delete _modbusUART;
    _modbusUART = nullptr;
    MBNoneMap.clear();
  };
};

void *getAPI_ModbusRTUasync(String subtype, String param)
{
  if (subtype == F("mbNode"))
  {
    return new ModbusNode(param);
  }
  else if (subtype == F("mbClient"))
  {
    return new ModbusClientAsync(param);
  }
  {
    return nullptr;
  }
}
