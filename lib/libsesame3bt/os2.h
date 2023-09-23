#pragma once
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <array>
#include <cstdint>
#include "Sesame.h"
#include "api_wrapper.h"
#include "handler_consts.h"

namespace libsesame3bt {

class SesameClient;

class OS2Handler {
 public:
	OS2Handler(SesameClient* client) : client(client) {}
	OS2Handler(const OS2Handler&) = delete;
	OS2Handler& operator=(const OS2Handler&) = delete;
	bool init() { return static_initialized; }
	bool set_keys(const char* pk_str, const char* secret_str);
	bool set_keys(const std::array<std::byte, c::PK_SIZE>& public_key, const std::array<std::byte, c::SECRET_SIZE>& secret_key);
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
	size_t get_cmd_tag_size(const std::byte* tag) const { return MAX_HISTORY_TAG_SIZE + 1; }
	static constexpr size_t MAX_HISTORY_TAG_SIZE = 21;

 private:
	SesameClient* client;
	api_wrapper<mbedtls_ecp_point> sesame_pk{mbedtls_ecp_point_init, mbedtls_ecp_point_free};
	std::array<std::byte, c::SECRET_SIZE> sesame_secret{};
	long long enc_count = 0;
	long long dec_count = 0;

	static bool static_initialized;
	static constexpr std::array<std::byte, 2> sesame_ki{};
	static inline api_wrapper<mbedtls_ctr_drbg_context> rng_ctx{mbedtls_ctr_drbg_init, mbedtls_ctr_drbg_free};
	static inline api_wrapper<mbedtls_entropy_context> ent_ctx{mbedtls_entropy_init, mbedtls_entropy_free};
	static inline api_wrapper<mbedtls_ecp_group> ec_grp{mbedtls_ecp_group_init, mbedtls_ecp_group_free};
	static constexpr size_t SK_SIZE = 32;
	static constexpr size_t AES_BLOCK_SIZE = 16;

	bool generate_session_key(const std::array<std::byte, Sesame::TOKEN_SIZE>& local_tok,
	                          const std::byte (&sesame_token)[Sesame::TOKEN_SIZE],
	                          std::array<std::byte, 1 + c::PK_SIZE>& pk);
	void init_endec_iv(const std::array<std::byte, Sesame::TOKEN_SIZE>& local_tok,
	                   const std::byte (&sesame_token)[Sesame::TOKEN_SIZE]);
	bool ecdh(const api_wrapper<mbedtls_mpi>& sk, std::array<std::byte, SK_SIZE>& out);
	bool create_key_pair(api_wrapper<mbedtls_mpi>& sk, std::array<std::byte, 1 + c::PK_SIZE>& pk);
	bool generate_tag_response(const std::array<std::byte, 1 + c::PK_SIZE>& pk,
	                           const std::array<std::byte, Sesame::TOKEN_SIZE>& local_token,
	                           const std::byte (&sesame_token)[Sesame::TOKEN_SIZE],
	                           std::array<std::byte, AES_BLOCK_SIZE>& tag_response);
	void update_sesame_status(const Sesame::mecha_status_t& mecha_status);
	static float battery_voltage(Sesame::model_t model, int16_t battery);
	static constexpr int8_t voltage_scale(Sesame::model_t model) {
		return model == Sesame::model_t::sesame_bike || model == Sesame::model_t::sesame_bot ? 2 : 1;
	};
};

}  // namespace libsesame3bt
