#pragma once

#include <NimBLEClient.h>
#include <mbedtls/ccm.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <array>
#include <cstddef>
#include "Sesame.h"
#include "SesameInfo.h"
#include "api_wrapper.h"
#include "util.h"

namespace libsesame3bt {

/**
 * @brief セサミ操作クラス
 *
 */
class SesameClient : private NimBLEClientCallbacks {
 public:
	enum class state_t : uint8_t { idle, connected, authenticating, active };
	class Status {
		friend class SesameClient;

	 public:
		Status() {
			std::fill(std::begin(setting.data), std::end(setting.data), std::byte{0});
			std::fill(std::begin(status.data), std::end(status.data), std::byte{0});
		}
		float voltage() const {
			return model == Sesame::model_t::sesame_cycle ? status.lock.voltage * 3.6f / 1023 : status.lock.voltage * 7.2f / 1023;
		}
		bool voltage_critical() const { return status.lock.voltage_critical; }
		bool in_lock() const { return status.lock.in_lock; }
		bool in_unlock() const { return status.lock.in_unlock; }
		int16_t position() const { return status.lock.position; }
		int16_t lock_position() const { return setting.lock.lock_position; }
		int16_t unlock_position() const { return setting.lock.unlock_position; }
		float battery_pct() const { return voltage_to_pct(voltage()); }

		bool operator==(const Status& that) const {
			if (&that == this) {
				return true;
			}
			return memcmp(&that.setting, &setting, sizeof(setting)) == 0 && memcmp(&that.status, &status, sizeof(status)) == 0;
		}
		bool operator!=(const Status& that) const { return !(*this == that); }
		static float voltage_to_pct(float voltage);

	 private:
		Status(const Sesame::mecha_setting_t& _setting, const Sesame::mecha_status_t& _status, Sesame::model_t _model) : model(_model) {
			std::copy(util::cbegin(_setting.data), util::cend(_setting.data), setting.data);
			std::copy(util::cbegin(_status.data), util::cend(_status.data), status.data);
		}
		Sesame::mecha_setting_t setting;
		Sesame::mecha_status_t status;
		Sesame::model_t model = Sesame::model_t::unknown;

		struct BatteryTable {
			float voltage;
			float pct;
		};
		static inline const BatteryTable batt_tbl[] = {{6.0f, 100.0f}, {5.8f, 50.0f}, {5.7f, 40.0f}, {5.6f, 32.0f}, {5.4f, 21.0f},
		                                               {5.2f, 13.0f},  {5.1f, 10.0f}, {5.0f, 7.0f},  {4.8f, 3.0f},  {4.6f, 0.0f}};
	};

	class BotStatus {
		friend class SesameClient;

	 public:
		BotStatus() {
			std::fill(std::begin(setting.data), std::end(setting.data), std::byte{0});
			std::fill(std::begin(status.data), std::end(status.data), std::byte{0});
		}
		float voltage() const { return status.bot.voltage * 3.6f / 1023; }
		Sesame::motor_status_t motor_status() const { return status.bot.motor_status; }
		uint8_t user_pref_dir() const { return setting.bot.user_pref_dir; }
		uint8_t lock_sec() const { return setting.bot.lock_sec; }
		uint8_t unlock_sec() const { return setting.bot.unlock_sec; }
		uint8_t click_lock_sec() const { return setting.bot.click_lock_sec; }
		uint8_t click_unlock_sec() const { return setting.bot.click_unlock_sec; }
		uint8_t click_hold_sec() const { return setting.bot.click_hold_sec; }
		uint8_t button_mode() const { return setting.bot.button_mode; }
		float battery_pct() const { return Status::voltage_to_pct(voltage() * 2.0f); }

		bool operator==(const BotStatus& that) const {
			if (&that == this) {
				return true;
			}
			return memcmp(&that.setting, &setting, sizeof(setting)) == 0 && memcmp(&that.status, &status, sizeof(status)) == 0;
		}
		bool operator!=(const BotStatus& that) const { return !(*this == that); }

	 private:
		BotStatus(const Sesame::mecha_setting_t& _setting, const Sesame::mecha_status_t& _status) {
			std::copy(util::cbegin(_setting.data), util::cend(_setting.data), setting.data);
			std::copy(util::cbegin(_status.data), util::cend(_status.data), status.data);
		}
		Sesame::mecha_setting_t setting;
		Sesame::mecha_status_t status;
	};

	using status_callback_t = std::function<void(SesameClient& client, SesameClient::Status status)>;
	using bot_status_callback_t = std::function<void(SesameClient& client, SesameClient::BotStatus status)>;
	using state_callback_t = std::function<void(SesameClient& client, SesameClient::state_t state)>;

	static constexpr size_t PK_SIZE = 64;
	static constexpr size_t SECRET_SIZE = 16;
	SesameClient();
	virtual ~SesameClient();
	bool begin(const BLEAddress& address, Sesame::model_t model);
	bool set_keys(const std::array<std::byte, PK_SIZE>& public_key, const std::array<std::byte, SECRET_SIZE>& secret_key);
	bool set_keys(const char* pk_str, const char* secret_str);
	bool connect(int retry = 0);
	void disconnect();
	bool unlock(const char* tag);
	bool lock(const char* tag);
	bool click(const char* tag);
	bool is_session_active() const { return state == state_t::active; }
	void set_status_callback(status_callback_t callback);
	void set_bot_status_callback(bot_status_callback_t callback);
	void set_state_callback(state_callback_t callback);
	Sesame::model_t get_model() const { return model; }
	state_t get_state() const { return state; }

 private:
	static constexpr size_t MAX_RECV = 40;
	static constexpr size_t SK_SIZE = 32;
	static constexpr size_t AES_BLOCK_SIZE = 16;
	static inline const BLEUUID TxUUID{"16860002-a5ae-9856-b6d3-dbb4c676993e"};
	static inline const BLEUUID RxUUID{"16860003-a5ae-9856-b6d3-dbb4c676993e"};

	enum packet_kind_t { not_finished = 0, plain = 1, encrypted = 2 };  // do not use enum class to avoid warning in structure below.
	union __attribute__((packed)) packet_header_t {
		struct __attribute__((packed)) {
			bool is_start : 1;
			packet_kind_t kind : 2;
			std::byte unused : 5;
		};
		std::byte value;
	};
	static constexpr std::array<std::byte, 1> auth_add_data{};
	static constexpr std::array<std::byte, 2> sesame_ki{};
	static bool static_initialized;
	static inline api_wrapper<mbedtls_ctr_drbg_context> rng_ctx{mbedtls_ctr_drbg_init, mbedtls_ctr_drbg_free};
	static inline api_wrapper<mbedtls_entropy_context> ent_ctx{mbedtls_entropy_init, mbedtls_entropy_free};
	static inline api_wrapper<mbedtls_ecp_group> ec_grp{mbedtls_ecp_group_init, mbedtls_ecp_group_free};

	// Sesame info
	BLEAddress address;
	api_wrapper<mbedtls_ecp_point> sesame_pk{mbedtls_ecp_point_init, mbedtls_ecp_point_free};
	std::array<std::byte, SECRET_SIZE> sesame_secret;
	// session data
	long long enc_count;
	long long dec_count;
	api_wrapper<mbedtls_ccm_context> ccm_ctx{mbedtls_ccm_init, mbedtls_ccm_free};
	std::array<std::byte, 13> enc_iv;
	std::array<std::byte, 13> dec_iv;
	std::array<std::byte, MAX_RECV> recv_buffer;
	volatile state_t state = state_t::idle;
	size_t recv_size = 0;
	bool skipping = false;
	NimBLEClient* blec = nullptr;
	NimBLERemoteCharacteristic* tx;
	NimBLERemoteCharacteristic* rx;
	Sesame::mecha_setting_t mecha_setting{};
	Sesame::mecha_status_t mecha_status{};
	union {
		status_callback_t lock_status_callback{};
		bot_status_callback_t bot_status_callback;
	};
	state_callback_t state_callback = nullptr;
	Sesame::model_t model;

	bool is_key_set = false;
	bool is_key_shared = false;

	void reset_session();
	void notify_cb(NimBLERemoteCharacteristic* ch, const std::byte* data, size_t size, bool is_notify);
	bool send_command(Sesame::op_code_t op_code, Sesame::item_code_t item_code, const std::byte* data, size_t size, bool use_crypt);
	bool decrypt(const std::byte* in, size_t in_size, std::byte* out, size_t out_size);
	bool encrypt(const std::byte* in, size_t in_size, std::byte* out, size_t out_size);
	void handle_publish_initial();
	void handle_publish_mecha_setting();
	void handle_publish_mecha_status();
	void handle_response_login();
	void update_mecha_status(const Sesame::mecha_status_t& status);
	void update_mecha_setting(const Sesame::mecha_setting_t& setting);
	bool ecdh(const api_wrapper<mbedtls_mpi>& sk, std::array<std::byte, SK_SIZE>& out);
	bool create_key_pair(api_wrapper<mbedtls_mpi>& sk, std::array<std::byte, 1 + PK_SIZE>& pk);
	bool generate_tag_response(const std::array<std::byte, 1 + PK_SIZE>& pk,
	                           const std::array<std::byte, Sesame::TOKEN_SIZE>& local_token,
	                           const std::byte (&sesame_token)[Sesame::TOKEN_SIZE],
	                           std::array<std::byte, AES_BLOCK_SIZE>& tag_response);
	bool generate_session_key(const std::array<std::byte, Sesame::TOKEN_SIZE>& local_token,
	                          const std::byte (&sesame_token)[Sesame::TOKEN_SIZE],
	                          std::array<std::byte, 1 + PK_SIZE>& pk);
	void init_endec_iv(const std::array<std::byte, Sesame::TOKEN_SIZE>& local_token,
	                   const std::byte (&sesame_token)[Sesame::TOKEN_SIZE]);
	void fire_status_callback();
	void update_state(state_t new_state);

	// NimBLEClientCallbacks
	virtual void onDisconnect(NimBLEClient* pClient) override;
};

}  // namespace libsesame3bt
