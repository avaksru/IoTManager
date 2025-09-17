#pragma once
#include <Arduino.h>
#include "Global.h"
#include "classes/IoTItem.h"

class IoTDiscovery : public IoTItem
{
public:
    IoTDiscovery(const String &parameters);
    ~IoTDiscovery();

//    inline bool isDiscoveryHomed() { return HOMEd; }

//    inline bool isDiscoveryHA() { return HA; }

    String HOMEdTopic = "";
    String HATopic = "";
    //String ChipId = "";

    virtual void mqttSubscribeDiscovery();

    virtual void publishStatusHOMEd(const String &topic, const String &data);



protected:
    boolean publishRetain(const String &topic, const String &data);
    virtual void getlayoutHA();
    virtual void deleteFromHOMEd();
    virtual void getlayoutHOMEd();

    //bool HOMEd = false;
    //bool HA = false;
    //String HOMEdTopic;

};