#pragma once
#include <NimBLEDevice.h>
#include <cstddef>
#include <string>
#include "Sesame.h"
namespace libsesame3bt {

class SesameInfo {
	friend class SesameScanner;

 public:
	SesameInfo() = delete;
	BLEAddress address;
	Sesame::model_t model = Sesame::model_t::unknown;
	union flags_t {
		struct {
			bool registered : 1;
			bool unused : 7;
		};
		std::byte v;
		flags_t(std::byte _v) : v(_v) {}
	} flags;
	BLEUUID uuid;
	NimBLEAdvertisedDevice& advertizement;

 private:
	SesameInfo(const BLEAddress& _address,
	           Sesame::model_t _model,
	           std::byte flags_byte,
	           const BLEUUID& _uuid,
	           NimBLEAdvertisedDevice* _advertizement)
	    : address(_address), model(_model), flags(flags_byte), uuid(_uuid), advertizement(*_advertizement) {}
};

}  // namespace libsesame3bt
