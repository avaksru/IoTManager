#pragma once
#include "Global.h"
#include "classes/IoTItem.h"

class ExprAST {
   public:
    // Счетчик созданных объектов (для отладки)
    static int objectCount;
    static int maxObjectCount;
    int objectId;  // Уникальный ID объекта
    
    ExprAST() {
        objectId = ++objectCount;
        if (objectCount > maxObjectCount) {
            maxObjectCount = objectCount;
        }
        //Serial.printf("[AST] Created #%d (total: %d, max: %d)\r\n", 
        //             objectId, objectCount, maxObjectCount);
    }
    
    virtual ~ExprAST() {
        objectCount--;
        //Serial.printf("[AST] Deleted #%d (remaining: %d)\r\n", 
        //             objectId, objectCount);
    }

    virtual IoTValue *exec();
    virtual int setValue(IoTValue *val, bool generateEvent); 
    virtual bool hasEventIdName(uint32_t eventHash) { return false; }
    virtual void addArg(ExprAST* arg) {}
};

class IoTScenario {
    
    //===----------------------------------------------------------------------===//
    // Lexer (Лексический анализатор)
    //===----------------------------------------------------------------------===//

    String IdentifierStr;    // Заполняется, если tok_identifier. Метод reserve() внутри gettok() защитит её от фрагментации
    uint32_t IdentifierHash; // Хэш текущего идентификатора   
    float NumVal;           // Заполняется, если tok_number   
    int LastChar;
    int curLine;

    /// gettok - Возвращает следующий токен из стандартного потока ввода.
    int gettok();

    //===----------------------------------------------------------------------===//
    // Parser (Парсер или Синтаксический Анализатор)
    //===----------------------------------------------------------------------===//

    int CurTok;
    int getNextToken();
    int GetTokPrecedence();

    ExprAST *Error(const String& Str);

    ExprAST *ParseNumberExpr();
    ExprAST *ParseQuotesExpr();
    ExprAST *ParseIdentifierExpr(bool callFromCondition);
    ExprAST *ParseParenExpr(bool callFromCondition);
    ExprAST *ParseBracketsExpr(bool callFromCondition);
    ExprAST *ParseIfExpr();
    ExprAST *ParsePrimary(bool callFromCondition);
    ExprAST *ParseBinOpRHS(int ExprPrec, ExprAST *LHS, bool callFromCondition);
    ExprAST *ParseExpression(bool callFromCondition);

    int getLastChar();
    fs::File file;

    // Добавьте это в секцию private вашего класса IoTScenario
    static const uint8_t MAX_TEMP_HASHES = 8;
    uint32_t tempHashBuffer[MAX_TEMP_HASHES];
    uint8_t tempHashCount = 0;

    //static const int MAX_CACHED_IFS = 30; // Установите лимит, сколько If может быть в сценарии
    //ExprAST* cachedIfs[MAX_CACHED_IFS];   // Массив указателей
    //int cachedIfCount = 0;               // Счетчик занятых ячеек

    // Стало:
    // Список top-level if-операторов сценария. Растёт через realloc —
    // аллокации происходят только при парсинге (loadScenario), не в exec(),
    // поэтому нет искусственного лимита на количество if в сценарии.
    ExprAST** cachedIfs = nullptr;
    int cachedIfCount = 0;
    int cachedIfCapacity = 0;

    // Добавляет узел в cachedIfs, увеличивая буфер при необходимости.
    // Возвращает false при нехватке памяти (узел в этом случае не добавлен).
    bool addCachedIf(ExprAST* node);

   public:
    void loadScenario(const String& fileName);
    void exec(const String& eventIdName);   // работа по имени
    void exec(uint32_t eventHash);   // работа по хешу 

    IoTScenario();
    ~IoTScenario(); // Деструктор теперь обязательно очищает память из-под strFromFile
};
