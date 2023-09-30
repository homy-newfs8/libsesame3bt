# libsesame3bt
ESP32 library to control SESAME 5 / SESAME 5 PRO / SESAME 3 / SESAME 4 / SESAME bot / SESAME 3 bike (SESAME Cycle) via Bluetooth LE

# Usage
platformio.ini
```ini
[env]
platform = espressif32
framework = arduino
lib_deps =
	https://github.com/homy-newfs8/libsesame3bt#0.11.0
build_flags =
	-std=gnu++17
build_unflags =
	-std=gnu++11
````

# Example
## Scan
```C++
using libsesame3bt::SesameInfo;
using libsesame3bt::SesameScanner;

void do_scan() {
	SesameScanner& scanner = SeameScanner::get();
	std::vector<SesameInfo> results;

	scanner.scan(10, [&results](SesameScanner& _scanner, const SesameInfo* _info)) {
		results.push_back(*_info);
	}
	Serial.printf("%u devices found\n", results.size());
	for (const auto& it : results) {
		Serial.printf("%s: %s: model=%u, registered=%u\n", it.uuid.toString().c_str(), it.address.toString().c_str(),
		                (unsigned int)it.model, it.flags.registered);
	}
}

```

## Control
```C++
using libsesame3bt::Sesame;
using libsesame3bt::SesameClient;
using libsesame3bt::SesameInfo;

void do_unlock_lock() {
	SesameClient client{};
	// Use SesameInfo to initialize
	client.begin(sesameInfo.addr, sesameInfo.model);
	// or specify bluetooth address and model type directory
	client.begin(BLEAddress{"***your device address***", BLE_ADDR_RANDOM}, Sesame::model_t::sesame_5);

	client.set_keys(nullptr, SESAME_SECRET);
	client.connect();
	// Wait for connection and authentication done
	// See example/by_scan/by_scan.cpp for details
	client.unlock(u8"**TAG**");
	delay(3000);
	client.lock(u8"***TAG***");
}
```
# Sample App
* [ESP32Sesame3App](http://github.com/homy-newfs8/ESP32Sesame3App)

# Integrate to your Home Automation system without code
* ESPHome External Component [esphome-sesame3](https://github.com/homy-newfs8/esphome-sesame3)

# License
MIT AND Apache-2.0

Almost all code is licensed under MIT-style license. Files under [src/mbedtls-extra](src/mbedtls-extra) are licensed under Apache-2.0.

# See Also
[README.ja.md](README.ja.md)
