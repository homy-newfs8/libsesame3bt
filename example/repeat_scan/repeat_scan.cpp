#include <Arduino.h>
#include <SesameScanner.h>

using libsesame3bt::Sesame;
using libsesame3bt::SesameInfo;
using libsesame3bt::SesameScanner;

static std::string
model_str(Sesame::model_t model) {
	switch (model) {
		case Sesame::model_t::sesame_3:
			return "SESAME 3";
		case Sesame::model_t::wifi_2:
			return "Wi-Fi Module 2";
		case Sesame::model_t::sesame_bot:
			return "SESAME bot";
		case Sesame::model_t::sesame_bike:
			return "SESAME Cycle";
		case Sesame::model_t::sesame_4:
			return "SESAME 4";
		case Sesame::model_t::sesame_5:
			return "SESAME 5";
		case Sesame::model_t::sesame_5_pro:
			return "SESAME 5 PRO";
		case Sesame::model_t::sesame_touch:
			return "SESAME TOUCH";
		case Sesame::model_t::sesame_touch_pro:
			return "SESAME TOUCH PRO";
		case Sesame::model_t::sesame_bike_2:
			return "SESAME Cycle 2";
		case Sesame::model_t::open_sensor_1:
			return "Open Sensor";
		case Sesame::model_t::remote:
			return "Remote";
		case Sesame::model_t::remote_nano:
			return "Remote nano";
		case Sesame::model_t::sesame_bot_2:
			return "SESAME Bot 2";
		case Sesame::model_t::sesame_face:
			return "SESAME Face";
		case Sesame::model_t::sesame_face_pro:
			return "SESAME Face PRO";
		case Sesame::model_t::sesame_6:
			return "SESAME 6";
		case Sesame::model_t::sesame_6_pro:
			return "SESAME 6 PRO";

		default:
			return "UNKNOWN(" + std::to_string(static_cast<int8_t>(model)) + ")";
	}
}

SesameScanner* scanner;

void
setup() {
	delay(5000);
	Serial.begin(115200);

	BLEDevice::init("");
	scanner = &SesameScanner::get();

	Serial.println("Scanning 10 seconds");
}

bool scanning;
std::vector<SesameInfo> results;

void
loop() {
	if (scanning) {
		delay(10);
		return;
	}
	scanning = true;
	Serial.printf("Start scan\n");
	scanner->scan_async(10'000, [](SesameScanner& _scanner, const SesameInfo* _info) {
		if (_info) {  // nullptrの検査を実施
			// 結果をコピーして results vector に格納する
			Serial.printf("model=%s,addr=%s,UUID=%s,registered=%u\n", model_str(_info->model).c_str(), _info->address.toString().c_str(),
			              _info->uuid.toString().c_str(), _info->flags.registered);
			results.push_back(*_info);
			// _scanner.stop(); // スキャンを停止させたくなったらstop()を呼び出す
		} else {
			Serial.printf("%u devices found\n", results.size());
			results.clear();
			scanning = false;
		}
	});
}
