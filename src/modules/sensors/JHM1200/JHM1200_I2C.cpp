#include "JHM1200_I2C.h"

JHM1200::JHM1200() : _sda_pin(21), _scl_pin(22), _initialized(false) {
}

bool JHM1200::begin(int sda_pin, int scl_pin, uint32_t i2c_speed) {
    if (sda_pin != -1) _sda_pin = sda_pin;
    if (scl_pin != -1) _scl_pin = scl_pin;
    
    pinMode(_sda_pin, INPUT_PULLUP);
    pinMode(_scl_pin, INPUT_PULLUP);
    _initialized = true;
    return true;
}
bool JHM1200::getPressure(double* pPress, double* pPressRaw, double* pTemp, double* pTempRaw, uint8_t devAddress) {
//bool JHM1200::getPressure(double* pPress, double* pPressRaw, uint8_t devAddress) {
    uint8_t buffer[6] = {0};
    
    // Send 0xAC command
    buffer[0] = 0xAC;
    if (_write(devAddress << 1, buffer, 1) != 0) {
        return false;
    }
    
    delay(5);
    while (isBusy(devAddress)) {
        delay(1);
    }
    
    if (_read(devAddress << 1, buffer, 6) != 0) {
        return false;
    }
    
    // Convert raw data
    uint32_t press_raw = ((uint32_t)buffer[1] << 16) | 
                        ((uint16_t)buffer[2] << 8) | 
                        buffer[3];
    uint16_t temp_raw = ((uint16_t)buffer[4] << 8) | buffer[5];
    
    // Convert to Celsius 
//    *pTemp = (double)temp_raw / 65536.0  * 190 - 4000;                        
    *pTempRaw = (double)temp_raw; 
    static const float kScaleFactor = 1.0f / 65535.0f;  // Нормализация 16-битного значения
    float normalized_temp = 1.0f - (float)temp_raw * kScaleFactor;
    *pTemp = normalized_temp * 80.0f - 2.0f;
    


    *pPress = (double)press_raw / 16777216.0;
    *pPressRaw = (double)press_raw;
    
    return true;
}

bool JHM1200::isBusy(uint8_t devAddress) {
    uint8_t status;
    _read(devAddress << 1, &status, 1);
    return (status >> 5) & 0x01;
}


// Private I2C methods
void JHM1200::_start() {
    if (!_initialized) begin();
    _sdaHigh();
    _delayUs(2);
    _sclHigh();
    _delayUs(2);
    _sdaLow();
    _delayUs(2);
    _sclLow();
}

void JHM1200::_stop() {
    _sdaOutputMode();
    _sdaLow();
    _delayUs(2);
    _sclHigh();
    _delayUs(2);
    _sdaHigh();
    _delayUs(2);
}

uint8_t JHM1200::_checkACK() {
    uint8_t ack;
    _sdaInputMode();
    _sclHigh();
    _delayUs(2);
    ack = _sdaStatus();
    _sclLow();
    _sdaOutputMode();
    return ack;
}

void JHM1200::_sendACK() {
    _sdaOutputMode();
    _sdaLow();
    _delayUs(2);
    _sclHigh();
    _delayUs(2);
    _sclLow();
    _delayUs(2);
}

void JHM1200::_sendByte(uint8_t byte) {
    uint8_t i = 0;
    _sdaOutputMode();
    do {
        if (byte & 0x80) {
            _sdaHigh();
        } else {
            _sdaLow();
        }
        _delayUs(2);
        _sclHigh();
        _delayUs(2);
        byte <<= 1;
        i++;
        _sclLow();
    } while (i < 8);
    _sclLow();
}

uint8_t JHM1200::_receiveByte() {
    uint8_t i = 0, tmp = 0;
    _sdaInputMode();
    do {
        tmp <<= 1;
        _sclHigh();
        _delayUs(2);
        if (_sdaStatus()) {
            tmp |= 1;
        }
        _sclLow();
        _delayUs(2);
        i++;
    } while (i < 8);
    return tmp;
}

uint8_t JHM1200::_write(uint8_t address, uint8_t *buf, uint8_t count) {
    uint8_t timeout, ack;
    address &= 0xFE;
    _start();
    _delayUs(2);
    _sendByte(address);
    _sdaInputMode();
    _delayUs(2);
    timeout = 0;
    do {
        ack = _checkACK();
        timeout++;
        if (timeout == 10) {
            _stop();
            return 1;
        }
    } while (ack);
    
    while (count) {
        _sendByte(*buf);
        _sdaInputMode();
        _delayUs(2);
        timeout = 0;
        do {
            ack = _checkACK();
            timeout++;
            if (timeout == 10) {
                return 2;
            }
        } while (ack);
        buf++;
        count--;
    }
    _stop();
    return 0;
}

uint8_t JHM1200::_read(uint8_t address, uint8_t *buf, uint8_t count) {
    uint8_t timeout, ack;
    address |= 0x01;
    _start();
    _sendByte(address);
    _sdaInputMode();
    _delayUs(2);
    timeout = 0;
    do {
        ack = _checkACK();
        timeout++;
        if (timeout == 4) {
            _stop();
            return 1;
        }
    } while (ack);
    
    _delayUs(2);
    while (count) {
        *buf = _receiveByte();
        if (count != 1)
            _sendACK();
        buf++;
        count--;
    }
    _stop();
    return 0;
}

// Pin control methods
void JHM1200::_sdaHigh() {
    pinMode(_sda_pin, INPUT_PULLUP);
}

void JHM1200::_sdaLow() {
    pinMode(_sda_pin, OUTPUT);
    digitalWrite(_sda_pin, LOW);
}

void JHM1200::_sclHigh() {
    pinMode(_scl_pin, INPUT_PULLUP);
}

void JHM1200::_sclLow() {
    pinMode(_scl_pin, OUTPUT);
    digitalWrite(_scl_pin, LOW);
}

bool JHM1200::_sdaStatus() {
    return digitalRead(_sda_pin);
}

void JHM1200::_sdaInputMode() {
    pinMode(_sda_pin, INPUT_PULLUP);
}

void JHM1200::_sdaOutputMode() {
    pinMode(_sda_pin, OUTPUT);
}

void JHM1200::_delayUs(uint8_t us) {
    delayMicroseconds(us);
}