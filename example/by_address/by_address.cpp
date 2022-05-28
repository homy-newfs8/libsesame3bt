/*
 * libasesame3btサンプル
 * SesameのBluetoothアドレスがわかっている場合
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
// 使用するSESAMEのモデル (sesame_3, sesame_4, sesame_cycle)
#define SESAME_MODEL Sesame::model_t::sesame_3
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
SesameClient::Status last_status{};
SesameClient::state_t sesame_state;

// Sesameの状態通知コールバック
// Sesameのつまみの位置、電圧、施錠開錠状態が通知される
// Sesameからの通知がある毎に呼び出される(変化がある場合のみ通知されている模様)
// Sesameの設定変更があった場合も呼び出される(はずだが、今のところ動作していない)
void
status_update(SesameClient& client, SesameClient::Status status) {
	if (status != last_status) {
		Serial.printf_P(PSTR("Setting lock=%d,unlock=%d\n"), status.lock_position(), status.unlock_position());
		Serial.printf_P(PSTR("Status in_lock=%u,in_unlock=%u,pos=%d,volt=%.2f,volt_crit=%u\n"), status.in_lock(), status.in_unlock(),
		                status.position(), status.voltage(), status.voltage_critical());
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
		Serial.println(F("Failed to begin"));
		return;
	}
	// Sesameの鍵情報を設定
	if (!client.set_keys(sesame_pk, sesame_sec)) {
		Serial.println(F("Failed to set keys"));
		return;
	}
	// SesameClient状態コールバックを設定
	client.set_state_callback([](auto& client, auto state) { sesame_state = state; });
	// Sesame状態コールバックを設定
	client.set_status_callback(status_update);
}

static uint32_t last_operated = 0;
int state = 0;
int count = 0;

void
loop() {
	// 接続開始、認証完了待ち、開錠、施錠を順次実行する
	switch (state) {
		case 0:
			if (last_operated == 0 || millis() - last_operated > 3000) {
				count++;
				Serial.println(F("Connecting"));
				// connectはたまに失敗するようなので3回リトライする
				if (!client.connect(3)) {
					Serial.println(F("Failed to connect, abort"));
					state = 9999;
					return;
				}
				last_operated = millis();
				state = 1;
			}
			break;
		case 1:
			if (client.is_session_active()) {
				Serial.println(F("Unlocking"));
				// unloc(), lock()ともにコマンドの送信が成功した時点でtrueを返す
				// 開錠、施錠されたかはstatusコールバックで確認する必要がある
				if (!client.unlock(u8"開錠:テスト")) {
					Serial.println(F("Failed to send unlock command"));
				}
				last_operated = millis();
				if (client.get_model() == Sesame::model_t::sesame_cycle) {
					state = 3;
				} else {
					state = 2;
				}
			}
			break;
		case 2:
			if (millis() - last_operated > 5000) {
				Serial.println(F("Locking"));
				if (!client.lock(u8"施錠:テスト")) {
					Serial.println(F("Failed to send lock command"));
				}
				last_operated = millis();
				state = 3;
			}
			break;
		case 3:
			if (millis() - last_operated > 3000) {
				client.disconnect();
				Serial.println(F("Disconnected"));
				last_operated = millis();
				if (count > 0) {
					state = 4;
				} else {
					state = 0;
				}
			}
			break;
		case 4:
			// テストを兼ねてデストラクタを呼び出しているが、あえて明示的に呼び出す必要はない
			client.~SesameClient();
			Serial.println(F("All done"));
			state = 9999;
			break;
		default:
			// nothing todo
			break;
	}
	delay(100);
}
