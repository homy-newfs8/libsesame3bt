# libsesame3bt
SESAME 3/4をBluetooth経由で制御するためのESP32用ライブラリ

## 概要
このライブラリはESP32マイコンで[CANDY HOUSE](https://jp.candyhouse.co/)社製のスマートロックSESAME 3およびSESAME 4を制御するためのライブラリです。Bluetooth LE経由で以下の機能を実行できます。

- SESAMEのスキャン
- SESAME状態の受信
- SESAMEの操作(施錠、開錠)

## 対応機種
以下の機種に対応しています。
- [SESAME 3](https://jp.candyhouse.co/products/sesame3)
- [SESAME 4](https://jp.candyhouse.co/products/sesame4)

## 開発環境
以下のデバイスで開発しました。
- [M5StickC](https://www.switch-science.com/catalog/6350/)

## 使用方法
- 本ライブラリは開発環境[PlatformIO](https://platformio.org/)での利用を前提としています。動作に必要となる外部ライブラリ等については[library.json](library.json)に記述してあります。
- C++17コンパイラの使用が前提となっています。PlatformIO上のESP32向け開発でC++17コンパイラを利用可能にする方法については、ESP32Sesame3Appのplatform.iniを参照願います。

## 関連リポジトリ
- [ESP32Sesame3App](公開予定)
本ライブラリを使ったアプリケーションサンプル
- [libsesame3bt-dev](公開予定)
本ライブラリの開発用リポジトリ。サンプルファイルの実行等が可能です。

## 制限事項
- 本ライブラリはSESAMEデバイスの初期設定を行うことができません。公式アプリで初期設定済みのSESAMEのみ制御可能です。
- ライブラリのドキュメントはありません。[example](example)フォルダに格納されているサンプルプログラム、サンプルアプリである[ESP32Sesame3App](公開予定)、ライブラリ自体のソースコードを参照願います。

## 謝辞
本ライブラリの開発には[pysesameos2](https://github.com/mochipon/pysesameos2)の成果を大いに参考にさせていただきました。感謝します。

困難な状況の中で開発を続けている[PlatformIO](https://platformio.org/)プロジェクトのメンバーに感謝します。
