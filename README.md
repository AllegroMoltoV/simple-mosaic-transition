# シンプルモザイクトランジション / Simple Mosaic Transition

OBS Studio 向けの、画面のモザイクがだんだん粗くなって次のシーンへ切り替わる、シンプルなシーントランジションのプラグインです。

A simple scene transition plugin for OBS Studio: the picture breaks up into ever coarser mosaic blocks, switches to the next scene, and then sharpens back.

[配信プラグイン倉庫くん](https://soko-kun.allegromoltov.jp/plugins/allegromoltov/simple-mosaic-transition) にも掲載しています。 / Also listed on [Plugin Soko-kun](https://soko-kun.allegromoltov.jp/plugins/allegromoltov/simple-mosaic-transition?lang=en).

## Features / 機能

ゲームで気を失うときのような演出のシーントランジションです。前のシーンのモザイクがだんだん粗くなり、最も粗い瞬間に次のシーンへ替わり、次のシーンのモザイクがだんだん細かくなって元に戻ります。途中で黒へ暗転させることも、暗転させずにモザイクのまま切り替えることもできます。描画はすべて GPU のシェーダーで行い、1920×1080 で最も粗い瞬間のモザイクの GPU 時間は 1 フレームあたり約 0.01〜0.03 ms です (RTX 3060 Ti で測定)。

A scene transition that looks like passing out in a game. The previous scene breaks up into ever coarser mosaic blocks, switches to the next scene at the coarsest moment, and the next scene sharpens back. It can fade through black on the way, or switch without black. All drawing runs in GPU shaders; the mosaic takes about 0.01–0.03 ms of GPU time per frame at the coarsest moment in 1920×1080 (measured on an RTX 3060 Ti).

- **Black portion length・暗転部分の長さ** - 黒へ暗転する部分の長さ。0% で暗転しない
- **Maximum mosaic size・モザイクの最大の粗さ** - 最も粗い瞬間のモザイクの 1 マスの一辺
- **Switch point・切り替えの位置** - 前のシーンから次のシーンへ替わる位置

トランジションの速さは、OBS Studio のシーントランジションの「期間」で決まります。

The speed of the transition follows the Duration of the scene transition in OBS Studio.

## Installation / インストール

### Windows

Windows 版は OBS Studio 33 以降向けです。

#### 通常版 OBS への導入

プラグインを OBS Studio の共通プラグインフォルダーへ配置します。

1. OBS Studio を終了してください。
2. [Releases](../../releases) から Windows 用の zip ファイルをダウンロードし、展開してください。
3. エクスプローラーのアドレスバーへ `%PROGRAMDATA%` を入力し、Enter キーを押してください。隠しフォルダーの表示設定を変更する必要はありません。
4. `obs-studio` フォルダーを開き、その中の `plugins` フォルダーを開いてください。存在しないフォルダーは順に作成してください。
5. 展開した `simple-mosaic-transition` フォルダーを `plugins` へコピーしてください。管理者の許可を求められた場合は許可してください。
6. OBS Studio を起動してください。

導入後の構成を次に示します。`simple-mosaic-transition` フォルダーを二重に作らないでください。PDB ファイルが同梱されている場合も、製品フォルダーごとコピーします。

```text
%PROGRAMDATA%\obs-studio\plugins\
└─ simple-mosaic-transition\
   ├─ simple-mosaic-transition.dll
   └─ data\
      ├─ effects\mosaic-transition.effect
      └─ locale\
         ├─ en-US.ini
         └─ ja-JP.ini
```

更新するときは、OBS Studio を終了し、同じ手順で `simple-mosaic-transition` フォルダーを上書きコピーしてください。設定やシーンは削除しないでください。

#### ポータブル版 OBS

ポータブル版では、OBS Studio のルートフォルダーにある `plugins` へ製品フォルダーをコピーします。通常版の `%PROGRAMDATA%` への導入では読み込まれません。

OBS Studio を終了し、`bin` や `data` と同じ階層に `plugins` フォルダーを作成してください。展開した製品フォルダーをコピーし、`plugins\simple-mosaic-transition\simple-mosaic-transition.dll` と `plugins\simple-mosaic-transition\data` を配置してから OBS Studio を起動します。

### macOS / Linux

macOS / Linux 向けのビルドは GitHub Actions で生成しています。GitHub Actions 上の Ubuntu 24.04 (Mesa の OpenGL) と macOS 15 (OpenGL) で、トランジションの描画を自動で検証しています。作者は macOS と Linux の実機を持たないため、実機の GPU と OBS Studio では動作を確認していません。macOS の OBS Studio 32 以降で選べる試験的な Metal 描画も確認していません。ソースからのビルドは「Build / ビルド」を参照してください。

## Usage / 使い方

1. OBS Studio の **シーントランジション** の **+** ボタン → **シンプルモザイクトランジション** (または **Simple Mosaic Transition**) を追加
2. 追加したトランジションを選び、**期間** で速さを決める
3. 歯車のボタン → **プロパティ** で、暗転部分の長さ・モザイクの最大の粗さ・切り替えの位置を調整する
4. シーンを切り替える

特定のシーンへ切り替えるときだけ使うには、シーンを右クリック → **トランジションの上書き** でこのトランジションを選びます。

1. Click **+** under **Scene Transitions** and add **Simple Mosaic Transition**.
2. Select it and set the speed with **Duration**.
3. Open the gear button → **Properties** to adjust the black portion length, the maximum mosaic size, and the switch point.
4. Switch scenes.

To use it only when switching to a particular scene, right-click the scene and choose it under **Transition Override**.

## Parameters / パラメータ

| Parameter | 日本語 | Range | Default |
|-----------|--------|-------|---------|
| Black portion length | 暗転部分の長さ | 0 ~ 100 % | 50 % |
| Maximum mosaic size | モザイクの最大の粗さ | 8 ~ 256 px | 64 px |
| Switch point | 切り替えの位置 | 0 ~ 100 % | 50 % |

暗転部分の長さは、モザイクが粗くなる時間と細かく戻る時間のそれぞれに対する割合です。50% では、粗くなる時間の後ろ半分で黒へ暗転し、細かく戻る時間の前半分で黒から戻ります。0% では暗転せず、最も粗いモザイクのまま次のシーンへ替わります。100% では、トランジションの全体で暗転します。

切り替えの位置は、トランジションの期間に対する割合です。50% では、期間の前半で前のシーンが粗くなり、後半で次のシーンが細かく戻ります。

The black portion length is a share of the time the mosaic coarsens, and of the time it sharpens back. At 50%, the picture fades to black during the second half of the coarsening and comes back during the first half of the sharpening. At 0%, the scene switches at the coarsest mosaic without black. At 100%, the whole transition fades through black.

The switch point is a share of the transition duration. At 50%, the previous scene coarsens during the first half and the next scene sharpens back during the second half.

## Known Behavior / 既知の挙動

- モザイクのマスの区切りは、画面の左上を起点にします。画面の右端と下端で欠けたマスは、欠けた大きさの中で平均します。
- 音声は、OBS Studio 標準の「色へフェード」と同じく、切り替えの位置までに前のシーンの音を下げ、切り替えの位置から次のシーンの音を上げます。
- トランジションの最後の瞬間は、OBS Studio が次のシーンをそのまま描きます。

## Build / ビルド

### Requirements / 必要環境

| Platform | Tool |
|----------|------|
| Windows | Visual Studio 17 2022 |
| Windows | CMake 3.28+ |
| macOS | Xcode 16.0, CMake 3.28+ |
| Ubuntu 24.04 | ninja-build, pkg-config, build-essential, CMake 3.28+ |

### Build Commands / ビルドコマンド

```bash
# Windows
cmake --preset windows-x64 -DENABLE_POST_BUILD_INSTALL=OFF
cmake --build build_x64 --config RelWithDebInfo

# macOS
cmake --preset macos
cmake --build build_macos --config RelWithDebInfo

# Ubuntu
cmake --preset ubuntu-x86_64
cmake --build build_x86_64 --config RelWithDebInfo
```

Windows で生成物を取り出すには、`cmake --install build_x64 --config RelWithDebInfo --prefix release/RelWithDebInfo` を実行してください。生成した製品フォルダーを前述の手順で導入してください。

Windows では、描画の統合テストを Direct3D 11 と OpenGL の両方で CTest に登録しています。OBS のグラフィックスの DLL を `cmake --build .deps/obs-studio-31.1.1/build_x64 --config Release --target libobs-d3d11 libobs-opengl` で作ってから、`ctest --test-dir build_x64 -C RelWithDebInfo` を実行してください。

OBS Studio の実機で配信者の使い方に沿って確かめるシナリオテストの設計と手順は、[docs/scenario-test.md](docs/scenario-test.md) にあります。

## AI利用開示 / AI Disclosure

### 日本語

v1.0.0 の制作では、Anthropic Claude Code を、調査、ソースコード、ビルド設定、テスト、文書を含むほぼすべての工程で使用しました。ロゴは、Claude Code が作成したスクリプトで生成した 3 つの案から、作者本人が選びました。

コード読解による厳密な人間レビューは実施していません。

### English

Anthropic Claude Code was used for almost the entire development process of v1.0.0, including research, source code, build configuration, tests, and documentation. The logo was chosen by the author from three candidates generated by a script that Claude Code wrote.

No rigorous human review of the source code was performed.

## License / ライセンス

GPL-2.0 License

このプラグインは [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) をベースにしています。モザイクのシェーダーと構成は、作者の [シンプルモザイク](https://github.com/AllegroMoltoV/simple-mosaic) から流用しています。

## Author / 作者

AllegroMoltoV - https://www.allegromoltov.jp

## Changelog / 更新情報

### v1.0.0 (未リリース)

- 最初の公開版です。
