/*
 * libasesame3btサンプル
 * SesameのBluetoothアドレスを指定して接続、入力されたコマンドを実行
 */
#include <Arduino.h>
#include <Sesame.h>
#include <SesameClient.h>
#include <cctype>
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
// SESAME OS3の機種では不要なので以下のように定義する
// #define SESAME_PK nullptr
#endif
#if !defined(SESAME_ADDRESS)
// 17文字のSesameのBluetoothアドレス (例 "01:23:45:67:89:ab")
#define SESAME_ADDRESS "**REPLACE**"
#endif
#if !defined(SESAME_MODEL)
// 使用するSESAMEのモデル (sesame_3, sesame_4, sesame_bike, sesame_bot, sesame_5, sesame_5_pro, sesame_bike_2, sesame_touch, sesame_touch_pro, open_sensor_1)
#define SESAME_MODEL Sesame::model_t::sesame_3
#endif

using libsesame3bt::Sesame;
using libsesame3bt::SesameClient;

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
	Serial.printf("Status in_lock=%u,in_unlock=%u,tgt=%d,pos=%d,volt=%.2f,batt_pct=%.2f,batt_crit=%u,motor_status=%s\n",
	              status.in_lock(), status.in_unlock(), status.target(), status.position(), status.voltage(), status.battery_pct(),
	              status.battery_critical(), motor_status_str(status.motor_status()));
	if ((client.get_model() == Sesame::model_t::sesame_bot && status.motor_status() == Sesame::motor_status_t::idle &&
	     last_status.motor_status() != Sesame::motor_status_t::idle) ||
	    (client.get_model() != Sesame::model_t::sesame_bot && status.in_lock() != last_status.in_lock())) {
		// 最新の操作履歴を読み出し可能
		// 読み出し内容はコールバックで取得する
		// ここでは初回の状態受信時と、施錠状態の変化時にrequest_history()を呼び出している
		Serial.println("history requested");
		client.request_history();
	}
	last_status = status;
}

// 履歴取得コールバック
// request_history()を実行するとコールバックされる
// SESAMEから失敗応答があった場合は history.type = Sesame::history_type_t::none となる
// 応答がない場合等、呼び出されない可能性もある
void
receive_history(SesameClient& client, const SesameClient::History& history) {
	if (history.type == Sesame::history_type_t::none) {
		Serial.println("Empty history");
		return;
	}
	struct tm tm;
	gmtime_r(&history.time, &tm);
	Serial.printf("History type=%u, %04d/%02d/%02d %02d:%02d:%02d, tag(%u)=%s\n", static_cast<uint8_t>(history.type),
	              tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, history.tag_len, history.tag);
}

// 登録デバイス一覧コールバック
// SESAME Touch / Remote に登録されている SESAME デバイスの一覧が通知される
void
receive_registered_devices(SesameClient& client, const std::vector<SesameClient::RegisteredDevice> devices) {
	Serial.printf("%u devices registered:\n", devices.size());
	for (const auto& dev : devices) {
		Serial.printf("uuid=%s, os=%s\n", BLEUUID(dev.uuid, sizeof(dev.uuid), true).toString().c_str(), os_str(dev.os_ver));
	}
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
	BLEDevice::init("");

	// Bluetoothアドレスと機種コードを設定
	// Bluetoothアドレスは必ずBLE_ADDR_RANDOMを指定すること
	if (!client.begin(BLEAddress{SESAME_ADDRESS, BLE_ADDR_RANDOM}, SESAME_MODEL)) {
		Serial.println("Failed to begin");
		for (;;) {
			delay(1000);
		}
	}
	// Sesameの鍵情報を設定
	if (!client.set_keys(SESAME_PK, SESAME_SECRET)) {
		Serial.println("Failed to set keys");
		return;
	}
	// SesameClient状態コールバックを設定
	client.set_state_callback([](auto& client, auto state) {
		sesame_state = state;
		Serial.printf("sesame state changed to %s\n", state_str(state));
	});
	// Sesame状態コールバックを設定
	client.set_status_callback(status_update);
	// 履歴受信コールバックを設定
	client.set_history_callback(receive_history);
	// 登録デバイス一覧コールバックを設定
	client.set_registered_devices_callback(receive_registered_devices);
}

enum class app_state { init, wait_running, running, done };

static uint32_t last_operated = 0;
app_state state = app_state::init;
int count = 0;

constexpr const char* MENU_STR = R"(
L) Lock
U) Unlock
C) Click
R) Request status

X) Exit

input>>)";

bool input_discarded;

void
loop() {
	switch (state) {
		case app_state::init:
			if (last_operated == 0 || millis() - last_operated > 3000) {
				count++;
				Serial.println("Connecting...");
				// connectはたまに失敗するようなので3回リトライする
				if (!client.connect(3)) {
					Serial.println("Failed to connect, abort");
					state = app_state::done;
					return;
				}
				last_operated = millis();
				state = app_state::wait_running;
			}
			break;
		case app_state::wait_running:
			if (client.is_session_active()) {
				// SesameClientの状態が Sesame::state_t::active になると設定を読み出し可能
				// 設定は接続完了時にのみ読み出されるため、接続後に他のアプリで変更された値を知ることはできない
				// SESAME Bot とそれ以外では読み出される設定値クラスが異なる
				// LockSettingとBotSettingのvariant型が得られるので、型を指定(判定)して読み出す必要がある
				if (const auto* setting = std::get_if<SesameClient::LockSetting>(&client.get_setting()); setting) {
					// 自動ロック時間は SESAME 5(PRO) のみ取得可能。その他の機種では -1 (不明)が得られる。
					// auto_lock_sec() が 0 であれば自動ロック無効
					Serial.printf("Setting lock=%d,unlock=%d,auto_lock=%d\n", setting->lock_position(), setting->unlock_position(),
					              setting->auto_lock_sec());
				} else if (const auto* setting = std::get_if<SesameClient::BotSetting>(&client.get_setting()); setting) {
					Serial.printf(
					    "Setting "
					    "pref_dir=%u,lock_sec=%u,unlock_sec=%u,click_lock_sec=%u,click_unlock_sec=%u,click_hold_sec=%u,button_mode=%u\n",
					    setting->user_pref_dir(), setting->lock_sec(), setting->unlock_sec(), setting->click_lock_sec(),
					    setting->click_unlock_sec(), setting->click_hold_sec(), setting->button_mode());
				} else {
					Serial.println("This model has no setting");
				}
				Serial.print(MENU_STR);
				state = app_state::running;
				break;
			} else if (sesame_state == SesameClient::state_t::idle) {
				Serial.printf("Disconnected from SESAME");
				state = app_state::done;
				break;
			}
			break;
		case app_state::running:
			// 認証が完了した
			if (client.is_session_active()) {
				if (!input_discarded) {  // discard VSCode Terminal initial echo
					while (Serial.read() >= 0) {
					}
					input_discarded = true;
				}
				int c = Serial.read();
				if (c <= 0) {
					break;
				}
				Serial.printf("%c", c);
				c = std::tolower(c);
				switch (c) {
					case 'l':
						client.lock(u8"施錠テスト");
						break;
					case 'u':
						client.unlock(u8"開錠テスト");
						break;
					case 'c':
						client.click(u8"クリックテスト");
						break;
					case 'r':
						client.request_status();
						Serial.println("Status requested");
						break;
					case 'x':
						client.disconnect();
						state = app_state::done;
						break;
				}
				if (state == app_state::running) {
					Serial.print(MENU_STR);
				}
			} else {
				Serial.println("Disconnected from SESAME");
				state = app_state::done;
				break;
			}
			break;
		case app_state::done:
			delay(1000);
			break;
	}
	delay(100);
}
