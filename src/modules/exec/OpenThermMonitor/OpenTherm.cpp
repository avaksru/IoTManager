/*
OpenTherm.cpp - OpenTherm Communication Library For Arduino, ESP8266, ESP32
Copyright 2023, Ihor Melnyk
*/

#include "OpenTherm.h"
#if !defined(__AVR__)
#include "FunctionalInterrupt.h"
#endif

OpenTherm::OpenTherm(int inPin, int outPin, bool isSlave) : status(OpenThermStatus::NOT_INITIALIZED),
															inPin(inPin),
															outPin(outPin),
															isSlave(isSlave),
															response(0),
															responseStatus(OpenThermResponseStatus::NONE),
															responseTimestamp(0),
															processResponseCallback(NULL)
{
	imitFlag = false;
}

void OpenTherm::begin(void (*handleInterruptCallback)(void))
{
	pinMode(inPin, INPUT);
	pinMode(outPin, OUTPUT);
	if (handleInterruptCallback != NULL)
	{
		this->handleInterruptCallback = handleInterruptCallback;
		attachInterrupt(digitalPinToInterrupt(inPin), handleInterruptCallback, CHANGE);
	}
	else
	{
#if !defined(__AVR__)
		attachInterruptArg(
			digitalPinToInterrupt(inPin),
			OpenTherm::handleInterruptHelper,
			this,
			CHANGE);
#endif
	}
	activateBoiler();
	status = OpenThermStatus::READY;
	this->processResponseCallback = processResponseCallback;
}

void OpenTherm::begin(void (*handleInterruptCallback)(void), void (*processResponseCallback)(unsigned long, OpenThermResponseStatus))
{
	begin(handleInterruptCallback);
	this->processResponseCallback = processResponseCallback;
}

#if !defined(__AVR__)
void OpenTherm::begin()
{
	begin(NULL);
}

void OpenTherm::begin(std::function<void(unsigned long, OpenThermResponseStatus)> processResponseFunction)
{
	begin();
	this->processResponseFunction = processResponseFunction;
}
#endif

bool IRAM_ATTR OpenTherm::isReady()
{
	return status == OpenThermStatus::READY;
}

int IRAM_ATTR OpenTherm::readState()
{
	return digitalRead(inPin);
}

void OpenTherm::setActiveState()
{
	digitalWrite(outPin, LOW);
}

void OpenTherm::setIdleState()
{
	digitalWrite(outPin, HIGH);
}

void OpenTherm::activateBoiler()
{
	setIdleState();
	delay(1000);
}

void OpenTherm::sendBit(bool high)
{
	if (high)
		setActiveState();
	else
		setIdleState();
	delayMicroseconds(500);
	if (high)
		setIdleState();
	else
		setActiveState();
	delayMicroseconds(500);
}

bool OpenTherm::sendRequestAsync(unsigned long request)
{
	noInterrupts();
	const bool ready = isReady();

	if (!ready)
	{
		interrupts();
		return false;
	}

	status = OpenThermStatus::REQUEST_SENDING;
	response = 0;
	responseStatus = OpenThermResponseStatus::NONE;

#ifdef INC_FREERTOS_H
	BaseType_t schedulerState = xTaskGetSchedulerState();
	if (schedulerState == taskSCHEDULER_RUNNING)
	{
		vTaskSuspendAll();
	}
#endif

	interrupts();

	sendBit(HIGH); // start bit
	for (int i = 31; i >= 0; i--)
	{
		sendBit(bitRead(request, i));
	}
	sendBit(HIGH); // stop bit
	setIdleState();

	responseTimestamp = micros();
	status = OpenThermStatus::RESPONSE_WAITING;

#ifdef INC_FREERTOS_H
	if (schedulerState == taskSCHEDULER_RUNNING)
	{
		xTaskResumeAll();
	}
#endif
	if (imitFlag)
		ImitationResponse(request);
	return true;
}

unsigned long OpenTherm::sendRequest(unsigned long request)
{
	if (!sendRequestAsync(request))
	{
		return 0;
	}

	while (!isReady())
	{
		process();
		yield();
	}
	return response;
}

bool OpenTherm::sendResponse(unsigned long request)
{
	noInterrupts();
	const bool ready = isReady();

	if (!ready)
	{
		interrupts();
		return false;
	}

	status = OpenThermStatus::REQUEST_SENDING;
	response = 0;
	responseStatus = OpenThermResponseStatus::NONE;

#ifdef INC_FREERTOS_H
	BaseType_t schedulerState = xTaskGetSchedulerState();
	if (schedulerState == taskSCHEDULER_RUNNING)
	{
		vTaskSuspendAll();
	}
#endif

	interrupts();

	sendBit(HIGH); // start bit
	for (int i = 31; i >= 0; i--)
	{
		sendBit(bitRead(request, i));
	}
	sendBit(HIGH); // stop bit
	setIdleState();
	status = OpenThermStatus::READY;

#ifdef INC_FREERTOS_H
	if (schedulerState == taskSCHEDULER_RUNNING)
	{
		xTaskResumeAll();
	}
#endif

	return true;
}

unsigned long OpenTherm::getLastResponse()
{
	return response;
}

OpenThermResponseStatus OpenTherm::getLastResponseStatus()
{
	return responseStatus;
}

void IRAM_ATTR OpenTherm::handleInterrupt()
{
	if (isReady())
	{
		if (isSlave && readState() == HIGH)
		{
			status = OpenThermStatus::RESPONSE_WAITING;
		}
		else
		{
			return;
		}
	}

	unsigned long newTs = micros();
	if (status == OpenThermStatus::RESPONSE_WAITING)
	{
		if (readState() == HIGH)
		{
			status = OpenThermStatus::RESPONSE_START_BIT;
			responseTimestamp = newTs;
		}
		else
		{
			status = OpenThermStatus::RESPONSE_INVALID;
			responseTimestamp = newTs;
		}
	}
	else if (status == OpenThermStatus::RESPONSE_START_BIT)
	{
		if ((newTs - responseTimestamp < 750) && readState() == LOW)
		{
			status = OpenThermStatus::RESPONSE_RECEIVING;
			responseTimestamp = newTs;
			responseBitIndex = 0;
		}
		else
		{
			status = OpenThermStatus::RESPONSE_INVALID;
			responseTimestamp = newTs;
		}
	}
	else if (status == OpenThermStatus::RESPONSE_RECEIVING)
	{
		if ((newTs - responseTimestamp) > 750)
		{
			if (responseBitIndex < 32)
			{
				response = (response << 1) | !readState();
				responseTimestamp = newTs;
				responseBitIndex = responseBitIndex + 1;
			}
			else
			{ // stop bit
				status = OpenThermStatus::RESPONSE_READY;
				responseTimestamp = newTs;
			}
		}
	}
}

#if !defined(__AVR__)
void IRAM_ATTR OpenTherm::handleInterruptHelper(void *ptr)
{
	static_cast<OpenTherm *>(ptr)->handleInterrupt();
}
#endif

void OpenTherm::processResponse()
{
	if (processResponseCallback != NULL)
	{
		processResponseCallback(response, responseStatus);
	}
#if !defined(__AVR__)
	if (this->processResponseFunction != NULL)
	{
		processResponseFunction(response, responseStatus);
	}
#endif
}

void OpenTherm::process()
{
	noInterrupts();
	OpenThermStatus st = status;
	unsigned long ts = responseTimestamp;
	interrupts();

	if (st == OpenThermStatus::READY)
		return;
	unsigned long newTs = micros();
	if (st != OpenThermStatus::NOT_INITIALIZED && st != OpenThermStatus::DELAY && (newTs - ts) > 1000000)
	{
		status = OpenThermStatus::READY;
		responseStatus = OpenThermResponseStatus::TIMEOUT;
		processResponse();
	}
	else if (st == OpenThermStatus::RESPONSE_INVALID)
	{
		status = OpenThermStatus::DELAY;
		responseStatus = OpenThermResponseStatus::INVALID;
		processResponse();
	}
	else if (st == OpenThermStatus::RESPONSE_READY)
	{
		status = OpenThermStatus::DELAY;
		responseStatus = (isSlave ? isValidRequest(response) : isValidResponse(response)) ? OpenThermResponseStatus::SUCCESS : OpenThermResponseStatus::INVALID;
		processResponse();
	}
	else if (st == OpenThermStatus::DELAY)
	{
		if ((newTs - ts) > (isSlave ? 20000 : 100000))
		{
			status = OpenThermStatus::READY;
		}
	}
}

bool OpenTherm::parity(unsigned long frame) // odd parity
{
	byte p = 0;
	while (frame > 0)
	{
		if (frame & 1)
			p++;
		frame = frame >> 1;
	}
	return (p & 1);
}

OpenThermMessageType OpenTherm::getMessageType(unsigned long message)
{
	OpenThermMessageType msg_type = static_cast<OpenThermMessageType>((message >> 28) & 7);
	return msg_type;
}

OpenThermMessageID OpenTherm::getDataID(unsigned long frame)
{
	return (OpenThermMessageID)((frame >> 16) & 0xFF);
}

unsigned long OpenTherm::buildRequest(OpenThermMessageType type, OpenThermMessageID id, unsigned int data)
{
	unsigned long request = data;
	if (type == OpenThermMessageType::WRITE_DATA)
	{
		request |= 1ul << 28;
	}
	request |= ((unsigned long)id) << 16;
	if (parity(request))
		request |= (1ul << 31);
	return request;
}
unsigned long OpenTherm::buildRequestID(OpenThermMessageType type, unsigned int id, unsigned int data)
{
	unsigned long request = data;
	if (type == OpenThermMessageType::WRITE_DATA)
	{
		request |= 1ul << 28;
	}
	request |= ((unsigned long)id) << 16;
	if (parity(request))
		request |= (1ul << 31);
	return request;
}

unsigned long OpenTherm::buildResponse(OpenThermMessageType type, OpenThermMessageID id, unsigned int data)
{
	unsigned long response = data;
	response |= ((unsigned long)type) << 28;
	response |= ((unsigned long)id) << 16;
	if (parity(response))
		response |= (1ul << 31);
	return response;
}

bool OpenTherm::isValidResponse(unsigned long response)
{
	if (parity(response))
		return false;
	byte msgType = (response << 1) >> 29;
	return msgType == (byte)OpenThermMessageType::READ_ACK || msgType == (byte)OpenThermMessageType::WRITE_ACK;
}

bool OpenTherm::isValidResponseID(unsigned long response, OpenThermMessageID id)
{
	byte responseId = (response >> 16) & 0xFF;
	return (byte)id == responseId;
}

bool OpenTherm::isValidRequest(unsigned long request)
{
	if (parity(request))
		return false;
	byte msgType = (request << 1) >> 29;
	return msgType == (byte)OpenThermMessageType::READ_DATA || msgType == (byte)OpenThermMessageType::WRITE_DATA;
}

void OpenTherm::end()
{
	detachInterrupt(digitalPinToInterrupt(inPin));
}

OpenTherm::~OpenTherm()
{
	end();
}

const char *OpenTherm::statusToString(OpenThermResponseStatus status)
{
	switch (status)
	{
	case OpenThermResponseStatus::NONE:
		return "NONE";
	case OpenThermResponseStatus::SUCCESS:
		return "SUCCESS";
	case OpenThermResponseStatus::INVALID:
		return "INVALID";
	case OpenThermResponseStatus::TIMEOUT:
		return "TIMEOUT";
	default:
		return "UNKNOWN";
	}
}

const char *OpenTherm::messageTypeToString(OpenThermMessageType message_type)
{
	switch (message_type)
	{
	case OpenThermMessageType::READ_DATA:
		return "READ_DATA";
	case OpenThermMessageType::WRITE_DATA:
		return "WRITE_DATA";
	case OpenThermMessageType::INVALID_DATA:
		return "INVALID_DATA";
	case OpenThermMessageType::RESERVED:
		return "RESERVED";
	case OpenThermMessageType::READ_ACK:
		return "READ_ACK";
	case OpenThermMessageType::WRITE_ACK:
		return "WRITE_ACK";
	case OpenThermMessageType::DATA_INVALID:
		return "DATA_INVALID";
	case OpenThermMessageType::UNKNOWN_DATA_ID:
		return "UNKNOWN_DATA_ID";
	default:
		return "UNKNOWN";
	}
}

// building requests

unsigned long OpenTherm::buildSetBoilerStatusRequest(bool enableCentralHeating, bool enableHotWater, bool enableCooling, bool enableOutsideTemperatureCompensation, bool enableCentralHeating2)
{
	unsigned int data = enableCentralHeating | (enableHotWater << 1) | (enableCooling << 2) | (enableOutsideTemperatureCompensation << 3) | (enableCentralHeating2 << 4);
	data <<= 8;
	return buildRequest(OpenThermMessageType::READ_DATA, OpenThermMessageID::Status, data);
}

unsigned long OpenTherm::buildSetBoilerTemperatureRequest(float temperature)
{
	unsigned int data = temperatureToData(temperature);
	return buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TSet, data);
}

unsigned long OpenTherm::buildGetBoilerTemperatureRequest()
{
	return buildRequest(OpenThermMessageType::READ_DATA, OpenThermMessageID::Tboiler, 0);
}

// parsing responses
bool OpenTherm::isFault(unsigned long response)
{
	return response & 0x1;
}

bool OpenTherm::isCentralHeatingActive(unsigned long response)
{
	return response & 0x2;
}

bool OpenTherm::isHotWaterActive(unsigned long response)
{
	return response & 0x4;
}

bool OpenTherm::isFlameOn(unsigned long response)
{
	return response & 0x8;
}

bool OpenTherm::isCoolingActive(unsigned long response)
{
	return response & 0x10;
}

bool OpenTherm::isDiagnostic(unsigned long response)
{
	return response & 0x40;
}

uint16_t OpenTherm::getUInt(const unsigned long response)
{
	const uint16_t u88 = response & 0xffff;
	return u88;
}

float OpenTherm::getFloat(const unsigned long response)
{
	const uint16_t u88 = getUInt(response);
	const float f = (u88 & 0x8000) ? -(0x10000L - u88) / 256.0f : u88 / 256.0f;
	return f;
}

unsigned int OpenTherm::temperatureToData(float temperature)
{
	if (temperature < 0)
		temperature = 0;
	if (temperature > 100)
		temperature = 100;
	unsigned int data = (unsigned int)(temperature * 256);
	return data;
}

// basic requests

unsigned long OpenTherm::setBoilerStatus(bool enableCentralHeating, bool enableHotWater, bool enableCooling, bool enableOutsideTemperatureCompensation, bool enableCentralHeating2)
{
	return sendRequest(buildSetBoilerStatusRequest(enableCentralHeating, enableHotWater, enableCooling, enableOutsideTemperatureCompensation, enableCentralHeating2));
}

bool OpenTherm::setBoilerTemperature(float temperature)
{
	unsigned long response = sendRequest(buildSetBoilerTemperatureRequest(temperature));
	return isValidResponse(response);
}

float OpenTherm::getBoilerTemperature()
{
	unsigned long response = sendRequest(buildGetBoilerTemperatureRequest());
	return isValidResponse(response) ? getFloat(response) : 0;
}

float OpenTherm::getReturnTemperature()
{
	unsigned long response = sendRequest(buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tret, 0));
	return isValidResponse(response) ? getFloat(response) : 0;
}

bool OpenTherm::setDHWSetpoint(float temperature)
{
	unsigned int data = temperatureToData(temperature);
	unsigned long response = sendRequest(buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TdhwSet, data));
	return isValidResponse(response);
}

float OpenTherm::getDHWTemperature()
{
	unsigned long response = sendRequest(buildRequest(OpenThermMessageType::READ_DATA, OpenThermMessageID::Tdhw, 0));
	return isValidResponse(response) ? getFloat(response) : 0;
}

float OpenTherm::getModulation()
{
	unsigned long response = sendRequest(buildRequest(OpenThermRequestType::READ, OpenThermMessageID::RelModLevel, 0));
	return isValidResponse(response) ? getFloat(response) : 0;
}

float OpenTherm::getPressure()
{
	unsigned long response = sendRequest(buildRequest(OpenThermRequestType::READ, OpenThermMessageID::CHPressure, 0));
	return isValidResponse(response) ? getFloat(response) : 0;
}

unsigned char OpenTherm::getFault()
{
	return ((sendRequest(buildRequest(OpenThermRequestType::READ, OpenThermMessageID::ASFflags, 0)) >> 8) & 0xff);
}
int8_t flame_timer = 0;
void OpenTherm::ImitationResponse(unsigned long request)
{

	//	unsigned long response;
	unsigned int data = getUInt(request);
	OpenThermMessageType msgType;
	byte ID;
	OpenThermMessageID id = getDataID(request);
	uint8_t flags;

	switch (id)
	{
	case OpenThermMessageID::Status:
		// Статус котла получен
		msgType = OpenThermMessageType::READ_ACK;
		static int8_t flame = 0;
		flame_timer++;
		if (flame_timer > 10)
			flame = 1;
		if (flame_timer > 20)
		{
			flame_timer = 0;
			flame = 0;
		}
		static int8_t fault = 0;
		// fault = 1 - fault;
		data = (bool)fault | (true << 1) | (true << 2) | ((bool)flame << 3) | (false << 4);
		break;
	case OpenThermMessageID::SConfigSMemberIDcode:
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::SlaveVersion:
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::MasterVersion:
		msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::RelModLevel:
		static float RelModLevel = 10;
		// RelModLevel = RelModLevel > 100 ? 10 : RelModLevel + 1;
		if (flame_timer < 11)
		{
			RelModLevel = 0;
		}
		else
		{
			RelModLevel = RelModLevel == 0 ? 10 : RelModLevel + 1;
		}
		// data = RelModLevel;
		data = temperatureToData(RelModLevel);
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::Tboiler:
		// Получили температуру котла
		static float Tboiler = 40;
		Tboiler = Tboiler > 60 ? 40 : Tboiler + 1;
		data = temperatureToData(Tboiler);
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::Tdhw:
		// Получили температуру ГВС
		static float Tdhw = 60;
		Tdhw = Tdhw > 80 ? 60 : Tdhw + 1;
		data = temperatureToData(Tdhw);
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::Toutside:
		// Получили внешнюю температуру
		static float Toutside = -10;
		Toutside = Toutside > 10 ? -10 : Toutside + 1;
		data = temperatureToData(Toutside);
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::ASFflags:
		msgType = OpenThermMessageType::READ_ACK;
		break;

	case OpenThermMessageID::TdhwSetUBTdhwSetLB:
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::MaxTSetUBMaxTSetLB:
		msgType = OpenThermMessageType::READ_ACK;
		break;

	case OpenThermMessageID::OEMDiagnosticCode:
		msgType = OpenThermMessageType::READ_ACK;
		break;

	case OpenThermMessageID::OpenThermVersionSlave:
		msgType = OpenThermMessageType::READ_ACK;
		break;

	case OpenThermMessageID::CHPressure:
		msgType = OpenThermMessageType::READ_ACK;
		break;

		break;
	case OpenThermMessageID::DHWFlowRate:
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::DayTime:
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::Date:
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::Year:
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;

	case OpenThermMessageID::Tret:
		//
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::Tstorage:
		//
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::Tcollector:
		//
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::TflowCH2:
		//
		msgType = OpenThermMessageType::READ_ACK;

		break;
	case OpenThermMessageID::Tdhw2:
		//
		msgType = OpenThermMessageType::READ_ACK;

		break;
	case OpenThermMessageID::Texhaust:
		//
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::TboilerHeatExchanger:
		//
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::BoilerFanSpeedSetpointAndActual:
		//
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::FlameCurrent:
		//
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::SuccessfulBurnerStarts:
		//
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::CHPumpStarts:
		//
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::DHWPumpValveStarts:
		//
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::DHWBurnerStarts:
		//
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::BurnerOperationHours:
		//
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::CHPumpOperationHours:
		//
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::DHWPumpValveOperationHours:
		//
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::DHWBurnerOperationHours:
		//
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::RBPflags:
		//
		// Pre-Defined Remote Boiler Parameters
		//
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::TdhwSet:
		//
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::TSet:
		//
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::MaxTSet:
		//
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::Hcratio:
		//
		if (getMessageType(request) == OpenThermMessageType::READ_DATA)
			msgType = OpenThermMessageType::READ_ACK;
		else
			msgType = OpenThermMessageType::WRITE_ACK;
		break;
	case OpenThermMessageID::TSP:
		//
		// Transparent Slave Parameters
		//
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::FHBsize:
		//
		// Fault History Data
		//
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::MaxCapacityMinModLevel:
		//
		// Boiler Sequencer Control
		//
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::TrOverride:
		//
		// Remote override room setpoint
		//
		msgType = OpenThermMessageType::READ_ACK;
		break;
	case OpenThermMessageID::RemoteOverrideFunction:
		msgType = OpenThermMessageType::READ_ACK;
		break;

	default:
		msgType = OpenThermMessageType::UNKNOWN_DATA_ID;
		break;
	}
	response = buildResponse(msgType, id, data);
	status = OpenThermStatus::RESPONSE_READY;
	responseStatus = OpenThermResponseStatus::SUCCESS;
	/*
	if (processResponseCallback != NULL)
	{
		processResponseCallback(response, OpenThermResponseStatus::SUCCESS);
	}
	*/
}