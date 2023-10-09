#pragma once
#include <array>
#include <cstdint>
#include "Sesame.h"
#include "api_wrapper.h"

namespace libsesame3bt {

class SesameClient;

class OS3Handler {
 public:
	OS3Handler(SesameClient* client) : client(client) {}
	OS3Handler(const OS3Handler&) = delete;
	OS3Handler& operator=(const OS3Handler&) = delete;
	bool init() { return true; }
	bool set_keys(const char* pk_str, const char* secret_str);
	bool set_keys(const std::array<std::byte, Sesame::PK_SIZE>& public_key,
	              const std::array<std::byte, Sesame::SECRET_SIZE>& secret_key);
	bool send_command(Sesame::op_code_t op_code,
	                  Sesame::item_code_t item_code,
	                  const std::byte* data,
	                  size_t data_size,
	                  bool is_crypted);
	void update_enc_iv();
	void update_dec_iv();

	void handle_publish_initial(const std::byte* in, size_t in_len);
	void handle_response_login(const std::byte* in, size_t in_len);
	void handle_publish_mecha_setting(const std::byte* in, size_t in_len);
	void handle_publish_mecha_status(const std::byte* in, size_t in_len);
	void handle_history(const std::byte* in, size_t in_len);
	size_t get_max_history_tag_size() const { return MAX_HISTORY_TAG_SIZE; }
	size_t get_cmd_tag_size(const std::byte* tag) const { return std::to_integer<size_t>(tag[0]) + 1; }
	static constexpr size_t MAX_HISTORY_TAG_SIZE = 29;

 private:
	SesameClient* client;
	std::array<std::byte, Sesame::SECRET_SIZE> sesame_secret{};
	long long enc_count = 0;
	long long dec_count = 0;
	bool setting_received = false;
	bool status_received = false;

	void init_endec_iv(const std::byte (&nonce)[Sesame::TOKEN_SIZE]);

	static float battery_voltage(int16_t battery) { return battery * 2.0f / 1000; };
	static constexpr int8_t voltage_scale(Sesame::model_t model) { return model == Sesame::model_t::sesame_bike_2 ? 2 : 1; };
};

}  // namespace libsesame3bt
