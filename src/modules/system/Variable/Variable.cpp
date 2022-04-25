#include "Global.h"
#include "classes/IoTItem.h"
#include "classes/IoTRTC.h"

extern IoTRTC *watch;

class Variable : public IoTItem {
   private:
    String _dtFormat;
   public:
    Variable(String parameters): IoTItem(parameters) {
        jsonRead(parameters, "dtFormat", _dtFormat);
        _dtFormat = _dtFormat;
    }

    // особенность данного модуля - просто хранение значения для сценария, нет событий
    // void setValue(IoTValue Value) {
    //     value = Value;
    // }

    void doByInterval() {
        if (_dtFormat != "") {
            value.isDecimal = false;
            value.valS = watch->gettime(_dtFormat + " ");
        }
    }
};

void* getAPI_Variable(String subtype, String param) {
    if (subtype == F("Variable")) {
        return new Variable(param);
    } else {
        return nullptr;
    }
}
