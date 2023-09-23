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

static constexpr bool
utf_follow(char c) {
	return (c & 0xc0) == 0x80;
}

size_t
cleanup_tail_utf8(const char* str, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (str[i] == 0) {
			return i;
		}
		if ((str[i] & 0x80) == 0) {
			continue;
		}
		if ((str[i] & 0xf8) == 0xf0) {  // 4 bytes
			if (len >= i + 4 && utf_follow(str[i + 1]) && utf_follow(str[i + 2]) && utf_follow(str[i + 3])) {
				i += 3;
				continue;
			}
		}
		if ((str[i] & 0xf0) == 0xe0) {  // 3 bytes
			if (len >= i + 3 && utf_follow(str[i + 1]) && utf_follow(str[i + 2])) {
				i += 2;
				continue;
			}
		}
		if ((str[i] & 0xe0) == 0xc0) {  // 2 bytes
			if (len >= i + 2 && utf_follow(str[i + 1])) {
				++i;
				continue;
			}
		}
		return i;
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

std::string
bin2hex(const std::byte* data, size_t data_size, bool upper) {
	std::string out(data_size * 2, 0);
	for (int i = 0; i < data_size; i++) {
		out[i * 2] = hexchar(std::to_integer<uint8_t>(data[i]) >> 4, upper);
		out[i * 2 + 1] = hexchar(std::to_integer<uint8_t>(data[i]) & 0x0f, upper);
	}

	return out;
}

}  // namespace util
}  // namespace libsesame3bt
