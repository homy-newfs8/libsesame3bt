#pragma once

#if LIBSESAME3BT_DEBUG
#include <Arduino.h>
#define DEBUG_PRINTLN(str) \
	do {                     \
		Serial.println(str);   \
	} while (false)
#define DEBUG_PRINT(fstr) \
	do {                    \
		Serial.print(fstr);   \
	} while (false)
#define DEBUG_PRINTF(...)       \
	do {                          \
		Serial.printf(__VA_ARGS__); \
	} while (false)
#define DEBUG_PRINTFP(...)        \
	do {                            \
		Serial.printf_P(__VA_ARGS__); \
	} while (false)

#else

#define DEBUG_PRINTLN(fstr) \
	do {                      \
	} while (false)
#define DEBUG_PRINT(fstr) \
	do {                    \
	} while (false)
#define DEBUG_PRINTF(...) \
	do {                    \
	} while (false)
#define DEBUG_PRINTFP(...) \
	do {                     \
	} while (false)

#endif
