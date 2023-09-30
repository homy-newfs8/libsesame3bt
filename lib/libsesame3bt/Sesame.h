#pragma once
#include <NimBLEDevice.h>
#include <cstddef>

namespace libsesame3bt {

class Sesame {
 public:
	static constexpr size_t TOKEN_SIZE = 4;
	static inline const BLEUUID SESAME3_SRV_UUID{"0000fd81-0000-1000-8000-00805f9b34fb"};

	enum class model_t : int8_t {
		unknown = -1,
		sesame_3 = 0,
		wifi_2 = 1,
		sesame_bot = 2,
		sesame_bike = 3,
		sesame_cycle = 3,
		sesame_4 = 4,
		sesame_5 = 5,
		sesame_bike_2 = 6,
		sesame_5_pro = 7,
		open_sensor_1 = 8,
		sesame_touch_pro = 9,
		sesame_touch = 10,
		ble_connector = 11,
	};
	enum class motor_status_t : uint8_t { idle = 0, locking, holding, unlocking };
	enum class op_code_t : uint8_t {
		create = 1,
		read = 2,
		update = 3,
		delete_ = 4,
		sync = 5,
		async = 6,
		response = 7,
		publish = 8,
		undefine = 16
	};
	enum class item_code_t : uint8_t {
		none = 0,
		registration = 1,
		login = 2,
		user = 3,
		history = 4,
		version_tag = 5,
		disconnect_reboot_now = 6,
		enable_dfu = 7,
		time = 8,
		ble_connection_param = 9,
		ble_adv_param = 10,
		autolock = 11,
		server_adv_kick = 12,
		ssmtoken = 13,
		initial = 14,
		irer = 15,
		time_phone = 16,
		mech_setting = 80,
		mech_status = 81,
		lock = 82,
		unlock = 83,
		move_to = 84,
		drive_direction = 85,
		stop = 86,
		detect_dir = 87,
		toggle = 88,
		click = 89,
	};
	enum class result_code_t : uint8_t {
		success = 0,
		invalid_format = 1,
		not_supported = 2,
		storage_fail = 3,
		invalid_sig = 4,
		not_found = 5,
		unknown = 6,
		busy = 7,
		invalid_param = 8
	};
	enum class history_type_t : uint8_t {
		none = 0,
		ble_lock = 1,
		ble_unlock,
		time_changed,
		autolock_updated,
		mech_setting_updated,
		autolock,
		manual_locked,
		manual_unlocked,
		manual_else,
		drive_locked,
		drive_unlocked,
		drive_failed,
		ble_adv_param_updated,
		wm2_lock,
		wm2_unlock,
		web_lock,
		web_unlock,
		drive_clicked = 21,  // observed value
	};

	enum class os_ver_t : uint8_t { unknown = 0, os2 = 2, os3 = 3 };

	static os_ver_t get_os_ver(model_t model) {
		int8_t v = static_cast<int8_t>(model);
		if (v < 0 || v > static_cast<int8_t>(model_t::ble_connector)) {
			return os_ver_t::unknown;
		} else if (v >= static_cast<int8_t>(model_t::sesame_3) && v <= static_cast<int8_t>(model_t::sesame_4)) {
			return os_ver_t::os2;
		} else {
			return os_ver_t::os3;
		}
	}

	union __attribute__((packed)) mecha_setting_t {
		struct __attribute((packed)) {
			int16_t lock_position;
			int16_t unlock_position;
		} lock;
		struct __attribute__((packed)) {
			uint8_t user_pref_dir;
			uint8_t lock_sec;
			uint8_t unlock_sec;
			uint8_t click_lock_sec;
			uint8_t click_hold_sec;
			uint8_t click_unlock_sec;
			uint8_t button_mode;
		} bot;
		std::byte data[12]{};
	};
	struct __attribute__((packed)) mecha_setting_5_t {
		int16_t lock_position;
		int16_t unlock_position;
		int16_t auto_lock_sec;
	};

	union __attribute__((packed)) mecha_status_t {
		struct __attribute__((packed)) mecha_lock_status_t {
			uint16_t battery;
			int16_t target;
			int16_t position;
			uint8_t retcode;
			uint8_t unknown1 : 1;
			bool in_lock : 1;
			bool in_unlock : 1;
			uint8_t unknown2 : 2;
			bool is_battery_critical : 1;
		} lock;
		struct __attribute__((packed)) mecha_bot_status_t {
			uint16_t battery;
			uint16_t unknown1;
			motor_status_t motor_status;
			uint8_t unknown2[2];
			bool not_stop : 1;
			bool in_lock : 1;
			bool in_unlock : 1;
			bool unknown3 : 2;
			bool is_battery_critical : 1;
		} bot;
		std::byte data[8]{};
	};
	struct __attribute__((packed)) mecha_status_5_t {
		int16_t battery;
		int16_t target;
		int16_t position;
		uint8_t unknown1 : 1;
		bool in_lock : 1;
		uint8_t unknown2 : 2;
		bool is_stop : 1;
		bool is_battery_critical : 1;
	};
	struct __attribute__((packed)) publish_initial_t {
		std::byte token[TOKEN_SIZE];
	};
	struct __attribute__((packed)) message_header_t {
		op_code_t op_code;
		item_code_t item_code;
	};
	struct __attribute__((packed)) publish_mecha_status_t {
		mecha_status_t status;
	};
	struct __attribute__((packed)) publish_mecha_status_5_t {
		mecha_status_5_t status;
	};
	struct __attribute__((packed)) publish_mecha_setting_t {
		mecha_setting_t setting;
	};
	struct __attribute__((packed)) publish_mecha_setting_5_t {
		mecha_setting_5_t setting;
	};
	struct __attribute__((packed)) response_login_t {
		uint8_t op_code_2;
		result_code_t result;
		uint32_t timestamp;
		std::byte _unknown[4];
		mecha_setting_t mecha_setting;
		mecha_status_t mecha_status;
	};
	struct __attribute__((packed)) response_login_5_t {
		result_code_t result;
		uint32_t timestamp;
	};
	struct __attribute__((packed)) response_history_t {
		uint8_t op_code_2;
		result_code_t result;
		int32_t record_id;
		history_type_t type;
		long long timestamp;
	};
	struct __attribute__((packed)) response_history_5_t {
		result_code_t result;
		int32_t record_id;
		history_type_t type;
		uint32_t timestamp;
		mecha_status_5_t mecha_status;
	};

 private:
	Sesame() = delete;
	~Sesame() = delete;
};

}  // namespace libsesame3bt
