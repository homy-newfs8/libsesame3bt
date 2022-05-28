#pragma once
#include <NimBLEDevice.h>
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
		uint8_t v;
		flags_t(uint8_t _v) : v(_v) {}
	} flags{0};
	BLEUUID uuid;
	NimBLEAdvertisedDevice& advertizement;

 private:
	SesameInfo(const BLEAddress& _address,
	           Sesame::model_t _model,
	           uint8_t flags_byte,
	           const BLEUUID& _uuid,
	           NimBLEAdvertisedDevice* _advertizement)
	    : address(_address), model(_model), flags(flags_byte), uuid(_uuid), advertizement(*_advertizement) {}
};

}  // namespace libsesame3bt