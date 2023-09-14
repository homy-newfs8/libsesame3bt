#include "os2.h"
#include <mbedtls/cmac.h>
#include <mbedtls/ecdh.h>
#include "SesameClient.h"
#include "util.h"

#ifndef LIBSESAME3BT_DEBUG
#define LIBSESAME3BT_DEBUG 0
#endif
#include "debug.h"

namespace libsesame3bt {

namespace {

constexpr size_t AUTH_TAG_TRUNCATED_SIZE = 4;
constexpr size_t AES_KEY_SIZE = 16;
constexpr size_t IV_COUNTER_SIZE = 5;

}  // namespace

using util::to_cptr;
using util::to_ptr;

bool OS2Handler::static_initialized = [] {
	if (mbedtls_ctr_drbg_seed(&rng_ctx, mbedtls_entropy_func, &ent_ctx, nullptr, 0) != 0) {
		DEBUG_PRINTLN("drbg_seed failed");
		return false;
	}
	if (mbedtls_ecp_group_load(&ec_grp, mbedtls_ecp_group_id::MBEDTLS_ECP_DP_SECP256R1) != 0) {
		DEBUG_PRINTLN("ecp_group_load failed");
		return false;
	}
	return true;
}();

bool
OS2Handler::set_keys(const char* pk_str, const char* secret_str) {
	if (!pk_str || !secret_str) {
		DEBUG_PRINTLN("Both pk_str and secret_str must be specified");
		return false;
	}
	std::array<std::byte, c::SECRET_SIZE> secret;
	if (!util::hex2bin(secret_str, secret)) {
		DEBUG_PRINTLN("secret_str invalid format");
		return false;
	}
	std::array<std::byte, c::PK_SIZE> pk;
	if (!util::hex2bin(pk_str, pk)) {
		DEBUG_PRINTLN("pk_str invalid format");
		return false;
	}
	return set_keys(pk, secret);

	return false;
}

bool
OS2Handler::set_keys(const std::array<std::byte, c::PK_SIZE>& public_key, const std::array<std::byte, c::SECRET_SIZE>& secret_key) {
	std::array<std::byte, 1 + c::PK_SIZE> bin_pk;  // 1 for indicator (SEC1 2.3.4)
	bin_pk[0] = std::byte{4};                      // uncompressed point indicator
	std::copy(std::cbegin(public_key), std::cend(public_key), &bin_pk[1]);
	int mbrc;
	if ((mbrc = mbedtls_ecp_point_read_binary(&ec_grp, &sesame_pk, to_cptr(bin_pk), bin_pk.size())) != 0) {
		DEBUG_PRINTF("%d: ecp_point_read_binary failed", mbrc);
		return false;
	}
	if ((mbrc = mbedtls_ecp_check_pubkey(&ec_grp, &sesame_pk)) != 0) {
		DEBUG_PRINTF("%d: ecp_check_pubkey failed", mbrc);
		return false;
	}
	std::copy(std::cbegin(secret_key), std::cend(secret_key), std::begin(sesame_secret));
	client->is_key_set = true;

	return true;
}

bool
OS2Handler::send_command(Sesame::op_code_t op_code,
                         Sesame::item_code_t item_code,
                         const std::byte* data,
                         size_t data_size,
                         bool is_crypted) {
	const size_t pkt_size = 2 + data_size + (is_crypted ? c::TAG_SIZE : 0);  // 2 for op/item, 4 for encrypted tag
	std::byte pkt[pkt_size];
	if (is_crypted) {
		std::byte plain[2 + data_size];
		plain[0] = std::byte{op_code};
		plain[1] = std::byte{item_code};
		std::copy(data, data + data_size, &plain[2]);
		if (!client->encrypt(plain, sizeof(plain), pkt, sizeof(pkt))) {
			return false;
		}
	} else {
		pkt[0] = std::byte{op_code};
		pkt[1] = std::byte{item_code};
		std::copy(data, data + data_size, &pkt[2]);
	}
	return client->send_data(pkt, pkt_size, is_crypted);
}

void
OS2Handler::update_enc_iv() {
	enc_count++;
	enc_count &= 0x7fffffffffLL;
	enc_count |= 0x8000000000LL;
	auto p = reinterpret_cast<const std::byte*>(&enc_count);
	std::copy(p, p + IV_COUNTER_SIZE, std::begin(client->enc_iv));
}

void
OS2Handler::update_dec_iv() {
	dec_count++;
	dec_count &= 0x7fffffffffLL;
	auto p = reinterpret_cast<const std::byte*>(&dec_count);
	std::copy(p, p + IV_COUNTER_SIZE, std::begin(client->dec_iv));
}

void
OS2Handler::handle_publish_initial(const std::byte* in, size_t in_len) {
	if (in_len < sizeof(Sesame::publish_initial_t)) {
		DEBUG_PRINTF("%u: short response initial data\n", in_len);
		client->disconnect();
		return;
	}
	client->reset_session();
	auto msg = reinterpret_cast<const Sesame::publish_initial_t*>(in);

	std::array<std::byte, Sesame::TOKEN_SIZE> local_tok;
	int mbrc;
	if ((mbrc = mbedtls_ctr_drbg_random(&rng_ctx, to_ptr(local_tok), local_tok.size())) != 0) {
		DEBUG_PRINTF("%d: drbg_random failed\n", mbrc);
		client->disconnect();
		return;
	}

	std::array<std::byte, 1 + c::PK_SIZE> bpk;
	if (!generate_session_key(local_tok, msg->token, bpk)) {
		client->disconnect();
		return;
	}
	init_endec_iv(local_tok, msg->token);
	std::array<std::byte, AES_BLOCK_SIZE> tag_response;
	if (!generate_tag_response(bpk, local_tok, msg->token, tag_response)) {
		client->disconnect();
		return;
	}

	constexpr size_t resp_size = sesame_ki.size() + c::PK_SIZE + local_tok.size() + AUTH_TAG_TRUNCATED_SIZE;
	std::array<std::byte, resp_size> resp;

	// resp = sesame_ki + pk + local_tok + tag_response[:4]
	std::copy(tag_response.cbegin(), tag_response.cbegin() + AUTH_TAG_TRUNCATED_SIZE,
	          std::copy(local_tok.cbegin(), local_tok.cend(),
	                    std::copy(bpk.begin() + 1, bpk.end(), std::copy(sesame_ki.cbegin(), sesame_ki.cend(), resp.begin()))));

	if (send_command(Sesame::op_code_t::sync, Sesame::item_code_t::login, resp.data(), resp.size(), false)) {
		client->update_state(SesameClient::state_t::authenticating);
	} else {
		client->disconnect();
	}
}

void
OS2Handler::handle_response_login(const std::byte* in, size_t in_len) {
	if (in_len < sizeof(Sesame::response_login_t)) {
		DEBUG_PRINTLN("short response login message");
		client->disconnect();
		return;
	}
	auto msg = reinterpret_cast<const Sesame::response_login_t*>(in);
	if (msg->result != Sesame::result_code_t::success) {
		DEBUG_PRINTF("%u: login response was not success\n", static_cast<uint8_t>(msg->result));
		client->disconnect();
		return;
	}
	if (client->model == Sesame::model_t::sesame_bot) {
		client->setting.emplace<SesameClient::BotSetting>(msg->mecha_setting);
	} else {
		client->setting.emplace<SesameClient::LockSetting>(msg->mecha_setting);
	}
	update_sesame_status(msg->mecha_status);
	client->update_state(SesameClient::state_t::active);
	client->fire_status_callback();
}

void
OS2Handler::update_sesame_status(const Sesame::mecha_status_t& mecha_status) {
	if (client->model == Sesame::model_t::sesame_bot) {
		client->sesame_status = {mecha_status.bot, battery_voltage(client->model, mecha_status.bot.battery)};
	} else {
		client->sesame_status = {mecha_status.lock, battery_voltage(client->model, mecha_status.lock.battery),
		                         voltage_scale(client->model)};
	}
}
void
OS2Handler::handle_publish_mecha_setting(const std::byte* in, size_t in_len) {
	if (in_len < sizeof(Sesame::publish_mecha_setting_t)) {
		DEBUG_PRINTF("%u: Unexpected size of mecha setting, ignored\n", in_len);
		return;
	}
	auto msg = reinterpret_cast<const Sesame::publish_mecha_setting_t*>(in);
	if (client->model == Sesame::model_t::sesame_bot) {
		client->setting.emplace<SesameClient::BotSetting>(msg->setting);
	} else {
		client->setting.emplace<SesameClient::LockSetting>(msg->setting);
	}
}

void
OS2Handler::handle_publish_mecha_status(const std::byte* in, size_t in_len) {
	if (in_len < sizeof(Sesame::publish_mecha_status_t)) {
		DEBUG_PRINTF("%u: Unexpected size of mecha status, ignored\n", in_len);
		return;
	}
	auto msg = reinterpret_cast<const Sesame::publish_mecha_status_t*>(in);
	update_sesame_status(msg->status);
	client->fire_status_callback();
}

bool
OS2Handler::generate_session_key(const std::array<std::byte, Sesame::TOKEN_SIZE>& local_tok,
                                 const std::byte (&sesame_token)[Sesame::TOKEN_SIZE],
                                 std::array<std::byte, 1 + c::PK_SIZE>& pk) {
	api_wrapper<mbedtls_cipher_context_t> ctx{mbedtls_cipher_init, mbedtls_cipher_free};
	api_wrapper<mbedtls_mpi> sk{mbedtls_mpi_init, mbedtls_mpi_free};

	if (!create_key_pair(sk, pk)) {
		return false;
	}
	std::array<std::byte, SK_SIZE> ssec;
	if (!ecdh(sk, ssec)) {
		return false;
	}

	int mbrc;
	if ((mbrc = mbedtls_cipher_setup(&ctx, mbedtls_cipher_info_from_type(mbedtls_cipher_type_t::MBEDTLS_CIPHER_AES_128_ECB))) != 0) {
		DEBUG_PRINTF("%d: cipher setup failed\n", mbrc);
		return false;
	}
	if ((mbrc = mbedtls_cipher_cmac_starts(&ctx, to_cptr(ssec), AES_KEY_SIZE * 8)) != 0) {
		DEBUG_PRINTF("%d: cmac start failed\n", mbrc);
		return false;
	}
	if ((mbrc = mbedtls_cipher_cmac_update(&ctx, to_cptr(local_tok), local_tok.size())) != 0 ||
	    (mbrc = mbedtls_cipher_cmac_update(&ctx, to_cptr(sesame_token), std::size(sesame_token))) != 0) {
		DEBUG_PRINTF("%d: cmac_update failed\n", mbrc);
		return false;
	}
	std::array<std::byte, 16> session_key;
	if ((mbrc = mbedtls_cipher_cmac_finish(&ctx, to_ptr(session_key))) != 0) {
		DEBUG_PRINTF("%d: cmac_finish failed\n", mbrc);
		return false;
	}

	if ((mbrc = mbedtls_ccm_setkey(&client->ccm_ctx, mbedtls_cipher_id_t::MBEDTLS_CIPHER_ID_AES, to_cptr(session_key),
	                               session_key.size() * 8)) != 0) {
		DEBUG_PRINTF("%d: ccm_setkey failed\n", mbrc);
		return false;
	}
	client->is_key_shared = true;
	return true;
}

void
OS2Handler::init_endec_iv(const std::array<std::byte, Sesame::TOKEN_SIZE>& local_tok,
                          const std::byte (&sesame_token)[Sesame::TOKEN_SIZE]) {
	// iv = count[5] + local_tok + sesame_token
	client->dec_iv = {};
	std::copy(std::cbegin(sesame_token), std::cend(sesame_token),
	          std::copy(local_tok.cbegin(), local_tok.cend(), &client->dec_iv[IV_COUNTER_SIZE]));
	dec_count = 0;

	std::copy(std::cbegin(client->dec_iv), std::cend(client->dec_iv), std::begin(client->enc_iv));
	enc_count = 0x8000000000;
	auto p = reinterpret_cast<const std::byte*>(&enc_count);
	std::copy(p, p + IV_COUNTER_SIZE, std::begin(client->enc_iv));
}

bool
OS2Handler::create_key_pair(api_wrapper<mbedtls_mpi>& sk, std::array<std::byte, 1 + c::PK_SIZE>& bin_pk) {
	api_wrapper<mbedtls_mpi> temp_sk{mbedtls_mpi_init, mbedtls_mpi_free};
	api_wrapper<mbedtls_ecp_point> point{mbedtls_ecp_point_init, mbedtls_ecp_point_free};

	int mbrc;
	if ((mbrc = mbedtls_ecdh_gen_public(&ec_grp, &temp_sk, &point, mbedtls_ctr_drbg_random, &rng_ctx)) != 0) {
		DEBUG_PRINTF("%d: ecdh_gen_public failed\n", mbrc);
		return false;
	}
	size_t olen;
	if ((mbrc = mbedtls_ecp_point_write_binary(&ec_grp, &point, MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, to_ptr(bin_pk), bin_pk.size())) !=
	    0) {
		DEBUG_PRINTF("%d: ecp_point_write_binary failed\n", mbrc);
		return false;
	}
	if (olen != bin_pk.size()) {
		DEBUG_PRINTLN("write_binary pk length not match");
		return false;
	}
	if ((mbrc = mbedtls_mpi_copy(&sk, &temp_sk)) != 0) {
		DEBUG_PRINTF("%d: mpi_copy failed\n", mbrc);
		return false;
	}
	return true;
}

float
OS2Handler::battery_voltage(Sesame::model_t model, int16_t battery) {
	switch (model) {
		case Sesame::model_t::sesame_3:
		case Sesame::model_t::sesame_4:
			return battery * 7.2f / 1023;
		case Sesame::model_t::sesame_bike:
		case Sesame::model_t::sesame_bot:
			return battery * 3.6f / 1023;
		default:
			return 0.0f;
	}
}

bool
OS2Handler::ecdh(const api_wrapper<mbedtls_mpi>& sk, std::array<std::byte, SK_SIZE>& out) {
	api_wrapper<mbedtls_mpi> shared_secret(mbedtls_mpi_init, mbedtls_mpi_free);
	int mbrc;
	if ((mbrc = mbedtls_ecdh_compute_shared(&ec_grp, &shared_secret, &sesame_pk, &sk, mbedtls_ctr_drbg_random, &rng_ctx)) != 0) {
		DEBUG_PRINTF("%d: ecdh_compute_shared failed\n", mbrc);
		return false;
	}
	if ((mbrc = mbedtls_mpi_write_binary(&shared_secret, to_ptr(out), out.size())) != 0) {
		DEBUG_PRINTF("%d: mpi_write_binary failed\n", mbrc);
		return false;
	}
	return true;
}

bool
OS2Handler::generate_tag_response(const std::array<std::byte, 1 + c::PK_SIZE>& bpk,
                                  const std::array<std::byte, Sesame::TOKEN_SIZE>& local_tok,
                                  const std::byte (&sesame_token)[4],
                                  std::array<std::byte, AES_BLOCK_SIZE>& tag_response) {
	api_wrapper<mbedtls_cipher_context_t> ctx{mbedtls_cipher_init, mbedtls_cipher_free};
	int mbrc;
	if ((mbrc = mbedtls_cipher_setup(&ctx, mbedtls_cipher_info_from_type(mbedtls_cipher_type_t::MBEDTLS_CIPHER_AES_128_ECB))) != 0) {
		DEBUG_PRINTF("%d: cipher_setup failed\n", mbrc);
		return false;
	}
	if ((mbrc = mbedtls_cipher_cmac_starts(&ctx, to_cptr(sesame_secret), sesame_secret.size() * 8)) != 0) {
		DEBUG_PRINTF("%d: cmac_start failed\n", mbrc);
		return false;
	}
	if ((mbrc = mbedtls_cipher_cmac_update(&ctx, to_cptr(sesame_ki), sesame_ki.size())) != 0 ||
	    (mbrc = mbedtls_cipher_cmac_update(&ctx, to_cptr(bpk) + 1, bpk.size() - 1)) != 0 ||
	    (mbrc = mbedtls_cipher_cmac_update(&ctx, to_cptr(local_tok), local_tok.size())) != 0 ||
	    (mbrc = mbedtls_cipher_cmac_update(&ctx, to_cptr(sesame_token), std::size(sesame_token))) != 0) {
		DEBUG_PRINTF("%d: cmac_update failed\n", mbrc);
		return false;
	}

	if ((mbrc = mbedtls_cipher_cmac_finish(&ctx, to_ptr(tag_response))) != 0) {
		DEBUG_PRINTF("%d: cmac_finish failed\n", mbrc);
		return false;
	}
	return true;
}

}  // namespace libsesame3bt
