

#include <HardwareSerial.h>
#include "mercury.h"

#define TIME_OUT 200 * 1000 // Mercury inter-command delay (mks)

#define MODBUS_UART_LINE 1

const int RS485_RX_PIN = GPIO_NUM_22;  // port RX
const int RS485_TX_PIN = GPIO_NUM_15;  // port TX
const int RS485_DIR_PIN = GPIO_NUM_23; // port dir

uint8_t Address = 0;

#define BAUD 9600 // Baud rate for esp32 and max485 communication

HardwareSerial *_modbusUART = nullptr;

#define LED_BUILTIN 5

void setup()
{

    Serial.begin(115200);
    while (!Serial)
    {
    };

    Serial.println("");
    Serial.println("Start Mercury 230 v 1.02");

    pinMode(LED_BUILTIN, OUTPUT);

    // CAN-RS485

    _modbusUART = new HardwareSerial(MODBUS_UART_LINE);
    _modbusUART->begin(BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN); // выбираем тип протокола, скорость и все пины с веба
    ((HardwareSerial *)_modbusUART)->setTimeout(200);

    pinMode(RS485_DIR_PIN, OUTPUT);
    digitalWrite(RS485_DIR_PIN, LOW);

    delay(2000);

    Serial.println("read");

    testChannel(Address);

    // Инициализация соединения и передача пароля
    openChannel(Address, 0x01, "111111"); // address, level, pass

    if (Address == 0)
        Address = getAddr(0); // Серийный номер

    Serial.print("Сетевой адрес = ");
    Serial.println(Address);
    Serial.println("");

    String numDate = getSerialNumDate(Address);

    int index = numDate.indexOf(' ');
    String num = numDate.substring(0, index);
    String date = numDate.substring(index + 1);

    Serial.print("Серийный номер = ");
    Serial.println(num.toInt());
    Serial.print("Дата изготовления = ");
    Serial.println(date);
    Serial.println("");

    String ver = getVersion(Address);
    Serial.print("Версия ПО = ");
    Serial.println(ver);

    Serial.println("");

    getVariantIsp(Address);

    Serial.println("");

    getCTr(Address); // коэфф трансформации

    P3V outU;
    getU(Address, &outU); // Напряжение U (В) по фазам

    Serial.print("U1 = ");
    Serial.println(outU.p1);
    Serial.print("U2 = ");
    Serial.println(outU.p2);
    Serial.print("U3 = ");
    Serial.println(outU.p3);
    Serial.println("");

    P3V outI;
    getI(Address, &outI); // Сила тока (А) по фазам

    Serial.print("I1 = ");
    Serial.println(outI.p1);
    Serial.print("I2 = ");
    Serial.println(outI.p2);
    Serial.print("I3 = ");
    Serial.println(outI.p3);
    Serial.println("");

    float fr = getFreq(Address); // частота сети
    Serial.print("Freq = ");
    Serial.println(fr);
    Serial.println("");

    String dtime = getDateTime(Address); // дата и время
    Serial.print("Дата и время ");
    Serial.println(dtime);
    Serial.println("");

    //********
    P3V outP;
    getPower(Address, &outP); // Мощность P (Вт) по фазам

    Serial.print("P1 = ");
    Serial.println(outP.p1);
    Serial.print("P2 = ");
    Serial.println(outP.p2);
    Serial.print("P3 = ");
    Serial.println(outP.p3);
    Serial.println("");

    P3V outPQ;
    getPowerQ(Address, &outPQ); // Мощность P (Вт) по фазам

    Serial.print("PQ1 = ");
    Serial.println(outPQ.p1);
    Serial.print("PQ2 = ");
    Serial.println(outPQ.p2);
    Serial.print("PQ3 = ");
    Serial.println(outPQ.p3);
    Serial.println("");

    P3V outPS;
    getPowerS(Address, &outPS); // Мощность P (Вт) по фазам

    Serial.print("PS1 = ");
    Serial.println(outPS.p1);
    Serial.print("PS2 = ");
    Serial.println(outPS.p2);
    Serial.print("PS3 = ");
    Serial.println(outPS.p3);
    Serial.println("");

    P3V outC;
    getC(Address, &outC); // Коэффициент мощности (С) по фазам

    Serial.print("C1 = ");
    Serial.println(outC.p1);
    Serial.print("C2 = ");
    Serial.println(outC.p2);
    Serial.print("C3 = ");
    Serial.println(outC.p3);
    Serial.println("");

    PWV power;
    getEnergyT0(Address, &power, 0, 0, 0x00); // total from reset
    Serial.println("Сумма");
    Serial.println("A+ ");
    Serial.println(power.ap);
    Serial.println("R+ ");
    Serial.println(power.rp);
    Serial.println("");

    P3V pp;
    getEnergyTT0(Address, &pp, 0, 0, 0x00); // total from reset
    Serial.print("A+ (f1) ");
    Serial.println(pp.p1);
    Serial.print("A+ (f2) ");
    Serial.println(pp.p2);
    Serial.print("A+ (f3) ");
    Serial.println(pp.p3);
    Serial.println("");

    getEnergyT0(Address, &power, 0, 0, 0x01); // day from reset
    Serial.println("Тариф 1");
    Serial.print("A+ ");
    Serial.println(power.ap);
    Serial.print("R+ ");
    Serial.println(power.rp);
    Serial.println("");

    getEnergyTT0(Address, &pp, 0, 0, 0x01); // day from reset
    Serial.print("A+ (f1) ");
    Serial.println(pp.p1);
    Serial.print("A+ (f2) ");
    Serial.println(pp.p2);
    Serial.print("A+ (f3) ");
    Serial.println(pp.p3);
    Serial.println("");

    getEnergyT0(Address, &power, 0, 0, 0x02); // night from reset
    Serial.println("Тариф 2");
    Serial.print("A+ ");
    Serial.println(power.ap);
    Serial.print("R+ ");
    Serial.println(power.rp);
    Serial.println("");

    getEnergyTT0(Address, &pp, 0, 0, 0x02); // night from reset
    Serial.print("A+ (f1) ");
    Serial.println(pp.p1);
    Serial.print("A+ (f2) ");
    Serial.println(pp.p2);
    Serial.print("A+ (f3) ");
    Serial.println(pp.p3);
    Serial.println("");

    closeChannel(Address);
    Serial.println("");
    // Write
    //************************

    // Инициализация соединения и передача пароля
    openChannel(Address, 0x02, "222222"); // address, level, pass
    Serial.println("");
    // 10:55:00 среда 05 марта 2008 года, зима.
    // Запрос: 80 03 0C 00 55 10 03 05 03 08 01 (CRC)
    Time tm;
    tm.sec = 0x00;
    tm.min = 0x55;
    tm.hour = 0x10;
    tm.dow = 0x03;
    tm.day = 0x05;
    tm.mon = 0x03;
    tm.year = 0x08;
    tm.tyear = 0x01; // зима(1)/лето(0)

    setDateTime(Address, tm);
    Serial.println("");

    closeChannel(Address);
    Serial.println("");
}

void loop()
{
}

//*********************************
void testChannel(uint8_t address)
{
    Serial.println("testChannel()");

    ChannelCmd cmd;
    cmd.address = address;
    cmd.code = 0x00;

    uint8_t response[4];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);
    Serial.println("");
}

void openChannel(uint8_t address, uint8_t level, const char *pass)
{
    Serial.println("openChannel()");
    ChannelOpen cmd;

    cmd.address = address;
    cmd.code = 0x01;
    cmd.accessLevel = level;

    cmd.password[0] = ascii2hex(pass[0]);
    cmd.password[1] = ascii2hex(pass[1]);
    cmd.password[2] = ascii2hex(pass[2]);
    cmd.password[3] = ascii2hex(pass[3]);
    cmd.password[4] = ascii2hex(pass[4]);
    cmd.password[5] = ascii2hex(pass[5]);

    uint8_t response[4];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);
}

void closeChannel(uint8_t address)
{
    Serial.println("closeChannel()");
    ChannelCmd cmd;
    cmd.address = address;
    cmd.code = 0x02;

    uint8_t response[4];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);
}

uint8_t getAddr(uint8_t address)
{
    Serial.println("readAddr()");
    ReadParam cmd;
    cmd.address = address;
    cmd.code = 0x08;
    cmd.paramNum = 0x05;

    uint8_t response[5];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    return response[2];
}

String getSerialNumDate(uint8_t address)
{
    Serial.println("getSerialNumDate()");

    ReadParam cmd;
    cmd.address = address;
    cmd.code = 0x08;
    cmd.paramNum = 0x00;

    uint8_t response[10];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    // response[0] не используется (address)

    char buffer[2];

    String num_str;
    sprintf(buffer, "%02d", response[1]);
    num_str += buffer;
    sprintf(buffer, "%02d", response[2]);
    num_str += buffer;
    sprintf(buffer, "%02d", response[3]);
    num_str += buffer;
    sprintf(buffer, "%02d", response[4]);
    num_str += buffer;
    num_str += " ";

    sprintf(buffer, "%02d", response[5]);
    String day = buffer;
    sprintf(buffer, "%02d", response[6]);
    String month = buffer;
    sprintf(buffer, "%02d", response[7]);
    String year = buffer;

    num_str += String(day + "." + month + ".20" + year);
    Serial.println("");

    return num_str;
}

String getVersion(uint8_t address) // Чтение версии ПО
{
    Serial.println("getVersion()");

    ReadParam cmd;
    cmd.address = address;
    cmd.code = 0x08;
    cmd.paramNum = 0x03;

    uint8_t response[6];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    String version;

    char buffer[2];

    sprintf(buffer, "%d", response[1]);
    version += buffer;
    version += ".";
    sprintf(buffer, "%d", response[2]);
    version += buffer;
    version += ".";
    sprintf(buffer, "%d", response[3]);
    version += buffer;

    Serial.println("");

    return version;
}

void getVariantIsp(uint8_t address) // Чтение варианта исполнения
{
    Serial.println("getVariantIsp()");

    ReadParam cmd;
    cmd.address = address;
    cmd.code = 0x08;
    cmd.paramNum = 0x12;

    uint8_t response[9];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    int val = binToDec(String(getBit(response[1], 3)) + String(getBit(response[1], 2)));
    Serial.print("Номинальное напряжение  ");
    switch (val)
    {
    case 0:
        Serial.println("57,7 В");
        break;
    case 1:
        Serial.println("230 В");
        break;
    }

    val = binToDec(String(getBit(response[1], 1)) + String(getBit(response[1], 0)));
    Serial.print("Номинальный ток ");
    switch (val)
    {
    case 0:
        Serial.println("5 А");
        break;
    case 1:
        Serial.println("1 А");
        break;
    case 2:
        Serial.println("10 А");
        break;
    }

    val = binToDec(String(getBit(response[1], 7)) + String(getBit(response[1], 6)));
    Serial.print("Класс точности энергии А+ ");
    switch (val)
    {
    case 0:
        Serial.println("0,2 %");
        break;
    case 1:
        Serial.println("0,5 %");
        break;
    case 2:
        Serial.println("1,0 %");
        break;
    case 3:
        Serial.println("2,0 %");
        break;
    }

    val = binToDec(String(getBit(response[1], 5)) + String(getBit(response[1], 4)));
    Serial.print("Класс точности энергии R+ ");
    switch (val)
    {
    case 0:
        Serial.println("0,2 %");
        break;
    case 1:
        Serial.println("0,5 %");
        break;
    case 2:
        Serial.println("1,0 %");
        break;
    case 3:
        Serial.println("2,0 %");
        break;
    }

    Serial.println("");
    Serial.println(String("Число направлений ") + String(getBit(response[2], 7) ? "1" : "2"));
    Serial.println(String("Температурный диапазон ") + String(getBit(response[2], 6) ? "-40°C" : "-20°C"));
    Serial.println(String("Учет профиля средних мощностей ") + String(getBit(response[2], 5) ? "да" : "нет"));
    Serial.println(String("Число фаз ") + String(getBit(response[2], 4) ? "1" : "3"));

    val = binToDec(String(getBit(response[2], 3)) + String(getBit(response[2], 2)) +
                   String(getBit(response[2], 1)) + String(getBit(response[2], 0)));

    Serial.print("Постоянная счетчика ");
    switch (val)
    {
    case 0:
        Serial.println("5000 имп/квт⋅ч");
        break;
    case 1:
        Serial.println("25000 имп/квт⋅ч");
        break;
    case 2:
        Serial.println("1250 имп/квт⋅ч");
        break;
    case 3:
        Serial.println("500 имп/квт⋅ч");
        break;
    case 4:
        Serial.println("1000 имп/квт⋅ч");
        break;
    case 5:
        Serial.println("250 имп/квт⋅ч");
        break;
    }

    Serial.println("");

    Serial.println(String("Суммирование фаз ") + String(getBit(response[3], 7) ? "- по модулю" : "с учетом знака"));
    Serial.println(String("Тарификатор ") + String(getBit(response[3], 6) ? "- внутренний" : "внешний"));

    val = binToDec(String(getBit(response[3], 5)) + String(getBit(response[3], 4)));
    Serial.print("Тип счетчика ");
    switch (val)
    {
    case 0:
        Serial.println("AR");
        break;
    case 1:
        Serial.println("A");
        break;
    }

    String var = String(getBit(response[3], 3)) + String(getBit(response[3], 2)) +
                 String(getBit(response[3], 1)) + String(getBit(response[3], 0));
    Serial.println(String("Вариант исполнения ") + String(binToDec(var)));

    Serial.println("");

    Serial.println(String("Объём энергонезавис. памяти ") + String(getBit(response[4], 7) ? String(int(131 * 8)) : String(int(65.5 * 8))));
    Serial.println(String("Модем PLM ") + String(getBit(response[4], 6) ? "да" : "нет"));
    Serial.println(String("Модем GSM ") + String(getBit(response[4], 5) ? "да" : "нет"));
    Serial.println(String("IRDA порт ") + String(getBit(response[4], 4) ? "да" : "нет"));

    val = binToDec(String(getBit(response[4], 3)) + String(getBit(response[4], 2)));
    Serial.print("Интерфейс ");
    switch (val)
    {
    case 0:
        Serial.println("CAN");
        break;
    case 1:
        Serial.println("RS-485");
        break;
    case 2:
        Serial.println("резерв");
        break;
    case 3:
        Serial.println("нет");
        break;
    }

    Serial.println(String("Внешнее питание ") + String(getBit(response[4], 1) ? "да" : "нет"));
    Serial.println(String("Эл. пломба верхней крышки ") + String(getBit(response[4], 0) ? "да" : "нет"));
    Serial.println("");

    Serial.println(String("Встроенное реле ") + String(getBit(response[5], 7) ? "да" : "нет"));
    Serial.println(String("Подсветка ЖКИ ") + String(getBit(response[5], 6) ? "да" : "нет"));
    Serial.println(String("Потарифный учёт максимумов мощности ") + String(getBit(response[5], 5) ? "да" : "нет"));
    Serial.println(String("Электронная пломба ") + String(getBit(response[5], 4) ? "да" : "нет"));
    Serial.println(String("Встроенное питание интерфейса 1 ") + String(getBit(response[5], 3) ? "да" : "нет"));
    Serial.println(String("Интерфейс 2 ") + String(getBit(response[5], 2) ? "да" : "нет"));
    Serial.println(String("Контроль ПКЭ ") + String(getBit(response[5], 1) ? "да" : "нет"));
    Serial.println(String("Пофазный учёт энергии A+ ") + String(getBit(response[5], 0) ? "да" : "нет"));
}

void getU(uint8_t address, P3V *U) // Get voltage (U) by phases
{
    Serial.println("getU()");

    ReadParamCmd cmd;

    cmd.address = address;
    cmd.code = 0x08;
    cmd.paramNum = 0x16;
    cmd.BWRI = 0x11;

    uint8_t response[12];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    long u1 = 0;
    u1 |= (long)response[1] << 16;
    u1 |= (long)response[3] << 8;
    u1 |= (long)response[2];

    long u2 = 0;
    u2 |= (long)response[4] << 16;
    u2 |= (long)response[6] << 8;
    u2 |= (long)response[5];

    long u3 = 0;
    u3 |= (long)response[7] << 16;
    u3 |= (long)response[9] << 8;
    u3 |= (long)response[8];

    U->p1 = u1 / 100.0;
    U->p2 = u2 / 100.0;
    U->p3 = u3 / 100.0;
}

void getI(uint8_t address, P3V *I) // Get current (I) by phases
{
    Serial.println("getI()");

    ReadParamCmd cmd;

    cmd.address = address;
    cmd.code = 0x08;
    cmd.paramNum = 0x16;
    cmd.BWRI = 0x21;

    uint8_t response[12];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    long I1 = 0;
    I1 |= (long)response[1] << 16;
    I1 |= (long)response[3] << 8;
    I1 |= (long)response[2];

    long I2 = 0;
    I2 |= (long)response[4] << 16;
    I2 |= (long)response[6] << 8;
    I2 |= (long)response[5];

    long I3 = 0;
    I3 |= (long)response[7] << 16;
    I3 |= (long)response[9] << 8;
    I3 |= (long)response[8];

    I->p1 = I1 / 1000.0;
    I->p2 = I2 / 1000.0;
    I->p3 = I3 / 1000.0;
}

void getPower(uint8_t address, P3V *U) // Мощность P (Вт) по фазам
{
    Serial.println("getPower()");

    ReadParamCmd cmd;

    cmd.address = address;
    cmd.code = 0x08;
    cmd.paramNum = 0x16;
    cmd.BWRI = 0x00;

    uint8_t response[15];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    long p1 = 0;
    p1 |= (long)response[1] << 16;
    p1 |= (long)response[3] << 8;
    p1 |= (long)response[2];

    long p2 = 0;
    p2 |= (long)response[4] << 16;
    p2 |= (long)response[6] << 8;
    p2 |= (long)response[5];

    long p3 = 0;
    p3 |= (long)response[7] << 16;
    p3 |= (long)response[9] << 8;
    p3 |= (long)response[8];

    U->p1 = p1 / 100.0;
    U->p2 = p2 / 100.0;
    U->p3 = p3 / 100.0;
}

void getPowerQ(uint8_t address, P3V *Q)
{
    Serial.println("getPowerQ()");

    ReadParamCmd cmd;

    cmd.address = address;
    cmd.code = 0x08;
    cmd.paramNum = 0x16;
    cmd.BWRI = 0x08;

    uint8_t response[15];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    long p1 = 0;
    p1 |= (long)response[1] << 16;
    p1 |= (long)response[3] << 8;
    p1 |= (long)response[2];

    long p2 = 0;
    p2 |= (long)response[4] << 16;
    p2 |= (long)response[6] << 8;
    p2 |= (long)response[5];

    long p3 = 0;
    p3 |= (long)response[7] << 16;
    p3 |= (long)response[9] << 8;
    p3 |= (long)response[8];

    Q->p1 = p1 / 100.0;
    Q->p2 = p2 / 100.0;
    Q->p3 = p3 / 100.0;
}

void getPowerS(uint8_t address, P3V *S)
{
    Serial.println("getPowerS()");

    ReadParamCmd cmd;

    cmd.address = address;
    cmd.code = 0x08;
    cmd.paramNum = 0x16;
    cmd.BWRI = 0x04;

    uint8_t response[15];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    long p1 = 0;
    p1 |= (long)response[1] << 16;
    p1 |= (long)response[3] << 8;
    p1 |= (long)response[2];

    long p2 = 0;
    p2 |= (long)response[4] << 16;
    p2 |= (long)response[6] << 8;
    p2 |= (long)response[5];

    long p3 = 0;
    p3 |= (long)response[7] << 16;
    p3 |= (long)response[9] << 8;
    p3 |= (long)response[8];

    S->p1 = p1 / 100.0;
    S->p2 = p2 / 100.0;
    S->p3 = p3 / 100.0;
}

void getC(uint8_t address, P3V *C) // Коэффициент мощности (С) по фазам
{
    Serial.println("getC()");

    ReadParamCmd cmd;

    cmd.address = address;
    cmd.code = 0x08;
    cmd.paramNum = 0x16;
    cmd.BWRI = 0x30;

    uint8_t response[15];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    long p1 = 0;
    p1 |= (long)response[1] << 16;
    p1 |= (long)response[3] << 8;
    p1 |= (long)response[2];

    long p2 = 0;
    p2 |= (long)response[4] << 16;
    p2 |= (long)response[6] << 8;
    p2 |= (long)response[5];

    long p3 = 0;
    p3 |= (long)response[7] << 16;
    p3 |= (long)response[9] << 8;
    p3 |= (long)response[8];

    C->p1 = p1 / 1000.0;
    C->p2 = p2 / 1000.0;
    C->p3 = p3 / 100.0;
}

float getFreq(uint8_t address) // Частота
{
    Serial.println("getFreq()");

    ReadParamCmd cmd;

    cmd.address = address;
    cmd.code = 0x08;
    cmd.paramNum = 0x16; // 0x11
    cmd.BWRI = 0x40;

    uint8_t response[6];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    long r = 0;
    r |= (long)response[1] << 16;
    r |= (long)response[3] << 8;
    r |= (long)response[2];

    float freq = r / 100.f;

    return freq;
}

void getAngle(uint8_t address, P3V *Ang) // Get phases angle
{
    Serial.println("getAngle()");

    ReadParamCmd cmd;

    cmd.address = address;
    cmd.code = 0x08;
    cmd.paramNum = 0x16; // 0x11 1 phase, 0x16 - 3 phases
    cmd.BWRI = 0x51;

    uint8_t response[15];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    long p1 = 0;
    p1 |= (long)response[1] << 16;
    p1 |= (long)response[3] << 8;
    p1 |= (long)response[2];

    long p2 = 0;
    p2 |= (long)response[4] << 16;
    p2 |= (long)response[6] << 8;
    p2 |= (long)response[5];

    long p3 = 0;
    p3 |= (long)response[7] << 16;
    p3 |= (long)response[9] << 8;
    p3 |= (long)response[8];

    Ang->p1 = p1 / 100.0;
    Ang->p2 = p2 / 100.0;
    Ang->p3 = p3 / 100.0;
}

String getDateTime(uint8_t address)
{

    Serial.println("getDateTime()");

    ReadParam cmd;

    cmd.address = address;
    cmd.code = 0x04;
    cmd.paramNum = 0x00;

    uint8_t response[11];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    // Запрос: 80 04 00 (CRC)
    // Ответ: 80 43 14 16 03 27 02 08 01 (CRC) 16:14:43 среда 27 февраля 2008 года, зима.

    String date;
    char buffer[2];

    sprintf(buffer, "%02X", response[3]);
    date += buffer;
    date += ":";
    sprintf(buffer, "%02X", response[2]);
    date += buffer;
    date += ":";
    sprintf(buffer, "%02X", response[1]);
    date += buffer;
    date += " ";

    sprintf(buffer, "%02X", response[4]); // dow
    date += dow[atoi(buffer) - 1];
    date += " ";

    sprintf(buffer, "%02X", response[5]); // day
    date += buffer;
    date += " ";

    sprintf(buffer, "%02X", response[6]); // month
    date += moy[atoi(buffer) - 1];
    date += " ";

    sprintf(buffer, "%02X", response[7]); // year
    date += "20";
    date += buffer;
    date += " ";

    sprintf(buffer, "%02X", response[8]); // лето - зима
    if (atoi(buffer) == 0)
        date += "лето";
    else if (atoi(buffer) == 1)
        date += "зима";

    return date;
}

//****************************************8

void getEnergyT0(uint8_t address, PWV *W, int periodId, int month, uint8_t number) // общий
{
    Serial.println("getEnergyT0()");

    ReadParamCmd cmd;

    cmd.address = address;
    cmd.code = 0x05;
    cmd.paramNum = 0x00; //(periodId << 4) | (month & 0xF);
    cmd.BWRI = number;   // 0 total, 1 day, 2 night

    uint8_t response[19];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    long val = 0;

    val |= ((response[2] & 0x3F) << 24);
    val |= (response[1] << 16);
    val |= (response[4] << 8);
    val |= response[3];

    W->ap = val / 1000.f;

    val = 0;
    val |= ((response[10] & 0x3F) << 24);
    val |= (response[9] << 16);
    val |= (response[12] << 8);
    val |= response[11];

    W->rp = val / 1000.f;
}

void getEnergyTT0(uint8_t address, P3V *P, int periodId, int month, uint8_t number) // 3 фазы
{
    Serial.println("getEnergyTT0()");

    ReadParamCmd cmd;

    cmd.address = address;
    cmd.code = 0x05;
    cmd.paramNum = 0x60;
    cmd.BWRI = number;

    uint8_t response[15];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    long val = 0;

    val |= ((response[2] & 0x3F) << 24);
    val |= (response[1] << 16);
    val |= (response[4] << 8);
    val |= response[3];

    P->p1 = val / 1000.f;

    val = 0;
    val |= ((response[6] & 0x3F) << 24);
    val |= (response[5] << 16);
    val |= (response[8] << 8);
    val |= response[7];

    P->p2 = val / 1000.f;

    val = 0;
    val |= ((response[10] & 0x3F) << 24);
    val |= (response[9] << 16);
    val |= (response[12] << 8);
    val |= response[11];

    P->p3 = val / 1000.f;
}

void getAllNumbers() // устройства в сети ????????????/
{

    Serial.println("getAllNumbers()");

    ReadParam cmd;

    cmd.address = 0x00;
    cmd.code = 0x08;
    cmd.paramNum = 0x05;

    uint8_t response[5];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);
}

void getCTr(uint8_t address) // коэфф трансформации
{

    Serial.println("getCTr()");

    ReadParam cmd;

    cmd.address = address;
    cmd.code = 0x08;
    cmd.paramNum = 0x02;

    uint8_t response[7];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    int val = 0;

    val |= (response[1] << 8);
    val |= response[2];
    Serial.print("Коэффициент трансформации по напряжению ");
    Serial.println(val);

    val = 0;
    val |= (response[3] << 8);
    val |= response[4];

    Serial.print("Коэффициент трансформации по току ");
    Serial.println(val);
}

// write
//****************************
void setDateTime(uint8_t address, Time tm)
{

    Serial.println("setDateTime()");

    WriteDate cmd;

    cmd.address = address;
    cmd.code = 0x03;
    cmd.paramNum = 0x0C;
    cmd.time = tm;

    // Установить внутреннее время счётчика с сетевым адресом 128 в следующее значение:
    // 10:55:00 среда 05 марта 2008 года, зима.
    // Запрос: 80 03 0C 00 55 10 03 05 03 08 01 (CRC)
    // Ответ: 80 00 (CRC)

    uint8_t response[4];
    sendCmd((uint8_t *)&cmd, sizeof(cmd), response);

    if (response[1] == 0x00)
        Serial.println("time set OK!");
    else
        Serial.println("time set Error!");
}

// write data
void sendCmd(uint8_t *cmd, int len, uint8_t *response)
{
    // RS485Transmit
    Serial.print("TX ");
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(RS485_DIR_PIN, HIGH);

    for (int i = 0; i < len; i++)
    {
        _modbusUART->write(cmd[i]);

        Serial.printf("%02X", cmd[i]);

        Serial.print(" ");
    }

    uint16_t crc16 = calculateCRC(cmd, len);
    uint8_t CRC1 = crc16 >> 8;
    uint8_t CRC2 = crc16 & 0xFF;

    _modbusUART->write(CRC1);
    _modbusUART->write(CRC2);
    _modbusUART->flush();

    Serial.printf("%02X", CRC1);
    Serial.print(" ");
    Serial.printf("%02X", CRC2);
    Serial.println("");

    digitalWrite(RS485_DIR_PIN, LOW);
    digitalWrite(LED_BUILTIN, LOW);
    usleep(TIME_OUT);

    // RS485Receive
    Serial.print("RX ");

    uint8_t size = 0;
    while (_modbusUART->available())
    {
        uint8_t byteReceived = _modbusUART->read();
        response[size++] = byteReceived;

        Serial.printf("%02X", byteReceived);
        Serial.print(" ");
    }

    // проверка контрольной суммы
    uint16_t crc = calculateCRC(response, size - sizeof(uint16_t));
    uint16_t crc_resp = (response[size - 1] | response[size - 2] << 8);
    if (response[0] != cmd[0] || crc != crc_resp) // address || crc
        Serial.println("Error read!!!");

    Serial.println("");
}
