#include "SesameClient.h"
#include <Arduino.h>
#include <cinttypes>

#ifndef LIBSESAME3BT_DEBUG
#define LIBSESAME3BT_DEBUG 0
#endif
#include "debug.h"

namespace libsesame3bt {

using SesameClientCore = core::SesameClientCore;

SesameClient::SesameClient() : SesameClientCore(static_cast<SesameBLEBackend&>(*this)) {
	SesameClientCore::set_state_callback([this](auto& core, auto state) { core_state_callback(core, state); });
	SesameClientCore::set_status_callback([this](auto&, Status status) {
		if (status_callback)
			status_callback(*this, status);
	});
	SesameClientCore::set_history_callback([this](auto&, const History& history) {
		if (history_callback)
			history_callback(*this, history);
	});
	SesameClientCore::set_registered_devices_callback([this](auto&, const auto& devs) {
		if (registered_devices_callback) {
			registered_devices_callback(*this, devs);
		}
	});
}

SesameClient::~SesameClient() {
	if (blec) {
		blec->setClientCallbacks(nullptr, true);
		NimBLEDevice::deleteClient(blec);
	}
}

void
SesameClient::core_state_callback(core::SesameClientCore& core, core::state_t state) {
	switch (state) {
		case core::state_t::idle:
			set_state(state_t::idle);
			break;
		case core::state_t::authenticating:
			set_state(state_t::authenticating);
			break;
		case core::state_t::active:
			set_state(state_t::active);
			break;
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
	on_disconnected();
}

bool
SesameClient::begin(const NimBLEAddress& address, Sesame::model_t model) {
	this->address = address;
	return SesameClientCore::begin(model);
}

void
SesameClient::set_state(state_t state) {
	if (state == this->state) {
		return;
	}
	this->state = state;
	if (state_callback) {
		state_callback(*this, this->state);
	}
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
	set_state(state_t::connected);
	auto srv = blec->getService(Sesame::SESAME3_SRV_UUID);
	if (srv && (tx = srv->getCharacteristic(Sesame::TxUUID)) && (rx = srv->getCharacteristic(Sesame::RxUUID))) {
		if (rx->subscribe(
		        true,
		        [this](NimBLERemoteCharacteristic* ch, uint8_t* data, size_t size, bool isNotify) {
			        if (!isNotify || size <= 1)
				        return;
			        on_received(reinterpret_cast<std::byte*>(data), size);
		        },
		        true)) {
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
SesameClient::onDisconnect(NimBLEClient* pClient, int reason) {
	DEBUG_PRINTLN("Bluetooth disconnected by peer");
	disconnect();
}

}  // namespace libsesame3bt
