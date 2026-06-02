#include "Global.h"
#include "classes/IoTItem.h"
#include "GyverPID.h"

extern IoTGpio IoTgpio;

class ThermostatGIST : public IoTItem
{
private:
    String _set_id;         // заданная температура
    String _term_id;        // термометр
    String _term_rezerv_id; // резервный термометр
    String _rele;           // реле
    float pv_last = 0;      // предыдущая температура
    float _gist = 1;        // гистерис
    float sp, pv, pv2;
    String interim;
    int enable = 1;
    int _direction = 0;

public:
    ThermostatGIST(String parameters) : IoTItem(parameters)
    {
        jsonRead(parameters, "set_id", _set_id);
        jsonRead(parameters, "term_id", _term_id);
        jsonRead(parameters, "term_rezerv_id", _term_rezerv_id);
        jsonRead(parameters, "gist", _gist);
        jsonRead(parameters, "rele", _rele);
        jsonRead(parameters, "direction", _direction);
    }

    void doByInterval()
    {
        // заданная температура
        IoTItem *tmp = findIoTItem(_set_id);
        if (tmp)
        {
            interim = tmp->getValue();
            sp = ::atof(interim.c_str());
        }
        // термометр
        tmp = findIoTItem(_term_id);
        if (tmp)
        {
            interim = tmp->getValue();
            pv = ::atof(interim.c_str());
        }
        if (sp && _rele != "")
        {
            if (pv > -40 && pv < 120 && pv)
            {
                if (enable)
                {
                    setValue("штатный режим");
                }
                // работаем по основному датчику
                if (pv >= sp + _gist - 0.0001 && enable)
                {
                    // SerialPrint("i", F("ThermostatPID"), "temp: " + String(pv) + " >= setpoint: " + String(sp + _gist));
                    tmp = findIoTItem(_rele);
                    if (tmp)
                    {
                        if (_direction)
                        {
                            tmp->setValue("0", true);
                        }
                        else
                        {
                            tmp->setValue("1", true);
                        }
                    }
                }
                if (pv <= sp - _gist + 0.0001 && enable)
                {
                    // SerialPrint("i", F("ThermostatPID"), "temp: " + String(pv) + " <= setpoint: " + String(sp - _gist));
                    tmp = findIoTItem(_rele);
                    if (tmp)
                    {
                        if (_direction)
                        {
                            tmp->setValue("1", true);
                        }
                        else
                        {
                            tmp->setValue("0", true);
                        }
                    }
                }
            }
            else
            {
                // резервный термометр
                if (_term_rezerv_id != "")
                {
                    tmp = findIoTItem(_term_rezerv_id);
                    if (tmp)
                    {
                        interim = tmp->getValue();
                        pv2 = ::atof(interim.c_str());
                    }
                    // работаем по резервному датчику
                    if (pv2 > -40 && pv2 < 120 && pv2)
                    {
                        if (enable)
                        {
                            setValue("резервный датчик");
                        }
                        if (pv2 >= sp + _gist - 0.0001 && enable)
                        {
                            tmp = findIoTItem(_rele);
                            if (tmp)
                            {
                                if (_direction)
                                {
                                    tmp->setValue("0", true);
                                }
                                else
                                {
                                    tmp->setValue("1", true);
                                }
                            }
                        }
                        if (pv2 <= sp - _gist + 0.0001 && enable)
                        {
                            tmp = findIoTItem(_rele);
                            if (tmp)
                            {
                                if (_direction)
                                {
                                    tmp->setValue("1", true);
                                }
                                else
                                {
                                    tmp->setValue("0", true);
                                }
                            }
                        }
                    }
                    else
                    {
                        if (enable)
                        {
                            setValue("ошибка резервного датчика");
                        }
                    }
                }
                else
                {
                    if (enable)
                    {
                        setValue("ошибка датчика температуры");
                    }
                }
            }
        }
        else
        {
            // если не заполнены настройки термостата
            setValue("ошибка настройки термостата");
        }

        pv_last = pv;
    }

    IoTValue execute(String command, std::vector<IoTValue> &param)
    {
        if (param.size() == 1)
        {
            if (command == "enable")
            {
                if (param.size())
                {
                    enable = param[0].valD;
                    if (enable)
                    {
                        setValue("включен");
                    }
                    else
                    {
                        setValue("выключен");
                    }
                }
            }
            if (command == "setDirection")
            {
                if (param.size())
                {
                    _direction = param[0].valD;
                }
            }
        }
        return {};
    }

    ~ThermostatGIST() {};
};

GyverPID *regulator = nullptr;
GyverPID *instanceregulator(float _KP, float _KI, float _KD, int interval, boolean setDirection, int setLimitsMIN, int setLimitsMAX)
{
    if (!regulator)
    {                                                      // Если библиотека ранее инициализировалась, то просто вернем указатель
                                                           // Инициализируем библиотеку
        regulator = new GyverPID(_KP, _KI, _KD, interval); // коэф. П, коэф. И, коэф. Д, период дискретизации dt (с)
        regulator->setDirection(setDirection);             // направление регулирования (NORMAL/REVERSE). ПО УМОЛЧАНИЮ СТОИТ NORMAL
        regulator->setLimits(setLimitsMIN, setLimitsMAX);  // пределы. ПО УМОЛЧАНИЮ СТОЯТ 0 И 100
        SerialPrint("i", F("ThermostatPID"), " _KP:" + String(_KP) + " _KI:" + String(_KI) + " _KD:" + String(_KD) + " interval:" + String(interval) + " _setLimitsMIN:" + String(setLimitsMIN) + " _setLimitsMAX:" + String(setLimitsMAX) + " Direction:" + String(setDirection));
        // GyverPID regulator(_KP, _KI, _KD, interval);
    }
    return regulator;
}

class ThermostatPID : public IoTItem
{
private:
    String _set_id;  // заданная температура
    String _term_id; // термометр
    boolean _setDirection;

    float _int, _KP, _KI, _KD,
        sp, pv,
        pv_last = 0, // предыдущая температура
        ierr = 0,    // интегральная погрешность
        dt = 0;      // время между измерениями
    String _rele;    // реле
    String interim;
    int enable = 1;
    int interval, _setLimitsMIN, _setLimitsMAX;
    IoTItem *tmp;
    int releState = 0;

public:
    ThermostatPID(String parameters) : IoTItem(parameters)
    {
        jsonRead(parameters, "set_id", _set_id);
        jsonRead(parameters, "term_id", _term_id);
        jsonRead(parameters, "int", _int);
        jsonRead(parameters, "KP", _KP);
        jsonRead(parameters, "KI", _KI);
        jsonRead(parameters, "KD", _KD);
        jsonRead(parameters, F("int"), interval);
        jsonRead(parameters, "rele", _rele);

        // GyverPID
        jsonRead(parameters, "setDirection", _setDirection);
        jsonRead(parameters, "setLimitsMIN", _setLimitsMIN);
        jsonRead(parameters, "setLimitsMAX", _setLimitsMAX);

       
    }

protected:
   
    void
    doByInterval()
    {
        // заданная температура
        IoTItem *tmp = findIoTItem(_set_id);
        if (tmp)
        {
            interim = tmp->getValue();
            sp = ::atof(interim.c_str());
        }
        // термометр
        tmp = findIoTItem(_term_id);
        if (tmp)
        {
            interim = tmp->getValue();
            pv = ::atof(interim.c_str());
        }
        if (enable)
        {
          
            instanceregulator(_KP, _KI, _KD, interval, _setDirection, _setLimitsMIN, _setLimitsMAX)->setpoint = sp;
            instanceregulator(_KP, _KI, _KD, interval, _setDirection, _setLimitsMIN, _setLimitsMAX)->input = pv;
            value.valD = instanceregulator(_KP, _KI, _KD, interval, _setDirection, _setLimitsMIN, _setLimitsMAX)->getResult();
            SerialPrint("i", F("ThermostatPID"), " _KP:" + String(_KP) + " _KI:" + String(_KI) + " _KD:" + String(_KD) + " interval:" + String(interval) + " _setLimitsMIN:" + String(_setLimitsMIN) + " _setLimitsMAX:" + String(_setLimitsMAX) + " Direction:" + String(_setDirection));
            SerialPrint("i", F("ThermostatPID"), "setpoint: " + String(sp) + " input: " + String(pv));
            regEvent(value.valD, "ThermostatPID", false, true);
        }
        else
        {
            value.valD = 0;
            regEvent(value.valD, "ThermostatPID", false, true);
        }
        pv_last = pv;
    }

    // временное решение
    unsigned long currentMillis;
    unsigned long prevMillis;
    unsigned long difference;

    void loop()
    {
        if (enableDoByInt)
        {
            currentMillis = millis();
            difference = currentMillis - prevMillis;

            if (_rele != "" && enable && value.valD * interval / 100 > difference / 1000 && releState == 0)
            {
                releState = 1;
                tmp = findIoTItem(_rele);
                if (tmp)
                    tmp->setValue("1", true);
            }
            if (_rele != "" && enable && value.valD * interval / 100 < difference / 1000 && releState == 1)
            {
                releState = 0;
                tmp = findIoTItem(_rele);
                if (tmp)
                    tmp->setValue("0", true);
            }

            if (difference >= interval * 1000)
            {
                prevMillis = millis();
                this->doByInterval();
            }
        }
    }
    IoTValue execute(String command, std::vector<IoTValue> &param)
    {
        if (param.size() == 1)
        {
            if (command == "enable")
            {
                if (param.size())
                {
                    enable = param[0].valD;
                    if (enable == 0)
                    {
                        delete regulator;
                        regulator = nullptr;
                        //    instanceregulator(_KP, _KI, _KD, interval, _setDirection, _setLimitsMIN, _setLimitsMAX);
                    }
                }
            }
            if (command == "setLimitsMIN")
            {
                if (param.size())
                {
                    _setLimitsMIN = param[0].valD;
                    if (regulator)
                    {
                        regulator->setLimits(_setLimitsMIN, _setLimitsMAX);
                        regulator->integral = constrain(regulator->integral, _setLimitsMIN, _setLimitsMAX);
                    }
                }
            }
            if (command == "setLimitsMAX")
            {
                if (param.size())
                {
                    _setLimitsMAX = param[0].valD;
                    if (regulator)
                    {
                        regulator->setLimits(_setLimitsMIN, _setLimitsMAX);
                        regulator->integral = constrain(regulator->integral, _setLimitsMIN, _setLimitsMAX);
                    }
                }
            }
            if (command == "KP")
            {
                if (param.size())
                {
                    _KP = param[0].valD;
                    if (regulator)
                    {
                        regulator->Kp = _KP;
                    }
                    else
                    {
                        instanceregulator(_KP, _KI, _KD, interval, _setDirection, _setLimitsMIN, _setLimitsMAX);
                    }
                }
            }
            if (command == "KI")
            {
                if (param.size())
                {
                    _KI = param[0].valD;
                    if (regulator)
                    {
                        float oldKi = regulator->Ki;
                        float oldIntegral = regulator->integral;
                        // Preserve current controller output to avoid abrupt jumps when changing Ki.
                        // Compute non-integral part (P + D). We cannot access prevInput (D term) so use just P-term.
                        float nonI = (regulator->setpoint - regulator->input) * regulator->Kp;
                        float desiredIntegral = regulator->output - nonI;
                        if (!isfinite(desiredIntegral)) desiredIntegral = 0;
                        regulator->integral = constrain(desiredIntegral, _setLimitsMIN, _setLimitsMAX);
                        regulator->Ki = _KI;
                        SerialPrint("i", F("ThermostatPID"), "KI changed from " + String(oldKi) + " to " + String(_KI) + ", integral: " + String(oldIntegral) + " -> " + String(regulator->integral));
                    }
                    else
                    {
                        instanceregulator(_KP, _KI, _KD, interval, _setDirection, _setLimitsMIN, _setLimitsMAX);
                    }
                }
            }
            if (command == "KD")
            {
                if (param.size())
                {
                    _KD = param[0].valD;
                    if (regulator)
                    {
                        regulator->Kd = _KD;
                    }
                    else
                    {
                        instanceregulator(_KP, _KI, _KD, interval, _setDirection, _setLimitsMIN, _setLimitsMAX);
                    }
                }
            }

            if (command == "setDirection")
            {
                if (param.size())
                {
                    _setDirection = param[0].valD;
                    if (regulator)
                    {
                        regulator->setDirection(_setDirection);
                        // invert integral when direction flips to preserve control state
                        regulator->integral = -regulator->integral;
                    }
                    else
                    {
                        instanceregulator(_KP, _KI, _KD, interval, _setDirection, _setLimitsMIN, _setLimitsMAX);
                    }
                }
            }
        }
        return {};
    }
    ~ThermostatPID()
    {
        delete regulator;
        regulator = nullptr;
    };
};

class ThermostatETK : public IoTItem
{
private:
    float pv, sp, outside_temp;
    float _iv_k;        // эквитермические кривые
    String _set_id;     // заданная температура
    String _term_id;    // термометр
    String _outside_id; // уличный термометр
    String interim;
    int enable = 1;

public:
    ThermostatETK(String parameters) : IoTItem(parameters)
    {
        //        jsonRead(parameters, "set_id", _set_id);
        //        jsonRead(parameters, "term_id", _term_id);
        jsonRead(parameters, "iv_k", _iv_k);
        jsonRead(parameters, "outside_id", _outside_id);
    }

protected:
    //===================================================================================================================
    //       Вычисляем температуру контура отпления, эквитермические кривые
    //===================================================================================================================
    float curve(float iv_k, float outside_temp)
    {
        float a = (-0.21 * iv_k) - 0.06;          // a = -0,21k — 0,06
        float b = (6.04 * iv_k) + 1.98;           // b = 6,04k + 1,98
        float c = (-5.06 * iv_k) + 18.06;         // с = -5,06k + 18,06
        float x = (-0.2 * outside_temp) + 5;      // x = -0.2*t1 + 5
        float temp_n = (a * x * x) + (b * x) + c; // Tn = ax2 + bx + c
        // Расчетная температура конура отопления
        float op = temp_n; // T = Tn
        // Ограничиваем температуру для ID-1
        op = constrain(op, 0, 100);
        return op;
    }

    void doByInterval()
    {
        //  уличный термометр
        IoTItem *tmp = findIoTItem(_outside_id);
        if (tmp)
        {
            interim = tmp->getValue();
            outside_temp = ::atof(interim.c_str());
        }
        if (_iv_k && outside_temp)
        {

            value.valD = curve(_iv_k, outside_temp);
            regEvent(value.valD, "ThermostatETK");
        }
    }
    IoTValue execute(String command, std::vector<IoTValue> &param)
    {
        if (param.size() == 1)
        {
            if (command == "set_iv_k")
            {
                if (param.size())
                {
                    _iv_k = param[0].valD;
                }
            }
        }
        return {};
    }
    ~ThermostatETK() {};
};

class ThermostatETK2 : public IoTItem
{
private:
    float pv, sp, outside_temp;
    float _iv_k;        // эквитермические кривые
    String _set_id;     // заданная температура
    String _term_id;    // термометр
    String _outside_id; // уличный термометр
    String interim;
    int enable = 1;

public:
    ThermostatETK2(String parameters) : IoTItem(parameters)
    {
        jsonRead(parameters, "set_id", _set_id);
        jsonRead(parameters, "term_id", _term_id);
        jsonRead(parameters, "iv_k", _iv_k);
        jsonRead(parameters, "outside_id", _outside_id);
    }

protected:
    //===================================================================================================================
    //       Вычисляем температуру контура отпления, эквитермические кривые с учётом влияния температуры в помещении
    //===================================================================================================================
    float curve2(float sp, float pv, float iv_k, float outside_temp)
    {
        // Расчет поправки (ошибки) термостата
        float error = sp - pv; // Tt = (Tu — T2) × 5
        float temp_t = error * 3.0;
        // Поправка на желаемую комнатную температуру
        // Температура контура отопления в зависимости от наружной температуры
        float a = (-0.21 * iv_k) - 0.06;          // a = -0,21k — 0,06
        float b = (6.04 * iv_k) + 1.98;           // b = 6,04k + 1,98
        float c = (-5.06 * iv_k) + 18.06;         // с = -5,06k + 18,06
        float x = (-0.2 * outside_temp) + 5;      // x = -0.2*t1 + 5
        float temp_n = (a * x * x) + (b * x) + c; // Tn = ax2 + bx + c
        // Расчетная температура конура отопления
        float op = temp_n + temp_t; // T = Tn + Tk + Tt
        // Ограничиваем температуру для ID-1
        op = constrain(op, 0, 100);
        return op;
    }

    void doByInterval()
    {
        // заданная температура
        IoTItem *tmp = findIoTItem(_set_id);
        if (tmp)
        {
            interim = tmp->getValue();
            sp = ::atof(interim.c_str());
        }
        // термометр
        tmp = findIoTItem(_term_id);
        if (tmp)
        {
            interim = tmp->getValue();
            pv = ::atof(interim.c_str());
        }
        //  уличный термометр
        tmp = findIoTItem(_outside_id);
        if (tmp)
        {
            interim = tmp->getValue();
            outside_temp = ::atof(interim.c_str());
        }
        if (sp && pv && _iv_k && outside_temp)
        {
            value.valD = curve2(sp, pv, _iv_k, outside_temp);
            regEvent(value.valD, "ThermostatETK2");
        }
    }
    IoTValue execute(String command, std::vector<IoTValue> &param)
    {
        if (param.size() == 1)
        {
            if (command == "set_iv_k")
            {
                if (param.size())
                {
                    _iv_k = param[0].valD;
                }
            }
        }
        return {};
    }
    ~ThermostatETK2() {};
};

void *getAPI_Thermostat(String subtype, String param)
{
    if (subtype == F("ThermostatGIST"))
    {
        return new ThermostatGIST(param);
    }
    else if (subtype == F("ThermostatPID"))
    {
        return new ThermostatPID(param);
    }
    else if (subtype == F("ThermostatETK"))
    {
        return new ThermostatETK(param);
    }
    else if (subtype == F("ThermostatETK2"))
    {
        return new ThermostatETK2(param);
    }
    //}

    return nullptr;
}