#include "Global.h"
//#include "classes/IoTItem.h"
#include "classes/IoTScenario.h"
//#include "utils/FileUtils.h"
//#include "utils/WiFiUtils.h"
#include "NTP.h"
#include <new> 

int ExprAST::objectCount = 0;
int ExprAST::maxObjectCount = 0;

// вызываем все нужное пока зависаем в сценариях
// void scenario_yield() {
//     yield();   
//     HTTP.handleClient();
// }

bool isIotScenException = false;  // признак исключения и попытки прекратить выполнение сценария заранее

// Лексический анализатор возвращает токены [0-255], если это неизвестны, иначе одну из известных единиц кода
enum Token {
    tok_eof = -1,

    // операнды (первичные выражения: идентификаторы, числа)
    tok_identifier = -4,
    tok_number = -5,
    tok_string = -3,

    // двухсимвольные операторы бинарных операций
    tok_equal = -2,
    tok_notequal = -9,
    tok_lesseq = -10,
    tok_greateq = -11,
    tok_silentset = -12,

    // управление
    tok_if = -6,
    tok_then = -7,
    tok_else = -8,

    // Инкременты и декременты
    tok_inc = -13, 
    tok_dec = -14, 
    tok_addassign = -15, 
    tok_subassign = -16, 
    tok_mulassign = -17, 
    tok_divassign = -18
};

//===----------------------------------------------------------------------===//
// Таблица приоритетов бинарных операторов (замена std::map)
// 18 записей × 2 байта = 36 байт RAM (вместо ~400 байт std::map)
//===----------------------------------------------------------------------===//
struct BinOpEntry {
    signed char op;
    int8_t prec;
};

static const BinOpEntry BinopPrecedenceData[] = {
    {tok_silentset, 1},
    {'=', 2},
    {tok_addassign, 2},
    {tok_subassign, 2},
    {tok_mulassign, 2},
    {tok_divassign, 2},
    {'|', 5},
    {'&', 6},
    {tok_equal, 10},
    {tok_notequal, 11},
    {tok_lesseq, 15},
    {tok_greateq, 16},
    {'<', 20},
    {'>', 21},
    {'+', 25},
    {'-', 26},
    {'/', 27},
    {'*', 28}
};

static const int BINOP_COUNT = sizeof(BinopPrecedenceData) / sizeof(BinopPrecedenceData[0]);

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (Абстрактное Синтаксическое Дерево или Дерево Парсинга)
//===----------------------------------------------------------------------===//
// ExprAST - Базовый класс для всех узлов выражений.
// ExprAST::~ExprAST() {}

IoTValue *ExprAST::exec() { return nullptr; }
int ExprAST::setValue(IoTValue *val, bool generateEvent) { return 0; }                  // 0 - установка значения не поддерживается наследником

/// UnaryNotExprAST - Класс узла выражения для унарного отрицания "!x".
/// Возвращает 1, если операнд ложный (0 или пустая строка), иначе 0.
class UnaryNotExprAST : public ExprAST {
    ExprAST* _operand;
    IoTValue* val = nullptr; // ленивая аллокация, как в BinaryExprAST

    IoTValue* getVal() {
        if (!val) {
            val = new (std::nothrow) IoTValue();
            if (!val) {
                isIotScenException = true;
                return nullptr;
            }
        }
        return val;
    }

   public:
    UnaryNotExprAST(ExprAST* operand) : _operand(operand) {}

    ~UnaryNotExprAST() {
        delete _operand;
        delete val;
    }

    IoTValue* exec() override {
        if (isIotScenException) return nullptr;
        if (_operand == nullptr) return nullptr;

        IoTValue* opVal = _operand->exec();
        if (opVal == nullptr) return nullptr;

        bool isTruthy = opVal->isDecimal ? (opVal->valD != 0.0f) : (opVal->valS != "");

        IoTValue* result = getVal();
        if (!result) return nullptr;

        result->clear();
        result->isDecimal = true;
        result->valD = isTruthy ? 0.0f : 1.0f; // инвертируем

        return result;
    }
};

/// NumberExprAST - Класс узла выражения для числовых литералов (Например, "1.0").
class NumberExprAST : public ExprAST {
    // Статические объекты для 0 и 1 (общие для всех)
    static IoTValue zeroValue;
    static IoTValue oneValue;
    
    IoTValue* _value;  // Указатель на значение (может быть общим)

public:
    NumberExprAST(float val) {
        if (val == 0.0f) {
            _value = &zeroValue;
        } else if (val == 1.0f) {
            _value = &oneValue;
        } else {
            // Для других чисел создаем свой объект
            _value = new IoTValue();
            _value->valD = val;
            _value->isDecimal = true;
            _value->valS = "";
        }
    }

    IoTValue *exec() override {
        if (isIotScenException) return nullptr;
        return _value;  // Теперь безопасно!
    }
    
    ~NumberExprAST() {
        // Удаляем только если это не общий объект (0 или 1)
        if (_value != &zeroValue && _value != &oneValue) {
            delete _value;
        }
    }
};

// ОПРЕДЕЛЕНИЕ СТАТИЧЕСКИХ ПЕРЕМЕННЫХ (в .cpp, после класса)
IoTValue NumberExprAST::zeroValue = {0.0f, "", true};
IoTValue NumberExprAST::oneValue = {1.0f, "", true};

/// StringExprAST - Класс узла выражения для строковых литералов (Например, "Example with spaces and quotes").
class StringExprAST : public ExprAST {
    IoTValue Val;

   public:
    StringExprAST(String val) {
        Val.isDecimal = false;       
        Val.valS = std::move(val);       
    }

    IoTValue *exec() {
        if (isIotScenException) return nullptr;     
        return &Val;
    }
};

/// VariableExprAST - Класс узла выражения для переменных (например, "a").
class VariableExprAST : public ExprAST {
    uint32_t _nameHash; // Храним числовой хэш вместо строки
   public:
    
    VariableExprAST(uint32_t nameHash) : _nameHash(nameHash) {}

    IoTValue *exec() override {
        if (isIotScenException) return nullptr;

        // Быстрый поиск элемента по числу (хэшу)
        IoTItem *Item = findIoTItem(_nameHash);
        if (Item) {
            return &(Item->value); // Возвращаем указатель на структуру его значений
        }
        return nullptr;
    }

    int setValue(IoTValue *val, bool generateEvent) override {
        if (isIotScenException) return 0;

        // Быстрый поиск элемента по числу (хэшу) для записи
        IoTItem *Item = findIoTItem(_nameHash);
        if (Item) {
            Item->setValue(val->isDecimal ? (String)val->valD : val->valS, generateEvent);
            return 1;
        }
        return 0;
    }
};

/// BinaryExprAST - Класс узла выражения для бинарных операторов. Kimi
class BinaryExprAST : public ExprAST {
    signed char Op;
    ExprAST *LHS, *RHS;
    IoTValue* val = nullptr;  // ← ЛЕНИВЫЙ: создаём только при первом exec()
    
    // Гарантируем существование val
    IoTValue* getVal() {
        if (!val) {
            val = new (std::nothrow) IoTValue();
            if (!val) {
                isIotScenException = true;
                return nullptr;
            }
        }
        return val;
    }

   public:
    BinaryExprAST(signed char op, ExprAST *lhs, ExprAST *rhs)
        : Op(op), LHS(lhs), RHS(rhs) {}

    ~BinaryExprAST() {
        if (LHS) delete LHS;
        if (RHS) delete RHS;
        delete val;  // ← Удаляем только если создавали
    }

    IoTValue *exec() {
        if (isIotScenException) return nullptr;
        if (RHS == nullptr || LHS == nullptr) return nullptr;
      
        // 1. ПРИСВАИВАНИЕ ПРАВОГО ОПЕРАНДА
        IoTValue *rhs = RHS->exec();  
        if (rhs == nullptr) return nullptr;

        if (Op == '=' && LHS->setValue(rhs, true)) {  
            return rhs;                         
        }

        if (Op == tok_silentset && LHS->setValue(rhs, false)) {  
            return rhs;                         
        }

        // --- СЛОЖНЫЕ ПРИСВАИВАНИЯ (+=, -=, *=, /=) ---
        if (Op == tok_addassign || Op == tok_subassign || Op == tok_mulassign || Op == tok_divassign) {
            IoTValue *lhs = LHS->exec();
            if (lhs == nullptr) return nullptr;

            float leftNum  = lhs->isDecimal ? lhs->valD : atof(lhs->valS.c_str());
            float rightNum = rhs->isDecimal ? rhs->valD : atof(rhs->valS.c_str());

            IoTValue* result = getVal();
            if (!result) return nullptr;
            
            result->clear();
            result->isDecimal = true;

            if (Op == tok_addassign) result->valD = leftNum + rightNum;
            else if (Op == tok_subassign) result->valD = leftNum - rightNum;
            else if (Op == tok_mulassign) result->valD = leftNum * rightNum;
            else if (Op == tok_divassign) {
                if (rightNum != 0.0f) result->valD = leftNum / rightNum;
                else result->valD = 3.4E+38;
            }

            if (LHS->setValue(result, true)) {
                return result;
            }
            return nullptr;
        }

        // 2. ПОЛУЧЕНИЕ ЛЕВОГО ОПЕРАНДА
        IoTValue *lhs = LHS->exec();  
        if (lhs == nullptr) return nullptr;

        // 3. ЧИСТО МАТЕМАТИЧЕСКИЕ И ЛОГИЧЕСКИЕ ОПЕРАЦИИ
        if (Op != '+' && Op != '-' && Op != tok_equal && Op != tok_notequal) {
            
            float leftNum  = lhs->isDecimal ? lhs->valD : atof(lhs->valS.c_str());
            float rightNum = rhs->isDecimal ? rhs->valD : atof(rhs->valS.c_str());

            IoTValue* result = getVal();
            if (!result) return nullptr;
            
            result->isDecimal = true; 
            result->clear();

            switch (Op) {
                case '>':         result->valD = leftNum > rightNum;  break;
                case '<':         result->valD = leftNum < rightNum;  break;
                case tok_lesseq:  result->valD = leftNum <= rightNum; break;
                case tok_greateq: result->valD = leftNum >= rightNum; break;
                case '*':         result->valD = leftNum * rightNum;  break;
                case '/':
                    if (rightNum != 0.0f)
                        result->valD = leftNum / rightNum;
                    else
                        result->valD = 3.4E+38;
                    break;
                case '|':         result->valD = (leftNum != 0.0f) || (rightNum != 0.0f); break;
                case '&':         result->valD = (leftNum != 0.0f) && (rightNum != 0.0f); break;
            }
            return result;
        } 
        // 4. ОПЕРАЦИИ, КОТОРЫЕ МОГУТ РАБОТАТЬ И СО СТРОКАМИ
        else {    
            IoTValue* result = getVal();
            if (!result) return nullptr;
            
            switch (Op) {
                case tok_equal:
                    if (lhs->isDecimal && rhs->isDecimal) {
                        result->valD = (lhs->valD == rhs->valD);
                    } else if (!lhs->isDecimal && !rhs->isDecimal) {
                        result->valD = compStr(lhs->valS, rhs->valS);
                    } else {
                        result->valD = false; 
                    }
                    result->isDecimal = true;
                    break;

                case tok_notequal:
                    if (lhs->isDecimal && rhs->isDecimal) {
                        result->valD = (lhs->valD != rhs->valD);
                    } else if (!lhs->isDecimal && !rhs->isDecimal) {
                        result->valD = !compStr(lhs->valS, rhs->valS);
                    } else {
                        result->valD = true; 
                    }
                    result->isDecimal = true;
                    break;

                case '+':
                    if (lhs->isDecimal && rhs->isDecimal) {
                        result->valD = lhs->valD + rhs->valD;
                        result->isDecimal = true;
                    } else {
                        String lStr = lhs->isDecimal ? String(lhs->valD, 2) : lhs->valS;
                        String rStr = rhs->isDecimal ? String(rhs->valD, 2) : rhs->valS;
                        result->valS = lStr + rStr;
                        result->valD = 1.0f;
                        result->isDecimal = false;
                    }
                    break;

                case '-':
                    if (lhs->isDecimal && rhs->isDecimal) {
                        result->valD = lhs->valD - rhs->valD;
                        result->isDecimal = true;
                    } else {
                        String lStr = lhs->isDecimal ? String(lhs->valD, 2) : lhs->valS;
                        String rStr = rhs->isDecimal ? String(rhs->valD, 2) : rhs->valS;
                        result->valS = lStr;
                        result->valS.replace(rStr, "");
                        result->valD = 1.0f;
                        result->isDecimal = false;
                    }
                    break;
            }
            return result;
        }    
    }

    bool compStr(const String& str1, const String& str2) {
        if (str1.length() != str2.length()) return false;
        
        for (size_t i = 0; i < str1.length(); i++) {
            if (str1[i] == '*' || str2[i] == '*') continue; 
            if (str1[i] != str2[i]) return false;
        }
        return true; 
    }
};

// Обёртка для безопасного создания BinaryExprAST.
static ExprAST* safeBinary(signed char op, ExprAST* lhs, ExprAST* rhs) {
    if (!lhs || !rhs) {
        delete lhs;
        delete rhs;
        return nullptr;
    }
    ExprAST* node = new (std::nothrow) BinaryExprAST(op, lhs, rhs);
    if (!node) {
        delete lhs;
        delete rhs;
    }
    return node;
}

/// CallExprAST - Класс узла выражения для вызова команды.
class CallExprAST : public ExprAST {
    static const uint8_t MAX_CALL_ARGS = 6; // Лимит аргументов для экономии памяти
    ExprAST* _args[MAX_CALL_ARGS];          // Фиксированный массив узлов
    uint8_t _count = 0;                     // Счетчик реально добавленных аргументов

    uint32_t _calleeHash;
    uint32_t _cmdHash;
    String _rawCmd; 
    IoTValue ret;

   public:
    // Конструктор теперь принимает только базовые данные, без вектора
    CallExprAST(uint32_t calleeHash, uint32_t cmdHash, String cmd)
        : _calleeHash(calleeHash), _cmdHash(cmdHash), _rawCmd(std::move(cmd)) {
        for(uint8_t i = 0; i < MAX_CALL_ARGS; i++) _args[i] = nullptr;
        SerialPrint("CallExprAST","RAW",_rawCmd);
    }

    // Метод для пошагового наполнения (вызывается из парсера)
    void addArg(ExprAST* arg) {
        if (_count < MAX_CALL_ARGS) {
            _args[_count++] = arg;
        }
    }

    IoTValue *exec() override {
        if (isIotScenException) return nullptr;  
        
        IoTItem *Item = findIoTItem(_calleeHash);
        if (!Item) return nullptr;

        // 1. Быстрая проверка системных команд (выход)
        if (_cmdHash == 3454868101U || _calleeHash == 3454868101U) { 
            // ... (ваша логика выхода)
            isIotScenException = true;
            return nullptr;
        }

        // 2. Оптимизированный switch-case (по хэшам команд)
        switch (_cmdHash) {
            case 2059385011U: { // getIntFromNet
                ret.valD = Item->getIntFromNet();
                ret.isDecimal = true;
                return &ret;
            }
            case 2723735340U: { // doByInterval
                Item->doByInterval();
                ret = Item->value;
                return &ret;
            }
        }

        // 3. Подготовка аргументов для вызова
        // Используем временный стек для вычисления значений
        IoTValue argsBuffer[MAX_CALL_ARGS];
        for (uint8_t i = 0; i < _count; i++) {
            IoTValue *tmp = _args[i]->exec();
            if (tmp) argsBuffer[i] = *tmp;
            else return nullptr;
        }

        // 4. Специальная проверка для интервала
        if (_cmdHash == 2684182154U && _count == 1) { 
            Item->setInterval(argsBuffer[0].valD);
            ret.valD = Item->getInterval();
            ret.isDecimal = true;
            return &ret;
        }

        // 5. Адаптер: превращаем статический массив в вектор для совместимости с модулями
        // Это единственный момент, где создается временный вектор
        std::vector<IoTValue> ArgsAsIoTValue;
        ArgsAsIoTValue.reserve(_count);
        for(uint8_t i = 0; i < _count; i++) {
            ArgsAsIoTValue.push_back(argsBuffer[i]);
        }

        // Вызов метода модуля
        ret = Item->execute(_rawCmd, ArgsAsIoTValue); 
        return &ret;
    }

    // Деструктор аккуратно очищает узлы
    ~CallExprAST() {
        for (uint8_t i = 0; i < _count; i++) {
            delete _args[i];
        }
    }
};

/// SysCallExprAST - Класс узла выражения для вызова системных команд.
/// Хэш имени команды сравнивается напрямую в switch, без промежуточного enum
/// и без отдельной функции sysExecute — раньше трансляция через enum имела
/// смысл при сравнении по строкам, сейчас (сравнение по числам-хэшам)
/// это лишний уровень косвенности.
class SysCallExprAST : public ExprAST {
    static const uint8_t MAX_SYS_ARGS = 6;
    ExprAST* _args[MAX_SYS_ARGS];
    uint8_t _count = 0;

    uint32_t _calleeHash;   // хэш имени команды, сохраняем напрямую
    IoTValue ret;

   public:
    SysCallExprAST(uint32_t calleeHash) : _count(0), _calleeHash(calleeHash) {
        for (uint8_t i = 0; i < MAX_SYS_ARGS; i++) _args[i] = nullptr;
    }

    void addArg(ExprAST* arg) {
        if (_count < MAX_SYS_ARGS) _args[_count++] = arg;
    }

    IoTValue *exec() override {
        if (isIotScenException) return nullptr;

        ret.clear();          // ← добавить
        ret.isDecimal = true; // ← по умолчанию число, строковые case переопределят

        // Статический буфер аргументов вместо вектора
        IoTValue argsBuffer[MAX_SYS_ARGS];
        for (uint8_t i = 0; i < _count; i++) {
            IoTValue *tmp = _args[i]->exec();
            if (tmp) argsBuffer[i] = *tmp;
            else return nullptr;
        }

        //ret.clear(); // сброс перед новым вычислением (сохраняя capacity valS)

        switch (_calleeHash) {
            // --- временные команды: проверяем _time_isTrust точечно ---
            case 1981576342U: // getHours
                if (_time_isTrust) ret.valD = _time_local.hour;
                break;
            case 3601197708U: // getMinutes
                if (_time_isTrust) ret.valD = _time_local.minute;
                break;
            case 1643975268U: // getSeconds
                if (_time_isTrust) ret.valD = _time_local.second;
                break;
            case 1852772979U: // getMonth
                if (_time_isTrust) ret.valD = _time_local.month;
                break;
            case 262753459U:  // getDay
                if (_time_isTrust) ret.valD = _time_local.day_of_month;
                break;
            case 4265708545U: // gethhmm
                ret.isDecimal = false;
                if (_time_isTrust) ret.valS = getTimeLocal_hhmm();
                break;
            case 431242463U:  // gethhmmss
                ret.isDecimal = false;
                if (_time_isTrust) ret.valS = getTimeLocal_hhmmss();
                break;
            case 3311495782U: // getTime
                ret.isDecimal = false;
                if (_time_isTrust) ret.valS = getDateTimeDotFormated();
                break;

            // --- обычные системные команды ---
            case 2272436152U: // reboot
                ESP.restart();
                break;
            case 2086015023U: // digitalRead
                if (_count) {
                    IoTgpio.pinMode(argsBuffer[0].valD, INPUT);
                    ret.valD = IoTgpio.digitalRead(argsBuffer[0].valD);
                }
                break;
            case 3962656017U: // analogRead
                if (_count) {
                    IoTgpio.pinMode(argsBuffer[0].valD, INPUT);
                    ret.valD = IoTgpio.analogRead(argsBuffer[0].valD);
                }
                break;
            case 80230478U: // digitalWrite
                if (_count == 2) {
                    IoTgpio.pinMode(argsBuffer[0].valD, OUTPUT);
                    IoTgpio.digitalWrite(argsBuffer[0].valD, argsBuffer[1].valD);
                }
                break;
            case 1319639419U: // digitalInvert
                if (_count) {
                    IoTgpio.pinMode(argsBuffer[0].valD, OUTPUT);
                    IoTgpio.digitalInvert(argsBuffer[0].valD);
                }
                break;
            case 89606834U: // deepSleep
                if (_count) {
                    Serial.printf("Ушел спать на %d сек...", (int)argsBuffer[0].valD);
#if defined(ESP32)
                    esp_sleep_enable_timer_wakeup(argsBuffer[0].valD * 1000000);
                    delay(1000);
                    esp_deep_sleep_start();
#elif defined(ESP8266)
                    ESP.deepSleep(argsBuffer[0].valD * 1000000);
#endif
                }
                break;
            case 714218580U: // getRSSI
                ret.valD = WiFi.RSSI();
                break;
            case 1930502878U: // getIP
                ret.isDecimal = false;
                ret.valS = myConfig.ip;
                break;
            case 2817869354U: // mqttPub
                if (_count == 2) {
                    String tmpStr = argsBuffer[1].valS;
                    if (argsBuffer[1].isDecimal) tmpStr = String(argsBuffer[1].valD);
                    ret.valD = publish(argsBuffer[0].valS, tmpStr);
                }
                break;
            case 3864144131U: // getUptime
                ret.isDecimal = false;
                ret.valS = String(errorHeap.upt);
                break;
            case 1718029443U: // mqttIsConnect
                ret.valD = mqttIsConnect();
                break;
            case 1257170198U: // wifiIsConnect
                ret.valD = isNetworkActive();
                break;
            case 2684182154U: // setInterval
                if (_count == 1) {
                    // ... логика setInterval, если появится
                }
                break;
            default:
                break; // неизвестная команда — sysop_notfound
        }

        return &ret;
    }

    ~SysCallExprAST() {
        for (uint8_t i = 0; i < _count; i++) delete _args[i];
    }
};

class IfExprAST : public ExprAST {
    ExprAST *_Cond, *_Then, *_Else;

    uint32_t* _IDHashes = nullptr;  // выделяется под точный размер, а не фиксированные 16
    uint8_t _IDCount = 0;

   public:
    IfExprAST(ExprAST *cond, ExprAST *then, ExprAST *_else, uint32_t* hashes, uint8_t count)
    : _Cond(cond), _Then(then), _Else(_else) {

        if (hashes && count > 0) {
            _IDHashes = new (std::nothrow) uint32_t[count];
            if (_IDHashes) {
                for (uint8_t i = 0; i < count; i++) {
                    _IDHashes[i] = hashes[i];
                }
                _IDCount = count;
            } else {
                // Не хватило памяти под хэши — узел будет просто не реагировать
                // ни на одно событие (hasEventIdName всегда false), но не упадёт
                SerialPrint("e", "IFEXPR", "NO MEMORY FOR HASHES", String(count));
                _IDCount = 0;
            }
        }
    }

    ~IfExprAST() {
        delete _Cond;
        if (_Then) delete _Then;
        if (_Else) delete _Else;
        delete[] _IDHashes;  // корректное освобождение массива
    }

    bool hasEventIdName(uint32_t eventHash) override {
        for (uint8_t i = 0; i < _IDCount; i++) {
            if (_IDHashes[i] == eventHash) return true;
        }
        // Если сам if не реагирует на это событие напрямую, проверяем,
        // не зависит ли от него вложенный if внутри Then/Else — иначе
        // такой вложенный блок никогда не будет вызван диспетчером,
        // так как в cachedIfs[] зарегистрирован только внешний узел.
        if (_Then && _Then->hasEventIdName(eventHash)) return true;
        if (_Else && _Else->hasEventIdName(eventHash)) return true;
        return false;
    }

    IoTValue *exec() override {
        if (isIotScenException) return nullptr;

        IoTValue *condVal = _Cond->exec();
        if (!condVal) return nullptr;

        if (condVal->isDecimal ? condVal->valD : (condVal->valS != "")) {
            if (_Then) return _Then->exec();
        } else {
            if (_Else) return _Else->exec();
        }
        return nullptr;
    }
};

/// BracketsExprAST - Класс узла блока кода {}.
/// Хранит список выражений в динамическом массиве с ростом через realloc.
/// Аллокации происходят только при парсинге (addExpr), exec() — плоский цикл
/// без рекурсии, поэтому глубина блока не влияет на использование стека.
class BracketsExprAST : public ExprAST {
    ExprAST** _exprs = nullptr;
    uint8_t _count = 0;
    uint8_t _capacity = 0;

public:
    BracketsExprAST() {
        //SerialPrint("i", "BRACKET", "CREATED", "empty");
    }

    void addExpr(ExprAST* expr) {
        if (!expr) return; // защита от добавления нулевого узла

        if (_count >= _capacity) {
            uint8_t newCapacity = _capacity ? (uint8_t)(_capacity * 2) : 4;

            ExprAST** newExprs = (ExprAST**)realloc(_exprs, newCapacity * sizeof(ExprAST*));
            if (!newExprs) {
                SerialPrint("e", "BRACKET", "REALLOC FAIL", String(_capacity));
                delete expr; // иначе узел потеряется и будет утечка
                return;
            }
            _exprs = newExprs;
            _capacity = newCapacity;
            //SerialPrint("i", "BRACKET", "EXPANDED", String(_capacity));
        }

        _exprs[_count++] = expr;
    }

    bool hasEventIdName(uint32_t eventHash) override {
        for (uint8_t i = 0; i < _count; i++) {
            if (_exprs[i]->hasEventIdName(eventHash)) return true;
        }
        return false;
    }
    
    IoTValue* exec() override {
        if (isIotScenException) return nullptr;

        IoTValue* lastResult = nullptr;
        for (uint8_t i = 0; i < _count; i++) {
            lastResult = _exprs[i]->exec();
            // Не прерываем на nullptr: это нормальный результат для
            // if-без-else с ложным условием, а не признак сбоя.
            // Прерываться нужно только на реальном исключении сценария
            // (isIotScenException), которое проверяется в начале каждого exec().
            if (isIotScenException) break;
        }
        return lastResult;
    }
    
    ~BracketsExprAST() {
        //SerialPrint("i", "BRACKET", "DESTROY", String(_count));
        for (uint8_t i = 0; i < _count; i++) {
            delete _exprs[i];
        }
        free(_exprs);
    }
};

//===----------------------------------------------------------------------===//
// Lexer (Лексический анализатор)
//===----------------------------------------------------------------------===//
int IoTScenario::getLastChar() {
    if (file && file.available()) {
        int ch = file.read();
        if (ch == '\n') curLine++; // Считаем строки
        return ch;
    }
    return EOF;
}

int IoTScenario::gettok() {
    // Пропускаем пробелы, табы, переносы и точки с запятой
    while (isspace((unsigned char)LastChar) || LastChar == ';')
        LastChar = getLastChar();

    // 1. ИДЕНТИФИКАТОРЫ И КЛЮЧЕВЫЕ СЛОВА
    if (isalpha((unsigned char)LastChar) || LastChar == '_') {  // [a-zA-Z_][a-zA-Z0-9_]*
        //IdentifierStr.clear();
        IdentifierStr = "";
        // Выделяем память заранее (большинство токенов типа "if", "else", "my_var" укладываются в 15 символов)
        //IdentifierStr.reserve(16); 
        
        IdentifierStr += (char)LastChar;
        while (isalnum((unsigned char)(LastChar = getLastChar())) || LastChar == '_') {
            IdentifierStr += (char)LastChar;
        }

        // Считаем хэш от полученного идентификатора
        IdentifierHash = hashFNV1a(IdentifierStr);

        // СВЕРХБЫСТРОЕ сравнение ключевых слов по числам
        switch (IdentifierHash) {
            case 959999494U: return tok_if;     // "if" 959999494
            case 3844270454U: return tok_then;   // "then" 3844270454
            case 3183434736U: return tok_else;   // "else" 3183434736
            default:          break;
        }
        
        return tok_identifier;
    }
   

    // 2. ЧИСЛА (Сборка double на лету без использования String)
    if (isdigit((unsigned char)LastChar)) {
        NumVal = 0.0;
        
        // Разбираем целую часть
        do {
            NumVal = NumVal * 10.0 + (LastChar - '0');
            LastChar = getLastChar();
        } while (isdigit((unsigned char)LastChar));

        // Разбираем дробную часть
        if (LastChar == '.') {
            LastChar = getLastChar();
            float weight = 0.1;
            while (isdigit((unsigned char)LastChar)) {
                NumVal += (LastChar - '0') * weight;
                weight *= 0.1;
                LastChar = getLastChar();
            }
        }

        return tok_number;
    }

    // 3. КОММЕНТАРИИ
    if (LastChar == '#') {
        do {
            LastChar = getLastChar();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF)
            return gettok();
    }

    // 4. СТРОКОВЫЕ ЛИТЕРАЛЫ
    if (LastChar == '"') {  // "строка"
        IdentifierStr.clear();
        //IdentifierStr.reserve(32); // Строки обычно длиннее идентификаторов
        
        LastChar = getLastChar();
        while (LastChar != '"' && LastChar != EOF) {
            if (LastChar == '\\') {     // Обработка экранированных символов
                LastChar = getLastChar();
                if (LastChar == '"') {
                    IdentifierStr += '"';
                } else if (LastChar == 'n') {
                    IdentifierStr += '\n';
                } else if (LastChar == '\\') {
                    IdentifierStr += '\\';
                } 
            } else {
                IdentifierStr += (char)LastChar;
            }
            LastChar = getLastChar();
        }
        LastChar = getLastChar(); // Пропускаем закрывающую кавычку

        return tok_string;
    }

    // 5. КОНЕЦ ФАЙЛА
    if (LastChar == EOF)
        return tok_eof;

    // 6. ОПЕРАТОРЫ СРАВНЕНИЯ И ПРИСВАИВАНИЯ
    // Компактная структура через switch экономит flash-память и работает быстрее цепочки if
    switch (LastChar) {
        case '=':
            LastChar = getLastChar();
            if (LastChar == '=') { LastChar = getLastChar(); return tok_equal; }
            return '=';
            
        case ':':
            LastChar = getLastChar();
            if (LastChar == '=') { LastChar = getLastChar(); return tok_silentset; }
            return ':';
            
        case '!':
            LastChar = getLastChar();
            if (LastChar == '=') { LastChar = getLastChar(); return tok_notequal; }
            return '!';
            
        case '<':
            LastChar = getLastChar();
            if (LastChar == '=') { LastChar = getLastChar(); return tok_lesseq; }
            return '<';
            
        case '>':
            LastChar = getLastChar();
            if (LastChar == '=') { LastChar = getLastChar(); return tok_greateq; }
            return '>';
        
        case '+':
            LastChar = getLastChar();
            if (LastChar == '+') { LastChar = getLastChar(); return tok_inc; } // ++
            if (LastChar == '=') { LastChar = getLastChar(); return tok_addassign; } // +=
            return '+';

        case '-':
            LastChar = getLastChar();
            if (LastChar == '-') { LastChar = getLastChar(); return tok_dec; } // --
            if (LastChar == '=') { LastChar = getLastChar(); return tok_subassign; } // -=
            return '-';

        case '*':
            LastChar = getLastChar();
            if (LastChar == '=') { LastChar = getLastChar(); return tok_mulassign; } // *=
            return '*';

        case '/':
            LastChar = getLastChar();
            if (LastChar == '=') { LastChar = getLastChar(); return tok_divassign; } // /=
            return '/';

    }

    // В противном случае просто возвращаем символ как значение ASCII
    int ThisChar = LastChar;
    LastChar = getLastChar();
    return ThisChar;
}

//===----------------------------------------------------------------------===//
// Parser (Парсер или Синтаксический Анализатор)
//===----------------------------------------------------------------------===//

/// CurTok/getNextToken - Предоставляет простой буфер токенов. CurTok - это текущий
/// токен, просматриваемый парсером. getNextToken получает следующий токен от
/// лексического анализатора и обновляет CurTok.
int IoTScenario::getNextToken() {
    return CurTok = gettok();
}

/// GetTokPrecedence - Возвращает приоритет текущего бинарного оператора.
int IoTScenario::GetTokPrecedence() {
    
    // Проверяем только ASCII и отрицательные токены (операторы)
    if (!(isascii(CurTok) || CurTok < 0))
        return -1;

    // Линейный поиск по таблице из 18 элементов (быстрее, чем map)
    for (int i = 0; i < BINOP_COUNT; i++) {
        if (BinopPrecedenceData[i].op == CurTok) {
            return BinopPrecedenceData[i].prec;
        }
    }
    
    return -1;  // оператор не найден
}

/// Error* - Это небольшие вспомогательные функции для обработки ошибок.
ExprAST *IoTScenario::Error(const String& Str) {
    Serial.printf("Scenario error in line %d: %s\n", curLine, Str.c_str());
    isIotScenException = true;
    return nullptr;
}

ExprAST* IoTScenario::ParseIdentifierExpr(bool callFromCondition) {
    if (isIotScenException) return nullptr;

    uint32_t nameHash = IdentifierHash;
    getNextToken(); 

    if (CurTok == '.') {
        getNextToken();
        if (CurTok != tok_identifier) return Error(F("Expected command"));
        
        // Строка создается ТОЛЬКО если есть команда
        String Cmd = std::move(IdentifierStr);
        getNextToken();
        
        if (CurTok != '(') {
            // Это не вызов функции, а что-то другое
            return Error(F("Expected '(' after command"));
        }
        
        getNextToken(); // '('
        
        // Создаем CallExprAST с командой      
        ExprAST* callNode = new (std::nothrow) CallExprAST(nameHash, hashFNV1a(Cmd), Cmd);
        if (!callNode) {
            // Args уже спарсены? Нет, мы только начали. Но если были аргументы до этого — их нет.
            return Error(F("Out of memory"));
        }
        
        // Парсим аргументы
        if (CurTok != ')') {
            while (1) {
                ExprAST *Arg = ParseExpression(callFromCondition);
                if (!Arg) { delete callNode; return nullptr; }
                
                callNode->addArg(Arg);
                
                if (CurTok == ')') break;
                if (CurTok != ',') { delete callNode; return Error(F("Expected ')' or ','")); }
                getNextToken();
            }
        }
        getNextToken(); // ')'
        return callNode;
    }

    // Нет точки - это просто переменная или вызов системной функции
    if (CurTok != '(') {
        // Обычная переменная
        ExprAST* VarNode = new (std::nothrow) VariableExprAST(nameHash);
        if (!VarNode) return nullptr;
        
        // Проверка постфиксного инкремента/декремента
        if (CurTok == tok_inc) {
            getNextToken();
            ExprAST *One = new (std::nothrow) NumberExprAST(1.0f);
            //return new (std::nothrow) BinaryExprAST(tok_addassign, VarNode, One);
            if (!VarNode || !One) { delete VarNode; delete One; return nullptr; }
            return safeBinary(tok_addassign, VarNode, One);
        }
        else if (CurTok == tok_dec) {
            getNextToken();
            ExprAST *One = new (std::nothrow) NumberExprAST(1.0f);
            //return new (std::nothrow) BinaryExprAST(tok_subassign, VarNode, One);
            if (!VarNode || !One) { delete VarNode; delete One; return nullptr; }
            return safeBinary(tok_subassign, VarNode, One);
        }
        return VarNode;
    }

    // Вызов системной функции (без точки)
    getNextToken(); // '('
    
    // Создаем SysCallExprAST (команды нет)
    ExprAST* callNode = new (std::nothrow) SysCallExprAST(nameHash);
    if (!callNode) return Error(F("Out of memory"));
  
    // Парсим аргументы
    if (CurTok != ')') {
        while (1) {
            ExprAST *Arg = ParseExpression(callFromCondition);
            if (!Arg) { delete callNode; return nullptr; }
            
            callNode->addArg(Arg);
            
            if (CurTok == ')') break;
            if (CurTok != ',') { delete callNode; return Error(F("Expected ')' or ','")); }
            getNextToken();
        }
    }
    getNextToken(); // ')'
    
    return callNode;
}

/// numberexpr ::= number
ExprAST *IoTScenario::ParseNumberExpr() {
    if (isIotScenException) return nullptr;

    //ExprAST *Result = new (std::nothrow) NumberExprAST(NumVal);
    ExprAST *Result = new (std::nothrow) NumberExprAST(NumVal);
    if (!Result) return Error(F("Out of memory"));
    //getNextToken();
    //return Result;
    getNextToken();  // получаем число
    return Result;
}

/// parenexpr ::= '(' expression ')'
ExprAST* IoTScenario::ParseParenExpr(bool callFromCondition) {
    getNextToken();  // съедаем '('
    
    // Вызываем ParseExpression уже без передачи вектора
    ExprAST *V = ParseExpression(callFromCondition);
    if (!V) return nullptr;

    if (CurTok != ')')
        return Error(F("expected ')'"));
    
    getNextToken();  // съедаем ')'
    
    return V;
}

/// bracketsexpr ::= '{' expression '}'
ExprAST* IoTScenario::ParseBracketsExpr(bool callFromCondition) {
    getNextToken(); // '{'
    
    // Создаем объект один раз
    BracketsExprAST* node = new (std::nothrow) BracketsExprAST();

    if (!node) {
        SerialPrint("e", "SCENARIO", "NO MEMORY FOR BRACKET", "");
        return Error(F("Out of memory for BracketsExprAST"));
    }

    int count = 0;

    while (CurTok != '}') {
        // Вызываем ParseExpression БЕЗ параметров (так как мы настроили 
        // использование глобального буфера класса)
        ExprAST *Expr = ParseExpression(callFromCondition);
        
        if (!Expr) {
            delete node; 
            return nullptr;
        }
        
        // Добавляем выражение в объект сразу после парсинга
        node->addExpr(Expr);
        //SerialPrint("i","BracketsExprAST "+String(node->objectId), "count =", String(count));
        count++;

        if (CurTok == tok_eof) {
            delete node;
            return Error(F("Expected '}'"));
        }
    }
    
    getNextToken(); // '}'
    return node;
}

/// quotesexpr ::= '"' expression '"'
ExprAST *IoTScenario::ParseQuotesExpr() {
    if (isIotScenException) return nullptr;
    
    ExprAST *Result = new (std::nothrow) StringExprAST(std::move(IdentifierStr));   // Забираем строку из IdentifierStr БЕЗ копирования через std::move   
    if (!Result) return Error(F("Out of memory"));
    //IdentifierStr.reserve(16);                                           // Очищаем IdentifierStr, так как данные улетели в StringExprAST
    getNextToken();                                                  // получаем следующий токен
    return Result;
}

ExprAST* IoTScenario::ParseIfExpr() {
    if (isIotScenException) return nullptr;

    getNextToken(); // 'if'

    tempHashCount = 0;
    ExprAST *Cond = ParseExpression(true);
    if (!Cond) return nullptr;

    // ВАЖНО: снимаем копию хэшей ЭТОГО if сразу после парсинга условия —
    // до парсинга Then/Else, которые могут содержать вложенные if
    // и перезаписать общий tempHashBuffer/tempHashCount.
    uint32_t localHashes[MAX_TEMP_HASHES];
    uint8_t localHashCount = tempHashCount;
    for (uint8_t i = 0; i < localHashCount; i++) {
        localHashes[i] = tempHashBuffer[i];
    }

    if (CurTok != tok_then) {
        delete Cond;
        return Error(F("expected then"));
    }
    getNextToken(); // 'then'

    ExprAST *Then = ParseExpression(false); // тут возможна рекурсия в ParseIfExpr —
                                             // это ОК, копия локальных хэшей уже сохранена выше
    if (!Then) {
        delete Cond;
        return nullptr;
    }

    ExprAST *Else = nullptr;
    if (CurTok == tok_else) {
        getNextToken();
        Else = ParseExpression(false);
        if (!Else) {
            delete Cond;
            delete Then;
            return nullptr;
        }
    }

    //return new (std::nothrow) IfExprAST(Cond, Then, Else, localHashes, localHashCount);
    ExprAST* node = new (std::nothrow) IfExprAST(Cond, Then, Else, localHashes, localHashCount);

    if (!node) {
        delete Cond;
        delete Then;
        delete Else;
        return nullptr;
    }
    return node;
}

/// primary  ::= identifierexpr   ::= numberexpr   ::= parenexpr
ExprAST* IoTScenario::ParsePrimary(bool callFromCondition) {
    switch (CurTok) {
        default:
            Serial.println(CurTok);
            return Error(F("unknown token when expecting an expression"));
            
        case tok_identifier: {
            // ИСПОЛЬЗУЕМ ГЛОБАЛЬНЫЙ БУФЕР КЛАССА
            if (callFromCondition && tempHashCount < MAX_TEMP_HASHES) {
                tempHashBuffer[tempHashCount++] = IdentifierHash;
            }
            // Передаем только флаг, вектор больше не нужен           
            return ParseIdentifierExpr(callFromCondition);
        }
        
        case '-': {
            getNextToken();
            // Вызываем саму себя без вектора
            ExprAST *Operand = ParsePrimary(callFromCondition);
            if (!Operand) return nullptr;
            
            //ExprAST *Zero = new (std::nothrow) NumberExprAST(0.0);
            //return new (std::nothrow) BinaryExprAST('-', Zero, Operand);
            ExprAST *Zero = new (std::nothrow) NumberExprAST(0.0);
            if (!Zero || !Operand) { delete Zero; delete Operand; return nullptr; }
            return safeBinary('-', Zero, Operand);
        }

        case '!': {                                  // ← НОВОЕ
            getNextToken();                           // съедаем '!'
            ExprAST *Operand = ParsePrimary(callFromCondition);
            if (!Operand) return nullptr;
            //return new (std::nothrow) UnaryNotExprAST(Operand);
            ExprAST* node = new (std::nothrow) UnaryNotExprAST(Operand);
            if (!node) { delete Operand; return nullptr; }
            return node;
        }
        
        case tok_number:
            return ParseNumberExpr();
            
        case '(':
            return ParseParenExpr(callFromCondition);
            
        case '{':
            return ParseBracketsExpr(callFromCondition);
            
        case tok_string:
            return ParseQuotesExpr();
            
        case tok_if:            
            return ParseIfExpr(); 
    }
}

/// binoprhs
///   ::= ('+' primary)*
ExprAST* IoTScenario::ParseBinOpRHS(int ExprPrec, ExprAST *LHS, bool callFromCondition) {
    if (isIotScenException) return nullptr;

    while (1) {
        int TokPrec = GetTokPrecedence();

        if (TokPrec < ExprPrec)
            return LHS;

        int BinOp = CurTok;
        getNextToken();  // eat binop

        // Парсим первичное выражение уже БЕЗ вектора
        ExprAST *RHS = ParsePrimary(callFromCondition);
        if (!RHS) return nullptr;

        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec) {
            // Рекурсивный вызов теперь принимает только приоритет, узел и флаг
            RHS = ParseBinOpRHS(TokPrec + 1, RHS, callFromCondition);
            if (RHS == nullptr) return nullptr;
        }

        // Собираем дерево выражений
        //LHS = new (std::nothrow) BinaryExprAST(BinOp, LHS, RHS);
        LHS = safeBinary(BinOp, LHS, RHS);
        if (!LHS) return nullptr;
    }
}

ExprAST* IoTScenario::ParseExpression(bool callFromCondition) {
    // Вектор больше не нужен, используем глобальные переменные класса
    ExprAST *LHS = ParsePrimary(callFromCondition);
    if (!LHS) return nullptr;
    
    // Передаем callFromCondition дальше по цепочке
    return ParseBinOpRHS(0, LHS, callFromCondition);
}

bool IoTScenario::addCachedIf(ExprAST* node) {
    if (cachedIfCount >= cachedIfCapacity) {
        int newCapacity = cachedIfCapacity ? cachedIfCapacity * 2 : 4;

        ExprAST** newArr = (ExprAST**)realloc(cachedIfs, newCapacity * sizeof(ExprAST*));
        if (!newArr) {
            SerialPrint("e", "SCENARIO", "REALLOC FAIL cachedIfs", String(cachedIfCapacity));
            return false;
        }
        cachedIfs = newArr;
        cachedIfCapacity = newCapacity;
    }

    cachedIfs[cachedIfCount++] = node;
    return true;
}

void IoTScenario::loadScenario(const String& fileName) {
    isIotScenException = false;

    for (int i = 0; i < cachedIfCount; i++) {
        delete cachedIfs[i];
        cachedIfs[i] = nullptr; 
    }
    cachedIfCount = 0; // capacity и сам буфер НЕ освобождаем — переиспользуем при повторной загрузке

    if (file) file.close();
    file = FileFS.open(fileName.c_str(), FILE_READ);
    if (!file) {
        Error(F("Open file scenario error"));
        return;
    }

    LastChar = getLastChar();
    CurTok = 0;
    curLine = 1;
    getNextToken();

    while (CurTok != tok_eof) {
        if (CurTok == tok_if) {
            ExprAST* node = ParseIfExpr();
            if (node) {
                if (!addCachedIf(node)) {
                    Error(F("Out of memory caching IF statement!"));
                    delete node; // не потерять узел, если добавить не удалось
                }
            } else {
                getNextToken();
            }
        } else {
            getNextToken();
        }
    }

    file.close();

    Serial.printf("=== SCENARIO MEMORY REPORT ===\r\n");
    Serial.printf("Total AST objects created: %d\r\n", ExprAST::maxObjectCount);
    Serial.printf("Active AST objects: %d\r\n", ExprAST::objectCount);
    Serial.printf("Cached IF statements: %d\r\n", cachedIfCount);
    Serial.printf("Free heap: %d bytes\r\n", ESP.getFreeHeap());
    Serial.printf("==============================\r\n");
}

// void IoTScenario::loadScenario(const String& fileName) {
//     isIotScenException = false;

//     for (int i = 0; i < cachedIfCount; i++) {
//         delete cachedIfs[i];
//     }
//     cachedIfCount = 0;

//     // 2. Открытие файла
//     if (file) file.close(); 
//     file = FileFS.open(fileName.c_str(), FILE_READ); 
//     if (!file) { 
//         Error(F("Open file scenario error")); 
//         return; 
//     } 

//     // 3. Инициализация парсера
//     LastChar = getLastChar(); // Читаем первый символ прямо из файла
//     CurTok = 0; 
//     curLine = 1; 
//     getNextToken(); 

//     // 4. Парсинг
//     while (CurTok != tok_eof) { 
//         if (CurTok == tok_if) { 
//             ExprAST* node = ParseIfExpr();
//             if (node) { 
//                 //cachedIfs.push_back(node); 
//                 if (cachedIfCount < MAX_CACHED_IFS) {
//                     cachedIfs[cachedIfCount++] = node;
//                 } else {
//                     Error(F("Too many IF statements!"));
//                 }
//             } else { 
//                 getNextToken(); 
//             } 
//         } else { 
//             getNextToken(); 
//         } 
//     } 

//     file.close(); // Файл больше не нужен
//     //Serial.println("Scenario loaded successfully.");
//     //SerialPrint(F("i"), F("Scenario"), F("Loaded and cached if-statements :"),String(cachedIfCount)); 
//     Serial.printf("=== SCENARIO MEMORY REPORT ===\r\n");
//     Serial.printf("Total AST objects created: %d\r\n", ExprAST::maxObjectCount);
//     Serial.printf("Active AST objects: %d\r\n", ExprAST::objectCount);
//     Serial.printf("Cached IF statements: %d\r\n", cachedIfCount);
//     Serial.printf("Free heap: %d bytes\r\n", ESP.getFreeHeap());
//     Serial.printf("==============================\r\n");
// }

void IoTScenario::exec(const String& eventIdName) {
    
    if (cachedIfCount == 0 || eventIdName.length() == 0) return;

    // Считаем хэш входящего события ОДИН раз
    uint32_t eventHash = hashFNV1a(eventIdName);

    for (int i = 0; i < cachedIfCount; i++) {
    // cachedIfs[i] — это ваш узел (вместо node из итератора)
    if (cachedIfs[i]->hasEventIdName(eventHash)) {
        cachedIfs[i]->exec();
        
        // Прерываем, если случилась ошибка
        if (isIotScenException) break;   
    }
}
}

void IoTScenario::exec(uint32_t eventHash) {
    
    if (cachedIfCount == 0 || eventHash == 2166136261U) return;
  
    for (int i = 0; i < cachedIfCount; i++) {
        // cachedIfs[i] — это ваш узел (вместо node из итератора)
        if (cachedIfs[i]->hasEventIdName(eventHash)) {
            cachedIfs[i]->exec();
            
            // Прерываем, если случилась ошибка
            if (isIotScenException) break;   
        }
    }
}

IoTScenario::IoTScenario() {}

// IoTScenario::~IoTScenario() {
    
//     for (int i = 0; i < cachedIfCount; i++) {
//         delete cachedIfs[i];
//     }
//     if (file) file.close();
// }

IoTScenario::~IoTScenario() {
    for (int i = 0; i < cachedIfCount; i++) {
        delete cachedIfs[i];
        cachedIfs[i] = nullptr; 
    }
    
    cachedIfCount = 0;           // <-- добавить
    cachedIfCapacity = 0;        // <-- добавить
    free(cachedIfs); // ← добавлено: освобождаем сам массив-буфер
    cachedIfs = nullptr;         // <-- добавить
    if (file) file.close();
}