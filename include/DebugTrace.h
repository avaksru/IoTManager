#pragma once

// В папке toolchchain с которым собирались 
// (Для esp32 например %%USERPROFILE%/.platformio/packages/toolchain-xtensa-esp32@8.4.0+2021r2-patch5/bin)
// из командной строки Windows (cmd) запустить файл c параметрами:
// xtensa-esp32-elf-addr2line.exe -pfiaC -e Путь_к_файлу/firmware.elf Стэк_адресов_из_сообщения
// %%USERPROFILE%/.platformio/packages/toolchain-xtensa-esp32@8.4.0+2021r2-patch5/bin xtensa-esp32-elf-addr2line.exe -pfiaC -e .pio/build/esp32_4mb3f/firmware.elf Стэк_адресов
#include "Global.h"
#if defined(ESP32) && !defined(esp32c3m_4mb) && !defined(esp32c6_4mb) && !defined(esp32c6_8mb)
#define RESTART_DEBUG_INFO
#endif
#if defined(RESTART_DEBUG_INFO)
#define CONFIG_RESTART_DEBUG_STACK_DEPTH 15
typedef struct {
  size_t heap_total;
  size_t heap_free;
  size_t heap_free_min;
  time_t heap_min_time;
  uint32_t backtrace[CONFIG_RESTART_DEBUG_STACK_DEPTH];
} re_restart_debug_t;
__NOINIT_ATTR static int8_t bootloop_panic_count;

void IRAM_ATTR debugUpdate();
#endif // RESTART_DEBUG_INFO



extern "C" void __real_esp_panic_handler(void*);
void printDebugTrace();
void sendDebugTraceAndFreeMemory(bool);

void startWatchDog();
//extern "C" bool verifyRollbackLater();
void verifyFirmware();