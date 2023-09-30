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
	uint8_t uuid_bin[16];
	auto [model, flag_byte, is_valid] = parse_advertisement(adv->getManufacturerData(), adv->getName(), uuid_bin);
	if (!is_valid) {
		DEBUG_PRINTF("%s: Unexpected advertisement/name data, ignored\n", addr.toString().c_str());
		return;
	}
	auto info = SesameInfo(addr, model, flag_byte, BLEUUID{uuid_bin, std::size(uuid_bin), true}, *adv);
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

/**
 * @brief
 * Convert BLE advertisement data and name to model, flags byte, UUID.
 * @param manu_data BLE manufacturer data prepended with 16-bit uuid
 * @param name BLE name data
 * @param[out] uuid_bin in big endian
 * @return std::tuple<Sesame::model_t model, std::byte flag_byte, bool success>
 */
std::tuple<Sesame::model_t, std::byte, bool>
SesameScanner::parse_advertisement(const std::string& manu_data, const std::string& name, uint8_t (&uuid_bin)[16]) {
	if (manu_data.length() < 5 || manu_data[0] != 0x5a || manu_data[1] != 0x05) {
		DEBUG_PRINTF("Unexpected manufacturer data\n");
		return {Sesame::model_t::unknown, std::byte(0), false};
	}
	Sesame::model_t model = static_cast<Sesame::model_t>(manu_data[2]);
	std::byte flags = std::byte{manu_data[4]};
	if (model == Sesame::model_t::wifi_2) {
		if (manu_data.size() < 11) {
			return {model, flags, false};
		}
		std::copy(std::cbegin(manu_data) + 5, std::cbegin(manu_data) + 11,
		          std::copy(std::cbegin(WIFI_MODULE_UUID_HEAD), std::cend(WIFI_MODULE_UUID_HEAD), uuid_bin));
	} else {
		auto os = Sesame::get_os_ver(model);
		if (os == Sesame::os_ver_t::os2) {
			if (model == Sesame::model_t::sesame_bot) {
				Serial.println(name.c_str());
			}
			if (name.length() != 22) {
				DEBUG_PRINTF("%u: Unexpected name field length\n", name.length());
				return {model, flags, false};
			}
			uint8_t mod_name[22 + 2];
			std::copy(std::cbegin(name), std::cend(name), mod_name);
			mod_name[22] = mod_name[23] = '=';  // not nul terminated
			size_t idlen;
			int rc = mbedtls_base64_decode(uuid_bin, sizeof(uuid_bin), &idlen, mod_name, sizeof(mod_name));
			if (rc != 0 || idlen != sizeof(uuid_bin)) {
				return {model, flags, false};
			}
		} else if (os == Sesame::os_ver_t::os3) {
			if (manu_data.size() > 20) {
				std::copy(&manu_data[5], &manu_data[21], uuid_bin);
			} else {
				DEBUG_PRINTF("Unexpected manufacture data length");
				return {model, flags, false};
			}
		}
	}
	return {model, flags, true};
}

}  // namespace libsesame3bt
