#include "Global.h"
#include "classes/IoTItem.h"
#include "GyverPID.h"
#include "MqttClient.h"
#include "NTP.h"
#include <stdarg.h> // va_list для посимвольной сборки JSON пульса диагностики (jbAdd)

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

// Заранее выделенный буфер и помощники посимвольной сборки машинного пульса диагностики
// ThermostatPRED (stateJson/publishState). Раньше на каждый такт пульс собирался из ~30
// временных String и уходил в MQTT - лишние для ESP аллокации и трафик в брокер. Теперь
// сообщение собирается в один статический буфер (без аллокаций), а тактовые пульсы
// троттлятся (PULSE_PERIOD_MS); внеплановые пульсы (смена состояния реле, включение debug)
// уходят сразу. Буфер общий для всех экземпляров ThermostatPRED: их такты и публикации
// выполняются в основном цикле (IoTItem::loop -> doByInterval), однопоточно, без пересечений.
static const int PULSE_BUF_CAP = 4000; // размер буфера пульса (реальная длина ~800-3700 байт, с запасом - умещается и длинный текст совета)
static const unsigned long PULSE_PERIOD_MS = 30000UL; // минимальный интервал тактового пульса
static char pulseMsgBuf[PULSE_BUF_CAP];

// дописывает фрагмент (формат printf) в буфер пульса; pos - позиция записи; при нехватке
// места запись безопасно обрезается, буфер всегда остается валидной C-строкой
static void jbAdd(char *buf, int cap, int &pos, const char *fmt, ...)
{
    if (!buf || pos < 0 || pos >= cap - 1) return;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + pos, cap - pos, fmt, args);
    va_end(args);
    if (n <= 0) return;
    pos = (pos + n > cap - 1) ? cap - 1 : pos + n;
}

// числовое поле JSON или null, если данных нет (буферный аналог jNum, без временных String)
static void jbAddNum(char *buf, int cap, int &pos, const char *prefix, float v, bool has)
{
    if (has) jbAdd(buf, cap, pos, "%s%.2f", prefix, (double)v);
    else jbAdd(buf, cap, pos, "%snull", prefix);
}

//======================================================================================================================
// Термостат Predict (ThermostatPRED) - предиктивный термостат с самообучением.
//
// Обычный гистерезисный термостат включает котел, когда температура УЖЕ упала ниже заданной,
// и выключает, когда она УЖЕ поднялась выше заданной. Из-за инерции системы отопления
// (горячие радиаторы продолжают отдавать тепло после выключения, а после включения требуется
// время, чтобы тепло дошло до комнаты) температура "качается". Predict обучает модель дома
// на каждом цикле прогрева и работает на опережение:
//   * ВКЛЮЧЕНИЕ заблаговременно (коэффициент on_k, 0..100 %): котел стартует раньше
//     гистерезисного порога. Порог включения = (заданная - gist) + on_k/100 * обученная
//     просадка (на сколько температура опускалась по инерции после включения, пока тепло
//     шло до комнаты; EMA по циклам, в первую очередь предыдущий). Мгновенная скорость
//     падения в пороге не участвует. Старт не выше гистерезисного порога выключения
//     (заданная + gist). При on_k=0 порог совпадает с гистерезисным уровнем вкл.
//   * ВЫКЛЮЧЕНИЕ заблаговременно (коэффициент off_k, 0..100 %): котел глушится, не дав
//     температуре дойти до гистерезисного порога выключения. Порог выключения =
//     (заданная + gist) - off_k/100 * обученный уровень перегрева прошлых циклов (EMA
//     измеренного перегрева каждого цикла, в первую очередь - предыдущего). Антипульс:
//     порог выключения не опускается ниже стартового + gist, зона прогрева не выворачивается.
//     Инерционный подъём после останова (rise) продолжает обучаться и работает в защитах.
//   * УЧЕТ ГОРЕЛКИ: время работы горелки (flame_id) в режиме отопления (ch_id) суммируется за
//     цикл прогрева. Бюджет горелки на текущий цикл = среднее время горелки прошлых циклов
//     минус коррекция по перегреву прошлого цикла относительно цели over_goal: обученный
//     перегрев, пересчитанный в секунды через обученную скорость прогрева, масштабируется
//     burn_k (burn_adj - вкл/выкл этой поправки).
//   * ЗАЩИТЫ: drop_max - от разморозки (немедленный старт), over_max - от перегрева (стоп
//     с учетом инерционного догрева), max_heat_min - лимит прогрева, min_rest_min - пауза.
//   * СОЛНЦЕ: рост температуры при выключенном отоплении, длящийся дольше окна допустимого
//     роста (большее из окна инерции системы и удвоенного времени последнего прогрева -
//     радиаторы после долгого прогрева отдают накопленное тепло значительно дольше окна
//     инерции), либо превысивший возможный инерционный подъем, считается внешним нагревом
//     (солнечным) и НЕ используется в самообучении.
// Справочные начальные оценки (до обучения): дом прогревается и остывает со скоростью
// около 1 °C за 50 минут (0.02 °C/мин). Обученная модель и накопленные измерения
// сохраняются на flash чекпоинтами (не реже чем раз в 5 минут, плюс фиксация при каждом
// старте/завершении цикла) и восстанавливаются после перезагрузки: незавершенный цикл
// прогрева продолжается со всеми накопленными измерениями (горелка, минимум/пик, таймеры),
// управление реле заново подтверждается на первом такте. При debug=1 в MQTT (топик <id>/debug)
// публикуется ТОЛЬКО машинный пульс метрик event:"state" (time/ts/interval_sec + полный снимок
// state, без message и без вывода в консоль/WS), собираемый в заранее выделенный буфер без
// временных String; тактовые пульсы ограничены частотой (не чаще раза в 30 сек), чтобы не
// грузить ESP и брокер, а после смены состояния реле и при включении debug пульс уходит сразу.
// События-пояснения (heat_start/heat_stop/learn/digest/advice/control/info) отменены -
// все цифры есть в пульсе. Пояснения работы выводятся только в консоль/WS-лог (say()).
//======================================================================================================================
class ThermostatPRED : public IoTItem
{
private:
    String _set_id;         // заданная температура
    String _term_id;        // термометр
    String _term_rezerv_id; // резервный термометр
    String _rele;           // реле котла (контура отопления)
    String _flame_id;       // статус горелки (isFlameOn)
    String _ch_id;          // статус контура отопления (isHeatingEnabled)

    float _gist = 0.1f;     // антидребезговый гистерезис, °C
    float _onK = 100;       // заблаговременность включения, % (0..100)
    float _offStop = 50;    // заблаговременность выключения, % (0..100)
    bool _burnAdj = true;   // адаптивный бюджет горелки по перегреву прошлого цикла
    float _burnK = 15;      // чувствительность бюджета горелки к перегреву (0..100, %)
    float _overGoal = 0.3f; // целевой инерционный перегрев, °C
    float _lagInitMin = 10; // начальная оценка инерции системы, мин
    float _burnInitMin = 20;// начальная оценка работы горелки за цикл, мин
    float _dropMax = 2;     // защита от разморозки: допустимое падение ниже заданной, °C
    float _overMax = 3;     // защита от перегрева: допустимое превышение заданной, °C
    float _maxHeatMin = 0;  // лимит непрерывного прогрева, мин (0 - выкл)
    float _minRestMin = 0;  // минимальная пауза между циклами, мин (0 - выкл)
    int _direction = 0;     // инверсия логики реле
    int _debug = 0;         // 1 - публиковать пояснения работы в MQTT (<id>/debug)
    int enable = 1;
    // измерения и скорость
    float pv = 0, sp = 0;
    unsigned long _lastMs = 0;
    bool _lastMsOk = false;
    unsigned long _lastPulseAt = 0; // millis последнего тактового пульса диагностики (троттлинг)
    float _pvPrev = 0;
    bool _pvPrevOk = false;
    float _rate = 0;            // сглаженная скорость изменения температуры, °C/сек
    // кольцевое окно последних 5 мгновенных скоростей (°C/сек) - источник метрики rate_per_hour;
    // заполняется в doByInterval() теми же отсечёнными от выбросов измерениями, что и EMA _rate,
    // усредняется в rateWinAvg(). После перезагрузки окно пусто: метрика = 0 до первых тиков.
    float _rateWin[5] = {0};
    uint8_t _rateWinCnt = 0;    // сколько валидных измерений накоплено (0..5)
    uint8_t _rateWinIdx = 0;    // следующая позиция записи в окно

    // состояние цикла прогрева
    bool _heating = false;      // идет прогрев
    bool _releOn = false;       // последнее состояние, отправленное на реле
    unsigned long _tOn = 0;     // момент включения котла
    unsigned long _tOff = 0;    // момент выключения котла
    unsigned long _tMin = 0;    // момент минимума температуры
    float _pvAtOn = 0;          // температура при включении
    float _pvAtOff = 0;         // температура при выключении
    float _minPv = 0;           // минимум температуры в цикле
    float _maxPv = 0;           // пик температуры после выключения
    bool _minDone = false;      // минимум зафиксирован (разворот вверх)
    bool _maxDone = false;      // пик зафиксирован
    float _burnSec = 0;         // работа горелки в режиме CH за цикл, сек
    float _heatSec = 0;         // длительность цикла прогрева, сек
    float _burnBudget = -1;     // бюджет горелки на цикл, сек (-1 - не задан)
    bool _cutByBudget = false;  // цикл прерван бюджетом (не учится в среднем)

    // обучаемая модель дома
    float _lagSec = 600;        // инерция: включение котла -> разворот температуры вверх
    float _dipAfterOn = 0.2f;   // проседание ниже температуры включения, °C
    float _riseAfterOff = 0.3f; // инерционный подъем после выключения, °C
    float _overPrev = -1;       // перегрев прошлого цикла (пик - заданная), °C (-1 - нет данных)
    float _burnPerCycle = 1200; // средняя работа горелки CH за цикл, сек
    float _coolRate = 0.00033f; // скорость остывания без отопления, °C/сек (положительная)
    float _heatRate = 0.00033f; // скорость прогрева с отоплением, °C/сек (положительная)
    unsigned long _starts = 0;  // пуски котла
    unsigned long _cycles = 0;  // завершенные циклы прогрева

    // защита обучения от внешнего нагрева (солнце)
    bool _extHeat = false;        // зафиксирован внешний нагрев - обучение пропускается
    bool _upTrend = false;        // температура растет при выключенном отоплении
    unsigned long _riseSince = 0; // начало непрерывного роста в паузе
    float _prevHeatSec = 0;       // длительность последнего завершенного прогрева, сек (рост после выключения может длиться до 2x)

    unsigned long _savedAt = 0;   // троттлинг записи модели на flash

    // диагностика предыдущих циклов для формирования рекомендаций (на основе самообучения)
    float _dipBelowSp = 0;        // просадка ниже заданной за цикл (EMA), °C
    float _cyclePeriodSec = 0;    // средний период между стартами котла (start-to-start), сек
    unsigned long _tStartPrev = 0;// время предыдущего старта котла, мс (0 - нет)
    float _minPvPause = 0;        // минимум температуры в текущей паузе, °C
    bool _minPvPauseOk = false;   // минимум паузы инициализирован
    bool _cycleResume = false;    // незавершенный прогрев восстановлен из чекпоинта - на первом такте заново подтвердим реле

    // диагностика входов и реле для метрик (пульс event:"state")
    bool _pvOkLast = false;       // термометр отвечал на последнем такте
    bool _spOkLast = false;       // заданная температура читалась
    bool _relOk = true;           // последняя команда на реле прошла
    bool _resUsed = false;        // на последнем такте работает по резервному термометру
    // снимок завершенного цикла прогрева для метрик (ключ prev_cycle в пульсе)
    float _pcBurnSec = 0;         // работа горелки за завершенный цикл, сек
    float _pcMinC = 0;            // минимум температуры за завершенный цикл, °C
    float _pcMaxC = 0;            // достигнутый пик после выключения, °C
    float _pcDipC = 0;            // просадка минимума ниже заданной, °C (0..)
    float _pcOverC = 0;           // перегрев пика над заданной, °C (0..)
    float _pcDipSec = 0;          // длительность падения температуры в прогреве прошлого цикла (включение->минимум), сек
    float _pcGrowSec = 0;         // длительность инерционного роста в остывании прошлого цикла (выключение->пик), сек
    float _pcFallSec = 0;         // длительность падения в остывании прошлого цикла (пик->включение), сек
    float _coolPrevSec = 0;       // длительность остывания прошлого цикла (выключение->включение) - эталон окна внешнего нагрева
    unsigned long _tMax = 0;      // момент пика текущего остывания (для длительностей роста/падения)
    bool _pcOk = false;           // есть завершенный цикл
    bool _pcMaxDone = false;      // пик зафиксирован (в паузе)
    bool _pcLearned = false;      // цикл вошел в самообучение (не бюджет, не короче 180 сек)
    String _pcStartKind = "unknown"; // как стартовал цикл: predict/hysteresis/freeze
    String _pcStoppedBy = "unknown"; // чем завершен: planned/budget/overheat/pred_overheat/max_heat/manual
    unsigned long _cyclesLearned = 0; // циклы, вошедшие в самообучение
    String _lastState = "";       // последнее изданное состояние (для дедупликации setState)
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
        bool hasOnK = jsonRead(parameters, "on_k", _onK);
        bool hasOffStop = jsonRead(parameters, "off_k", _offStop);
        if (!hasOnK) // совместимость со старым именем lead_k (коэффициент 0..3 -> проценты 0..100)
        {
            float leadK = 0;
            if (jsonRead(parameters, "lead_k", leadK)) _onK = (leadK <= 3.0f) ? leadK * 100.0f : leadK;
        }
        if (!hasOffStop) // совместимость со старым именем k_stop (доля 0..1 -> проценты 0..100)
        {
            float kStop = 0;
            if (jsonRead(parameters, "k_stop", kStop)) _offStop = (kStop <= 1.5f) ? kStop * 100.0f : kStop;
        }
        int burnAdj = 1;
        if (jsonRead(parameters, "burn_adj", burnAdj)) _burnAdj = (burnAdj != 0);
        jsonRead(parameters, "burn_k", _burnK);
        jsonRead(parameters, "over_goal", _overGoal);
        jsonRead(parameters, "lag_min", _lagInitMin);
        jsonRead(parameters, "burn_init", _burnInitMin);
        jsonRead(parameters, "drop_max", _dropMax);
        jsonRead(parameters, "over_max", _overMax);
        jsonRead(parameters, "max_heat_min", _maxHeatMin);
        jsonRead(parameters, "min_rest_min", _minRestMin);
        jsonRead(parameters, "direction", _direction);
        jsonRead(parameters, "debug", _debug);
        if (_onK < 0) _onK = 0;
        if (_onK > 100) _onK = 100;
        if (_offStop < 0) _offStop = 0;
        if (_offStop > 100) _offStop = 100;
        if (_burnK < 0) _burnK = 0;
        if (_burnK > 100) _burnK = 100;
        if (_gist < 0.02f) _gist = 0.02f;
        if (_lagInitMin < 1) _lagInitMin = 1;
        if (_burnInitMin < 1) _burnInitMin = 1;
        if (_dropMax < 0.2f) _dropMax = 0.2f;
        if (_overMax < 0.3f) _overMax = 0.3f;
        resetModel();
        loadLearn();
        setState(_heating ? "прогрев (восстановлен)" : "пауза");
    }

private:
    float ema(float oldV, float newV, float k)
    {
        return oldV + k * (newV - oldV);
    }

    // средняя мгновенная скорость за последние 5 тиков doByInterval (°C/сек);
    // при накоплении меньше 5 измерений усредняются те, что уже есть, 0 - нет данных
    float rateWinAvg()
    {
        if (_rateWinCnt == 0) return 0;
        float sum = 0;
        for (uint8_t i = 0; i < _rateWinCnt; i++) sum += _rateWin[i];
        return sum / _rateWinCnt;
    }

    String f0(float v) { return String(v, 0); }
    String f1(float v) { return String(v, 1); }
    String f2(float v) { return String(v, 2); }

    // setState() - издание состояния с дедупликацией. IoTItem::setValue() шлёт
    // рассылку в MQTT и на все WS-клиенты (regEvent) на вызов БЕЗ проверки на
    // изменение, а doByInterval() вызывается на каждом такте (60 с) с той же
    // строкой ("пауза" и т.п.) - лишний трафик и нагрузка в MQTT/WS, когда
    // канал и так нагружен. Идентичный текст пропускаем: первый вызов
    // (пустая _lastState) и реальные смены состояния публикуются как раньше.
    void setState(const String &s)
    {
        if (s == _lastState) return;
        _lastState = s;
        setValue(s);
    }

    // чтение числового значения элемента по ID
    bool readNum(const String &id, float &out)
    {
        if (id == "") return false;
        IoTItem *it = findIoTItem(id);
        if (!it) return false;
        out = ::atof(it->getValue().c_str());
        return true;
    }

    // интерпретация текстового состояния "включено": "1", "✅", "🔥..." или число > 0
    bool isOnText(const String &v)
    {
        if (v.indexOf("✅") >= 0 || v.indexOf("🔥") >= 0) return true;
        return v.toFloat() > 0.0001f;
    }

    bool flameOn()
    {
        if (_flame_id == "") return true; // нет датчика пламени - горелка считается работающей весь прогрев
        IoTItem *f = findIoTItem(_flame_id);
        if (!f) return true;
        return isOnText(f->getValue());
    }

    bool chOn()
    {
        if (_ch_id == "") return true; // нет статуса CH - считаем, что режим отопления активен
        IoTItem *c = findIoTItem(_ch_id);
        if (!c) return true;
        return isOnText(c->getValue());
    }

    bool setRele(bool on)
    {
        if (_releOn == on) return true; // не дублируем команды на реле
        IoTItem *r = findIoTItem(_rele);
        if (!r)
        {
            _relOk = false; // элемент реле не найден - фиксируем для метрик
            return false;
        }
        r->setValue(_direction ? (on ? "1" : "0") : (on ? "0" : "1"), true);
        _releOn = on;
        _relOk = true;
        return true;
    }

    // машинный код фазы управления для метрик (state_code):
    // 0 пауза, 1 прогрев, 2 зона заморозки, 3 ожидание min_rest, 4 выключен,
    // 5 ошибка входов (термометр/уставка/настройка), 6 ошибка реле, 7 пауза (солнце)
    int statusCode()
    {
        if (!_relOk) return 6;
        if (_rele == "" || !_pvOkLast || !_spOkLast) return 5;
        if (!enable) return 4;
        if (pv <= sp - _dropMax + 0.0001f) return 2;
        if (_heating) return 1;
        bool restOk = (_minRestMin <= 0) || (_tOff == 0) || (millis() - _tOff >= (unsigned long)(_minRestMin * 60000.0f));
        if (!restOk) return 3;
        if (_extHeat) return 7;
        return 0;
    }

    // рекомендации советника в машинной форме (для пульса метрик), формулы из advisor():
    // off_k подбирается по ФАКТИЧЕСКОЙ формуле порога выключения (calcOffSug, см.
    // stopThreshold), on_k растет при широких качелях (+15%) и просадке ниже заданной (+20%),
    // burn_k при перегреве предлагается поднять до 25%
    void suggestK(int &onSug, int &offSug, int &burnSug)
    {
        offSug = calcOffSug().value; // off_k: уравнение следующего цикла (обученный перегрев + антипульс)
        float over = (_overPrev > 0) ? _overPrev : 0;
        float dipBelow = (_dipBelowSp > 0) ? _dipBelowSp : 0;
        float swing = over + dipBelow;
        // on_k: поднимаем при широких качелях и/или просадке ниже заданной (максимум из применимых)
        onSug = (int)_onK;
        int add = 0;
        if (swing > 1.0f && add < 15) add = 15;
        if (dipBelow > 0.05f && add < 20) add = 20;
        onSug += add;
        if (onSug > 100) onSug = 100;
        if (onSug < 0) onSug = 0;
        // burn_k: при перегреве - активнее бюджет (как в советнике)
        burnSug = (int)_burnK;
        if (over > _overGoal + 0.05f && _burnK < 25) burnSug = 25;
        if (burnSug < 0) burnSug = 0;
        if (burnSug > 100) burnSug = 100;
    }

    // полный снимок состояния для тактового пульса метрик (топик <id>/debug): дописывается
    // в заранее выделенный буфер через jbAdd - без временных String (иначе каждое поле через
    // String на ESP - новая мелкая аллокация, десятки на каждый пульс);
    // единицы: температуры - °C, скорости - °C/ч, *_seconds - сек, burner_per_cycle, predict_burn_c - сек,
    // null - нет данных.
    void stateJson(char *buf, int cap, int &pos, const char *recText = nullptr)
    {
        int onSug, offSug, burnSug;
        suggestK(onSug, offSug, burnSug);
        bool sugOk = (_cycles >= 2);                       // рекомендации советника появляются после 2 циклов
        jbAdd(buf, cap, pos, "{\"hysteresis\":%.2f", (double)_gist);       // гистерезис
        jbAdd(buf, cap, pos, ",\"current_temperature\":%.2f", (double)pv); // текущая температура
        jbAdd(buf, cap, pos, ",\"rate_per_hour\":%.2f", (double)(rateWinAvg() * 3600.0f)); // средняя скорость за последние 5 тиков doByInterval, °C/ч
        jbAdd(buf, cap, pos, ",\"predict_on_c\":%.2f", (double)startThreshold());   // порог упреждающего включения
        jbAdd(buf, cap, pos, ",\"predict_off_c\":%.2f", (double)stopThreshold());   // порог планового выключения
        // бюджет горелки на цикл, сек (рассчитывается в heatStart; null - не рассчитан)
        if (_burnBudget >= 0.0f) jbAdd(buf, cap, pos, ",\"predict_burn_c\":%d", (int)(_burnBudget + 0.5f));
        else jbAdd(buf, cap, pos, ",\"predict_burn_c\":null");
        // рекомендации советника (null до 2 циклов)
        jbAdd(buf, cap, pos, ",\"recommended_on_k\":");
        if (sugOk) jbAdd(buf, cap, pos, "%d", onSug); else jbAdd(buf, cap, pos, "null");
        jbAdd(buf, cap, pos, ",\"recommended_off_k\":");
        if (sugOk) jbAdd(buf, cap, pos, "%d", offSug); else jbAdd(buf, cap, pos, "null");
        jbAdd(buf, cap, pos, ",\"recommended_burn_k\":");
        if (sugOk) jbAdd(buf, cap, pos, "%d", burnSug); else jbAdd(buf, cap, pos, "null");
        // ключ recommended: полный текст совета (advisor()) - только во внеплановом пульсе команды advice;
        // тексты советов не содержат кавычек и переносов строк - вставляются в JSON напрямую
        if (recText && recText[0]) jbAdd(buf, cap, pos, ",\"recommended\":\"%s\"", recText);
        // накопления текущего цикла
        jbAdd(buf, cap, pos, ",\"heat_seconds\":%.0f", (double)_heatSec);       // длительность текущего прогрева, сек
        jbAdd(buf, cap, pos, ",\"burner_seconds\":%.0f", (double)_burnSec);     // работа горелки в текущем цикле, сек
        jbAdd(buf, cap, pos, ",\"external_heat\":%d", (int)(_extHeat ? 1 : 0));  // фиксация внешнего нагрева (солнце)
        // снимок предыдущего цикла прогрева
        jbAdd(buf, cap, pos, ",\"prev_cycle\":");
        if (_pcOk)
        {
            jbAdd(buf, cap, pos, "{\"min_c\":%.2f", (double)_pcMinC);                // минимум цикла
            jbAddNum(buf, cap, pos, ",\"max_c\":", _pcMaxC, _pcMaxDone);             // достигнутый пик
            jbAddNum(buf, cap, pos, ",\"delta_c\":", _pcMaxC - _pcMinC, _pcMaxDone); // качели мин/макс
            jbAdd(buf, cap, pos, ",\"dip_below_c\":%.2f", (double)_pcDipC);          // просадка ниже заданной
            jbAddNum(buf, cap, pos, ",\"over_c\":", _pcOverC, _pcMaxDone);           // перегрев пика над заданной
            jbAddNum(buf, cap, pos, ",\"burn_sec\":", _pcBurnSec, _pcOk);            // работа горелки за завершенный прогрев, сек
            jbAddNum(buf, cap, pos, ",\"dip_sec\":", _pcDipSec, _pcOk);              // длительность падения температуры после включения (лаг), сек
            jbAddNum(buf, cap, pos, ",\"grow_sec\":", _pcGrowSec, _pcMaxDone);       // длительность инерционного роста после выключения, сек
            jbAddNum(buf, cap, pos, ",\"fall_sec\":", _pcFallSec, _pcMaxDone);       // длительность падения от пика до включения, сек
            jbAddNum(buf, cap, pos, ",\"cool_sec\":", _coolPrevSec, _pcOk);          // длительность полного остывания, сек
            jbAdd(buf, cap, pos, "}");
        }
        else jbAdd(buf, cap, pos, "null");
        // текущий цикл: минимум/пик в реальном времени (пик null до фиксации разворота)
        jbAdd(buf, cap, pos, ",\"curent_cycle\":");
        if (_starts > 0)
        {
            float overC = (_maxPv > sp) ? _maxPv - sp : 0;                           // перегрев пика над заданной
            jbAdd(buf, cap, pos, "{\"min_c\":%.2f", (double)_minPv);                 // минимум цикла
            jbAddNum(buf, cap, pos, ",\"max_c\":", _maxPv, _maxDone);                // достигнутый пик
            jbAddNum(buf, cap, pos, ",\"delta_c\":", _maxPv - _minPv, _maxDone);     // качели мин/макс
            jbAdd(buf, cap, pos, ",\"dip_below_c\":%.2f", (double)((sp - _minPv > 0) ? sp - _minPv : 0)); // просадка ниже заданной
            jbAddNum(buf, cap, pos, ",\"over_c\":", overC, _maxDone);                // перегрев пика над заданной
            jbAdd(buf, cap, pos, "}");
        }
        else jbAdd(buf, cap, pos, "null");
        // счетчики
        jbAdd(buf, cap, pos, ",\"burner_per_cycle\":%.0f", (double)_burnPerCycle); // средняя работа горелки за цикл, сек
        jbAdd(buf, cap, pos, ",\"boiler_starts\":%lu", _starts);                   // пуски котла
        jbAdd(buf, cap, pos, ",\"cycles_total\":%lu", _cycles);                    // завершенные циклы прогрева
        jbAdd(buf, cap, pos, "}");
    }

    // тактовый пульс метрик в MQTT (топик <id>/debug): машинный снимок state без
    // человекочитаемого message и без вывода в консоль/WS; единственный публикуемый
    // формат отладки (события-пояснения отменены). Чтобы не собирать сообщение из десятков
    // временных String на каждый такт и не заливать брокер трафиком:
    // 1) пульс собирается в заранее выделенный буфер pulseMsgBuf (без аллокаций);
    // 2) тактовые пульсы ограничены частотой PULSE_PERIOD_MS (не чаще раза в 30 сек);
    //    внеплановые (force=true) уходят сразу: смена состояния реле (heatStop), включение debug
    void publishState(bool force = false, const char *recText = nullptr) // recText - текст совета для внепланового пульса команды advice
    {
        if (!_debug || !mqttIsConnect()) return;
        unsigned long now = millis();
        if (!force && _lastPulseAt && now - _lastPulseAt < PULSE_PERIOD_MS) return;
        _lastPulseAt = now;
        int pos = 0;
        String timeS = getTimeLocal_hhmmss();
        jbAdd(pulseMsgBuf, PULSE_BUF_CAP, pos, "{\"event\":\"state\"");
        jbAdd(pulseMsgBuf, PULSE_BUF_CAP, pos, ",\"time\":\"%s\"", timeS.c_str());
        time_t ts = getSystemTime();
        if (ts > 0) jbAdd(pulseMsgBuf, PULSE_BUF_CAP, pos, ",\"ts\":%lu", (unsigned long)ts);
        jbAdd(pulseMsgBuf, PULSE_BUF_CAP, pos, ",\"interval_sec\":%d", (int)(getInterval() / 1000));
        jbAdd(pulseMsgBuf, PULSE_BUF_CAP, pos, ",\"state\":");
        stateJson(pulseMsgBuf, PULSE_BUF_CAP, pos, recText);
        jbAdd(pulseMsgBuf, PULSE_BUF_CAP, pos, "}");
        publishData(String(_id.c_str()) + "/debug", String(pulseMsgBuf));
    }

    // пояснения работы в консоль и WS-лог (в MQTT <id>/debug публикуется только
    // тактовый пульс метрик publishState - события-пояснения отменены)
    void say(const String &msg)
    {
        SerialPrint("i", F("Термостат Predict"), msg);
    }

    // обученная просадка после включения котла, °C: только самообучение - уровень
    // просадки прошлых циклов (EMA, в первую очередь предыдущий), без мгновенной скорости
    float predictedDip()
    {
        float d = _dipAfterOn;
        if (d < 0) d = 0;
        if (d > _dropMax) d = _dropMax;
        return d;
    }

    // порог упреждающего включения, °C: уровень гистерезисного включения (заданная
    // минус gist) плюс доля on_k от обученной просадки; выше гистерезисного порога
    // выключения (заданная плюс gist) старт не поднимается никогда
    float startThreshold()
    {
        float thr = (sp - _gist) + (_onK / 100.0f) * predictedDip();
        if (thr > sp + _gist) thr = sp + _gist;
        return thr;
    }

    // порог планового выключения, °C: отсчитывается от уровня гистерезисного ВЫКЛЮЧЕНИЯ
    // (заданная плюс gist) вниз; обученная составляющая - уровень перегрева прошлых
    // циклов (EMA измеренного перегрева, в первую очередь предыдущего), off_k задает
    // его долю; антипульс: порог не опускается ниже точки старта плюс gist, чтобы
    // зона прогрева котла не выворачивалась
    float stopThreshold()
    {
        float base = sp + _gist;
        float overLearn = (_overPrev >= 0) ? _overPrev : 0; // обученный перегрев, °C
        float thr = base - (_offStop / 100.0f) * overLearn;
        float minThr = startThreshold() + _gist; // минимальная рабочая полоса котла
        if (thr < minThr) thr = minThr;
        return thr;
    }

    // Рекомендация off_k по ФАКТИЧЕСКОЙ формуле порога выключения (см. stopThreshold).
    // Порог = (заданная + gist) - off_k/100 * обученный перегрев (_overPrev), но не ниже
    // startThreshold() + gist = заданная + floorAboveSp (антипульс). Пик следующего цикла =
    // порог + инерционный подъем rise, поэтому перегрев следующего цикла:
    //   over_next = gist + rise - off_k/100 * overPrev   (пока порог не уперся в антипульс)
    // Решая over_next = over_goal, советуем off_k = 100 * (gist + rise - over_goal) / overPrev -
    // уравнение СЛЕДУЮЩЕГО цикла с текущим обученным перегревом. Равновесная формула
    // (gist + rise) / (1 + off_k/100) - 1 игнорировала _overPrev и нижнее ограничение порога,
    // что давало существенно завышенные или заниженные советы. Если идеальный off_k доталкивает
    // порог до нижней границы (колено off_k = 100 * (gist - floorAboveSp) / overPrev),
    // дальнейшее увеличение коэффициента порог уже не опускает - советуем только колено и
    // показываем неустранимый перегрев residualOver (снижается только уменьшением on_k).
    struct OffSugResult
    {
        int value;          // рекомендуемый off_k, %
        bool noLearned;     // обученного перегрева нет - off_k на порог выключения не влияет
        bool floorBinds;    // колено антипульса достигнуто - выше value порог не опустится
        float residualOver; // неустранимый перегрев при активном антипульсе (floorAboveSp + rise), °C
        float floorAboveSp; // нижняя граница порога выключения над заданной, °C
    };

    OffSugResult calcOffSug()
    {
        OffSugResult r;
        r.value = 0;
        r.noLearned = false;
        r.floorBinds = false;
        r.residualOver = -1;
        r.floorAboveSp = 0;
        float overPrev = _overPrev; // обученный перегрев прошлых циклов (EMA), °C
        if (overPrev < 0.0001f)
        {
            r.noLearned = true; // порог выключения равен (заданная + gist) при любом off_k
            return r;
        }
        float rise = _riseAfterOff;
        if (rise < 0.05f) rise = 0.05f; // страховка от деления на ~0
        float overGoalGuard = (_overGoal >= 0.05f) ? _overGoal : 0.05f;
        // нижнее ограничение stopThreshold(): заданная + min(on_k% обученной просадки, 2*gist)
        r.floorAboveSp = (_onK / 100.0f) * predictedDip();
        if (r.floorAboveSp > 2.0f * _gist) r.floorAboveSp = 2.0f * _gist;
        float sug = 100.0f * (_gist + rise - overGoalGuard) / overPrev; // цель на следующий цикл
        float knee = 100.0f * (_gist - r.floorAboveSp) / overPrev;      // колено антипульса
        if (knee < 0) knee = 0; // антипульс держит порог выше гист. уровня уже при off_k = 0
        if (sug > knee)
        {
            r.floorBinds = true;
            r.residualOver = r.floorAboveSp + rise; // перегрев, который off_k уже не уберет
            sug = knee; // советовать коэффициент выше колена бессмысленно - порог не сдвинется
        }
        if (sug < 0) sug = 0;
        if (sug > 100) sug = 100;
        r.value = (int)(sug + 0.5f);
        return r;
    }

    // окно инерции после выключения, сек: в течение него радиаторы отдают запасенное тепло;
    // рост температуры за пределами окна - не от инерции котла (солнце/другие источники)
    unsigned long inertiaWindowSec()
    {
        unsigned long w = (unsigned long)(2.0f * _lagSec);
        if (w < 600UL) w = 600UL;    // минимум 10 минут
        if (w > 2700UL) w = 2700UL;  // максимум 45 минут
        return w;
    }
    // окно допустимого роста без отопления, сек: радиаторы отдают запасенное тепло тем дольше,
    // чем дольше грел котел, поэтому если последний прогрев был долгим, температура может
    // продолжать расти значительно дольше окна инерции; берем большее из окна инерции и
    // удвоенного последнего прогрева (максимум 2 часа), пока прогрев неизвестен - окно инерции
    unsigned long riseWindowSec()
    {
        unsigned long w = inertiaWindowSec();
        if (_prevHeatSec > 0)
        {
            unsigned long w2 = (unsigned long)(2.0f * _prevHeatSec);
            if (w2 > w) w = w2;
            if (w > 7200UL) w = 7200UL; // максимум 2 часа, чтобы солнце не детектировалось "вечно"
        }
        return w;
    }
    // самообучение: модель хранится на flash отдельным ключом "<id>_L" в valuesFlashJson
    String learnKey() { return String(_id.c_str()) + "_L"; }

    // таймеры цикла/паузы хранятся как "сколько секунд назад было событие": millis() обнуляется при перезагрузке,
    // поэтому разности вида (now - _tOn) после восстановления остаются корректными
    String agoS(unsigned long t) { return t ? f0((float)((millis() - t) / 1000UL)) : "0"; }
    void agoSet(unsigned long &dst, const String &j, const String &key)
    {
        float v = 0;
        dst = (jsonRead(j, key, v, false) && v > 0) ? millis() - (unsigned long)(v * 1000.0f + 0.5f) : 0;
    }

    void saveLearn(bool force = false)
    {
        unsigned long now = millis();
        if (!force && _savedAt && (now - _savedAt < 300000UL)) return; // запись не чаще 5 минут
        _savedAt = now;
        String j = "{";
        j += "\"lag\":" + f0(_lagSec);
        j += ",\"dip\":" + f2(_dipAfterOn);
        j += ",\"rise\":" + f2(_riseAfterOff);
        j += ",\"over\":" + f2(_overPrev < 0 ? 0.0f : _overPrev);
        j += ",\"hasover\":" + String(_overPrev >= 0 ? 1 : 0);
        j += ",\"burn\":" + f0(_burnPerCycle);
        j += ",\"cr\":" + String(_coolRate * 60.0f, 5); // храним в °C/мин для читаемости
        j += ",\"hr\":" + String(_heatRate * 60.0f, 5);
        j += ",\"n\":" + String(_cycles);
        j += ",\"dbs\":" + f2(_dipBelowSp);          // просадка ниже заданной, °C
        j += ",\"cps\":" + f0(_cyclePeriodSec);       // период старт-к-старту, сек
        // чекпоинт текущего цикла/паузы: при перезагрузке накопленные измерения не теряются
        j += ",\"en\":" + String(enable ? 1 : 0);      // термостат включен
        j += ",\"st\":" + String(_heating ? 1 : 0);    // идет ли прогрев на момент записи
        j += ",\"stt\":" + f0((float)_starts);         // пуски котла
        j += ",\"bs\":" + f1(_burnSec);                // работа горелки в текущем цикле, сек
        j += ",\"hs\":" + f1(_heatSec);                // длительность текущего цикла, сек
        j += ",\"ph\":" + f1(_prevHeatSec);            // последний завершенный прогрев, сек (для окна допустимого роста)
        j += ",\"cb\":" + String(_cutByBudget ? 1 : 0); // цикл прерван бюджетом
        j += ",\"bb\":" + f0(_burnBudget < 0 ? -1 : _burnBudget); // бюджет горелки, сек
        j += ",\"rt\":" + String(_rate * 60.0f, 4);    // скорость температуры, °C/мин
        j += ",\"mnp\":" + f2(_minPv);                 // минимум температуры в цикле
        j += ",\"pvaon\":" + f2(_pvAtOn);              // температура при включении
        j += ",\"mnok\":" + String(_minDone ? 1 : 0);  // разворот после минимума зафиксирован
        j += ",\"mxp\":" + f2(_maxPv);                 // пик после выключения
        j += ",\"pvaoff\":" + f2(_pvAtOff);            // температура при выключении
        j += ",\"mxok\":" + String(_maxDone ? 1 : 0);  // пик зафиксирован
        j += ",\"mpp\":" + f2(_minPvPause);            // минимум температуры в паузе
        j += ",\"mpok\":" + String(_minPvPauseOk ? 1 : 0);
        j += ",\"up\":" + String(_upTrend ? 1 : 0);    // непрерывный рост в паузе
        j += ",\"eh\":" + String(_extHeat ? 1 : 0);    // внешний нагрев (солнце)
        j += ",\"ton\":" + agoS(_tOn);                 // таймеры - сколько секунд назад было событие
        j += ",\"tmin\":" + agoS(_tMin);
        j += ",\"toff\":" + agoS(_tOff);
        j += ",\"tsp\":" + agoS(_tStartPrev);
        j += ",\"rs\":" + agoS(_riseSince);
        // счетчик обученных циклов и снимок завершенного цикла (метрики prev_cycle)
        j += ",\"cn\":" + String(_cyclesLearned);
        j += ",\"pcok\":" + String(_pcOk ? 1 : 0);
        j += ",\"pcmd\":" + String(_pcMaxDone ? 1 : 0);
        j += ",\"pcln\":" + String(_pcLearned ? 1 : 0);
        j += ",\"pcbs\":" + f0(_pcBurnSec);
        j += ",\"pcmn\":" + f2(_pcMinC);
        j += ",\"pcmx\":" + f2(_pcMaxC);
        j += ",\"pcdp\":" + f2(_pcDipC);
        j += ",\"pcov\":" + f2(_pcOverC);
        j += ",\"pcds\":" + f0(_pcDipSec);           // длительность падения после включения (прошлый прогрев), сек
        j += ",\"pcg\":" + f0(_pcGrowSec);           // длительность инерционного роста прошлого остывания, сек
        j += ",\"pcf\":" + f0(_pcFallSec);           // длительность падения прошлого остывания, сек
        j += ",\"cpl\":" + f0(_coolPrevSec);         // длительность полного остывания прошлого цикла, сек
        j += ",\"tmx\":" + agoS(_tMax);              // сколько секунд назад был пик текущего остывания
        j += ",\"pcsk\":\"" + _pcStartKind + "\"";
        j += ",\"pcsp\":\"" + _pcStoppedBy + "\"";
        j += "}";
        jsonWriteStr_(valuesFlashJson, learnKey(), j);
        needSaveValues = true;
    }

    void loadLearn()
    {
        String j = "";
        if (!jsonRead(valuesFlashJson, learnKey(), j, false) || j == "") return;
        float crM = 0, hrM = 0;
        jsonRead(j, "lag", _lagSec, false);
        jsonRead(j, "dip", _dipAfterOn, false);
        jsonRead(j, "rise", _riseAfterOff, false);
        jsonRead(j, "burn", _burnPerCycle, false);
        jsonRead(j, "cr", crM, false);
        jsonRead(j, "hr", hrM, false);
        if (crM > 0.0001f) _coolRate = crM / 60.0f;
        if (hrM > 0.0001f) _heatRate = hrM / 60.0f;
        int hasover = 0;
        jsonRead(j, "hasover", hasover, false);
        float over = 0;
        jsonRead(j, "over", over, false);
        _overPrev = hasover ? over : -1;
        long n = 0;
        jsonRead(j, "n", n, false);
        _cycles = (unsigned long)n;
        jsonRead(j, "dbs", _dipBelowSp, false);
        jsonRead(j, "cps", _cyclePeriodSec, false);
        long ncl = 0;
        jsonRead(j, "cn", ncl, false);
        _cyclesLearned = (unsigned long)ncl;
        // накопленные измерения текущего цикла/паузы из чекпоинта; поля, отсутствующие в JSON,
        // остаются со значениями по умолчанию (это обеспечивает совместимость со старым форматом)
        float f = 0;
        jsonRead(j, "bs", _burnSec, false);
        jsonRead(j, "hs", _heatSec, false);
        jsonRead(j, "ph", _prevHeatSec, false);
        jsonRead(j, "mnp", _minPv, false);
        jsonRead(j, "pvaon", _pvAtOn, false);
        jsonRead(j, "mxp", _maxPv, false);
        jsonRead(j, "pvaoff", _pvAtOff, false);
        jsonRead(j, "mpp", _minPvPause, false);
        f = 0;
        jsonRead(j, "rt", f, false);
        _rate = f / 60.0f;
        int flag = 0;
        jsonRead(j, "cb", flag, false);
        _cutByBudget = (flag != 0);
        jsonRead(j, "mnok", flag, false);
        _minDone = (flag != 0);
        jsonRead(j, "mxok", flag, false);
        _maxDone = (flag != 0);
        jsonRead(j, "mpok", flag, false);
        _minPvPauseOk = (flag != 0);
        jsonRead(j, "up", flag, false);
        _upTrend = (flag != 0);
        jsonRead(j, "eh", flag, false);
        _extHeat = (flag != 0);
        // снимок завершенного цикла для метрик prev_cycle
        jsonRead(j, "pcsk", _pcStartKind, false);
        jsonRead(j, "pcsp", _pcStoppedBy, false);
        f = 0;
        jsonRead(j, "pcbs", f, false);
        _pcBurnSec = f;
        jsonRead(j, "pcmn", f, false);
        _pcMinC = f;
        jsonRead(j, "pcmx", f, false);
        _pcMaxC = f;
        jsonRead(j, "pcdp", f, false);
        _pcDipC = f;
        jsonRead(j, "pcov", f, false);
        _pcOverC = f;
        jsonRead(j, "pcds", f, false);
        _pcDipSec = f;
        jsonRead(j, "pcg", f, false);
        _pcGrowSec = f;
        jsonRead(j, "pcf", f, false);
        _pcFallSec = f;
        jsonRead(j, "cpl", f, false);
        _coolPrevSec = f;
        flag = 0;
        jsonRead(j, "pcok", flag, false);
        _pcOk = (flag != 0);
        jsonRead(j, "pcmd", flag, false);
        _pcMaxDone = (flag != 0);
        jsonRead(j, "pcln", flag, false);
        _pcLearned = (flag != 0);
        f = -1;
        jsonRead(j, "bb", f, false);
        _burnBudget = f;
        f = 0;
        jsonRead(j, "stt", f, false);
        _starts = (unsigned long)f;
        agoSet(_tOn, j, "ton");
        agoSet(_tMin, j, "tmin");
        agoSet(_tOff, j, "toff");
        agoSet(_tStartPrev, j, "tsp");
        agoSet(_riseSince, j, "rs");
        agoSet(_tMax, j, "tmx");
        int en = 1;
        jsonRead(j, "en", en, false);
        enable = en ? 1 : 0;
        int st = 0;
        jsonRead(j, "st", st, false);
        if (st == 1 && enable)
        {
            // перезагрузка пришлась на прогрев: продолжаем цикл с накопленными измерениями;
            // реальное состояние реле после загрузки неизвестно - подтвердим команду на первом такте
            _heating = true;
            _releOn = false;
            _cycleResume = true;
        }
        _savedAt = millis();
        say("модель дома и измерения восстановлены из памяти: " + modelInfo() + (enable ? "" : "; термостат выключен") + (_heating ? String("; незавершенный прогрев продолжится (накоплено " + f1(_heatSec / 60.0f) + " мин, горелка " + f1(_burnSec / 60.0f) + " мин)") : ""));
    }

    // сброс модели к начальным оценкам (команда reset или первый запуск)
    void resetModel()
    {
        _coolRate = _heatRate = 0.02f / 60.0f; // справочно: ~1 °C за 50 минут
        _lagSec = _lagInitMin * 60;
        _dipAfterOn = _coolRate * _lagSec;
        _riseAfterOff = _overGoal;
        _burnPerCycle = _burnInitMin * 60;
        _overPrev = -1;
        _cycles = 0;
        _burnBudget = -1;
        _cutByBudget = false;
        _extHeat = false;
        _upTrend = false;
        _dipBelowSp = 0;
        _cyclePeriodSec = 0;
        _tStartPrev = 0;
        _minPvPause = 0;
        _minPvPauseOk = false;
        _prevHeatSec = 0;
        _coolPrevSec = 0;
        _pcDipSec = _pcGrowSec = _pcFallSec = 0;
        _tMax = 0;
    }

    String modelInfo()
    {
        String r = "проседание после включения " + f2(_dipAfterOn) + " °C за " + f1(_lagSec / 60.0f) + " мин";
        r += ", подъем после выключения " + f2(_riseAfterOff) + " °C";
        r += ", горелка/цикл " + f1(_burnPerCycle / 60.0f) + " мин";
        r += ", остывание " + f2(_coolRate * 3600.0f) + " °C/ч, прогрев " + f2(_heatRate * 3600.0f) + " °C/ч";
        r += ", циклов " + String(_cycles);
        if (_overPrev >= 0) r += ", обученный перегрев прошлых циклов " + f2(_overPrev) + " °C (цель " + f2(_overGoal) + " °C)";
        return r;
    }

    // Анализ предыдущих циклов прогрева и рекомендации по корректировке коэффициентов.
    // Формулируется ТОЛЬКО на основе самообученной модели (пики, развороты, длительность
    // пауз и работа горелки накопленных циклов усредняются).
    // Каждая рекомендация приводит конкретное значение коэффициента и ожидаемый эффект.
    // Вызывается вручную командой thermostat.advice(), ответ - текст text в консоль/WS-лог.
    void advisor(String &text)
    {
        text = "";
        if (_cycles < 2)
        {
            text = "СОВЕТЫ Predict появятся после нескольких завершенных циклов прогрева (сейчас завершено " + String(_cycles) + "): модель учится на каждом пике и развороте температуры, затем подскажет, как скорректировать коэффициенты";
            return;
        }
        float rise = _riseAfterOff;
        if (rise < 0.05f) rise = 0.05f;           // страховка от деления на ~0
        float over = (_overPrev > 0) ? _overPrev : 0;   // перегрев прошлого цикла
        float dipBelow = (_dipBelowSp > 0) ? _dipBelowSp : 0; // просадка ниже заданной за паузу
        float swing = over + dipBelow;            // суммарные качели вокруг заданной
        float spH = (_cyclePeriodSec > 0) ? 3600.0f / _cyclePeriodSec : 0; // стартов котла в час
        // off_k под цель перегрева - по ФАКТИЧЕСКОЙ формуле порога выключения (см. calcOffSug):
        // перегрев следующего цикла = gist + подъем - off_k/100 * обученный перегрев, а после
        // упора порога в нижнюю границу антипульса off_k вообще перестает его опускать.
        // Равновесная формула (gist + подъем) / (1 + off_k/100) не учитывала ни _overPrev,
        // ни нижнее ограничение - советы выходили существенно завышенными или заниженными.
        OffSugResult off = calcOffSug();
        int offSug = off.value;

        String r = "СОВЕТЫ Predict по коэффициентам (анализ " + String(_cycles) + " циклов: перегрев " + f2(over) + " °C при цели " + f2(_overGoal) + " °C, просадка ниже заданной " + f2(dipBelow) + " °C, качели ~" + f2(swing) + " °C";
        if (spH > 0) r += ", стартов ~" + f1(spH) + "/ч";
        r += ")";

        // 1) Перегрев помещения: пик выше заданной больше цели
        if (_overPrev > _overGoal + 0.05f)
        {
            String problem = String("ПЕРЕГРЕВ: пик выше заданной на ") + f2(over) + " °C (цель " + f2(_overGoal) + " °C), радиаторы догревают комнату на " + f2(rise) + " °C после выключения";
            String suggestion;
            if (off.floorBinds)
            {
                // порог уперся в нижнюю границу антипульса (заданная + on_k% обученной просадки):
                // выше колена off_k порог не опустит, остаточный перегрев снимет только уменьшение on_k
                if (_offStop >= (float)offSug)
                    suggestion = String("off_k = ") + f0(_offStop) + "% уже прижал порог выключения к нижней границе (заданная + " + f2(off.floorAboveSp) + " °C, on_k% обученной просадки) - увеличение off_k порог больше не изменит; остаточный перегрев ~" + f2(off.residualOver) + " °C снизит только уменьшение on_k (сейчас " + f0(_onK) + "%)";
                else
                    suggestion = String("увеличьте off_k с ") + f0(_offStop) + "% до ~" + String(offSug) + "% (set_off_k) - порог выключения опустится до нижней границы (заданная + " + f2(off.floorAboveSp) + " °C); выше ~" + String(offSug) + "% off_k порог уже не опустит, оставшийся перегрев ~" + f2(off.residualOver) + " °C снизит только уменьшение on_k";
            }
            else
            {
                suggestion = String("увеличьте off_k с ") + f0(_offStop) + "% до ~" + String(offSug) + "% (set_off_k) - по обученному перегреву " + f2(_overPrev) + " °C и подъему " + f2(rise) + " °C такой порог выключения спланирует пик на цель " + f2(_overGoal) + " °C";
            }
            if (!_burnAdj) suggestion += ", включите burn_adj=1";
            else if (_burnK < 25) suggestion += String(", поднимите burn_k с ") + f0(_burnK) + "% до ~25%, чтобы бюджет горелки активнее укорачивал перегретый цикл";
            r += ". " + problem + " - " + suggestion;
        }
        // 2) Температурные качели: большой размах вокруг заданной
        if (swing > 1.0f)
        {
            int onSug = (_onK < 100) ? ((int)_onK + 15) : 100;
            if (onSug > 100) onSug = 100;
            String problem = String("КАЧЕЛИ ~") + f2(swing) + " °C (вниз " + f2(dipBelow) + " + вверх " + f2(over) + " °C)";
            String suggestion = String("увеличьте on_k до ~") + String(onSug) + "% (set_on_k)";
            // без обученного перегрева off_k на порог выключения не влияет; не советуем и бесполезное
            // увеличение (порог уже на цели или уперся в нижнюю границу антипульса)
            if (!off.noLearned && offSug > (int)_offStop) suggestion += String(" и off_k до ~") + String(offSug) + "% (set_off_k)";
            suggestion += " - старт поднимется выше гистерезисного порога, стоп станет заблаговременным и качели сожмутся (учтите: слегка возрастет частота пусков)";
            r += ". " + problem + ": " + suggestion;
        }
        // 3) Просадка ниже заданной: в паузе комната проваливалась ниже заданной
        if (dipBelow > 0.05f)
        {
            int onSug = (_onK < 100) ? ((int)_onK + 20) : 100;
            if (onSug > 100) onSug = 100;
            String problem = String("ПРОСАДКА ниже заданной на ") + f2(dipBelow) + " °C (инерция системы " + f1(_lagSec / 60.0f) + " мин)";
            String suggestion = String("увеличьте on_k с ") + f0(_onK) + "% до ~" + String(onSug) + "% (set_on_k), чтобы котел стартовал раньше и не успевал провалиться";
            if (onSug >= 100) suggestion += String("; при on_k=100 просадка остается - котел не успевает догревать за время инерции (ниже ") + f1(_dropMax) + " °C не упадет, срабатывает защита от замерзания)";
            r += ". " + problem + ": " + suggestion;
        }
        // 4) Частота пусков: слишком частое включение котла
        if (spH > 5.0f)
        {
            int restSug = (int)((3600.0f / 5.0f - _cyclePeriodSec) / 60.0f + 0.5f);
            if (restSug < 1) restSug = 1;
            String problem = String("ЧАСТЫЕ ПУСКИ ~") + f1(spH) + "/ч";
            String suggestion = String("увеличьте min_rest_min до ~") + String(restSug) + " мин (минимальная пауза между циклами), чтобы число стартов упало";
            suggestion += String("; либо умеренно увеличьте over_goal до ~") + f1(_overGoal + 0.2f) + " °C и снизьте on_k - циклы станут длиннее и реже (компромисс: чуть больше качель)";
            r += ". " + problem + ": " + suggestion;
        }
        // все показатели в норме
        if (_overPrev <= _overGoal + 0.05f && swing <= 1.0f && dipBelow <= 0.05f && (spH <= 5.0f || spH == 0))
        {
            r += ". Все показатели в норме (перегрев, качели, просадка, частота пусков) - коэффициенты менять не требуется";
        }
        text = r;
    }
    bool heatStart(const String &kind)
    {
        unsigned long now = millis();
        if (!setRele(true))
        {
            setState("ошибка управления реле");
            return false;
        }
        _heating = true;
        _starts++;
        // просадка ниже заданной за прошлую паузу (минимум паузы относительно заданной) - EMA
        if (_minPvPauseOk)
        {
            float below = sp - _minPvPause;
            if (below > 0)
                _dipBelowSp = (_dipBelowSp > 0) ? ema(_dipBelowSp, below, 0.3f) : below;
        }
        // период между стартами (start-to-start) по истории - основа оценки числа включений в час
        if (_tStartPrev > 0 && (long)(now - _tStartPrev) > 0) // (long) - корректно, когда millis() обнулился после перезагрузки
        {
            float period = (now - _tStartPrev) / 1000.0f;
            if (period > 60 && period < 6 * 3600)
                _cyclePeriodSec = (_cyclePeriodSec > 0) ? ema(_cyclePeriodSec, period, 0.3f) : period;
        }
        _tStartPrev = now;
        // === НАЧАЛО ЦИКЛА (этап прогрев): финализируем завершившееся остывание ===
        // Переносим в статистику предыдущего цикла запомненные за "этап остывание" значения -
        // максимальную температуру (пик) и длительности роста/падения - только если они были
        // измерены. Заодно фиксируем полную длительность остывания (выключение->включение):
        // она станет эталоном "предыдущего остывания" для окна внешнего нагрева (2x) в новой паузе.
        if (_tOff > 0)
        {
            float coolNow = (now - _tOff) / 1000.0f;
            if (coolNow < 0) coolNow = 0;
            _coolPrevSec = coolNow;
            if (_maxDone && _tMax >= _tOff) // пик был измерен и зафиксирован в этой паузе
            {
                float over = _maxPv - sp;
                _pcMaxC = _maxPv;                       // перенос запомненного максимума в prev
                _pcOverC = (over > 0) ? over : 0;
                _pcMaxDone = true;
                _pcGrowSec = (_tMax - _tOff) / 1000.0f; // длительность роста (инерционный подъем)
                _pcFallSec = (now - _tMax) / 1000.0f;   // длительность падения (от пика до включения)
            }
        }
        // порог выключения predict_off_c далее рассчитывается автоматически (stopThreshold()):
        // он учитывает свежий перегрев прошлого цикла (_overPrev зафиксирован в паузе), смещая
        // порог на долю off_k; EMA сглаживает незначительные перегревы в незначительные правки
        _tOn = now;
        _pvAtOn = pv;
        _minPv = pv;
        _tMin = now;
        _minDone = false;
        _maxDone = false;
        _extHeat = false;
        _upTrend = false;
        _burnSec = 0;
        _heatSec = 0;
        _cutByBudget = false;
        _pcStartKind = (kind == "заморозка") ? "freeze" : ((kind == "упреждение") ? "predict" : "hysteresis"); // токен старта для метрик prev_cycle
        // бюджет горелки на цикл: в первую очередь - обученный уровень перегрева прошлого
        // цикла относительно цели (over_goal): перегрев, пересчитанный в секунды через
        // обученную скорость прогрева и масштабированный burn_k, корректирует среднюю
        // длительность (перегрели - цикл короче, недогрели - длиннее)
        _burnBudget = -1;
        if (_burnAdj && _overPrev >= 0 && _burnPerCycle > 120)
        {
            float heatRate = (_heatRate > 0.00005f) ? _heatRate : 0.00005f; // °C/сек, защита от деления на ~0
            float corr = (_burnK / 100.0f) * (_overPrev - _overGoal) / heatRate; // поправка перегрева, сек
            _burnBudget = _burnPerCycle - corr;
            if (_burnBudget < 60.0f) _burnBudget = 60.0f;
            if (_burnBudget > 2.0f * _burnPerCycle) _burnBudget = 2.0f * _burnPerCycle;
        }
        setState(kind == "заморозка" ? "защита от замерзания" : "прогрев");
        saveLearn(true); // старт цикла сразу фиксируем на flash: перезагрузка в любой момент восстановит его
        return true;
    }
    bool heatStop(bool planned, const String &code)
    {
        unsigned long now = millis();
        if (!setRele(false))
        {
            setState("ошибка управления реле");
            publishState(true); // внеплановый пульс: состояние реле/ошибка - сразу в MQTT, мимо троттлинга
            return false;
        }
        _heating = false;
        _prevHeatSec = _heatSec; // окно допустимого роста в предстоящей паузе станет 2x этого прогрева
        _cycles++;
        _tOff = now;
        _pvAtOff = pv;
        _maxPv = pv;
        _tMax = now;          // пик нового остывания отслеживаем от момента выключения
        _maxDone = false;
        _upTrend = true;      // после выключения ожидаем инерционный подъем
        _riseSince = now;
        _extHeat = false;
        _minPvPause = pv;     // отслеживаем минимум в предстоящей паузе (просадка ниже заданной)
        _minPvPauseOk = true;
        // обучение средней работе горелки: циклы, прерванные бюджетом (занижены) - не учим
        bool burnLearned = false;
        if (!_cutByBudget && _heatSec > 180)
        {
            _burnPerCycle = ema(_burnPerCycle, _burnSec, 0.3f);
            burnLearned = true;
        }
        _cutByBudget = false;
        // === СЕРЕДИНА ЦИКЛА (прогрев завершен, начинается остывание): переносим в статистику
        // предыдущего цикла запомненные за прогрев значения: работу горелки и длительность
        // падения температуры после включения (лаг; валидна только при зафиксированном развороте)
        _pcBurnSec = _burnSec;
        _pcDipSec = (_minDone && _tMin >= _tOn) ? (_tMin - _tOn) / 1000.0f : 0;
        // снимок завершенного цикла для метрик prev_cycle (пик добавится позже, в паузе)
        _pcMinC = _minPv;
        _pcDipC = (sp - _minPv > 0) ? sp - _minPv : 0;
        _pcMaxDone = false;
        _pcLearned = burnLearned;
        _pcStoppedBy = code;
        _pcOk = true;
        if (burnLearned) _cyclesLearned++;
        setState(planned ? "пауза (по расчету)" : "пауза (защита)");
        saveLearn(true); // итог цикла фиксируем сразу, без троттлинга - иначе перезагрузка может потерять последний цикл
        publishState(true); // внеплановый пульс сразу после изменения состояния реле (мимо троттлинга)
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
            dtOk = (dt > 0 && dt <= 1800);
        }
        _lastMs = now;
        _lastMsOk = true;

        float v = 0;
        bool spOk = readNum(_set_id, v);
        _spOkLast = spOk; // статус уставки для метрик
        if (spOk) sp = v;

        float pvMain = 0, pvRes = 0;
        bool mainOk = readNum(_term_id, pvMain) && pvMain > -40 && pvMain < 120;
        bool resOk = readNum(_term_rezerv_id, pvRes) && pvRes > -40 && pvRes < 120;
        bool resUsed = false;
        bool pvOk = false;
        if (mainOk)
        {
            pv = pvMain;
            pvOk = true;
        }
        else if (resOk)
        {
            pv = pvRes;
            pvOk = true;
            resUsed = true;
        }
        _pvOkLast = pvOk;   // статус термометра для метрик
        _resUsed = resUsed; // работа по резервному датчику

        if (!spOk || _rele == "")
        {
            _pvPrevOk = false;
            setState("ошибка настройки термостата");
            publishState();
            return;
        }
        if (!pvOk)
        {
            _pvPrevOk = false; // реле не трогаем (как в гистерезисе)
            if (enable) setState(_term_rezerv_id != "" ? "ошибка резервного датчика" : "ошибка датчика температуры");
            publishState();
            return;
        }

        // сглаженная скорость изменения температуры (EMA производной, выбросы отсечены);
        // то же мгновенное значение накапливается в окне 5 тиков для метрики rate_per_hour
        if (dtOk && _pvPrevOk)
        {
            float inst = (pv - _pvPrev) / dt; // °C/сек
            if (inst > -0.02f && inst < 0.02f)
            {
                _rate = ema(_rate, inst, 0.3f);
                // то же отсечённое от выбросов измерение попадает в окно последних 5 тиков
                _rateWin[_rateWinIdx] = inst;
                _rateWinIdx = (uint8_t)((_rateWinIdx + 1) % 5);
                if (_rateWinCnt < 5) _rateWinCnt++;
            }
        }
        _pvPrev = pv;
        _pvPrevOk = true;

        if (_heating)
        {
            // восстановление контроля после перезагрузки: подтверждаем команду реле (состояние реле после
            // загрузки нам неизвестно, а _heating восстановлен из чекпоинта); заодно снова работают все защиты
            if (!_releOn && !setRele(true))
            {
                setState("ошибка управления реле");
                publishState();
                return;
            }
            if (_cycleResume)
            {
                _cycleResume = false;
                say("прогрев восстановлен после перезагрузки: в цикле накоплено " + f1(_heatSec / 60.0f) + " мин, горелка " + f1(_burnSec / 60.0f) + " мин");
            }
            if (dtOk)
            {
                _heatSec += dt;
                if (flameOn() && chOn()) _burnSec += dt; // работа горелки в режиме отопления
            }
            // трек минимума температуры
            if (pv < _minPv)
            {
                _minPv = pv;
                _tMin = now;
            }
            // разворот вверх: фиксация инерционного проседания и инерции системы (обучение)
            if (!_minDone && now - _tOn > 120000UL && _rate > 0.00002f && pv >= _minPv + 0.05f && now - _tMin > 120000UL)
            {
                _minDone = true;
                float dip = _pvAtOn - _minPv;
                float lag = (_tMin - _tOn) / 1000.0f;
                if (dip > 0 && dip < _dropMax && lag > 30 && lag < 7200)
                {
                    _dipAfterOn = ema(_dipAfterOn, dip, 0.3f);
                    _lagSec = ema(_lagSec, lag, 0.3f);
                    saveLearn();
                }
            }
            // обучение скорости прогрева (после разворота, на растущем тренде)
            if (_minDone && _rate > 0.00005f)
            {
                _heatRate = ema(_heatRate, _rate, 0.2f);
                if (_heatRate > 0.005f) _heatRate = 0.005f;
            }
            // ЗАЩИТА ОТ ПЕРЕГРЕВА, жесткая
            if (pv >= sp + _overMax - 0.0001f)
            {
                heatStop(false, "overheat");
                return;
            }
            // ЗАЩИТА ОТ ПЕРЕГРЕВА, упреждающая: с учетом инерционного догрева
            if (pv + _riseAfterOff >= sp + _overMax)
            {
                heatStop(false, "pred_overheat");
                return;
            }
            // лимит длительности прогрева
            if (_maxHeatMin > 0 && _heatSec >= _maxHeatMin * 60)
            {
                heatStop(false, "max_heat");
                return;
            }
            // адаптивный бюджет горелки (учет времени горелки и перегрева прошлого цикла)
            if (_burnBudget > 0 && _burnSec >= _burnBudget)
            {
                _cutByBudget = true;
                heatStop(false, "budget");
                return;
            }
            // плановое заблаговременное выключение: на растущем тренде достигли порога
            // гистерезисное выкл - off_k% от обученного перегрева (минимум 2 минуты прогрева)
            if (_rate >= 0 && pv >= stopThreshold() - 0.0001f && now - _tOn >= 120000UL)
            {
                heatStop(true, "planned");
                return;
            }
            saveLearn(); // периодический чекпоинт накоплений цикла (перезагрузка продолжит с ними)
            setState(resUsed ? "резервный датчик" : String("прогрев ( ") + f0(_burnSec / 60.0f) + " м )");
            publishState();
            return;
        }
        // пауза: котел выключен
        // трек минимума температуры в паузе: просадка ниже заданной за цикл (для советов)
        if (_minPvPauseOk && pv < _minPvPause) _minPvPause = pv;
        // трек роста без отопления: внешним нагревом (солнцем) фиксируем ТОЛЬКО если текущее
        // остывание (от выключения котла до текущего момента) в два раза превысило длительность
        // предыдущего остывания И в два раза превысило lag_min - более короткий рост еще
        // объясним инерцией котла
        if (_rate > 0.00005f)
        {
            if (!_upTrend)
            {
                _upTrend = true;
                _riseSince = now;
            }
            if (!_extHeat && _tOff > 0)
            {
                float coolNow = (now - _tOff) / 1000.0f; // длительность текущего остывания, сек
                float winPrev = 2.0f * _coolPrevSec;     // вдвое дольше предыдущего остывания
                float winLag = 2.0f * _lagInitMin * 60.0f; // вдвое дольше lag_min
                float win = (winPrev > winLag) ? winPrev : winLag;
                if (coolNow > win) _extHeat = true; // внешний нагрев (солнце); обучение на росте отключается
            }
        }
        else if (_rate < -0.00002f)
        {
            _upTrend = false;
            _extHeat = false;
        }
        // фиксация пика после выключения (инерционный подъем / солнце)
        if (!_maxDone && _tOff > 0)
        {
            if (pv > _maxPv)
            {
                _maxPv = pv;
                _tMax = now; // момент пика - по нему остывание делится на рост и падение
            }
            bool turnedDown = (_rate < -0.00002f && now - _tOff > 120000UL);
            bool windowOut = (now - _tOff > riseWindowSec() * 1000UL);
            if (turnedDown || windowOut)
            {
                _maxDone = true;
                float rise = _maxPv - _pvAtOff;
                float over = _maxPv - sp;
                // пик завершенного цикла зафиксирован - дополняем снимок prev_cycle
                _pcMaxC = _maxPv;
                _pcOverC = (over > 0) ? over : 0;
                _pcMaxDone = true;
                float guard = 1.6f * _riseAfterOff + 0.2f; // максимум, что может дать инерция
                if (guard < 0.5f) guard = 0.5f;
                // в самообучение идет только подъем, объяснимый инерцией котла: не солнце,
                // не рост дольше окна допустимого и не сверх физических возможностей инерции
                if (!_extHeat && !windowOut && rise <= guard && rise >= 0)
                {
                    _riseAfterOff = ema(_riseAfterOff, rise, 0.3f);
                    float overClamped = (over > 0) ? over : 0; // перегрев - обучающая цель порога выключения
                    _overPrev = (_overPrev >= 0) ? ema(_overPrev, overClamped, 0.3f) : overClamped;
                    saveLearn();
                }
            }
        }
        // обучение скорости остывания: чистое остывание без отопления и без солнца
        if (!_extHeat && _rate < -0.00005f)
        {
            _coolRate = ema(_coolRate, -_rate, 0.1f);
            if (_coolRate > 0.005f) _coolRate = 0.005f;
        }
        if (!enable)
        {
            setState("выключен");
            publishState();
            return;
        }
        bool freeze = (pv <= sp - _dropMax + 0.0001f);
        float leadThr = startThreshold(); // порог упреждающего включения (гист.вкл + доля обученной просадки)
        bool predict = (_rate < -0.00001f && pv <= leadThr + 0.0001f && !freeze);
        // классическое включение (страховка): температура уже просела ниже заданной
        bool hyst = (pv <= sp - _gist + 0.0001f);
        // антидребезг: после высокого пика не включаться, пока не остынем ниже пика - gist
        if (hyst && _maxDone && _maxPv > 0)
        {
            float cap = _maxPv - _gist;
            if (pv > cap) hyst = false;
        }
        bool restOk = (_minRestMin <= 0) || (_tOff == 0) || (now - _tOff >= (unsigned long)(_minRestMin * 60000.0f));
        if (freeze)
        {
            heatStart("заморозка");
        }
        else if ((predict || hyst) && restOk)
        {
            // при on_k=0 порог старта совпадает с гистерезисным - это обычное включение, а не упреждение
            heatStart((predict && leadThr > sp - _gist + 0.0001f) ? "упреждение" : "гистерезис");
        }
        else
        {
            if (resUsed) setState("резервный датчик");
            else if (_extHeat) setState("пауза (солнечный нагрев)");
            else setState("пауза");
        }
        saveLearn(); // периодический чекпоинт состояния паузы: минимум/пик/рост восстановятся после перезагрузки
        publishState();
    }
    IoTValue execute(String command, std::vector<IoTValue> &param)
    {
        if (command == "enable" && param.size() >= 1)
        {
            enable = (param[0].valD != 0) ? 1 : 0;
            if (!enable && _heating)
            {
                setRele(false);
                _heating = false;
                _prevHeatSec = _heatSec; // окно допустимого роста в предстоящей паузе станет 2x этого прогрева
                _tOff = millis();
                _maxPv = pv;
                _tMax = _tOff; // пик нового остывания отслеживаем от момента выключения
                _maxDone = false;
                _upTrend = true;
                _riseSince = millis();
                // снимок прерванного вручную цикла для метрик prev_cycle (пик не доучен)
                _pcBurnSec = _burnSec;
                _pcDipSec = (_minDone && _tMin >= _tOn) ? (_tMin - _tOn) / 1000.0f : 0;
                _pcMinC = _minPv;
                _pcDipC = (sp - _minPv > 0) ? sp - _minPv : 0;
                _pcMaxDone = false;
                _pcLearned = false;
                _pcStoppedBy = "manual";
                _pcOk = true;
            }
            setState(enable ? "пауза" : "выключен");
        }
        else if (command == "debug" && param.size() >= 1)
        {
            _debug = (param[0].valD != 0) ? 1 : 0;
            say(String("пульс метрик в MQTT (топик <id>/debug) ") + (_debug ? "включен" : "выключен"));
            if (_debug) publishState(true); // при включении пульс уходит сразу, не дожидаясь такта
        }
        else if (command == "set_on_k" && param.size() >= 1)
        {
            _onK = param[0].valD;
            if (_onK < 0) _onK = 0;
            if (_onK > 100) _onK = 100;
        }
        else if (command == "set_off_k" && param.size() >= 1)
        {
            _offStop = param[0].valD;
            if (_offStop < 0) _offStop = 0;
            if (_offStop > 100) _offStop = 100;
        }
        else if (command == "set_over_goal" && param.size() >= 1)
        {
            _overGoal = param[0].valD;
            if (_overGoal < 0) _overGoal = 0;
            if (_overGoal > 2) _overGoal = 2;
        }
        else if (command == "set_burn_adj" && param.size() >= 1)
        {
            _burnAdj = (param[0].valD != 0);
        }
        else if (command == "set_burn_k" && param.size() >= 1)
        {
            _burnK = param[0].valD;
            if (_burnK < 0) _burnK = 0;
            if (_burnK > 100) _burnK = 100;
        }
        else if (command == "reset")
        {
            resetModel();
            saveLearn(true);
            say("сброс самообучения выполнен: накопленные оценки возвращены к начальным (прогрев/остывание ~1 °C за 50 мин)");
        }
        else if (command == "learn")
        {
            say("обученная модель дома: " + modelInfo());
        }
        else if (command == "advice")
        {
            String advice;
            advisor(advice);
            say(advice);
            publishState(true, advice.c_str()); // внеплановый пульс метрик в <id>/debug с ключом recommended (текст совета)
        }
        return {};
    }
    ~ThermostatPRED()
    {
        saveLearn(true); // при перечитывании конфигурации или перезагрузке оставляем последний чекпоинт
    };
};
void *getAPI_Thermostat(String subtype, String param)
{
    if (subtype == F("ThermostatPRED"))
    {
        return new ThermostatPRED(param);
    }
    else if (subtype == F("ThermostatGIST"))
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
