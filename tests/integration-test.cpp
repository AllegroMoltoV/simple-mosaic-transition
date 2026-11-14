// モザイクのトランジションの出力を画素で検査する統合テスト。
// 前のシーン A (グラデーション) と次のシーン B (縦縞) をテスト用のソースで作り、手動のトランジションで進み具合 t を固定して描く。
// 守るのは、利用者に見える境界 (始まりと終わりで画面が跳ねない、切り替えの位置で黒になる) と、
// 流用したシェーダーや libobs の色空間の分岐との結合。
// 使い方: integration-test <プラグイン> <プラグインの data> <描画モジュール> <libobs の data> <シナリオ>
// シナリオ sequence は CTest に登録せず、環境変数 MOSAIC_TEST_OUTPUT_DIR に連番画像を書き出す。
// 終了コードは、合格が 0、検査の不合格が 1、起動の失敗が 2。

#include "mosaic-fixture.hpp"

#include <cstdio>

namespace {

using namespace mosaic_test;

// 端のマスが割り切れない大きさ。168 = 16 x 10.5、116 = 16 x 7.25
constexpr uint32_t kCanvasWidth = 168;
constexpr uint32_t kCanvasHeight = 116;
constexpr uint32_t kMaxCell = 16;
// マスの平均の許容差。8 ビットへの丸めと、線形補間の重みの量子化の分だけ許す
constexpr int kAverageTolerance = 2;

std::string at(uint32_t x, uint32_t y)
{
	return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
}

bool same_size(Checker &c, const Image &a, const Image &b)
{
	const bool same = a.width == b.width && a.height == b.height;
	c.expect(same, "output size " + std::to_string(b.width) + "x" + std::to_string(b.height) +
			       " differs from the reference");
	return same;
}

// out が reference と全画素で ±1 以内であることを検査する
void check_equal(Checker &c, const Image &reference, const Image &out, const std::string &what)
{
	if (!same_size(c, reference, out)) {
		return;
	}
	for (uint32_t y = 0; y < out.height; ++y) {
		for (uint32_t x = 0; x < out.width; ++x) {
			for (int ch = 0; ch < 4; ++ch) {
				c.expect(std::abs(out.at(x, y)[ch] - reference.at(x, y)[ch]) <= 1,
					 what + ": pixel " + at(x, y) + " channel " + std::to_string(ch) + " is " +
						 std::to_string(out.at(x, y)[ch]) + " (expected " +
						 std::to_string(reference.at(x, y)[ch]) + ")");
			}
		}
	}
}

// out が、cell ごとに同じ色で、その色が reference のマスの平均に近いことを検査する。端の欠けたマスも同じ条件で検査する。
// あわせて、隣のマスと色が違うことを確かめ、全体が 1 色になる誤りを検出する
void check_mosaic(Checker &c, const Image &reference, const Image &out, uint32_t cell, const std::string &what)
{
	if (!same_size(c, reference, out)) {
		return;
	}
	for (uint32_t y = 0; y < out.height; ++y) {
		for (uint32_t x = 0; x < out.width; ++x) {
			const uint32_t bx = (x / cell) * cell;
			const uint32_t by = (y / cell) * cell;
			const uint8_t *first = out.at(bx, by);
			c.expect(std::memcmp(out.at(x, y), first, 4) == 0,
				 what + ": pixel " + at(x, y) + " differs from its cell origin " + at(bx, by));
			if (x != bx || y != by) {
				continue;
			}
			const uint32_t ex = std::min(bx + cell, out.width);
			const uint32_t ey = std::min(by + cell, out.height);
			for (int ch = 0; ch < 4; ++ch) {
				long sum = 0;
				for (uint32_t yy = by; yy < ey; ++yy) {
					for (uint32_t xx = bx; xx < ex; ++xx) {
						sum += reference.at(xx, yy)[ch];
					}
				}
				const long count = static_cast<long>(ex - bx) * (ey - by);
				const int expected = static_cast<int>((sum + count / 2) / count);
				c.expect(std::abs(first[ch] - expected) <= kAverageTolerance,
					 what + ": cell " + at(bx, by) + " channel " + std::to_string(ch) + ": " +
						 std::to_string(first[ch]) + " (expected " + std::to_string(expected) +
						 ")");
			}
		}
	}
	if (out.width > cell) {
		c.expect(std::memcmp(out.at(0, 0), out.at(cell, 0), 4) != 0,
			 what + ": adjacent cells have the same color");
	}
}

// 全画素が不透明な黒であることを検査する。透明になる誤りを見分けるため、アルファも確かめる
void check_black(Checker &c, const Image &out, const std::string &what)
{
	for (uint32_t y = 0; y < out.height; ++y) {
		for (uint32_t x = 0; x < out.width; ++x) {
			const uint8_t *p = out.at(x, y);
			c.expect(p[0] <= 1 && p[1] <= 1 && p[2] <= 1 && p[3] >= 254,
				 what + ": pixel " + at(x, y) + " is not opaque black (" + std::to_string(p[0]) + ", " +
					 std::to_string(p[1]) + ", " + std::to_string(p[2]) + ", " +
					 std::to_string(p[3]) + ")");
		}
	}
}

struct Scene {
	obs_source_t *a = nullptr;
	obs_source_t *b = nullptr;
	obs_source_t *transition = nullptr;
	Image ref_a;
	Image ref_b;
};

// A と B とトランジションを作り、A と B の基準の画像を読んでから check を呼ぶ
void with_scene(Checker &c, const std::string &name, const TransitionSpec &spec, uint32_t width, uint32_t height,
		const std::function<void(Checker &, Scene &)> &check, enum gs_color_space space = GS_CS_SRGB)
{
	Scene s;
	s.a = create_test_source({width, height, paint_gradient}, "scene-a");
	s.b = create_test_source({width, height, paint_stripes}, "scene-b");
	s.transition = create_transition(spec, width, height);
	if (!s.a || !s.b || !s.transition) {
		c.expect(false, "failed to create the test sources or the transition");
	} else if (!render(s.a, s.ref_a, space) || !render(s.b, s.ref_b, space)) {
		c.expect(false, "failed to render the references");
	} else {
		save_image(name + "-a", s.ref_a);
		save_image(name + "-b", s.ref_b);
		check(c, s);
	}
	obs_source_release(s.transition);
	obs_source_release(s.b);
	obs_source_release(s.a);
}

// transition を t に固定して描き、画像を保存する。失敗したときは false
bool draw_with(Checker &c, obs_source_t *transition, Scene &s, float t, const std::string &name, Image &out,
	       enum gs_color_space space = GS_CS_SRGB)
{
	if (!set_time(transition, s.a, s.b, t)) {
		c.expect(false, name + ": failed to fix the transition time");
		return false;
	}
	if (!render(transition, out, space)) {
		c.expect(false, name + ": failed to render the transition");
		return false;
	}
	save_image(name, out);
	return true;
}

bool draw_at(Checker &c, Scene &s, float t, const std::string &name, Image &out, enum gs_color_space space = GS_CS_SRGB)
{
	return draw_with(c, s.transition, s, t, name, out, space);
}

// 暗転部分の長さの境界値の検査。切り替えの位置 50% で、前半の進み具合 u または後半の進み具合 v の位置で描き、
// 暗転部分の長さ 0% で同じ t に描いた出力と比べる
enum class DarkExpect {
	Same,    // 暗転していない。0% と ±1 以内
	Half,    // 暗転の割合がちょうど 0.5。0% の RGB の半分と ±2 以内で、不透明
	Black,   // 不透明な黒
	MosaicB, // 黒にならず、次のシーンの最も粗いモザイク
};

struct DarkCase {
	int window;     // 暗転部分の長さ (%)。範囲外の値は 0 から 100 に丸められる
	bool second;    // 後半なら true
	float progress; // 前半の u、または後半の v
	DarkExpect expect;
};

// 暗転部分の長さ w に対し、暗転は前半の u が 1 - w から 1 の区間、後半の v が 0 から w の区間にかかり、
// 区間の中央で割合が 0.5 になる
const DarkCase dark_cases[] = {
	// 0%: 暗転しない。切り替えの位置でも黒にならない
	{0, false, 0.999f, DarkExpect::Same},
	{0, true, 0.0f, DarkExpect::MosaicB},
	// 1%: 最も短い暗転。区間の直前と直後は暗転せず、区間の中央で半分、切り替えの位置で黒
	{1, false, 0.98f, DarkExpect::Same},
	{1, false, 0.995f, DarkExpect::Half},
	{1, true, 0.0f, DarkExpect::Black},
	{1, true, 0.005f, DarkExpect::Half},
	{1, true, 0.02f, DarkExpect::Same},
	// 50%: 既定値
	{50, false, 0.4f, DarkExpect::Same},
	{50, false, 0.75f, DarkExpect::Half},
	{50, true, 0.0f, DarkExpect::Black},
	{50, true, 0.25f, DarkExpect::Half},
	{50, true, 0.6f, DarkExpect::Same},
	// 99%: 区間の始まりの直前はまだ暗転しない
	{99, false, 0.008f, DarkExpect::Same},
	{99, false, 0.505f, DarkExpect::Half},
	{99, true, 0.0f, DarkExpect::Black},
	{99, true, 0.495f, DarkExpect::Half},
	{99, true, 0.992f, DarkExpect::Same},
	// 100%: 前半と後半の全体で暗転する。それでも始まりと終わりで画面が跳ねない
	{100, false, 2.0f * kStartTime, DarkExpect::Same},
	{100, false, 0.5f, DarkExpect::Half},
	{100, true, 0.0f, DarkExpect::Black},
	{100, true, 0.5f, DarkExpect::Half},
	{100, true, 0.998f, DarkExpect::Same},
	// 範囲外: 保存された設定を手で書き換えた値は、0% と 100% として扱う
	{-1, true, 0.0f, DarkExpect::MosaicB},
	{101, false, 0.5f, DarkExpect::Half},
};

// out が、暗転していない出力 plain を半分の明るさにした不透明な画像であることを検査する
void check_half(Checker &c, const Image &plain, const Image &out, const std::string &what)
{
	if (!same_size(c, plain, out)) {
		return;
	}
	for (uint32_t y = 0; y < out.height; ++y) {
		for (uint32_t x = 0; x < out.width; ++x) {
			const uint8_t *o = out.at(x, y);
			const uint8_t *p = plain.at(x, y);
			for (int ch = 0; ch < 3; ++ch) {
				const double expected = p[ch] * 0.5;
				c.expect(std::abs(o[ch] - expected) <= 2.0,
					 what + ": pixel " + at(x, y) + " channel " + std::to_string(ch) + " is " +
						 std::to_string(o[ch]) + " (expected about " +
						 std::to_string(expected) + ")");
			}
			c.expect(o[3] >= 254, what + ": pixel " + at(x, y) + " is not opaque");
		}
	}
}

struct Scenario {
	const char *name;
	std::function<void(Checker &)> run;
};

const Scenario scenarios[] = {
	// 始まりでは前のシーンのまま。切り替えを始めた瞬間に画面が跳ねない
	{"t0",
	 [](Checker &c) {
		 with_scene(c, "t0", {}, kCanvasWidth, kCanvasHeight, [](Checker &c, Scene &s) {
			 Image out;
			 if (draw_at(c, s, kStartTime, "t0", out)) {
				 check_equal(c, s.ref_a, out, "t=start");
			 }
		 });
	 }},
	// 終わりの直前では次のシーンのまま。t = 1 は libobs が B を直接描き、プラグインを通らないため検査しない
	{"near-end",
	 [](Checker &c) {
		 with_scene(c, "near-end", {}, kCanvasWidth, kCanvasHeight, [](Checker &c, Scene &s) {
			 Image out;
			 if (draw_at(c, s, 0.999f, "near-end", out)) {
				 check_equal(c, s.ref_b, out, "t=0.999");
			 }
		 });
	 }},
	// 暗転部分の長さが 0% より大きいとき、切り替えの位置で全体が不透明な黒になる
	{"black-at-switch",
	 [](Checker &c) {
		 with_scene(c, "black-at-switch", {50, kMaxCell, 50}, kCanvasWidth, kCanvasHeight,
			    [](Checker &c, Scene &s) {
				    Image out;
				    if (draw_at(c, s, 0.5f, "black-at-switch", out)) {
					    check_black(c, out, "t=switch point");
				    }
			    });
	 }},
	// 暗転部分の長さの境界値 (0%・1%・50%・99%・100% と範囲外) で、暗転がかかる区間と割合が式どおりになる。
	// 暗転部分の長さ 0% の出力 (s.transition) を基準にする
	{"dark-window",
	 [](Checker &c) {
		 with_scene(c, "dark-window", {0, kMaxCell, 50}, kCanvasWidth, kCanvasHeight, [](Checker &c, Scene &s) {
			 for (const DarkCase &dc : dark_cases) {
				 const float t = dc.second ? 0.5f + dc.progress * 0.5f : dc.progress * 0.5f;
				 const std::string what = "dark " + std::to_string(dc.window) + "%, " +
							  (dc.second ? "v=" : "u=") + std::to_string(dc.progress);
				 const std::string name = "dark-window-" + std::to_string(dc.window) +
							  (dc.second ? "-v" : "-u") +
							  std::to_string(static_cast<int>(dc.progress * 1000.0f));
				 obs_source_t *transition = create_transition(
					 {dc.window, static_cast<int>(kMaxCell), 50}, kCanvasWidth, kCanvasHeight);
				 Image plain;
				 Image out;
				 if (!transition) {
					 c.expect(false, what + ": failed to create the transition");
				 } else if (draw_with(c, s.transition, s, t, name + "-plain", plain) &&
					    draw_with(c, transition, s, t, name, out)) {
					 switch (dc.expect) {
					 case DarkExpect::Same:
						 check_equal(c, plain, out, what);
						 break;
					 case DarkExpect::Half:
						 check_half(c, plain, out, what);
						 break;
					 case DarkExpect::Black:
						 check_black(c, out, what);
						 break;
					 case DarkExpect::MosaicB:
						 check_mosaic(c, s.ref_b, out, kMaxCell, what);
						 break;
					 }
				 }
				 obs_source_release(transition);
			 }
		 });
	 }},
	// 暗転部分の長さが 0% のとき、切り替えの位置の直前は A、直後は B の最も粗いモザイクになる。
	// 切り替えの位置を既定と異なる 30% にし、設定の読み込みもあわせて確かめる
	{"swap-around-switch",
	 [](Checker &c) {
		 with_scene(c, "swap-around-switch", {0, kMaxCell, 30}, kCanvasWidth, kCanvasHeight,
			    [](Checker &c, Scene &s) {
				    Image before;
				    if (draw_at(c, s, 0.29f, "swap-around-switch-before", before)) {
					    check_mosaic(c, s.ref_a, before, kMaxCell, "t=0.29");
				    }
				    Image after;
				    if (draw_at(c, s, 0.31f, "swap-around-switch-after", after)) {
					    check_mosaic(c, s.ref_b, after, kMaxCell, "t=0.31");
				    }
			    });
	 }},
	// 切り替えの位置が 0% と 100% でも、0 で割らずに描ける。
	// 0% では B の最も粗いモザイクから戻るだけ、100% では A が最も粗いモザイクになるだけになる
	{"switch-point-edges",
	 [](Checker &c) {
		 with_scene(c, "switch-point-0", {0, kMaxCell, 0}, kCanvasWidth, kCanvasHeight,
			    [](Checker &c, Scene &s) {
				    Image out;
				    if (draw_at(c, s, kStartTime, "switch-point-0-start", out)) {
					    check_mosaic(c, s.ref_b, out, kMaxCell, "switch point 0%, t=start");
				    }
				    if (draw_at(c, s, 0.999f, "switch-point-0-end", out)) {
					    check_equal(c, s.ref_b, out, "switch point 0%, t=0.999");
				    }
			    });
		 with_scene(c, "switch-point-100", {50, kMaxCell, 100}, kCanvasWidth, kCanvasHeight,
			    [](Checker &c, Scene &s) {
				    Image out;
				    if (draw_at(c, s, kStartTime, "switch-point-100-start", out)) {
					    check_equal(c, s.ref_a, out, "switch point 100%, t=start");
				    }
				    if (draw_at(c, s, 0.999999f, "switch-point-100-end", out)) {
					    check_black(c, out, "switch point 100%, t=0.999999");
				    }
			    });
	 }},
	// sRGB 以外の色空間 (OBS Studio の HDR の出力) でも、始まりは A のまま、切り替えの位置は黒になる
	{"non-srgb",
	 [](Checker &c) {
		 with_scene(
			 c, "non-srgb", {50, kMaxCell, 50}, kCanvasWidth, kCanvasHeight,
			 [](Checker &c, Scene &s) {
				 Image out;
				 if (draw_at(c, s, kStartTime, "non-srgb-t0", out, GS_CS_709_EXTENDED)) {
					 check_equal(c, s.ref_a, out, "non-srgb t=start");
				 }
				 if (draw_at(c, s, 0.5f, "non-srgb-switch", out, GS_CS_709_EXTENDED)) {
					 check_black(c, out, "non-srgb t=switch point");
				 }
			 },
			 GS_CS_709_EXTENDED);
	 }},
	// 目視の確認用の連番画像。暗転部分の長さ 50% と 0% を、既定の最大の粗さで 1/30 刻みに描く
	{"sequence",
	 [](Checker &c) {
		 for (const bool fade : {true, false}) {
			 const std::string prefix = fade ? "seq-black" : "seq-direct";
			 with_scene(c, prefix, {fade ? 50 : 0, 64, 50}, 640, 360, [&prefix](Checker &c, Scene &s) {
				 for (int i = 0; i < 30; ++i) {
					 char name[64];
					 std::snprintf(name, sizeof(name), "%s-%02d", prefix.c_str(), i);
					 Image out;
					 draw_at(c, s, static_cast<float>(i) / 30.0f, name, out);
				 }
			 });
		 }
	 }},
};

} // namespace

int main(int argc, char **argv)
{
	if (argc != 6) {
		std::cerr
			<< "usage: integration-test <plugin> <plugin data> <graphics module> <libobs data> <scenario>\n";
		return 2;
	}
	const Scenario *scenario = nullptr;
	for (const Scenario &s : scenarios) {
		if (std::strcmp(argv[5], s.name) == 0) {
			scenario = &s;
		}
	}
	if (!scenario) {
		std::cerr << "unknown scenario: " << argv[5] << '\n';
		return 2;
	}
	if (!obs_test::start(argv[3], argv[4], kCanvasWidth, kCanvasHeight)) {
		return 2;
	}
	register_test_source();
	int result = 2;
	if (obs_test::load_plugin(argv[1], argv[2])) {
		Checker checker;
		scenario->run(checker);
		result = checker.failures == 0 ? 0 : 1;
		if (result != 0) {
			std::cerr << checker.failures << " check(s) failed in " << scenario->name << '\n';
		} else {
			std::cout << "PASS " << scenario->name << '\n';
		}
	}
	obs_shutdown();
	return result;
}
