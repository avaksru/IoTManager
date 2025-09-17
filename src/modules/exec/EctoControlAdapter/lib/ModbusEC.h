
#ifndef ModbusEC_h
#define ModbusEC_h

#define __MODBUSMASTER_DEBUG__ (0)
#define __MODBUSMASTER_DEBUG_PIN_A__ 4
#define __MODBUSMASTER_DEBUG_PIN_B__ 5

#include "Arduino.h"
#include "util/crc16.h"
#include "util/word.h"

/* _____CLASS DEFINITIONS____________________________________________________ */
/**
Arduino class library for communicating with Modbus slaves over
RS232/485 (via RTU protocol).
*/
class ModbusMaster
{
public:
  ModbusMaster();

  void begin(uint8_t, Stream &serial);
  void idle(void (*)());
  void preTransmission(void (*)());
  void postTransmission(void (*)());

  static const uint8_t ku8MBIllegalFunction = 0x01;
  static const uint8_t ku8MBIllegalDataAddress = 0x02;
  static const uint8_t ku8MBIllegalDataValue = 0x03;
  static const uint8_t ku8MBSlaveDeviceFailure = 0x04;
  static const uint8_t ku8MBSuccess = 0x00;
  static const uint8_t ku8MBInvalidSlaveID = 0xE0;
  static const uint8_t ku8MBInvalidFunction = 0xE1;
  static const uint8_t ku8MBResponseTimedOut = 0xE2;
  static const uint8_t ku8MBInvalidCRC = 0xE3;

  uint16_t getResponseBuffer(uint8_t);
  void clearResponseBuffer();
  uint8_t setTransmitBuffer(uint8_t, uint16_t);
  void clearTransmitBuffer();

  void beginTransmission(uint16_t);
  uint8_t requestFrom(uint16_t, uint16_t);
  void sendBit(bool);
  void send(uint8_t);
  void send(uint16_t);
  void send(uint32_t);
  uint8_t available(void);
  uint16_t receive(void);

  uint8_t readHoldingRegisters(uint16_t, uint16_t);
  uint8_t writeMultipleRegisters(uint16_t, uint16_t);
  uint8_t writeMultipleRegisters();

  uint8_t readAddresEctoControl();
  uint8_t writeAddresEctoControl(uint8_t);

private:
  Stream *_serial;                               ///< reference to serial port object
  uint8_t _u8MBSlave;                            ///< Modbus slave (1..255) initialized in begin()
  static const uint8_t ku8MaxBufferSize = 20;    ///< size of response/transmit buffers
  uint16_t _u16ReadAddress;                      ///< slave register from which to read
  uint16_t _u16ReadQty;                          ///< quantity of words to read
  uint16_t _u16ResponseBuffer[ku8MaxBufferSize]; ///< buffer to store Modbus slave response; read via GetResponseBuffer()
  uint16_t _u16WriteAddress;                     ///< slave register to which to write
  uint16_t _u16WriteQty;                         ///< quantity of words to write
  uint16_t _u16TransmitBuffer[ku8MaxBufferSize]; ///< buffer containing data to transmit to Modbus slave; set via SetTransmitBuffer()
  uint16_t *txBuffer;                            // from Wire.h -- need to clean this up Rx
  uint8_t _u8TransmitBufferIndex;
  uint16_t u16TransmitBufferLength;
  uint16_t *rxBuffer; // from Wire.h -- need to clean this up Rx
  uint8_t _u8ResponseBufferIndex;
  uint8_t _u8ResponseBufferLength;

  // Modbus function codes for 16 bit access
  static const uint8_t ku8MBReadHoldingRegisters = 0x03;   ///< Modbus function 0x03 Read Holding Registers
  static const uint8_t ku8MBWriteMultipleRegisters = 0x10; ///< Modbus function 0x10 Write Multiple Registers

  static const uint8_t ku8MBProgRead46 = 0x46;  ///< EctoControl function 0x46 Устройство возвращает в ответе свой текущий адрес ADDR
  static const uint8_t ku8MBProgWrite47 = 0x47; ///< EctoControl function 0x47 высылается ведущим устройством ведомому с указанием сменить свой имеющийся адрес на заданный
                                                // высылается ведущим устройством единственному устройству на шине с неизвестным адресом

  // Modbus timeout [milliseconds]
  static const uint16_t ku16MBResponseTimeout = 2000; ///< Modbus timeout [milliseconds]

  // master function that conducts Modbus transactions
  uint8_t ModbusMasterTransaction(uint8_t u8MBFunction);

  // idle callback function; gets called during idle time between TX and RX
  void (*_idle)();
  // preTransmission callback function; gets called before writing a Modbus message
  void (*_preTransmission)();
  // postTransmission callback function; gets called after a Modbus message has been sent
  void (*_postTransmission)();
};
#endif
