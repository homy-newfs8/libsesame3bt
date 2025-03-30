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
		blec->setClientCallbacks(nullptr, false);
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
	this->state = state_t::idle;
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

/***
 * @brief Connect to the device asynchronously
 * @return true if start connecting successfully
 * @details This function will return immediately, and the connection result will be notified by the state callback.
 *          If the connection fails, state callback will be called with state_t::connect_failed.
 *          If the connection is successful, state callback will be called with state_t::connected.
 * 					To authenticate, call start_authenticate() after the state is state_t::connected.
 * 					DO NOT CALL start_authenticate() or disconnect() from the state callback, it will cause a deadlock.
 */
bool
SesameClient::connect_async() {
	if (!blec) {
		blec = NimBLEDevice::createClient();
		blec->setClientCallbacks(this, false);
	}
	is_async_connect = true;
	blec->setConnectTimeout(connect_timeout);
	if (blec->connect(address, true, true)) {
		set_state(state_t::connecting);
		return true;
	} else {
		DEBUG_PRINTLN("BLE connect async failed");
		return false;
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
	return start_authenticate();
}

/***
 * @brief Start authentication
 * @return true if authentication started successfully
 * @details Call this function after the state is state_t::connected.
 * Do not call this function from the state callback, it will cause a deadlock.
 */
bool
SesameClient::start_authenticate() {
	if (!blec) {
		DEBUG_PRINTLN("BLE client not initialized (already disconnected?)");
		return false;
	}
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
	DEBUG_PRINTLN("BT disconnected by peer, rc=%d", reason);
	disconnect();
}

void
SesameClient::onConnect(NimBLEClient* pClient) {
	if (!is_async_connect) {
		return;
	}
	DEBUG_PRINTLN("BT connected");
	set_state(state_t::connected);
}

void
SesameClient::onConnectFail(NimBLEClient* pClient, int reason) {
	if (!is_async_connect) {
		return;
	}
	DEBUG_PRINTLN("BT connect failed, rc=%d", reason);
	blec->setClientCallbacks(nullptr, false);
	blec->cancelConnect();
	set_state(state_t::connect_failed);
}

}  // namespace libsesame3bt
