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
		case state_t::connect_failed:
			return "connect_failed";
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
	Serial.printf("Status in_lock=%u,in_unlock=%u,tgt=%d,pos=%d,volt=%.2f,batt_pct=%.2f,batt_crit=%u,motor_status=%s\n",
	              status.in_lock(), status.in_unlock(), status.target(), status.position(), status.voltage(), status.battery_pct(),
	              status.battery_critical(), motor_status_str(status.motor_status()));
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
	Serial.printf("History(%d) type=%u, %04d/%02d/%02d %02d:%02d:%02d, tag(%u)=%s\n", history.record_id,
	              static_cast<uint8_t>(history.type), tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
	              history.tag_len, history.tag);
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
	if (!client.begin(NimBLEAddress{SESAME_ADDRESS, BLE_ADDR_RANDOM}, SESAME_MODEL)) {
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
	client.set_state_callback([](auto& client, auto state) { Serial.printf("sesame state changed to %s\n", state_str(state)); });
	// Sesame状態コールバックを設定
	client.set_status_callback(status_update);
	// 履歴受信コールバックを設定
	client.set_history_callback(receive_history);
	// 登録デバイス一覧コールバックを設定
	client.set_registered_devices_callback(receive_registered_devices);
	// 5秒でタイムアウト
	client.set_connect_timeout(5'000);
}

enum class app_state { init, wait_connected, wait_running, running, done };

static uint32_t last_tried;
static app_state state = app_state::init;
static bool input_discarded;
static int try_count = 0;

constexpr const char* MENU_STR = R"(
L) Lock
U) Unlock
C) Click(tag)
D) Click()
0～9) Click(N)
R) Request status

X) Exit

input>>)";

void
loop() {
	switch (state) {
		case app_state::init:
			if (last_tried == 0 || millis() - last_tried > 500) {
				if (try_count > 4) {
					Serial.println("Giving up");
					state = app_state::done;
					return;
				}
				try_count++;
				Serial.println("Connecting async...");
				last_tried = millis();
				if (!client.connect_async()) {
					Serial.println("Failed to connect async");
					state = app_state::init;
					return;
				}
				Serial.println("async_connect retruned");
				state = app_state::wait_connected;
			}
			break;
		case app_state::wait_connected:
			if (client.get_state() == SesameClient::state_t::connected) {
				Serial.println("Connected");
				last_tried = millis();
				if (client.start_authenticate()) {
					Serial.println("Started authenticate");
					state = app_state::wait_running;
				} else {
					Serial.println("Failed to start authenticate");
					client.disconnect();
					state = app_state::init;
					return;
				}
			} else if (client.get_state() != SesameClient::state_t::connecting) {
				Serial.println("Failed to connect");
				client.disconnect();
				state = app_state::init;
				return;
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
			} else if (client.get_state() == SesameClient::state_t::idle) {
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
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						client.click(c - '0');
						break;
					case 'l':
						client.lock(u8"施錠テスト");
						break;
					case 'u':
						client.unlock(u8"開錠テスト");
						break;
					case 'c':
						client.click(u8"クリックテスト");
						break;
					case 'd':
						client.click();
						break;
					case 'r':
						client.request_status();
						Serial.println("Status requested");
						break;
					case 'h':
						client.request_history();
						Serial.println("History requested");
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
