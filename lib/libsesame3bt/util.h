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
size_t truncate_utf8(const char* str, size_t len, size_t limit);
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
std::string bin2hex(const std::byte* data, size_t data_size, bool upper = false);

template <size_t N>
static inline uint8_t*
to_ptr(std::array<std::byte, N>& array) {
	return reinterpret_cast<uint8_t*>(array.data());
}

template <size_t N>
static inline const uint8_t*
to_cptr(const std::array<std::byte, N>& array) {
	return reinterpret_cast<const uint8_t*>(array.data());
}

static inline uint8_t*
to_ptr(std::byte* p) {
	return reinterpret_cast<uint8_t*>(p);
}

static inline const uint8_t*
to_cptr(const std::byte* p) {
	return reinterpret_cast<const uint8_t*>(p);
}

}  // namespace util
}  // namespace libsesame3bt
