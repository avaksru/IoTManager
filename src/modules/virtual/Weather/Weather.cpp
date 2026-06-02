#include "Global.h"
#include "classes/IoTItem.h"
#include <ArduinoJson.h>

// StaticJsonDocument<JSON_BUFFER_SIZE * 2> Weatherdoc;

extern IoTGpio IoTgpio;
class Weather : public IoTItem
{
private:
    String _location;
    String _param;
    // long interval;
    JsonDocument Weatherdoc;
    unsigned long _lastRequestMillis = 0;               // last attempt time
    unsigned long _lastSuccessMillis = 0;               // last successful update time
    bool _hasValidData = false;                         // set only after successful parse
    const unsigned long REQUEST_MIN_INTERVAL = 30000UL; // 3600000UL; // 1 hour
    unsigned long _prevWeatherMillis = 0;               // instance timer for automatic updates

public:
    Weather(String parameters) : IoTItem(parameters)
    {
        _location = jsonReadStr(parameters, "location");
        _param = jsonReadStr(parameters, "param");
        long interval;
        jsonRead(parameters, F("int"), interval); // интервал проверки погоды
        int randomAddition = random(1, 20);
        setInterval((interval * 60 * 60 + randomAddition)); // convert minutes to seconds, add random seconds
                                                            // SerialPrint("i", F("Weather"), "Initialized with location: " + _location + ", param: " + _param + ", interval: " + String(interval) + " min (+" + String(randomAddition) + " sec random)");
        // initialize instance timer to allow immediate automatic check after reinit
        _prevWeatherMillis = millis() - REQUEST_MIN_INTERVAL - 1; // make it older than 1 hour so first check runs
    }

    void getWeather()
    {
        String ret;

        // Enforce minimum interval between attempts; use last successful time if available
        unsigned long lastAnchor = (_lastSuccessMillis != 0) ? _lastSuccessMillis : _lastRequestMillis;
        if (lastAnchor != 0 && (millis() - lastAnchor < REQUEST_MIN_INTERVAL))
        {
            SerialPrint("w", F("getWeather"), "Request skipped: less than 30 min since last attempt/success");
            return;
        }

        if (!isNetworkActive())
        {
            SerialPrint("w", F("getWeather"), "Network not active, cannot request weather");
            return;
        }

        _lastRequestMillis = millis(); // record attempt time

        // Make HTTP POST to proxy with a reasonable timeout
        String payload;
        WiFiClient client;
        HTTPClient http;
        http.setTimeout(15000); // 5s timeout to avoid long blocking when server is slow
        http.begin(client, "http://live-control.com/iotm/weather.php");
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        String httpRequestData = "loc=" + _location;
        int httpResponseCode = http.POST(httpRequestData);

        if (httpResponseCode > 0)
        {
            ret = String(httpResponseCode);

            if (httpResponseCode == HTTP_CODE_OK)
            {
                payload = http.getString();

                // parse JSON only after successful HTTP_OK
                DeserializationError err = deserializeJson(Weatherdoc, payload);
                if (err == DeserializationError::Ok && !Weatherdoc["current_condition"].isNull())
                {
                    _hasValidData = true;
                    _lastSuccessMillis = millis();
                    // start the interval countdown from the last successful update
                    _lastRequestMillis = _lastSuccessMillis;

                    if (jsonReadStr(Weatherdoc["current_condition"][0], "temp_C", true) != "null")
                    {
                        if (_param == "temp_C")
                        {
                            value.valS = jsonReadStr(Weatherdoc["current_condition"][0], "temp_C", true).c_str();
                        }
                        if (_param == "avgtempC")
                        {
                            value.valS = jsonReadStr(Weatherdoc["weather"][0], "avgtempC", true).c_str();
                        }
                        if (_param == "humidity")
                        {
                            value.valS = jsonReadStr(Weatherdoc["current_condition"][0], "humidity", true).c_str();
                        }
                        if (_param == "weatherCode")
                        {
                            value.valS = jsonReadStr(Weatherdoc["current_condition"][0], "weatherCode", true).c_str();
                        }
                        if (_param == "sunrise")
                        {

                            value.valS = jsonReadStr(Weatherdoc["weather"][0]["astronomy"][0], "sunrise", true).c_str();
                        }
                        if (_param == "sunset")
                        {
                            value.valS = jsonReadStr(Weatherdoc["weather"][0]["astronomy"][0], "sunset", true).c_str();
                        }

                        if (_param == "rangetempC")
                        {
                            value.valS = (jsonReadStr(Weatherdoc["weather"][0], "mintempC", true) + "..." + jsonReadStr(Weatherdoc["weather"][0], "maxtempC", true)).c_str();
                        }

                        // погода на завтра
                        if (_param == "temp_C_tomorrow")
                        {
                            value.valS = jsonReadStr(Weatherdoc["weather"][1], "avgtempC", true).c_str();
                        }
                        if (_param == "rangetempC_tomorrow")
                        {
                            value.valS = (jsonReadStr(Weatherdoc["weather"][1], "mintempC", true) + "..." + jsonReadStr(Weatherdoc["weather"][1], "maxtempC", true)).c_str();
                        }
                        _prevWeatherMillis = millis();
                        regEvent(String(value.valS.c_str()), "Weather");
                    }
                }
                else
                {
                    _hasValidData = false;
                    SerialPrint("e", F("getWeather"), "JSON parse error: " + String(err.c_str()));
                }
            }
            else
            {
                _hasValidData = false;
            }
        }
        else
        {
            ret = http.errorToString(httpResponseCode).c_str();
            _hasValidData = false;
        }

        SerialPrint("<-", F("getWeather"), httpRequestData);
        SerialPrint("->", F("getWeather"), "server: " + ret);

        http.end();
    }

    void doByInterval()
    {

        // SerialPrint("d", F("Weather"), "doByInterval check: network " + String(isNetworkActive() ? "active" : "inactive") + ", last attempt " + String((millis() - _lastRequestMillis) / 60000) + " min ago, last success " + String((millis() - _lastSuccessMillis) / 60000) + " min ago");
        if (_prevWeatherMillis + REQUEST_MIN_INTERVAL < millis())
        {
            if (isNetworkActive())
            {
                getWeather();
            }
        }
    }

    void loop()
    {
        IoTItem::loop();
    }

    IoTValue execute(String command, std::vector<IoTValue> &param)
    {
        if (command == "get")
        {
            // Respect user rate limit: one request per hour; use last successful time if available
            unsigned long lastAnchor = (_lastSuccessMillis != 0) ? _lastSuccessMillis : _lastRequestMillis;
            if (lastAnchor != 0 && (millis() - lastAnchor < REQUEST_MIN_INTERVAL))
            {
                SerialPrint("w", F("getWeather"), "User request blocked: last request/success less than 1 hour ago");
            }
            else
            {
                if (isNetworkActive())
                {
                    getWeather();
                    // doByInterval();
                }
                else
                {
                    SerialPrint("w", F("getWeather"), "Network not active, cannot perform user request");
                }
            }
        }

        return {};
    }

    ~Weather() {};
};

void *getAPI_Weather(String subtype, String param)
{
    if (subtype == F("Weather"))
    {
        return new Weather(param);
    }
    else
    {
        return nullptr;
    }
}
