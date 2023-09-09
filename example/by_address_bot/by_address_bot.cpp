/*
 * libasesame3btサンプル
 * SESAME botのBluetoothアドレスがわかっている場合
 */
#include <Arduino.h>
#include <Sesame.h>
#include <SesameClient.h>
#include <SesameScanner.h>
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
#endif
#if !defined(SESAME_ADDRESS)
// 17文字のSesameのBluetoothアドレス (例 "01:23:45:67:89:ab")
#define SESAME_ADDRESS "**REPLACE**"
#endif
#if !defined(SESAME_MODEL)
// 使用するSESAMEのモデル (sesame_3, sesame_4, sesame_cycle, sesame_bot)
#define SESAME_MODEL Sesame::model_t::sesame_bot
#endif

// 64 bytes public key of Sesame
// 128 bytes hex str
const char* sesame_pk = SESAME_PK;
// 16 bytes secret key of Sesame
// 32 bytes hex str
const char* sesame_sec = SESAME_SECRET;

using libsesame3bt::Sesame;
using libsesame3bt::SesameClient;

SesameClient client{};
SesameClient::BotStatus last_status{};
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

// SESAME botの状態通知コールバック
// 鍵系のSESAMEとはコールバックに渡される引数が異なりる
// Sesameからの通知がある毎に呼び出される
void
status_update(SesameClient& client, SesameClient::BotStatus status) {
	if (status != last_status) {
		Serial.printf(
		    "Setting dir=%u,lock_sec=%u,unlock_sec=%u,click_lock_sec=%u,click_hold_sec=%u,click_unlock_sec=%u,button_mode=%u\n",
		    status.user_pref_dir(), status.lock_sec(), status.unlock_sec(), status.click_lock_sec(), status.click_hold_sec(),
		    status.click_unlock_sec(), status.button_mode());
		Serial.printf("Status volt=%.2f,motor_status=%s,stopped=%u,in_lock=%u,in_unlock=%u,batt_pct=%.2f,batt_crit=%u\n",
		              status.voltage(), motor_status_str(status.motor_status()), status.stopped(), status.in_lock(), status.in_unlock(),
		              status.battery_pct(), status.battery_critical());
		last_status = status;
	}
}

void
setup() {
	Serial.begin(115200);

	// Bluetoothは初期化しておくこと
	BLEDevice::init("");

	// Bluetoothアドレスと機種コードを設定(sesame_3, sesame_4, sesame_cycle を指定可能)
	if (!client.begin(BLEAddress{SESAME_ADDRESS, BLE_ADDR_RANDOM}, SESAME_MODEL)) {
		Serial.println("Failed to begin");
		return;
	}
	// Sesameの鍵情報を設定
	if (!client.set_keys(sesame_pk, sesame_sec)) {
		Serial.println("Failed to set keys");
		return;
	}
	// SesameClient状態コールバックを設定
	client.set_state_callback([](auto& client, auto state) { sesame_state = state; });
	// SesameBot状態コールバックを設定(鍵系とは呼び出すメソッドが異なる)
	client.set_bot_status_callback(status_update);
}

static uint32_t last_operated = 0;
enum class state_t { init, connecting, unlocking, locking, done, finish, none };
state_t state = state_t::init;

void
loop() {
	// 接続開始、認証完了待ち、開錠、施錠を順次実行する
	switch (state) {
		case state_t::init:
			if (last_operated == 0 || millis() - last_operated > 3000) {
				Serial.println("Connecting...");
				// connectはたまに失敗するようなので3回リトライする
				if (!client.connect(3)) {
					Serial.println("Failed to connect, abort");
					state = state_t::finish;
					return;
				}
				Serial.println("connected");
				last_operated = millis();
				state = state_t::connecting;
			}
			break;
		case state_t::connecting:
			if (client.is_session_active()) {
				Serial.println("Unlocking");
				// unloc(), lock(), click()ともにコマンドの送信が成功した時点でtrueを返す
				// 動作状況はstatusコールバックで確認する必要がある
				if (!client.unlock(u8"開錠:テスト")) {
					Serial.println("Failed to send unlock command");
				}
				last_operated = millis();
				state = state_t::unlocking;
			} else {
				if (client.get_state() == SesameClient::state_t::idle) {
					Serial.println("Failed to authenticate");
					state = state_t::finish;
				}
			}
			break;
		case state_t::unlocking:
			if (millis() - last_operated > 5000) {
				Serial.println("Locking");
				if (!client.lock(u8"施錠:テスト")) {
					Serial.println("Failed to send lock command");
				}
				last_operated = millis();
				state = state_t::locking;
			}
			break;
		case state_t::locking:
			if (millis() - last_operated > 5000) {
				Serial.println("Clicking");
				if (!client.click(u8"クリック:テスト")) {
					Serial.println("Failed to send lock command");
				}
				last_operated = millis();
				state = state_t::done;
			}
			break;
		case state_t::done:
			if (millis() - last_operated > 5000) {
				client.disconnect();
				Serial.println("Disconnected");
				last_operated = millis();
				state = state_t::finish;
			}
			break;
		case state_t::finish:
			// テストを兼ねてデストラクタを呼び出しているが、あえて明示的に呼び出す必要はない
			client.~SesameClient();
			Serial.println("All done");
			state = state_t::none;
			break;
		default:
			// nothing todo
			break;
	}
	delay(100);
}
