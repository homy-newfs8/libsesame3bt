#pragma once

#if LIBSESAME3BT_DEBUG
#include <Arduino.h>
#define DEBUG_PRINTLN(...)      \
	do {                          \
		Serial.printf(__VA_ARGS__); \
		Serial.println();           \
	} while (false)
#define DEBUG_PRINT(...)        \
	do {                          \
		Serial.printf(__VA_ARGS__); \
	} while (false)
#define DEBUG_PRINTF(...)       \
	do {                          \
		Serial.printf(__VA_ARGS__); \
	} while (false)

#else

#define DEBUG_PRINTLN(...) \
	do {                      \
	} while (false)
#define DEBUG_PRINT(...) \
	do {                    \
	} while (false)
#define DEBUG_PRINTF(...) \
	do {                    \
	} while (false)

#endif
