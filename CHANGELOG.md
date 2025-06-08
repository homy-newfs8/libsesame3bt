# Changelog

# [0.25.0] 2025-06-08
- Support SESAME Face / SESAME Face Pro

## Important changes
- SESAME history specification has changed (May 2025). `struct History` has been modified to handle the new spec history in the history callback.
	- Added `trigger_type` member.
	- Maximum tag string length increased to 32 (Hexstring of UUID).
	- If `trigger_type` has a value, the `tag` string will be a UUID (128-bit) hex string.
- The tag parameter handling in `lock()`/`unlock()`/`click()` is unchanged. Specified string is passed to SESAME as is.

# [0.24.2] 2025-05-31
- Bump libsesame3bt-core version to v0.10.0

# [0.24.1] 2025-05-10
- Disable automatic exchangeMTU on connect (Caused connection instability on vanilla ESP32).
  (Later version of this library may handle MTU exchange properly.)

# [0.24.0] 2025-04-05
## Important changes
- `disconnect()` is no longer called automatically (from a callback) when disconnected by a peer, because it was also calling NimBLE::disconnected() from the callback, which made it unstable.
  - After you detect a disconnection, call `disconnect()` from outside the callback. You can detect the disconnection by checking the `get_state()` value.

## Feature added
- Added the function `connect_async()` and `start_authenticate()` to connect asynchronously.
- `SesameClient::state_t` symbols added for async connection state detection.
- See example/interactive for connecting asynchronously.

## Others

- Bump NimBLE-Arduino version to 2.2.3

# [0.23.0] 2025-02-22

- Bump libsesame3bt-core version to 0.9.0
- example/interactive modified
	- request history on every status reported
	- revert click(tag)
	- add click() (run default script)
	- add request history

# [0.22.0] 2025-1-31
- `SesameClient::get_ble_client()` added.
- Bump Nimble-Arduino version to 2.2.1.

# [0.21.0] 2024-12-29

## Breaking change and bug fix
- rename `SesameClient::set_connect_timeout_sec()` to `set_connect_timeout()`, and timeout resolution changed from seconds to milliseconds (not properly handled from 0.20.0).

# [0.20.1] 2024-12-29

- bump libsesame3bt-core version to 0.8.0

# [0.20.0] 2024-12-28

- Dependent library NimBLE-Arduino version bumped to 2.1.2 (released version).

# [0.19.0] 2024-11-03

## Breaking changes

- Dependent library NimBLE-Arduino version bumped to 2.x (under development).
  To use stable version of NimBLE-Arduino (1.14.x), keep using 0.18.0.
- The resolution of the scan_duration parameter of `SesameScanner::scan()` and `scan_async()` has been changed to millseconds (aligned to NimBLE-Aruduino API).

# [0.18.0] 2024-09-14

- Support SESAME Bot 2
- Example repeat_scan added

## Breaking changes

- click() (for Bot) parameter changed from string_view to optional<uint8_t>. History TAG support removed.


# [0.17.0] 2023-04-28

- Support SESAME Touch / Bike2 / Open Sensor (Tested on Touch only).
- Add `request_status()` (handled on some devices: SESAME 5 seems not handle).
- Add interactive example.

# [0.16.0] 2023-04-01

- Update libsesame3bt-core (v0.5.0)

# [0.14.0] 2023-03-24

- Update libsesame3bt-core library (v0.3.0)
- Some interface accept std::string_view instead of const char *, but usage is almost same.

# [0.13.0] 2023-03-23

- Update libsesame3bt-core library

# [0.12.0] 2023-03-13

## Major changes

- Split platform independent part to `libsesame3-core` library.

# [0.11.0] 2023-09-30

### Major changes

- Make `SesameInfo` fields to const.
- Change field name `SesameInfo::advertizement` to `SesameInfo::advertised_device`.
- `SesameScanner` can retrieve correct UUID from Touch / Touch PRO devices (However operations are not supported as before).

### Minor changes

- Parsing BLE advertisement/name functionality to public static function `SesameScanner::parse_advertisement()`.
- `lock()`, `unlock()`, `click()` will not crash when nullptr passed.

# [0.10.0] 2023-09-23

### Function added

- History reading method `request_history()` added. See by_address example for usage.

### Minor changes

- Change maximum tag size of SESAME 5 / SESAME 5 PRO to 29 (was 30)
- Do not send extra null bytes following the command tag on OS3.
- Fixed an issue operations cannot continue after received (and ignored) a long packet.

## [0.9.0] 2023-09-14 (A.R.E.)

### Major changes

- SESAME 5 / SESAME 5 PRO support added
	- Added new values to `Sesame::model_t`
- Unified the callback for SESAME Lock and SESAME bot
	- Removed `SesameClient::set_bot_status_callback()`
- Removed retrieve setting APIs from `SesameClient::Status`
- Added `SesameClient::LockSetting` and `SesameClient::BotSetting`
- Updated battery remaining percentage calculation table (align to SDK 3.0)

## [0.8.0] 2023-08-11

### Function added

- add `set_connect_timeout_sec(uint8_t timeout)` to set BLE connection timeout. Default is 30s.

## [0.7.0] 2023-07-20

### Minor changes

- #include filename fix in `Sesame.h`. Thanks @felidae for pointing out!

## [0.6.0] 2023-03-18

### Minor changes

- Bump NimBLE-Arduino version to 1.4.*
- Project structure changed, build example and test in one repository
	- libsesame3bt-dev project has been closed

## [0.5.0] 2022-09-10

### Major changes

- Fix fatal error on `SesameClient::disconnect()`.
- `SesameClient::Status::battery_pct()`, `SesameClient::BotStatus::battery_pct()` added.
- Delete internal `NimBLEClient` object on `SesameClient::disconnect()`. You can re-initialize NimBLE stack while disconnected.

### Minor changes

- constant `SesameClient::MAX_CMD_TAG_SIZE` moved to public.
- `util::truncate_utf8()` for not-null-terminated string added.

## [0.4.0] 2022-07-16

### Major changes

- Use std::byte instead of uint8_t, some public interfaces are affected.

## [0.3.0] 2022-06-11

### Major changes

- SESAME bot support.
- (use `SesameClient::set_bot_status_callback()` to set callback)

### Minor changes

- `SesameClient::get_state()` added.

## [0.2.0] 2022-05-20

### Major changes

- SESAME Cycle (SESAME 3 bike) support

### Minor changes

- `SesameClient::get_model()` added.
- `SesameClient::connect(int retry)` default retry count to 0 (was 3).
- SesameClient state transition changed: authenticating -> idle when authentication failure message received (implies disconnect).
- Member `NimBLEAdvertisedDevice& advertisement` added to `SesameInfo`.
- Public empty constructor deleted from `SesameInfo` .

### Environment changes

- Sample codes use stable platform framework `espressif32`.
