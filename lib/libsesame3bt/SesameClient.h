#pragma once

#include <NimBLEDevice.h>
#include <libsesame3bt/ClientCore.h>
#include <cstddef>

namespace libsesame3bt {

/**
 * @brief Sesame client
 *
 */
class SesameClient : private core::SesameClientCore, private NimBLEClientCallbacks, private core::SesameBLEBackend {
 public:
	enum class state_t { idle, connected, authenticating, active };
	static constexpr size_t MAX_CMD_TAG_SIZE = Sesame::MAX_HISTORY_TAG_SIZE;

	using LockSetting = core::LockSetting;
	using BotSetting = core::BotSetting;
	using Status = core::Status;
	using History = core::History;
	using RegisteredDevice = core::RegisteredDevice;
	using status_callback_t = std::function<void(SesameClient& client, Status status)>;
	using state_callback_t = std::function<void(SesameClient& client, state_t state)>;
	using history_callback_t = std::function<void(SesameClient& client, const History& history)>;
	using registered_devices_callback_t = std::function<void(SesameClient& client, const std::vector<RegisteredDevice> devices)>;

	SesameClient();
	SesameClient(const SesameClient&) = delete;
	SesameClient& operator=(const SesameClient&) = delete;
	virtual ~SesameClient();
	bool begin(const NimBLEAddress& address, Sesame::model_t model);
	bool connect(int retry = 0);
	virtual void disconnect() override;
	void set_connect_timeout(uint32_t timeout) { connect_timeout = timeout; }
	void set_status_callback(status_callback_t callback) { status_callback = callback; }
	void set_state_callback(state_callback_t callback) { state_callback = callback; }
	void set_history_callback(history_callback_t callback) { history_callback = callback; }
	void set_registered_devices_callback(registered_devices_callback_t callback) { registered_devices_callback = callback; }
	// warning: oveloading core method
	state_t get_state() const { return state; }
	/**
	 * @brief Get the ble client object
	 * @details This function may return nullptr when get_state() is `idle`, please check before use.
	 * @return NimBLEClient*
	 */
	NimBLEClient* get_ble_client() const { return blec; }
	using core::SesameClientCore::click;
	using core::SesameClientCore::get_model;
	using core::SesameClientCore::get_setting;
	using core::SesameClientCore::is_session_active;
	using core::SesameClientCore::lock;
	using core::SesameClientCore::request_history;
	using core::SesameClientCore::request_status;
	using core::SesameClientCore::set_keys;
	using core::SesameClientCore::unlock;

 private:
	NimBLEAddress address;
	NimBLEClient* blec = nullptr;
	NimBLERemoteCharacteristic* tx = nullptr;
	NimBLERemoteCharacteristic* rx = nullptr;
	status_callback_t status_callback{};
	state_callback_t state_callback{};
	history_callback_t history_callback{};
	registered_devices_callback_t registered_devices_callback{};
	state_t state = state_t::idle;
	uint32_t connect_timeout = 30'000;

	void core_state_callback(core::SesameClientCore& core, core::state_t state);
	void set_state(state_t state);

	virtual void onDisconnect(NimBLEClient* pClient, int reason) override;
	virtual bool write_to_tx(const uint8_t* data, size_t size) override;
};

}  // namespace libsesame3bt
