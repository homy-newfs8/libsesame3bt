#include <Arduino.h>
#include <unity.h>
#include "SesameClient.h"
#include "util.h"
#if __has_include("mysesame-config.h")
#include "mysesame-config.h"
#endif

#if !defined(SESAME_SECRET)
#define SESAME_SECRET "**REPLACE**"
#endif
#if !defined(SESAME_PK)
#define SESAME_PK "**REPLACE**"
#endif
#if !defined(SESAME_ADDRESS)
#define SESAME_ADDRESS "**REPLACE**"
#endif
#if !defined(SESAME_MODEL)
#define SESAME_MODEL Sesame::model_t::sesame_3
#endif

#define TEST_UTILITY 1
#define TEST_BLE 0

namespace util = libsesame3bt::util;
using libsesame3bt::Sesame;
using libsesame3bt::SesameClient;

void
test_truncate_utf8() {
	TEST_ASSERT_EQUAL(0, util::truncate_utf8(nullptr, 100));
	TEST_ASSERT_EQUAL(0, util::truncate_utf8(u8"", 10));
	TEST_ASSERT_EQUAL(0, util::truncate_utf8(u8"あ", 0));
	TEST_ASSERT_EQUAL(0, util::truncate_utf8(u8"あ", 1));
	TEST_ASSERT_EQUAL(0, util::truncate_utf8(u8"あ", 2));
	TEST_ASSERT_EQUAL(3, util::truncate_utf8(u8"あ", 3));
	TEST_ASSERT_EQUAL(3, util::truncate_utf8(u8"あ", 4));
	TEST_ASSERT_EQUAL(0, util::truncate_utf8(u8"a", 0));
	TEST_ASSERT_EQUAL(1, util::truncate_utf8(u8"a", 1));
	TEST_ASSERT_EQUAL(1, util::truncate_utf8(u8"a", 2));
	TEST_ASSERT_EQUAL(0, util::truncate_utf8(u8"aあ", 0));
	TEST_ASSERT_EQUAL(1, util::truncate_utf8(u8"aあ", 1));
	TEST_ASSERT_EQUAL(1, util::truncate_utf8(u8"aあ", 2));
	TEST_ASSERT_EQUAL(1, util::truncate_utf8(u8"aあ", 3));
	TEST_ASSERT_EQUAL(4, util::truncate_utf8(u8"aあ", 4));
	TEST_ASSERT_EQUAL(4, util::truncate_utf8(u8"aあ", 5));
	TEST_ASSERT_EQUAL(0, util::truncate_utf8(u8"あa", 0));
	TEST_ASSERT_EQUAL(0, util::truncate_utf8(u8"あa", 1));
	TEST_ASSERT_EQUAL(0, util::truncate_utf8(u8"あa", 2));
	TEST_ASSERT_EQUAL(3, util::truncate_utf8(u8"あa", 3));
	TEST_ASSERT_EQUAL(4, util::truncate_utf8(u8"あa", 4));
	TEST_ASSERT_EQUAL(4, util::truncate_utf8(u8"あa", 5));
}

void
test_cleanup_tail_utf8() {
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("", 0));
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("\0", 1));
	TEST_ASSERT_EQUAL(1, util::cleanup_tail_utf8("A"));
	TEST_ASSERT_EQUAL(2, util::cleanup_tail_utf8(u8"Ā"));
	TEST_ASSERT_EQUAL(3, util::cleanup_tail_utf8(u8"あ"));
	TEST_ASSERT_EQUAL(5, util::cleanup_tail_utf8(u8"AあB"));
	TEST_ASSERT_EQUAL(4, util::cleanup_tail_utf8(u8"😀"));

	// incorrect 2 bytes code
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("\xc4"));
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("\xc4\x00", 2));

	// incorrect 3 bytes code
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("\xe0\x81"));
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("\xe0"));
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("\xe0\x81\x00", 3));
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("\xe0\x20\x00", 3));

	// incorrect 4 bytes code
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("\xf0\x9f\x98"));
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("\xf0\x9f"));
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("\xf0"));
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("\xf0\x9f\x98\x00", 4));
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("\xf0\x30\x98\x80", 4));
	TEST_ASSERT_EQUAL(0, util::cleanup_tail_utf8("\xf0\x9f\x40\x80", 4));

	// mixture
	TEST_ASSERT_EQUAL(9, util::cleanup_tail_utf8("あ😀Ā\xe0\x81\x00", 12));
}

void
test_vol_pct() {
	TEST_ASSERT_EQUAL_FLOAT(0.0f, SesameClient::Status::voltage_to_pct(0.00f));
	TEST_ASSERT_EQUAL_FLOAT(0.0f, SesameClient::Status::voltage_to_pct(4.60f));
	TEST_ASSERT_EQUAL_FLOAT(100.0f, SesameClient::Status::voltage_to_pct(5.85f));
	TEST_ASSERT_EQUAL_FLOAT(100.0f, SesameClient::Status::voltage_to_pct(6.00f));
	TEST_ASSERT_EQUAL_FLOAT(50.0f, SesameClient::Status::voltage_to_pct(5.60f));
	TEST_ASSERT_EQUAL_FLOAT(40.0f, SesameClient::Status::voltage_to_pct(5.55f));
	TEST_ASSERT_EQUAL_FLOAT(32.0f, SesameClient::Status::voltage_to_pct(5.50f));
	TEST_ASSERT_EQUAL_FLOAT(21.0f, SesameClient::Status::voltage_to_pct(5.40f));
	TEST_ASSERT_EQUAL_FLOAT(13.0f, SesameClient::Status::voltage_to_pct(5.20f));
	TEST_ASSERT_EQUAL_FLOAT(10.0f, SesameClient::Status::voltage_to_pct(5.10f));
	TEST_ASSERT_EQUAL_FLOAT(7.0f, SesameClient::Status::voltage_to_pct(5.00f));
	TEST_ASSERT_EQUAL_FLOAT(3.0f, SesameClient::Status::voltage_to_pct(4.80f));

	float upper = SesameClient::Status::voltage_to_pct(5.84f);
	float lower = SesameClient::Status::voltage_to_pct(5.61f);
	TEST_ASSERT_MESSAGE(upper > lower, "upper > lower");
	TEST_ASSERT_MESSAGE(upper < 100.0f, "upper < 100%");
	TEST_ASSERT_MESSAGE(lower > 50.0f, "lower > 50%");
}

void
test_restart_while_disconnected() {
	NimBLEDevice::init("");
	SesameClient client{};
	client.set_keys(SESAME_PK, SESAME_SECRET);
	client.begin(BLEAddress{SESAME_ADDRESS, BLE_ADDR_RANDOM}, SESAME_MODEL);
	int testcount = 0;
	do {
		bool rc = false;
		Serial.println("connecting");
		for (int i = 0; i < 5; i++) {
			if (client.connect()) {
				rc = true;
				break;
			}
			delay(300);
		}
		if (!rc) {
			TEST_FAIL_MESSAGE("Failed to connect to sesame, abort");
			return;
		}
		Serial.println("authenticating");
		uint32_t begin = millis();
		rc = false;
		while (!client.is_session_active()) {
			if (millis() - begin > 5'000) {
				TEST_FAIL_MESSAGE("Authentication not finished in 5sec, abort");
				return;
			}
			delay(10);
		}
		Serial.println("disconnecting");
		client.disconnect();
		Serial.println("deiniting");
		NimBLEDevice::deinit(true);
		Serial.println("initing");
		NimBLEDevice::init("");
	} while (testcount++ < 3);
	TEST_PASS();
}

void
setup() {
	Serial.begin(115200);
	delay(3000);
	UNITY_BEGIN();
#if TEST_UTILITY
	RUN_TEST(test_truncate_utf8);
	RUN_TEST(test_cleanup_tail_utf8);
	RUN_TEST(test_vol_pct);
#endif
#if TEST_BLE
	RUN_TEST(test_restart_while_disconnected);
#endif
	UNITY_END();
}

void
loop() {
	delay(100);
}
