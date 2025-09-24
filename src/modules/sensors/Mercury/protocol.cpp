#include "protocol.h"

const char *dow[7] = {"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
const char *moy[12] = {"Янв", "Фев", "Мар", "Апр", "Май", "Июн", "Июл", "Авг", "Сен", "Окт", "Ноя", "Дек"};

uint8_t ascii2hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return 0;
}

uint8_t getBit(uint8_t byte, uint8_t bit)
{
    return (byte >> bit) & 0x01;
}

int binToDec(String bin)
{
    int result = 0;
    for (int i = 0; i < bin.length(); i++)
    {
        result = result * 2 + (bin[i] - '0');
    }
    return result;
}

uint16_t calculateCRC(uint8_t *data, int len)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}