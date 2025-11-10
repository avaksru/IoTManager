#ifndef JHM1200_I2C_H
#define JHM1200_I2C_H

#include <Arduino.h>

class JHM1200 {
public:
    // Конструктор
    JHM1200();
    
    // Инициализация с возможностью указания пинов
    bool begin(int sda_pin = -1, int scl_pin = -1, uint32_t i2c_speed = 10000);
    
    
    // Получение данных о давлении
    bool getPressure(double* pPress, double* pPressRaw, double* pTemp, double* pTempRaw, uint8_t devAddress);
    //bool getPressure(double* pPress, double* pPressRaw, uint8_t devAddress);
    
    // Проверка, занят ли датчик
    bool isBusy(uint8_t devAddress);

private:
    // Приватные методы для работы с I2C
    void _start();
    void _stop();
    uint8_t _checkACK();
    void _sendACK();
    void _sendByte(uint8_t byte);
    uint8_t _receiveByte();
    uint8_t _write(uint8_t address, uint8_t *buf, uint8_t count);
    uint8_t _read(uint8_t address, uint8_t *buf, uint8_t count);
    
    // Пины I2C
    byte _sda_pin;
    byte _scl_pin;
    bool _initialized;
    
    // Адрес датчика
    //static const uint8_t DEVICE_ADDRESS = 0x78 << 1;
    
    // Вспомогательные функции
    void _sdaHigh();
    void _sdaLow();
    void _sclHigh();
    void _sclLow();
    bool _sdaStatus();
    void _sdaInputMode();
    void _sdaOutputMode();
    void _delayUs(uint8_t us);
};

#endif