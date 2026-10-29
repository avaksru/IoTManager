#include "EventsAndOrders.h"
#include "classes/IoTScenario.h"

#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>  // portMUX_TYPE / portENTER_CRITICAL - защита очереди событий
#endif

extern IoTScenario iotScen;  // объект управления сценарием

//=====================================================================================
// События едут через безаллокационную FIFO (замена старых строковых eventBuf/orderBuf):
// - в очереди лежит только hashFNV1a(id), поэтому куча не фрагментируется;
// - элемент и сценарии матчатся по кэшированным хэшам имен (IoTItem::_idHash и IfExprAST);
// - команды (orders) очереди не требуют: generateOrder выполняет их сразу.
//=====================================================================================

// hashFNV1a("") == 2166136261U - это offset basis FNV-1a, он же хэш пустого имени.
// Используем как признак "мусорного" события (страховка: продюсеры сами такое не пушат).
static const uint32_t EMPTY_ID_HASH = 2166136261U;

#ifdef ESP32
// На ESP32 (в т.ч. C6) среда FreeRTOS: события могут пушиться не только из loopTask,
// но и из других задач (на esp32c6_8mb это коллбэк NimBLE - BleSens::setBLEdata).
// Доступ к индексам очереди защищаем критической секцией.
// На ESP8266 один контекст loop и ISR события не пушат - достаточно атомарной записи слова.
static portMUX_TYPE eventFifoMux = portMUX_INITIALIZER_UNLOCKED;
#endif

class EspFifoBuffer {
   private:
    // Для ESP8266 выделение памяти под 32 хэша — это ничтожно мало, зато надежно
    static const size_t CAPACITY = 32;
    static const size_t MASK = CAPACITY - 1;  // 31 или 0x1F
    static_assert((CAPACITY & (CAPACITY - 1)) == 0, "CAPACITY must be power of two");

    uint32_t entries[CAPACITY];

    // volatile обязателен: head/tail читаются и пишутся из разных контекстов.
    // На ESP32 конкурентный доступ из разных задач FreeRTOS закрыт критической секцией eventFifoMux,
    // на ESP8266 гарантируется единственный контекст loop (ISR события не генерируют).
    volatile size_t head = 0;
    volatile size_t tail = 0;

   public:
    // Добавить элемент. Возвращает true, если успешно.
    // Если буфер полон — возвращает false и данные теряются (generateEvent логирует это).
    bool push(uint32_t item) {
        size_t nextTail = (tail + 1) & MASK;

#ifdef ESP32
        // ESP32: push возможен из другой задачи (например, коллбэка NimBLE), а не только из loopTask
        portENTER_CRITICAL(&eventFifoMux);
        if (nextTail != head) {
            entries[tail] = item;
            tail = nextTail;
        }
        portEXIT_CRITICAL(&eventFifoMux);
        return nextTail != head;
#else
        if (nextTail == head) {
            return false;  // Буфер переполнен
        }

        entries[tail] = item;
        tail = nextTail;  // На ESP8266 происходит атомарно: один продюсер в контексте loop
        return true;
#endif
    }

    // Извлечь элемент. Возвращает true, если успешно, и записывает значение в item.
    // Если буфер пуст — возвращает false.
    bool pop(uint32_t& item) {
#ifdef ESP32
        // ESP32: под критической секцией, чтобы pop не пересекался с push из другой задачи
        portENTER_CRITICAL(&eventFifoMux);
        bool hasData = (head != tail);
        if (hasData) {
            item = entries[head];
            head = (head + 1) & MASK;
        }
        portEXIT_CRITICAL(&eventFifoMux);
        return hasData;
#else
        if (head == tail) {
            return false;  // Буфер пуст
        }

        item = entries[head];
        head = (head + 1) & MASK;  // На ESP8266 происходит атомарно
        return true;
#endif
    }

    bool isEmpty() const { return head == tail; }
    bool isFull() const { return ((tail + 1) & MASK) == head; }
};

EspFifoBuffer eventFifo;

//эта функция будет вызываться:
// 1.на приеме сообщений в mqtt (см mqttCallback в mqttClient.cpp)
// 2.на приеме сообщений в ws
// 3.в сценарях когда надо выполнить команды
void generateOrder(const String& id, const String& value) {
    IoTItem* item = findIoTItem(id);
    if (item) {
        SerialPrint(F("i"), F("ORDER"), id + " " + value);  // прямая команда, нужно тут же изменить параметр
        item->setValue(value);                              // выполняем сразу, без очереди
    }
    // команды к несуществующим элементам молча игнорируем (как и раньше)
}

//эта функция будет вызываться:
// 1.в местах где событие только что произошло (IoTItem::regEvent)
void generateEvent(const String& id, const String& value) {
    (void)value;  // значение в очередь не пишем, актуальное значение берется из самого элемента при обработке
    if (id.length() == 0) return;

    if (!eventFifo.push(hashFNV1a(id.c_str()))) {
        // переполнение означает потерю события - логируем с прореживанием, чтобы не завалить serial
        static uint32_t lastOverflowLog = 0;
        uint32_t now = millis();
        if (now - lastOverflowLog > 5000) {
            lastOverflowLog = now;
            SerialPrint(F("E"), F("EVENT"), "FIFO overflow, event dropped");
        }
    }
}

// для вызова из мест, где id уже хранится хэшем
void generateEvent(uint32_t idHash) {
    if (idHash != EMPTY_ID_HASH) eventFifo.push(idHash);
}

//эта функция будет вызываться в loop()
void handleEvent() {
    if (eventFifo.isEmpty()) return;

    uint32_t eventIdHash = 0;
    eventFifo.pop(eventIdHash);

    if (eventIdHash == EMPTY_ID_HASH) return;  // страховка от хэша пустой строки

    // поиск по кэшированному хэшу имени - быстрее, чем сравнение строк
    IoTItem* eventIoTItem = findIoTItem(eventIdHash);

    if (eventIoTItem) {
        SerialPrint(F("i"), F("EVENT"), String(eventIoTItem->getID().c_str()) + " " + eventIoTItem->getValue());

        // распространяем событие через хуки
        for (std::list<IoTItem*>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
            (*it)->onRegEvent(eventIoTItem);  // прямой хук

            // вызов хука при условии отслеживания изменения
            if ((*it)->isTracking(eventIoTItem)) (*it)->onTrackingValue(eventIoTItem);
        }
    } else {
        // элемент мог быть удален (например, временный элемент из сети) - по имени не напечатаем,
        // показываем хэш; событие все равно уходит в сценарии (матчинг в сценариях по хэшам)
        SerialPrint(F("i"), F("EVENT"), "event of unknown item, hash 0x" + String(eventIdHash, HEX));
    }

    //здесь нужно пропускать данное событие через условия сценариев
    //и если оно есть в условии сценария и совпадает. Матчинг в сценариях идет по хэшам имен
    //(IfExprAST::hasEventIdName), поэтому событие дойдет до сценария даже если элемент уже удален
    iotScen.exec(eventIdHash);
}