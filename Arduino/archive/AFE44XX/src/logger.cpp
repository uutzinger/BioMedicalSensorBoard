#include "logger.h"

int currentLogLevel = LOG_LEVEL_DEBUG; // Default log level
char binaryBuffer[36];                 // int32 to binary string

void logPrintLevelln(const char* level, const char* format, ...) {
    // Custom logging messaging
    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");

    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Serial.println(buffer);
}

void logPrintLevel(const char* level, const char* format, ...) {
    // Custom logging messaging
    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");

    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Serial.print(buffer);
}

void logPrint(const char* format, ...) {
    // Custom logging messaging

    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Serial.print(buffer);
}

void logPrintln(const char* format, ...) {
    // Custom logging messaging

    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Serial.println(buffer);
}

char* intToBinaryString(uint32_t num) {
    // Converts integer to a string of 0s and 1s with commas after specific positions
    size_t bufferSize = sizeof(binaryBuffer);
    binaryBuffer[bufferSize - 1] = '\0'; // Null-terminate the string   
    int j = bufferSize - 2; // Index for buffer (excluding null terminator)
    for (int i = 0; i < 32; ++i) {
        if (i > 0 && (i % 8 == 0)) {
            binaryBuffer[j--] = ','; // Insert comma at positions 24, 16, and 8
        }
        binaryBuffer[j--] = (num & 1) ? '1' : '0';
        num >>= 1;
    }
    return binaryBuffer;
}
