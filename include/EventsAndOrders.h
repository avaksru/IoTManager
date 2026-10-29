#pragma once
#include "Global.h"

// прямые команды (orders) выполняются сразу в контексте вызывающего: mqtt/ws/telegram/сценарии
void generateOrder(const String& id, const String& value);

// события (events) кладутся в безаллокационную FIFO - внутри только хэши имен, heap не фрагментируется
void generateEvent(const String& id, const String& value);
void generateEvent(uint32_t idHash);
void handleEvent();