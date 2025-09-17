/**
 * @file NexUpload.cpp
 *
 * The implementation of uploading tft file for nextion displays.
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
/*
#ifdef CORE_DEBUG_LEVEL
#undef CORE_DEBUG_LEVEL
#endif

#define CORE_DEBUG_LEVEL 3
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
*/

#include "ESPNexUpload.h"

static const char *TAG = "nextion upload";

ESPNexUpload::ESPNexUpload(uart_port_t uart_num, uint32_t baud_rate, gpio_num_t tx_io_num, gpio_num_t rx_io_num)
{
	_upload_uart_lock = xSemaphoreCreateMutex();
	_uart_diver_installed = false;
	setBaudrate(uart_num, baud_rate, tx_io_num, rx_io_num);
}

bool ESPNexUpload::connect()
{
#if defined ESP8266
	yield();
#endif

	ESP_LOGI(TAG, "serial tests & connect");

	if (_getBaudrate() == 0)
	{
		ESP_LOGE(TAG, "get baudrate error");
		return false;
	}

	_setRunningMode();

	if (!_echoTest("mystop_yesABC"))
	{
		ESP_LOGE(TAG, "echo test failed");
		return false;
	}

	if (!_handlingSleepAndDim())
	{
		ESP_LOGE(TAG, "handling sleep and dim settings failed");
		return false;
	}

	if (!_setPrepareForFirmwareUpdate(_upload_baudrate))
	{
		ESP_LOGE(TAG, "modifybaudrate error");
		return false;
	}

	return true;
}

bool ESPNexUpload::prepareUpload(uint32_t file_size, bool oldProt)
{
	_oldProtv11 = oldProt;
	_undownloadByte = file_size;
	ESP_LOGD(TAG, "prepareUpload: %" PRIu32, file_size);
	vTaskDelay(5 / portTICK_PERIOD_MS);
	return this->connect();
}

uint16_t ESPNexUpload::_getBaudrate(void)
{

	_baudrate = 0;
	uint32_t baudrate_array[7] = {115200, 19200, 9600, 57600, 38400, 4800, 2400};
	for (uint8_t i = 0; i < 7; i++)
	{
		if (_searchBaudrate(baudrate_array[i]))
		{
			_baudrate = baudrate_array[i];
			ESP_LOGI(TAG, "baudrate determined: %i", _baudrate);
			break;
		}
		vTaskDelay(1500 / portTICK_PERIOD_MS); // wait for 1500 ms
	}
	return _baudrate;
}

bool ESPNexUpload::_searchBaudrate(int baudrate)
{

#if defined ESP8266
	yield();
#endif

	std::string response = "";
	ESP_LOGD(TAG, "init nextion serial interface on baudrate: %i", baudrate);

	setBaudrate(_upload_uart_num,
				baudrate,
				_upload_tx_io_num,
				_upload_rx_io_num);

	ESP_LOGD(TAG, "ESP baudrate established, try to connect to display");
	const char _nextion_FF_FF[3] = {0xFF, 0xFF, 0x00};

	this->sendCommand("DRAKJHSUYDGBNCJHGJKSHBDN", true, true); // 0x00 0xFF 0xFF 0xFF
	this->recvRetString(response);
	if (response[0] != 0x1A)
	{
		ESP_LOGW(TAG, "first indication that baudrate is wrong");
	}
	else
	{
		ESP_LOGI(TAG, "first respone from display, first indication that baudrate is correct");
	}

	this->sendCommand("connect"); // first connect attempt
	this->recvRetString(response);
	if (response.find("comok") == -1)
	{
		ESP_LOGW(TAG, "display doesn't accept the first connect request");
	}
	else
	{
		ESP_LOGI(TAG, "display accept the first connect request");
	}

	vTaskDelay(110 / portTICK_PERIOD_MS); // based on serial analyser from Nextion editor V0.58 to Nextion display NX4024T032_011R
	this->sendCommand(_nextion_FF_FF, false);

	this->sendCommand("connect"); // second attempt
	this->recvRetString(response);

	if (response.find("comok") == -1 && response[0] != 0x1A)
	{
		ESP_LOGW(TAG, "display doesn't accept the second connect request");
		ESP_LOGW(TAG, "conclusion, wrong baudrate");
		return false;
	}
	else
	{
		ESP_LOGI(TAG, "display accept the second connect request");
		ESP_LOGI(TAG, "conclusion, correct baudrate");
	}

	return true;
}

void ESPNexUpload::sendCommand(const char *cmd, bool tail, bool null_head)
{

#if defined ESP8266
	yield();
#endif

	if (null_head)
	{
		uartWrite(0x00);
	}

	while (uartAvailable())
	{
		uartRead();
	}

	uartWriteBuf(cmd, strlen(cmd));

	if (tail)
	{
		uartWrite(0xFF);
		uartWrite(0xFF);
		uartWrite(0xFF);
	}
}

uint16_t ESPNexUpload::recvRetString(std::string &response, uint32_t timeout, bool recv_flag)
{

#if defined ESP8266
	yield();
#endif

	uint16_t ret = 0;
	uint8_t c = 0;
	uint8_t nr_of_FF_bytes = 0;
	long start;
	bool exit_flag = false;
	bool ff_flag = false;
	response = "";
	// if (timeout != 500)
	ESP_LOGD(TAG, "timeout setting serial read: %" PRIu32, timeout);

	start = (unsigned long)(esp_timer_get_time() / 1000ULL);

	while ((unsigned long)(esp_timer_get_time() / 1000ULL) - start <= timeout)
	{

		while (uartAvailable())
		{

			c = uartRead();
			if (c == 0)
			{
				continue;
			}

			if (c == 0xFF)
				nr_of_FF_bytes++;
			else
			{
				nr_of_FF_bytes = 0;
				ff_flag = false;
			}

			if (nr_of_FF_bytes >= 3)
				ff_flag = true;

			response += (char)c;

			if (recv_flag)
			{
				if (response.find(0x05) != -1 && response.length() == 1)
				{
					exit_flag = true;
				}
				else if (response.find(0x08) != -1 && response.length() == 5)
				{
					exit_flag = true;
				}
			}
		}
		if (exit_flag || ff_flag)
		{
			break;
		}
	}

	if (ff_flag)
		response = response.substr(0, response.length() - 3); // Remove last 3 0xFF

	ret = response.length();

	return ret;
}

bool ESPNexUpload::_setPrepareForFirmwareUpdate(uint32_t upload_baudrate)
{

#if defined ESP8266
	yield();
#endif

	std::string response = "";
	std::string cmd = "";

	cmd = "00";
	this->sendCommand(cmd.c_str());
	vTaskDelay(10 / portTICK_PERIOD_MS);
	this->recvRetString(response, 800, true); // normal response time is 400ms
	ESP_LOGD(TAG, "response (00): %s", response.c_str());
	if (_oldProtv11)
		cmd = "whmi-wri " + std::to_string(_undownloadByte) + "," + std::to_string(upload_baudrate) + ",0";
	else
		cmd = "whmi-wris " + std::to_string(_undownloadByte) + "," + std::to_string(upload_baudrate) + ",1";
	ESP_LOGI(TAG, "cmd: %s", cmd.c_str());

	this->sendCommand(cmd.c_str());

	// Without flush, the whmi command will NOT transmitted by the ESP in the current baudrate
	// because switching to another baudrate (nexSerialBegin command) has an higher prio.
	// The ESP will first jump to the new 'upload_baudrate' and than process the serial 'transmit buffer'
	// The flush command forced the ESP to wait until the 'transmit buffer' is empty
	// nexSerial.flush();
	uartFlushTxOnly();

	this->recvRetString(response, 800, true); // normal response time is 400ms

	ESP_LOGD(TAG, "response (01): %s", response.c_str());

	// The Nextion display will, if it's ready to accept data, send a 0x05 byte.
	if (response.find(0x05) != -1)
	{
		ESP_LOGI(TAG, "preparation for firmware update done");
		return true;
	}
	else
	{
		ESP_LOGE(TAG, "preparation for firmware update failed");
		return false;
	}
}

// НЕ ПРОВЕРЯЛОСЬ !!!!!!!!!!!!!!!!!!
bool ESPNexUpload::upload(const uint8_t *file_buf, size_t file_size)
{

#if defined ESP8266
	yield();
#endif

	// uint8_t c;
	uint8_t timeout = 0;
	std::string response = "";
	uint8_t sent_bulk_counter = 0;
	// uint8_t buff[4096] = {0};
	uint32_t offset = 0;
	int remainingBlocks = ceil(file_size / 4096);
	int blockSize = 4096;

	while (remainingBlocks > 0)
	{
		if (remainingBlocks == 1)
		{
			blockSize = file_size - offset;
		}
		uartWriteBuf((char *)file_buf[offset], blockSize);
		// wait for the Nextion to return its 0x05 byte confirming reception and readiness to receive the next packets
		this->recvRetString(response, 2000, true);

		ESP_LOGE(TAG, "response [%s]",
				 format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str());

		if (response[0] == 0x08 && response.size() == 5)
		{ // handle partial upload request
			remainingBlocks -= 1;
			_sent_packets_total += blockSize;
			sent_bulk_counter++;
			if (sent_bulk_counter % 10 == 0)
			{
				ESP_LOGI(TAG, "bulk: %i, total bytes %" PRIu32 ", response: %s", sent_bulk_counter, _sent_packets_total, response.c_str());
			}

			// ESP_LOGE(TAG, "response [%s]",
			//		 format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str());

			for (int j = 0; j < 4; ++j)
			{
				offset += static_cast<uint8_t>(response[j + 1]) << (8 * j);
				ESP_LOGI(TAG, "Offset : %" PRIu32, offset);
			}
			if (offset)
				remainingBlocks = ceil((file_size - offset) / blockSize);
		}
		else if (response[0] == 0x05)
		{
			remainingBlocks -= 1;
			_sent_packets_total += blockSize;
			sent_bulk_counter++;
			if (sent_bulk_counter % 10 == 0)
			{
				ESP_LOGI(TAG, "bulk: %i, total bytes %" PRIu32 ", response: %s", sent_bulk_counter, _sent_packets_total, response.c_str());
			}

			// ESP_LOGE(TAG, "response [%s]",
			//		 format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str());
			offset += 4096;
		}

		else
		{
			if (timeout >= 2)
			{
				ESP_LOGE(TAG, "upload failed, no valid response from display, total bytes send : %" PRIu32, _sent_packets_total);
				sent_bulk_counter = 0;
				return false;
			}
			timeout++;
		}
	}
	ESP_LOGI(TAG, "upload send last bytes %" PRIu32 ", response: %s", _sent_packets_total, response.c_str());
	// ESP_LOGI(TAG,"upload finished, total bytes send : %"PRIu32, _sent_packets_total);
	sent_bulk_counter = 0;
	return true;
}

bool ESPNexUpload::upload(Stream &myFile)
{

#if defined ESP8266
	yield();
#endif
	// uint8_t c;
	uint8_t timeout = 0;
	std::string response = "";
	uint8_t sent_bulk_counter = 0;
	uint8_t file_buf[4096] = {0};
	uint32_t offset = 0;
	uint32_t _seekByte = 0;
	uint32_t _packets_total_byte = 0;
	// get available data size
	size_t file_size = _undownloadByte; // myFile.available();
	if (file_size)
	{
		int remainingBlocks = ceil(file_size / 4096.);
		int blockSize = 4096;
		ESP_LOGI(TAG, "Remaining Blocks ALL: %" PRIu32, remainingBlocks);
		while (remainingBlocks > 0)
		{
			// myFile.available();
			//  read up to 4096 byte into the buffer
			if (_seekByte > 0)
			{
				if (file_size > _seekByte)
				{
					blockSize = myFile.readBytes(file_buf, _seekByte);
					// file_size = myFile.available();
					file_size -= _seekByte;
					ESP_LOGI(TAG, "Seek file: %" PRIu32 ", left bytes %" PRIu32, _seekByte, file_size);
				}
				else
				{
					ESP_LOGE(TAG, "File failed seek");
					return false;
				}
				blockSize = myFile.readBytes(file_buf, ((file_size > sizeof(file_buf)) ? sizeof(file_buf) : file_size));
				file_size -= blockSize; // осталось байт
			}
			else
			{
				blockSize = myFile.readBytes(file_buf, ((file_size > sizeof(file_buf)) ? sizeof(file_buf) : file_size));
				file_size -= blockSize; // осталось байт
			}
			uartWriteBuf((char *)file_buf, blockSize);
			// wait for the Nextion to return its 0x05 byte confirming reception and readiness to receive the next packets
			if (response[0] == 0x08 || timeout >= 4)
				this->recvRetString(response, 2000, true);
			else
				this->recvRetString(response, 500, true);

			ESP_LOGE(TAG, "upload response byte [%s]",
					 format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str());
			ESP_LOG_BUFFER_HEX(TAG, response.data(), response.size());
			if (response[0] == 0x08 && response.size() == 5)
			{ // handle partial upload request
				remainingBlocks -= 1;
				_sent_packets_total += blockSize; // отправлено байт
				_packets_total_byte += blockSize; // всего байт отправлено или пропущено
				sent_bulk_counter++;
				if (sent_bulk_counter % 10 == 0)
				{
					//		ESP_LOGI(TAG, "bulk: %i, total bytes %" PRIu32 ", response: %s", sent_bulk_counter, _sent_packets_total, response.c_str());
				}
				ESP_LOGE(TAG, "upload response [%s]",
						 format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str());
				for (int j = 0; j < 4; ++j)
				{
					offset += static_cast<uint8_t>(response[j + 1]) << (8 * j);
				}
				ESP_LOGI(TAG, "Offset : %" PRIu32, offset);
				if (offset)
				{
					remainingBlocks = ceil((file_size - offset) / blockSize);
					_seekByte = offset - _packets_total_byte;
					_packets_total_byte += _seekByte;
					ESP_LOGI(TAG, "Seek Byte : %" PRIu32, _seekByte);
					ESP_LOGI(TAG, "Remaining Blocks : %" PRIu32, remainingBlocks);
				}
			}
			else if ((response[0] == 0x08 || response[0] == 0x05) && response.size() == 1)
			{
				remainingBlocks -= 1;
				_sent_packets_total += blockSize;
				_packets_total_byte += blockSize;
				file_size -= blockSize;
				sent_bulk_counter++;
				if (sent_bulk_counter % 10 == 0)
				{
					//		ESP_LOGI(TAG, "bulk: %i, total bytes %" PRIu32 ", response: %s", sent_bulk_counter, _sent_packets_total, response.c_str());
				}

				ESP_LOGE(TAG, "upload response [%s]",
						 format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str());
				offset += 4096;
			}

			else
			{
				ESP_LOGE(TAG, "Fail response [%s]",
						 format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str());
				if (timeout >= 9)
				{
					ESP_LOGE(TAG, "upload failed, no valid response from display, total bytes send : %" PRIu32, _sent_packets_total);
					sent_bulk_counter = 0;
					return false;
				}
				timeout++;
			}
			ESP_LOGI(TAG, "bulk: %i, total bytes %" PRIu32 ", response: %s", sent_bulk_counter, _sent_packets_total, response.c_str());
		}
		this->recvRetString(response, 3000, true);
		if (response[0] == 0x88)
		{
			ESP_LOGI(TAG, "upload finished (Response 0x88), total bytes send : %" PRIu32, _sent_packets_total);
			this->end();
		}
		else
		{
			ESP_LOGE(TAG, "upload response [%s]",
					 format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str());
			ESP_LOGI(TAG, "upload finished (TimeOut 0x88), total bytes send : %" PRIu32, _sent_packets_total);
			this->end();
		}
		// ESP_LOGI(TAG, "upload send last bytes %" PRIu32 ", response: %s", _sent_packets_total, response.c_str());
		//  ESP_LOGI(TAG,"upload finished, total bytes send : %"PRIu32, _sent_packets_total);
		sent_bulk_counter = 0;
		return true;
	}
	else
	{
		ESP_LOGE(TAG, "File failed available");
		return false;
	}
}

void ESPNexUpload::softReset(void)
{
	// soft reset nextion device
	this->sendCommand("rest");
}

void ESPNexUpload::end()
{

	if (_upload_uart_num == NULL)
	{
		return;
	}

	// wait for the nextion to finish internal processes
	// vTaskDelay(1600 / portTICK_PERIOD_MS);

	// soft reset the nextion
	this->softReset();

	// end Serial connection
	// uart_mutex_lock();
	// ESP_ERROR_CHECK(uart_driver_delete(_upload_uart_num));
	// uart_mutex_unlock();

	// reset sent packets counter
	//_sent_packets = 0;
	_sent_packets_total = 0;

	ESP_LOGI(TAG, "serial connection closed");
}

void ESPNexUpload::_setRunningMode(void)
{
	vTaskDelay(100 / portTICK_PERIOD_MS);
	this->sendCommand("runmod=2");
	vTaskDelay(60 / portTICK_PERIOD_MS);
}

bool ESPNexUpload::_echoTest(std::string input)
{

	std::string response = "";
	std::string cmd = "print \"" + input + "\"";

	this->sendCommand(cmd.c_str());
	uint32_t duration_ms = calculateTransmissionTimeMs(cmd) * 2 + 10; // times 2  (send + receive) and 10 ms extra
	this->recvRetString(response, duration_ms);

	return (response.find(input) != -1);
}

bool ESPNexUpload::_handlingSleepAndDim(void)
{

	std::string response = "";
	bool set_sleep = false;
	bool set_dim = false;

	this->sendCommand("get sleep");
	this->recvRetString(response);

	if (response[0] != 0x71)
	{
		ESP_LOGE(TAG, "unknown response from 'get sleep' request");
		return false;
	}

	if (response[1] != 0x00)
	{
		ESP_LOGD(TAG, "sleep enabled");
		set_sleep = true;
	}
	else
	{
		ESP_LOGD(TAG, "sleep disabled");
	}

	this->sendCommand("get dim");
	this->recvRetString(response);

	if (response[0] != 0x71)
	{
		ESP_LOGE(TAG, "unknown response from 'get dim' request");
		return false;
	}

	if (response[1] == 0x00)
	{
		ESP_LOGD(TAG, "dim is 0%%, backlight from display is turned off");
		set_dim = true;
	}
	else
	{
		ESP_LOGD(TAG, "dim %i%%", (uint8_t)response[1]);
	}

	if (!_echoTest("ABC"))
	{
		ESP_LOGE(TAG, "echo test in 'handling sleep and dim' failed");
		return false;
	}

	if (set_sleep)
	{
		this->sendCommand("sleep=0");
		// Unfortunately the display doesn't send any respone on the wake up request (sleep=0)
		// Let the ESP wait for one second, this is based on serial analyser from Nextion editor V0.58 to Nextion display NX4024T032_011R
		// This gives the Nextion display some time to wake up
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

	if (set_dim)
	{
		this->sendCommand("dim=100");
		vTaskDelay(15 / portTICK_PERIOD_MS);
	}

	return true;
}

void ESPNexUpload::_printSerialData(bool esp_request, std::string input)
{
	char c;
	if (esp_request)
		ESP_LOGI(TAG, "ESP     request: ");
	else
		ESP_LOGI(TAG, "Nextion respone: ");

	if (input.length() == 0)
	{
		ESP_LOGW(TAG, "none");
		return;
	}

	for (int i = 0; i < input.length(); i++)
	{

		c = input[i];
		if ((uint8_t)c >= 0x20 && (uint8_t)c <= 0x7E)
			printf("%c", c);
		else
		{
			printf("0x\\%02hhx", c);
		}
	}
}

uint32_t ESPNexUpload::calculateTransmissionTimeMs(std::string message)
{
	// In general, 1 second (s) = 1000 (10^-3) millisecond (ms) or
	//             1 second (s) = 1000 000 (10^-6) microsecond (us).
	// To calculate how much microsecond one BIT of data takes with a certain baudrate you have to divide
	// the baudrate by one second.
	// For example 9600 baud = 1000 000 us / 9600 ≈ 104 us
	// The time to transmit one DATA byte (if we use default UART modulation) takes 10 bits.
	// 8 DATA bits and one START and one STOP bit makes 10 bits.
	// In this example (9600 baud) a byte will take 1041 us to send or receive.
	// Multiply this value by the length of the message (number of bytes) and the total transmit/ receive time
	// is calculated.

	uint32_t duration_one_byte_us = 10000000 / _baudrate; // 1000 000 * 10 bits / baudrate
	uint16_t nr_of_bytes = message.length() + 3;		  // 3 times 0xFF byte
	uint32_t duration_message_us = nr_of_bytes * duration_one_byte_us;
	uint32_t return_value_ms = duration_message_us / 1000;

	ESP_LOGD(TAG, "calculated transmission time: %" PRIu32 " ms", return_value_ms);
	return return_value_ms;
}

uint32_t ESPNexUpload::uartAvailable()
{
	if (_upload_uart_num == NULL)
	{
		return 0;
	}

	uart_mutex_lock();
	size_t available;
	uart_get_buffered_data_len(_upload_uart_num, &available);
	if (_upload_uart_has_peek)
		available++;
	uart_mutex_unlock();
	return available;
}

uint8_t ESPNexUpload::uartRead()
{
	if (_upload_uart_num == NULL)
	{
		return 0;
	}
	uint8_t c = 0;

	uart_mutex_lock();
	if (_upload_uart_has_peek)
	{
		_upload_uart_has_peek = false;
		c = _upload_uart_peek_byte;
	}
	else
	{

		int len = uart_read_bytes(_upload_uart_num, &c, 1, 20 / portTICK_RATE_MS);
		if (len == 0)
		{
			c = 0;
		}
	}
	uart_mutex_unlock();
	return c;
}

void ESPNexUpload::uartWrite(uint8_t c)
{
	if (_upload_uart_num == NULL)
	{
		return;
	}
	uart_mutex_lock();
	uart_write_bytes(_upload_uart_num, &c, 1);
	uart_mutex_unlock();
}

void ESPNexUpload::uartWriteBuf(const char *data, size_t len)
{
	if (_upload_uart_num == NULL || data == NULL || !len)
	{
		return;
	}

	uart_mutex_lock();
	uart_write_bytes(_upload_uart_num, data, len);
	uart_mutex_unlock();
}

void ESPNexUpload::uartFlushTxOnly()
{

	bool txOnly = true;
	if (_upload_uart_num == NULL)
	{
		return;
	}

	uart_mutex_lock();
	ESP_ERROR_CHECK(uart_wait_tx_done(_upload_uart_num, portMAX_DELAY));

	if (!txOnly)
	{
		ESP_ERROR_CHECK(uart_flush_input(_upload_uart_num));
	}
	uart_mutex_unlock();
}

void ESPNexUpload::setBaudrate(uart_port_t uart_num, uint32_t baud_rate, gpio_num_t tx_io_num, gpio_num_t rx_io_num)
{

	ESP_LOGD(TAG, "installing driver on uart %d with baud rate %d", uart_num, baud_rate);

	_upload_uart_num = uart_num;
	_upload_baudrate = baud_rate;
	_upload_tx_io_num = tx_io_num;
	_upload_rx_io_num = rx_io_num;

	const uart_config_t uart_config = {
		.baud_rate = (int)baud_rate,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

	// Do not change the UART initialization order.
	// This order was gotten from: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/uart.html

	ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
	if (_uart_diver_installed == true)
	{
		ESP_LOGD(TAG, "baud rate changed");
		return;
	}

	ESP_ERROR_CHECK(uart_set_pin(uart_num, tx_io_num, rx_io_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	/*
		ESP_ERROR_CHECK(uart_driver_install(uart_num,
											CONFIG_NEX_UART_RECV_BUFFER_SIZE, // Receive buffer size.
											0,								  // Transmit buffer size.
											10,								  // Queue size.
											NULL,							  // Queue pointer.
											0));							  // Allocation flags.
		*/
	ESP_LOGD(TAG, "driver installed");
	_uart_diver_installed = true;
}

std::string ESPNexUpload::str_snprintf(const char *fmt, size_t len, ...)
{
	std::string str;
	va_list args;

	str.resize(len);
	va_start(args, len);
	size_t out_length = vsnprintf(&str[0], len + 1, fmt, args);
	va_end(args);

	if (out_length < len)
		str.resize(out_length);

	return str;
}
char ESPNexUpload::format_hex_pretty_char(uint8_t v) { return v >= 10 ? 'A' + (v - 10) : '0' + v; }
std::string ESPNexUpload::format_hex_pretty(const uint8_t *data, size_t length)
{
	if (length == 0)
		return "";
	std::string ret;
	ret.resize(3 * length - 1);
	for (size_t i = 0; i < length; i++)
	{
		ret[3 * i] = format_hex_pretty_char((data[i] & 0xF0) >> 4);
		ret[3 * i + 1] = format_hex_pretty_char(data[i] & 0x0F);
		if (i != length - 1)
			ret[3 * i + 2] = '.';
	}
	if (length > 4)
		return ret + " (" + str_snprintf("%u", 32, length) + ")";
	return ret;
}