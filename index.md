# libsesame3bt
ESP32 library to control SESAME 5 / SESAME 5 PRO / SESAME 3 / SESAME 4 / SESAME bot / SESAME 3 bike (SESAME Cycle) via Bluetooth LE

# Usage
platformio.ini
```ini
[env]
platform = espressif32
framework = arduino
lib_deps =
	https://github.com/homy-newfs8/libsesame3bt#0.9.0
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
	client.begin(BLEAddress{"***your device address***", BLE_ADDR_RANDOM}, Sesame::model_t::sesame_3);

	client.set_keys(SESAME_PK, SESAME_SECRET);
	client.connect();
	// Wait for connection and authentication done
	// See example/by_scan/by_scan.cpp for details
	client.unlock(u8"**TAG**");
	delay(3000);
	client.lock(u8"***TAG***");
}
```
# Sample App
[ESP32Sesame3App](http://github.com/homy-newfs8/ESP32Sesame3App)

<div class="youtube">
<iframe width="560" height="315" src="https://www.youtube.com/embed/iC5dZ8WWqk4?cc_load_policy=1" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>
</div>

# License
MIT AND Apache-2.0