#include "SesameScanner.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "Sesame.h"
#include "libbase64-arduino/libbase64.h"
#include "util.h"

#ifndef LIBSESAME3BT_DEBUG
#define LIBSESAME3BT_DEBUG 0
#endif
#include "debug.h"
namespace libsesame3bt {

namespace {

const uint8_t WIFI_MODULE_UUID_HEAD[]{0x00, 0x00, 0x00, 0x00, 0x05, 0x5a, 0xfd, 0x81, 0x00, 0x01};

}

// scan_async handler
void
scan_completed_handler(NimBLEScanResults results) {
	SesameScanner::get().scan_completed(results);
}

void
SesameScanner::scan_completed(NimBLEScanResults results) {
	if (handler) {
		handler(*this, nullptr);
		handler = nullptr;
	}
}

bool
SesameScanner::scan_async(uint32_t scan_duration, scan_handler_t handler) {
	this->handler = handler;
	scanner = NimBLEDevice::getScan();
	scanner->clearResults();
	scanner->setAdvertisedDeviceCallbacks(this);
	scanner->setInterval(1349);
	scanner->setWindow(449);
	scanner->setActiveScan(true);
	scanner->setMaxResults(0);
	return scanner->start(scan_duration, scan_completed_handler, false);
}

void
SesameScanner::scan(uint32_t scan_duration, scan_handler_t handler) {
	this->handler = handler;
	scanner = NimBLEDevice::getScan();
	scanner->clearResults();
	scanner->setAdvertisedDeviceCallbacks(this);
	scanner->setInterval(1349);
	scanner->setWindow(449);
	scanner->setActiveScan(true);
	scanner->setMaxResults(0);
	scanner->start(scan_duration, false);
	if (this->handler) {
		this->handler(*this, nullptr);
		this->handler = nullptr;
	}
}

void
SesameScanner::onResult(NimBLEAdvertisedDevice* adv) {
	if (!adv->isAdvertisingService(Sesame::SESAME3_SRV_UUID)) {
		return;
	}
	auto addr = adv->getAddress();
	auto manu_data = adv->getManufacturerData();
	if (manu_data.length() < 5 || manu_data[0] != 0x5a || manu_data[1] != 0x05) {
		DEBUG_PRINTFP(PSTR("%s: Unexpected manufacturer data, ignore this device\n"), addr.toString().c_str());
		return;
	}
	Sesame::model_t model = static_cast<Sesame::model_t>(manu_data[2]);
	uint8_t uuid_bin[16];
	if (model == Sesame::model_t::wifi_2) {
		if (manu_data.size() < 11) {
			DEBUG_PRINTFP(PSTR("%s: Short manufacturer data for WiFi module, ignore this device\n"), addr.toString().c_str());
		}
		std::copy(std::cbegin(manu_data) + 5, std::cbegin(manu_data) + 11,
		          std::copy(util::cbegin(WIFI_MODULE_UUID_HEAD), util::cend(WIFI_MODULE_UUID_HEAD), uuid_bin));
	} else {
		auto name = adv->getName();
		if (name.empty() || name.length() != 22) {
			DEBUG_PRINTFP(PSTR("%u: %s: Unexpected name field length, ignore this device\n"), manu_data[2], addr.toString().c_str());
			return;
		}
		size_t idlen = decode_base64_length(reinterpret_cast<uint8_t*>(&name[0]), name.length());
		if (idlen != 16) {
			DEBUG_PRINTFP(PSTR("%s: Unexpected name format, ignore this device\n"), addr.toString().c_str());
			return;
		}
		if (decode_base64((uint8_t*)name.data(), name.length(), uuid_bin) != std::size(uuid_bin)) {
			DEBUG_PRINTFP(PSTR("%s: Unexpected name format, ignore this device\n"), addr.toString().c_str());
			return;
		}
	}
	auto info = SesameInfo(addr, model, static_cast<const uint8_t>(manu_data[4]), BLEUUID{uuid_bin, std::size(uuid_bin), true});
	if (handler) {
		handler(*this, &info);
	}
}

void
SesameScanner::stop() {
	if (scanner) {
		scanner->stop();
	}
}

}  // namespace libsesame3bt
