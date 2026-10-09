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

//======================================================================================================================
// Термостат Predict - предиктивный термостат на базе Гистере́зиса.
// 1) Упреждающее включение: когда температура (term_id) падает и приближается к заданной (set_id), котел (rele)
//    включается заранее, еще до того как температура стала ниже заданной. Заблаговременность (коэффициент lead_k)
//    коррелирует со скоростью падения температуры и с величиной, на которую температура упадет ниже заданной по
//    инерции, когда котел уже включился (проседание измеряется самообучением в каждом цикле).
// 2) После включения отопления ведется суммарный учет длительности работы горелки getValue(isFlameOn) в режиме CH
//    на протяжении всего цикла прогрева (от включения отопления до его выключения).
// 3) После выключения отопления отслеживается инерционный перегрев - насколько фактическая температура превысит
//    заданную по инерции. Перегрев корректирует самообучением время прогрева (работу CH) следующего цикла:
//    чем больше инерционный перегрев, тем раньше выключаем отопление (коэффициенты k_stop, over_goal).
//    Дополнительно (burn_adj) длительность ТЕКУЩЕГО цикла ограничивается адаптивным бюджетом
//    работы горелки CH: среднее время горелки (_burnEma) масштабируется отклонением инерционного
//    перегрева прошлого цикла (_overLast) от цели (_overGoal) с коэффициентом burn_k (масштаб
//    клампится 0.3..2). Прерванный бюджетом цикл не учитывается в самообучении среднего (_burnEma).
// 4) Самообучение уменьшает температурные качели и число пусков котла в час.
// 5) Защита: drop_max - от заморозки (при любых ошибках коэффициентов), over_max - от перегрева, max_heat_min -
//    лимит длительности прогрева.
//======================================================================================================================
class ThermostatPRED : public IoTItem
{
private:
    String _set_id;         // заданная температура
    String _term_id;        // термометр
    String _term_rezerv_id; // резервный термометр
    String _rele;           // реле котла (отопление)
    String _flame_id;       // ID виджета датчика пламени горелки (isFlameOn)
    String _ch_id;          // ID виджета статуса режима CH (например isHeatingActive)
    float _gist = 0.1;      // антидребезговый зазор между пиком и порогом включения, °C
    float _leadK = 1;       // коэффициент преждевременности включения (0 - без упреждения)
    float _kStop = 0.5;     // коэффициент коррекции времени прогрева (CH) от инерционного перегрева
    bool _burnAdj = true;   // адаптивный лимит длительности текущего цикла прогрева по перегреву прошлого цикла
    float _kBurn = 0.3;     // коэффициент влияния перегрева на длительность работы горелки CH (доля бюджета на 1 °C сверх цели)
    float _overGoal = 0.3;  // целевой инерционный перегрев, °C
    float _lagInitMin = 10; // начальная оценка инерции: время от включения котла до минимума температуры, мин
    float _burnInitMin = 20;// начальный бюджет работы горелки CH за цикл прогрева, мин
    float _dropMax = 2;     // защита от замерзания: максимально допустимое падение ниже заданной, °C
    float _overMax = 3;     // защита от перегрева: максимально допустимое превышение заданной, °C
    float _maxHeatMin = 0;  // максимальная длительность непрерывного прогрева, мин (0 - без ограничения)
    float _minRestMin = 0;  // минимальная пауза между циклами прогрева, мин (0 - без ограничения)
    int _direction = 0;     // инверсия логики реле (как в Гистере́зисе)
    int enable = 1;

    float pv = 0, pv2 = 0, sp = 0;
    String interim;
    unsigned long _lastMs = 0;
    bool _lastMsOk = false;
    float _pvPrev = 0;
    bool _pvPrevOk = false;
    float _rateEma = 0; // сглаженная скорость изменения температуры, °C/сек

    bool _heating = false;     // состояние: идет прогрев (отопление включено)
    unsigned long _tOnMs = 0;  // момент включения отопления
    unsigned long _tOffMs = 0; // момент выключения отопления
    unsigned long _tMinMs = 0; // момент минимума температуры
    float _pvOn = 0;           // температура в момент включения отопления
    float _pvStop = 0;         // температура в момент выключения отопления
    float _minPv = 0;          // минимум температуры в цикле прогрева
    float _peakPv = 0;         // пик температуры после выключения отопления
    bool _minFix = false;      // минимум зафиксирован (температура развернулась вверх)
    bool _peakFix = false;     // пик зафиксирован (температура развернулась вниз)
    float _burnSec = 0;        // суммарная длительность работы горелки в режиме CH за цикл, сек
    float _heatSec = 0;        // длительность цикла прогрева, сек
    float _burnBudget = -1;    // адаптивный бюджет работы горелки CH на текущий цикл, сек (-1 - лимит не задан)
    bool _budgetCut = false;   // текущий цикл прерван адаптивным лимитом (не усреднять _burnEma)
    float _stopPv = 999;       // адаптивный порог выключения отопления, °C (999 = не инициализирован)
    float _lastStopPv = 999;   // порог последнего выключения (для антидребезга повторного включения)
    bool _releOn = false;      // текущее логическое состояние реле

    float _tauSec = 600;   // самообучение: инерция - время от включения котла до минимума температуры
    float _deltaEma = 0;   // самообучение: прогноз проседания температуры после включения котла, °C
    float _overEma = 0.3;  // самообучение: средний инерционный перегрев (пик - заданная), °C
    float _burnEma = 1200; // самообучение: средняя работа горелки CH за цикл прогрева, сек
    float _overLast = -1;  // инерционный перегрев последнего завершенного цикла, °C
    float _riseLast = 0;   // подъем температуры после последнего выключения отопления, °C
    unsigned long _starts = 0; // количество пусков котла
    unsigned long _stops = 0;  // количество остановок прогрева
    unsigned long _dbgCycle = 0; // прореживание диагностического вывода (раз в 20 циклов)

    float ema(float oldV, float newV, float k)
    {
        return oldV + k * (newV - oldV);
    }

public:
    ThermostatPRED(String parameters) : IoTItem(parameters)
    {
        jsonRead(parameters, "set_id", _set_id);
        jsonRead(parameters, "term_id", _term_id);
        jsonRead(parameters, "term_rezerv_id", _term_rezerv_id);
        jsonRead(parameters, "rele", _rele);
        jsonRead(parameters, "flame_id", _flame_id);
        jsonRead(parameters, "ch_id", _ch_id);
        jsonRead(parameters, "gist", _gist);
        jsonRead(parameters, "lead_k", _leadK);
        jsonRead(parameters, "k_stop", _kStop);
        jsonRead(parameters, "burn_adj", _burnAdj);
        jsonRead(parameters, "burn_k", _kBurn);
        jsonRead(parameters, "over_goal", _overGoal);
        jsonRead(parameters, "lag_min", _lagInitMin);
        jsonRead(parameters, "burn_init", _burnInitMin);
        jsonRead(parameters, "drop_max", _dropMax);
        jsonRead(parameters, "over_max", _overMax);
        jsonRead(parameters, "max_heat_min", _maxHeatMin);
        jsonRead(parameters, "min_rest_min", _minRestMin);
        jsonRead(parameters, "direction", _direction);

        // защита от ошибочных настроек коэффициентов
        if (_gist < 0.01f) _gist = 0.01f;
        if (_leadK < 0) _leadK = 0;
        if (_leadK > 10) _leadK = 10;
        if (_kStop < 0) _kStop = 0;
        if (_kStop > 1) _kStop = 1;
        if (_kBurn < 0) _kBurn = 0;
        if (_kBurn > 2) _kBurn = 2;
        if (_overGoal < 0.05f) _overGoal = 0.05f;
        if (_lagInitMin < 1) _lagInitMin = 1;
        if (_burnInitMin < 5) _burnInitMin = 5;
        if (_dropMax < 0.5f) _dropMax = 0.5f;
        if (_overMax < 0.5f) _overMax = 0.5f;
        if (_maxHeatMin < 0) _maxHeatMin = 0;
        if (_minRestMin < 0) _minRestMin = 0;

        // начальные оценки самообучения
        _tauSec = _lagInitMin * 60;
        _deltaEma = 0;
        _overEma = _overGoal;
        _burnEma = _burnInitMin * 60;
        _overLast = -1;

        // диагностика привязок: без set_id/term_id/rele отопление не включится никогда
        if (_set_id == "" || _term_id == "" || _rele == "")
        {
            SerialPrint("E", F("Термостат Predict"), "проверка привязок: заданная температура='" + _set_id + "', термометр='" + _term_id + "', резервный термометр='" + _term_rezerv_id + "', реле отопления='" + _rele + "', датчик пламени горелки='" + _flame_id + "', режим отопления котла='" + _ch_id + "' - есть ПУСТЫЕ привязки, отопление работать не будет");
        }
        else
        {
            // direction: 0 - активный "0" (0=вкл, как в Гистере́зисе), 1 - активный "1" (1=вкл)
            SerialPrint("i", F("Термостат Predict"), "привязки настроены: заданная температура='" + _set_id + "', термометр='" + _term_id + "', реле отопления='" + _rele + "', датчик пламени горелки='" + _flame_id + "', режим отопления котла='" + _ch_id + "', логика включения реле=" + String(_direction) + (_direction ? " (вкл=1)" : " (вкл=0, инверсия!)"));
        }
    }

    // парсинг состояния вкл/выкл из значения виджета (isFlameOn: "🔥работает"/"➖", isHeatingActive: "✅"/"➖")
    static bool parseOnState(String val)
    {
        if (val.length() == 0) return false;
        val.toLowerCase();
        if (val.indexOf("не ") != -1 || val.indexOf("➖") != -1 || val.indexOf("off") != -1 || val.indexOf("false") != -1 || val == "0") return false; 
        if (val.indexOf("🔥") != -1 || val.indexOf("✅") != -1) return true;
        if (val.indexOf("работает") != -1 || val.indexOf("вкл") != -1) return true;
        if (val.indexOf("true") != -1 || val.indexOf("on") != -1 || val == "1") return true;
        return false;
    }

    static bool readNumericValue(const String &id, float &result)
    {
        if (id == "") return false;
        IoTItem *item = findIoTItem(id);
        if (!item) return false;

        String text = item->getValue();
        text.trim();
        if (text.length() == 0) return false;

        const char *start = text.c_str();
        char *end = nullptr;
        float parsed = strtof(start, &end);
        if (end == start || *end != '\0' || !isfinite(parsed)) return false;

        result = parsed;
        return true;
    }

    bool flameOnNow()
    {
        if (_flame_id == "") return _heating; // датчик пламени не задан - считаем горелку работающей весь цикл
        IoTItem *f = findIoTItem(_flame_id);
        if (!f) return false;
        return parseOnState(f->getValue());
    }

    bool chOnNow()
    {
        if (_ch_id == "") return true; // статус CH не задан - считаем что режим отопления активен
        IoTItem *c = findIoTItem(_ch_id);
        if (!c) return false;
        return parseOnState(c->getValue());
    }

    bool setRele(bool on)
    {
        if (on == _releOn) return true;
        IoTItem *r = findIoTItem(_rele);
        if (!r)
        {
            SerialPrint("E", F("Термостат Predict"), "реле отопления недоступно: привязка '" + _rele + "' не найдена");
            return false;
        }

        if (_direction)
        {
            r->setValue(on ? "1" : "0", true);
        }
        else
        {
            r->setValue(on ? "0" : "1", true);
        }
        _releOn = on;
        return true;
    }

    // прогноз инерционного проседания температуры после включения котла, °C:
    // коррелирует с текущей скоростью падения температуры и с проседанием из истории циклов
    float predictedDip()
    {
        float d = (_rateEma < 0) ? (-_rateEma) * _tauSec : 0;
        if (_deltaEma > d) d = _deltaEma;
        if (d > _dropMax) d = _dropMax;
        if (d < 0) d = 0;
        return d;
    }

    // накопленные данные самообучения одной понятной строкой (для сообщений в консоль)
    String learningState()
    {
        String result = "накопленные данные обучения: завершенных циклов прогрева=" + String((unsigned long)_stops);
        result += ", инерция системы (время от включения котла до минимума температуры)=" + String(_tauSec / 60.0f, 1) + " мин";
        result += ", средняя работа горелки за цикл прогрева=" + String(_burnEma / 60.0f, 1) + " мин";
        if (_overLast >= 0)
        {
            result += ", инерционный перегрев помещения: последний=" + String(_overLast, 2) + " °C, средний=" + String(_overEma, 2) + " °C (целевой=" + String(_overGoal, 2) + " °C)";
        }
        else
        {
            result += ", инерционный перегрев помещения еще не измерен";
        }
        result += ", прогноз проседания температуры после включения котла=" + String(predictedDip(), 2) + " °C";
        return result;
    }

    bool startHeating(bool freeze, bool predict, float predictThreshold)
    {
        if (!setRele(true))
        {
            setValue("ошибка управления реле");
            return false;
        }

        _heating = true;
        _starts++;
        _tOnMs = millis();
        _pvOn = pv;
        _minPv = pv;
        _tMinMs = _tOnMs;
        _minFix = false;
        _peakFix = false;
        _peakPv = pv;
        _burnSec = 0;
        _heatSec = 0;
        _budgetCut = false;
        // адаптивный бюджет работы горелки CH на цикл: среднее время горелки (_burnEma)
        // масштабируется отклонением инерционного перегрева прошлого цикла (_overLast) от цели
        // (_overGoal): перегрев выше цели сокращает бюджет пропорционально burn_k, заниженный
        // перегрев - симметрично увеличивает (масштаб клампится 0.3..2). Нет данных о перегреве
        // (нет завершенного цикла) или алгоритм выключен - лимит не применяется.
        _burnBudget = -1;
        if (_burnAdj && _kBurn > 0 && _overLast >= 0 && _burnEma > 120)
        {
            float scale = 1 - _kBurn * (_overLast - _overGoal);
            if (scale < 0.3f) scale = 0.3f;
            if (scale > 2) scale = 2;
            _burnBudget = _burnEma * scale;
        }
        if (_stopPv > 100)
        {
            _stopPv = sp; // первый цикл: порог выключения на уровне заданной
        }
        setValue(freeze ? "защита от замерзания" : "прогрев");

        // основной статус понятными словами
        float rateCMin = _rateEma * 60.0f;
        String rateText;
        if (_rateEma < -0.0001f)
        {
            rateText = "температура падает со скоростью " + String(-rateCMin, 2) + " °C в минуту";
        }
        else if (_rateEma > 0.0001f)
        {
            rateText = "температура растет со скоростью " + String(rateCMin, 2) + " °C в минуту";
        }
        else
        {
            rateText = "температура стоит на месте";
        }
        SerialPrint("i", F("Термостат Predict"), "включение отопления: текущая температура=" + String(pv, 2) + " °C, заданная температура=" + String(sp, 2) + " °C, " + rateText + (_burnBudget > 0 ? String("; адаптивный бюджет горелки на этот цикл=") + String(_burnBudget / 60.0f, 1) + " мин" : String("")) + "; " + learningState());

        // чем в этот момент работа отличается от обычного гистерезиса и что можно скорректировать коэффициентами
        float hystOn = sp - _gist;
        if (freeze)
        {
            SerialPrint("i", F("Термостат Predict"), "отличие от обычного гистерезиса: аварийный старт по защите от замерзания - температура " + String(pv, 2) + " °C опустилась до границы " + String(sp - _dropMax, 2) + " °C (заданная температура минус допустимое падение " + String(_dropMax, 2) + " °C), обычный гистерезис ждал бы порога " + String(hystOn, 2) + " °C, но отопление включено без ожидания; настройка: если защита от замерзания срабатывает часто, увеличьте коэффициент упреждения lead_k (сейчас " + String(_leadK, 2) + "), чтобы отопление включалось раньше; допустимое падение drop_max - это защита, менять не рекомендуется");
        }
        else if (predict)
        {
            bool waitOk = (_rateEma < -0.0001f);
            float waitMin = waitOk ? (pv - hystOn) / (-rateCMin) : 0;
            SerialPrint("i", F("Термостат Predict"), "отличие от обычного гистерезиса: РАННЕЕ (упреждающее) включение - отопление включилось при температуре " + String(pv, 2) + " °C, что на " + String(pv - hystOn, 2) + " °C выше порога включения обычного гистерезиса " + String(hystOn, 2) + " °C (заданная температура минус гистерезис " + String(_gist, 2) + " °C), порог упреждающего включения=" + String(predictThreshold, 2) + " °C" + (waitOk ? String("; обычный гистерезис включился бы примерно через ") + String(waitMin, 1) + " мин (оценка)" : String("; обычный гистерезис включился бы позже: сейчас температура не падает, упреждение сделано по накопленному прогнозу проседания")) + "; настройка: чтобы включение происходило позже (цикл прогрева короче, но температура проседает глубже), уменьшите коэффициент упреждения lead_k (сейчас " + String(_leadK, 2) + ", диапазон 0-10): thermostat.set_lead_k(0.5) или настройка lead_k в параметрах модуля; чтобы включать раньше - увеличьте его");
        }
        else
        {
            SerialPrint("i", F("Термостат Predict"), "отличие от обычного гистерезиса: включение обычное (классическое) - температура " + String(pv, 2) + " °C дошла до порога включения гистерезиса " + String(hystOn, 2) + " °C (заданная температура минус гистерезис " + String(_gist, 2) + " °C)");
        }
        return true;
    }

    bool stopHeating(const String &reason)
    {
        if (!setRele(false))
        {
            setValue("ошибка управления реле");
            return false;
        }

        _heating = false;
        _stops++;
        _tOffMs = millis();
        _pvStop = pv;
        _peakPv = pv;
        _lastStopPv = pv;
        _peakFix = false;
        float burnEmaPrev = _burnEma; // средняя работа горелки за прошлые циклы (до учета текущего цикла)
        if (!_budgetCut)
        {
            // цикл, прерванный адаптивным бюджетом, не учитываем в среднем (_burnEma)
            _burnEma = ema(_burnEma, _burnSec, 0.3f);
        }
        _budgetCut = false;
        setValue(reason);

        // итоги цикла понятными словами
        float duty = (_heatSec > 60) ? (_burnSec / _heatSec) : 1;
        if (duty < 0) duty = 0;
        if (duty > 1) duty = 1;
        SerialPrint("i", F("Термостат Predict"), reason + ": текущая температура=" + String(pv, 2) + " °C, заданная температура=" + String(sp, 2) + " °C, длительность цикла прогрева=" + String(_heatSec / 60.0f, 1) + " мин, работа горелки за цикл прогрева=" + String(_burnSec / 60.0f, 1) + " мин (" + String(duty * 100.0f, 0) + "% времени цикла)" + (burnEmaPrev > 0 ? String(", средняя работа горелки за прошлые циклы=") + String(burnEmaPrev / 60.0f, 1) + " мин" : String("")));

        // чем в этот момент работа отличается от обычного гистерезиса и что можно скорректировать коэффициентами
        float hystOff = sp + _gist;
        if (reason.startsWith("завершен"))
        {
            float diffC = hystOff - _stopPv;
            String diffText;
            if (diffC > 0.05f)
            {
                diffText = "выключение произошло РАНЬШЕ на " + String(diffC, 2) + " °C, чем сделал бы обычный гистерезис";
            }
            else if (diffC < -0.05f)
            {
                diffText = "выключение произошло ПОЗЖЕ на " + String(-diffC, 2) + " °C, чем сделал бы обычный гистерезис";
            }
            else
            {
                diffText = "выключение совпало с точкой обычного гистерезиса";
            }
            String timeText;
            if (diffC > 0 && _rateEma > 0.0001f)
            {
                float residMin = diffC / (_rateEma * 60.0f);
                timeText = "; при текущем росте температуры " + String(_rateEma * 60.0f, 2) + " °C в минуту обычный гистерезис продолжал бы прогрев еще примерно " + String(residMin, 1) + " мин, и горелка отработала бы еще примерно " + String(residMin * duty, 1) + " мин (оценка)";
            }
            else if (diffC <= 0)
            {
                timeText = "; обычный гистерезис завершил бы прогрев раньше, мы держим котел дольше, чтобы температура установилась у заданной (оценка по накопленным данным)";
            }
            else
            {
                timeText = "; оценка времени недоступна: температура стоит на месте";
            }
            String burnText;
            if (burnEmaPrev > 0)
            {
                float burnDiffMin = (burnEmaPrev - _burnSec) / 60.0f;
                float econPct = burnDiffMin / (burnEmaPrev / 60.0f) * 100.0f;
                burnText = econPct >= 0 ? String("; работа горелки в этом цикле короче средней за прошлые циклы на ") + String(burnDiffMin, 1) + " мин (экономия " + String(econPct, 0) + "%)" : String("; работа горелки в этом цикле дольше средней за прошлые циклы на ") + String(-burnDiffMin, 1) + " мин";
            }
            else
            {
                burnText = "; это первый завершенный цикл - сравнение со средним появится после накопления данных";
            }
            String hintText;
            if (_overLast < 0)
            {
                hintText = "; настройка: инерционный перегрев еще не измерен, после первого цикла самообучение начнет сдвигать порог выключения";
            }
            else if (_overLast > _overGoal + 0.05f)
            {
                hintText = "; настройка: инерционный перегрев прошлого цикла (" + String(_overLast, 2) + " °C) больше целевого " + String(_overGoal, 2) + " °C - чтобы уменьшить длительность прогрева и перегрев, увеличьте коэффициент коррекции времени прогрева k_stop (сейчас " + String(_kStop, 2) + ", диапазон 0-1: thermostat.set_k_stop(0.7)), увеличьте коэффициент влияния перегрева на бюджет горелки burn_k (сейчас " + String(_kBurn, 2) + ", диапазон 0-2: thermostat.set_burn_k(0.6)) или уменьшите целевой перегрев over_goal (сейчас " + String(_overGoal, 2) + " °C: thermostat.set_over_goal(0.15))";
            }
            else
            {
                hintText = "; настройка: инерционный перегрев прошлого цикла (" + String(_overLast, 2) + " °C) не превышает целевой " + String(_overGoal, 2) + " °C, поэтому термостат намеренно продлевает прогрев - чтобы выключаться раньше, приблизьте целевой перегрев к фактическому (thermostat.set_over_goal(" + String(_overLast, 2) + ")) либо уменьшите k_stop (сейчас " + String(_kStop, 2) + ") и burn_k (сейчас " + String(_kBurn, 2) + ")";
            }
            SerialPrint("i", F("Термостат Predict"), "отличие от обычного гистерезиса (выключение): обученный порог выключения=" + String(_stopPv, 2) + " °C, обычный гистерезис выключил бы при " + String(hystOff, 2) + " °C (заданная температура плюс гистерезис " + String(_gist, 2) + " °C), " + diffText + timeText + burnText + "; вклад самообучения: порог выключения отстоит от заданной температуры на " + String(_stopPv - sp, 2) + " °C (в необученном состоянии порог равен заданной температуре), завершенных циклов прогрева=" + String((unsigned long)_stops) + hintText);
        }
        else if (reason.startsWith("пауза (адаптивный лимит"))
        {
            String scaleText = (_burnEma > 120 && _burnBudget > 0) ? String("бюджет рассчитан из средней работы горелки за цикл=") + String(_burnEma / 60.0f, 1) + " мин, масштаб бюджета по перегреву прошлого цикла=" + String(_burnBudget / _burnEma, 2) : String("бюджет равен средней работе горелки (данных о перегреве недостаточно)");
            SerialPrint("i", F("Термостат Predict"), "отличие от обычного гистерезиса (выключение): цикл остановлен адаптивным бюджетом работы горелки - бюджет на цикл=" + String(_burnBudget / 60.0f, 1) + " мин, отработано=" + String(_burnSec / 60.0f, 1) + " мин (" + scaleText + "); обычный гистерезис работал бы без ограничения по времени, пока температура не дойдет до " + String(hystOff, 2) + " °C (заданная температура плюс гистерезис " + String(_gist, 2) + " °C), раннее выключение по бюджету исключает инерционный перегрев от долгой работы котла; настройка: чтобы бюджет был строже (циклы прогрева короче), увеличьте burn_k (сейчас " + String(_kBurn, 2) + ", диапазон 0-2: thermostat.set_burn_k(0.6)); чтобы бюджет был мягче - уменьшите burn_k или выключите лимит thermostat.set_burn_adj(0); пересчитать накопленные оценки с нуля можно выполнением thermostat.reset()");
        }
        else
        {
            SerialPrint("i", F("Термостат Predict"), "отличие от обычного гистерезиса (выключение): сработала срочная защита раньше обученного порога - обычный гистерезис выключил бы при " + String(hystOff, 2) + " °C (заданная температура плюс гистерезис " + String(_gist, 2) + " °C), текущий обученный порог выключения=" + String(_stopPv, 2) + " °C, текущая температура=" + String(pv, 2) + " °C, работа горелки за цикл прогрева=" + String(_burnSec / 60.0f, 1) + " мин; " + learningState() + "; настройка: чтобы срочная защита не срабатывала, увеличьте k_stop (сейчас " + String(_kStop, 2) + ", диапазон 0-1: thermostat.set_k_stop(0.7)) и burn_k (сейчас " + String(_kBurn, 2) + ", диапазон 0-2: thermostat.set_burn_k(0.6)), уменьшите целевой перегрев over_goal (сейчас " + String(_overGoal, 2) + " °C); границы защиты drop_max и over_max менять не рекомендуется");
        }
        return true;
    }

    void doByInterval()
    {
        unsigned long now = millis();
        float dt = 0;
        bool dtOk = false;
        if (_lastMsOk)
        {
            dt = (now - _lastMs) / 1000.0f;
            dtOk = (dt > 0 && dt <= 3600);
        }
        _lastMs = now;
        _lastMsOk = true;

        float parsedSp = 0;
        bool spOk = readNumericValue(_set_id, parsedSp);
        if (spOk)
        {
            sp = parsedSp;
        }
        else
        {
            sp = 0;
        }

        float primaryPv = 0;
        bool primaryOk = readNumericValue(_term_id, primaryPv) && primaryPv > -40 && primaryPv < 120;
        float reservePv = 0;
        bool reserveOk = readNumericValue(_term_rezerv_id, reservePv) && reservePv > -40 && reservePv < 120;
        bool reserveUsed = !primaryOk && reserveOk;
        bool pvOk = primaryOk || reserveOk;
        if (primaryOk)
        {
            pv = primaryPv;
        }
        else if (reserveOk)
        {
            pv = reservePv;
        }
        else
        {
            pv = 0;
        }
        pv2 = reserveOk ? reservePv : 0;

        if (!spOk || _rele == "")
        {
            // если не заполнены настройки термостата
            _pvPrevOk = false;
            setValue("ошибка настройки термостата");
            SerialPrint("E", F("Термостат Predict"), "ошибка настройки: заданная температура=" + String(sp, 2) + ", привязка заданной температуры='" + _set_id + "', привязка термометра='" + _term_id + "', привязка реле отопления='" + _rele + "'");
            return;
        }
        if (!pvOk)
        {
            // не трогаем реле (как в Гистере́зисе)
            _pvPrevOk = false;
            if (enable)
            {
                setValue(_term_rezerv_id != "" ? "ошибка резервного датчика" : "ошибка датчика температуры");
                SerialPrint("i", F("Термостат Predict"), "нет показаний температуры: основной термометр=" + String(pv, 2) + ", резервный термометр=" + String(pv2, 2) + ", привязка термометра='" + _term_id + "', привязка резервного термометра='" + _term_rezerv_id + "', термостат " + String(enable ? "включен" : "выключен"));
            }
            return;
        }

        // сглаженная скорость изменения температуры
        if (dtOk && _pvPrevOk)
        {
            float inst = (pv - _pvPrev) / dt;
            if (inst > -0.5f && inst < 0.5f)
            {
                _rateEma = ema(_rateEma, inst, 0.35f);
            }
        }
        _pvPrev = pv;
        _pvPrevOk = true;

        if (_dbgCycle++ % 20 == 0)
        {
            // диагностика всей цепочки: реле, мост включения котла, пламя, статус CH
            String releState = "(нет)";
            if (_rele != "")
            {
                IoTItem *r = findIoTItem(_rele);
                if (r) releState = r->getValue();
            }
            String brState = "(нет)";
            if (_ch_id != "")
            {
                IoTItem *b = findIoTItem(_ch_id);
                if (b) brState = b->getValue();
            }
            String flameState = "(нет)";
            if (_flame_id != "")
            {
                IoTItem *f = findIoTItem(_flame_id);
                if (f) flameState = f->getValue();
            }
            SerialPrint("i", F("Термостат Predict"), "состояние: текущая температура=" + String(pv, 2) + " °C, заданная температура=" + String(sp, 2) + " °C, скорость изменения температуры=" + String(_rateEma * 60.0f, 2) + " °C в минуту, режим=" + String(_heating ? "прогрев" : "пауза") + ", термостат=" + String(enable ? "включен" : "выключен") + ", реле='" + releState + "'" + (_direction ? "" : " (инверсия: 0=вкл)") + ", обученный порог выключения=" + (_stopPv > 100 ? String("еще не установлен") : String(_stopPv, 2) + " °C") + (_burnBudget > 0 ? String(", бюджет горелки на цикл=") + String(_burnBudget / 60.0f, 1) + " мин" : String("")) + ", режим отопления='" + brState + "', датчик пламени='" + flameState + "'" + (!_heating && enable ? String(", порог включения обычного гистерезиса=") + String(sp - _gist, 2) + " °C" : String("")));
        }

        if (_heating)
        {
            if (dtOk)
            {
                _heatSec += dt;
                if (flameOnNow() && chOnNow())
                {
                    _burnSec += dt; // суммарная длительность работы горелки в режиме CH
                }
            }
            // минимум температуры и его фиксация при развороте вверх (инерционное проседание после включения)
            if (pv < _minPv)
            {
                _minPv = pv;
                _tMinMs = now;
            }
            if (!_minFix && _rateEma > 0 && now - _tOnMs > 60000 && pv >= _minPv + 0.05f && now - _tMinMs > 180000)
            {
                _minFix = true;
                float delta = _pvOn - _minPv;
                if (delta > 0)
                {
                    _deltaEma = ema(_deltaEma, delta, 0.3f);
                }
                float tau = (_tMinMs - _tOnMs) / 1000.0f;
                if (tau > 30 && tau < 4 * 3600)
                {
                    _tauSec = ema(_tauSec, tau, 0.3f);
                }
            }

            if (enable)
            {
                // ЗАЩИТА ОТ ЗАМОРОЗКИ: не выключаем котел, пока температура ниже допустимой границы
                if (pv <= sp - _dropMax + 0.0001f)
                {
                    if (!setRele(true))
                    {
                        setValue("ошибка управления реле");
                        return;
                    }
                    setValue("прогрев (защита от замерзания)");
                    return;
                }
                // ЗАЩИТА ОТ ПЕРЕГРЕВА: жесткая граница и упреждающая по инерционному подъему.
                // Упреждающая оценка подъема масштабируется накопленным запасом тепла радиаторов
                // (долей работы горелки CH в этом цикле к средней за цикл)
                float charge = (_burnEma > 120) ? _burnSec / _burnEma : 1;
                if (charge > 1) charge = 1;
                float riseEst = _riseLast * charge;
                if (riseEst < 0) riseEst = 0;
                if (riseEst > _overMax) riseEst = _overMax;
                if (pv >= sp + _overMax - 0.0001f || pv + riseEst >= sp + _overMax)
                {
                    stopHeating("пауза (защита от перегрева)");
                    return;
                }
                // лимит длительности прогрева
                if (_maxHeatMin > 0 && _heatSec >= _maxHeatMin * 60)
                {
                    stopHeating("пауза (лимит времени прогрева)");
                    return;
                }
                // адаптивный лимит длительности прогрева: бюджет работы горелки CH за цикл,
                // рассчитанный на старте цикла из среднего (_burnEma) с поправкой на инерционный
                // перегрев прошлого цикла (см. startHeating). Прерванный этим лимитом цикл не
                // учитывается в самообучении _burnEma - иначе среднее зациклится вниз
                if (_burnBudget > 0 && _burnSec >= _burnBudget)
                {
                    _budgetCut = true;
                    stopHeating("пауза (адаптивный лимит CH)");
                    return;
                }
                // плановое упреждающее выключение: температура достигла адаптивного порога.
                // Порог самообучением смещается по измеренному инерционному перегреву:
                // чем больше инерционный перегрев, тем раньше выключаем отопление.
                // Порог клампится к ТЕКУЩЕЙ заданной: при снижении уставки во время
                // прогрева не зависаем на старом пороге
                float stopPv = _stopPv;
                if (stopPv < sp - _dropMax) stopPv = sp - _dropMax;
                if (stopPv > sp + 0.5f) stopPv = sp + 0.5f;
                // синхронизируем адаптивный порог с действующим (кламп к текущей sp):
                // застарелый порог не «зависает», и диагностика показывает реальный порог
                if (stopPv != _stopPv)
                {
                    _stopPv = stopPv;
                }
                // плановое выключение - ТОЛЬКО на неубывающем тренде (rate >= 0): пересечение
                // адаптивного порога снизу. На спаде прогрев держится: котел, включенный
                // упреждением выше порога, должен довести pv до разворота тренда - иначе
                // мгновенное погасание (дребезг). При снижении уставки порог клампится к
                // новой sp; на реальном доме pv растет от работающего котла - выкл сразу.
                // Жесткие защиты (перегрев/замерзание) выше работают без этого гейта
                if (_rateEma >= 0 && pv >= stopPv - 0.0001f)
                {
                    stopHeating("завершен (" + String(_burnSec / 60.0f, 0) + " мин)");
                    return;
                }
                setValue(reserveUsed ? "резервный датчик" : "прогрев (горелка " + String(_burnSec / 60.0f, 0) + " мин)");
            }
        }
        else
        {
            // после выключения отопления отслеживаем пик температуры (инерционный перегрев)
            if (_stops > 0 && !_peakFix)
            {
                if (pv > _peakPv)
                {
                    _peakPv = pv;
                }
                if (_rateEma < 0 && now - _tOffMs > 60000)
                {
                    _peakFix = true;
                    float rise = _peakPv - _pvStop;
                    if (rise < 0) rise = 0;
                    _riseLast = rise;
                    float over = _peakPv - sp;
                    if (over > 0)
                    {
                        _overLast = over;
                        _overEma = ema(_overEma, over, 0.3f);
                        // самообучение порога выключения: чем больше инерционный перегрев,
                        // тем раньше (ниже по температуре) выключаем отопление в следующем цикле
                        float shift = _kStop * (_overGoal - over);
                        _stopPv += shift;
                        SerialPrint("i", F("Термостат Predict"), "самообучение: температура развернулась вниз, измерен инерционный перегрев " + String(over, 2) + " °C (пик " + String(_peakPv, 2) + " °C против заданной " + String(sp, 2) + " °C), инерционный подъем после выключения котла составил " + String(rise, 2) + " °C; обученный порог выключения скорректирован на " + String(shift, 2) + " °C и теперь равен " + String(_stopPv, 2) + " °C, обычный же гистерезис выключался бы только при " + String(sp + _gist, 2) + " °C (заданная температура плюс гистерезис), так цикл прогрева становится " + (shift < 0 ? String("короче") : String("дольше")) + "; настройка: если инерционный перегрев системно отличается от целевого (" + String(_overGoal, 2) + " °C), подстройте целевой перегрев (thermostat.set_over_goal) или силу коррекции: k_stop (thermostat.set_k_stop, диапазон 0-1) и burn_k (thermostat.set_burn_k, диапазон 0-2)");
                    }
                }
            }

            if (enable)
            {
                bool freeze = (pv <= sp - _dropMax + 0.0001f);
                bool predict = false;
                float predictThreshold = -999; // порог упреждающего включения (для сообщений в консоль)
                bool hyst = false; // классическое включение гистерезисом
                if (_rateEma < 0)
                {
                    // порог упреждающего включения: заданная + упреждение по прогнозу проседания
                    float pvOn = sp + _leadK * predictedDip();
                    if (pvOn > sp + _overMax)
                    {
                        pvOn = sp + _overMax;
                    }
                    if (_peakFix)
                    {
                        // антидребезг: не включаться раньше, чем температура остынет ниже пика на _gist
                        float cap = _peakPv - _gist;
                        if (pvOn > cap)
                        {
                            pvOn = cap;
                        }
                    }
                    if (_lastStopPv < 900)
                    {
                        // антидребезг: повторное упреждающее включение не раньше, чем pv остынет
                        // ниже порога последнего выключения на гистерезис
                        float stopCap = _lastStopPv - _gist;
                        if (pvOn > stopCap)
                        {
                            pvOn = stopCap;
                        }
                    }
                    predictThreshold = pvOn;
                    if (pv <= pvOn + 0.0001f)
                    {
                        predict = true;
                    }
                }
                // классическое включение (как в Гистере́зисе): температура просела ниже заданной.
                // Без него отопление не включается, когда температура ниже заданной, но скорость
                // стала неубывающей (rate >= 0): после рестарта или при стабилизации температуры
                float hystOn = sp - _gist;
                if (_peakFix)
                {
                    // антидребезг: не включаться раньше, чем температура остынет ниже пика на _gist
                    float hystCap = _peakPv - _gist;
                    if (hystOn > hystCap)
                    {
                        hystOn = hystCap;
                    }
                }
                if (pv <= hystOn + 0.0001f)
                {
                    hyst = true;
                }
                bool restOk = (_minRestMin <= 0) || (now - _tOffMs >= (unsigned long)(_minRestMin * 60000UL));
                if (((predict || hyst) && restOk) || freeze)
                {
                    startHeating(freeze, predict, predictThreshold);
                }
                else
                {
                    if (reserveUsed)
                    {
                        setValue("резервный датчик");
                    }
                    else if (_overLast >= 0)
                    {
                        setValue("ожидание (прошлый перегрев " + String(_overLast, 1) + " °C)");
                    }
                    else
                    {
                        setValue("ожидание");
                    }
                    if (_dbgCycle % 20 == 0)
                    {
                        SerialPrint("i", F("Термостат Predict"), "отопление не включено: текущая температура=" + String(pv, 2) + " °C, заданная температура=" + String(sp, 2) + " °C, скорость изменения температуры=" + String(_rateEma * 60.0f, 2) + " °C в минуту, упреждающее включение=" + String(predict ? "условие выполнено" : "условие не выполнено") + ", порог упреждающего включения=" + (predictThreshold > 900 ? String("не определено (температура не падает)") : String(predictThreshold, 2) + " °C") + ", включение по гистерезису=" + String(hyst ? "условие выполнено" : "условие не выполнено") + ", порог включения по гистерезису=" + String(hystOn, 2) + " °C, пауза между циклами прогрева=" + String(restOk ? "выдержана" : "еще не выдержана"));
                    }
                }
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
                    enable = (int)param[0].valD;
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
            if (command == "set_lead_k")
            {
                if (param.size())
                {
                    _leadK = param[0].valD;
                    if (_leadK < 0) _leadK = 0;
                    if (_leadK > 10) _leadK = 10;
                }
            }
            if (command == "set_k_stop")
            {
                if (param.size())
                {
                    _kStop = param[0].valD;
                    if (_kStop < 0) _kStop = 0;
                    if (_kStop > 1) _kStop = 1;
                }
            }
            if (command == "set_over_goal")
            {
                if (param.size())
                {
                    _overGoal = param[0].valD;
                    if (_overGoal < 0.05f) _overGoal = 0.05f;
                }
            }
            if (command == "set_burn_adj")
            {
                if (param.size())
                {
                    _burnAdj = ((int)param[0].valD != 0);
                    SerialPrint("i", F("Термостат Predict"), String("адаптивный лимит длительности прогрева (бюджет горелки по перегреву прошлого цикла): ") + (_burnAdj ? "включен" : "выключен"));
                }
            }
            if (command == "set_burn_k")
            {
                if (param.size())
                {
                    _kBurn = param[0].valD;
                    if (_kBurn < 0) _kBurn = 0;
                    if (_kBurn > 2) _kBurn = 2;
                }
            }
        }
        if (command == "reset")
        {
            // сброс самообучения к начальным оценкам
            _tauSec = _lagInitMin * 60;
            _deltaEma = 0;
            _overEma = _overGoal;
            _burnEma = _burnInitMin * 60;
            _stopPv = 999;
            _lastStopPv = 999;
            _overLast = -1;
            _burnBudget = -1;
            _budgetCut = false;
            _riseLast = 0;
            _minFix = false;
            _peakFix = false;
            SerialPrint("i", F("Термостат Predict"), "сброс самообучения выполнен: накопленные оценки возвращены к начальным настройкам (инерция системы=" + String(_lagInitMin, 1) + " мин, средняя работа горелки=" + String(_burnInitMin, 1) + " мин, целевой инерционный перегрев=" + String(_overGoal, 2) + " °C, обученный порог выключения будет установлен заново)");
        }
        return {};
    }
    ~ThermostatPRED() {};
};

void *getAPI_Thermostat(String subtype, String param)
{
    if (subtype == F("ThermostatGIST"))
    {
        return new ThermostatGIST(param);
    }
    else if (subtype == F("ThermostatPRED"))
    {
        return new ThermostatPRED(param);
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