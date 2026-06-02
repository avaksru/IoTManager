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

unsigned int calculateCRC(unsigned char* buf, int len)
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