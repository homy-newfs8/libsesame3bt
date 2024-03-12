#pragma once
#include <NimBLEDevice.h>
#include <Sesame.h>
#include <cstddef>
#include <string>

namespace libsesame3bt {

class SesameInfo {
	friend class SesameScanner;

 public:
	SesameInfo() = delete;
	const BLEAddress address;
	const Sesame::model_t model;
	union flags_t {
		struct {
			bool registered : 1;
			bool unused : 7;
		};
		std::byte v;
		flags_t(std::byte _v) : v(_v) {}
	} const flags;
	const BLEUUID uuid;
	const NimBLEAdvertisedDevice& advertised_device;

 private:
	SesameInfo(const BLEAddress& _address,
	           Sesame::model_t _model,
	           std::byte flags_byte,
	           const BLEUUID& _uuid,
	           NimBLEAdvertisedDevice& _adv)
	    : address(_address), model(_model), flags(flags_byte), uuid(_uuid), advertised_device(_adv) {}
};

}  // namespace libsesame3bt
