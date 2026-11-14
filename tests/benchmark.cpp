// モザイクのトランジションの 1 フレームあたりの GPU 時間を、1920x1080 の最も粗い瞬間で測るベンチマーク。
// 計測は gs_timer (タイムスタンプのクエリ) で行う。同じトランジションの t = 0 (マスが 1 px で、モザイクの 2 パスを描かない) を
// 基準とし、その差をモザイクの時間とする。前後のシーンの描き直しは tick の時刻に左右されるため、ソースを単独で描いた時間は基準にしない。
// 使い方: benchmark <プラグイン> <プラグインの data> <描画モジュール> <libobs の data> <最大の粗さ>...
// 環境で値が変わるため CTest には登録しない。

#include "mosaic-fixture.hpp"

#include <iomanip>

namespace {

using namespace mosaic_test;

constexpr uint32_t kWidth = 1920;
constexpr uint32_t kHeight = 1080;
constexpr int kWarmupFrames = 10;
constexpr int kMeasuredFrames = 60;
constexpr int kRepeats = 5;

// 1 フレームあたりの GPU 時間 (ミリ秒) を返す。失敗時は負の値
double measure(obs_source_t *source)
{
	double result = -1.0;
	obs_enter_graphics();
	gs_texrender_t *target = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	gs_timer_range_t *range = gs_timer_range_create();
	gs_timer_t *timer = gs_timer_create();

	auto render_frame = [&]() {
		gs_texrender_reset(target);
		if (!gs_texrender_begin(target, kWidth, kHeight)) {
			return false;
		}
		vec4 clear = {};
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(0.0f, static_cast<float>(kWidth), 0.0f, static_cast<float>(kHeight), -100.0f, 100.0f);
		obs_source_video_render(source);
		gs_texrender_end(target);
		return true;
	};

	// OpenGL の libobs は範囲を作らず NULL を返す。範囲のデータの取得は引数を使わずに周波数を返すため、
	// 範囲が無いときは開始と終了だけを省く
	bool ok = target && timer;
	for (int i = 0; ok && i < kWarmupFrames; ++i) {
		ok = render_frame();
	}
	if (ok) {
		if (range) {
			gs_timer_range_begin(range);
		}
		gs_timer_begin(timer);
		for (int i = 0; ok && i < kMeasuredFrames; ++i) {
			ok = render_frame();
		}
		gs_timer_end(timer);
		if (range) {
			gs_timer_range_end(range);
		}
		gs_flush();
	}
	if (ok) {
		bool disjoint = true;
		uint64_t frequency = 0;
		uint64_t ticks = 0;
		if (gs_timer_range_get_data(range, &disjoint, &frequency) && gs_timer_get_data(timer, &ticks) &&
		    !disjoint && frequency > 0) {
			result = static_cast<double>(ticks) * 1000.0 / static_cast<double>(frequency) / kMeasuredFrames;
		}
	}
	gs_timer_destroy(timer);
	if (range) {
		gs_timer_range_destroy(range);
	}
	gs_texrender_destroy(target);
	obs_leave_graphics();
	return result;
}

double median_of_repeats(obs_source_t *source)
{
	std::vector<double> samples;
	for (int i = 0; i < kRepeats; ++i) {
		const double ms = measure(source);
		if (ms < 0.0) {
			return -1.0;
		}
		samples.push_back(ms);
	}
	std::sort(samples.begin(), samples.end());
	return samples[samples.size() / 2];
}

} // namespace

int main(int argc, char **argv)
{
	if (argc < 6) {
		std::cerr
			<< "usage: benchmark <plugin> <plugin data> <graphics module> <libobs data> <max cell size>...\n";
		return 2;
	}
	if (!obs_test::start(argv[3], argv[4], kWidth, kHeight)) {
		return 2;
	}
	register_test_source();
	if (!obs_test::load_plugin(argv[1], argv[2])) {
		obs_shutdown();
		return 2;
	}

	obs_source_t *a = create_test_source({kWidth, kHeight, paint_gradient}, "scene-a");
	obs_source_t *b = create_test_source({kWidth, kHeight, paint_stripes}, "scene-b");
	if (!a || !b) {
		std::cerr << "failed to create the test sources\n";
		obs_source_release(b);
		obs_source_release(a);
		obs_shutdown();
		return 2;
	}

	int exit_code = 0;
	std::cout << std::fixed << std::setprecision(3);
	std::cout << "max_cell_size,width,height,frame_ms,baseline_ms,mosaic_ms\n";
	for (int i = 5; i < argc; ++i) {
		// 暗転は描く手順を変えないため、暗転部分の長さを 0% にして測る。切り替えの位置の直後で最も粗くなる
		obs_source_t *transition = create_transition({0, std::atoi(argv[i]), 50}, kWidth, kHeight);
		double baseline = -1.0;
		double frame = -1.0;
		if (transition && set_time(transition, a, b, 0.0f)) {
			baseline = median_of_repeats(transition);
			if (set_time(transition, a, b, 0.5001f)) {
				frame = median_of_repeats(transition);
			}
		}
		std::cout << argv[i] << ',' << kWidth << ',' << kHeight << ',' << frame << ',' << baseline << ','
			  << (frame - baseline) << '\n';
		// 1920x1080 を 60 フレーム描いて 0 になることはない。0 以下はタイムスタンプが取れなかったことを示す
		if (frame <= 0.0 || baseline <= 0.0) {
			std::cerr << "GPU timestamps are unavailable on this graphics backend\n";
			exit_code = 1;
		}
		obs_source_release(transition);
	}

	obs_source_release(b);
	obs_source_release(a);
	obs_shutdown();
	return exit_code;
}
