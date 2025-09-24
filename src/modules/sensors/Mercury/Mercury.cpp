#include "Global.h"
#include "classes/IoTItem.h"
#include <map>
#include <HardwareSerial.h>
#include "protocol.h"

#define TIME_OUT 200 // Mercury inter-command delay (ms)

Stream *_modbusUART = nullptr;
int8_t MODBUS_DIR_PIN = 0;
#define MODBUS_UART_LINE 1
#define MODBUS_RX_PIN 16 // Rx pin
#define MODBUS_TX_PIN 17 // Tx pin
#define MODBUS_SERIAL_BAUD 9600

// Forward declaration of MercuryNode class
class MercuryNode;

uint32_t mercuryTokenCount = 0; // Счетчик токенов для узлов
std::map<uint32_t, MercuryNode *> MercuryNodeMap;

class MercuryNode : public IoTItem
{
private:
    uint8_t _addr = 0;     // Адрес устройства (1-247)
    String _command = "";  // Команда для выполнения
    uint32_t _token = 0;   // Токен запроса
    bool _isFloat = false; // Флаг для обработки float-данных
    uint8_t _countReg = 1; // Количество регистров
    uint8_t _paramNum = 0; // Номер параметра
    uint8_t _bwri = 0;     // BWRI для команд
    uint8_t _func = 0;     // Код функции
    String _parameters;    // Хранит JSON-параметры

    String parseVariantIsp(uint8_t *response, int size)
    {
        String result = "";
        int val = binToDec(String(getBit(response[1], 3)) + String(getBit(response[1], 2)));
        result += "Номинальное напряжение: ";
        switch (val)
        {
        case 0:
            result += "57,7 В\n";
            break;
        case 1:
            result += "230 В\n";
            break;
        }
        val = binToDec(String(getBit(response[1], 1)) + String(getBit(response[1], 0)));
        result += "Номинальный ток: ";
        switch (val)
        {
        case 0:
            result += "5 А\n";
            break;
        case 1:
            result += "1 А\n";
            break;
        case 2:
            result += "10 А\n";
            break;
        }
        val = binToDec(String(getBit(response[1], 7)) + String(getBit(response[1], 6)));
        result += "Класс точности энергии А+: ";
        switch (val)
        {
        case 0:
            result += "0,2 %\n";
            break;
        case 1:
            result += "0,5 %\n";
            break;
        case 2:
            result += "1,0 %\n";
            break;
        case 3:
            result += "2,0 %\n";
            break;
        }
        val = binToDec(String(getBit(response[1], 5)) + String(getBit(response[1], 4)));
        result += "Класс точности энергии R+: ";
        switch (val)
        {
        case 0:
            result += "0,2 %\n";
            break;
        case 1:
            result += "0,5 %\n";
            break;
        case 2:
            result += "1,0 %\n";
            break;
        case 3:
            result += "2,0 %\n";
            break;
        }
        result += "Число направлений: " + String(getBit(response[2], 7) ? "1" : "2") + "\n";
        result += "Температурный диапазон: " + String(getBit(response[2], 6) ? "-40°C" : "-20°C") + "\n";
        result += "Учет профиля средних мощностей: " + String(getBit(response[2], 5) ? "да" : "нет") + "\n";
        result += "Число фаз: " + String(getBit(response[2], 4) ? "1" : "3") + "\n";
        val = binToDec(String(getBit(response[2], 3)) + String(getBit(response[2], 2)) +
                       String(getBit(response[2], 1)) + String(getBit(response[2], 0)));
        result += "Постоянная счетчика: ";
        switch (val)
        {
        case 0:
            result += "5000 имп/квт⋅ч\n";
            break;
        case 1:
            result += "25000 имп/квт⋅ч\n";
            break;
        case 2:
            result += "1250 имп/квт⋅ч\n";
            break;
        case 3:
            result += "500 имп/квт⋅ч\n";
            break;
        case 4:
            result += "1000 имп/квт⋅ч\n";
            break;
        case 5:
            result += "250 имп/квт⋅ч\n";
            break;
        }
        result += "Суммирование фаз: " + String(getBit(response[3], 7) ? "- по модулю" : "с учетом знака") + "\n";
        result += "Тарификатор: " + String(getBit(response[3], 6) ? "- внутренний" : "внешний") + "\n";
        val = binToDec(String(getBit(response[3], 5)) + String(getBit(response[3], 4)));
        result += "Тип счетчика: ";
        switch (val)
        {
        case 0:
            result += "AR\n";
            break;
        case 1:
            result += "A\n";
            break;
        }
        String var = String(getBit(response[3], 3)) + String(getBit(response[3], 2)) +
                     String(getBit(response[3], 1)) + String(getBit(response[3], 0));
        result += "Вариант исполнения: " + String(binToDec(var)) + "\n";
        result += "Объём энергонезавис. памяти: " + String(getBit(response[4], 7) ? String(int(131 * 8)) : String(int(65.5 * 8))) + "\n";
        result += "Модем PLM: " + String(getBit(response[4], 6) ? "да" : "нет") + "\n";
        result += "Модем GSM: " + String(getBit(response[4], 5) ? "да" : "нет") + "\n";
        result += "IRDA порт: " + String(getBit(response[4], 4) ? "да" : "нет") + "\n";
        val = binToDec(String(getBit(response[4], 3)) + String(getBit(response[4], 2)));
        result += "Интерфейс: ";
        switch (val)
        {
        case 0:
            result += "CAN\n";
            break;
        case 1:
            result += "RS-485\n";
            break;
        case 2:
            result += "резерв\n";
            break;
        case 3:
            result += "нет\n";
            break;
        }
        result += "Внешнее питание: " + String(getBit(response[4], 1) ? "да" : "нет") + "\n";
        result += "Эл. пломба верхней крышки: " + String(getBit(response[4], 0) ? "да" : "нет") + "\n";
        result += "Встроенное реле: " + String(getBit(response[5], 7) ? "да" : "нет") + "\n";
        result += "Подсветка ЖКИ: " + String(getBit(response[5], 6) ? "да" : "нет") + "\n";
        result += "Потарифный учёт максимумов мощности: " + String(getBit(response[5], 5) ? "да" : "нет") + "\n";
        result += "Электронная пломба: " + String(getBit(response[5], 4) ? "да" : "нет") + "\n";
        result += "Встроенное питание интерфейса 1: " + String(getBit(response[5], 3) ? "да" : "нет") + "\n";
        result += "Интерфейс 2: " + String(getBit(response[5], 2) ? "да" : "нет") + "\n";
        result += "Контроль ПКЭ: " + String(getBit(response[5], 1) ? "да" : "нет") + "\n";
        result += "Пофазный учёт энергии A+: " + String(getBit(response[5], 0) ? "да" : "нет") + "\n";
        return result;
    }

    bool sendRawCmd(uint8_t *cmd, int len, uint8_t *response, int &respSize)
    {
        if (!_modbusUART)
        {
            SerialPrint("E", "MercuryNode", "UART not initialized");
            return false;
        }

        // Send command
        String txMsg = "RX ";
        for (int i = 0; i < len; i++)
        {
            ((HardwareSerial *)_modbusUART)->write(cmd[i]);
            char buf[3];
            sprintf(buf, "%02X", cmd[i]);
            txMsg += String(buf) + " ";
        }
        uint16_t crc16 = calculateCRC(cmd, len);
        uint8_t CRC1 = crc16 >> 8;
        uint8_t CRC2 = crc16 & 0xFF;
        ((HardwareSerial *)_modbusUART)->write(CRC1);
        ((HardwareSerial *)_modbusUART)->write(CRC2);
        char buf[3];
        sprintf(buf, "%02X", CRC1);
        txMsg += String(buf) + " ";
        sprintf(buf, "%02X", CRC2);
        txMsg += String(buf);
        SerialPrint("I", "MercuryNode", txMsg);

        // Wait for response
        delayMicroseconds(TIME_OUT * 1000); // Convert ms to us
        String rxMsg = "TX ";
        respSize = 0;
        while (((HardwareSerial *)_modbusUART)->available() && respSize < 64)
        { // Arbitrary max size
            response[respSize] = ((HardwareSerial *)_modbusUART)->read();
            sprintf(buf, "%02X", response[respSize]);
            rxMsg += String(buf) + " ";
            respSize++;
        }
        SerialPrint("I", "MercuryNode", rxMsg);

        // Verify CRC
        if (respSize >= 2)
        {
            uint16_t crc = calculateCRC(response, respSize - 2);
            uint16_t crc_resp = (response[respSize - 1] | (response[respSize - 2] << 8));
            if (response[0] != cmd[0] || crc != crc_resp)
            {
                SerialPrint("E", "MercuryNode", "Error read: Invalid address or CRC");
                return false;
            }
        }
        else
        {
            SerialPrint("E", "MercuryNode", "Error read: Response too short");
            return false;
        }
        return true;
    }

public:
    MercuryNode(String parameters) : IoTItem(parameters)
    {
        _parameters = parameters; // Store JSON parameters
        _addr = jsonReadInt(parameters, "addr");
        jsonRead(parameters, "command", _command);
        _isFloat = jsonReadBool(parameters, "isFloat");
        _countReg = jsonReadInt(parameters, "count");
        _paramNum = jsonReadInt(parameters, "paramNum");
        _bwri = jsonReadInt(parameters, "bwri");
        _func = jsonReadInt(parameters, "func");
        mercuryTokenCount++;
        _token = mercuryTokenCount;
        MercuryNodeMap[_token] = this;
        SerialPrint("I", "MercuryNode", "Добавлен узел/токен: " + getID() + " - " + String(_token));
    }

    void doByInterval()
    {
        if (!_modbusUART)
        {
            SerialPrint("E", "MercuryNode", "UART is NULL");
            return;
        }

        uint8_t response[64]; // Arbitrary max response size
        int respSize = 0;
        bool success = false;

        if (_command == "testChannel")
        {
            ChannelCmd cmd;
            cmd.address = _addr;
            cmd.code = 0x00;
            success = sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize);
        }
        else if (_command == "openChannel")
        {
            ChannelOpen cmd;
            cmd.address = _addr;
            cmd.code = 0x01;
            cmd.accessLevel = jsonReadInt(_parameters, "accessLevel");
            String pass = jsonReadStr(_parameters, "password");
            for (int i = 0; i < 6 && i < pass.length(); i++)
            {
                cmd.password[i] = ascii2hex(pass[i]);
            }
            success = sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize);
        }
        else if (_command == "closeChannel")
        {
            ChannelCmd cmd;
            cmd.address = _addr;
            cmd.code = 0x02;
            success = sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize);
        }
        else if (_command == "getAddr")
        {
            ReadParam cmd;
            cmd.address = _addr;
            cmd.code = 0x08;
            cmd.paramNum = 0x05;
            success = sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize);
        }
        else if (_command == "getSerialNumDate")
        {
            ReadParam cmd;
            cmd.address = _addr;
            cmd.code = 0x08;
            cmd.paramNum = 0x00;
            success = sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize);
        }
        else if (_command == "getVersion")
        {
            ReadParam cmd;
            cmd.address = _addr;
            cmd.code = 0x08;
            cmd.paramNum = 0x03;
            success = sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize);
        }
        else if (_command == "getVariantIsp")
        {
            ReadParam cmd;
            cmd.address = _addr;
            cmd.code = 0x08;
            cmd.paramNum = 0x12;
            success = sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize);
        }
        else if (_command == "getCTr")
        {
            ReadParam cmd;
            cmd.address = _addr;
            cmd.code = 0x08;
            cmd.paramNum = 0x02;
            success = sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize);
        }
        else if (_command == "getU" || _command == "getI" || _command == "getPower" ||
                 _command == "getPowerQ" || _command == "getPowerS" || _command == "getC" ||
                 _command == "getAngle")
        {
            ReadParamCmd cmd;
            cmd.address = _addr;
            cmd.code = 0x08;
            cmd.paramNum = 0x16;
            cmd.BWRI = _bwri;
            success = sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize);
        }
        else if (_command == "getFreq")
        {
            ReadParamCmd cmd;
            cmd.address = _addr;
            cmd.code = 0x08;
            cmd.paramNum = 0x11;
            cmd.BWRI = _bwri;
            success = sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize);
        }
        else if (_command == "getDateTime")
        {
            ReadParam cmd;
            cmd.address = _addr;
            cmd.code = 0x04;
            cmd.paramNum = 0x00;
            success = sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize);
        }
        else if (_command == "getEnergyT0")
        {
            ReadParamCmd cmd;
            cmd.address = _addr;
            cmd.code = 0x05;
            cmd.paramNum = 0x00;
            cmd.BWRI = _bwri;
            success = sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize);
        }
        else if (_command == "getEnergyTT0")
        {
            ReadParamCmd cmd;
            cmd.address = _addr;
            cmd.code = 0x05;
            cmd.paramNum = 0x60;
            cmd.BWRI = _bwri;
            success = sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize);
        }

        if (success)
        {
            parseResponse(response, respSize);
        }
    }

    void parseResponse(uint8_t *response, int size)
    {
        if (_command == "testChannel")
        {
            SerialPrint("I", "MercuryNode", "testChannel response received");
        }
        else if (_command == "openChannel")
        {
            SerialPrint("I", "MercuryNode", "openChannel response received");
        }
        else if (_command == "closeChannel")
        {
            SerialPrint("I", "MercuryNode", "closeChannel response received");
        }
        else if (_command == "getAddr")
        {
            uint8_t addr = response[2];
            regEvent(addr, "MercuryNode");
            SerialPrint("I", "MercuryNode", "Сетевой адрес = " + String(addr));
        }
        else if (_command == "getSerialNumDate")
        {
            char buffer[3];
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
            num_str += day + "." + month + ".20" + year;
            regEvent(num_str, "MercuryNode");
            SerialPrint("I", "MercuryNode", "Серийный номер = " + String(num_str.substring(0, num_str.indexOf(' ')).toInt()));
            SerialPrint("I", "MercuryNode", "Дата изготовления = " + num_str.substring(num_str.indexOf(' ') + 1));
        }
        else if (_command == "getVersion")
        {
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
            regEvent(version, "MercuryNode");
            SerialPrint("I", "MercuryNode", "Версия ПО = " + version);
        }
        else if (_command == "getVariantIsp")
        {
            String result = parseVariantIsp(response, size);
            regEvent(result, "MercuryNode");
            SerialPrint("I", "MercuryNode", result);
        }
        else if (_command == "getCTr")
        {
            int val = (response[1] << 8) | response[2];
            String result = "Коэффициент трансформации по напряжению: " + String(val);
            val = (response[3] << 8) | response[4];
            result += "\nКоэффициент трансформации по току: " + String(val);
            regEvent(result, "MercuryNode");
            SerialPrint("I", "MercuryNode", result);
        }
        else if (_command == "getU" || _command == "getI" || _command == "getPower" ||
                 _command == "getPowerQ" || _command == "getPowerS" || _command == "getC" ||
                 _command == "getAngle")
        {
            P3V out;
            long p1 = (response[1] << 16) | (response[3] << 8) | response[2];
            long p2 = (response[4] << 16) | (response[6] << 8) | response[5];
            long p3 = (response[7] << 16) | (response[9] << 8) | response[8];
            out.p1 = (_command == "getI" || _command == "getC") ? p1 / 1000.0 : p1 / 100.0;
            out.p2 = (_command == "getI" || _command == "getC") ? p2 / 1000.0 : p2 / 100.0;
            out.p3 = (_command == "getI" || _command == "getC") ? p3 / 1000.0 : p3 / 100.0;
            String result = String(_command == "getU" ? "U1" : _command == "getI"    ? "I1"
                                                           : _command == "getPower"  ? "P1"
                                                           : _command == "getPowerQ" ? "PQ1"
                                                           : _command == "getPowerS" ? "PS1"
                                                           : _command == "getC"      ? "C1"
                                                                                     : "Ang1") +
                            " = " + String(out.p1) + "\n" +
                            String(_command == "getU" ? "U2" : _command == "getI"    ? "I2"
                                                           : _command == "getPower"  ? "P2"
                                                           : _command == "getPowerQ" ? "PQ2"
                                                           : _command == "getPowerS" ? "PS2"
                                                           : _command == "getC"      ? "C2"
                                                                                     : "Ang2") +
                            " = " + String(out.p2) + "\n" +
                            String(_command == "getU" ? "U3" : _command == "getI"    ? "I3"
                                                           : _command == "getPower"  ? "P3"
                                                           : _command == "getPowerQ" ? "PQ3"
                                                           : _command == "getPowerS" ? "PS3"
                                                           : _command == "getC"      ? "C3"
                                                                                     : "Ang3") +
                            " = " + String(out.p3);
            regEvent(result, "MercuryNode");
            SerialPrint("I", "MercuryNode", result);
        }
        else if (_command == "getFreq")
        {
            long r = (response[1] << 16) | (response[3] << 8) | response[2];
            float freq = r / 100.0;
            regEvent(freq, "MercuryNode");
            SerialPrint("I", "MercuryNode", "Freq = " + String(freq));
        }
        else if (_command == "getDateTime")
        {
            char buffer[3];
            String date;
            sprintf(buffer, "%02X", response[3]);
            date += buffer;
            date += ":";
            sprintf(buffer, "%02X", response[2]);
            date += buffer;
            date += ":";
            sprintf(buffer, "%02X", response[1]);
            date += buffer;
            date += " ";
            sprintf(buffer, "%02X", response[4]);
            date += dow[atoi(buffer) - 1];
            date += " ";
            sprintf(buffer, "%02X", response[5]);
            date += buffer;
            date += " ";
            sprintf(buffer, "%02X", response[6]);
            date += moy[atoi(buffer) - 1];
            date += " ";
            sprintf(buffer, "%02X", response[7]);
            date += "20" + String(buffer);
            date += " ";
            sprintf(buffer, "%02X", response[8]);
            date += (atoi(buffer) == 0 ? "лето" : "зима");
            regEvent(date, "MercuryNode");
            SerialPrint("I", "MercuryNode", "Дата и время: " + date);
        }
        else if (_command == "getEnergyT0")
        {
            PWV power;
            long val = ((response[2] & 0x3F) << 24) | (response[1] << 16) | (response[4] << 8) | response[3];
            power.ap = val / 1000.0;
            val = ((response[10] & 0x3F) << 24) | (response[9] << 16) | (response[12] << 8) | response[11];
            power.rp = val / 1000.0;
            String result = "A+ = " + String(power.ap) + "\nR+ = " + String(power.rp);
            regEvent(result, "MercuryNode");
            SerialPrint("I", "MercuryNode", result);
        }
        else if (_command == "getEnergyTT0")
        {
            P3V pp;
            long val = ((response[2] & 0x3F) << 24) | (response[1] << 16) | (response[4] << 8) | response[3];
            pp.p1 = val / 1000.0;
            val = ((response[6] & 0x3F) << 24) | (response[5] << 16) | (response[8] << 8) | response[7];
            pp.p2 = val / 1000.0;
            val = ((response[10] & 0x3F) << 24) | (response[9] << 16) | (response[12] << 8) | response[11];
            pp.p3 = val / 1000.0;
            String result = "A+ (f1) = " + String(pp.p1) + "\nA+ (f2) = " + String(pp.p2) + "\nA+ (f3) = " + String(pp.p3);
            regEvent(result, "MercuryNode");
            SerialPrint("I", "MercuryNode", result);
        }
    }

    IoTValue execute(String command, std::vector<IoTValue> &param)
    {
        IoTValue val;
        if (command == "setDateTime")
        {
            if (param.size() >= 8)
            {
                WriteDate cmd;
                cmd.address = _addr;
                cmd.code = 0x03;
                cmd.paramNum = 0x0C;
                cmd.time.sec = param[0].valD;
                cmd.time.min = param[1].valD;
                cmd.time.hour = param[2].valD;
                cmd.time.dow = param[3].valD;
                cmd.time.day = param[4].valD;
                cmd.time.mon = param[5].valD;
                cmd.time.year = param[6].valD;
                cmd.time.tyear = param[7].valD;
                uint8_t response[64];
                int respSize = 0;
                if (sendRawCmd((uint8_t *)&cmd, sizeof(cmd), response, respSize))
                {
                    if (respSize >= 2 && response[1] == 0x00)
                    {
                        SerialPrint("I", "MercuryNode", "time set OK!");
                        val.valS = "time set OK!";
                    }
                    else
                    {
                        SerialPrint("E", "MercuryNode", "time set Error!");
                        val.valS = "time set Error!";
                    }
                }
                else
                {
                    SerialPrint("E", "MercuryNode", "time set Error: Failed to send command");
                    val.valS = "time set Error!";
                }
            }
            else
            {
                SerialPrint("E", "MercuryNode", "Insufficient parameters for setDateTime");
                val.valS = "time set Error!";
            }
        }
        return val;
    }

    ~MercuryNode()
    {
        MercuryNodeMap.erase(_token);
    }
};

class MercuryClient : public IoTItem
{
private:
    int8_t _rx = MODBUS_RX_PIN;
    int8_t _tx = MODBUS_TX_PIN;
    int _baud = MODBUS_SERIAL_BAUD;
    String _prot = "SERIAL_8N1";
    int protocol = SERIAL_8N1;
    bool _debug = false;

public:
    MercuryClient(String parameters) : IoTItem(parameters)
    {
        _rx = jsonReadInt(parameters, "RX");
        _tx = jsonReadInt(parameters, "TX");
        MODBUS_DIR_PIN = jsonReadInt(parameters, "DIR_PIN");
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

        _modbusUART = new HardwareSerial(MODBUS_UART_LINE);

        if (_debug)
        {
            SerialPrint("I", "MercuryClient", "baud: " + String(_baud) + ", protocol: " + String(protocol, HEX) + ", RX: " + String(_rx) + ", TX: " + String(_tx));
        }

        ((HardwareSerial *)_modbusUART)->begin(_baud, protocol, _rx, _tx);
        ((HardwareSerial *)_modbusUART)->setTimeout(TIME_OUT);
        SerialPrint("I", "MercuryClient", "Start Mercury 230 v 1.0");
    }

    ~MercuryClient()
    {
        delete _modbusUART;
        _modbusUART = nullptr;
        MercuryNodeMap.clear();
    }
};

void *getAPI_Mercury(String subtype, String param)
{
    if (subtype == "MercuryNode")
    {
        return new MercuryNode(param);
    }
    else if (subtype == "MercuryClient")
    {
        return new MercuryClient(param);
    }
    return nullptr;
}