/**
 * @file NexUpload.h
 * The definition of class NexUpload. 
 * 
 * 1 - Removed all the Arduino code and replaced it by ESP-IDF
 * 2 - Removed hard-coded UART configuration, see ESPNexUpload constructor
 * 3 - Removed statusMessage and the function _printInfoLine
 * 4 - Removed call-back functionality
 * 5 - Removed one out of two upload functions
 * 6 - BugFix in upload function 
 * @author Machiel Mastenbroek (machiel.mastenbroek@gmail.com)
 * @date   2022/08/14
 * @version 0.6.0
 * 
 * 1 - BugFix when display baudrate is diffrent from initial ESP baudrate
 * 2 - Improved debug information
 * 3 - Make delay commands dependent on the baudrate
 * @author Machiel Mastenbroek (machiel.mastenbroek@gmail.com)
 * @date   2019/11/04
 * @version 0.5.5
 *
 * Stability improvement, Nextion display doesn’t freeze after the seconds 4096 trance of firmware bytes. 
 * Now the firmware upload process is stabled without the need of a hard Display power off-on intervention. 
 * Undocumented features (not mentioned in nextion-hmi-upload-protocol-v1-1 specification) are added. 
 * This implementation is based in on a reverse engineering with a UART logic analyser between 
 * the Nextion editor v0.58 and a NX4024T032_011R Display.
 * 
 * @author Machiel Mastenbroek (machiel.mastenbroek@gmail.com)
 * @date   2019/10/24
 * @version 0.5.0
 *
 * Modified to work with ESP32, HardwareSerial and removed SPIFFS dependency
 * @author Onno Dirkzwager (onno.dirkzwager@gmail.com)
 * @date   2018/12/26
 * @version 0.3.0
 * 
 * Modified to work with ESP8266 and SoftwareSerial
 * @author Ville Vilpas (psoden@gmail.com)
 * @date   2018/2/3
 * @version 0.2.0
 *
 * Original version (a part of https://github.com/itead/ITEADLIB_Arduino_Nextion)
 * @author  Chen Zengpeng (email:<zengpeng.chen@itead.cc>)
 * @date    2016/3/29
 * @copyright 
 * Copyright (C) 2014-2015 ITEAD Intelligent Systems Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __ESPNEXUPLOAD_H__
#define __ESPNEXUPLOAD_H__
//#include <iostream>
#include <string.h>      /* printf, scanf, NULL */

//#include <inttypes.h>
#include "esp_log.h"
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "driver/gpio.h"
#include "driver/uart.h"

//#include "hal/uart_types.h"
#include <Arduino.h>
#include <StreamString.h>

#define CONFIG_NEX_UART_RECV_BUFFER_SIZE 256

/**
 * @addtogroup CoreAPI 
 * @{ 
 */

/**
 *
 * Provides the API for nextion to upload the ftf file.
 */
class ESPNexUpload
{
public: /* methods */
	
    /**
     * Constructor. 
     * 
     */
    ESPNexUpload(uart_port_t uart_num, uint32_t baud_rate, gpio_num_t tx_io_num, gpio_num_t rx_io_num);
    
    /**
     * destructor. 
     * 
     */
    ~ESPNexUpload(){}
	
    /**
     * Connect to Nextion over serial
     *
     * @return true or false.
     */
    bool connect();
    
    /**
     * prepare upload. Set file size & Connect to Nextion over serial
     *
     * @return true if success, false for failure.
     */
	bool prepareUpload(uint32_t file_size, bool oldProt);
    
    /**
     * start update tft file to nextion. 
     * 
     * @param const uint8_t *file_buf
     * @param size_t buf_size
     * @return true if success, false for failure.
     */
    bool upload(const uint8_t *file_buf, size_t buf_size);

    bool upload(Stream &myFile);
    /**
     * Send reset command to Nextion over serial
     *
     * @return none.
     */
	void softReset(void);

    /**
     * Send reset, end serial, reset _sent_packets & update status message
     *
     * @return none.
     */
	void end(void);
	
private: /* methods */
    /*
     * Semaphore construction to prevent double UART actions
     *
     */
    void uart_mutex_lock(void) {do {} while (xSemaphoreTake(_upload_uart_lock, portMAX_DELAY) != pdPASS);};
    void uart_mutex_unlock(void) {xSemaphoreGive(_upload_uart_lock);};

    /*
     * get communicate baudrate. 
     * 
     * @return communicate baudrate.
     *
     */
    uint16_t _getBaudrate(void);

    /*
     * search communicate baudrate.
     *
     * @param baudrate - communicate baudrate.
     *   
     * @return true if success, false for failure. 
     */
    bool _searchBaudrate(int baudrate);

    /*
     * set download baudrate.
     *
     * @param baudrate - set download baudrate.
     *   
     * @return true if success, false for failure. 
     */
    bool _setPrepareForFirmwareUpdate(uint32_t upload_baudrate);

    /*
     * set Nextion running mode.
     *
	 * Undocumented feature of the Nextion protocol.
	 * It's used by the 'upload to Nextion device' feature of the Nextion Editor V0.58
	 * 
	 * The nextion display doesn't send any response
	 *
     */
    void _setRunningMode(void);

    /*
     * Test UART nextion connection availability 
     *
	 * @param input  - echo string,
	 *
	 * @return true when the 'echo string' that is send is equal to the received string
	 * 
	 * This test is used by the 'upload to Nextion device' feature of the Nextion Editor V0.58
	 *
     */
    bool _echoTest(std::string input);
    
    /*
     * This function get the sleep and dim value from the Nextion display.
     *
	 * If sleep = 1 meaning: sleep is enabled
     *	            action : sleep will be disabled
	 * If dim = 0,  meaning: the display backlight is turned off
	 *              action : dim will be set to 100 (percent)
	 *
     */
    bool _handlingSleepAndDim(void);
	
    /*
     * This function (debug) print the Nextion response to a human readable string
     *
	 * @param esp_request  - true:  request  message from esp     to nextion
	 *                       false: response message from nextion to esp
	 *
	 * @param input - string to print
	 *
     */
    void _printSerialData(bool esp_request, std::string input);
	
    /*
     * Send command to Nextion.
     *
     * @param cmd       - the string of command.
	 * @param tail      - end the string with tripple 0xFF byte
	 * @param null_head - start the string with a single 0x00 byte
     *
     * @return none.
     */
    void sendCommand(const char* cmd, bool tail = true, bool null_head = false);

    /*
     * Receive string data. 
     * 
     * @param buffer - save string data.  
     * @param timeout - set timeout time. 
     * @param recv_flag - if recv_flag is true,will braak when receive 0x05.
     *
     * @return the length of string buffer.
     *
     */   
    uint16_t recvRetString(std::string &string, uint32_t timeout = 500,bool recv_flag = false);

    /*
     * 
     * This function calculates the transmission time, the transmission time 
     * is based on the length of the message and the baudrate.
     * 
     * @param message - only used to determine the length of the message 
     *
     * @return time in us length of string buffer.
     *
     */
    uint32_t calculateTransmissionTimeMs(std::string message);
    
    /*
     * Setup UART for communication with display
     * 
     * @param uart_num - UART number
     * @param baud_rate - baud rate speed
     * @param tx_io_num - GPIO TX pin
     * @param rx_io_num - GPIO RX pin
     *
     */   
    void setBaudrate(uart_port_t uart_num, uint32_t baud_rate, gpio_num_t tx_io_num, gpio_num_t rx_io_num);

    /*
     * Check is UART is avaialble
     */
    uint32_t uartAvailable();

    /*
     * Read one RX byte
     *
     * @return one received UART byte
     */
    uint8_t uartRead();

    /*
     * Write one TX byte
     *
     * @param c - one byte
     * 
     */
    void uartWrite(uint8_t c);

    /*
     * Write char string
     *
     * @param data - char string of data to send 
     * @param len - length of the string 
     * 
     */
    void uartWriteBuf(const char * data, size_t len);

    /*
     * Clear TX UART buffer
     */
    void uartFlushTxOnly();

private: /* data */ 
    bool _oldProtv11;
    uint32_t _baudrate; 	            /* nextion serail baudrate */
    uint32_t _undownloadByte; 	        /* undownload byte of tft file */
    uart_port_t _upload_uart_num;       /* upload uart port number */
    uint32_t    _upload_baudrate;       /* upload baudrate */
    gpio_num_t  _upload_tx_io_num;      /* upload gpio TX */
    gpio_num_t  _upload_rx_io_num;      /* upload gpio RX */
    xSemaphoreHandle _upload_uart_lock; /* semaphore to prevent double UART actions */
    bool _upload_uart_has_peek;         /* UART RX peek flag */
    uint8_t _upload_uart_peek_byte;     /* UART RX peek byte */ 
    //uint16_t _sent_packets = 0;         /* _sent_packets till 4096 bytes */ 
    uint32_t _sent_packets_total = 0;   /* total number of uploaded display firmware bytes */
    bool _uart_diver_installed;         /* flag, if true UART is installed */

std::string str_snprintf(const char *fmt, size_t len, ...);
    /// Format the byte array \p data of length \p len in pretty-printed, human-readable hex.
std::string format_hex_pretty(const uint8_t *data, size_t length);
static char format_hex_pretty_char(uint8_t v);
};
/**
 * @}
 */

#endif /* #ifndef __ESPNEXUPLOAD_H__ */
