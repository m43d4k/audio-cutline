# AGENTS.md

## Project

* 目的: Apple Silicon macOS向けのステレオVST3フィルタープラグインCutlineを提供する
* 要件と受け入れ条件: `SPEC.md`
* 設計判断: `DECISIONS.md`

## Repo layout

* JUCE ProcessorとUI: `Source/PluginProcessor.*`、`Source/PluginEditor.*`
* JUCE非依存DSP: `Source/DSP/`
* 状態検証: `Source/State/`
* 自動テストとベンチマーク: `Tests/`
* ビルド定義: `CMakeLists.txt`

## Commands

* Configure: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64`
* Build: `cmake --build build --target Cutline_VST3 -j 6`
* Test: `ctest --test-dir build --output-on-failure`
* Benchmark: `cmake --build build --target CutlineDspBenchmark && ./build/CutlineDspBenchmark`

`JUCE_DIR=/absolute/path/to/JUCE`ではなく、configure時に
`-DJUCE_DIR=/absolute/path/to/JUCE`を渡すと既存checkoutを使用できる。

## Constraints

* 対象はApple Silicon macOS、VST3、ステレオ入出力に限定する
* `Source/DSP/`はJUCE GUI部品とAPVTSへ依存させない
* オーディオスレッドで確保、ロック、I/O、ログ出力を行わない
* パラメータID、VST3識別子、状態スキーマを互換性確認なしに変更しない
* ビルド済みVST3、署名済み成果物、インストーラーをリポジトリへ追加しない

## Design decisions

作業前に`SPEC.md`と`DECISIONS.md`を確認する。将来の互換性や公開インターフェースに
影響する判断を確定した場合は、同じタスク内で`DECISIONS.md`へ追記する。

## Change expectations

* 振る舞いの変更には関連テストを追加または更新する
* 関係のないリファクタリングや整形を混ぜない
* コミットは明示的に依頼された場合だけ行う

## Done when

* `SPEC.md`の該当する受け入れ条件を満たす
* DebugとReleaseの関連ビルド、テストが成功する
* ホスト固有の項目はpluginvalとAbleton Liveの手動確認事項として明示する
