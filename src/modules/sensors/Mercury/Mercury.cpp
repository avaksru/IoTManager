#include "Global.h"
#include "classes/IoTItem.h"
#include "protocol.h"

// Прототипы функций
unsigned int calculateCRC(unsigned char *buf, int len);
void testChannel();
void openChannel(bool open);
uint8_t readAddr();
long readSerialNum();
void sendCmd(uint8_t *cmd, int len, uint8_t *response);

// Данные Modbus по умолчанию
int8_t MODBUS_DIR_PIN = 0;
Stream *_modbusUART = nullptr;
#define MODBUS_UART_LINE 1
#define MODBUS_RX_PIN 16        // Rx pin
#define MODBUS_TX_PIN 17        // Tx pin
#define MODBUS_SERIAL_BAUD 9600 // Baud rate for esp32 and max485 communication
int Address = 0;
// Это файл сенсора, в нем осуществляется чтение сенсора.
// Название должно быть уникальным, коротким и отражать суть сенсора.

// ребенок       -       родитель
class Mercury : public IoTItem
{
private:
    //=======================================================================================================
    // Секция переменных.
    // Это секция где Вы можете объявлять переменные и объекты arduino библиотек, что бы
    // впоследствии использовать их в loop и setup

    int8_t PORT_RX_PIN = MODBUS_RX_PIN; // адреса прочитаем с веба
    int8_t PORT_TX_PIN = MODBUS_TX_PIN;
    int _baud = MODBUS_SERIAL_BAUD;
    String _prot = "SERIAL_8N1";
    int protocol = SERIAL_8N1;

    //    String _regStr = ""; // Адрес регистра который будем дергать ( по коду от 0х0000 до 0х????)
    //    uint16_t _reg = 0;
    bool _debug; // Дебаг
                 //    uint32_t _token = 0; // Токен у главного класса весгда 0

    //    const int PORT_RX_PIN = GPIO_NUM_16; // port TX
    //    const int PORT_TX_PIN = GPIO_NUM_17; // port RX
    //    uint8_t Address = 0;
    long SerialNum = 0;

    enum
    {
        none,
        read_address,
        read_serial
    };

public:
    //=======================================================================================================
    // setup()
    // это аналог setup из arduino. Здесь вы можете выполнять методы инициализации сенсора.
    // Такие как ...begin и подставлять в них параметры полученные из web интерфейса.
    // Все параметры хранятся в перемененной parameters, вы можете прочитать любой параметр используя jsonRead функции:
    // jsonReadStr, jsonReadBool, jsonReadInt
    Mercury(String parameters) : IoTItem(parameters)
    {
        PORT_RX_PIN = (int8_t)jsonReadInt(parameters, "RX"); // прочитаем с веба
        PORT_TX_PIN = (int8_t)jsonReadInt(parameters, "TX");
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
        Serial.begin(115200);
        Serial.println("Start Mercury v 1.0");

        // CAN-RS485
        // Serial2.begin(_baud, protocol, PORT_RX_PIN, PORT_TX_PIN);
        pinMode(MODBUS_DIR_PIN, OUTPUT);
        digitalWrite(MODBUS_DIR_PIN, LOW);

        _modbusUART = new HardwareSerial(MODBUS_UART_LINE);

        if (_debug)
        {
            SerialPrint("I", "ModbusMaster", "baud: " + String(_baud) + ", protocol: " + String(protocol, HEX) + ", RX: " + String(PORT_RX_PIN) + ", TX: " + String(PORT_TX_PIN));
        }
        ((HardwareSerial *)_modbusUART)->begin(_baud, protocol, PORT_RX_PIN, PORT_TX_PIN); // выбираем тип протокола, скорость и все пины с веба
        ((HardwareSerial *)_modbusUART)->setTimeout(200);

        Serial.print("testChannel = ");
        testChannel();
        Serial.print("openChannel = ");
        openChannel(true);
        Serial.print("Address = ");
        Address = readAddr();
        Serial.println(Address);
        Serial.print("SerialNum = ");
        SerialNum = readSerialNum();
        Serial.println(SerialNum);
    }

    //=======================================================================================================

    void doByInterval()
    {
        // doByInterval()
        // это аналог loop из arduino, но вызываемый каждые int секунд, заданные в настройках. Здесь вы должны выполнить чтение вашего сенсора
        // а затем выполнить regEvent - это регистрация произошедшего события чтения
        // здесь так же доступны все переменные из секции переменных, и полученные в setup
        // если у сенсора несколько величин то делайте несколько regEvent
        // не используйте delay - помните, что данный loop общий для всех модулей. Если у вас планируется длительная операция, постарайтесь разбить ее на порции
        // и выполнить за несколько тактов
    }

    //=======================================================================================================
    // loop()
    // полный аналог loop() из arduino. Нужно помнить, что все модули имеют равный поочередный доступ к центральному loop(), поэтому, необходимо следить
    // за задержками в алгоритме и не создавать пауз. Кроме того, данная версия перегружает родительскую, поэтому doByInterval() отключается, если
    // не повторить механизм расчета интервалов.
    void loop()
    {
        static String inputString = ""; // Буфер для хранения входящих данных

        while (_modbusUART->available())
        {
            char incomingChar = _modbusUART->read();
            Serial.println(incomingChar);
            // Накапливаем символы до обнаружения символа новой строки
            if (incomingChar == '\n')
            {
                inputString.trim(); // Удаляет пробельные символы (включая '\r') с обеих сторон строки
                Serial.println(inputString);
                inputString = ""; // Очищаем буфер после обработки команды
            }
            else
            {
                inputString += incomingChar;
            }
        }

        IoTItem::loop();
    }

    ~Mercury() {};
};

unsigned int calculateCRC(unsigned char *buf, int len)
{
    unsigned int temp, temp2, flag;
    temp = 0xFFFF;
    for (unsigned char i = 0; i < len; i++)
    {
        temp = temp ^ buf[i];
        for (unsigned char j = 1; j <= 8; j++)
        {
            flag = temp & 0x0001;
            temp >>= 1;
            if (flag)
                temp ^= 0xA001;
        }
    }
    // Reverse byte order.
    temp2 = temp >> 8;
    temp = (temp << 8) | temp2;
    temp &= 0xFFFF;
    // the returned value is already swapped
    // crcLo byte is first & crcHi byte is last
    return temp;
}

//*********************************
void testChannel()
{
    ChannelTest cmd;
    cmd.address = Address;
    cmd.code = 0x00;

    uint8_t response[4];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    // if(crc?????????????????????????
    // Пример:
    // Проверить канал связи со счётчиком с сетевым адресом 80h.
    // Запрос: 80 00 (CRC)
    // Ответ: 80 00 (CRC) Тестирование канала связи прошло успешно.
}

void openChannel(bool open)
{
    ChannelOpen cmd;

    cmd.address = Address;
    cmd.code = open ? 0x01 : 0x00;
    cmd.accessLevel = 0x01;
    cmd.password[0] = 0x01;
    cmd.password[1] = 0x01;
    cmd.password[2] = 0x01;
    cmd.password[3] = 0x01;
    cmd.password[4] = 0x01;
    cmd.password[5] = 0x01;

    uint8_t response[4];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    // crc?????????????????????????
}

uint8_t readAddr()
{
    ReadParam cmd;
    cmd.address = Address;
    cmd.code = 0x08;
    cmd.parameter = 0x05;

    uint8_t response[5];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    // if(crc?????????????????????????

    // 2 двоичных байта (первый=0)
    // response[1];
    return response[2];
}

long readSerialNum()
{

    ReadParam cmd;
    cmd.address = Address;
    cmd.code = 0x08;
    cmd.parameter = 0x00;

    uint8_t response[10];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    // 4 байта серийного номера и три байта кода
    // даты выпуска в последовательности: число,
    // месяц, год (без открытия канала связи)

    //if(sizeof(response) < 6) return -1;\

    //?????????????????????????????????????/
    long r = 0;
    r |= (long)response[2] << 24;
    r |= (long)response[1] << 16;
    r |= (long)response[4] << 8;
    r |= (long)response[3];
    return r;
}

//****************************

void sendCmd(uint8_t *cmd, int len, uint8_t *response)
{

    for (int i = 0; i < len; i++)
    {
        _modbusUART->write(cmd[i]);
        Serial.print(cmd[i]);
        Serial.print(" ");
    }

    uint16_t crc16 = calculateCRC(cmd, len);
    _modbusUART->write(crc16 >> 8);
    _modbusUART->write(crc16 & 0xFF);
    Serial.print(crc16 >> 8);
    Serial.print(" ");
    Serial.print(crc16 & 0xFF);
    Serial.println("");

    delay(200);

    uint8_t i = 0;
    while (_modbusUART->available())
    {
        uint8_t byteReceived = _modbusUART->read();
        response[i++] = byteReceived;
        Serial.print(byteReceived); // Show on Serial Monitor
        Serial.print(" ");
    }

    // Serial.println();        // Show on Serial Monitor
}

// void scanDeviceIds() {
//   for(int i=0; i<=255; i++) {
//    byte cmd[] = {i, 0};
//    send(cmd, sizeof(cmd));
//    delay(1000);
//  }
//}

// если сенсор предполагает использование общего объекта библиотеки для нескольких экземпляров сенсора, то в данной функции необходимо предусмотреть
// создание и контроль соответствующих глобальных переменных
void *getAPI_Mercury(String subtype, String param)
{
    if (subtype == F("Mercury"))
    {
        return new Mercury(param);
    }
    else
    {
        return nullptr;
    }
}
