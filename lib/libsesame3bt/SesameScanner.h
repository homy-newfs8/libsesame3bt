#pragma once
#include <NimBLEDevice.h>
#include "SesameInfo.h"

namespace libsesame3bt {

class SesameScanner : private NimBLEScanCallbacks {
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
	NimBLEScan* scanner{};
	scan_handler_t handler{};

	void scan_completed(NimBLEScanResults results);
	virtual void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;
	virtual void onScanEnd(NimBLEScanResults results) override;

	SesameScanner() = default;
	~SesameScanner() = default;
};

}  // namespace libsesame3bt
