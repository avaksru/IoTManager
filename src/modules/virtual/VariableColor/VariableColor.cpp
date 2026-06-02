#include "Global.h"
#include "classes/IoTItem.h"

// дочь         -        родитель
class VariableColor : public IoTItem
{
private:
public:
    VariableColor(String parameters) : IoTItem(parameters)
    {
    }

    void doByInterval()
    {
    }

    // событие когда пользователь подключается приложением или веб интерфейсом к усройству
    void onMqttWsAppConnectEvent()
    {
        //  SerialPrint("i", "Connecting", "Dashbord open ");
        regEvent(String(value.valS.c_str()), "VariableColor", false, true);
    }

    IoTValue execute(String command, std::vector<IoTValue> &param)
    {
        if (command == "widget" && param.size() == 2)
        {
            String json = "{}";
            jsonWriteStr(json, String(param[0].valS.c_str()), String(param[1].valS.c_str()));
            String idStr = String(_id.c_str());
            sendSubWidgetsValues(idStr, json);
        }
        return {};
    }
};

void *getAPI_VariableColor(String subtype, String param)
{
    if (subtype == F("VariableColor"))
    {
        return new VariableColor(param);
    }
    else
    {
        return nullptr;
    }
}
