#pragma once

#if LIBSESAME3BT_DEBUG
#if ARDUINO
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
#include <stdio.h>
#define DEBUG_PRINTLN(...) \
	do {                     \
		printf(__VA_ARGS__);   \
		putchar('\n');         \
	} while (false)
#define DEBUG_PRINT(...) \
	do {                   \
		printf(__VA_ARGS__); \
	} while (false)
#define DEBUG_PRINTF(...) \
	do {                    \
		printf(__VA_ARGS__);  \
	} while (false)
#endif
#else

#define DEBUG_PRINTLN(...) \
	do {                     \
	} while (false)
#define DEBUG_PRINT(...) \
	do {                   \
	} while (false)
#define DEBUG_PRINTF(...) \
	do {                    \
	} while (false)

#endif
