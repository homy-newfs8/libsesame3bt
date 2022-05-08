#pragma once
#include <NimBLEDevice.h>
#include "SesameInfo.h"

namespace libsesame3bt {

void scan_completed_handler(NimBLEScanResults);
class SesameScanner : private NimBLEAdvertisedDeviceCallbacks {
 public:
	using scan_handler_t = std::function<void(SesameScanner&, const SesameInfo*)>;
	static SesameScanner& get() {
		static SesameScanner instance;
		return instance;
	}
	void scan(uint32_t scan_duration, scan_handler_t handler);
	bool scan_async(uint32_t scan_duration, scan_handler_t handler);
	void stop();
	SesameScanner(const SesameScanner&) = delete;
	SesameScanner& operator=(const SesameScanner&) = delete;
	SesameScanner(SesameScanner&&) = delete;
	SesameScanner& operator=(SesameScanner&&) = delete;

 private:
	friend void scan_completed_handler(NimBLEScanResults);
	static SesameScanner* current_scanner;
	NimBLEScan* scanner;
	scan_handler_t handler;

	void scan_completed(NimBLEScanResults results);
	void scan_internal(uint32_t scan_duration);
	virtual void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;

	SesameScanner() = default;
	~SesameScanner() = default;
};

}  // namespace libsesame3bt
