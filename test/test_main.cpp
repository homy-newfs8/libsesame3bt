#include <Arduino.h>
#include <unity.h>
#include "SesameClient.h"
#include "util.h"

namespace util = libsesame3bt::util;
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
test_vol_pct() {
	TEST_ASSERT_EQUAL_FLOAT(0.0f, SesameClient::Status::voltage_to_pct(0.0f));
	TEST_ASSERT_EQUAL_FLOAT(0.0f, SesameClient::Status::voltage_to_pct(4.6f));
	TEST_ASSERT_EQUAL_FLOAT(100.0f, SesameClient::Status::voltage_to_pct(6.0f));
	TEST_ASSERT_EQUAL_FLOAT(100.0f, SesameClient::Status::voltage_to_pct(7.0f));
	TEST_ASSERT_EQUAL_FLOAT(50.0f, SesameClient::Status::voltage_to_pct(5.8f));
	TEST_ASSERT_EQUAL_FLOAT(40.0f, SesameClient::Status::voltage_to_pct(5.7f));
	TEST_ASSERT_EQUAL_FLOAT(32.0f, SesameClient::Status::voltage_to_pct(5.6f));
	TEST_ASSERT_EQUAL_FLOAT(21.0f, SesameClient::Status::voltage_to_pct(5.4f));
	TEST_ASSERT_EQUAL_FLOAT(13.0f, SesameClient::Status::voltage_to_pct(5.2f));
	TEST_ASSERT_EQUAL_FLOAT(10.0f, SesameClient::Status::voltage_to_pct(5.1f));
	TEST_ASSERT_EQUAL_FLOAT(7.0f, SesameClient::Status::voltage_to_pct(5.0f));
	TEST_ASSERT_EQUAL_FLOAT(3.0f, SesameClient::Status::voltage_to_pct(4.8f));

	float upper = SesameClient::Status::voltage_to_pct(5.83f);
	float lower = SesameClient::Status::voltage_to_pct(5.82f);
	TEST_ASSERT_MESSAGE(upper > lower, "upper > lower");
	TEST_ASSERT_MESSAGE(upper < 100.0f, "upper < 100%");
	TEST_ASSERT_MESSAGE(lower > 50.0f, "lower > 50%");
}

void
setup() {
	delay(3000);
	UNITY_BEGIN();
	RUN_TEST(test_truncate_utf8);
	RUN_TEST(test_vol_pct);
	UNITY_END();
}
void
loop() {
	delay(100);
}
