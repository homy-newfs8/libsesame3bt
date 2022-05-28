# Changelog

## [0.2.0] 2022-05-20

### Major changes

- `SesameClient::connect(int retry)` default retry count to 0 (was 3).
- Member `NimBLEAdvertisedDevice& advertisement` added to `SesameInfo` class.

### Minor changes

- Public empty constructor deleted from `SesameInfo` class.

### Environment changes

- Sample codes use stable platform framework `espressif32`.
