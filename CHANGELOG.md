# Changelog

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
