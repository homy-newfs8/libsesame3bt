#include "SesameClient.h"
#include <Arduino.h>
#include <cinttypes>

#ifndef LIBSESAME3BT_DEBUG
#define LIBSESAME3BT_DEBUG 0
#endif
#include "debug.h"

namespace libsesame3bt {

using SesameClientCore = core::SesameClientCore;

SesameClient::SesameClient() : SesameClientCore(static_cast<SesameClientBackend&>(*this)) {
	SesameClientCore::set_state_callback([this](SesameClientCore& client, state_t state) { _state_callback(client, state); });
	SesameClientCore::set_status_callback([this](SesameClientCore& client, Status status) { _status_callback(client, status); });
	SesameClientCore::set_history_callback(
	    [this](SesameClientCore& client, const History& history) { _history_callback(client, history); });
}

SesameClient::~SesameClient() {
	if (blec) {
		blec->setClientCallbacks(nullptr, true);
		NimBLEDevice::deleteClient(blec);
	}
}

bool
SesameClient::write_to_tx(const uint8_t* data, size_t size) {
	if (!blec || !tx) {
		DEBUG_PRINTLN("ble or tx not initialized");
		return false;
	}
	return tx->writeValue(data, size, false);
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
}

bool
SesameClient::begin(const BLEAddress& address, Sesame::model_t model) {
	this->address = address;
	return SesameClientCore::begin(model);
}

bool
SesameClient::connect(int retry) {
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
	if (srv && (tx = srv->getCharacteristic(Sesame::TxUUID)) && (rx = srv->getCharacteristic(Sesame::RxUUID))) {
		if (rx->subscribe(
		        true,
		        [this](NimBLERemoteCharacteristic* ch, uint8_t* data, size_t size, bool isNotify) {
			        ble_notify_callback(ch, reinterpret_cast<std::byte*>(data), size, isNotify);
		        },
		        true)) {
			on_connected();
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
SesameClient::_status_callback(SesameClientCore& client, SesameClient::Status status) {
	if (status_callback) {
		status_callback(*this, status);
	}
}
void
SesameClient::_state_callback(SesameClientCore& client, SesameClient::state_t state) {
	if (state_callback) {
		state_callback(*this, state);
	}
}
void
SesameClient::_history_callback(SesameClientCore& client, const SesameClient::History& history) {
	if (history_callback) {
		history_callback(*this, history);
	}
}

void
SesameClient::ble_notify_callback(NimBLERemoteCharacteristic* ch, const std::byte* p, size_t len, bool is_notify) {
	if (!is_notify || len <= 1) {
		return;
	}
	on_received(p, len);
}

void
SesameClient::onDisconnect(NimBLEClient* pClient) {
	DEBUG_PRINTLN("Bluetooth disconnected by peer");
	disconnect();
	on_disconnected();
}

}  // namespace libsesame3bt
