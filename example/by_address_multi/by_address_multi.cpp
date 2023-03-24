/*
 * libasesame3btマルチ接続サンプル
 * SesameのBluetoothアドレスがわかっている場合
 */
#include <Arduino.h>
#include <Sesame.h>
#include <SesameClient.h>
#include <SesameScanner.h>
#include <algorithm>

// Sesame鍵情報設定用インクルードファイル
// 数行下で SESAME_SECRET 等を直接定義する場合は別ファイルを用意する必要はない
#if __has_include("mysesame-config.h")
#include "mysesame-config.h"
#endif

#if !defined(SESAME0_SECRET)
// 32文字の16進数でSesameの秘密鍵(sesame-qr-reader 結果の Secret Key)
#define SESAME0_SECRET "**REPLACE**"
#endif
#if !defined(SESAME0_PK)
// 128文字の16進数でSesameの公開鍵(sesame-qr-reader 結果の Public Key)
#define SESAME0_PK "**REPLACE**"
#endif
#if !defined(SESAME0_ADDRESS)
// 17文字のSesameのBluetoothアドレス (例 "01:23:45:67:89:ab")
#define SESAME0_ADDRESS "**REPLACE**"
#endif
#if !defined(SESAME0_MODEL)
// 使用するSESAMEのモデル (sesame_3, sesame_4, sesame_cycle)
#define SESAME0_MODEL Sesame::model_t::sesame_3
#endif

#if !defined(SESAME1_SECRET)
// 32文字の16進数でSesameの秘密鍵(sesame-qr-reader 結果の Secret Key)
#define SESAME1_SECRET "**REPLACE**"
#endif
#if !defined(SESAME1_PK)
// 128文字の16進数でSesameの公開鍵(sesame-qr-reader 結果の Public Key)
#define SESAME1_PK "**REPLACE**"
#endif
#if !defined(SESAME1_ADDRESS)
// 17文字のSesameのBluetoothアドレス (例 "01:23:45:67:89:ab")
#define SESAME1_ADDRESS "**REPLACE**"
#endif
#if !defined(SESAME1_MODEL)
// 使用するSESAMEのモデル (sesame_3, sesame_4, sesame_cycle)
#define SESAME1_MODEL Sesame::model_t::sesame_3
#endif

using libsesame3bt::Sesame;
using libsesame3bt::SesameClient;

// 接続するSesameの数だけ、秘密情報、PK等を用意する
constexpr const char* const sesame_secret[] = {SESAME0_SECRET, SESAME1_SECRET};
constexpr const char* const sesame_pk[] = {SESAME0_PK, SESAME1_PK};
const BLEAddress sesame_address[] = {BLEAddress(SESAME0_ADDRESS, BLE_ADDR_RANDOM), BLEAddress(SESAME1_ADDRESS, BLE_ADDR_RANDOM)};
constexpr const Sesame::model_t sesame_model[] = {SESAME0_MODEL, SESAME1_MODEL};

SesameClient clients[std::size(sesame_secret)];
SesameClient::Status sesame_status[std::size(sesame_secret)];
SesameClient::state_t sesame_state[std::size(sesame_secret)];

void
setup() {
	Serial.begin(115200);

	BLEDevice::init("");
	for (size_t i = 0; i < std::size(clients); i++) {
		if (!clients[i].begin(sesame_address[i], sesame_model[i])) {
			Serial.printf("%u: Failed to begin\n", i);
			break;
		}
		if (!clients[i].set_keys(sesame_pk[i], sesame_secret[i])) {
			Serial.printf("%u: Failed to set keys\n", i);
			return;
		}
		clients[i].set_state_callback([i](auto& client, auto state) { sesame_state[i] = state; });
		clients[i].set_status_callback([i](auto& client, auto status) {
			if (status != sesame_status[i]) {
				Serial.printf("%u: Setting lock=%d,unlock=%d\n", i, status.lock_position(), status.unlock_position());
				Serial.printf("%u: Status in_lock=%u,in_unlock=%u,pos=%d,volt=%.2f,volt_crit=%u\n", i, status.in_lock(), status.in_unlock(),
				              status.position(), status.voltage(), status.voltage_critical());
				sesame_status[i] = status;
			}
		});
	}
}

static uint32_t last_operated = 0;
enum class op_t { connect, lock, unlock };
op_t op = op_t::connect;
int count = 0;

// すべてのSesameに対して、
// 接続→施錠→開錠→施錠→開錠...
// を実行する(電源を切るまで繰り返し)
void
loop() {
	switch (op) {
		case op_t::connect:
			if (std::all_of(std::begin(sesame_state), std::end(sesame_state),
			                [](auto& state) { return state == SesameClient::state_t::active; })) {
				Serial.println("all connected");
				op = op_t::lock;
				break;
			}
			if (last_operated == 0 || millis() - last_operated > 3'000) {
				count++;
				Serial.println("Connecting...");
				for (size_t i = 0; i < std::size(clients); i++) {
					if (sesame_state[i] == SesameClient::state_t::idle) {
						if (!clients[i].connect(3)) {
							Serial.printf("%u: Failed to connect\n", i);
						}
					}
				}
				last_operated = millis();
			}
			break;
		case op_t::lock:
			if (millis() - last_operated > 5'000) {
				if (std::all_of(std::begin(sesame_status), std::end(sesame_status), [](auto& status) { return status.in_lock(); })) {
					Serial.println("all locked");
					op = op_t::unlock;
					break;
				}
				Serial.println("Locking");
				for (size_t i = 0; i < std::size(clients); i++) {
					if (sesame_status[i].in_lock()) {
						continue;
					}
					if (clients[i].get_model() == Sesame::model_t::sesame_cycle) {  // Sesameサイクルは手動でLockしてください
						Serial.printf("%u: Please Lock Sesame Cycle by hand!\n", i);
					} else {
						clients[i].lock("example multi");
					}
					last_operated = millis();
				}
			}
			break;
		case op_t::unlock:
			if (millis() - last_operated > 5'000) {
				if (std::all_of(std::begin(sesame_status), std::end(sesame_status), [](auto& status) { return status.in_unlock(); })) {
					Serial.println("all unlocked");
					op = op_t::lock;
					break;
				}
				Serial.println("Unlocking");
				for (size_t i = 0; i < std::size(clients); i++) {
					if (sesame_status[i].in_unlock()) {
						continue;
					}
					clients[i].unlock("example multi");
					last_operated = millis();
				}
			}
			break;
		default:
			// nothing todo
			break;
	}
	delay(100);
}
