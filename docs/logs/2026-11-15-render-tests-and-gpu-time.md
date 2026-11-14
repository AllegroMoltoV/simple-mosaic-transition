# 描画の統合テストと GPU 時間の記録

記録日は 2026-11-15 です。ローカルの Windows で、描画の統合テストと 1920×1080 の GPU 時間の計測を行いました。

## 検証環境

| 項目 | 値 |
| --- | --- |
| OS | Windows 11 Home 10.0.26300 |
| GPU | NVIDIA GeForce RTX 3060 Ti、ドライバー 32.0.16.1088 |
| ビルドに使った OBS | 31.1.1 (`buildspec.json`) |
| ビルドの構成 | `RelWithDebInfo` |

## 実行したコマンド

```text
cmake --preset windows-x64 -DENABLE_POST_BUILD_INSTALL=OFF
cmake --build build_x64 --config RelWithDebInfo
cmake --build .deps/obs-studio-31.1.1/build_x64 --config Release --target libobs-d3d11 libobs-opengl
ctest --test-dir build_x64 -C RelWithDebInfo
build_x64/RelWithDebInfo/mosaic-benchmark.exe <simple-mosaic-transition.dll> <data> <libobs-d3d11.dll> <libobs の data> 8 64 256
```

ベンチマークは `ctest` の環境を使わないため、`PATH` に `.deps/bin/64bit` と `.deps/obs-deps-2025-07-11-x64/bin` を足し、引数は絶対パスで渡しました。

## 統合テストの結果

D3D11 と OpenGL の 7 シナリオ、計 14 件が合格しました。

| シナリオ | 検査の内容 |
| --- | --- |
| `t0` | t = 0 で前のシーンと ±1 以内 |
| `near-end` | t = 0.999 で次のシーンと ±1 以内 |
| `black-at-switch` | 暗転部分の長さ 50% で、切り替えの位置の全画素が不透明な黒 |
| `dark-window` | 暗転部分の長さの境界値 0%・1%・50%・99%・100% と、範囲外の -1%・101% の 24 か所。暗転の区間の直前と直後は 0% の出力と ±1 以内、区間の中央は 0% の RGB の半分と画素ごとに ±2 以内で不透明、切り替えの位置は不透明な黒。0% と -1% では切り替えの位置が黒にならない |
| `swap-around-switch` | 暗転部分の長さ 0%、切り替えの位置 30% のとき、t = 0.29 は前のシーン、t = 0.31 は次のシーンの最も粗いモザイク。端の欠けたマスを含めて平均が ±2 以内で、隣のマスと色が異なる |
| `switch-point-edges` | 切り替えの位置が 0% と 100% のとき、始まりと終わりの直前の出力が期待どおり |
| `non-srgb` | `GS_CS_709_EXTENDED` の描画先で、t = 0 が前のシーンと一致し、切り替えの位置が黒 |

テストが誤りを検出することも確かめました。シェーダーで黒へ近づける先を `float4(0, 0, 0, 1)` から `float4(0, 0, 0, 0)` に変えると、`black-at-switch` と `non-srgb` が D3D11 と OpenGL の両方で不合格になりました。生のログは `.logs/2026111505-ctest-mutant-transparent.log` です。暗転の始まりを 1% 早める誤りを入れると、`dark-window` が D3D11 と OpenGL の両方で不合格になりました。生のログは `.logs/2026111534-ctest-mutant-boundary.log` です。

暗転を前半の後ろ半分と後半の前半分にだけかける変更は、シェーダーの処理を変えないため、GPU 時間は計り直していません。

## GPU 時間

1920×1080 で、切り替えの位置の直後 (t = 0.5001、最も粗い瞬間) の 1 フレームの GPU 時間を、D3D11 で測りました。基準は同じトランジションの t = 0 で、マスが 1 px のためモザイクの 2 パスを描きません。各値は 60 フレームの平均を 5 回測った中央値です。

| 最大の粗さ | 1 フレーム | 基準 | モザイクの分 |
| --- | --- | --- | --- |
| 8 px | 0.036 ms | 0.028 ms | 0.008 ms |
| 64 px | 0.040 ms | 0.023 ms | 0.016 ms |
| 256 px | 0.051 ms | 0.024 ms | 0.028 ms |

基準の値は 0.023 ms から 0.028 ms の範囲でばらつきました。モザイクの分の差は、このばらつきと同じ桁です。

## 書式の検査

- clang-format 19.1.5 (Visual Studio 2022 に同梱) の `--dry-run --Werror` に、`src/` と `tests/` のファイルが合格しました。
- gersemi 0.25.0 の `--check` に、`CMakeLists.txt` が合格しました。`cmake/` の 5 ファイルは整形の対象と判定されました。いずれも流用元から変更していないファイルです。
