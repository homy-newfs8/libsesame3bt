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
	SesameClientCore::set_state_callback([this](auto&, state_t state) {
		if (state_callback)
			state_callback(*this, state);
	});
	SesameClientCore::set_status_callback([this](auto&, Status status) {
		if (status_callback)
			status_callback(*this, status);
	});
	SesameClientCore::set_history_callback([this](auto&, const History& history) {
		if (history_callback)
			history_callback(*this, history);
	});
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
			        if (!isNotify || size <= 1)
				        return;
			        on_received(reinterpret_cast<std::byte*>(data), size);
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
SesameClient::onDisconnect(NimBLEClient* pClient) {
	DEBUG_PRINTLN("Bluetooth disconnected by peer");
	disconnect();
	on_disconnected();
}

}  // namespace libsesame3bt
