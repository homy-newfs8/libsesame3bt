#pragma once

#include <NimBLEClient.h>
#include <NimBLEDevice.h>
#include <libsesame3bt/core.h>
#include <array>
#include <cstddef>
#include <memory>

namespace libsesame3bt {

/**
 * @brief Sesame client
 *
 */
class SesameClient : private NimBLEClientCallbacks, public libsesame3bt::core::SesameClientBackend {
 public:
	using state_t = libsesame3bt::core::SesameClientCore::state_t;
	// enum class state_t : uint8_t { idle, connected, authenticating, active };
	static constexpr size_t MAX_CMD_TAG_SIZE = Sesame::MAX_HISTORY_TAG_SIZE;

	using LockSetting = libsesame3bt::core::LockSetting;
	using BotSetting = libsesame3bt::core::BotSetting;
	using Status = libsesame3bt::core::Status;
	using History = libsesame3bt::core::History;
	using status_callback_t = std::function<void(SesameClient& client, Status status)>;
	using state_callback_t = std::function<void(SesameClient& client, SesameClient::state_t state)>;
	using history_callback_t = std::function<void(SesameClient& client, const History& history)>;

	SesameClient();
	SesameClient(const SesameClient&) = delete;
	SesameClient& operator=(const SesameClient&) = delete;
	virtual ~SesameClient();
	bool begin(const BLEAddress& address, Sesame::model_t model);
	bool set_keys(const std::array<std::byte, Sesame::PK_SIZE>& public_key,
	              const std::array<std::byte, Sesame::SECRET_SIZE>& secret_key) {
		return core->set_keys(public_key, secret_key);
	}
	bool set_keys(const char* pk_str, const char* secret_str) { return core->set_keys(pk_str, secret_str); }
	bool connect(int retry = 0);
	bool unlock(const char* tag) { return core->unlock(tag); }
	bool lock(const char* tag) { return core->lock(tag); }
	/**
	 * @brief Click operation (for Bot only)
	 *
	 * @param tag %History tag (But it seems not recorded in bot)
	 * @return True if the command sent successfully
	 */
	bool click(const char* tag) { return core->click(tag); }
	bool request_history() { return core->request_history(); }
	bool is_session_active() const { return core->is_session_active(); }
	void set_status_callback(status_callback_t callback) { status_callback = callback; }
	void set_state_callback(state_callback_t callback) { state_callback = callback; }
	void set_history_callback(history_callback_t callback) { history_callback = callback; }
	Sesame::model_t get_model() const { return core->get_model(); }
	state_t get_state() const { return core->get_state(); }
	void set_connect_timeout_sec(uint8_t timeout) { connect_timeout = timeout; }
	const std::variant<LockSetting, BotSetting>& get_setting() const { return core->get_setting(); }

	virtual bool write_to_tx(const uint8_t* data, size_t size) override;
	virtual void disconnect() override;

 private:
	static constexpr size_t MAX_RECV = 128;
	std::unique_ptr<libsesame3bt::core::SesameClientCore> core;

	// Sesame info
	BLEAddress address;
	// session data
	NimBLEClient* blec = nullptr;
	NimBLERemoteCharacteristic* tx = nullptr;
	NimBLERemoteCharacteristic* rx = nullptr;
	std::variant<LockSetting, BotSetting> setting;
	Status sesame_status;
	status_callback_t status_callback{};
	state_callback_t state_callback{};
	history_callback_t history_callback{};
	uint8_t connect_timeout = 30;

	void notify_cb(NimBLERemoteCharacteristic* ch, const std::byte* data, size_t size, bool is_notify);
	void _status_callback(libsesame3bt::core::SesameClientCore& client, SesameClient::Status status);
	void _state_callback(libsesame3bt::core::SesameClientCore& client, SesameClient::state_t state);
	void _history_callback(libsesame3bt::core::SesameClientCore& client, const SesameClient::History& history);
	virtual void onDisconnect(NimBLEClient* pClient) override;
};

}  // namespace libsesame3bt
