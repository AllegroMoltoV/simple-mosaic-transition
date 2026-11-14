// モザイクが粗くなって次のシーンへ切り替わるトランジション。
// 切り替えの位置より前は前のシーン a のマスを 1 px から最大の粗さへ大きくし、後は次のシーン b のマスを最大の粗さから 1 px へ
// 小さくする。黒へのフェードを使うときは、粗くなるのと同時に黒へ近づき、切り替えの位置で不透明な黒になる。
// 色空間と音声の扱いは、OBS 標準の「色へフェード」(plugins/obs-transitions/transition-fade-to-color.c) にならう。

#include <obs-module.h>
#include <plugin-support.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <new>

namespace {

constexpr const char *kFadeToBlack = "fade_to_black";
constexpr const char *kMaxCellSize = "max_cell_size";
constexpr const char *kSwitchPoint = "switch_point";

constexpr int kMinMaxCellSize = 8;
constexpr int kMaxMaxCellSize = 256;
constexpr int kDefaultMaxCellSize = 64;
constexpr int kDefaultSwitchPoint = 50;

float clamp01(float x)
{
	return std::clamp(x, 0.0f, 1.0f);
}

// 区間 [lo, hi] での smoothstep。区間の幅が 0 のときは、hi を境に 0 か 1 を返す。
// 切り替えの位置が 0% か 100% のとき、片側の区間が無くなっても 0 で割らないため
float smoothstep(float lo, float hi, float x)
{
	if (hi <= lo) {
		return x >= hi ? 1.0f : 0.0f;
	}
	const float u = clamp01((x - lo) / (hi - lo));
	return u * u * (3.0f - 2.0f * u);
}

float lerp(float a, float b, float x)
{
	return (1.0f - x) * a + x * b;
}

// トランジションの進み具合 t での描き方
struct Frame {
	bool use_b;   // 次のシーンを描く
	float coarse; // 粗さの度合い。0 で元の映像、1 で最大の粗さ
};

Frame frame_at(float t, float switch_point)
{
	if (t < switch_point) {
		return {false, smoothstep(0.0f, switch_point, t)};
	}
	return {true, 1.0f - smoothstep(switch_point, 1.0f, t)};
}

class MosaicTransition {
public:
	MosaicTransition(obs_data_t *settings, obs_source_t *context) : context_(context)
	{
		obs_enter_graphics();
		char *path = obs_module_file("effects/mosaic-transition.effect");
		if (path) {
			effect_ = gs_effect_create_from_file(path, nullptr);
			bfree(path);
		}
		if (effect_) {
			param_image_ = gs_effect_get_param_by_name(effect_, "image");
			param_rows_ = gs_effect_get_param_by_name(effect_, "rows");
			param_cells_ = gs_effect_get_param_by_name(effect_, "cells");
			param_tex_size_ = gs_effect_get_param_by_name(effect_, "tex_size");
			param_cell_size_ = gs_effect_get_param_by_name(effect_, "cell_size");
			param_rows_size_ = gs_effect_get_param_by_name(effect_, "rows_size");
			param_cells_size_ = gs_effect_get_param_by_name(effect_, "cells_size");
			param_pair_count_ = gs_effect_get_param_by_name(effect_, "pair_count");
			param_black_ = gs_effect_get_param_by_name(effect_, "black");
		}
		// 途中の平均は 8 ビットに丸めず、丸めの誤差が 2 段で重ならないよう浮動小数点で持つ
		rows_render_ = gs_texrender_create(GS_RGBA16F, GS_ZS_NONE);
		cells_render_ = gs_texrender_create(GS_RGBA16F, GS_ZS_NONE);
		obs_leave_graphics();
		if (!Ready()) {
			obs_log(LOG_ERROR, "[mosaic-transition] failed to load effects/mosaic-transition.effect");
		}
		Update(settings);
	}

	~MosaicTransition()
	{
		obs_enter_graphics();
		gs_texrender_destroy(cells_render_);
		gs_texrender_destroy(rows_render_);
		gs_effect_destroy(effect_);
		obs_leave_graphics();
	}

	// トランジションの update は、設定画面などの呼び出し元のスレッドで呼ばれ、描画スレッドと並行する。そのため値は atomic で持つ
	void Update(obs_data_t *settings)
	{
		fade_to_black_ = obs_data_get_bool(settings, kFadeToBlack);
		max_cell_size_ = static_cast<int>(std::clamp<long long>(obs_data_get_int(settings, kMaxCellSize),
									kMinMaxCellSize, kMaxMaxCellSize));
		switch_point_ =
			static_cast<float>(std::clamp<long long>(obs_data_get_int(settings, kSwitchPoint), 0, 100)) /
			100.0f;
	}

	void VideoRender()
	{
		const bool previous = gs_set_linear_srgb(true);
		obs_transition_video_render(context_, RenderCallback);
		gs_set_linear_srgb(previous);
	}

	void Render(gs_texture_t *a, gs_texture_t *b, float t, uint32_t cx, uint32_t cy)
	{
		// 色空間は最初に読む。texrender を begin すると、その texrender の色空間に切り替わるため
		const bool nonlinear = gs_get_color_space() == GS_CS_SRGB;
		const Frame frame = frame_at(t, switch_point_);
		gs_texture_t *tex = frame.use_b ? b : a;
		if (!Ready() || !tex) {
			return;
		}
		const int cell = std::max(
			1, static_cast<int>(std::lround(lerp(1.0f, static_cast<float>(max_cell_size_), frame.coarse))));
		const float black = fade_to_black_ ? frame.coarse : 0.0f;

		const auto cells_x = static_cast<uint32_t>((cx + cell - 1) / cell);
		const auto cells_y = static_cast<uint32_t>((cy + cell - 1) / cell);
		vec2 tex_size;
		vec2_set(&tex_size, static_cast<float>(cx), static_cast<float>(cy));
		vec2 rows_size;
		vec2_set(&rows_size, static_cast<float>(cells_x), static_cast<float>(cy));
		vec2 cells_size;
		vec2_set(&cells_size, static_cast<float>(cells_x), static_cast<float>(cells_y));
		// gs_technique_end はパスのたびにパラメーターの値を消すため、パスの直前に毎回すべて設定する
		const auto set_params = [&](gs_texture_t *rows, gs_texture_t *cells) {
			if (nonlinear) {
				gs_effect_set_texture(param_image_, tex);
			} else {
				gs_effect_set_texture_srgb(param_image_, tex);
			}
			gs_effect_set_texture(param_rows_, rows);
			gs_effect_set_texture(param_cells_, cells);
			gs_effect_set_vec2(param_tex_size_, &tex_size);
			gs_effect_set_float(param_cell_size_, static_cast<float>(cell));
			gs_effect_set_vec2(param_rows_size_, &rows_size);
			gs_effect_set_vec2(param_cells_size_, &cells_size);
			gs_effect_set_int(param_pair_count_, (cell + 1) / 2);
			gs_effect_set_float(param_black_, black);
		};

		const char *technique = "Direct";
		gs_texture_t *rows = nullptr;
		gs_texture_t *cells = nullptr;
		if (cell > 1) {
			// 1 パス目: 各行で、マスの幅ごとの平均を求める
			set_params(nullptr, nullptr);
			if (!DrawPass(rows_render_, "Rows", cells_x, cy)) {
				return;
			}
			rows = gs_texrender_get_texture(rows_render_);
			// 2 パス目: 行の平均を縦に平均し、マスごとの平均を求める
			set_params(rows, nullptr);
			if (!DrawPass(cells_render_, "Cells", cells_x, cells_y)) {
				return;
			}
			cells = gs_texrender_get_texture(cells_render_);
			technique = "Draw";
		}

		// 出力先へ描く。ブレンドは libobs が ONE, INVSRCALPHA に設定済み
		const bool previous_srgb = gs_framebuffer_srgb_enabled();
		gs_enable_framebuffer_srgb(!nonlinear);
		set_params(rows, cells);
		while (gs_effect_loop(effect_, technique)) {
			gs_draw_sprite(nullptr, 0, cx, cy);
		}
		gs_enable_framebuffer_srgb(previous_srgb);
	}

	// 設定済みのパラメーターで technique を描き、output (cx x cy) に書く
	bool DrawPass(gs_texrender_t *output, const char *technique, uint32_t cx, uint32_t cy)
	{
		// 1 フレームに複数回描かれる (プレビューとプロジェクターなど) ため、描くたびに reset する。
		// 描画済みの texrender は begin に失敗する
		gs_texrender_reset(output);
		if (!gs_texrender_begin(output, cx, cy)) {
			return false;
		}
		vec4 clear = {};
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(0.0f, static_cast<float>(cx), 0.0f, static_cast<float>(cy), -100.0f, 100.0f);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
		while (gs_effect_loop(effect_, technique)) {
			gs_draw_sprite(nullptr, 0, cx, cy);
		}
		gs_blend_state_pop();
		gs_texrender_end(output);
		return true;
	}

	bool Ready() const
	{
		return effect_ && param_image_ && param_rows_ && param_cells_ && param_tex_size_ && param_cell_size_ &&
		       param_rows_size_ && param_cells_size_ && param_pair_count_ && param_black_ && rows_render_ &&
		       cells_render_ && gs_effect_get_technique(effect_, "Rows") &&
		       gs_effect_get_technique(effect_, "Cells") && gs_effect_get_technique(effect_, "Draw") &&
		       gs_effect_get_technique(effect_, "Direct");
	}

	// 音声は「色へフェード」と同じく、前のシーンを切り替えの位置までに消し、次のシーンを切り替えの位置から上げる
	float MixA(float t) const { return lerp(1.0f - t, 0.0f, smoothstep(0.0f, switch_point_, t)); }
	float MixB(float t) const { return lerp(0.0f, t, smoothstep(switch_point_, 1.0f, t)); }

	bool AudioRender(uint64_t *ts_out, obs_source_audio_mix *audio, uint32_t mixers, size_t channels,
			 size_t sample_rate)
	{
		return obs_transition_audio_render(context_, ts_out, audio, mixers, channels, sample_rate, MixACallback,
						   MixBCallback);
	}

	enum gs_color_space ColorSpace(size_t count, const enum gs_color_space *preferred_spaces) const
	{
		const enum gs_color_space transition_space = obs_transition_video_get_color_space(context_);
		enum gs_color_space space = transition_space;
		for (size_t i = 0; i < count; ++i) {
			space = preferred_spaces[i];
			if (space == transition_space) {
				break;
			}
		}
		return space;
	}

	static void RenderCallback(void *data, gs_texture_t *a, gs_texture_t *b, float t, uint32_t cx, uint32_t cy)
	{
		static_cast<MosaicTransition *>(data)->Render(a, b, t, cx, cy);
	}

	static float MixACallback(void *data, float t) { return static_cast<MosaicTransition *>(data)->MixA(t); }
	static float MixBCallback(void *data, float t) { return static_cast<MosaicTransition *>(data)->MixB(t); }

	static const char *GetName(void *) { return obs_module_text("SimpleMosaicTransition"); }

	static void *Create(obs_data_t *settings, obs_source_t *context)
	{
		return new (std::nothrow) MosaicTransition(settings, context);
	}

	static void Destroy(void *data) { delete static_cast<MosaicTransition *>(data); }

	static void UpdateCallback(void *data, obs_data_t *settings)
	{
		static_cast<MosaicTransition *>(data)->Update(settings);
	}

	static void Defaults(obs_data_t *settings)
	{
		obs_data_set_default_bool(settings, kFadeToBlack, true);
		obs_data_set_default_int(settings, kMaxCellSize, kDefaultMaxCellSize);
		obs_data_set_default_int(settings, kSwitchPoint, kDefaultSwitchPoint);
	}

	static obs_properties_t *Properties(void *)
	{
		obs_properties_t *props = obs_properties_create();
		obs_properties_add_bool(props, kFadeToBlack, obs_module_text("FadeToBlack"));
		obs_property_t *cell = obs_properties_add_int_slider(
			props, kMaxCellSize, obs_module_text("MaxCellSize"), kMinMaxCellSize, kMaxMaxCellSize, 1);
		obs_property_int_set_suffix(cell, " px");
		obs_property_t *swp =
			obs_properties_add_int_slider(props, kSwitchPoint, obs_module_text("SwitchPoint"), 0, 100, 1);
		obs_property_int_set_suffix(swp, "%");
		return props;
	}

	static void VideoRenderCallback(void *data, gs_effect_t *)
	{
		static_cast<MosaicTransition *>(data)->VideoRender();
	}

	static bool AudioRenderCallback(void *data, uint64_t *ts_out, obs_source_audio_mix *audio, uint32_t mixers,
					size_t channels, size_t sample_rate)
	{
		return static_cast<MosaicTransition *>(data)->AudioRender(ts_out, audio, mixers, channels, sample_rate);
	}

	static enum gs_color_space ColorSpaceCallback(void *data, size_t count,
						      const enum gs_color_space *preferred_spaces)
	{
		return static_cast<MosaicTransition *>(data)->ColorSpace(count, preferred_spaces);
	}

private:
	obs_source_t *context_ = nullptr;
	gs_effect_t *effect_ = nullptr;
	gs_eparam_t *param_image_ = nullptr;
	gs_eparam_t *param_rows_ = nullptr;
	gs_eparam_t *param_cells_ = nullptr;
	gs_eparam_t *param_tex_size_ = nullptr;
	gs_eparam_t *param_cell_size_ = nullptr;
	gs_eparam_t *param_rows_size_ = nullptr;
	gs_eparam_t *param_cells_size_ = nullptr;
	gs_eparam_t *param_pair_count_ = nullptr;
	gs_eparam_t *param_black_ = nullptr;
	gs_texrender_t *rows_render_ = nullptr;
	gs_texrender_t *cells_render_ = nullptr;

	std::atomic<bool> fade_to_black_ = true;
	std::atomic<int> max_cell_size_ = kDefaultMaxCellSize;
	std::atomic<float> switch_point_ = kDefaultSwitchPoint / 100.0f;
};

} // namespace

void register_mosaic_transition(void)
{
	static obs_source_info info = {};
	info.id = "simple_mosaic_transition";
	info.type = OBS_SOURCE_TYPE_TRANSITION;
	info.get_name = MosaicTransition::GetName;
	info.create = MosaicTransition::Create;
	info.destroy = MosaicTransition::Destroy;
	info.update = MosaicTransition::UpdateCallback;
	info.get_defaults = MosaicTransition::Defaults;
	info.get_properties = MosaicTransition::Properties;
	info.video_render = MosaicTransition::VideoRenderCallback;
	info.audio_render = MosaicTransition::AudioRenderCallback;
	info.video_get_color_space = MosaicTransition::ColorSpaceCallback;
	obs_register_source(&info);
}
