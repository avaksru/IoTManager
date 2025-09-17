#include "Global.h"
#include "classes/IoTDiscovery.h"

IoTDiscovery::IoTDiscovery(const String &parameters) : IoTItem(parameters)
{
    /*    int _tx, _rx, _speed, _line;
        jsonRead(parameters, "rx", _rx);
        jsonRead(parameters, "tx", _tx);
        jsonRead(parameters,  "speed", _speed);
        jsonRead(parameters,  "line", _line);
    */
    //ChipId = getChipId();
}

void IoTDiscovery::publishStatusHOMEd(const String &topic, const String &data) {}
void IoTDiscovery::getlayoutHA() {}
void IoTDiscovery::getlayoutHOMEd() {}
void IoTDiscovery::deleteFromHOMEd() {}
void IoTDiscovery::mqttSubscribeDiscovery(){}

boolean IoTDiscovery::publishRetain(const String &topic, const String &data)
{
    if (mqtt.beginPublish(topic.c_str(), data.length(), true))
    {
        mqtt.print(data);
        return mqtt.endPublish();
    }
    return false;
}

IoTDiscovery::~IoTDiscovery() {}
