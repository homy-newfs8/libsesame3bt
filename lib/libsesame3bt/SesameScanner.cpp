#include "SesameScanner.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Sesame.h>
#include <libsesame3bt/ScannerCore.h>

#ifndef LIBSESAME3BT_DEBUG
#define LIBSESAME3BT_DEBUG 0
#endif
#include "debug.h"

namespace libsesame3bt {

bool
SesameScanner::scan_async(uint32_t scan_duration, scan_handler_t handler) {
	this->handler = handler;
	scanner = NimBLEDevice::getScan();
	scanner->clearResults();
	scanner->setScanCallbacks(this);
	scanner->setInterval(1349);
	scanner->setWindow(449);
	scanner->setActiveScan(true);
	scanner->setMaxResults(0);
	return scanner->start(scan_duration, false);
}

void
SesameScanner::onScanEnd(const NimBLEScanResults& results, int reason) {
	if (handler) {
		handler(*this, nullptr);
		handler = nullptr;
	}
}

void
SesameScanner::scan(uint32_t scan_duration, scan_handler_t handler) {
	this->handler = handler;
	scanner = NimBLEDevice::getScan();
	scanner->clearResults();
	scanner->setScanCallbacks(this);
	scanner->setInterval(1349);
	scanner->setWindow(449);
	scanner->setActiveScan(true);
	scanner->setMaxResults(0);
	scanner->getResults(scan_duration, false);
	if (this->handler) {
		this->handler(*this, nullptr);
		this->handler = nullptr;
	}
}

void
SesameScanner::onResult(const NimBLEAdvertisedDevice* adv) {
	if (!adv->isAdvertisingService(NimBLEUUID(Sesame::SESAME3_SRV_UUID))) {
		return;
	}
	auto addr = adv->getAddress();
	uint8_t uuid_bin[16];
	auto [model, flag_byte, is_valid] = libsesame3bt::core::parse_advertisement(adv->getManufacturerData(), adv->getName(), uuid_bin);
	if (!is_valid) {
		DEBUG_PRINTF("%s: Unexpected advertisement/name data, ignored\n", addr.toString().c_str());
		return;
	}
	auto info = SesameInfo(addr, model, flag_byte, NimBLEUUID{uuid_bin, std::size(uuid_bin)}.reverseByteOrder(), *adv);
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
