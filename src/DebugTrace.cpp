#include "DebugTrace.h"
#if defined(RESTART_DEBUG_INFO)
// #ifdef RESTART_DEBUG_INFO
__NOINIT_ATTR static re_restart_debug_t _debug_info;

#include "esp_debug_helpers.h"
#include "esp_types.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "soc/soc_memory_layout.h"
#include "soc/cpu.h"

// RU: Размер буфера для конвертации даты и времeни в строку
#define CONFIG_FORMAT_STRFTIME_BUFFER_SIZE 32
#define CONFIG_FORMAT_STRFTIME_DTS_BUFFER_SIZE 20 // YYYY.MM.DD HH:NN:SS + \n

// RU: Форматы даты и времени
#define CONFIG_FORMAT_DTS "%d.%m.%Y %H:%M:%S"

void IRAM_ATTR debugHeapUpdate()
{
  _debug_info.heap_total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
  _debug_info.heap_free = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
  size_t _new_free_min = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
  if ((_debug_info.heap_free_min == 0) || (_new_free_min < _debug_info.heap_free_min))
  {
    _debug_info.heap_free_min = _new_free_min;
    _debug_info.heap_min_time = time(nullptr);
  };
}

void IRAM_ATTR debugBacktraceUpdate()
{
  esp_backtrace_frame_t stk_frame;
  esp_backtrace_get_start(&(stk_frame.pc), &(stk_frame.sp), &(stk_frame.next_pc));
  _debug_info.backtrace[0] = esp_cpu_process_stack_pc(stk_frame.pc);

  bool corrupted = (esp_stack_ptr_is_sane(stk_frame.sp) &&
                    esp_ptr_executable((void *)esp_cpu_process_stack_pc(stk_frame.pc)))
                       ? false
                       : true;

  uint8_t i = CONFIG_RESTART_DEBUG_STACK_DEPTH;
  while (i-- > 0 && stk_frame.next_pc != 0 && !corrupted)
  {
    if (!esp_backtrace_get_next_frame(&stk_frame))
    {
      corrupted = true;
    };
    _debug_info.backtrace[CONFIG_RESTART_DEBUG_STACK_DEPTH - i] = esp_cpu_process_stack_pc(stk_frame.pc);
  };
}

void IRAM_ATTR debugUpdate()
{
  debugHeapUpdate();
  debugBacktraceUpdate();
}

extern "C" void __wrap_esp_panic_handler(void *info)
{

  debugHeapUpdate();
  debugBacktraceUpdate();
  bootloop_panic_count += 1;
  // Call the original panic handler function to finish processing this error (creating a core dump for example...)
  __real_esp_panic_handler(info);
}

re_restart_debug_t debugGet()
{
  re_restart_debug_t ret;
  memset(&ret, 0, sizeof(re_restart_debug_t));
  esp_reset_reason_t esp_reason = esp_reset_reason();
  if ((esp_reason != ESP_RST_UNKNOWN) && (esp_reason != ESP_RST_POWERON))
  {
    uint8_t i = CONFIG_RESTART_DEBUG_STACK_DEPTH;
    ret = _debug_info;
    if (_debug_info.heap_total > heap_caps_get_total_size(MALLOC_CAP_DEFAULT))
    {
      memset(&ret, 0, sizeof(re_restart_debug_t));
    };
  };
  memset(&_debug_info, 0, sizeof(re_restart_debug_t));
  return ret;
}

#define CONFIG_MESSAGE_TG_VERSION_DEF "! Устройство запущено\n\nИмя устройства:     %s\nПричина перезапуска: %s\nCPU0: %s\nCPU1: %s"
#define CONFIG_MESSAGE_TG_VERSION_HEAP "! Устройство аварийно перезапущено !\n\nИмя устройства:     %s\nПричина перезапуска: %s\nCPU0: %s\nCPU1: %s\nHEAP: %s"
#define CONFIG_MESSAGE_TG_VERSION_TRACE "! Устройство  аварийно перезапущено !\n\nИмя устройства:     %s\nПричина перезапуска: %s\nCPU0:  %s\nCPU1:  %s\nHEAP:  %s\nTRACE: %s"

#define INFO_MESSAGE_DEBUG "By used -> USERPROFILE/.platformio/packages/toolchain-xtensa-esp32@8.4.0+2021r2-patch5/bin xtensa-esp32-elf-addr2line.exe -pfiaC -e .pio/build/esp32_4mb3f/firmware.elf Стэк_адресов"

char *malloc_stringf(const char *format, ...)
{
  char *ret = nullptr;
  if (format != nullptr)
  {
    // get the list of arguments
    va_list args1, args2;
    va_start(args1, format);
    va_copy(args2, args1);
    // calculate length of resulting string
    int len = vsnprintf(nullptr, 0, format, args1);
    va_end(args1);
    // allocate memory for string
    if (len > 0)
    {
#if USE_ESP_MALLOC
      ret = (char *)esp_malloc(len + 1);
#else
      ret = (char *)malloc(len + 1);
#endif
      if (ret != nullptr)
      {
        memset(ret, 0, len + 1);
        vsnprintf(ret, len + 1, format, args2);
      }
      else
      {
        // rlog_e(tagHEAP, "Failed to format string: out of memory!");
      };
    };
    va_end(args2);
  };
  return ret;
}

static char *statesGetDebugHeap(re_restart_debug_t *debug)
{
  if ((debug->heap_total > 0) && (debug->heap_total > debug->heap_free))
  {
    struct tm timeinfo;
    localtime_r(&debug->heap_min_time, &timeinfo);
    char time_buffer[CONFIG_FORMAT_STRFTIME_DTS_BUFFER_SIZE];
    memset(&time_buffer, 0, CONFIG_FORMAT_STRFTIME_DTS_BUFFER_SIZE);
    strftime(time_buffer, CONFIG_FORMAT_STRFTIME_DTS_BUFFER_SIZE, CONFIG_FORMAT_DTS, &timeinfo);

    double heapTotal = (double)debug->heap_total / 1024;
    double heapFree = (double)debug->heap_free / 1024;
    double heapFreeMin = (double)debug->heap_free_min / 1024;

    return malloc_stringf("Total %.1fkB ; Free %.1fkB (%.1f%%) ; FreeMin %.1fkB (%.1f%%) %s",
                          heapTotal,
                          heapFree, 100.0 * (heapFree / heapTotal),
                          heapFreeMin, 100.0 * (heapFreeMin / heapTotal), time_buffer);
  };
  return nullptr;
}

static char *statesGetDebugTrace(re_restart_debug_t *debug)
{
  char *backtrace = nullptr;
  char *item = nullptr;
  char *temp = nullptr;
  for (uint8_t i = 0; i < CONFIG_RESTART_DEBUG_STACK_DEPTH; i++)
  {
    if (debug->backtrace[i] != 0)
    {
      item = malloc_stringf("0x%08x", debug->backtrace[i]);
      if (item)
      {
        if (backtrace)
        {
          temp = backtrace;
          backtrace = malloc_stringf("%s %s", temp, item);
          free(item);
          free(temp);
        }
        else
        {
          backtrace = item;
        };
        item = nullptr;
      };
    }
    else
    {
      break;
    }
  };
  return backtrace;
}

void printDebugTrace()
{
  // esp_register_shutdown_handler(debugUpdate);
  re_restart_debug_t debug = debugGet();
  char *debug_heap = statesGetDebugHeap(&debug);
  char *debug_trace = nullptr;
  if (debug_heap)
  {
    debug_trace = statesGetDebugTrace(&debug);
    if (debug_trace)
    {
      Serial.printf(CONFIG_MESSAGE_TG_VERSION_TRACE,
                    jsonReadStr(settingsFlashJson, F("name")), ESP_getResetReason().c_str(), ESP32GetResetReason(0).c_str(), ESP32GetResetReason(1).c_str(),
                    debug_heap, debug_trace);
      //    free(debug_trace);
    }
    else
    {
      Serial.printf(CONFIG_MESSAGE_TG_VERSION_HEAP,
                    jsonReadStr(settingsFlashJson, F("name")), ESP_getResetReason().c_str(), ESP32GetResetReason(0).c_str(), ESP32GetResetReason(1).c_str(),
                    debug_heap);
    };
    //  free(debug_heap);
  }
  else
  {
    //            Serial.println("DEVICE START");
    Serial.printf(CONFIG_MESSAGE_TG_VERSION_DEF,
                  jsonReadStr(settingsFlashJson, F("name")), ESP_getResetReason().c_str(), ESP32GetResetReason(0).c_str(), ESP32GetResetReason(1).c_str());
  }

  Serial.println(INFO_MESSAGE_DEBUG);
}

void sendDebugTraceAndFreeMemory(bool postMsg)
{
  // esp_register_shutdown_handler(debugUpdate);
  re_restart_debug_t debug = debugGet();
  char *debug_heap = statesGetDebugHeap(&debug);
  char *debug_trace = nullptr;

  if (debug_heap)
  {
    if (isNetworkActive() && postMsg)
    {
      debug_trace = statesGetDebugTrace(&debug);
      if (debug_trace)
      {
        if (tlgrmItem)
        {
          char *msg;
          msg = malloc_stringf(CONFIG_MESSAGE_TG_VERSION_TRACE,
                               jsonReadStr(settingsFlashJson, F("name")).c_str(), ESP_getResetReason().c_str(), ESP32GetResetReason(0).c_str(), ESP32GetResetReason(1).c_str(),
                               debug_heap, debug_trace);
          tlgrmItem->sendTelegramMsg(false, String(msg));
          tlgrmItem->sendTelegramMsg(false, String("Подробности /helpDebug в Telegram_v2"));
          free(msg);
        }
        free(debug_trace);
      }
      else
      {
        /*
        Serial.printf(CONFIG_MESSAGE_TG_VERSION_HEAP,
                      jsonReadStr(settingsFlashJson, F("name")), ESP_getResetReason().c_str(), ESP32GetResetReason(0).c_str(), ESP32GetResetReason(1).c_str(),
                      debug_heap);
                      */
        if (tlgrmItem)
        {
          char *msg;
          msg = malloc_stringf(CONFIG_MESSAGE_TG_VERSION_HEAP,
                               jsonReadStr(settingsFlashJson, F("name")).c_str(), ESP_getResetReason().c_str(), ESP32GetResetReason(0).c_str(), ESP32GetResetReason(1).c_str(),
                               debug_heap);
          tlgrmItem->sendTelegramMsg(false, String(msg));
          tlgrmItem->sendTelegramMsg(false, String("Подробности /helpDebug в Telegram_v2"));
          free(msg);
        }
      }
    }
    free(debug_heap);
  }
  /*  else
    {
      //            Serial.println("DEVICE START");
      //  Serial.printf(CONFIG_MESSAGE_TG_VERSION_DEF,
      //               FIRMWARE_VERSION, ESP_getResetReason().c_str(), ESP32GetResetReason(0).c_str(), ESP32GetResetReason(1).c_str());
      if (tlgrmItem && isNetworkActive())
      {
        char *msg;
        msg = malloc_stringf(CONFIG_MESSAGE_TG_VERSION_DEF,
                              WiFi.localIP().toString(), FIRMWARE_VERSION, ESP_getResetReason().c_str(), ESP32GetResetReason(0).c_str(), ESP32GetResetReason(1).c_str());
        tlgrmItem->sendTelegramMsg(false, String(msg));
        free(msg);
      }
    };*/
}
#else // RESTART_DEBUG_INFO
void printDebugTrace() {}
void sendDebugTraceAndFreeMemory(bool) {}
//void IRAM_ATTR debugUpdate() {}
#if !defined(esp32c6_4mb) && !defined(esp32c6_8mb)
extern "C" void __wrap_esp_panic_handler(void *info)
{
  // Call the original panic handler function to finish processing this error (creating a core dump for example...)
  __real_esp_panic_handler(info);
}
#endif //esp32c6
#endif // !RESTART_DEBUG_INFO

#if defined(ESP32)

#include "esp_ota_ops.h"

#include <esp_task_wdt.h>
// 3 seconds WDT, reset in 1 seconds
#define WDT_TIMEOUT 180

void startWatchDog()
{
#if !defined(esp32c6_4mb) && !defined(esp32c6_8mb) //TODO esp32-c6 переписать esp_task_wdt_init
  esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);               // add current thread to WDT watch
#endif
}

extern "C" bool verifyRollbackLater()
{
  ets_printf("[SYSTEM] - verifyRollbackLater OVERRIDDEN FUNCTION!\n");
  return true;
}

void verifyFirmware()
{
  Serial.printf("[SYSTEM] - Checking firmware...\n");
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
  {
    const char *otaState = ota_state == ESP_OTA_IMG_NEW              ? "ESP_OTA_IMG_NEW"
                           : ota_state == ESP_OTA_IMG_PENDING_VERIFY ? "ESP_OTA_IMG_PENDING_VERIFY"
                           : ota_state == ESP_OTA_IMG_VALID          ? "ESP_OTA_IMG_VALID"
                           : ota_state == ESP_OTA_IMG_INVALID        ? "ESP_OTA_IMG_INVALID"
                           : ota_state == ESP_OTA_IMG_ABORTED        ? "ESP_OTA_IMG_ABORTED"
                                                                     : "ESP_OTA_IMG_UNDEFINED";
    Serial.printf("[SYSTEM] - Ota state: %s\n", otaState);

    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
    {
      if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
      {
        Serial.printf("[SYSTEM] - App is valid, rollback cancelled successfully\n");
      }
      else
      {
        Serial.printf("[SYSTEM] - Failed to cancel rollback\n");
      }
    }
  }
  else
  {
    Serial.printf("[SYSTEM] - OTA partition has no record in OTA data\n");
  }
} 
#else //ESP32
void startWatchDog() {}
//extern "C" bool verifyRollbackLater() {}
void verifyFirmware() {}
#endif