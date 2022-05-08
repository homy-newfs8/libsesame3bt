#include "util.h"
#include <cstring>

namespace libsesame3bt {
namespace util {

size_t
truncate_utf8(const char* str, size_t limit) {
	if (str == nullptr || limit == 0) {
		return 0;
	}
	size_t len = strnlen(str, limit + 1);
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

}  // namespace util
}  // namespace libsesame3bt
