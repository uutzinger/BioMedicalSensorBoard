#ifndef LOGGER_H
#define LOGGER_H

#ifndef USE_AUDIO_LOGGING

#include <Arduino.h>

// Current log level
extern int currentLogLevel;

// Log levels
#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

// Macros for logging
#define LOGE(...) if (currentLogLevel >= LOG_LEVEL_ERROR) { logPrintLevelln("ERROR", __VA_ARGS__); }
#define LOGW(...) if (currentLogLevel >= LOG_LEVEL_WARN)  { logPrintLevelln("WARN",  __VA_ARGS__); }
#define LOGI(...) if (currentLogLevel >= LOG_LEVEL_INFO)  { logPrintLevelln("INFO",  __VA_ARGS__); }
#ifdef DEBUG
    #define LOGD(...) if (currentLogLevel >= LOG_LEVEL_DEBUG) { logPrintLevelln("DEBUG", __VA_ARGS__); }
#else
    #define LOGD(...)  
#endif

// Continous info logging one some line with multiple calls (start, continue, end) (LOGI, LOGIC, LOGIE)
#define LOGIS(...) if (currentLogLevel >= LOG_LEVEL_INFO)  { logPrintLevel("INFO",  __VA_ARGS__); }
#define LOGIC(...) if (currentLogLevel >= LOG_LEVEL_INFO)  { logPrint(__VA_ARGS__); }
#define LOGIE(...) if (currentLogLevel >= LOG_LEVEL_INFO)  { logPrintln(__VA_ARGS__); }

#endif // USE_AUDIO_LOGGING

// Simplle log macros
#define LOG(...) logPrint(__VA_ARGS__);
#define LOGln(...) logPrintln(__VA_ARGS__);

// Functions to print log messages
void logPrintLevel(const char* level, const char* format, ...);
void logPrintLevelln(const char* level, const char* format, ...);
void logPrint(const char* format, ...);
void logPrintln(const char* format, ...);

char* intToBinaryString(uint32_t num);

#endif // LOGGER_H
