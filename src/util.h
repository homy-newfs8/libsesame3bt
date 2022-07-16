#pragma once
#include <Arduino.h>
#include <cstddef>
#include <iterator>
#include <string>

namespace libsesame3bt {
namespace util {

template <class C>
constexpr auto size(const C& c) -> decltype(c.size());

template <class T, size_t N>
constexpr size_t size(const T (&array)[N]) noexcept;

template <class C>
auto
cbegin(const C& container) -> decltype(std::begin(container)) {
	return std::begin(container);
}

template <class C>
auto
cend(const C& container) -> decltype(std::end(container)) {
	return std::end(container);
}

size_t truncate_utf8(const char* str, size_t limit);

int8_t nibble(char c);
char hexchar(int b, bool upper = false);

template <size_t N>
bool
hex2bin(const char* str, std::array<std::byte, N>& out) {
	if (!str || strlen(str) != N * 2) {
		return false;
	}
	for (int i = 0; i < N; i++) {
		int8_t n1 = nibble(str[i * 2]);
		int8_t n2 = nibble(str[i * 2 + 1]);
		if (n1 < 0 || n2 < 0) {
			return false;
		}
		out[i] = std::byte{static_cast<uint8_t>((n1 << 4) + n2)};
	}
	return true;
}

static std::string
bin2hex(const std::byte* data, size_t data_size, bool upper = false) {
	std::string out(data_size * 2, 0);
	for (int i = 0; i < data_size; i++) {
		out[i * 2] = hexchar(std::to_integer<uint8_t>(data[i]) >> 4, upper);
		out[i * 2 + 1] = hexchar(std::to_integer<uint8_t>(data[i]) & 0x0f, upper);
	}

	return out;
}

}  // namespace util
}  // namespace libsesame3bt
