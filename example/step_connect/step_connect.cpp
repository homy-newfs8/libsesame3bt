/*
 * libasesame3btサンプル
 * 接続フェーズをステップバイステップで実行する
 */
#include <Arduino.h>
#include <Sesame.h>
#include <SesameClient.h>
#include <inttypes.h>
#include <libsesame3bt/util.h>
#include <cctype>
#include <optional>
// Sesame鍵情報設定用インクルードファイル
// 数行下で SESAME_SECRET 等を直接定義する場合は別ファイルを用意する必要はない
#if __has_include("mysesame-config.h")
#include "mysesame-config.h"
#endif

#if !defined(SESAME_SECRET)
// 32文字の16進数でSesameの秘密鍵(sesame-qr-reader 結果の Secret Key)
#define SESAME_SECRET "**REPLACE**"
#endif
#if !defined(SESAME_PK)
// 128文字の16進数でSesameの公開鍵(sesame-qr-reader 結果の Public Key)
#define SESAME_PK "**REPLACE**"
// SESAME OS3の機種では不要なので以下のように定義する(nullptrは指定しないこと)
// #define SESAME_PK ""
#endif
#if !defined(SESAME_ADDRESS) && !defined(SESAME_UUID)
// 17文字のSesameのBluetoothアドレス (例 "01:23:45:67:89:ab")を指定する
// SESAME OS3の機種ではUUIDを指定することもできる(UUIDから計算で求めたBluetoothアドレスに接続される)
// 34文字のUUID (例 "00001800-0000-1000-8000-00805f9b34fb")を指定する
#define SESAME_UUID "**REPLACE**"
#define SESAME_ADDRESS "**REPLACE**"
#endif
#if !defined(SESAME_MODEL)
// 使用するSESAMEのモデル (sesame_3, sesame_4, sesame_bike, sesame_bot, sesame_5, sesame_5_pro, sesame_bike_2, sesame_touch, sesame_touch_pro, open_sensor_1)
#define SESAME_MODEL Sesame::model_t::sesame_3
#endif
// TAGにUUIDを使う場合はUSE_UUID_TAGを1に設定し、TAG_UUIDとTAG_TYPEを定義する
#ifndef USE_UUID_TAG
#define USE_UUID_TAG 0
#endif
#if USE_UUID_TAG
#ifndef TAG_UUID
#define TAG_UUID "**REPLACE**"
#endif
#ifndef TAG_TYPE
#define TAG_TYPE libsesame3bt::history_tag_type_t::remote
#endif
#endif

using libsesame3bt::history_tag_type_t;
using libsesame3bt::Sesame;
using libsesame3bt::SesameClient;
using libsesame3bt::core::result_t;
namespace util = libsesame3bt::core::util;

#define DEBUG_AUTH_ERROR 0
#if DEBUG_AUTH_ERROR
#undef SESAME_SECRET
#define SESAME_SECRET "00000000000000000000000000000000"
#endif

SesameClient client;
SesameClient::Status last_status;
SesameClient::state_t sesame_state;

static const char*
motor_status_str(Sesame::motor_status_t status) {
	switch (status) {
		case Sesame::motor_status_t::idle:
			return "idle";
		case Sesame::motor_status_t::locking:
			return "locking";
		case Sesame::motor_status_t::holding:
			return "holding";
		case Sesame::motor_status_t::unlocking:
			return "unlocking";
		default:
			return "UNKNOWN";
	}
}

static const char*
state_str(SesameClient::state_t state) {
	using state_t = SesameClient::state_t;
	switch (state) {
		case state_t::idle:
			return "idle";
		case state_t::active:
			return "active";
		case state_t::authenticating:
			return "authenticating";
		case state_t::connected:
			return "connected";
		case state_t::connecting:
			return "connecting";
		case state_t::disconnecting:
			return "disconnecting";
		default:
			return "UNKNOWN";
	}
}

static const char*
os_str(Sesame::os_ver_t os) {
	switch (os) {
		case Sesame::os_ver_t::os2:
			return "OS2";
		case Sesame::os_ver_t::os3:
			return "OS3";
		default:
			return "UNKNOWN";
	}
}

// Sesameの状態通知コールバック
// Sesameのつまみの位置、電圧、施錠開錠状態が通知される
// Sesameからの通知がある毎に呼び出される(変化がある場合のみ通知されている模様)
void
status_update(SesameClient& client, SesameClient::Status status) {
	// 履歴の読み出し要求(履歴は上がってくる場合こない場合がある)
	client.request_history();
	Serial.printf(
	    "Status "
	    "in_lock=%u,in_unlock=%u,is_crit=%u,clutch_fail=%u,tgt=%d,pos=%d,volt=%.2f,batt_pct=%.2f,batt_crit=%u,is_stop=%s,motor_"
	    "status=%s\n",
	    status.in_lock(), status.in_unlock(), status.is_critical(), status.is_clutch_failed(), status.target(), status.position(),
	    status.voltage(), status.battery_pct(), status.battery_critical(), status.stopped() ? "stop" : "move",
	    motor_status_str(status.motor_status()));
	last_status = status;
}

// 履歴取得コールバック
// request_history()を実行するとコールバックされる
// SESAMEから失敗応答があった場合は history.result != Sesame::reslt_code_t::success となる
// 応答がない場合等、呼び出されない可能性もある
void
receive_history(SesameClient& client, const SesameClient::History& history) {
	// resultがsuccessでない場合は履歴は取得できていない(resultにはおおむねnot_foundかbusyが設定されている)
	if (history.result != Sesame::result_code_t::success) {
		return;
	}
	struct tm tm;
	gmtime_r(&history.time, &tm);
	// 過去に通知されたものと同じものが通知される場合がある。record_idで重複を検査可能
	Serial.printf(
	    "History(%" PRId32
	    ") type=%u, %04d/%02d/%02d %02d:%02d:%02d, tag(%u)=%s, history_tag_type=%s, s_volt=%s, s_volt2=%s, pct=%s, "
	    "pct(opensensor)=%s, extra=%s\n",
	    history.record_id, static_cast<uint8_t>(history.type), tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
	    tm.tm_sec, history.tag_len, history.tag,
	    history.history_tag_type.has_value() ? std::to_string(static_cast<uint8_t>(*history.history_tag_type)).c_str() : "none",
	    isnan(history.scaled_voltage) ? "N/A" : String(history.scaled_voltage, 2).c_str(),
	    isnan(history.scaled_voltage2) ? "N/A" : String(history.scaled_voltage2, 2).c_str(),
	    isnan(history.scaled_voltage)
	        ? "N/A"
	        : String(SesameClient::Status::scaled_voltage_to_pct(history.scaled_voltage, Sesame::model_t::sesame_5), 2).c_str(),
	    isnan(history.scaled_voltage)
	        ? "N/A"
	        : String(SesameClient::Status::scaled_voltage_to_pct(history.scaled_voltage, Sesame::model_t::open_sensor_1), 2).c_str(),
	    history.extra.empty() ? "N/A" : util::bin2hex(history.extra.data(), history.extra.size()).c_str());
}

// 登録デバイス一覧コールバック
// SESAME Touch / Remote に登録されている SESAME デバイスの一覧が通知される
void
receive_registered_devices(SesameClient& client, const std::vector<SesameClient::RegisteredDevice> devices) {
	Serial.printf("%u devices registered:\n", devices.size());
	for (const auto& dev : devices) {
		Serial.printf("uuid=%s, os=%s\n", NimBLEUUID(dev.uuid, sizeof(dev.uuid)).reverseByteOrder().toString().c_str(),
		              os_str(dev.os_ver));
	}
}

void
state_update(SesameClient& client, SesameClient::state_t state) {
	sesame_state = state;
}

void
setup() {
	Serial.begin(115200);
#ifdef ARDUINO_M5Stick_C
	pinMode(10, OUTPUT);
	digitalWrite(10, 0);
#endif
	delay(5000);
	Serial.println("setup started");

	// Bluetoothは初期化しておくこと
	// 通常はクライアント側のBLEアドレス指定は不要。NimBLEDevice::init()の呼び出しのみでよい。
	// esphome-seesame_serverに接続する場合は明示的にランダムアドレスを指定する必要がある
	if (!NimBLEDevice::init("") || !NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM) ||
	    !NimBLEDevice::setOwnAddr(NimBLEAddress{"ff:32:04:22:65:ff", BLE_ADDR_RANDOM})) {
		Serial.println("Failed to set own address");
		return;
	}
	Serial.println("BLEDevice initialized");

	// SESAME OS3機種ならばUUID指定で接続可能
	// Bluetoothアドレス指定の場合は必ずBLE_ADDR_RANDOMを指定すること
#if defined(SESAME_ADDRESS)
	if (!client.begin(NimBLEAddress{SESAME_ADDRESS, BLE_ADDR_RANDOM}, SESAME_MODEL)) {
#else
	Serial.printf("Connecting to Address: %s from UUID %s\n",
	              SesameClient::uuid_to_ble_address(NimBLEUUID{SESAME_UUID}).toString().c_str(), SESAME_UUID);
	if (!client.begin(NimBLEUUID{SESAME_UUID}, SESAME_MODEL)) {
#endif
		Serial.println("Failed to begin");
		for (;;) {
			delay(1000);
		}
	}
	Serial.println("SesameClient initialized");
	// Sesameの鍵情報を設定
	// SESAME OS3機種では公開鍵は不要だがnullptrを指定しないこと
	if (!client.set_keys(SESAME_PK == nullptr ? "" : SESAME_PK, SESAME_SECRET)) {
		Serial.println("Failed to set keys");
		return;
	}
	Serial.println("Sesame keys set");
	// SesameClient状態コールバックを設定
	client.set_state_callback(state_update);
	// Sesame状態コールバックを設定
	client.set_status_callback(status_update);
	// 履歴受信コールバックを設定
	client.set_history_callback(receive_history);
	// 登録デバイス一覧コールバックを設定
	client.set_registered_devices_callback(receive_registered_devices);
	// 5秒でタイムアウト
	client.set_connect_timeout(5'000);
	Serial.println("setup completed");
}

enum class app_state { init, wait_connected, wait_running, running, done };

constexpr const char* MENU_STR = R"(
C) Connect
A) Start Authenticate
D) Disconnect

L) Lock
U) Unlock

X) Exit
input>>)";

SesameClient::state_t last_state = sesame_state;
bool wait_input;

void
loop() {
	if (last_state != sesame_state) {
		Serial.printf("sesame state = %s\n", state_str(sesame_state));
		last_state = sesame_state;
	}
	if (!wait_input) {
		Serial.print(MENU_STR);
		wait_input = true;
	}
	int c = Serial.read();
	if (c <= 0) {
		delay(100);
		return;
	}
	Serial.printf("%c", c);
	c = std::tolower(c);
	switch (c) {
		case 'c':
			Serial.println("connecting");
			if (!client.connect_async()) {
				Serial.println("Failed to connect_async");
			}
			break;
		case 'a':
			Serial.println("authenticating");
			if (!client.start_authenticate()) {
				Serial.println("Failed to start_authenticate");
			}
			break;
		case 'd':
			Serial.println("disconnecting");
			if (!client.disconnect()) {
				Serial.println("Failed to disconnect");
			}
			break;
		case 'l':
			Serial.println("locking");
			if (!client.lock("")) {
				Serial.println("Failed to lock");
			}
			break;
		case 'u':
			Serial.println("unlocking");
			if (!client.unlock("")) {
				Serial.println("Failed to unlock");
			}
			break;
		default:
			Serial.println();
			Serial.print(MENU_STR);
	}
	delay(100);
}
