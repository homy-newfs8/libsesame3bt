#pragma once
#include <array>
#include <cstdint>
#include <variant>
#include "handler_consts.h"
#include "os2.h"
#include "os3.h"

namespace libsesame3bt {

class Handler {
 public:
	template <typename T>
	Handler(std::in_place_type_t<T> t, SesameClient* client) : handler(t, client) {}
	bool init() {
		return std::visit([](auto& v) { return v.init(); }, handler);
	}
	bool set_keys(const char* pk_str, const char* secret_str) {
		return std::visit([pk_str, secret_str](auto& v) { return v.set_keys(pk_str, secret_str); }, handler);
	}
	bool set_keys(const std::array<std::byte, c::PK_SIZE>& public_key, const std::array<std::byte, c::SECRET_SIZE>& secret_key) {
		return std::visit([public_key, secret_key](auto& v) { return v.set_keys(public_key, secret_key); }, handler);
	}
	bool send_command(Sesame::op_code_t op_code,
	                  Sesame::item_code_t item_code,
	                  const std::byte* data,
	                  size_t data_size,
	                  bool is_crypted) {
		return std::visit([op_code, item_code, data, data_size,
		                   is_crypted](auto& v) { return v.send_command(op_code, item_code, data, data_size, is_crypted); },
		                  handler);
	}
	void update_enc_iv() {
		std::visit([](auto& v) { v.update_enc_iv(); }, handler);
	}
	void update_dec_iv() {
		std::visit([](auto& v) { v.update_dec_iv(); }, handler);
	}
	size_t get_max_history_tag_size() const {
		return std::visit([](auto& v) { return v.get_max_history_tag_size(); }, handler);
	}

	void handle_publish_initial(const std::byte* in, size_t in_len) {
		std::visit([in, in_len](auto& v) { v.handle_publish_initial(in, in_len); }, handler);
	}
	void handle_response_login(const std::byte* in, size_t in_len) {
		std::visit([in, in_len](auto& v) { v.handle_response_login(in, in_len); }, handler);
	}
	void handle_publish_mecha_setting(const std::byte* in, size_t in_len) {
		std::visit([in, in_len](auto& v) { v.handle_publish_mecha_setting(in, in_len); }, handler);
	}
	void handle_publish_mecha_status(const std::byte* in, size_t in_len) {
		std::visit([in, in_len](auto& v) { v.handle_publish_mecha_status(in, in_len); }, handler);
	}

	static constexpr size_t MAX_HISTORY_TAG_SIZE = std::max(OS2Handler::MAX_HISTORY_TAG_SIZE, OS3Handler::MAX_HISTORY_TAG_SIZE);

 private:
	std::variant<OS3Handler, OS2Handler> handler;
};

}  // namespace libsesame3bt
