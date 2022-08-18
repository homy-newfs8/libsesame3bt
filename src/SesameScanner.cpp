#include "SesameScanner.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <mbedtls/base64.h>
#include "Sesame.h"
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
		if (name.length() != 22) {
			DEBUG_PRINTFP(PSTR("%u: %s: Unexpected name field length, ignore this device\n"), manu_data[2], addr.toString().c_str());
			return;
		}
		uint8_t fixed_name[22 + 2];
		std::copy(name.cbegin(), name.cend(), fixed_name);
		fixed_name[22] = fixed_name[23] = '=';  // not nul terminated
		size_t idlen;
		int rc = mbedtls_base64_decode(uuid_bin, sizeof(uuid_bin), &idlen, fixed_name, sizeof(fixed_name));
		if (rc != 0 || idlen != sizeof(uuid_bin)) {
			DEBUG_PRINTFP(PSTR("%s: Unexpected name format, ignore this device\n"), addr.toString().c_str());
			return;
		}
	}
	auto info = SesameInfo(addr, model, std::byte{manu_data[4]}, BLEUUID{uuid_bin, std::size(uuid_bin), true}, adv);
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
