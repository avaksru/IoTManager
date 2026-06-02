#include "Global.h"
#include "classes/IoTItem.h"

#include <HardwareSerial.h>
#include <ModbusRTU.h>

#define UART_LINE 1

// read
#define Remaining_Water 0x2002
#define Remaining_Water2 0x2003
#define Remaining_Time 0x2004
#define Fault_Status 0x2005
#define Current_Flow_Rate 0x2006
#define Regeneration_Time_Hour 0x2009
#define Regeneration_Time_Minute 0x200A
#define Current_Time_Hour 0x201D
#define Current_Time_Minute 0x201E
#define Current_Status 0x2007
#define Signal_Output 0x200E
// write
#define Regeneration_control_mode 0x3002
#define Switch_working_position 0x3018

/*
0x2002 + 0x2003  Оставшаяся вода (float)
0x2004           Оставшееся время мин
0x2005           Статус обычный/неисправность
0x2006           Текущий расход
0x2009           Время регенерации час
0x200A           Время регенерации мин
0x201D           Текущее время час
0x201E           Текущее время мин
0x2007           Текущий статус
0x200E           Выходной сигнал

Запись:
0x3002           Режим управления регенерацией (для клапана измерительного типа)
0x3018           Рабочее положение переключателя (Сила регенерации)

*/

class ModbusEmelianov : public IoTItem
{
private:
  Stream *_modbusUART = nullptr;

  ModbusRTU *mb = nullptr;

  int _rx = 22;     // Rx pin
  int _tx = 15;     // Tx pin
  int _dir = 23;     // Dir pin
  int _baud = 9600; // Baud rate for esp32 and max485 communication

  String _prot = "SERIAL_8N1";
  int _protocol = SERIAL_8N1;

  int _addr = 1;       // Адрес слейва от 1 до 247 ( вроде )
  String _regStr = ""; // Адрес регистра который будем дергать ( по коду от 0х0000 до 0х????)
  uint16_t _reg = 0;
  bool _debug = 1;

      uint16_t readArr[13] = {0};
    

public:
  ModbusEmelianov(String parameters) : IoTItem(parameters)
  {

    String addr;

    _rx = jsonReadInt(parameters, "rx"); // прочитаем с веба
    _tx = jsonReadInt(parameters, "tx");
    _dir = jsonReadInt(parameters, "dir");
    _baud = jsonReadInt(parameters, "baud");
    _prot = jsonReadStr(parameters, "protocol");
    _addr = jsonReadInt(parameters, "addr");

   // _debug = jsonRead(parameters, "debug", _debug);

    if (_prot == "SERIAL_8N1")
      _protocol = SERIAL_8N1;

    if (_prot == "SERIAL_8N2")
      _protocol = SERIAL_8N2;

   // pinMode(_dir, OUTPUT);
   // digitalWrite(_dir, LOW);

    _modbusUART = new HardwareSerial(UART_LINE);

    if (_debug)
    {
      //SerialPrint("I", "ModbusMaster", "baud: " + String(_baud) + ", protocol: " + String(_protocol, HEX) + ", RX: " + String(_rx) + ", TX: " + String(_tx));
    }
    // start
    ((HardwareSerial *)_modbusUART)->begin(_baud, SERIAL_8N1, _rx, _tx); // выбираем тип протокола, скорость и все пины с веба
    ((HardwareSerial *)_modbusUART)->setTimeout(200);

    mb = new ModbusRTU;

    mb->begin(_modbusUART, _dir);
    mb->master();
    wait();

  }

  void doByInterval()
  {

    if (_debug)
    {
       SerialPrint("I", "ModbusMaster", "baud: " + String(_baud) + ", protocol: " + String(_protocol, HEX) + ", RX: " + String(_rx) + ", TX: " + String(_tx));
       SerialPrint("I", "ModbusMaster", "addr: " + String(_addr) + ", dir: " + String(_dir));
    }



    if (!mb->slave())
    { // Check if no transaction in progress

      // Вот это я не понимаю. Вероятно чтение можно переместить. И делать его только если есть соответствующий элемент на дашборде
      mb->readHreg(_addr, Remaining_Water, readArr, 5);
      wait();
      mb->readHreg(_addr, Regeneration_Time_Hour, readArr + 5, 2); // Send Read Hreg from Modbus Server
      wait();
      mb->readHreg(_addr, Current_Time_Hour, readArr + 7, 2);
      wait();
      mb->readHreg(_addr, Current_Status, readArr + 9, 1);
      wait();
      mb->readHreg(_addr, Signal_Output, readArr + 10, 1);
      wait();

//not read
      // mb->readHreg(_addr, Regeneration_control_mode, readArr + 11, 1);
      //wait();
      //  mb->readHreg(_addr, Switch_working_position, readArr + 12, 1);
      //wait();
     

      //  SerialPrint("i", F("ModbusMaster"), "Current_Time = " + String(readArr[7]) + ":" + String(readArr[8]));
    }

    // Ищем на дашборде элемент с соответствующим ID. Если нашли, то пишем в него значение прочитанное из   Modbus
    // 0x2002 + 0x2003  Оставшаяся вода (float)
    IoTItem *tmp = findIoTItem("Remaining_Water");
    if (tmp)
    {
      tmp->setValue(String(readArr[0] + readArr[1]), false);
      if (_debug)
      {
        SerialPrint("i", F("ModbusMaster"), "Remaining_Water = " + String(readArr[0] + readArr[1]));
      }
    }
    // 0x2004           Оставшееся время мин
    tmp = findIoTItem("Remaining_Time");
    if (tmp)
    {
      tmp->setValue(String(readArr[2]), false);
      if (_debug)
      {
        SerialPrint("i", F("ModbusMaster"), "Remaining_Time = " + String(readArr[2]));
      }
    }
    // 0x2005           Статус ошибки
    tmp = findIoTItem("Fault_Status");
    if (tmp)
    {
      tmp->setValue(String(readArr[3]), false);
      if (_debug)
      {
        SerialPrint("i", F("ModbusMaster"), "Fault_Status = " + String(readArr[3]));
      }
    }
    // 0x2006           Текущий расход
    tmp = findIoTItem("Current_Flow_Rate");
    if (tmp)
    {
      tmp->setValue(String(readArr[4]), false);
      if (_debug)
      {
        SerialPrint("i", F("ModbusMaster"), "Current_Flow_Rate = " + String(readArr[4]));
      }
    }
    // 0x2009           Время регенерации
    tmp = findIoTItem("Regeneration_Time");
    if (tmp)
    {
      tmp->setValue(String(readArr[5]) + ":" + String(readArr[6]), false);
      if (_debug)
      {
        SerialPrint("i", F("ModbusMaster"), "Regeneration_Time = " + String(readArr[5]) + ":" + String(readArr[6]));
      }
    }
    // 0x201D           Текущее время
    tmp = findIoTItem("Current_Time");
    if (tmp)
    {
      tmp->setValue(String(readArr[7]) + ":" + String(readArr[8]), false);
      if (_debug)
      {
        SerialPrint("i", F("ModbusMaster"), "Current_Time = " + String(readArr[7]) + ":" + String(readArr[8]));
      }
    }
    // 0x2007           Текущий статус
    tmp = findIoTItem("Current_Status");
    if (tmp)
    {
      tmp->setValue(String(readArr[9]), false);
      if (_debug)
      {
        SerialPrint("i", F("ModbusMaster"), "Current_Status = " + String(readArr[9]));
      }
    }
    // 0x200E           Выходной сигнал
    tmp = findIoTItem("Signal_Output");
    if (tmp)
    {
      tmp->setValue(String(readArr[10]), false);
      if (_debug)
      {
        SerialPrint("i", F("ModbusMaster"), "Signal_Output = " + String(readArr[10]));
      }
    }










    // Запись:
    // 0x3002           Режим управления регенерацией (для клапана измерительного типа)
    tmp = findIoTItem("control_mode");
    if (tmp)
    {


   String Reg_control = tmp->getValue();

if(Reg_control.toInt() != readArr[11])
{

if(Reg_control.toInt() == 1)
{
    readArr[11] = 1;
  // тут как-то записываем в Modbus
  mb->writeHreg(_addr, Regeneration_control_mode, readArr + 11, 1);
  wait();

  tmp->setValue(String(readArr[11]), false);  

}
if(Reg_control.toInt() == 0)
{

  readArr[11] = 0;
  // тут как-то записываем в Modbus
  mb->writeHreg(_addr, Regeneration_control_mode, readArr + 11, 1);
  wait();
  

  tmp->setValue(String(readArr[11]), false);  

}


      if (_debug)
      {
        SerialPrint("i", F("ModbusMaster"), "Regeneration_control_mode = " + String(readArr[11]));
      }  

    }


    }




    // 0x3018           Рабочее положение переключателя (Сила регенерации)
    tmp = findIoTItem("Switch_working");
    if (tmp)
    {

      String Reg_control = tmp->getValue();


if(Reg_control.toInt() != readArr[12])
{

if(Reg_control.toInt() == 1)
{
    readArr[12] = 1;
  // тут как-то записываем в Modbus
  mb->writeHreg(_addr, Switch_working_position, readArr + 12, 1);
  wait();

  tmp->setValue(String(readArr[12]), false);  

}
if(Reg_control.toInt() == 0)
{
  readArr[12] = 0;
  // тут как-то записываем в Modbus
  mb->writeHreg(_addr, Switch_working_position, readArr + 12, 1);
  wait();
  
  tmp->setValue(String(readArr[12]), false);  

}


      if (_debug)
      {
        SerialPrint("i", F("ModbusMaster"), "Switch_working_position = " + String(readArr[12]));
      }
    }

      
    }
  }

  void wait()
  {
    while (mb->slave())
    { // Check if transaction is active
      mb->task();
      delay(10);
    }
  }

  ~ModbusEmelianov()
  {
    if (_modbusUART)
      delete mb;
      delete _modbusUART;
  };
};

void *getAPI_ModbusEmelianov(String subtype, String param)
{
  if (subtype == F("ModbusEm"))
  {
    return new ModbusEmelianov(param);
  }
  else
  {
    return nullptr;
  }
}
