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

// Bluetoothスキャンを実行し、最初に見つけたSesame3またはSesame4向けに初期化を実行する
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
	// スキャン結果には WiFiモジュール2、SesameBotが含まれるが本ライブラリでは対応していない
	// 非同期スキャンを実行する SesameScanner::scan_async()もある
	scanner.scan(10, [&results](SesameScanner& _scanner, const SesameInfo* _info) {
		if (_info) {  // nullptrの検査を実施
			// 結果をコピーして results vector に格納する
			results.push_back(*_info);
		}
		// _scanner.stop(); // スキャンを停止させたくなったらstop()を呼び出す
	});
	Serial.printf_P(PSTR("%u devices found\n"), results.size());
	for (const auto& it : results) {
		Serial.printf_P(PSTR("%s: %s: model=%u, registered=%u\n"), it.uuid.toString().c_str(), it.address.toString().c_str(),
		                (unsigned int)it.model, it.flags.registered);
	}
	auto found = std::find_if(results.cbegin(), results.cend(), [](auto& it) {
		return (it.model == Sesame::model_t::sesame_3 || it.model == Sesame::model_t::sesame_4) && it.flags.registered;
	});
	if (found != results.cend()) {
		Serial.printf_P(PSTR("Using %s (Sesame%d)\n"), found->uuid.toString().c_str(),
		                found->model == Sesame::model_t::sesame_3 ? 3 : 4);
		// 最初に見つけた Sesame3/4 で初期化する
		// 本サンプルでは認証用の鍵と見つかったSesameの組合せ確認は実施していないので、複数のSesame3/4がある環境では接続に失敗することがある
		if (!client.begin(found->address, found->model)) {
			Serial.println(F("Failed to begin"));
			return;
		}
		// Sesameの鍵情報を設定
		if (!client.set_keys(sesame_pk, sesame_sec)) {
			Serial.println(F("Failed to set keys"));
		}
	} else {
		Serial.println(F("No usable Sesame found"));
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
			Serial.println(F("State: idle"));
			break;
		case SesameClient::state_t::connected:  // とりあえず接続はした
			Serial.println(F("State: connected"));
			break;
		case SesameClient::state_t::authenticating:  // 認証結果待ち
			Serial.println(F("State: authenticating"));
			break;
		case SesameClient::state_t::active:  // 認証完了しSesameが使える状態
			Serial.println(F("State: active"));
			break;
		default:
			Serial.printf_P(PSTR("State: Unknown(%u)\n"), static_cast<uint8_t>(state));
			break;
	}
}

void
setup() {
	Serial.begin(115200);

	// Bluetoothは初期化しておくこと
	BLEDevice::init("");

	scan_and_init();
	client.set_state_callback(state_update);
	Serial.print(F("Connecting..."));
	// connectはたまに失敗するようなのでデフォルトでは3回リトライする
	connected = client.connect();
	Serial.println(connected ? F("done") : F("failed"));
}

bool unlock_requested = false;

// setup()内で接続に成功していたら unlock を一回実行する
void
loop() {
	if (!connected) {
		delay(1000);
		return;
	}
	// 認証完了してSesameを制御可能になるまで待つ
	if (sesame_state != SesameClient::state_t::active) {
		delay(100);
		return;
	}
	if (!unlock_requested) {
		Serial.println(F("Unlocking"));
		client.unlock(u8"ラベルは21バイトに収まるよう(勝手に切ります)");
		client.disconnect();
		Serial.println(F("Disconnected"));
	}
	delay(1000);
}
