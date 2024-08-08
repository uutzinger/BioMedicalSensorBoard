#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// Log levels
#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

// Current log level
extern int currentLogLevel;

// Macros for logging
#define LOGE(...)  if (currentLogLevel >= LOG_LEVEL_ERROR) { logPrintln("ERROR", __VA_ARGS__); }
#define LOGW(...)  if (currentLogLevel >= LOG_LEVEL_WARN)  { logPrintln("WARN",  __VA_ARGS__); }
#define LOGI(...)  if (currentLogLevel >= LOG_LEVEL_INFO)  { logPrintln("INFO",  __VA_ARGS__); }
#define LOGIS(...) if (currentLogLevel >= LOG_LEVEL_INFO)  { logPrintS("INFO",   __VA_ARGS__); }
#define LOGIC(...) if (currentLogLevel >= LOG_LEVEL_INFO)  { logPrintC("INFO",   __VA_ARGS__); }
#define LOGIE(...) if (currentLogLevel >= LOG_LEVEL_INFO)  { logPrintE("INFO",   __VA_ARGS__); }
#define LOGD(...)  if (currentLogLevel >= LOG_LEVEL_DEBUG) { logPrintln("DEBUG", __VA_ARGS__); }

// Functions to print log messages
void logPrint(const char* level, const char* format, ...);
void logPrintln(const char* level, const char* format, ...);
char* intToBinaryString(uint32_t num);

#endif // LOGGER_H
