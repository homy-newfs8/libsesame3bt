# libsesame3bt
SESAME 5(PRO)/3/4/bot/サイクルをBluetooth経由で制御するためのESP32用ライブラリ。SESAME Face(PRO) / SESAME Touch(PRO) / CANDY HOUSE Remote のバッテリー残量も取得可能です。

## 概要
このライブラリはESP32マイコンで[CANDY HOUSE](https://jp.candyhouse.co/)社製のスマートロックSESAME 5、SESAME 5 PRO、SESAME Bot 2、SESAME 4、SESAME 3、SESAME bot、SESAME サイクル等を制御するためのライブラリです。Bluetooth LE経由で以下の機能を実行できます。

- SESAMEのスキャン
- SESAME状態の受信
- SESAMEの操作(施錠、開錠)

## 対応機種
以下の機種に対応しています。
- [SESAME 5](https://jp.candyhouse.co/products/sesame5)
- [SESAME 5 PRO](https://jp.candyhouse.co/products/sesame5-pro)
- [SESAME Bot 2](https://jp.candyhouse.co/products/sesamebot2)
- [SESAME Face](https://jp.candyhouse.co/products/sesame-face)
- [SESAME Face PRO](https://jp.candyhouse.co/products/sesame-face-pro)
- [SESAME Touch](https://jp.candyhouse.co/products/sesame-touch)
- [SESAME Touch PRO](https://jp.candyhouse.co/products/sesame-touch-pro)
- [CANDY HOUSE Remote](https://jp.candyhouse.co/products/candyhouse_remote)
- [SESAME bot](https://jp.candyhouse.co/products/sesame3-bot)
- [SESAME 3](https://jp.candyhouse.co/products/sesame3)
- [SESAME 4](https://jp.candyhouse.co/products/sesame4)
- [SESAME サイクル](https://jp.candyhouse.co/products/sesame3-bike)

[SESAMEサイクル2](https://jp.candyhouse.co/products/sesame-bike-2)は所有していないため対応していません。

## 開発環境
以下のデバイスで開発しました。
- [M5StickC](https://docs.m5stack.com/en/core/m5stickc)
- [Seeed Studio XIAO ESP32C3](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/)

## 使用方法
- 本ライブラリは開発環境[PlatformIO](https://platformio.org/)での利用を前提としています。動作に必要となる外部ライブラリ等については[library.json](library.json)に記述してあります。
- [Arduino Framework](https://github.com/espressif/arduino-esp32) 2.xと3.xに対応しています。コンパイル方法は同梱の[platformio.ini](platformio.ini)を参照願います。

## 関連リポジトリ
- [ESP32Sesame3App](https://github.com/homy-newfs8/ESP32Sesame3App)
本ライブラリを使ったアプリケーションサンプル
- [esphome-sesame](https://github.com/homy-newfs8/esphome-sesame3)
[ESPHome](https://esphome.io/)でSESAMEを使うための外部コンポーネント

## 制限事項
- 本ライブラリはSESAMEデバイスの初期設定を行うことができません。公式アプリで初期設定済みのSESAMEのみ制御可能です。
- ライブラリのドキュメントはありません。[example](example)フォルダに格納されているサンプルプログラム、サンプルアプリである[ESP32Sesame3App](https://github.com/homy-newfs8/ESP32Sesame3App)、ライブラリ自体のソースコードを参照願います。

## 謝辞

本ライブラリの開発には[pysesameos2](https://github.com/mochipon/pysesameos2)の成果を大いに参考にさせていただきました。感謝します。

困難な状況の中で開発を続けている[PlatformIO](https://platformio.org/)プロジェクトのメンバーに感謝します。

オープンソースでSDK[SesameSDK3.0 for Android](https://github.com/CANDY-HOUSE/SesameSDK_Android_with_DemoApp)を公開してくれた[CANDY HOUSE](https://jp.candyhouse.co/)様、ありがとうございます。
