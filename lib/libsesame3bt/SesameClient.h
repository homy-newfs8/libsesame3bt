#pragma once

#include <NimBLEDevice.h>
#include <libsesame3bt/ClientCore.h>
#include <cstddef>
#if __cplusplus >= 202002L
#include <source_location>
#endif

namespace libsesame3bt {

/**
 * @brief Sesame client
 *
 */
class SesameClient : private core::SesameClientCore, private NimBLEClientCallbacks, private core::SesameBLEBackend {
 public:
	enum class state_t { idle, connected, authenticating, active, connecting, disconnecting };
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
	using result_t = core::result_t;

	SesameClient();
	SesameClient(const SesameClient&) = delete;
	SesameClient& operator=(const SesameClient&) = delete;
	virtual ~SesameClient();
	bool begin(const NimBLEAddress& address, Sesame::model_t model);
	bool begin(const NimBLEUUID& uuid, Sesame::model_t model);
	bool connect(int retry = 0);
	bool connect_async();
	bool start_authenticate();
	bool disconnect();
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
	bool unlock(history_tag_type_t type, const NimBLEUUID& uuid);
	bool lock(history_tag_type_t type, const NimBLEUUID& uuid);
	result_t get_last_result() const { return last_result; }

	static NimBLEAddress uuid_to_ble_address(const NimBLEUUID& uuid);

	bool unlock(std::string_view tag) { return accept_result(core::SesameClientCore::unlock(tag)); }
	bool unlock(history_tag_type_t type, const std::array<std::byte, HISTORY_TAG_UUID_SIZE>& uuid) {
		return accept_result(core::SesameClientCore::unlock(type, uuid));
	}
	bool lock(std::string_view tag) { return accept_result(core::SesameClientCore ::lock(tag)); }
	bool lock(history_tag_type_t type, const std::array<std::byte, HISTORY_TAG_UUID_SIZE>& uuid) {
		return accept_result(core::SesameClientCore::lock(type, uuid));
	}
	bool click(std::optional<uint8_t> script_no = std::nullopt) { return accept_result(core::SesameClientCore ::click(script_no)); }
	bool click(std::string_view tag) { return accept_result(core::SesameClientCore ::click(tag)); }
	bool request_history() { return accept_result(core::SesameClientCore ::request_history()); }
	bool request_status() { return accept_result(core::SesameClientCore ::request_status()); }
	bool set_keys(const std::array<std::byte, Sesame::PK_SIZE>& public_key,
	              const std::array<std::byte, Sesame::SECRET_SIZE>& secret_key) {
		return core::SesameClientCore::set_keys(public_key, secret_key) == core::result_t::success;
	}
	bool set_keys(std::string_view pk_str, std::string_view secret_str) {
		return core::SesameClientCore::set_keys(pk_str, secret_str) == result_t::success;
	}

	using core::SesameClientCore::get_model;
	using core::SesameClientCore::get_setting;
	using core::SesameClientCore::is_key_set;
	using core::SesameClientCore::is_session_active;

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
	bool is_async_connect;
	result_t last_result;

	void core_state_callback(core::SesameClientCore& core, core::state_t state);
	void set_state(state_t state);
#if __cplusplus >= 202002L && LIBSESAME3BT_DEBUG
	bool accept_result(core::result_t result, const std::source_location = std::source_location::current());
#else
	bool accept_result(core::result_t result);
#endif

	virtual void onDisconnect(NimBLEClient* pClient, int reason) override;
	virtual void onConnect(NimBLEClient* pClient) override;
	virtual void onConnectFail(NimBLEClient* pClient, int reason) override;
	virtual bool write_to_tx(const uint8_t* data, size_t size) override;
};

}  // namespace libsesame3bt
