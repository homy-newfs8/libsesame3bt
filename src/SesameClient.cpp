#include "SesameClient.h"
#include <Arduino.h>
#include <mbedtls/cmac.h>
#include <mbedtls/ecdh.h>
#include <cinttypes>
#include "api_wrapper.h"
#include "util.h"

#ifndef LIBSESAME3BT_DEBUG
#define LIBSESAME3BT_DEBUG 0
#endif
#include "debug.h"

namespace libsesame3bt {
namespace {

constexpr size_t TAG_SIZE = 4;
constexpr size_t AES_KEY_SIZE = 16;
constexpr size_t FRAGMENT_SIZE = 19;
constexpr size_t AUTH_TAG_TRUNCATED_SIZE = 4;
constexpr size_t KEY_INDEX_SIZE = 2;
constexpr size_t ADD_DATA_SIZE = 1;
constexpr size_t TOKEN_SIZE = Sesame::TOKEN_SIZE;
constexpr size_t MAX_CMD_TAG_SIZE = 21;
constexpr size_t IV_COUNTER_SIZE = 5;

}  // namespace

bool SesameClient::static_initialized = [] {
	if (mbedtls_ctr_drbg_seed(&rng_ctx, mbedtls_entropy_func, &ent_ctx, nullptr, 0) != 0) {
		DEBUG_PRINTLN(F("drbg_seed failed"));
		return false;
	}
	if (mbedtls_ecp_group_load(&ec_grp, mbedtls_ecp_group_id::MBEDTLS_ECP_DP_SECP256R1) != 0) {
		DEBUG_PRINTLN(F("ecp_group_load failed"));
		return false;
	}
	return true;
}();

SesameClient::SesameClient() {}

SesameClient::~SesameClient() {
	if (blec) {
		NimBLEDevice::deleteClient(blec);
	}
}

void
SesameClient::reset_session() {
	ccm_ctx.reset();
	is_key_shared = false;
}

void
SesameClient::disconnect() {
	if (!blec) {
		return;
	}
	blec->disconnect();
	reset_session();
	update_state(state_t::idle);
}

bool
SesameClient::create_key_pair(api_wrapper<mbedtls_mpi>& sk, std::array<uint8_t, 1 + PK_SIZE>& bin_pk) {
	api_wrapper<mbedtls_mpi> temp_sk{mbedtls_mpi_init, mbedtls_mpi_free};
	api_wrapper<mbedtls_ecp_point> point{mbedtls_ecp_point_init, mbedtls_ecp_point_free};

	int mbrc;
	if ((mbrc = mbedtls_ecdh_gen_public(&ec_grp, &temp_sk, &point, mbedtls_ctr_drbg_random, &rng_ctx)) != 0) {
		DEBUG_PRINTFP(PSTR("%d: ecdh_gen_public failed\n"), mbrc);
		return false;
	}
	size_t olen;
	if ((mbrc = mbedtls_ecp_point_write_binary(&ec_grp, &point, MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, bin_pk.data(), bin_pk.size())) !=
	    0) {
		DEBUG_PRINTFP(PSTR("%d: ecp_point_write_binary failed\n"), mbrc);
		return false;
	}
	if (olen != bin_pk.size()) {
		DEBUG_PRINTLN(F("write_binary pk length not match"));
		return false;
	}
	if ((mbrc = mbedtls_mpi_copy(&sk, &temp_sk)) != 0) {
		DEBUG_PRINTFP(PSTR("%d: mpi_copy failed\n"), mbrc);
		return false;
	}
	return true;
}

bool
SesameClient::begin(const BLEAddress& address, Sesame::model_t model) {
	if (model != Sesame::model_t::sesame_3 && model != Sesame::model_t::sesame_4 && model != Sesame::model_t::sesame_cycle &&
	    model != Sesame::model_t::sesame_bot) {
		return false;
	}
	if (!static_initialized) {
		return false;
	}
	this->address = address;
	this->model = model;

	return true;
}

bool
SesameClient::set_keys(const char* pk_str, const char* secret_str) {
	if (!pk_str || !secret_str) {
		DEBUG_PRINTLN("pk_str or secret_str is nullptr");
		return false;
	}
	std::array<uint8_t, PK_SIZE> pk;
	if (!util::hex2bin(pk_str, pk)) {
		DEBUG_PRINTLN("pk_str invalid format");
		return false;
	}
	std::array<uint8_t, SECRET_SIZE> secret;
	if (!util::hex2bin(secret_str, secret)) {
		DEBUG_PRINTLN("secret_str invalid format");
		return false;
	}
	return set_keys(pk, secret);
}

bool
SesameClient::set_keys(const std::array<uint8_t, PK_SIZE>& public_key, const std::array<uint8_t, SECRET_SIZE>& secret_key) {
	std::array<uint8_t, 1 + PK_SIZE> bin_pk;  // 1 for indicator (SEC1 2.3.4)
	bin_pk[0] = 4;                            // uncompressed point indicator
	std::copy(util::cbegin(public_key), util::cend(public_key), &bin_pk[1]);
	int mbrc;
	if ((mbrc = mbedtls_ecp_point_read_binary(&ec_grp, &sesame_pk, bin_pk.data(), bin_pk.size())) != 0) {
		DEBUG_PRINTFP(PSTR("%d: ecp_point_read_binary failed"), mbrc);
		return false;
	}
	if ((mbrc = mbedtls_ecp_check_pubkey(&ec_grp, &sesame_pk)) != 0) {
		DEBUG_PRINTF(PSTR("%d: ecp_check_pubkey failed"), mbrc);
		return false;
	}
	std::copy(util::cbegin(secret_key), util::cend(secret_key), sesame_secret.begin());
	is_key_set = true;

	return true;
}

bool
SesameClient::connect(int retry) {
	if (!is_key_set) {
		return false;
	}
	if (!blec) {
		blec = NimBLEDevice::createClient();
		blec->setClientCallbacks(this, false);
	}
	for (int t = 0; t < 100; t++) {
		if (blec->connect(address)) {
			break;
		}
		if (retry <= 0 || t >= retry) {
			DEBUG_PRINTFP(PSTR("BLE connect failed (retry=%d)\n"), retry);
			return false;
		}
		delay(500);
	}
	auto srv = blec->getService(Sesame::SESAME3_SRV_UUID);
	if (srv && (tx = srv->getCharacteristic(TxUUID)) && (rx = srv->getCharacteristic(RxUUID))) {
		if (rx->subscribe(true, [this](NimBLERemoteCharacteristic* ch, uint8_t* data, size_t size, bool isNotify) {
			    notify_cb(ch, data, size, isNotify);
		    })) {
			update_state(state_t::connected);
			return true;
		} else {
			DEBUG_PRINTLN(F("Failed to subscribe RX char"));
		}
	} else {
		DEBUG_PRINTLN(F("The device does not have TX or RX chars"));
	}
	disconnect();
	return false;
}

void
SesameClient::update_state(state_t new_state) {
	if (state == new_state) {
		return;
	}
	state = new_state;
	if (state_callback) {
		state_callback(*this, new_state);
	}
}

bool
SesameClient::send_command(Sesame::op_code_t op_code,
                           Sesame::item_code_t item_code,
                           const uint8_t* data,
                           size_t data_size,
                           bool is_crypted) {
	const size_t pkt_size = 2 + data_size + (is_crypted ? TAG_SIZE : 0);  // 2 for op/item, 4 for encrypted tag
	uint8_t pkt[pkt_size];
	if (is_crypted) {
		uint8_t plain[2 + data_size];
		plain[0] = static_cast<uint8_t>(op_code);
		plain[1] = static_cast<uint8_t>(item_code);
		std::copy(data, data + data_size, &plain[2]);
		if (!encrypt(plain, sizeof(plain), pkt, sizeof(pkt))) {
			return false;
		}
	} else {
		pkt[0] = static_cast<uint8_t>(op_code);
		pkt[1] = static_cast<uint8_t>(item_code);
		std::copy(data, data + data_size, &pkt[2]);
	}

	std::array<uint8_t, 1 + FRAGMENT_SIZE> fragment;  // 1 for header
	int pos = 0;
	for (size_t remain = pkt_size; remain > 0;) {
		fragment[0] = packet_header_t{pos == 0, remain > FRAGMENT_SIZE ? packet_kind_t::not_finished
		                                        : is_crypted           ? packet_kind_t::encrypted
		                                                               : packet_kind_t::plain}
		                  .value;
		size_t ssz = std::min(remain, FRAGMENT_SIZE);
		std::copy(pkt + pos, pkt + pos + ssz, &fragment[1]);
		if (!tx->writeValue(fragment.data(), ssz + 1, false)) {
			DEBUG_PRINTLN(F("Failed to send data to the device"));
			return false;
		}
		pos += ssz;
		remain -= ssz;
	}
	return true;
}

bool
SesameClient::decrypt(const uint8_t* in, size_t in_len, uint8_t* out, size_t out_size) {
	if (in_len < TAG_SIZE || out_size < in_len - TAG_SIZE) {
		return false;
	}
	int mbrc;
	if ((mbrc = mbedtls_ccm_auth_decrypt(&ccm_ctx, in_len - TAG_SIZE, dec_iv.data(), dec_iv.size(), auth_add_data.data(),
	                                     auth_add_data.size(), in, out, &in[in_len - TAG_SIZE], TAG_SIZE)) != 0) {
		DEBUG_PRINTFP(PSTR("%d: auth_decrypt failed\n"), mbrc);
		return false;
	}
	dec_count++;
	dec_count &= 0x7fffffffffLL;
	auto p = reinterpret_cast<const uint8_t*>(&dec_count);
	std::copy(p, p + IV_COUNTER_SIZE, dec_iv.begin());
	return true;
}

bool
SesameClient::encrypt(const uint8_t* in, size_t in_len, uint8_t* out, size_t out_size) {
	if (out_size < in_len + TAG_SIZE) {
		return false;
	}
	int rc;
	if ((rc = mbedtls_ccm_encrypt_and_tag(&ccm_ctx, in_len, enc_iv.data(), enc_iv.size(), auth_add_data.data(), auth_add_data.size(),
	                                      in, out, &out[in_len], TAG_SIZE)) != 0) {
		DEBUG_PRINTFP(PSTR("%d: encrypt_and_tag failed\n"), rc);
	}
	enc_count++;
	enc_count &= 0x7fffffffffLL;
	enc_count |= 0x8000000000LL;
	auto p = reinterpret_cast<const uint8_t*>(&enc_count);
	std::copy(p, p + IV_COUNTER_SIZE, enc_iv.begin());
	return true;
}

void
SesameClient::notify_cb(NimBLERemoteCharacteristic* ch, const uint8_t* p, size_t len, bool is_notify) {
	if (!is_notify || len <= 1) {
		return;
	}
	packet_header_t h;
	h.value = p[0];
	if (h.is_start) {
		skipping = false;
		recv_size = 0;
	}
	if (skipping) {
		return;
	}
	if (recv_size + len - 1 > MAX_RECV) {
		DEBUG_PRINTLN(F("Received data too long, skipping"));
		skipping = true;
		return;
	}
	std::copy(p + 1, p + len, &recv_buffer[recv_size]);
	recv_size += len - 1;
	if (h.kind == packet_kind_t::not_finished) {
		// wait next packet
		return;
	}
	skipping = true;
	if (h.kind == packet_kind_t::encrypted) {
		if (recv_size < TAG_SIZE) {
			DEBUG_PRINTLN(F("Encrypted message too short"));
			return;
		}
		if (!is_key_shared) {
			DEBUG_PRINTLN(F("Encrypted message received before key sharing"));
			return;
		}
		std::array<uint8_t, MAX_RECV - TAG_SIZE> decrypted{};
		decrypt(recv_buffer.data(), recv_size, &decrypted[0], recv_size - TAG_SIZE);
		std::copy(decrypted.cbegin(), decrypted.cbegin() + recv_size - TAG_SIZE, &recv_buffer[0]);
		recv_size -= TAG_SIZE;
	} else if (h.kind != packet_kind_t::plain) {
		DEBUG_PRINTFP(PSTR("%u: Unexpected packet kind\n"), static_cast<uint8_t>(h.kind));
		return;
	}
	if (recv_size < sizeof(Sesame::message_header_t)) {
		DEBUG_PRINTFP(PSTR("%u: Short notification, ignore\n"), recv_size);
		return;
	}
	auto msg = reinterpret_cast<const Sesame::message_header_t*>(recv_buffer.data());
	switch (msg->op_code) {
		case Sesame::op_code_t::publish:
			switch (msg->item_code) {
				case Sesame::item_code_t::initial:
					handle_publish_initial();
					break;
				case Sesame::item_code_t::mech_setting:
					handle_publish_mecha_setting();
					break;
				case Sesame::item_code_t::mech_status:
					handle_publish_mecha_status();
					break;
				default:
					DEBUG_PRINTFP(PSTR("%u: Unsupported item on publish\n"), static_cast<uint8_t>(msg->item_code));
					break;
			}
			break;
		case Sesame::op_code_t::response:
			switch (msg->item_code) {
				case Sesame::item_code_t::login:
					handle_response_login();
					break;
				default:
					DEBUG_PRINTFP(PSTR("%u: Unsupported item on response\n"), static_cast<uint8_t>(msg->item_code));
					break;
			}
			break;
		default:
			DEBUG_PRINTFP(PSTR("%u: Unexpected op code\n"), static_cast<uint8_t>(msg->op_code));
			break;
	}
}

bool
SesameClient::generate_tag_response(const std::array<uint8_t, 1 + PK_SIZE>& bpk,
                                    const std::array<uint8_t, TOKEN_SIZE>& local_tok,
                                    const uint8_t (&sesame_token)[4],
                                    std::array<uint8_t, AES_BLOCK_SIZE>& tag_response) {
	api_wrapper<mbedtls_cipher_context_t> ctx{mbedtls_cipher_init, mbedtls_cipher_free};
	int mbrc;
	if ((mbrc = mbedtls_cipher_setup(&ctx, mbedtls_cipher_info_from_type(mbedtls_cipher_type_t::MBEDTLS_CIPHER_AES_128_ECB))) != 0) {
		DEBUG_PRINTFP(PSTR("%d: cipher_setup failed\n"), mbrc);
		return false;
	}
	if ((mbrc = mbedtls_cipher_cmac_starts(&ctx, sesame_secret.data(), sesame_secret.size() * 8)) != 0) {
		DEBUG_PRINTFP(PSTR("%d: cmac_start failed\n"), mbrc);
		return false;
	}
	if ((mbrc = mbedtls_cipher_cmac_update(&ctx, sesame_ki.data(), sesame_ki.size())) != 0 ||
	    (mbrc = mbedtls_cipher_cmac_update(&ctx, bpk.data() + 1, bpk.size() - 1)) != 0 ||
	    (mbrc = mbedtls_cipher_cmac_update(&ctx, local_tok.data(), local_tok.size())) != 0 ||
	    (mbrc = mbedtls_cipher_cmac_update(&ctx, sesame_token, std::size(sesame_token))) != 0) {
		DEBUG_PRINTFP(PSTR("%d: cmac_update failed\n"), mbrc);
		return false;
	}

	if ((mbrc = mbedtls_cipher_cmac_finish(&ctx, tag_response.data())) != 0) {
		DEBUG_PRINTFP(PSTR("%d: cmac_finish failed\n"), mbrc);
		return false;
	}
	return true;
}

bool
SesameClient::generate_session_key(const std::array<uint8_t, TOKEN_SIZE>& local_tok,
                                   const uint8_t (&sesame_token)[TOKEN_SIZE],
                                   std::array<uint8_t, 1 + PK_SIZE>& pk) {
	api_wrapper<mbedtls_cipher_context_t> ctx{mbedtls_cipher_init, mbedtls_cipher_free};
	api_wrapper<mbedtls_mpi> sk{mbedtls_mpi_init, mbedtls_mpi_free};

	bool rc = false;
	if (!create_key_pair(sk, pk)) {
		return false;
	}
	std::array<uint8_t, SK_SIZE> ssec;
	if (!ecdh(sk, ssec)) {
		return false;
	}

	int mbrc;
	if ((mbrc = mbedtls_cipher_setup(&ctx, mbedtls_cipher_info_from_type(mbedtls_cipher_type_t::MBEDTLS_CIPHER_AES_128_ECB))) != 0) {
		DEBUG_PRINTFP(PSTR("%d: cipher setup failed\n"), mbrc);
		return false;
	}
	if ((mbrc = mbedtls_cipher_cmac_starts(&ctx, ssec.data(), AES_KEY_SIZE * 8)) != 0) {
		DEBUG_PRINTFP(PSTR("%d: cmac start failed\n"), mbrc);
		return false;
	}
	if ((mbrc = mbedtls_cipher_cmac_update(&ctx, local_tok.data(), local_tok.size())) != 0 ||
	    (mbrc = mbedtls_cipher_cmac_update(&ctx, sesame_token, std::size(sesame_token))) != 0) {
		DEBUG_PRINTFP(PSTR("%d: cmac_update failed\n"), mbrc);
		return false;
	}
	std::array<uint8_t, 16> session_key;
	if ((mbrc = mbedtls_cipher_cmac_finish(&ctx, session_key.data())) != 0) {
		DEBUG_PRINTF(PSTR("%d: cmac_finish failed\n"), mbrc);
		return false;
	}

	if ((mbrc = mbedtls_ccm_setkey(&ccm_ctx, mbedtls_cipher_id_t::MBEDTLS_CIPHER_ID_AES, session_key.data(),
	                               session_key.size() * 8)) != 0) {
		DEBUG_PRINTFP(PSTR("%d: ccm_setkey failed\n"), mbrc);
		return false;
	}
	is_key_shared = true;
	return true;
}

void
SesameClient::init_endec_iv(const std::array<uint8_t, TOKEN_SIZE>& local_tok, const uint8_t (&sesame_token)[TOKEN_SIZE]) {
	// iv = count[5] + local_tok + sesame_token
	dec_iv = {};
	std::copy(util::cbegin(sesame_token), util::cend(sesame_token),
	          std::copy(local_tok.cbegin(), local_tok.cend(), &dec_iv[IV_COUNTER_SIZE]));
	dec_count = 0;

	std::copy(dec_iv.cbegin(), dec_iv.cend(), enc_iv.begin());
	enc_count = 0x8000000000;
	auto p = reinterpret_cast<const uint8_t*>(&enc_count);
	std::copy(p, p + IV_COUNTER_SIZE, enc_iv.begin());
}

void
SesameClient::handle_publish_initial() {
	if (recv_size < sizeof(Sesame::message_header_t) + sizeof(Sesame::publish_initial_t)) {
		DEBUG_PRINTF("%u: short response initial data\n", recv_size);
		disconnect();
		return;
	}
	reset_session();
	auto msg = reinterpret_cast<const Sesame::publish_initial_t*>(&recv_buffer[sizeof(Sesame::message_header_t)]);

	std::array<uint8_t, TOKEN_SIZE> local_tok;
	int mbrc;
	if ((mbrc = mbedtls_ctr_drbg_random(&rng_ctx, local_tok.data(), local_tok.size())) != 0) {
		DEBUG_PRINTFP(PSTR("%d: drbg_random failed\n"), mbrc);
		disconnect();
		return;
	}

	std::array<uint8_t, 1 + PK_SIZE> bpk;
	if (!generate_session_key(local_tok, msg->token, bpk)) {
		disconnect();
		return;
	}
	init_endec_iv(local_tok, msg->token);
	std::array<uint8_t, AES_BLOCK_SIZE> tag_response;
	if (!generate_tag_response(bpk, local_tok, msg->token, tag_response)) {
		disconnect();
		return;
	}

	constexpr size_t resp_size = sesame_ki.size() + PK_SIZE + local_tok.size() + AUTH_TAG_TRUNCATED_SIZE;
	std::array<uint8_t, resp_size> resp;

	// resp = sesame_ki + pk + local_tok + tag_response[:4]
	std::copy(tag_response.cbegin(), tag_response.cbegin() + AUTH_TAG_TRUNCATED_SIZE,
	          std::copy(local_tok.cbegin(), local_tok.cend(),
	                    std::copy(bpk.begin() + 1, bpk.end(), std::copy(sesame_ki.cbegin(), sesame_ki.cend(), resp.begin()))));

	if (send_command(Sesame::op_code_t::sync, Sesame::item_code_t::login, resp.data(), resp.size(), false)) {
		update_state(state_t::authenticating);
	} else {
		disconnect();
	}
}

bool
SesameClient::ecdh(const api_wrapper<mbedtls_mpi>& sk, std::array<uint8_t, SK_SIZE>& out) {
	api_wrapper<mbedtls_mpi> shared_secret(mbedtls_mpi_init, mbedtls_mpi_free);
	int mbrc;
	if ((mbrc = mbedtls_ecdh_compute_shared(&ec_grp, &shared_secret, &sesame_pk, &sk, mbedtls_ctr_drbg_random, &rng_ctx)) != 0) {
		DEBUG_PRINTFP(PSTR("%d: ecdh_compute_shared failed\n"), mbrc);
		return false;
	}
	if ((mbrc = mbedtls_mpi_write_binary(&shared_secret, &out[0], out.size())) != 0) {
		DEBUG_PRINTFP(PSTR("%d: mpi_write_binary failed\n"), mbrc);
		return false;
	}
	return true;
}

void
SesameClient::update_mecha_setting(const Sesame::mecha_setting_t& setting) {
	this->mecha_setting = setting;
}

void
SesameClient::update_mecha_status(const Sesame::mecha_status_t& status) {
	this->mecha_status = status;
}

void
SesameClient::handle_response_login() {
	DEBUG_PRINTLN(F("debug:login"));
	if (recv_size < sizeof(Sesame::message_header_t) + sizeof(Sesame::response_login_t)) {
		DEBUG_PRINTLN(F("short response login message"));
		disconnect();
		return;
	}
	auto msg = reinterpret_cast<const Sesame::response_login_t*>(&recv_buffer[sizeof(Sesame::message_header_t)]);
	if (msg->result != Sesame::result_code_t::success) {
		DEBUG_PRINTFP(PSTR("%u: login response was not success\n"), static_cast<uint8_t>(msg->result));
		disconnect();
		return;
	}
	update_state(state_t::active);
	update_mecha_setting(msg->mecha_setting);
	update_mecha_status(msg->mecha_status);
	fire_status_callback();
}

bool
SesameClient::unlock(const char* tag) {
	if (!is_session_active()) {
		DEBUG_PRINTLN(F("Cannot operate while session is not active"));
		return false;
	}
	std::array<char, 1 + MAX_CMD_TAG_SIZE> tagbytes{};
	tagbytes[0] = util::truncate_utf8(tag, MAX_CMD_TAG_SIZE);
	std::copy(tag, tag + tagbytes[0], &tagbytes[1]);
	return send_command(Sesame::op_code_t::async, Sesame::item_code_t::unlock, reinterpret_cast<const uint8_t*>(tagbytes.data()),
	                    tagbytes.size(), true);
}

bool
SesameClient::lock(const char* tag) {
	if (model == Sesame::model_t::sesame_cycle) {
		DEBUG_PRINTLN(F("SESAME Cycle do not support locking"));
		return false;
	}
	if (!is_session_active()) {
		DEBUG_PRINTLN(F("Cannot operate while session is not active"));
		return false;
	}
	std::array<char, 1 + MAX_CMD_TAG_SIZE> tagbytes{};
	tagbytes[0] = util::truncate_utf8(tag, MAX_CMD_TAG_SIZE);
	std::copy(tag, tag + tagbytes[0], &tagbytes[1]);
	return send_command(Sesame::op_code_t::async, Sesame::item_code_t::lock, reinterpret_cast<const uint8_t*>(tagbytes.data()),
	                    tagbytes.size(), true);
}

bool
SesameClient::click(const char* tag) {
	if (model != Sesame::model_t::sesame_bot) {
		DEBUG_PRINTLN(F("click is supported only on SESAME bot"));
		return false;
	}
	if (!is_session_active()) {
		DEBUG_PRINTLN(F("Cannot operate while session is not active"));
		return false;
	}
	std::array<char, 1 + MAX_CMD_TAG_SIZE> tagbytes{};
	tagbytes[0] = util::truncate_utf8(tag, MAX_CMD_TAG_SIZE);
	std::copy(tag, tag + tagbytes[0], &tagbytes[1]);
	return send_command(Sesame::op_code_t::async, Sesame::item_code_t::click, reinterpret_cast<const uint8_t*>(tagbytes.data()),
	                    tagbytes.size(), true);
}

void
SesameClient::handle_publish_mecha_setting() {
	if (recv_size < sizeof(Sesame::message_header_t) + sizeof(Sesame::publish_mecha_setting_t)) {
		DEBUG_PRINTFP(PSTR("%u: Unexpected size of mecha setting, ignored\n"), recv_size);
		return;
	}
	auto msg = reinterpret_cast<const Sesame::publish_mecha_setting_t*>(&recv_buffer[sizeof(Sesame::message_header_t)]);
	update_mecha_setting(msg->setting);
	fire_status_callback();
}

void
SesameClient::handle_publish_mecha_status() {
	if (recv_size < sizeof(Sesame::message_header_t) + sizeof(Sesame::publish_mecha_status_t)) {
		DEBUG_PRINTFP(PSTR("%u: Unexpected size of mecha status, ignored\n"), recv_size);
		return;
	}
	auto msg = reinterpret_cast<const Sesame::publish_mecha_status_t*>(&recv_buffer[sizeof(Sesame::message_header_t)]);
	update_mecha_status(msg->status);
	fire_status_callback();
}

void
SesameClient::fire_status_callback() {
	if (model == Sesame::model_t::sesame_bot) {
		if (bot_status_callback) {
			bot_status_callback(*this, BotStatus(mecha_setting, mecha_status));
		}
	} else {
		if (lock_status_callback) {
			lock_status_callback(*this, Status(mecha_setting, mecha_status, model));
		}
	}
}

void
SesameClient::set_status_callback(status_callback_t callback) {
	if (model == Sesame::model_t::sesame_bot) {
		DEBUG_PRINTLN(F("Do not use this method for SESAME bot"));
		return;
	}
	lock_status_callback = callback;
}

void
SesameClient::set_bot_status_callback(bot_status_callback_t callback) {
	if (model != Sesame::model_t::sesame_bot) {
		DEBUG_PRINTLN(F("Use this method only for SESAME bot"));
		return;
	}
	bot_status_callback = callback;
}

void
SesameClient::set_state_callback(state_callback_t callback) {
	state_callback = callback;
}

void
SesameClient::onDisconnect(NimBLEClient* pClient) {
	if (state != state_t::idle) {
		DEBUG_PRINTLN(F("Bluetooth disconnected by peer"));
		disconnect();
	}
}

}  // namespace libsesame3bt
