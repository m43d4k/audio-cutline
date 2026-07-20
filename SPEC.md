# SPEC

## Overview

Cutlineは、macOS上でAbleton Liveを主に使用する音楽制作者が、不要な低域または高域を簡潔かつ高品質に除去するためのVST3オーディオプラグインである。

Minimum Phaseのハイパスフィルターとローパスフィルター、出力ゲイン、LR Swap、Filter Bypass、Mono、フィルター応答表示を提供する。一般的なデジタルEQと遜色ないButterworth応答、クリックレスな操作、ゼロレイテンシー、再現可能な状態保存を重視する。

この文書は、プロジェクトの目的、対象範囲、要件、受け入れ条件の正本とする。設計上の選択と理由は`DECISIONS.md`に記録する。

## Goal

このプロジェクトの目的は、1〜8 poleのHP/LPを独立して設定でき、DAWオートメーション中も安定して使用できる軽量なステレオVST3を提供することである。

成功した状態では、利用者がApple Silicon Mac上のAbleton LiveへCutlineを挿入し、カットオフ周波数、傾斜、個別On/Off、出力ゲインを操作またはオートメーションし、表示された応答を確認しながら不要帯域を除去できる。DAWプロジェクトの再読込後も全状態が復元される。

## Non-goals

このプロジェクトでは、次を目的としない。

* バンドゲイン、ピーキング、シェルビング、動的EQを追加する
* Linear Phase、Mixed Phase、オーバーサンプリングを提供する
* スペクトラム、波形、レベルメーターを表示する
* カーブ上の操作点をドラッグ可能にする
* リミッター、クリッパー、自動ゲイン補正を追加する
* モノ入出力バス、M/S、サイドチェイン、マルチチャンネルへ対応する
* Windows、Intel Mac、AU、AAX、Standaloneへ対応する
* 初期版でUIリサイズ、UIスケール、テーマ切替、独自プリセット管理を実装する
* サンプル単位のホストオートメーション精度を保証する
* 署名済みまたは未署名のVST3バイナリ、インストーラーを配布する

## Users

### Primary users

* Apple Silicon MacでAbleton Liveを使用する音楽制作者
* ソースからVST3をビルドする場合は、CMake、Xcode、JUCEの利用資格を用意できる利用者

### Use cases

* 利用者がHPを有効にしてカットオフとPole数を設定し、不要な低域を除去する
* 利用者がLPを有効にしてカットオフとPole数を設定し、不要な高域を除去する
* 利用者がHPとLPを併用し、通過帯域を限定する
* 利用者がOutput Gainで処理後のレベルを-12〜+12 dBの範囲で調整する
* 利用者が全パラメータをAbleton Liveからオートメーションする
* 利用者がDAWプロジェクトを保存、再読込し、以前の状態を復元する

## Scope

### In scope

* Minimum PhaseのButterworth HP/LP
* HP/LPごとのOn/Off、1〜8 pole、20 Hz〜20 kHz
* -12〜+12 dBのOutput Gain
* LR Swap、Filter Bypass、ステレオ入力のMono化
* 全パラメータのVST3オートメーション公開
* ステレオ入出力へ同一設定を適用する処理
* `float`と`double`のホスト処理精度
* 表示専用の合成周波数応答カーブ
* APVTSによる全10パラメータの状態保存、復元、スキーマ移行の基盤
* 任意の有効なホストサンプルレートへの追従
* 0 samplesを含む可変ブロックサイズの安全な処理
* CMakeによるソースビルドとGitHub Actionsでの検証

### Out of scope

* `Non-goals`に列挙した機能と環境
* 正式検証対象外のサンプルレートにおける20 kHz設定の応答精度保証
* 最低対応macOSの表明
* Apple Developer Programを使う署名、公証、バイナリ配布
* 利用者のJUCEライセンス資格の提供または代行

## Requirements

### Functional requirements

#### FR-001: ハイパスフィルター

利用者は、HPを個別にOn/Offし、カットオフ周波数とPole数を設定できる。

受け入れ条件:

* Freq範囲は20 Hz〜20 kHzである
* Pole範囲は1〜8で、1 poleあたりの遮断帯域の漸近傾斜は6 dB/octである
* 初期値はOff、1 pole、20 Hzである
* 各次数についてフィルター全体がButterworth応答となる
* 設定周波数は、表現可能なサンプルレートで全体の約-3.0103 dB点となる

#### FR-002: ローパスフィルター

利用者は、LPを個別にOn/Offし、カットオフ周波数とPole数を設定できる。

受け入れ条件:

* Freq範囲は20 Hz〜20 kHzである
* Pole範囲は1〜8で、1 poleあたりの遮断帯域の漸近傾斜は6 dB/octである
* 初期値はOff、1 pole、20 kHzである
* 各次数についてフィルター全体がButterworth応答となる
* 設定周波数は、表現可能なサンプルレートで全体の約-3.0103 dB点となる

#### FR-003: Output Gain

利用者は、フィルター処理後の出力ゲインを調整できる。

受け入れ条件:

* 範囲は-12〜+12 dBである
* 初期値は0 dBである
* リミッター、クリッパー、自動ゲイン補正を適用しない
* 利用者が正のゲインを設定した結果として0 dBFSを超える出力を許容する

#### FR-004: オートメーションとクリックレス遷移

利用者は、HP On、HP Pole、HP Freq、LP On、LP Pole、LP Freq、Output Gain、LR Swap、Filter Bypass、Monoの全項目をホストからオートメーションできる。

受け入れ条件:

* APVTSからDSPへの目標値更新はブロック単位である
* Freqは対数周波数上で約20 ms平滑化する
* Output Gainは線形振幅上で約10 ms平滑化する
* On/Offはフィルター状態を維持したままDry/Wetを約5 msで線形クロスフェードする
* LR Swap、Filter Bypass、Monoは約5 msで線形クロスフェードする
* Poleは旧／新バンクを約10 msで線形クロスフェードする
* Pole遷移中の再変更は最新値だけを保持し、現在の遷移完了後に適用する
* 追加レイテンシーは0 samplesである
* 通常の単発変更に対するパラメータ追従時間は最大約20 msである

#### FR-005: 状態保存と復元

ホストは、Cutlineの全パラメータとOn/Off状態を保存し、再読込時に復元できる。

受け入れ条件:

* APVTSルート`ValueTree`にスキーマバージョンを保持する
* 一時`ValueTree`へ復元した後、スキーマと値を検証する
* 必要な場合は`replaceState()`の前にマイグレーションする
* 検証に成功した場合だけAPVTSへ反映する
* 不正データまたは未知の新しいスキーマは現在値を壊さず拒否する
* 状態失敗の診断をオーディオスレッドから直接出力しない
* 独自プリセットブラウザやファクトリープリセットを持たない

#### FR-006: 周波数応答表示

利用者は、現在のHPとLPによる合成応答を表示専用グラフで確認できる。

受け入れ条件:

* 横軸は20 Hz〜20 kHzの対数軸である
* 縦軸の初期表示範囲は+6〜-48 dBである
* -48 dB未満の応答カーブは表示しない
* 主要周波数の縦グリッドとラベルを表示する
* 横グリッドは0 dB線だけを表示する
* HP/LPのカットオフ位置へ非操作の識別点を表示する
* カーブはHPとLPの合成応答であり、Output Gainを含めない
* UI側でパラメータとサンプルレートから計算し、音声バッファやDSP履歴を参照しない

#### FR-007: UI操作

利用者は、固定サイズのダークUIから全パラメータを操作できる。

受け入れ条件:

* 初期サイズは720×420 pxである
* 上段約60%を応答グラフ、下段をHP、LP、Outputの3区画とし、LR Swap、Filter Bypass、Monoを上部へ配置する
* HP/LPはOn/Off、Pole選択、Freqノブ、編集可能な数値欄を持ち、Pole選択は6〜48 dB/Octで表示する
* OutputはGainノブと編集可能な数値欄を持つ
* Filter Bypass中の応答グラフはフラット表示とし、HP/LPの識別点を表示しない
* 数値欄は`Hz`、`kHz`、`dB`の表示と、単位付き／単位なし入力に対応する
* 範囲外入力は確定時に対応パラメータの範囲へ制限する
* 初期版はリサイズ不可で、テーマ切替を持たない

#### FR-008: サンプルレートとブロックサイズ

Cutlineは、ホストから渡された任意の有効なサンプルレートと可変ブロックサイズを安全に処理する。

受け入れ条件:

* サンプルレートを列挙して処理可否を制限しない
* 設定周波数が表現不能な場合は、パラメータ値を維持したまま内部カットオフをNyquist直下へ制限する
* 0 samplesのブロックを安全に処理する
* `processBlock()`内でブロックサイズに応じた再確保を行わない
* 無効なサンプルレートはdebug assertの対象とし、リリース版では安全なドライ通過とする

#### FR-009: ステレオユーティリティ

利用者は、ステレオ信号の左右交換、フィルター全体のバイパス、モノ化を個別にOn/Offできる。

受け入れ条件:

* LR Swapは左入力を右出力、右入力を左出力へ送る
* Filter BypassはHP/LP処理だけをバイパスし、Output Gain、LR Swap、Monoは適用する
* Filter Bypass中もHP/LPの内部状態を更新し続ける
* Monoはステレオ信号の算術平均`(L + R) / 2`を左右へ出力する
* 信号順序はHP/LP、Filter Bypass、Output Gain、LR Swap、Monoとする
* 3項目の初期値はOffである
* Monoはステレオ入出力バスを維持し、モノ入出力バスを追加しない

### Non-functional requirements

#### NFR-001: 音質と数値精度

* Minimum PhaseのデジタルButterworth応答を生成する
* カットオフ点は-3.0103 dBに対して±0.1 dB以内とする
* 同一サンプルレート、同一デジタル化方式の基準応答が-120 dB以上の測定点では、誤差を±0.1 dB以内とする
* 基準応答が-120 dB未満の領域では、十分な減衰、有限性、安定性によって判定する
* 係数量子化後の全極が単位円内に存在する
* 無音、最大振幅、DC、インパルス、境界設定でNaN、Inf、発振を生じない

#### NFR-002: リアルタイム安全性

* オーディオスレッドでメモリ確保、ロック、ファイルI/O、ログ出力を行わない
* `getRawParameterValue()`で取得したatomicポインタを初期化時から保持する
* パラメータID検索をブロック処理内で行わない
* 最大段数、遷移バンク、作業領域を準備時に確保する
* 非正規化数による処理負荷増加を防ぐ
* ホストへ報告する追加レイテンシーは0 samplesとする

#### NFR-003: 性能

* 最大8 poleのHPとLPを有効にしても、一般的なApple Silicon環境で軽量に動作する
* Pole遷移中の並列バンクを含む最悪条件をベンチマークする
* 係数更新は最大16 samples単位の固定小区間とし、音質試験とCPU測定の両方で確認する

#### NFR-004: 保守性

* DSP層をJUCEのGUI部品とAPVTSから分離し、`float`と`double`で単体テスト可能にする
* UI、Processor、DSP、状態検証の責務を分離する
* パラメータIDと状態スキーマをリリース後に安易に変更しない
* 外部依存をバージョンまたは完全SHAへ固定する

#### NFR-005: 互換性

* Apple SiliconのmacOS VST3として動作する
* ステレオ入出力だけを受け入れる
* Ableton Liveを主な手動検証ホストとする
* `float`と`double`のホスト処理精度へ対応する
* 正式検証対象サンプルレートは44.1、48、88.2、96、176.4、192 kHzとする

## User flow

1. 利用者がCutlineをステレオトラックへ挿入する
2. 必要なHPまたはLPをOnにする
3. Pole数とカットオフ周波数をノブ、選択部品、数値入力で設定する
4. 応答カーブで通過帯域と遮断帯域を確認する
5. 必要に応じてOutput Gain、LR Swap、Filter Bypass、Monoを操作する
6. 必要に応じてDAWから各パラメータをオートメーションする
7. DAWプロジェクトを保存し、次回の読込時に状態を復元する

例外時:

* 範囲外の数値入力は確定時に有効範囲へ制限する
* 20 kHzを表現できないサンプルレートでは内部カットオフだけをNyquist直下へ制限する
* 不正または未知の状態データは現在値を維持したまま拒否する
* 無効なサンプルレートでは、リリース版はドライ信号を安全に通過させる
* ステレオ以外のバス構成は受け入れない

## Data

### Inputs

* ステレオ音声: ホストから渡される`float`または`double`のオーディオブロック
* パラメータ目標値: APVTSのatomic値をブロック開始時に取得
* サンプルレートとブロック情報: ホストのprepare／processコールから取得
* 保存状態: ホストから渡されるシリアライズ済みAPVTS状態

### Outputs

* 処理済みステレオ音声: HP、LP、Filter Bypass、Output Gain、LR Swap、Monoを順に適用した音声ブロック
* 保存状態: スキーマバージョンと全パラメータを含むシリアライズデータ
* 応答表示: UI側で計算したHP/LP合成応答

### Persistence

* APVTSへ10個のパラメータとOn/Off状態を保存する
* APVTSルート`ValueTree`へスキーマバージョンを保存する
* 保存と復元はホストの状態API経由で行う
* 音声履歴、波形、メーター値、独自プリセットは永続化しない

## Interfaces

### User interface

* 固定サイズの単一Editor
* HP、LP、Outputの操作区画とLR Swap、Filter Bypass、Monoのトグル
* HP/LP合成応答グラフ
* APVTS Attachmentを介したノブ、数値欄、On/Off、Pole選択、ユーティリティ切替

### External interfaces

* VST3ホスト: ステレオ音声、パラメータ、オートメーション、状態保存／復元
* CMake: Cutline VST3とテストのconfigure、build、実行
* JUCE 8.0.14: VST3ラッパー、APVTS、GUI、プラットフォーム統合
* pluginval 1.0.4: 生成したVST3を独立プロセスとして検証

公開済みのパラメータID、状態スキーマ、VST3識別子を変更する場合は、互換性への影響を確認し、`DECISIONS.md`へ記録する。

## Technical constraints

* 対応OS: macOS。最低対応バージョンは未規定
* 対象アーキテクチャ: Apple Silicon / arm64
* 検証済みローカル環境: macOS 15.7.7、Xcode 26.3
* プラグイン形式: VST3
* チャンネル構成: ステレオ入出力のみ
* 使用言語: C++17以上
* フレームワーク: JUCE 8.0.14
* JUCE完全SHA: `2cdfca8feb300fb424002ba2c2751569e5bacb64`
* ビルドシステム: CMake 3.22以上
* JUCE取得: `JUCE_DIR`を優先し、未指定時だけ`FetchContent`
* CI: GitHub Actionsの`macos-15` arm64 runnerでXcode 26.3を明示選択
* pluginval: v1.0.4、完全SHA`ed19c2c16b57a6d94db391bea3ef4a80b769d5bf`
* ライセンス: 自作コードはMIT License、JUCEはJUCE Starter EULA
* 配布方式: GitHubでソースコードだけを公開
* Projucer、バイナリ配布、インストーラー、コード署名、公証を使用しない

## Constraints

* ユーザーが指定した範囲を超えて機能を追加しない
* 入力されていない要件を確定事項として補わない
* 音質要件を実装しやすい別特性へ置き換えない
* JUCE自体をCutlineのMIT License対象へ含めない
* プロジェクト所有者はJUCE Starter EULAの資格を継続確認する
* Starter条件を超えた場合は、適切なJUCEライセンスへ移行するか、新規開発と配布を停止する
* 公開時に最新のJUCE EULAを再確認する
* CIで生成したVST3を配布成果物として公開しない
* オーディオスレッドの異常を暗黙のメモリ確保やロックで回避しない

## Acceptance criteria

プロジェクトは、次の条件をすべて満たした場合に受け入れ可能とする。

* Apple Silicon MacのAbleton LiveでステレオVST3として認識される
* 10個のパラメータをUIとホストオートメーションから操作できる
* 各1〜8 poleのHP/LPが指定したデジタルButterworth基準を満たす
* 正式6サンプルレートと`float` / `double`でDSP検証に合格する
* 量子化後の係数が、最低、中間、最高、Nyquist制限直下の周波数で安定する
* 急激なパラメータ変更でクリック、NaN、Inf、発振を生じない
* 低周波・8 poleのPole遷移が実用上受容可能である
* オーディオスレッドでメモリ確保、ロック、ファイルI/O、ログ出力を行わない
* 追加レイテンシーが0 samplesである
* 0 samplesを含む可変ブロックサイズを安全に処理する
* DAWプロジェクトの保存と再読込で全状態が復元される
* 不正状態を現在値を壊さず拒否する
* pluginvalとVST3適合検証に合格する
* CMakeビルドと全自動テストが成功する
* README、ライセンス文書、実際の挙動が一致する
* ビルド済みVST3が公開成果物に含まれない

## Rejection criteria

次のいずれかに該当する場合は不合格とする。

* 同一Qの2 poleセクションを単純反復し、全体Butterworth応答になっていない
* アナログ理論式だけと比較し、同一条件のデジタル基準応答を検証していない
* 理論値が-120 dB未満の領域を無制限にdB誤差比較し、数値誤差を応答不良と誤判定する
* `float`または`double`の一方で係数が不安定になる
* Pole、On/Off、Freq、Gain、LR Swap、Filter Bypass、Mono変更で実用上明確なクリックや発振が生じる
* `processBlock()`内でメモリ確保、ロック、パラメータID検索、ログ出力を行う
* 平滑化時間をプラグインレイテンシーとして報告する
* ステレオ以外を未検証のまま対応済みと扱う
* 不正な状態データで現在のAPVTS状態を破壊する
* Output Gainを応答カーブへ含め、0 dBのフィルター基準を変化させる
* 最低対応macOSを検証なしに表明する
* 未署名または署名済みVST3バイナリをGitHubリリースへ掲載する
* JUCEをCutlineのMIT License対象として扱う
* 対象範囲外の機能を必須機能として追加する

## Verification

### Automated verification

実装後、少なくとも次のコマンド経路を提供する。preset名やbuild directoryはCMake実装時に確定し、READMEとCIで同じ経路を使用する。

* Configure: `cmake -S . -B build`
* Build: `cmake --build build --config Release --parallel`
* Tests: `ctest --test-dir build -C Release --output-on-failure`
* VST3検証: 固定したpluginval v1.0.4を生成済みCutline VST3へ実行する

自動検証は次を含む。

* HP/LP、1〜8 pole、正式6サンプルレート、`float` / `double`の応答試験
* 最低、中間、最高、Nyquist制限直下における量子化後の極検査
* インパルス、無音、最大振幅、DC、急激なオートメーションの有限性と安定性
* 0 samplesを含む可変ブロックサイズ
* LR Swap、Filter Bypass、Monoの信号経路と切替遷移
* APVTS状態の往復、不正状態、旧スキーマ、未知スキーマ
* ステレオ以外のバス拒否
* オーディオスレッド内のメモリ確保検出
* pluginvalとVST3 Validator

### Manual verification

1. macOS 15.7.7、Xcode 26.3、Apple Silicon環境でRelease VST3をビルドする
2. Ableton LiveでCutlineが認識され、ステレオトラックへ挿入できることを確認する
3. 全パラメータをUIとAbleton Liveオートメーションから操作する
4. 応答カーブ、数値入力、範囲制限、On/Off表示を確認する
5. 低周波・8 poleを含むPole切替、Freq掃引、On/Off、Gain、LR Swap、Filter Bypass、Mono変更を聴感確認する
6. プロジェクト保存、Live終了、再起動、再読込後の完全な状態復元を確認する
7. Editorを繰り返し開閉し、表示、状態、音声処理を確認する
8. 正のOutput Gainでクリップ保護が暗黙に追加されていないことを確認する

### Comparison

* Cutline実測応答と同一サンプルレート・同一デジタル化方式の基準Butterworth応答
* `float`処理と`double`処理の安定性および許容誤差
* UI表示応答とDSP係数から算出した応答
* 保存前と復元後の全パラメータ値
* 通常処理時とPoleクロスフェード中のCPU負荷

## Open questions

* VST3のmanufacturer名、manufacturer code、plugin code、bundle identifier
* Cutlineの初期リリースバージョン番号
* Nyquist直下へ制限する具体的な安全マージン
* CIで使用するpluginval v1.0.4 macOSアセットのURLとSHA-256
* Ableton Liveの正式な手動検証バージョン

これらは実装または初回リリース準備までに確定する。未確定事項を要件や対応済み事項として扱わない。

## References

* `DECISIONS.md`: 設計判断と採否理由
* JUCE 8.0.14 release: <https://github.com/juce-framework/JUCE/releases/tag/8.0.14>
* JUCE 8 EULA: <https://juce.com/legal/juce-8-licence/>
* JUCE repository: <https://github.com/juce-framework/JUCE>
* pluginval v1.0.4: <https://github.com/Tracktion/pluginval/releases/tag/v1.0.4>
* GitHub-hosted runners: <https://docs.github.com/en/actions/reference/runners/github-hosted-runners>
