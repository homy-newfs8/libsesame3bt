#include "SesameClient.h"
#include <Arduino.h>
#include <mbedtls/cmac.h>
#include <mbedtls/ecdh.h>
#include <cinttypes>
#include "api_wrapper.h"
#include "os2.h"
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
constexpr size_t IV_COUNTER_SIZE = 5;

}  // namespace

using util::to_cptr;
using util::to_ptr;

SesameClient::SesameClient() {}

SesameClient::~SesameClient() {
	if (blec) {
		blec->setClientCallbacks(nullptr, true);
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
	// prevent disconnect callback loop
	blec->setClientCallbacks(nullptr, false);
	NimBLEDevice::deleteClient(blec);
	blec = nullptr;
	reset_session();
	update_state(state_t::idle);
}

bool
SesameClient::begin(const BLEAddress& address, Sesame::model_t model) {
	this->address = address;
	this->model = model;
	switch (model) {
		case Sesame::model_t::sesame_3:
		case Sesame::model_t::sesame_4:
		case Sesame::model_t::sesame_bike:
		case Sesame::model_t::sesame_bot:
			handler.emplace(std::in_place_type<OS2Handler>, this);
			break;
		case Sesame::model_t::sesame_5:
		case Sesame::model_t::sesame_5_pro:
			handler.emplace(std::in_place_type<OS3Handler>, this);
			break;
		default:
			DEBUG_PRINTF("%u: model not supported\n", static_cast<uint8_t>(model));
			return false;
	}
	if (!handler->init()) {
		handler.reset();
		return false;
	}
	return true;
}

bool
SesameClient::set_keys(const char* pk_str, const char* secret_str) {
	if (!handler) {
		DEBUG_PRINTLN("begin() not finished");
		return false;
	}
	return handler->set_keys(pk_str, secret_str);
}

bool
SesameClient::set_keys(const std::array<std::byte, PK_SIZE>& public_key, const std::array<std::byte, SECRET_SIZE>& secret_key) {
	if (!handler) {
		DEBUG_PRINTLN("begin() not finished");
		return false;
	}
	return handler->set_keys(public_key, secret_key);
}

bool
SesameClient::connect(int retry) {
	if (!is_key_set) {
		DEBUG_PRINTLN("Keys not set");
		return false;
	}
	if (!blec) {
		blec = NimBLEDevice::createClient();
		blec->setClientCallbacks(this, false);
	}
	blec->setConnectTimeout(connect_timeout);
	for (int t = 0; t < 100; t++) {
		if (blec->connect(address)) {
			break;
		}
		if (retry <= 0 || t >= retry) {
			DEBUG_PRINTF("BLE connect failed (retry=%d)\n", retry);
			return false;
		}
		delay(500);
	}
	auto srv = blec->getService(Sesame::SESAME3_SRV_UUID);
	if (srv && (tx = srv->getCharacteristic(TxUUID)) && (rx = srv->getCharacteristic(RxUUID))) {
		if (rx->subscribe(
		        true,
		        [this](NimBLERemoteCharacteristic* ch, uint8_t* data, size_t size, bool isNotify) {
			        notify_cb(ch, reinterpret_cast<std::byte*>(data), size, isNotify);
		        },
		        true)) {
			update_state(state_t::connected);
			return true;
		} else {
			DEBUG_PRINTLN("Failed to subscribe RX char");
		}
	} else {
		DEBUG_PRINTLN("The device does not have TX or RX chars");
	}
	disconnect();
	return false;
}

void
SesameClient::update_state(state_t new_state) {
	if (state.exchange(new_state) == new_state) {
		return;
	}
	if (state_callback) {
		state_callback(*this, new_state);
	}
}

bool
SesameClient::send_data(std::byte* pkt, size_t pkt_size, bool is_crypted) {
	std::array<std::byte, 1 + FRAGMENT_SIZE> fragment;  // 1 for header
	int pos = 0;
	for (size_t remain = pkt_size; remain > 0;) {
		fragment[0] = packet_header_t{
		    pos == 0,
		    remain > FRAGMENT_SIZE ? packet_kind_t::not_finished
		    : is_crypted           ? packet_kind_t::encrypted
		                           : packet_kind_t::plain,
		    std::byte{0}}.value;
		size_t ssz = std::min(remain, FRAGMENT_SIZE);
		std::copy(pkt + pos, pkt + pos + ssz, &fragment[1]);
		if (!tx->writeValue(to_cptr(fragment), ssz + 1, false)) {
			DEBUG_PRINTLN("Failed to send data to the device");
			return false;
		}
		pos += ssz;
		remain -= ssz;
	}
	return true;
}

bool
SesameClient::decrypt(const std::byte* in, size_t in_len, std::byte* out, size_t out_size) {
	if (in_len < TAG_SIZE || out_size < in_len - TAG_SIZE) {
		return false;
	}
	int mbrc;
	if ((mbrc = mbedtls_ccm_auth_decrypt(&ccm_ctx, in_len - TAG_SIZE, to_cptr(dec_iv), dec_iv.size(), to_cptr(auth_add_data),
	                                     auth_add_data.size(), to_cptr(in), to_ptr(out), to_cptr(&in[in_len - TAG_SIZE]),
	                                     TAG_SIZE)) != 0) {
		DEBUG_PRINTF("%d: auth_decrypt failed\n", mbrc);
		return false;
	}
	handler->update_dec_iv();
	return true;
}

bool
SesameClient::encrypt(const std::byte* in, size_t in_len, std::byte* out, size_t out_size) {
	if (out_size < in_len + TAG_SIZE) {
		return false;
	}
	int rc;
	if ((rc = mbedtls_ccm_encrypt_and_tag(&ccm_ctx, in_len, to_cptr(enc_iv), enc_iv.size(), to_cptr(auth_add_data),
	                                      auth_add_data.size(), to_cptr(in), to_ptr(out), to_ptr(&out[in_len]), TAG_SIZE)) != 0) {
		DEBUG_PRINTF("%d: encrypt_and_tag failed\n", rc);
	}
	handler->update_enc_iv();
	return true;
}

void
SesameClient::notify_cb(NimBLERemoteCharacteristic* ch, const std::byte* p, size_t len, bool is_notify) {
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
		if (h.kind == SesameClient::packet_kind_t::encrypted) {
			handler->update_dec_iv();
		}
		return;
	}
	if (recv_size + len - 1 > MAX_RECV) {
		DEBUG_PRINTLN("Received data too long, skipping");
		skipping = true;
		if (h.kind == SesameClient::packet_kind_t::encrypted) {
			handler->update_dec_iv();
		}
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
			DEBUG_PRINTLN("Encrypted message too short");
			return;
		}
		if (!is_key_shared) {
			DEBUG_PRINTLN("Encrypted message received before key sharing");
			return;
		}
		std::array<std::byte, MAX_RECV - TAG_SIZE> decrypted{};
		if (!decrypt(recv_buffer.data(), recv_size, &decrypted[0], recv_size - TAG_SIZE)) {
			return;
		}
		std::copy(decrypted.cbegin(), decrypted.cbegin() + recv_size - TAG_SIZE, &recv_buffer[0]);
		recv_size -= TAG_SIZE;
	} else if (h.kind != packet_kind_t::plain) {
		DEBUG_PRINTF("%u: Unexpected packet kind\n", static_cast<uint8_t>(h.kind));
		return;
	}
	if (recv_size < sizeof(Sesame::message_header_t)) {
		DEBUG_PRINTF("%u: Short notification, ignore\n", recv_size);
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
					DEBUG_PRINTF("%u: Unsupported item on publish\n", static_cast<uint8_t>(msg->item_code));
					break;
			}
			break;
		case Sesame::op_code_t::response:
			switch (msg->item_code) {
				case Sesame::item_code_t::login:
					handle_response_login();
					break;
				case Sesame::item_code_t::history:
					if (history_callback) {
						handler->handle_history(&recv_buffer[sizeof(Sesame::message_header_t)], recv_size - sizeof(Sesame::message_header_t));
					}
					break;
				default:
					DEBUG_PRINTF("%u: Unsupported item on response\n", static_cast<uint8_t>(msg->item_code));
					break;
			}
			break;
		default:
			DEBUG_PRINTF("%u: Unexpected op code\n", static_cast<uint8_t>(msg->op_code));
			break;
	}
}

void
SesameClient::handle_publish_initial() {
	handler->handle_publish_initial(&recv_buffer[sizeof(Sesame::message_header_t)], recv_size - sizeof(Sesame::message_header_t));
	return;
}

void
SesameClient::handle_response_login() {
	handler->handle_response_login(&recv_buffer[sizeof(Sesame::message_header_t)], recv_size - sizeof(Sesame::message_header_t));
	return;
}

bool
SesameClient::request_history() {
	std::byte flag{0};
	return handler->send_command(Sesame::op_code_t::read, Sesame::item_code_t::history, &flag, sizeof(flag), true);
}

void
SesameClient::fire_history_callback(const History& history) {
	if (history_callback) {
		history_callback(*this, history);
	}
}

bool
SesameClient::send_cmd_with_tag(Sesame::item_code_t code, const char* tag) {
	std::array<char, 1 + Handler::MAX_HISTORY_TAG_SIZE> tagchars{};
	if (tag) {
		tagchars[0] = util::truncate_utf8(tag, handler->get_max_history_tag_size());
		std::copy(tag, tag + tagchars[0], &tagchars[1]);
	}
	auto tagbytes = reinterpret_cast<std::byte*>(tagchars.data());
	return handler->send_command(Sesame::op_code_t::async, code, tagbytes, handler->get_cmd_tag_size(tagbytes), true);
}

bool
SesameClient::unlock(const char* tag) {
	if (!is_session_active()) {
		DEBUG_PRINTLN("Cannot operate while session is not active");
		return false;
	}
	return send_cmd_with_tag(Sesame::item_code_t::unlock, tag);
}

bool
SesameClient::lock(const char* tag) {
	if (model == Sesame::model_t::sesame_cycle) {
		DEBUG_PRINTLN("SESAME Cycle do not support locking");
		return false;
	}
	if (!is_session_active()) {
		DEBUG_PRINTLN("Cannot operate while session is not active");
		return false;
	}
	return send_cmd_with_tag(Sesame::item_code_t::lock, tag);
}

bool
SesameClient::click(const char* tag) {
	if (model != Sesame::model_t::sesame_bot) {
		DEBUG_PRINTLN("click is supported only on SESAME bot");
		return false;
	}
	if (!is_session_active()) {
		DEBUG_PRINTLN("Cannot operate while session is not active");
		return false;
	}
	return send_cmd_with_tag(Sesame::item_code_t::click, tag);
}

void
SesameClient::handle_publish_mecha_setting() {
	handler->handle_publish_mecha_setting(&recv_buffer[sizeof(Sesame::message_header_t)],
	                                      recv_size - sizeof(Sesame::message_header_t));
	return;
}

void
SesameClient::handle_publish_mecha_status() {
	handler->handle_publish_mecha_status(&recv_buffer[sizeof(Sesame::message_header_t)],
	                                     recv_size - sizeof(Sesame::message_header_t));
	return;
}

void
SesameClient::fire_status_callback() {
	if (lock_status_callback) {
		lock_status_callback(*this, sesame_status);
	}
}

void
SesameClient::set_status_callback(status_callback_t callback) {
	lock_status_callback = callback;
}

void
SesameClient::set_state_callback(state_callback_t callback) {
	state_callback = callback;
}

void
SesameClient::onDisconnect(NimBLEClient* pClient) {
	if (state.load() != state_t::idle) {
		DEBUG_PRINTLN("Bluetooth disconnected by peer");
		disconnect();
	}
}

float
SesameClient::Status::voltage_to_pct(float voltage) {
	if (voltage >= batt_tbl[0].voltage) {
		return batt_tbl[0].pct;
	}
	if (voltage <= batt_tbl[std::size(batt_tbl) - 1].voltage) {
		return batt_tbl[std::size(batt_tbl) - 1].pct;
	}
	for (auto i = 1; i < std::size(batt_tbl); i++) {
		if (voltage >= batt_tbl[i].voltage) {
			return (voltage - batt_tbl[i].voltage) / (batt_tbl[i - 1].voltage - batt_tbl[i].voltage) *
			           (batt_tbl[i - 1].pct - batt_tbl[i].pct) +
			       batt_tbl[i].pct;
		}
	}
	return 0.0f;  // Never reach
}

}  // namespace libsesame3bt
