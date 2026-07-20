# Cutline

Apple Silicon Mac上のAbleton Liveを主な対象とする、ステレオVST3フィルター
プラグインである。1〜8 poleのMinimum Phase Butterworthハイパス／ローパス、
-12〜+12 dBの出力ゲイン、ステレオユーティリティを提供する。

## 機能

* HP／LPごとのOn/Off、20 Hz〜20 kHz、1〜8 pole
* LR Swap、Filter Bypass、ステレオ入力のMono化
* 周波数と出力ゲインの平滑化
* Dry/WetとPole変更のクロスフェード
* float／double処理、0 samplesを含む可変ブロック、0 samplesレイテンシー
* APVTSによる全10パラメータのオートメーションと検証付き状態復元
* 20 Hz〜20 kHzの合成応答を表示する固定サイズのダークUI

スペクトラム、Linear Phase、オーバーサンプリング、モノ入出力バス／M/S、AU、Windowsには
対応しない。

## 必要環境

* Apple Silicon MacとXcode（Command Line Toolsを含む）
* CMake 3.22以上
* C++17対応コンパイラ
* Ninja（下記の例で使用）
* JUCE 8を利用するための適切なライセンス資格

依存するJUCEは8.0.14のcommit
`2cdfca8feb300fb424002ba2c2751569e5bacb64`へ固定している。既存のJUCE
checkoutを指定しない場合、CMakeがconfigure時に取得する。

## ビルド

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --target Cutline_VST3 -j 6
```

既存のJUCE checkoutを使う場合は、最初のコマンドへ
`-DJUCE_DIR=/absolute/path/to/JUCE`を追加する。生成物は
`build/Cutline_artefacts/Release/VST3/Cutline.vst3`である。

## インストール

生成したVST3をローカルユーザー用VST3フォルダーへコピーする。

```sh
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
cp -R build/Cutline_artefacts/Release/VST3/Cutline.vst3 \
  "$HOME/Library/Audio/Plug-Ins/VST3/"
```

Ableton LiveでVST3を再スキャンした後、ステレオトラックへ挿入する。

リポジトリではVST3バイナリ、署名、公証、インストーラーを配布しない。

## テスト

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build -j 6
ctest --test-dir build --output-on-failure
```

DSPテストは全正式サンプルレート、1〜8 pole、HP／LPのカットオフ精度、傾斜、
極の安定性、float／double、境界入力を確認する。状態・Processorテストは不正状態
の拒否、状態往復、ステレオ限定、UI描画などを確認する。

最悪条件の簡易CPU測定:

```sh
cmake --build build --target CutlineDspBenchmark
./build/CutlineDspBenchmark
```

CIでは固定commitのpluginval 1.0.4をstrictness level 10で実行する。Ableton Live
での認識、オートメーション、保存復元、聴感、UI再表示はリリース前の手動確認項目である。

## ドキュメント

* [SPEC.md](SPEC.md): 目的、対象範囲、要件、受け入れ条件
* [DECISIONS.md](DECISIONS.md): 設計上の選択と採用理由

## ライセンス

Cutlineの自作コードには[MIT License](LICENSE)を適用する。JUCEと検証ツールには
個別のライセンスが適用される。詳細は
[THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES)を参照する。

## 制作者

[m43d4k](https://github.com/m43d4k)
