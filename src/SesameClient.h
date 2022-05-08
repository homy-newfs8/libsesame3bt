#pragma once

#include <NimBLEClient.h>
#include <mbedtls/ccm.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <array>
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
		/**
		 * @brief Construct a new Status object
		 *
		 */
		Status() {
			std::fill(std::begin(setting.data), std::end(setting.data), 0);
			std::fill(std::begin(status.data), std::end(status.data), 0);
		}
		float voltage() const { return status.voltage * 7.2f / 1023; }
		bool voltage_critical() const { return status.voltage_critical; }
		bool in_lock() const { return status.in_lock; }
		bool in_unlock() const { return status.in_unlock; }
		int16_t position() const { return status.position; }
		int16_t lock_position() const { return setting.lock_position; }
		int16_t unlock_position() const { return setting.unlock_position; }

		bool operator==(const Status& that) const {
			if (&that == this) {
				return true;
			}
			return memcmp(&that.setting, &setting, sizeof(setting)) == 0 && memcmp(&that.status, &status, sizeof(status)) == 0;
		}
		bool operator!=(const Status& that) const { return !(*this == that); }

	 private:
		Status(const Sesame::mecha_setting_t& _setting, const Sesame::mecha_status_t& _status) {
			std::copy(util::cbegin(_setting.data), util::cend(_setting.data), setting.data);
			std::copy(util::cbegin(_status.data), util::cend(_status.data), status.data);
		}
		Sesame::mecha_setting_t setting;
		Sesame::mecha_status_t status;
	};
	using status_callback_t = std::function<void(SesameClient& client, SesameClient::Status status)>;
	using state_callback_t = std::function<void(SesameClient& client, SesameClient::state_t state)>;

	static constexpr size_t PK_SIZE = 64;
	static constexpr size_t SECRET_SIZE = 16;
	SesameClient();
	virtual ~SesameClient();
	bool begin(const BLEAddress& address, Sesame::model_t model);
	bool set_keys(const std::array<uint8_t, PK_SIZE>& public_key, const std::array<uint8_t, SECRET_SIZE>& secret_key);
	bool set_keys(const char* pk_str, const char* secret_str);
	bool connect(int retry = 3);
	void disconnect();
	bool unlock(const char* tag);
	bool lock(const char* tag);
	bool is_session_active() const { return state == state_t::active; }
	void set_status_callback(status_callback_t callback);
	void set_state_callback(state_callback_t callback);

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
			uint8_t unused : 5;
		};
		uint8_t value;
	};
	static constexpr std::array<uint8_t, 1> auth_add_data{};
	static constexpr std::array<uint8_t, 2> sesame_ki{};
	static bool static_initialized;
	static inline api_wrapper<mbedtls_ctr_drbg_context> rng_ctx{mbedtls_ctr_drbg_init, mbedtls_ctr_drbg_free};
	static inline api_wrapper<mbedtls_entropy_context> ent_ctx{mbedtls_entropy_init, mbedtls_entropy_free};
	static inline api_wrapper<mbedtls_ecp_group> ec_grp{mbedtls_ecp_group_init, mbedtls_ecp_group_free};

	// Sesame info
	BLEAddress address;
	api_wrapper<mbedtls_ecp_point> sesame_pk{mbedtls_ecp_point_init, mbedtls_ecp_point_free};
	std::array<uint8_t, SECRET_SIZE> sesame_secret;
	// session data
	long long enc_count;
	long long dec_count;
	api_wrapper<mbedtls_ccm_context> ccm_ctx{mbedtls_ccm_init, mbedtls_ccm_free};
	std::array<uint8_t, 13> enc_iv;
	std::array<uint8_t, 13> dec_iv;
	std::array<uint8_t, MAX_RECV> recv_buffer;
	state_t state = state_t::idle;
	size_t recv_size = 0;
	bool skipping = false;
	NimBLEClient* blec = nullptr;
	NimBLERemoteCharacteristic* tx;
	NimBLERemoteCharacteristic* rx;
	Sesame::mecha_setting_t mecha_setting{};
	Sesame::mecha_status_t mecha_status{};
	status_callback_t status_callback = nullptr;
	state_callback_t state_callback = nullptr;

	bool is_key_set = false;
	bool is_key_shared = false;

	void reset_session();
	void notify_cb(NimBLERemoteCharacteristic* ch, const uint8_t* data, size_t size, bool is_notify);
	bool send_command(Sesame::op_code_t op_code, Sesame::item_code_t item_code, const uint8_t* data, size_t size, bool use_crypt);
	bool decrypt(const uint8_t* in, size_t in_size, uint8_t* out, size_t out_size);
	bool encrypt(const uint8_t* in, size_t in_size, uint8_t* out, size_t out_size);
	void handle_publish_initial();
	void handle_publish_mecha_setting();
	void handle_publish_mecha_status();
	void handle_response_login();
	void update_mecha_status(const Sesame::mecha_status_t& status);
	void update_mecha_setting(const Sesame::mecha_setting_t& setting);
	bool ecdh(const api_wrapper<mbedtls_mpi>& sk, std::array<uint8_t, SK_SIZE>& out);
	bool create_key_pair(api_wrapper<mbedtls_mpi>& sk, std::array<uint8_t, 1 + PK_SIZE>& pk);
	bool generate_tag_response(const std::array<uint8_t, 1 + PK_SIZE>& pk,
	                           const std::array<uint8_t, Sesame::TOKEN_SIZE>& local_token,
	                           const uint8_t (&sesame_token)[Sesame::TOKEN_SIZE],
	                           std::array<uint8_t, AES_BLOCK_SIZE>& tag_response);
	bool generate_session_key(const std::array<uint8_t, Sesame::TOKEN_SIZE>& local_token,
	                          const uint8_t (&sesame_token)[Sesame::TOKEN_SIZE],
	                          std::array<uint8_t, 1 + PK_SIZE>& pk);
	void init_endec_iv(const std::array<uint8_t, Sesame::TOKEN_SIZE>& local_token, const uint8_t (&sesame_token)[Sesame::TOKEN_SIZE]);
	void fire_status_callback();
	void update_state(state_t new_state);

	// NimBLEClientCallbacks
	virtual void onDisconnect(NimBLEClient* pClient) override;
};

}  // namespace libsesame3bt
