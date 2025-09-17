
#include "Global.h"
#include "classes/IoTItem.h"

#include "classes/IoTUart.h"
#include "Energy485Header.h"

// PZEMContainer _pzemCntr;
Stream *_myUART_Gran = nullptr;

namespace _Electro485
{

    unsigned short int sdm120_mbCRC16(unsigned char *puchMsg, unsigned char usDataLen)
    {
        unsigned char uchCRCHi = 0xFF; /* инициализация старшего байта контрольной суммы */
        unsigned char uchCRCLo = 0xFF; /* инициализация младшего байта контрольной суммы */
        unsigned char uIndex;

        uchCRCHi = 0xFF;
        uchCRCLo = 0xFF;

        while (usDataLen--)
        {
            uIndex = uchCRCHi ^ *puchMsg++;
            uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex];
            uchCRCLo = auchCRCLo[uIndex];
        }

        return (uchCRCLo << 8 | uchCRCHi);
    }

    unsigned short int gran485_mbCRC16(unsigned char *puchMsg, unsigned char usDataLen)
    {
        unsigned char uchCRCHi = 0xFF; /* инициализация старшего байта контрольной суммы */
        unsigned char uchCRCLo = 0xFF; /* инициализация младшего байта контрольной суммы */
        unsigned char uIndex;

        uchCRCHi = 0xFF;
        uchCRCLo = 0xFF;

        while (usDataLen--)
        {
            uIndex = (uchCRCHi ^ *puchMsg++) & 0xFF;
            uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex];
            uchCRCLo = auchCRCLo[uIndex];
        }

        return (uchCRCHi << 8 | uchCRCLo);
    }

    void uart485_send(unsigned char *str, unsigned char bytes_to_send)
    {
        // delay(20);
        if (_myUART_Gran)
        {
            for (int i = 0; i < bytes_to_send; i++)
            {
                _myUART_Gran->write(str[i]);
            }
        }
        //  delay(100);
    }

    int8_t gran485_read(byte addr, String &param, float &res)
    {
        int8_t ret;
        if (!_myUART_Gran)
            return 3;
        unsigned short int param_hex;
        mass_to_send[1] = 0x03;
        mass_to_send[4] = 0x00;
        if (param == "v")
            param_hex = 0x0A00;
        else if (param == "a")
            param_hex = 0x0B00;
        else if (param == "w")
            param_hex = 0x0800;
        else if (param == "r")
            param_hex = 0x0900;
        else if (param == "f")
            param_hex = 0x0D00;
        else if (param == "k")
        {
            param_hex = 0x0100;
            mass_to_send[1] = 0x04;
            mass_to_send[4] = 0x01;
        }
        else if (param == "p")
        {
            param_hex = 0x0C00;
        }
        SerialPrint("i", "Gran", "param: " + param + ", param_hex: " + String(param_hex, HEX));
        mass_to_send[0] = addr;
        // mass_to_send[1] = 0x03;
        mass_to_send[2] = param_hex >> 8;
        mass_to_send[3] = param_hex;

        mass_to_send[5] = 0x01;
        checkSum = gran485_mbCRC16(mass_to_send, 6);
        mass_to_send[6] = checkSum;
        mass_to_send[7] = checkSum >> 8;
        uart485_send(mass_to_send, 8);

        // Считываем первые 3 байта из ответа
        int s = _myUART_Gran->readBytes(mass_read, 10);
        // uart_send(mass_read, 3);
        // Serial.println("Count read byte: " + String(s));

        // Если вернулся правильный адрес и команда
        if (mass_read[0] == addr && mass_read[1] == mass_to_send[1])
        {
            // Проверяем контрольную сумму
            checkSum = 0;
            checkSum = gran485_mbCRC16(mass_read, 8);
            /*
                    Serial.print("HEX: ");
                    Serial.print(String(mass_read[4], HEX));
                    Serial.print(String(mass_read[5], HEX));
                    Serial.print(String(mass_read[6], HEX));
                    Serial.println(String(mass_read[7], HEX));
                    */
            uint32_t x = mass_read[7] << 24 | mass_read[6] << 16 | mass_read[5] << 8 | mass_read[4];
            float y = *(float *)&x;
            Serial.println(String(y));
            if ((byte)checkSum == mass_read[8] && (checkSum >> 8) == mass_read[9])
            {
                res = y;
                ret = 0;
            }
            else
            {
                // Serial.println("ERROR_CRC");
                ret = 1;
            }
        }
        else
        {
            // Serial.println("ERROR_NO_RESPONSE");
            ret = 2;
        }

        // очистка массива
        mass_read[0] = 0;
        mass_read[1] = 0;
        mass_read[2] = 0;
        mass_read[3] = 0;
        mass_read[4] = 0;
        mass_read[5] = 0;
        mass_read[6] = 0;
        // очистка буфера serial
        while (_myUART_Gran->available())
            _myUART_Gran->read();
        return ret;
    }

    int8_t sdm120_read(byte addr, String &param, float &res)
    {
        int8_t ret;
        if (!_myUART_Gran)
            return 3;
        unsigned short int param_hex;
        if (param == "v")
            param_hex = 0x0000;
        else if (param == "a")
            param_hex = 0x0006;
        else if (param == "w")
            param_hex = 0x000C;
        else if (param == "r")
            param_hex = 0x0018;
        else if (param == "f")
            param_hex = 0x0046;
        else if (param == "k")
            param_hex = 0x0156;
        else if (param == "p")
            param_hex = 0x001E;
        SerialPrint("i", "SMD120", "param: " + param + ", param_hex: " + String(param_hex, HEX));

        mass_to_send[0] = addr;
        mass_to_send[1] = 0x04;
        mass_to_send[2] = param_hex >> 8;
        mass_to_send[3] = param_hex;
        mass_to_send[4] = 0x00;
        mass_to_send[5] = 0x02;
        checkSum = sdm120_mbCRC16(mass_to_send, 6);
        mass_to_send[6] = checkSum;
        mass_to_send[7] = checkSum >> 8;
        uart485_send(mass_to_send, 8);

        // Считываем первые 3 байта из ответа
        _myUART_Gran->readBytes(mass_read, 3);
        // uart_send(mass_read, 3);

        // Если вернулся правильный адрес и команда
        if (mass_read[0] == addr && mass_read[1] == 0x04)
        {
            // то считываем данные (их количество было в тетьем байте) +2 байта на контрольную сумму +3 байта чтобы не затереть начало массива
            for (int i = 3; i < mass_read[2] + 5; i++)
            {
                mass_read[i] = _myUART_Gran->read();
            }

            // Проверяем контрольную сумму
            checkSum = 0;
            checkSum = sdm120_mbCRC16(mass_read, mass_read[2] + 3);
            if ((byte)checkSum == mass_read[mass_read[2] + 3] && (checkSum >> 8) == mass_read[mass_read[2] + 4])
            {
                // преобразуем результат во float

                uint32_t x = mass_read[3] << 24 | mass_read[4] << 16 | mass_read[5] << 8 | mass_read[6];
                float y = *(float *)&x;
                res = y;

                ret = 0;
            }
            else
            {
                // Serial.println("ERROR_CRC");
                ret = 1;
            }
        }
        else
        {
            // Serial.println("ERROR_NO_RESPONSE");
            ret = 2;
        }

        // очистка массива
        mass_read[0] = 0;
        mass_read[1] = 0;
        mass_read[2] = 0;
        mass_read[3] = 0;
        mass_read[4] = 0;
        mass_read[5] = 0;
        mass_read[6] = 0;

        // очистка буфера serial
        while (_myUART_Gran->available())
            _myUART_Gran->read();
        return ret;
    }

    class GranItem : public IoTItem
    {
    private:
        String _sensor;

    public:
        GranItem(String parameters) : IoTItem(parameters)
        {
            _sensor = jsonReadStr(parameters, "sensor");
        }

        void doByInterval()
        {

            byte addr = 00;
            float val = 0;
            int8_t result = gran485_read(addr, _sensor, val);

            if (result == 0) // OK
                regEvent(val, "Gran");
            else
            {
                regEvent(NAN, "Gran");
                if (result == 1) // ERROR_CRC
                    SerialPrint("E", "Gran", "ERROR_CRC", _id);
                else if (result == 2) // ERROR_NO_RESPONSE
                    SerialPrint("E", "Gran", "ERROR_NO_RESPONSE", _id);
                else if (result == 3) // (!_myUART_Gran)
                    SerialPrint("E", "Gran", "gran_uart not found", _id);
            }
        }

        ~GranItem(){};
    };

    class SDM120Item : public IoTItem
    {
    private:
        String _sensor;

    public:
        SDM120Item(String parameters) : IoTItem(parameters)
        {
            _sensor = jsonReadStr(parameters, "sensor");
        }

        void doByInterval()
        {

            byte addr = 00;
            float val = 0;
            int8_t result = sdm120_read(addr, _sensor, val);

            if (result == 0) // OK
                regEvent(val, "SDM120");
            else
            {
                regEvent(NAN, "SDM120");
                if (result == 1) // ERROR_CRC
                    SerialPrint("E", "SDM120", "ERROR_CRC", _id);
                else if (result == 2) // ERROR_NO_RESPONSE
                    SerialPrint("E", "SDM120", "ERROR_NO_RESPONSE", _id);
                else if (result == 3) // (!_myUART_Gran)
                    SerialPrint("E", "SDM120", "gran_uart not found", _id);
            }
        }

        ~SDM120Item(){};
    };

    class EnergyUART : public IoTUart
    {
    public:
        EnergyUART(String parameters) : IoTUart(parameters)
        {
            _myUART_Gran = _myUART;
        }

        ~EnergyUART(){};
    };
}
void *getAPI_EnergyMon485(String subtype, String param)
{
    if (subtype == F("gran485"))
    {
        return new _Electro485::GranItem(param);
    }
    else if (subtype == F("sdm120"))
    {
        return new _Electro485::SDM120Item(param);
    }
    else if (subtype == F("energy_uart"))
    {
        return new _Electro485::EnergyUART(param);
    }
    else
    {
        return nullptr;
    }
}
