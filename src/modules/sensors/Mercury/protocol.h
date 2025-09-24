#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>

// Structure for three-phase values (e.g., voltage, current, power)
struct P3V
{
    float p1; // Phase 1 value
    float p2; // Phase 2 value
    float p3; // Phase 3 value
};

// Structure for power values (active and reactive)
struct PWV
{
    float ap; // Active power (A+)
    float rp; // Reactive power (R+)
};

// Structure for date and time
struct Time
{
    uint8_t sec;   // Seconds
    uint8_t min;   // Minutes
    uint8_t hour;  // Hours
    uint8_t dow;   // Day of week
    uint8_t day;   // Day
    uint8_t mon;   // Month
    uint8_t year;  // Year (last two digits)
    uint8_t tyear; // Season (0 = summer, 1 = winter)
};

// Structure for channel commands
struct ChannelCmd
{
    uint8_t address;
    uint8_t code;
};

// Structure for opening a channel with password
struct ChannelOpen
{
    uint8_t address;
    uint8_t code;
    uint8_t accessLevel;
    uint8_t password[6];
};

// Structure for reading parameters
struct ReadParam
{
    uint8_t address;
    uint8_t code;
    uint8_t paramNum;
};

// Structure for reading parameters with BWRI
struct ReadParamCmd
{
    uint8_t address;
    uint8_t code;
    uint8_t paramNum;
    uint8_t BWRI;
};

// Structure for writing date/time
struct WriteDate
{
    uint8_t address;
    uint8_t code;
    uint8_t paramNum;
    Time time;
};

// Sample arrays for days and months
extern const char *dow[7];
extern const char *moy[12];

// Stub functions
uint8_t ascii2hex(char c);
uint8_t getBit(uint8_t byte, uint8_t bit);
int binToDec(String bin);
uint16_t calculateCRC(uint8_t *data, int len);

#endif