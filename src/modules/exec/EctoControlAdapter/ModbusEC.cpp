#include "ModbusEC.h"

#define COUNT_BIT_AVAIL 5
#define COUNT_BIT_AVAIL_46F 4

ModbusMaster::ModbusMaster(void)
{
  _idle = 0;
  _preTransmission = 0;
  _postTransmission = 0;
}

void ModbusMaster::begin(uint8_t slave, Stream *serial)
{
  _u8MBSlave = slave;
  _serial = serial;
  _u8TransmitBufferIndex = 0;
  u16TransmitBufferLength = 0;

#if __MODBUSMASTER_DEBUG__
  pinMode(__MODBUSMASTER_DEBUG_PIN_A__, OUTPUT);
  pinMode(__MODBUSMASTER_DEBUG_PIN_B__, OUTPUT);
#endif
}

void ModbusMaster::beginTransmission(uint16_t u16Address)
{
  _u16WriteAddress = u16Address;
  _u8TransmitBufferIndex = 0;
  u16TransmitBufferLength = 0;
}

uint8_t ModbusMaster::requestFrom(uint16_t address, uint16_t quantity)
{
  uint8_t read = 0; // Инициализировано значение по умолчанию
  if (quantity > ku8MaxBufferSize)
  {
    quantity = ku8MaxBufferSize;
  }
  _u8ResponseBufferIndex = 0;
  _u8ResponseBufferLength = read;

  return read;
}

void ModbusMaster::sendBit(bool data)
{
  uint8_t txBitIndex = u16TransmitBufferLength % 16;
  if ((u16TransmitBufferLength >> 4) < ku8MaxBufferSize)
  {
    if (0 == txBitIndex)
    {
      _u16TransmitBuffer[_u8TransmitBufferIndex] = 0;
    }
    bitWrite(_u16TransmitBuffer[_u8TransmitBufferIndex], txBitIndex, data);
    u16TransmitBufferLength++;
    _u8TransmitBufferIndex = u16TransmitBufferLength >> 4;
  }
}

void ModbusMaster::send(uint16_t data)
{
  if (_u8TransmitBufferIndex < ku8MaxBufferSize)
  {
    _u16TransmitBuffer[_u8TransmitBufferIndex++] = data;
    u16TransmitBufferLength = _u8TransmitBufferIndex << 4;
  }
}

void ModbusMaster::send(uint32_t data)
{
  send(lowWord(data));
  send(highWord(data));
}

void ModbusMaster::send(uint8_t data)
{
  send(word(data));
}

uint8_t ModbusMaster::available(void)
{
  return _u8ResponseBufferLength - _u8ResponseBufferIndex;
}

uint16_t ModbusMaster::receive(void)
{
  if (_u8ResponseBufferIndex < _u8ResponseBufferLength)
  {
    return _u16ResponseBuffer[_u8ResponseBufferIndex++];
  }
  else
  {
    return 0xFFFF;
  }
}

void ModbusMaster::idle(void (*idle)())
{
  _idle = idle;
}

void ModbusMaster::preTransmission(void (*preTransmission)())
{
  _preTransmission = preTransmission;
}

void ModbusMaster::postTransmission(void (*postTransmission)())
{
  _postTransmission = postTransmission;
}

uint16_t ModbusMaster::getResponseBuffer(uint8_t u8Index)
{
  if (u8Index < ku8MaxBufferSize)
  {
    return _u16ResponseBuffer[u8Index];
  }
  else
  {
    return 0xFFFF;
  }
}

void ModbusMaster::clearResponseBuffer()
{
  uint8_t i;
  for (i = 0; i < ku8MaxBufferSize; i++)
  {
    _u16ResponseBuffer[i] = 0;
  }
}

uint8_t ModbusMaster::setTransmitBuffer(uint8_t u8Index, uint16_t u16Value)
{
  if (u8Index < ku8MaxBufferSize)
  {
    _u16TransmitBuffer[u8Index] = u16Value;
    return ku8MBSuccess;
  }
  else
  {
    return ku8MBIllegalDataAddress;
  }
}

void ModbusMaster::clearTransmitBuffer()
{
  uint8_t i;
  for (i = 0; i < ku8MaxBufferSize; i++)
  {
    _u16TransmitBuffer[i] = 0;
  }
}

uint8_t ModbusMaster::readHoldingRegisters(uint16_t u16ReadAddress,
                                           uint16_t u16ReadQty)
{
  _u16ReadAddress = u16ReadAddress;
  _u16ReadQty = u16ReadQty;
  return ModbusMasterTransaction(ku8MBReadHoldingRegisters);
}

uint8_t ModbusMaster::writeSingleRegister(uint16_t u16WriteAddress,
                                          uint16_t u16WriteValue)
{
  _u16WriteAddress = u16WriteAddress;
  _u16WriteQty = 0;
  _u16TransmitBuffer[0] = u16WriteValue;
  return ModbusMasterTransaction(ku8MBWriteSingleRegister);
}

uint8_t ModbusMaster::writeMultipleRegisters(uint16_t u16WriteAddress,
                                             uint16_t u16WriteQty)
{
  _u16WriteAddress = u16WriteAddress;
  _u16WriteQty = u16WriteQty;
  return ModbusMasterTransaction(ku8MBWriteMultipleRegisters);
}

uint8_t ModbusMaster::writeMultipleRegisters()
{
  _u16WriteQty = _u8TransmitBufferIndex;
  return ModbusMasterTransaction(ku8MBWriteMultipleRegisters);
}

uint8_t ModbusMaster::readAddresEctoControl()
{
  _u16ReadAddress = 0x00;
  _u16ReadQty = 1;
  ModbusMasterTransaction(ku8MBProgRead46);
  return getResponseBuffer(0x00);
}

uint8_t ModbusMaster::writeAddresEctoControl(uint8_t addr)
{
  _u16WriteAddress = 0x00;
  _u16WriteQty = 1;
  _u16TransmitBuffer[0] = (uint16_t)addr;
  return ModbusMasterTransaction(ku8MBProgWrite47);
}

uint8_t ModbusMaster::ModbusMasterTransaction(uint8_t u8MBFunction)
{
  uint8_t u8ModbusADU[24];
  uint8_t u8ModbusADUSize = 0;
  uint8_t i, u8Qty;
  uint16_t u16CRC;
  uint32_t u32StartTime;
  uint8_t u8BytesLeft = 8;
  uint8_t u8MBStatus = ku8MBSuccess;

  // Assemble Modbus Request Application Data Unit
  if (u8MBFunction == ku8MBProgRead46 || u8MBFunction == ku8MBProgWrite47)
  {
    u8ModbusADU[u8ModbusADUSize++] = 0x00;
    u8ModbusADU[u8ModbusADUSize++] = u8MBFunction;
  }
  else
  {
    u8ModbusADU[u8ModbusADUSize++] = _u8MBSlave;
    u8ModbusADU[u8ModbusADUSize++] = u8MBFunction;
  }

  switch (u8MBFunction)
  {
  case ku8MBReadHoldingRegisters:
    u8ModbusADU[u8ModbusADUSize++] = highByte(_u16ReadAddress);
    u8ModbusADU[u8ModbusADUSize++] = lowByte(_u16ReadAddress);
    u8ModbusADU[u8ModbusADUSize++] = highByte(_u16ReadQty);
    u8ModbusADU[u8ModbusADUSize++] = lowByte(_u16ReadQty);
    break;
  }

  switch (u8MBFunction)
  {
  case ku8MBWriteMultipleRegisters:
    u8ModbusADU[u8ModbusADUSize++] = highByte(_u16WriteAddress);
    u8ModbusADU[u8ModbusADUSize++] = lowByte(_u16WriteAddress);
    break;
  }

  switch (u8MBFunction)
  {
  case ku8MBWriteMultipleRegisters:
    u8ModbusADU[u8ModbusADUSize++] = highByte(_u16WriteQty);
    u8ModbusADU[u8ModbusADUSize++] = lowByte(_u16WriteQty);
    u8ModbusADU[u8ModbusADUSize++] = lowByte(_u16WriteQty << 1);

    // Исправлено: используем _u16WriteQty вместо lowByte()
    for (i = 0; i < _u16WriteQty; i++)
    {
      u8ModbusADU[u8ModbusADUSize++] = highByte(_u16TransmitBuffer[i]);
      u8ModbusADU[u8ModbusADUSize++] = lowByte(_u16TransmitBuffer[i]);
    }
    break;
  }

  // Append CRC
  u16CRC = 0xFFFF;
  for (i = 0; i < u8ModbusADUSize; i++)
  {
    u16CRC = crc16_update(u16CRC, u8ModbusADU[i]);
  }
  u8ModbusADU[u8ModbusADUSize++] = lowByte(u16CRC);
  u8ModbusADU[u8ModbusADUSize++] = highByte(u16CRC);
  u8ModbusADU[u8ModbusADUSize] = 0;

  // Flush receive buffer before transmitting request
  while (_serial->read() != -1);

  // Transmit request
  if (_preTransmission) _preTransmission();
  for (i = 0; i < u8ModbusADUSize; i++)
  {
    _serial->write(u8ModbusADU[i]);
  }

  u8ModbusADUSize = 0;
  _serial->flush();
  if (_postTransmission) _postTransmission();

  // Loop until timeout or response complete
  u32StartTime = millis();
  while (u8BytesLeft && !u8MBStatus)
  {
    if (_serial->available())
    {
#if __MODBUSMASTER_DEBUG__
      digitalWrite(__MODBUSMASTER_DEBUG_PIN_A__, true);
#endif
      uint8_t req = _serial->read();
      u8ModbusADU[u8ModbusADUSize++] = req;
      u8BytesLeft--;
#if __MODBUSMASTER_DEBUG__
      digitalWrite(__MODBUSMASTER_DEBUG_PIN_A__, false);
#endif
    }
    else
    {
#if __MODBUSMASTER_DEBUG__
      digitalWrite(__MODBUSMASTER_DEBUG_PIN_B__, true);
#endif
      if (_idle) _idle();
#if __MODBUSMASTER_DEBUG__
      digitalWrite(__MODBUSMASTER_DEBUG_PIN_B__, false);
#endif
    }

    // Evaluate response after enough bytes received
    uint8_t count = (u8MBFunction == ku8MBProgRead46 || u8MBFunction == ku8MBProgWrite47)
                    ? COUNT_BIT_AVAIL_46F : COUNT_BIT_AVAIL;

    if (u8ModbusADUSize == count)
    {
      if (u8MBFunction != ku8MBProgRead46 && u8MBFunction != ku8MBProgWrite47)
      {
        if (u8ModbusADU[0] != _u8MBSlave)
        {
          u8MBStatus = ku8MBInvalidSlaveID;
          break;
        }

        if ((u8ModbusADU[1] & 0x7F) != u8MBFunction)
        {
          u8MBStatus = ku8MBInvalidFunction;
          break;
        }
      }
      
      if (bitRead(u8ModbusADU[1], 7))
      {
        u8MBStatus = u8ModbusADU[2];
        break;
      }

      // Determine remaining bytes based on function
      switch (u8ModbusADU[1])
      {
        case ku8MBReadHoldingRegisters:
          u8BytesLeft = u8ModbusADU[2];
          break;
        case ku8MBWriteMultipleRegisters:
          u8BytesLeft = 3;
          break;
        case ku8MBProgRead46:
        case ku8MBProgWrite47:
          u8BytesLeft = 1;
          break;
        default: ; // Пустой оператор для корректного синтаксиса
      }
    }
    
    if ((millis() - u32StartTime) > ku16MBResponseTimeout)
    {
      u8MBStatus = ku8MBResponseTimedOut;
    }
  }

  // Verify CRC for standard functions
  if (!u8MBStatus && 
      u8MBFunction != ku8MBProgRead46 && 
      u8MBFunction != ku8MBProgWrite47 &&
      u8ModbusADUSize >= COUNT_BIT_AVAIL)
  {
    u16CRC = 0xFFFF;
    for (i = 0; i < (u8ModbusADUSize - 2); i++)
    {
      u16CRC = crc16_update(u16CRC, u8ModbusADU[i]);
    }

    // Исправлено: добавлены скобки для группировки условий
    if (!u8MBStatus && 
        ((lowByte(u16CRC) != u8ModbusADU[u8ModbusADUSize - 2]) ||
         (highByte(u16CRC) != u8ModbusADU[u8ModbusADUSize - 1])))
    {
      u8MBStatus = ku8MBInvalidCRC;
    }
  }

  // Parse response data
  if (!u8MBStatus)
  {
    switch (u8ModbusADU[1])
    {
      case ku8MBReadHoldingRegisters:
        for (i = 0; i < (u8ModbusADU[2] >> 1); i++)
        {
          if (i < ku8MaxBufferSize)
          {
            _u16ResponseBuffer[i] = word(u8ModbusADU[2 * i + 3], u8ModbusADU[2 * i + 4]);
          }
          _u8ResponseBufferLength = i;
        }
        break;
        
      case ku8MBProgRead46:
        _u16ResponseBuffer[0] = (uint16_t)u8ModbusADU[2];
        _u8ResponseBufferLength = 1;
        break;
    }
  }

  // Reset buffers
  _u8TransmitBufferIndex = 0;
  u16TransmitBufferLength = 0;
  _u8ResponseBufferIndex = 0;
  
  return u8MBStatus;
}