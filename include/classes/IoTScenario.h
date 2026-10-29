#pragma once
#include "Global.h"
#include "classes/IoTItem.h"

class ExprAST {
   public:
#ifdef IoTSCEN_DEBUG
    static int objectCount;      // счетчик созданных объектов (для отладки, только с -DIoTSCEN_DEBUG)
    static int maxObjectCount;
    int objectId = 0;            // уникальный ID объекта
    ExprAST();
#endif
    virtual ~ExprAST();
    virtual IoTValue *exec();
    virtual int setValue(IoTValue *val, bool generateEvent);  // ret 0 - установка значения не поддерживается наследником
    // матч события по кэшированному хэшу FNV-1a имени (без строк и аллокаций)
    virtual bool hasEventIdName(uint32_t eventHash) { return false; }
    // пошаговая сборка списка аргументов для Call/SysCall (память выделяется только при парсинге)
    virtual void addArg(ExprAST *arg) {}
};

class IoTScenario {
    //===----------------------------------------------------------------------===//
    // Lexer (Лексический анализатор)
    //===----------------------------------------------------------------------===//
    String IdentifierStr;    // заполняется, если tok_identifier
    uint32_t IdentifierHash; // хэш текущего идентификатора, вычисляется прямо в лексере
    float NumVal;            // заполняется, если tok_number (собирается на лету, без String)
    int LastChar;
    int curLine;
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

    // Хэши идентификаторов, встреченных в разбираемом сейчас условии.
    // Снимок берётся сразу после парсинга условия - до вложенных if в then/else.
    static const uint8_t MAX_TEMP_HASHES = 8;
    uint32_t tempHashBuffer[MAX_TEMP_HASHES];
    uint8_t tempHashCount = 0;

    // Список top-level if-операторов сценария. Растёт через realloc -
    // аллокации происходят только при парсинге (loadScenario), не в exec(),
    // поэтому нет искусственного лимита на количество if в сценарии.
    ExprAST** cachedIfs = nullptr;
    int cachedIfCount = 0;
    int cachedIfCapacity = 0;
    // Добавляет узел в cachedIfs, увеличивая буфер при необходимости.
    // Возвращает false при нехватке памяти (узел в этом случае не добавлен).
    bool addCachedIf(ExprAST* node);

   public:
    void loadScenario(const String& fileName);   // парсим сценарий один раз в кэш
    void exec(const String& eventIdName);        // работа по имени (конвертируется в хэш)
    void exec(uint32_t eventHash);               // работа по хэшу

    IoTScenario();
    ~IoTScenario();
};
