#include "util.h"
#include <cstring>

namespace libsesame3bt {
namespace util {

size_t
truncate_utf8(const char* str, size_t limit) {
	if (!str) {
		return 0;
	}
	return truncate_utf8(str, std::strlen(str), limit);
}

size_t
truncate_utf8(const char* str, size_t len, size_t limit) {
	if (!str || len == 0 || limit == 0) {
		return 0;
	}
	if (len > limit) {
		len = limit + 1;
	}
	while (len > limit) {
		len--;
		while (len > 0 && (str[len] & 0xc0) == 0x80) {
			len--;
		}
	}
	return len;
}

int8_t
nibble(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	return -1;
}

char
hexchar(int b, bool upper) {
	if (b >= 0 && b <= 9) {
		return '0' + b;
	}
	if (b >= 10 && b <= 15) {
		return (upper ? 'A' : 'a') + (b - 10);
	}
	return 0;
}

}  // namespace util
}  // namespace libsesame3bt
