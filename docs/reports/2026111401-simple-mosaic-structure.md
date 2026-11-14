# simple-mosaic の構成の一覧

調査日は 2026-11-14 です。対象はローカルの `D:/work/obs-simple-mosaic` と、GitHub の `AllegroMoltoV/simple-mosaic` です。ローカルへの書き込みは行っていません。GitHub と MCP には参照の操作だけを行いました。

## 0. 先に伝えること

- ローカルの写しは古い状態です。ローカルの `main` と `origin/main` は `9d499e4` (v1.0.1) を指します。`git ls-remote origin` では、リモートの `main` は `a8a5d69` (v1.0.2) です。差分は PR #5 と PR #6 の 4 コミットで、`.soko-kun/config.json` の追加、`README.md` の題名と更新情報、`buildspec.json` の版の 3 ファイルです。
- `.soko-kun/config.json` はローカルにはありません。全文は GitHub の API で取得しました。6 節に載せています。
- リポジトリ名は、作成時の `obs-simple-mosaic` から `simple-mosaic` へ変わっています。GitHub の API は `full_name: AllegroMoltoV/simple-mosaic` を返し、ローカルの `git remote -v` も `simple-mosaic.git` です。Beads の永続メモリ `repo-github` は旧名の `obs-simple-mosaic` のままです。改名の日時と経緯の記録は見つかりませんでした。
- 基盤のファイルは、drop-shadow と burst-lines から `git checkout <commit> -- <パス>` で取り出したものです。mosaic の git 履歴には obs-plugintemplate の上流コミットがありません。

## 1. ディレクトリ構成

### git 管理下のファイル

ローカルの `git ls-files` (HEAD `9d499e4`) の結果です。`.deps/`、`build_x64/`、`cmake/.CMakeBuildNumber` は `.gitignore` で除外されており、一覧に出ません。

```text
.clang-format
.gersemirc
.gitignore
.github/
  actions/  build-plugin, check-changes, package-plugin, run-clang-format, run-gersemi, setup-macos-codesigning (各 action.yaml)
  scripts/  .Aptfile, .Brewfile, Build-Windows.ps1, Package-Windows.ps1, build-macos, build-ubuntu, package-macos, package-ubuntu
            utils.pwsh/ Ensure-Location.ps1, Invoke-External.ps1, Logger.ps1
            utils.zsh/  check_macos, check_ubuntu, log_*, mkcd, set_loglevel, setup_ubuntu
  workflows/ build-project.yaml, check-format.yaml, dispatch.yaml, pr-pull.yaml, push.yaml, render-test.yaml
.soko-kun/  image.gif, logo.png   (リモートの main には config.json もある)
CMakeLists.txt
CMakePresets.json
LICENSE
README.md
build-aux/  .functions/log_*, set_loglevel, .run-format.zsh, run-clang-format, run-gersemi, run-swift-format
buildspec.json
cmake/
  common/   bootstrap, buildnumber, buildspec_common, ccache, compiler_common, helpers_common, osconfig (.cmake)
  linux/    compilerconfig, defaults, helpers (.cmake)
  macos/    buildspec, compilerconfig, defaults, helpers, xcode (.cmake), resources/ (ccache-launcher-c.in ほか 5 件)
  windows/  buildspec, compilerconfig, defaults, helpers (.cmake), resources/resource.rc.in
data/
  effects/mosaic.effect
  locale/en-US.ini, ja-JP.ini
docs/
  INDEX.md
  images/logo.png, logo.svg, obs-simple-mosaic.gif
  logs/2026-11-11-render-tests-and-gpu-time.md
  reports/2026-11-11-drop-shadow-survey.md
src/
  mosaic-filter.cpp, plugin-main.cpp, plugin-support.c.in, plugin-support.h
tests/
  benchmark.cpp, integration-test.cpp, mosaic-fixture.hpp, obs-test-env.hpp
```

### git 管理外のローカル領域

`git status --short --ignored` の結果では、`.appendix/`、`.beads/`、`.beads.gate.lock`、`.deps/`、`.logs/`、`.prompts/`、`.tmp/`、`build_x64/`、`cmake/.CMakeBuildNumber` が除外されています。

- `.prompts/`: `INIT.md`、`QUESTIONS.md`、`MANUALS/2026111101-usability-check.md`、`MANUALS/2026111102-gif-recording.md`、空の `PLANS/` と `DISCUSSIONS/`
- `.appendix/`: `20261111/2026111101-distribution-conventions.md` (配布規約の調査)、`BLOGS/2026111101-obs-simple-mosaic.md`
- `.logs/`: 27 件。8 節で扱います
- `.tmp/`: テストの書き出し画像、CI の描画の成果物、ruleset の JSON 4 件、配布 ZIP の検証、リリースノートの下書き

### .gitignore

`.gitignore` の 1-20 行は、`/*` ですべてを除外してから `!/.github !/.soko-kun !/docs !/build-aux !/cmake !/data !/src !/tests` と主要ファイルを戻す形です。新しいトップレベルのディレクトリを追跡するには `!/` の行を足す必要があります。31 行に `.docs` があります。

## 2. ビルド系

### 由来と版

- 事実: `cmake/`、`build-aux/`、`.github/` のうち、drop-shadow (`D:/work/obs-simple-drop-shadow`) と内容が異なるのは、`cmake/windows/defaults.cmake`、`.github/scripts/Build-Windows.ps1`、`.github/workflows/{build-project,push,render-test}.yaml` だけです。`diff -rq` で確認しました。`.clang-format`、`.gersemirc`、`.gitignore`、`LICENSE`、`CMakePresets.json`、`src/plugin-support.c.in`、`tests/obs-test-env.hpp` は drop-shadow と一致します。
- 事実: drop-shadow の `git log -- cmake .github build-aux CMakePresets.json` で最も新しい上流コミットは `7540ffa` (2025-09-30、Kaito Udagawa「CI: Add libsimde-dev for OBS 32 on Ubuntu」) です。その前は `942b4ab` (2025-07-14、PatTheMav「buildspec: Update build specification for OBS Studio 31.1.1 support」) です。
- 推測: mosaic の基盤は obs-plugintemplate の上流コミット `7540ffa` の時点に相当します。obs-plugintemplate にはタグの版が無いため、版はコミットで特定するしかありません。上流のリポジトリそのものとの照合は行っていないため未確認です。
- 事実: Beads の `obs-simple-mosaic-08e.2` の完了理由によると、基盤は burst-lines `774288e` と drop-shadow `8e82cb0` から `git checkout` で取り出し、`git ls-files -s` で実行ビット 100755 が 5 件、シンボリックリンク 120000 が 3 件あることを照合しています。

### buildspec.json

ローカルの `buildspec.json` の全 45 行の要点です。

| 項目 | 値 | 行 |
| --- | --- | --- |
| obs-studio | `31.1.1`、ハッシュは macos と windows-x64 | 3-11 |
| prebuilt (obs-deps) | `2025-07-11` | 12-20 |
| qt6 | `2025-07-11`、windows-x64 の debugSymbols のハッシュあり | 21-33 |
| macOS bundleId | `com.example.plugintemplate-for-obs`、テンプレートの値のまま | 36 |
| name | `simple-mosaic` | 39 |
| displayName | `シンプルモザイク / Simple Mosaic` | 40 |
| version | ローカルは `1.0.1`、リモートの `main` は `1.0.2` | 41 |
| author、website、email | `AllegroMoltoV`、`https://www.allegromoltov.jp`、`contact@allegromoltov.jp` | 42-44 |

`name` がプロジェクト名、DLL 名、導入先のフォルダー名になります。`CMakeLists.txt:5` の `project(${_name} VERSION ${_version} ...)` と `cmake/common/bootstrap.cmake` の読み込みによるもので、後者の行は drop-shadow の調査レポート `docs/reports/2026-11-11-drop-shadow-survey.md:62` の記述にもとづきます。

### CMakeLists.txt

全 132 行です。

- 1 行: `cmake_minimum_required(VERSION 3.28...3.30)`
- 7-8 行: `ENABLE_FRONTEND_API` と `ENABLE_QT` は既定で OFF
- 14-20 行: `add_library(... MODULE)`、C++17、`find_package(libobs)`
- 41-56 行: `src/*.c|*.cpp|*.h` と `data/*.effect`、`data/locale/*.ini` を `file(GLOB ... CONFIGURE_DEPENDS)` で拾う
- 59 行: `source_group(TREE ...)` で Visual Studio のツリーを実ディレクトリにそろえる
- 61 行: `set_target_properties_plugin(... OUTPUT_NAME ${_name})`
- 66-79 行: `ENABLE_RENDER_TESTS` (既定 OFF)。Windows では常に、macOS と Linux では ON のときだけ `mosaic-integration-test` と `mosaic-benchmark` をビルドする。Linux では `X11::X11` をリンクする
- 81-118 行: Windows だけ `enable_testing()` と CTest の登録。4 節で扱います
- 122-132 行: `ENABLE_POST_BUILD_INSTALL` は既定で OFF。drop-shadow では既定 ON だったものを、共有の `%PROGRAMDATA%\obs-studio\plugins` へ導入しないよう OFF に変えた

### CMakePresets.json

テンプレートのままで、drop-shadow と一致します。

| プリセット | 生成器 | ビルド先 | 補足 |
| --- | --- | --- | --- |
| `windows-x64` | Visual Studio 17 2022、`x64,version=10.0.22621` | `build_x64` | 59-61 行 |
| `windows-ci-x64` | 同上 | 同上 | `CMAKE_COMPILE_WARNING_AS_ERROR` |
| `macos`、`macos-ci` | Xcode、`arm64;x86_64`、配布先 macOS 12.0 | `build_macos` | ci は警告をエラーにし、ccache を使う |
| `ubuntu-x86_64`、`ubuntu-ci-x86_64` | Ninja | `build_x86_64` | ci は警告をエラーにし、ccache を使う |

ビルドのプリセットはすべて `RelWithDebInfo` です。

### cmake/ 配下で独自に変えた箇所

| ファイル | 変更 | 出所 |
| --- | --- | --- |
| `cmake/windows/helpers.cmake` 25 行と 27-32 行 | DLL と PDB の導入先を、テンプレートの `${target}/bin/64bit` から、OBS 33 の配置の `${target}` へ変えた | drop-shadow のコミット `19a8e02`。`git diff 7540ffa 19a8e02 -- cmake` で確認 |
| `cmake/windows/defaults.cmake` 11-15 行 | `file(TO_CMAKE_PATH "$ENV{ALLUSERSPROFILE}" ...)` で区切りを `/` へ直してから導入先にする。`C:\ProgramData` のままでは `cmake_install.cmake` の文字列で `\P` が不正なエスケープになるため | mosaic で追加。drop-shadow との `diff` で確認 |

`cmake/windows/helpers.cmake:78` は `data/` を `${target}/data` へ導入します。そのほかの `cmake/macos/*`、`cmake/linux/*`、`cmake/common/*` は drop-shadow と一致し、本文は読んでいません。

## 3. ソース

### src/ の各ファイル

| ファイル | 役割 |
| --- | --- |
| `src/plugin-main.cpp` | `OBS_DECLARE_MODULE()`、既定のロケール `en-US`、`obs_module_load` で `register_mosaic_filter()` を呼ぶ (1-19 行) |
| `src/mosaic-filter.cpp` | フィルターの本体。`MosaicFilter` クラスと登録関数 (全 339 行) |
| `src/plugin-support.c.in`、`plugin-support.h` | テンプレートのまま。`obs_log`、`PLUGIN_NAME`、`PLUGIN_VERSION` を提供する。ライセンスのヘッダーは雛形のまま (drop-shadow の調査レポート 115 行) |

### obs_source_info の登録

`src/mosaic-filter.cpp:324-339` です。

| 項目 | 値 |
| --- | --- |
| `id` | `simple_mosaic_filter` |
| `type` | `OBS_SOURCE_TYPE_FILTER` |
| `output_flags` | `OBS_SOURCE_VIDEO` のみ。`OBS_SOURCE_SRGB` は付けていない |
| コールバック | `get_name`、`create`、`destroy`、`update`、`get_defaults`、`get_properties`、`video_tick`、`video_render` |
| 名前 | `obs_module_text("SimpleMosaic")` (248 行) |

`get_width`、`get_height`、`filter_add`、`filter_remove` は持ちません。出力の大きさが変わらないためです。

### 設定項目

| キー | 表示名のキー | 範囲 | 既定値 | 行 |
| --- | --- | --- | --- | --- |
| `region_x` | `RegionX` | 0 から対象の幅 | 0 | 13、264、286 |
| `region_y` | `RegionY` | 0 から対象の高さ | 0 | 14、265、287 |
| `region_width` | `RegionWidth` | 0 から対象の幅 | 0、初回に対象の幅へ書き換える | 15、266、288 |
| `region_height` | `RegionHeight` | 0 から対象の高さ | 0、初回に対象の高さへ書き換える | 16、267、289 |
| `cell_size` | `CellSize` | 2 から 256 | 16 | 17、21-23、268、290 |
| `region_initialized` | なし。画面に出さない | bool | false | 19、269 |

- スライダーはすべて `obs_properties_add_int_slider` で、接尾辞は ` px` です (281-285 行)。上限は `get_properties` で対象の大きさを読み、分からなければ 8192 です (25、274-278 行)。
- `get_defaults` はインスタンスを受け取れないため、`video_tick` で対象の大きさを初めて取得できたときに範囲を全体へ書き込みます (83-105 行)。書き込んだあと `obs_source_update_properties` を呼び、開いている設定画面に読み直しを知らせます (104 行)。これはバグ `obs-simple-mosaic-08e.13` の修正です。
- `update` は値を読むだけで、範囲を 0 以上、マスの大きさを 2 から 256 に丸めます (70-79 行)。

### 描画の手順

`src/mosaic-filter.cpp:107-227` です。

1. 範囲を対象の内側へ切り詰め、幅か高さが 0 なら `obs_source_skip_video_filter` で素通しします (115-123 行)。
2. 対象を `source_render_` (`GS_RGBA`) に描きます。`gs_texrender_begin_with_color_space(..., GS_CS_SRGB)` を使い、ブレンドは `SRCALPHA, INVSRCALPHA, ONE, INVSRCALPHA` です。`obs_source_process_filter_begin` と同じ手順で、親が `OBS_SOURCE_CUSTOM_DRAW` と `OBS_SOURCE_ASYNC` のどちらでもなければ `obs_source_default_render`、それ以外は `obs_source_video_render` を呼びます (201-227 行)。中間の色はアルファを掛けた値になります。
3. 1 パス目 `Rows` で `rows_render_` (`GS_RGBA16F`、マスの列の数×範囲の高さ) に描きます (156-162 行)。
4. 2 パス目 `Cells` で `cells_render_` (`GS_RGBA16F`、マスの列の数×マスの行の数) に描きます (163-168 行)。中間を 16 ビットの浮動小数点にするのは、8 ビットへの丸めの誤差を 2 段で重ねないためです (49-51 行)。
5. 3 パス目 `Draw` で出力先へ描きます。ブレンドは `GS_BLEND_ONE, GS_BLEND_INVSRCALPHA` です (170-177 行)。
6. `gs_technique_end` がパスのたびにパラメーターの値を消すため、各パスの直前に `set_params` ですべて設定し直します (143-154 行)。

### data/effects/mosaic.effect

全 123 行です。

| パラメーター | 型 | 意味 |
| --- | --- | --- |
| `ViewProj` | `float4x4` | 変換行列 |
| `image` | `texture2d` | 対象を描いた中間テクスチャ。アルファを掛けた値 |
| `rows` | `texture2d` | 1 パス目の結果 |
| `cells` | `texture2d` | 2 パス目の結果 |
| `tex_size` | `float2` | 対象の幅と高さ (px) |
| `rect` | `float4` | 範囲の左上 x・y と右下 x・y。切り詰め済み |
| `cell_size` | `float` | マスの一辺 (px) |
| `rows_size` | `float2` | rows の幅と高さ |
| `cells_size` | `float2` | cells の幅と高さ |
| `pair_count` | `int` | `ceil(cell_size / 2)` |

- サンプラーは `linearClamp` の 1 つだけです (23-27 行)。頂点シェーダーは `VSDefault` です (34-40 行)。
- `PSRows` (45-64 行): マスの幅を 2 画素ずつ、境目の点を線形補間で読み、2 画素の平均を 1 回で得ます。奇数幅の最後の 1 画素は重み 1 で読みます。UV に大きさを掛けた位置は `floor` で番号へ丸めます (47 行)。
- `PSCells` (66-85 行): `rows` を同じ方法で縦に平均します。
- `PSDraw` (87-96 行): 範囲の外は `image` をそのまま返し、範囲の中は自分のマスの平均を `cells` から読みます。マスの番号の計算に 0.5 を足し、割り算の誤差で境目の画素が前のマスへずれないようにしています (93-94 行)。
- technique は `Rows`、`Cells`、`Draw` の 3 つです (98-123 行)。

D3D11 と OpenGL の注意点は、ファイル冒頭のコメント 8-9 行と `docs/logs/2026-11-11-render-tests-and-gpu-time.md:54` にあります。

- 型の変換は `float(i)` のようなコンストラクタの形で書きます。
- 回数が実行時に決まるループの中で `Sample` を使うと、D3D11 が `error X3511: unable to unroll loop` でコンパイルに失敗します。ループの中は `SampleLevel(..., 0.0)` で読みます。失敗の実ログは `.logs/2026111107-ctest.log:59-66` と `.logs/2026111108-d3d11-region.log:56-63` にあります。コンパイルに失敗すると `effect_` が作られず、フィルターは素通しになります。

### 色空間の扱い

- 事実: 中間の描画は `GS_CS_SRGB` で、テクスチャは `GS_RGBA` と `GS_RGBA16F` です (`src/mosaic-filter.cpp:48-51、209`)。
- 事実: `video_get_color_space` の実装、`OBS_SOURCE_SRGB` のフラグ、`gs_effect_set_texture_srgb`、`gs_enable_framebuffer_srgb` は使っていません。`src/` に該当する呼び出しはありません。
- 事実: 平均はアルファを掛けた値のまま、sRGB の符号化値で取ります。線形の値へ戻してから平均してはいません。
- 推測: HDR の映像や広色域の扱いは考慮されていません。その動作は確認されていないため未確認です。

### data/locale

`en-US.ini` と `ja-JP.ini` のどちらも 6 行で、キーは `SimpleMosaic`、`RegionX`、`RegionY`、`RegionWidth`、`RegionHeight`、`CellSize` です。

## 4. テスト

### 構成

| ファイル | 役割 |
| --- | --- |
| `tests/obs-test-env.hpp` | フロントエンドなしで libobs を起動し、プラグインを読み込む。drop-shadow と一致 |
| `tests/mosaic-fixture.hpp` | テスト用の入力ソース、フィルターの設定、出力の読み出し、BMP の書き出し、`Checker` |
| `tests/integration-test.cpp` | 9 シナリオの画素の検査 |
| `tests/benchmark.cpp` | 1920×1080 の GPU 時間の計測 |

### CTest への登録

`CMakeLists.txt:81-118` で、Windows だけ登録します。

- 描画モジュールは `.deps/obs-studio-31.1.1/build_x64/libobs-d3d11/Release/libobs-d3d11.dll` と `libobs-opengl/Release/libobs-opengl.dll` です (86-88 行)。configure では作られないため、別にビルドします (84-85 行のコメント)。
- シナリオは `default-full region overflow large-cell zero-size resize-keeps-px translucent scene settings-update` の 9 つです (89-100 行)。2 つの描画方式と掛け合わせて 18 件になります。テスト名は `mosaic-<d3d11|opengl>-<シナリオ>` です (103 行)。
- 引数は、プラグインの DLL、`data`、描画モジュール、`.deps/obs-studio-31.1.1/libobs/data`、シナリオ名の順です (104-109 行)。
- `ENVIRONMENT_MODIFICATION` で `PATH` の先頭に `.deps/bin/64bit` と `.deps/obs-deps-2025-07-11-x64/bin` を足し、`obs.dll` などを解決します (110-115 行)。

### libobs の読み込みと描画方式の切り替え

`tests/obs-test-env.hpp` です。

- Linux では `XOpenDisplay` のあと `obs_set_nix_platform(OBS_NIX_PLATFORM_X11_EGL)` を呼びます (23-32 行)。
- `obs_startup("en-US", ...)` のあと、非推奨の `obs_add_data_path` で libobs の既定エフェクトの場所を渡します。MSVC は警告 4996、GCC と Clang は `-Wdeprecated-declarations` を pragma で抑えます (33-52 行)。CI のプリセットは警告をエラーにするため、抑えないとビルドが失敗します (drop-shadow の調査レポート 391 行、コミット `50fdd05`)。
- `obs_video_info.graphics_module` に引数の描画モジュールのパスを渡し、D3D11 と OpenGL を切り替えます (53-66 行)。出力の形式は `VIDEO_FORMAT_RGBA` です。
- OpenGL では描画スレッドが最初のフレームを進める前に描くと出力が空になることがあるため、`obs_get_total_frames() >= 2` を最大 5 秒待ちます (67-78 行)。
- `obs_open_module` と `obs_init_module` でプラグインを読み込みます (82-90 行)。

### 画素の検証

- テスト用の入力ソース `simple_mosaic_test_source` を登録し、`SourceSpec` の描き方で作った RGBA を `gs_texture_create` して描きます (`tests/mosaic-fixture.hpp:83-123`)。設定の `width` と `height` で大きさを変えられます (71-81 行)。
- 出力の読み出しは `render()` で、`gs_texrender` へブレンド `ONE, ZERO` で描き、`gs_stagesurface_map` で行ごとに `stride` を考えて写します (222-261 行)。
- 設定の変更は `update` シグナルを待つ `update_and_wait` (207-219 行)、フレームの経過は `wait_frames` (187-199 行) で待ちます。
- `check_mosaic` は、範囲の外の画素が基準と ±1 以内、範囲の中がマスごとに同じ色、マスの左上の色がマスの平均と ±2 以内であることを検査します (`tests/integration-test.cpp:35-79`)。基準はフィルターを付ける前に読んだ出力です。
- `check_not_flat` は隣のマスと色が違うことを確かめ、全体が 1 色になる誤りを検出します (82-89 行)。
- 終了コードは合格が 0、不合格が 1、起動の失敗が 2 です (5 行、347-381 行)。
- `scene` シナリオでは、OBS 31 以降はキャンバスがシーンを保持するため、`obs_source_remove` してから `obs_scene_release` します (317-321 行)。

### 画像の書き出し

環境変数 `MOSAIC_TEST_OUTPUT_DIR` があるときだけ、出力を白い背景に合成して 24 ビット BMP で書き出します (`tests/mosaic-fixture.hpp:263-320`)。各シナリオは基準を `<名前>-reference.bmp` として保存します。CI では `test-images` を artifact に上げます。

### GPU 時間の測り方

`tests/benchmark.cpp` です。CTest には登録しません (4 行、`CMakeLists.txt:65`)。

- 1920×1080 のグラデーションのソースを使います (111 行)。
- warmup 10 フレームのあと、`gs_timer_range_begin`、`gs_timer_begin`、60 フレームの描画、`gs_timer_end`、`gs_timer_range_end`、`gs_flush` の順に処理し、`ticks*1000/frequency/60` で 1 フレームのミリ秒を出します (21-78 行)。
- OpenGL の libobs は `gs_timer_range_create` が NULL を返すため、範囲が無いときは開始と終了だけを省きます (42-43 行、48-59 行)。
- 5 回の中央値を取り (80-92 行)、フィルターなしのソースとの差を `filter_ms` として CSV で出します (123-136 行)。値が 0 以下ならタイムスタンプが取れなかったとして終了コード 1 を返します (137-141 行)。

### ローカルでの実行コマンド

`docs/logs/2026-11-11-render-tests-and-gpu-time.md:13-18、33` と `README.md:110-124` に記録されています。

```text
cmake --preset windows-x64 -DENABLE_POST_BUILD_INSTALL=OFF
cmake --build build_x64 --config RelWithDebInfo
cmake --build .deps/obs-studio-31.1.1/build_x64 --config Release --target libobs-d3d11 libobs-opengl
ctest --test-dir build_x64 -C RelWithDebInfo
build_x64/RelWithDebInfo/mosaic-benchmark.exe <simple-mosaic.dll> <data> <libobs-d3d11.dll> <libobs の data> 2 16 64 256
```

ベンチマークは `ctest` の環境を使わないため、`PATH` に `.deps/bin/64bit` と `.deps/obs-deps-2025-07-11-x64/bin` を足し、引数は絶対パスで渡します。`LoadLibrary` が相対パスのプラグインを読み込めないためです (`render-test.yaml:54-55`)。

## 5. CI

### ワークフロー

| ファイル | トリガー | 役割 |
| --- | --- | --- |
| `push.yaml` | `master`、`main`、`release/**` への push と全タグ (4-10 行) | `main` では書式検査、常に `build-project`、タグでは `create-release` |
| `pr-pull.yaml` | `master` と `main` への PR。`**.md` だけの変更は除く (5-9 行) | 書式検査と `build-project` |
| `dispatch.yaml` | 手動 | `build-project` だけ |
| `build-project.yaml` | `workflow_call` | テンプレートの標準。macOS 15 (Xcode 16.1)、Ubuntu 24.04、Windows 2022 でビルドとパッケージ化を行い、artifact を上げる |
| `check-format.yaml` | `workflow_call` | clang-format と gersemi |
| `render-test.yaml` | 手動と、`CMakeLists.txt`、`src/**`、`data/**`、`tests/**`、自身を変えた PR (8-16 行) | 3 OS での描画の検証。push では走らない |

`build-project.yaml` の独自の変更は、タグのビルドで ccache を復元しない `if: github.ref_type != 'tag'` の 2 か所です (115-117 行、208-210 行)。タグの push では構成を `Release`、notarize を true にします (43-49 行)。

### render-test.yaml

| ジョブ | ランナー | 描画 | 要点 |
| --- | --- | --- | --- |
| `windows-render` | windows-2022 | D3D11。GPU が無いため WARP | `build-plugin` アクションのあと `libobs-d3d11` だけをビルドし、`ctest -R d3d11` を実行する (36-60 行)。Windows の CI では OpenGL のテストを実行しない |
| `ubuntu-render` | ubuntu-24.04 | Mesa の OpenGL | `xvfb xauth libgl1-mesa-dri libegl-mesa0 libx11-dev` を入れ、`-DENABLE_RENDER_TESTS=ON` で再構成する。描画モジュールは PPA の `libobs-opengl.so*`、データは `/usr/share/obs/libobs`。`xvfb-run -a -s '-screen 0 1280x720x24'` で各シナリオを実行する (82-120 行) |
| `macos-render` | macos-15、Xcode 16.1 | OpenGL | `.deps/obs-studio-31.1.1/build_universal` で `libobs-opengl` をビルドする。`DYLD_FRAMEWORK_PATH=$PWD/.deps/Frameworks` と、`libavcodec.dylib` のあるディレクトリを `DYLD_LIBRARY_PATH` に渡す (154-186 行) |

`env.SCENARIOS` と `BENCH_CELL_SIZES: 2 16 256` は mosaic 固有です (19-21 行)。ベンチマークは合否に使いません。

### リリース、immutable release、来歴の証明

- `push.yaml:28-143` の `create-release` は `build-project` の後に走り、権限は `contents: write`、`id-token: write`、`attestations: write` です (33-36 行)。
- タグの判定は extglob の `?(v)+([0-9]).+([0-9]).+([0-9])` で、`v1.0.0` の形も受け付けます。`-beta` と `-rc` はプレリリースになります (41-61 行)。drop-shadow は `v` を受け付けず、v1.0.0 までは手動でリリースを作っていました。mosaic ではコミット `9c02df0` (PR #3) で直しました。
- 手順は、artifact の取得、名前の整理 (`windows-x64;zip|exe`、`macos-universal;tar.xz|pkg`、`ubuntu-24.04-x86_64;tar.xz|deb|ddeb`、`sources;tar.xz`)、`CHECKSUMS.txt` の作成、`sha256sum` で `release-checksums.txt` を作る、`actions/attest-build-provenance@4d101475d8b20a2381f78447822ac1eab6504dd8` (v4.2.2) に `subject-checksums` で渡す、`softprops/action-gh-release@9d7c94cf...` で下書きのリリースを作る、の順です (63-143 行)。
- リリースは `draft: true` で作られます (132 行)。公開は手で行う前提です。
- immutable release: リポジトリの設定は `gh api repos/AllegroMoltoV/simple-mosaic/immutable-releases` で `{"enabled":true,"enforced_by_owner":false}` でした。リリースの `immutable` は、v1.0.0 が false、v1.0.1 と v1.0.2 が true です。設定を有効にした時点と手順の記録は、リポジトリ、`.logs`、`.tmp`、Beads のどこにも見つかりませんでした。`immutable` で全文を検索した結果は 0 件です。
- 来歴の証明: v1.0.2 の ZIP (`sha256:a54f1377...`) に対し、`gh api repos/AllegroMoltoV/simple-mosaic/attestations/sha256:...` は 2 件を返しました。2 件になった理由は未確認です。
- v1.0.1 と v1.0.2 の添付は、`windows-x64.zip`、`macos-universal.pkg`、`source.tar.xz`、`x86_64-linux-gnu.deb`、`x86_64-linux-gnu-dbgsym.ddeb` の 5 件です。v1.0.0 は手動で作り、ZIP の 1 件だけです。

### 書式検査

| 道具 | 版 | 根拠 |
| --- | --- | --- |
| clang-format | 19。`brew install obsproject/tools/clang-format@19` | `.github/actions/run-clang-format/action.yaml:38、53-59` |
| gersemi | 版の指定なし。`brew install obsproject/tools/gersemi` | `.github/actions/run-gersemi/action.yaml:52-58` |

- どちらも変更されたファイルだけを `./build-aux/run-clang-format` と `./build-aux/run-gersemi` で検査し、Windows のランナーでは動きません。
- 対象は clang-format が `*.c *.h *.cpp *.hpp *.m *.mm`、gersemi が `*.cmake *CMakeLists.txt` です。
- ローカルでは `clang-format 19.1.5 --dry-run --Werror` と `gersemi --check` が合格した記録があります (Beads `obs-simple-mosaic-08e.2` の完了理由)。

## 6. 配布と掲載

### .soko-kun/

| ファイル | 内容 |
| --- | --- |
| `logo.png` | 512×512、41,683 バイト。`docs/images/logo.png` と同じ大きさ (Beads `08e.7`) |
| `image.gif` | 1150×509、87 コマ、8.13 秒、444,469 バイト。`docs/images/obs-simple-mosaic.gif` と同じ大きさ (Beads `08e.11`) |
| `config.json` | ローカルには無い。リモートの `main` にはコミット `20b90f9` (PR #5) で追加されている |

`config.json` の全文は次のとおりです。`gh api "repos/AllegroMoltoV/simple-mosaic/contents/.soko-kun/config.json?ref=main"` で取得しました。

```json
{"name": "シンプルモザイク / Simple Mosaic", "os": ["windows"]}
```

コミット `20b90f9` のメッセージによると、`os` を `windows` だけにしたのは macOS と Linux の配布物の動作を確かめていないためです。OBS Forums の方針で名前に OBS を使わないよう求められているため、README の題名から OBS を外しています。

倉庫くんの `get_plugin("allegromoltov/simple-mosaic")` では、`status: listed`、`listedRelease.tag: v1.0.2`、`builtWithObsVersion: 31.1.1`、`category: video-filter`、`os: ["windows"]` で、ZIP と source.tar.xz だけが載っています。

### README の構成

ローカルの `README.md` (全 158 行) の見出しです。リモートの `main` では 1 行目が `# シンプルモザイク / Simple Mosaic` に変わり、更新情報に v1.0.2 が加わっています。

| 行 | 見出し |
| --- | --- |
| 1 | `# Simple Mosaic Plugin for OBS Studio` (リモートでは `# シンプルモザイク / Simple Mosaic`) |
| 3-9 | 日英の概要、倉庫くんへのリンク、GIF |
| 11 | `## Features / 機能` |
| 21 | `## Installation / インストール` |
| 23、27、53 | `### Windows`、`#### 通常版 OBS への導入`、`#### ポータブル版 OBS` |
| 59 | `### macOS / Linux` |
| 63 | `## Usage / 使い方` |
| 74 | `## Parameters / パラメータ` |
| 86 | `## Known Behavior / 既知の挙動` |
| 95、97、106 | `## Build / ビルド`、`### Requirements / 必要環境`、`### Build Commands / ビルドコマンド` |
| 126、128、134 | `## AI利用開示 / AI Disclosure`、`### 日本語`、`### English` |
| 140 | `## License / ライセンス` (GPL-2.0、obs-plugintemplate がベース) |
| 146 | `## Author / 作者` |
| 150 | `## Changelog / 更新情報`、版ごとに `### vX.Y.Z (日付)` |

README 7 行の倉庫くんへのリンクは `/plugins/allegromoltov/obs-simple-mosaic` で、改名前の名前です。`get_plugin` が返す `detailUrl` は `/plugins/allegromoltov/simple-mosaic` です。旧 URL の転送の有無は確かめていないため未確認です。

### ruleset

- リポジトリに ruleset のスクリプトはありません。記録は `.tmp/` の JSON 4 件と、Beads、`.appendix` の文書だけです。
- GitHub の ruleset `main` (id 27007401) は `enforcement: active` で、`~DEFAULT_BRANCH` を対象にします。ルールは削除の禁止、`non_fast_forward`、承認 1 件の PR の必須で、`bypass_actors` は空です。`gh api .../rulesets/27007401` で取得した内容は `.tmp/2026111106-ruleset-mosaic.json` と一致しました。
- 運用は、マージのたびに依頼者の承認を得て `gh api -X PUT .../rulesets/<id> -f enforcement=disabled` で無効にし、直後に `active` へ戻す形です (Beads 永続メモリ `repo-github`、`.appendix/20261111/2026111101-distribution-conventions.md:111`)。PR #5 と #6 はマージコミットで入っています。
- 手順の正本はスキル `soko-kun-publish` です (`.appendix/...distribution-conventions.md:94-119`)。

## 7. 開発中に詰まった点と既知の落とし穴

出典は `.prompts/INIT.md`、`docs/`、`.logs/`、Beads (`bd list --all`、`bd show`、`bd recall`) です。Beads の課題は 14 件すべて closed です。

| 事象 | 内容 | 出典 |
| --- | --- | --- |
| D3D11 のループのコンパイル | 実行時に回数が決まるループで `Sample` を使うと `X3511` で失敗し、エフェクトが読めずフィルターが素通しになる。`SampleLevel` で直した | `.logs/2026111107-ctest.log:59-66`、`docs/logs/...gpu-time.md:54` |
| 平均の精度 | 4×4 点を線形補間で読む 1 パスの方式は、16 px のマスで平均が最大 7 ずれ、統合テストに不合格だった | `.logs/2026111105-ctest.log:63-66`、`docs/logs/...gpu-time.md:49` |
| 大きいマスの負荷 | マスごとに 1 画素へ正確な平均を書く方式は、256 px で 1.679 ms かかった。行と縦の 2 段に分けて 0.04 ms 台にした | `docs/logs/...gpu-time.md:50-52` |
| 設定画面の表示 | `video_tick` で範囲を書き換えても、開いている設定画面は読み直さない。`obs_source_update_properties` が要る | Beads `08e.13` |
| OpenGL の最初のフレーム | 描画スレッドが 2 フレーム進む前に描くと出力が空になることがある | `tests/obs-test-env.hpp:67-78` |
| 非推奨 API の警告 | `obs_add_data_path` の警告が CI の `-Werror` で失敗の原因になる | `tests/obs-test-env.hpp:37-52`、drop-shadow の調査レポート 391 行 |
| シーンの後始末 | OBS 31 以降はシーンを `obs_source_remove` してから解放する | `tests/integration-test.cpp:317-321` |
| 実行ビットとシンボリックリンク | この PC でコピーで複製すると、実行ビット 5 件とシンボリックリンク 3 件が失われ、macOS と Ubuntu の CI が終了コード 126 で失敗する (burst-lines の事例)。mosaic は `git checkout <commit> -- <パス>` で取り出して避けた | `.appendix/...distribution-conventions.md:124`、Beads `08e.2` |
| `ALLUSERSPROFILE` のエスケープ | `C:\ProgramData` のまま導入先にすると `\P` が不正なエスケープになる | `cmake/windows/defaults.cmake:11-12` |
| 共有の導入先 | `ENABLE_POST_BUILD_INSTALL` が既定 ON だと、ほかのプラグインと共有の `C:/ProgramData/obs-studio/plugins` へ導入してしまう | Beads `08e` の design |
| OBS アプリの競合 | この PC では複数のセッションが動くため、OBS アプリを使う作業と使わない作業を分けた | `.prompts/INIT.md:52、162-194` |
| render-test の起動 | push では走らない。PR か `gh workflow run render-test.yaml` で起動する | Beads 永続メモリ `repo-github` |
| macOS の dylib | `DYLD_FRAMEWORK_PATH` と `DYLD_LIBRARY_PATH` を渡さないと描画のテストが起動しない。drop-shadow で 4 回試行して直した | drop-shadow の調査レポート 308、390 行 |
| 範囲の枠 | プレビューだけに描く公開 API が無い。依頼者は枠を作らない案を選んだ | Beads `08e` の design、`08e.9` |
| OBS Forums | AI の利用の方針により掲載しない。名前に OBS を入れない | `.appendix/...distribution-conventions.md:66-72`、コミット `20b90f9` |
| Sponsors | GitHub Sponsors のプロフィールに載せない (依頼者判断 2026-11-11) | Beads 永続メモリ `sponsors-decision` |
| 永続メモリの古さ | `repo-github` のリポジトリ名が改名前のまま | `bd recall repo-github` |

## 8. ローカルでのビルドの実績

| 項目 | 値 | 根拠 |
| --- | --- | --- |
| Visual Studio | 2022 Community、MSVC 19.44.35223.0、ツールセット 14.44.35207 | `.logs/2026111102-configure.log:2-11` |
| MSBuild | 17.14.40 | `.logs/2026111104-graphics-modules.log:1` |
| Windows SDK | 10.0.22621.0 | `.logs/2026111102-configure.log:1` |
| OS | Windows 10.0.26300 | 同上 |
| GPU | NVIDIA GeForce RTX 3060 Ti、ドライバー 32.0.16.1088 | `.logs/2026111108-d3d11-region.log:17-22` |
| configure の所要 | 91.1 秒。obs-deps、Qt6、OBS 31.1.1 のソースを取得し、Debug と Release で構成する | `.logs/2026111102-configure.log:14-75` |
| CMake の版 | 記録なし。ログに版の表示が無い | `.logs` を grep して該当なし |

- 実行したコマンドは 4 節のとおりです。ログにはコマンド行が残っていないため、コマンドは `docs/logs/2026-11-11-render-tests-and-gpu-time.md:13-18、33` の記録によります。
- ctest の結果は、初期 16 件の段階で 6 件不合格 (`.logs/2026111105-ctest.log:392`)、D3D11 のコンパイル失敗 (`2026111107`)、16 件合格 (`2026111110-ctest.log:35`) を経て、最終は 18 件合格です (`docs/logs/...gpu-time.md:23`)。
- `2026111122-build-mutant.log` と `2026111123-ctest-mutant.log` は、`obs_source_update_properties` を外すと不合格になることの確認です (Beads `08e.13`)。
- configure の際、`cmake -E tar` が OBS のソースのシンボリックリンク 7 件を飛ばす警告を出します (`.logs/2026111102-configure.log:25-31`)。ビルドの成否には影響していません。
- GPU 時間は、RTX 3060 Ti と D3D11 で、マス 2 px が 0.057 ms、16 px が 0.038 ms、64 px が 0.041 ms、256 px が 0.043 ms でした (`docs/logs/...gpu-time.md:36-41`)。

## 調べた範囲と調べていない範囲

- 読んだもの: `git ls-files` の全一覧、`CMakeLists.txt`、`CMakePresets.json`、`buildspec.json`、`.gitignore`、`src/` の `.cpp`2 件、`data/` の全 3 件、`tests/` の全 4 件、`.github/workflows/` の全 6 件、`.github/actions/` の build-plugin、run-clang-format、run-gersemi、`cmake/windows/defaults.cmake`、`README.md`、`docs/INDEX.md`、`docs/logs/` の 1 件、`.prompts/INIT.md`、`QUESTIONS.md`、`.tmp/` の ruleset の JSON とリリースノート、`.appendix/...distribution-conventions.md` の 1-130 行。
- 部分だけ読んだもの: `docs/reports/2026-11-11-drop-shadow-survey.md` (見出しと 35-124 行、234-451 行)、`cmake/windows/helpers.cmake` (grep の結果)、`.logs/` (grep の結果と 5 件の冒頭)。
- diff で一致だけを確かめたもの: `cmake/` の残り、`build-aux/`、`.clang-format`、`.gersemirc`、`LICENSE`、`src/plugin-support.c.in`、`tests/obs-test-env.hpp`。
- 読んでいないもの: `src/plugin-support.h`、`.github/scripts/` の本文、`.github/actions/` の残り 3 件、`.prompts/MANUALS/`、`.appendix/BLOGS/`、`.logs/` の残り、Beads の課題 `08e.1` と `08e.8` の本文。
- GitHub で参照したもの: リポジトリの情報、immutable release の設定、リリースの一覧と添付の digest、来歴の証明の件数、ruleset、`main` との差分、`config.json`。倉庫くんは `get_plugin` だけを参照しました。
- 未確認のもの: obs-plugintemplate の上流との直接の照合、リポジトリの改名の日時、immutable release を有効にした日時、来歴の証明が 2 件ある理由、README の旧 URL の転送、CMake の版。
