# libsesame3bt
ESP32 library to control SESAME 5 / SESAME 5 PRO / SESAME Bot 2 / SESAME Touch / SESAME Touch PRO / SESAME 4 / SESAME 3 / SESAME bot / SESAME 3 bike (SESAME Cycle) via Bluetooth LE

This library is integration of [libsesame3bt-core](https://github.com/homy-newfs8/libsesame3bt-core) with [arduino-esp32](https://github.com/espressif/arduino-esp32), [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino).

# Usage
platformio.ini
```ini
[env]
platform = espressif32
framework = arduino
lib_deps =
	https://github.com/homy-newfs8/libsesame3bt#0.24.0
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

	scanner.scan(10'000, [&results](SesameScanner& _scanner, const SesameInfo* _info)) {
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
## Touch devices usage
For SESAME Touch / SESAME Touch PRO devices, you can retrieve battery information with this library. Try with [interactive example](example/interactive/).

Want to handle touch events? [libsesame3bt-server](https://github.com/homy-newfs8/libsesame3bt-server) exists for You!

# Sample App
* [ESP32Sesame3App](http://github.com/homy-newfs8/ESP32Sesame3App)

# Platform independent library
* [libsesame3bt-core](https://github.com/homy-newfs8/libsesame3bt-core)

# SESAME Touch, Remote, Open Sensor event handling library
* [libsesame3bt-server](https://github.com/homy-newfs8/libsesame3bt-server)

# Integrate to your Home Automation system without coding
* ESPHome External Component [esphome-sesame3](https://github.com/homy-newfs8/esphome-sesame3)

# See Also
[README.ja.md](README.ja.md)
