#include <Arduino.h>
#include <unity.h>
#include "util.h"

namespace util = libsesame3bt::util;

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
setup() {
	delay(3000);
	UNITY_BEGIN();
	RUN_TEST(test_truncate_utf8);
	UNITY_END();
}
void
loop() {
	delay(100);
}
