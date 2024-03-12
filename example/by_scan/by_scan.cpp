/*
 * libasesame3btサンプル
 * BluetoothスキャンしてSesameを探す場合
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
#if !defined(SESAME_MODEL)
// 使用するSESAMEのモデル (sesame_3, sesame_4, sesame_cycle, sesame_bot)
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
using libsesame3bt::SesameInfo;
using libsesame3bt::SesameScanner;

SesameClient client{};

static const char*
model_str(Sesame::model_t model) {
	switch (model) {
		case Sesame::model_t::sesame_3:
			return "SESAME 3";
		case Sesame::model_t::wifi_2:
			return "Wi-Fi Module 2";
		case Sesame::model_t::sesame_bot:
			return "SESAME bot";
		case Sesame::model_t::sesame_bike:
			return "SESAME Cycle";
		case Sesame::model_t::sesame_4:
			return "SESAME 4";
		case Sesame::model_t::sesame_5:
			return "SESAME 5";
		case Sesame::model_t::sesame_5_pro:
			return "SESAME 5 PRO";
		case Sesame::model_t::sesame_touch:
			return "SESAME TOUCH";
		case Sesame::model_t::sesame_touch_pro:
			return "SESAME TOUCH PRO";
		case Sesame::model_t::sesame_bike_2:
			return "SESAME Cycle 2";
		default:
			return "UNKNOWN";
	}
}

// Bluetoothスキャンを実行し、最初に見つけたSESAME向けに接続設定を実行する
void
scan_and_init() {
	// SesameScannerはシングルトン
	SesameScanner& scanner = SesameScanner::get();

	Serial.println(F("Scanning 10 seconds"));
	std::vector<SesameInfo> results;

	// SesameScanner::scanはスキャン完了までブロックする
	// コールバック関数には SesameScanner& と、スキャンによって得られた情報 SesameInfo* が渡される
	// SesameInfo* の中身はコールバックを抜けると破壊されるので必要ならばコピーしておくこと
	// スキャンが終了する際には SesameInfo* = nullptr で呼び出される
	// コールバック中に _scanner.stop() を呼び出すと、そこでスキャンは終了する
	// スキャン結果には WiFiモジュール2が含まれるが本ライブラリでは対応していない
	// 非同期スキャンを実行する SesameScanner::scan_async()もある
	scanner.scan(10, [&results](SesameScanner& _scanner, const SesameInfo* _info) {
		if (_info) {  // nullptrの検査を実施
			// 結果をコピーして results vector に格納する
			Serial.printf("model=%s,addr=%s,UUID=%s,registered=%u\n", model_str(_info->model), _info->address.toString().c_str(),
			              _info->uuid.toString().c_str(), _info->flags.registered);
			results.push_back(*_info);
			// _scanner.stop(); // スキャンを停止させたくなったらstop()を呼び出す
		}
	});
	Serial.printf("%u devices found\n", results.size());
	auto found =
	    std::find_if(results.cbegin(), results.cend(), [](auto& it) { return it.model == SESAME_MODEL && it.flags.registered; });
	if (found != results.cend()) {
		Serial.printf("Using %s (%s)\n", found->uuid.toString().c_str(), model_str(found->model));
		// 最初に見つけた SESAME_MODEL のデバイスに接続する
		// 本サンプルでは認証用の鍵と見つかったSESAMEの組合せ確認は実施していないので、複数のSESAMEがある環境では接続に失敗することがある
		if (!client.begin(found->address, found->model)) {
			Serial.println("Failed to begin");
			return;
		}
		// SESAMEの鍵情報を設定
		if (!client.set_keys(sesame_pk, sesame_sec)) {
			Serial.println("Failed to set keys");
		}
	} else {
		Serial.println("No usable Sesame found");
	}
}

bool connected = false;
SesameClient::state_t sesame_state;

// SesameClientの状態変化コールバック
// Sesameとの切断を検知した場合は idle ステートへ遷移する
void
state_update(SesameClient& client, SesameClient::state_t state) {
	sesame_state = state;
	switch (state) {
		case SesameClient::state_t::idle:  // 暇、接続していない
			Serial.println("State: idle");
			break;
		case SesameClient::state_t::connected:  // とりあえず接続はした
			Serial.println("State: connected");
			break;
		case SesameClient::state_t::authenticating:  // 認証結果待ち
			Serial.println("State: authenticating");
			break;
		case SesameClient::state_t::active:  // 認証完了しSesameが使える状態
			Serial.println("State: active");
			break;
		default:
			Serial.printf("State: Unknown(%u)\n", static_cast<uint8_t>(state));
			break;
	}
}

void
setup() {
	Serial.begin(115200);
#ifdef ARDUINO_M5Stick_C
	pinMode(10, OUTPUT);
	digitalWrite(10, 0);
#endif

	// Bluetoothは初期化しておくこと
	BLEDevice::init("");

	// Bluetoothスキャンと接続設定
	scan_and_init();
	client.set_state_callback(state_update);

	// connectはたまに失敗するようなので3回リトライする
	Serial.print("Connecting...");
	connected = client.connect(3);

	Serial.println(connected ? "done" : "failed");
}

bool unlock_requested = false;

// setup()内で接続に成功していたら unlock を一回実行する
void
loop() {
	if (!connected) {
		delay(1000);
		return;
	}
	// 接続完了後に idle に遷移するのは認証エラーや切断が発生した場合(接続からやり直す必要がある)
	if (sesame_state == SesameClient::state_t::idle) {
		Serial.println("Failed to operate");
		connected = false;
		// このサンプルではリトライは実装していない
		return;
	}
	// 認証完了してSesameを制御可能になるまで待つ
	if (sesame_state != SesameClient::state_t::active) {
		delay(100);
		return;
	}
	if (!unlock_requested) {
		Serial.println("Unlocking");
		client.unlock(u8"ラベルは21バイトまたは30バイトに収まるように(勝手に切ります)");
		client.disconnect();
		Serial.println("Disconnected");
		connected = false;
	}
	delay(1000);
}
