#pragma once

#include <NimBLEDevice.h>
#include <libsesame3bt/ClientCore.h>
#include <cstddef>

namespace libsesame3bt {

/**
 * @brief Sesame client
 *
 */
class SesameClient : public core::SesameClientCore, private NimBLEClientCallbacks, private core::SesameClientBackend {
 public:
	using state_t = core::SesameClientCore::state_t;
	static constexpr size_t MAX_CMD_TAG_SIZE = Sesame::MAX_HISTORY_TAG_SIZE;

	using LockSetting = core::LockSetting;
	using BotSetting = core::BotSetting;
	using Status = core::Status;
	using History = core::History;
	using status_callback_t = std::function<void(SesameClient& client, Status status)>;
	using state_callback_t = std::function<void(SesameClient& client, SesameClient::state_t state)>;
	using history_callback_t = std::function<void(SesameClient& client, const History& history)>;

	SesameClient();
	SesameClient(const SesameClient&) = delete;
	SesameClient& operator=(const SesameClient&) = delete;
	virtual ~SesameClient();
	bool begin(const BLEAddress& address, Sesame::model_t model);
	bool connect(int retry = 0);
	virtual void disconnect() override;
	void set_connect_timeout_sec(uint8_t timeout) { connect_timeout = timeout; }
	void set_status_callback(status_callback_t callback) { status_callback = callback; }
	void set_state_callback(state_callback_t callback) { state_callback = callback; }
	void set_history_callback(history_callback_t callback) { history_callback = callback; }

 private:
	using SesameClientCore::begin;
	using SesameClientCore::on_connected;
	using SesameClientCore::on_disconnected;
	using SesameClientCore::on_received;

	BLEAddress address;
	NimBLEClient* blec = nullptr;
	NimBLERemoteCharacteristic* tx = nullptr;
	NimBLERemoteCharacteristic* rx = nullptr;
	status_callback_t status_callback{};
	state_callback_t state_callback{};
	history_callback_t history_callback{};
	uint8_t connect_timeout = 30;

	void ble_notify_callback(NimBLERemoteCharacteristic* ch, const std::byte* data, size_t size, bool is_notify);
	void _status_callback(core::SesameClientCore& client, SesameClient::Status status);
	void _state_callback(core::SesameClientCore& client, SesameClient::state_t state);
	void _history_callback(core::SesameClientCore& client, const SesameClient::History& history);
	virtual void onDisconnect(NimBLEClient* pClient) override;
	virtual bool write_to_tx(const uint8_t* data, size_t size) override;
};

}  // namespace libsesame3bt
