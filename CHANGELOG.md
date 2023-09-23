# Changelog

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
