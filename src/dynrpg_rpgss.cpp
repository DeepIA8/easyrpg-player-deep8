/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

// Headers
#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif

#include <array>
#include <cmath>
#include <map>
#include <memory>

#include "dynrpg_rpgss.h"
#include "baseui.h"
#include "bitmap.h"
#include "filefinder.h"
#include "game_map.h"
#include "game_party.h"
#include "graphics.h"
#include "sprite.h"
#include "picojson.h"
#include "main_data.h"
#include "game_variables.h"

class RpgssSprite;

// Lowest Z-order is drawn above. wtf
constexpr int layer_mask = (5 << 16);
constexpr int default_priority = Priority_Timer + layer_mask;

namespace {
	std::map<std::string, std::unique_ptr<RpgssSprite>> graphics;
	std::map<std::string, std::function<double(double, double, double, double)>> easing_funcs;

	// via http://www.gizma.com/easing/
	// via https://gist.github.com/Metallix/628de265d0a24e0c4acb
	// t - current time
	// b - initial value
	// c - relative change to initial value
	// d - duration
	double linear_easing(double t, double b, double c, double d) {
		return c*t/d + b;
	}

	double quadratic_in_easing(double t, double b, double c, double d) {
		t = t / d;
		return c*t*t + b;
	}

	double quadratic_out_easing(double t, double b, double c, double d) {
		t = t / d;
		return -c * t*(t-2) + b;
	}

	double quadratic_in_out_easing(double t, double b, double c, double d) {
		t = t / (d / 2);
		if (t < 1) {
			return c / 2 * t * t + b;
		} else {
			t = t - 1;
			return -c / 2 * (t * (t - 2) - 1) + b;
		}
	}

	double cubic_in_easing(double t, double b, double c, double d) {
		t = t / d;
		return c*t*t*t + b;
	}

	double cubic_out_easing(double t, double b, double c, double d) {
		t = (t / d) - 1;
		return c*(t*t*t + 1) + b;
	}

	double cubic_in_out_easing(double t, double b, double c, double d) {
		t = t / (d / 2);
		if (t < 1) {
			return c / 2 * t * t * t + b;
		} else {
			t -= 2;
			return c / 2 * (t * t * t + 2) + b;
		}
	}

	double sinusoidal_in_easing(double t, double b, double c, double d) {
		return -c * cos(t/d * (M_PI/2)) + c + b;
	}

	double sinusoidal_out_easing(double t, double b, double c, double d) {
		return c * sin(t/d * (M_PI/2)) + b;
	}

	double sinusoidal_in_out_easing(double t, double b, double c, double d) {
		return -c/2 * (cos(M_PI*t/d) - 1) + b;
	}

	double exponential_in_easing(double t, double b, double c, double d) {
		return c * pow(2, 10 * (t/d - 1)) + b;
	}

	double exponential_out_easing(double t, double b, double c, double d) {
		return c * (-pow(2, -10 * t/d) + 1) + b;
	}

	double exponential_in_out_easing(double t, double b, double c, double d) {
		t = t / (d / 2);
		if (t < 1) {
			return c / 2 * pow(2, 10 * (t - 1)) + b;
		} else {
			t = t - 1;
			return c / 2 * (-pow(2, -10 * t) + 2) + b;
		}
	}

	double circular_in_easing(double t, double b, double c, double d) {
		t = t / d;
		return -c * (sqrt(1 - t*t) - 1) + b;
	}

	double circular_out_easing(double t, double b, double c, double d) {
		t = (t / d) - 1;
		return c * sqrt(1 - t*t) + b;
	}

	double circular_in_out_easing(double t, double b, double c, double d) {
		t = t / (d / 2);
		if (t < 1) {
			return -c/2 * (sqrt(1 - t * t) - 1) + b;
		} else {
			t = t - 2;
			return c/2 * (sqrt(1 - t * t) + 1) + b;
		}
	}
}

class Effect {
public:
	Effect() {

	}

	Effect(double start) :
		start(0.0), current(start), finish(0.0), finish_frame(0), easing("linear")
	{
		// static effect
	}

	Effect(double start, double finish, int frames, const std::string& easing) :
		start(start), current(start), finish(finish), finish_frame(frames), easing(easing)
	{
		if (easing.empty()) {
			this->easing = "linear";
			return;
		}

		if (easing_funcs.find(easing) == easing_funcs.end()) {
			Output::Warning("RPGSS: Unsupported easing mode %s", easing.c_str());
			this->easing = "linear";
		}
	}

	double NextFrame() {
		if (finish_frame == 0) {
			return current;
		}

		bool ok = current_frame <= finish_frame;

		if (!ok) {
			return current;
		}

		if (easing_precalc.empty()) {
			PrecalculateEasing();
		}

		current += easing_precalc[current_frame];

		++current_frame;
		return current;
	}

	bool IsFinished() {
		return !(finish_frame > 0 && current_frame <= finish_frame);
	}

	picojson::object ToJson() {
		picojson::object o;
		o["start"] = picojson::value(start);
		o["finish"] = picojson::value(finish);
		o["current"] = picojson::value(current);
		o["current_frame"] = picojson::value((double)current_frame);
		o["finish_frame"] = picojson::value((double)finish_frame);
		o["easing"] = picojson::value(easing);

		return o;
	}

	static Effect FromJson(picojson::object& o) {
		Effect e;
		e.start = o["start"].get<double>();
		e.finish = o["finish"].get<double>();
		e.current = o["current"].get<double>();
		e.current_frame = (int)(o["current_frame"].get<double>());
		e.finish_frame = (int)(o["finish_frame"].get<double>());
		e.easing = o["easing"].get<std::string>();

		return e;
	}

	double start = 0.0;
	double finish = 0.0;
	double current = 0.0;
	int current_frame = 0;
	int finish_frame = 0;
	std::vector<double> easing_precalc;
	std::string easing = "linear";

private:
	void PrecalculateEasing() {
		easing_precalc.clear();
		easing_precalc.resize((size_t)finish_frame + 1);

		double prev = start;

		if (easing.empty()) {
			easing = "linear";
		}

		auto interpolate_easing = easing_funcs[easing];

		for (size_t i = 1; i < (size_t)finish_frame; ++i) {
			double e = interpolate_easing(i, start, finish - start, finish_frame);
			easing_precalc[i] = e - prev;
			prev = e;
		}

		easing_precalc[finish_frame] = finish - prev;
	}
};

class RpgssSprite {
public:
	enum BlendMode {
		BlendMode_Mix
	};

	enum FixedTo {
		FixedTo_Map,
		FixedTo_Screen,
		FixedTo_Mouse
	};

	RpgssSprite() {
	}

	RpgssSprite(const std::string& filename) {
		SetSpriteImage(filename);
		SetSpriteDefaults();
	}

	~RpgssSprite() {
	};

	bool SetSprite(const std::string& filename) {
		return SetSpriteImage(filename);
	}

	Sprite* const GetSprite() const {
		return sprite.get();
	}

	static double interpolate(double d, double x0, double x1) {
		return (x0 * (d - 1) + x1) / d;
	}

	static int frames(int ms) {
		return (int)(DEFAULT_FPS * ms / 1000.0f);
	}

	void Update() {
		if (!sprite) {
			return;
		}

		double x = movement_x.NextFrame();
		double y = movement_y.NextFrame();

		if (fixed_to == FixedTo_Map) {
			x -= (double)(Game_Map::GetDisplayX() / TILE_SIZE);
			y -= (double)(Game_Map::GetDisplayY() / TILE_SIZE);
		}

		if (rotation_time_left > 0) {
			// TODO: Rotate ccw
			current_angle = interpolate(rotation_time_left, current_angle, finish_angle);
			--rotation_time_left;
		}

		if (opacity_time_left > 0) {
			current_opacity = interpolate(opacity_time_left, current_opacity, finish_opacity);
			--opacity_time_left;
		}

		if (tone_time_left > 0)	{
			current_red = interpolate(tone_time_left, current_red, finish_red);
			current_green = interpolate(tone_time_left, current_green, finish_green);
			current_blue = interpolate(tone_time_left, current_blue, finish_blue);
			current_sat = interpolate(tone_time_left, current_sat, finish_sat);
			--tone_time_left;
		}

		if (rotate_forever_degree) {
			current_angle += (rotate_cw ? 1 : -1) * rotate_forever_degree;
		}

		sprite->SetX(x);
		sprite->SetY(y);
		sprite->SetZ(z);
		sprite->SetOx((int)(sprite->GetWidth() / 2));
		sprite->SetOy((int)(sprite->GetHeight() / 2));
		sprite->SetAngle(current_angle * (2 * M_PI) / 360);
		sprite->SetZoomX(zoom_x.NextFrame() / 100.0);
		sprite->SetZoomY(zoom_y.NextFrame() / 100.0);
		sprite->SetOpacity((int)(current_opacity));
		sprite->SetTone(Tone(current_red, current_green, current_blue, current_sat));
		sprite->SetVisible(visible);
	}

	void SetRelativeMovementXEffect(int ox, int ms, const std::string& easing) {
		movement_x = Effect(movement_x.current,(double)ox + movement_x.current, frames(ms), easing);
	}

	void SetRelativeMovementYEffect(int oy, int ms, const std::string& easing) {
		movement_y = Effect(movement_y.current,(double)oy + movement_y.current, frames(ms), easing);
	}

	void SetMovementXEffect(int x, int ms, const std::string& easing) {
		movement_x = Effect(movement_x.current,(double)x, frames(ms), easing);
	}

	void SetMovementYEffect(int y, int ms, const std::string& easing) {
		movement_y = Effect(movement_y.current,(double)y, frames(ms), easing);
	}

	void SetRelativeRotationEffect(double angle, int ms) {
		SetRotationEffect(angle >= 0.0, current_angle + angle, ms);
	}

	void SetRotationEffect(bool forward, double angle, int ms) {
		finish_angle = angle;
		rotation_time_left = frames(ms);
		rotate_forever_degree = 0.0;
		rotate_cw = forward;
	}

	void SetRotationForever(bool forward, int ms_per_full_rotation) {
		rotate_forever_degree = 360.0 / frames(ms_per_full_rotation);
		rotate_cw = forward;
	}

	void SetZoomXEffect(int new_zoom, int ms, const std::string& easing_) {
		zoom_x = Effect(zoom_x.current,(double)new_zoom, frames(ms), easing_);
	}

	void SetZoomYEffect(int new_zoom, int ms, const std::string& easing_) {
		zoom_y = Effect(zoom_y.current,(double)new_zoom, frames(ms), easing_);
	}

	void SetOpacityEffect(int new_opacity, int ms) {
		finish_opacity = new_opacity;
		opacity_time_left = frames(ms);
	}

	void SetToneEffect(Tone new_tone, int ms) {
		finish_red = new_tone.red;
		finish_green = new_tone.green;
		finish_blue = new_tone.blue;
		finish_sat = new_tone.gray;
		tone_time_left = frames(ms);
	}

	void SetFixedTo(FixedTo to) {
		if (fixed_to == FixedTo_Mouse) {
			Output::Warning("Sprite: Fixed to mouse not supported");
		} else {
			fixed_to = to;
		}
	}

	int GetX() const {
		return movement_x.current;
	}

	void SetX(int x) {
		movement_x = Effect((double)x);
	}

	int GetY() const {
		return movement_y.current;
	}

	void SetY(int y) {
		movement_y = Effect((double)y);
	}

	int GetZ() const{
		return z;
	}

	void SetZ(int z) {
		this->z = z;
	}

	void SetTone(Tone new_tone) {
		current_red = new_tone.red;
		current_green = new_tone.green;
		current_blue = new_tone.blue;
		current_sat = new_tone.gray;
		tone_time_left = 0;
	}

	void SetAngle(int degree) {
		current_angle = degree;
		rotation_time_left = 0;
		rotate_forever_degree = 0.0;
	}

	void SetZoomX(double zoom) {
		zoom_x = Effect((double)zoom);
	}

	void SetZoomY(double zoom) {
		zoom_y = Effect((double)zoom);
	}

	void SetVisible(bool v) {
		visible = v;
	}

	void SetOpacity(int opacity) {
		current_opacity = opacity;
		opacity_time_left = 0;
	}

	picojson::object Save() {
		picojson::object o;

		o["version"] = picojson::value(1.0);

		o["movement_x"] = picojson::value(movement_x.ToJson());
		o["movement_y"] = picojson::value(movement_y.ToJson());
		o["zoom_x"] = picojson::value(zoom_x.ToJson());
		o["zoom_y"] = picojson::value(zoom_y.ToJson());

		o["blendmode"] = picojson::value((double)blendmode);
		o["fixed_to"] = picojson::value((double)fixed_to);
		o["current_angle"] = picojson::value(current_angle);
		o["finish_angle"] = picojson::value(finish_angle);
		o["rotation_time_left"] = picojson::value((double)rotation_time_left);
		o["z"] = picojson::value((double)z);
		o["visible"] = picojson::value(visible);
		o["rotate_cw"] = picojson::value(rotate_cw);
		o["rotate_forever_degree"] = picojson::value(rotate_forever_degree);
		o["time_left"] = picojson::value((double)time_left);
		o["current_opacity"] = picojson::value(current_opacity);
		o["finish_opacity"] = picojson::value(finish_opacity);
		o["opacity_time_left"] = picojson::value((double)opacity_time_left);
		o["filename"] = picojson::value(file);
		o["current_red"] = picojson::value(current_red);
		o["current_green"] = picojson::value(current_green);
		o["current_blue"] = picojson::value(current_blue);
		o["current_sat"] = picojson::value(current_sat);
		o["finish_red"] = picojson::value(finish_red);
		o["finish_green"] = picojson::value(finish_green);
		o["finish_blue"] = picojson::value(finish_blue);
		o["finish_sat"] = picojson::value(finish_sat);
		o["tone_time_left"] = picojson::value((double)tone_time_left);

		return o;
	}

	static std::unique_ptr<RpgssSprite> Load(picojson::object& o) {
		auto sprite = std::make_unique<RpgssSprite>(o["filename"].get<std::string>());

		int version = 1;
		if (o["version"].is<double>()) {
			version = (int)o["version"].get<double>();
		}

		if (version > 1) {
			sprite->movement_x = Effect::FromJson(o["movement_x"].get<picojson::object>());
			sprite->movement_y = Effect::FromJson(o["movement_y"].get<picojson::object>());
			sprite->zoom_x = Effect::FromJson(o["zoom_x"].get<picojson::object>());
			sprite->zoom_y = Effect::FromJson(o["zoom_y"].get<picojson::object>());
		}

		sprite->blendmode = (int)(o["blendmode"].get<double>());
		sprite->fixed_to = (int)(o["fixed_to"].get<double>());
		sprite->current_angle = o["current_angle"].get<double>();
		sprite->finish_angle = o["finish_angle"].get<double>();
		sprite->rotation_time_left = (int)o["rotation_time_left"].get<double>();
		sprite->z = (int)o["z"].get<double>();
		sprite->visible = o["visible"].get<bool>();
		sprite->rotate_cw = o["rotate_cw"].get<bool>();
		sprite->rotate_forever_degree = o["rotate_forever_degree"].get<double>();
		sprite->time_left = (int)o["time_left"].get<double>();
		sprite->current_opacity = o["current_opacity"].get<double>();
		sprite->finish_opacity = o["finish_opacity"].get<double>();
		sprite->opacity_time_left = (int)o["opacity_time_left"].get<double>();
		sprite->current_red = o["current_red"].get<double>();
		sprite->current_green = o["current_green"].get<double>();
		sprite->current_blue = o["current_blue"].get<double>();
		sprite->current_sat = o["current_sat"].get<double>();
		sprite->finish_red = o["finish_red"].get<double>();
		sprite->finish_green = o["finish_green"].get<double>();
		sprite->finish_blue = o["finish_blue"].get<double>();
		sprite->finish_sat = o["finish_sat"].get<double>();
		sprite->tone_time_left = (int)o["tone_time_left"].get<double>();

		return sprite;
	}

private:
	void SetSpriteDefaults() {
		if (!sprite) {
			return;
		}

		movement_x.current = 160.0;
		movement_y.current = 120.0;
		z = default_priority;
		zoom_x.current = 100.0;
		zoom_y.current = 100.0;
	}

	bool SetSpriteImage(const std::string& filename) {
		// Does not go through the Cache code
		// No fancy stuff like checkerboard on load error :(

		file = FileFinder::FindDefault(filename);

		if (file.empty()) {
			Output::Warning("Sprite not found: %s", filename.c_str());
			return false;
		}
		sprite.reset(new Sprite());
		sprite->SetBitmap(Bitmap::Create(file));

		return true;
	}

	std::unique_ptr<Sprite> sprite;

	int blendmode = BlendMode_Mix;
	int fixed_to = FixedTo_Screen;

	Effect movement_x;
	Effect movement_y;
	Effect zoom_x;
	Effect zoom_y;

	double current_angle = 0.0;
	double finish_angle = 0.0;
	int rotation_time_left = 0;
	int z = 0;
	bool visible = true;

	bool rotate_cw = true;
	double rotate_forever_degree = 0.0;
	int time_left = 0;

	double current_opacity = 255.0;
	double finish_opacity = 0.0;
	int opacity_time_left = 0;

	double current_red = 128.0;
	double current_green = 128.0;
	double current_blue = 128.0;
	double current_sat = 128.0;

	double finish_red = 100;
	double finish_green = 100;
	double finish_blue = 100;
	double finish_sat = 100;
	int tone_time_left = 0;

	std::string file;
};

static bool AddSprite(dyn_arg_list args) {
	auto func = "add_sprite";
	bool okay;
	std::string id, filename;

	std::tie(id, filename) = DynRpg::ParseArgs<std::string, std::string>(func, args, &okay);
	if (!okay) {
		return true;
	}

	graphics[id] = std::make_unique<RpgssSprite>(filename);

	Sprite* const sprite = graphics[id]->GetSprite();

	if (!sprite) {
		return true;
	}

	RpgssSprite* graphic = graphics[id].get();

	switch (args.size()) {
		default:
		case 9:
		{
			auto angle = DynRpg::ParseSingleArg<float>(func, args.subspan(8), &okay);
			if (!okay) {
				return true;
			}
			graphic->SetAngle(angle);
		}
		case 8:
		{
			auto scale = DynRpg::ParseSingleArg<float>(func, args.subspan(7), &okay);
			if (!okay) {
				return true;
			}
			graphic->SetZoomX(scale);
			graphic->SetZoomY(scale);
		}
		case 7:
		{
			auto y = DynRpg::ParseSingleArg<int>(func, args.subspan(6), &okay);
			if (!okay) {
				return true;
			}
			graphic->SetY(y);
		}
		case 6:
		{
			auto x = DynRpg::ParseSingleArg<int>(func, args.subspan(5), &okay);
			if (!okay) {
				return true;
			}
			graphic->SetX(x);
		}
		case 5:
		{
			auto z = DynRpg::ParseSingleArg<int>(func, args.subspan(4), &okay);
			if (!okay) {
				return true;
			}
			graphic->SetZ(default_priority - z);
		}
		case 4:
		{
			auto visible = DynRpg::ParseSingleArg<int>(func, args.subspan(3), &okay);
			if (!okay) {
				return true;
			}
			graphic->SetVisible(visible > 0);
		}
		case 3:
			// Blend Mode
			break;
	}

	return true;
}

static bool RemoveSprite(dyn_arg_list args) {
	auto func = "remove_sprite";
	bool okay;

	auto id = DynRpg::ParseSingleArg<std::string>(func, args, &okay);
	if (!okay) {
		return true;
	}

	auto it = graphics.find(id);
	if (it != graphics.end()) {
		graphics.erase(it);
	}

	return true;
}

static bool SetSpriteBlendMode(dyn_arg_list args) {
	auto func = "set_sprite_blend_mode";
	bool okay;

#if 0
	// TODO unsupported
	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_STR_ARG(1, blendmode)

	Output::Post("blendmode %s", blendmode.c_str());

	// ERRORCHK

	//graphics[id]->SetSprite(filename);
#endif
	return true;
}

static bool SetSpriteImage(dyn_arg_list args) {
	auto func = "set_sprite_image";
	bool okay;
	std::string id, filename;

	std::tie(id, filename) = DynRpg::ParseArgs<std::string, std::string>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	graphics[id]->SetSprite(filename);

	return true;
}

static bool BindSpriteTo(dyn_arg_list args) {
	auto func = "bind_sprite_to";
	bool okay;
	std::string id, coordsys;
	std::tie(id, coordsys) = DynRpg::ParseArgs<std::string, std::string>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	graphics[id]->SetFixedTo(coordsys == "mouse" ? RpgssSprite::FixedTo_Mouse :
		coordsys == "map" ? RpgssSprite::FixedTo_Map : RpgssSprite::FixedTo_Screen);

	return true;
}


static bool MoveSpriteBy(dyn_arg_list args) {
	auto func = "move_sprite_by";
	bool okay;
	std::string id;
	int ox, oy, ms;
	std::tie(id, ox, oy, ms) = DynRpg::ParseArgs<std::string, int, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	if (args.size() >= 5) {
		auto easing_x = DynRpg::ParseSingleArg<std::string>(func, args.subspan(4), &okay);
		if (!okay)
			return true;
		graphics[id]->SetRelativeMovementXEffect(ox, ms, easing_x);

		if (args.size() >= 6) {
			auto easing_y = DynRpg::ParseSingleArg<std::string>(func, args.subspan(5), &okay);
			if (!okay)
				return true;
			graphics[id]->SetRelativeMovementYEffect(oy, ms, easing_y);
		} else {
			graphics[id]->SetRelativeMovementYEffect(oy, ms, easing_x);
		}
	} else {
		graphics[id]->SetRelativeMovementXEffect(ox, ms, "linear");
		graphics[id]->SetRelativeMovementYEffect(oy, ms, "linear");
	}

	return true;
}

static bool MoveXSpriteBy(dyn_arg_list args) {
	auto func = "move_x_sprite_by";
	bool okay;
	std::string id;
	int ox, ms;
	std::tie(id, ox, ms) = DynRpg::ParseArgs<std::string, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	if (args.size() >= 4) {
		auto easing = DynRpg::ParseSingleArg<std::string>(func, args.subspan(3), &okay);
		if (!okay)
			return true;
		graphics[id]->SetRelativeMovementXEffect(ox, ms, easing);
	} else {
		graphics[id]->SetRelativeMovementXEffect(ox, ms, "linear");
	}

	return true;
}

static bool MoveYSpriteBy(dyn_arg_list args) {
	auto func = "move_y_sprite_by";
	bool okay;
	std::string id;
	int oy, ms;
	std::tie(id, oy, ms) = DynRpg::ParseArgs<std::string, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	if (args.size() >= 4) {
		auto easing = DynRpg::ParseSingleArg<std::string>(func, args.subspan(3), &okay);
		if (!okay)
			return true;
		graphics[id]->SetRelativeMovementYEffect(oy, ms, easing);
	} else {
		graphics[id]->SetRelativeMovementYEffect(oy, ms, "linear");
	}

	return true;
}

static bool MoveSpriteTo(dyn_arg_list args) {
	auto func = "move_sprite_to";
	bool okay;
	std::string id;
	int ox, oy, ms;
	std::tie(id, ox, oy, ms) = DynRpg::ParseArgs<std::string, int, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	if (args.size() >= 5) {
		auto easing_x = DynRpg::ParseSingleArg<std::string>(func, args.subspan(4), &okay);
		if (!okay)
			return true;
		graphics[id]->SetMovementXEffect(ox, ms, easing_x);

		if (args.size() >= 6) {
			auto easing_y = DynRpg::ParseSingleArg<std::string>(func, args.subspan(5), &okay);
			if (!okay)
				return true;
			graphics[id]->SetMovementYEffect(oy, ms, easing_y);
		} else {
			graphics[id]->SetMovementYEffect(oy, ms, easing_x);
		}
	} else {
		graphics[id]->SetMovementXEffect(ox, ms, "linear");
		graphics[id]->SetMovementYEffect(oy, ms, "linear");
	}

	return true;
}

static bool MoveXSpriteTo(dyn_arg_list args) {
	auto func = "move_x_sprite_to";
	bool okay;
	std::string id;
	int ox, ms;
	std::tie(id, ox, ms) = DynRpg::ParseArgs<std::string, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	if (args.size() >= 4) {
		auto easing = DynRpg::ParseSingleArg<std::string>(func, args.subspan(3), &okay);
		if (!okay)
			return true;
		graphics[id]->SetMovementXEffect(ox, ms, easing);
	} else {
		graphics[id]->SetMovementXEffect(ox, ms, "linear");
	}

	return true;
}

static bool MoveYSpriteTo(dyn_arg_list args) {
	auto func = "move_y_sprite_to";
	bool okay;
	std::string id;
	int oy, ms;
	std::tie(id, oy, ms) = DynRpg::ParseArgs<std::string, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	if (args.size() >= 4) {
		auto easing = DynRpg::ParseSingleArg<std::string>(func, args.subspan(3), &okay);
		if (!okay)
			return true;
		graphics[id]->SetMovementYEffect(oy, ms, easing);
	} else {
		graphics[id]->SetMovementYEffect(oy, ms, "linear");
	}

	return true;
}

static bool ScaleSpriteTo(dyn_arg_list args) {
	auto func = "scale_sprite_to";
	bool okay;
	std::string id;
	int scale, ms;
	std::tie(id, scale, ms) = DynRpg::ParseArgs<std::string, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	if (args.size() >= 4) {
		auto easing_x = DynRpg::ParseSingleArg<std::string>(func, args.subspan(3), &okay);
		if (!okay)
			return true;
		graphics[id]->SetZoomXEffect(scale, ms, easing_x);

		if (args.size() >= 5) {
			auto easing_y = DynRpg::ParseSingleArg<std::string>(func, args.subspan(4), &okay);
			if (!okay)
				return true;
			graphics[id]->SetZoomYEffect(scale, ms, easing_y);
		} else {
			graphics[id]->SetZoomYEffect(scale, ms, easing_x);
		}

	} else {
		graphics[id]->SetZoomXEffect(scale, ms, "linear");
		graphics[id]->SetZoomYEffect(scale, ms, "linear");
	}

	return true;
}

static bool ScaleXSpriteTo(dyn_arg_list args) {
	auto func = "scale_x_sprite_to";
	bool okay;
	std::string id;
	int scale, ms;
	std::tie(id, scale, ms) = DynRpg::ParseArgs<std::string, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	if (args.size() >= 4) {
		auto easing = DynRpg::ParseSingleArg<std::string>(func, args.subspan(3), &okay);
		if (!okay)
			return true;
		graphics[id]->SetZoomXEffect(scale, ms, easing);
	} else {
		graphics[id]->SetZoomXEffect(scale, ms, "linear");
	}

	return true;
}


static bool ScaleYSpriteTo(dyn_arg_list args) {
	auto func = "scale_y_sprite_to";
	bool okay;
	std::string id;
	int scale, ms;
	std::tie(id, scale, ms) = DynRpg::ParseArgs<std::string, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	if (args.size() >= 4) {
		auto easing = DynRpg::ParseSingleArg<std::string>(func, args.subspan(3), &okay);
		if (!okay)
			return true;
		graphics[id]->SetZoomYEffect(scale, ms, easing);
	} else {
		graphics[id]->SetZoomYEffect(scale, ms, "linear");
	}

	return true;
}

static bool RotateSpriteBy(dyn_arg_list args) {
	auto func = "rotate_sprite_by";
	bool okay;
	std::string id;
	int angle, ms;
	std::tie(id, angle, ms) = DynRpg::ParseArgs<std::string, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	graphics[id]->SetRelativeRotationEffect(-angle, ms);

	return true;
}

static bool RotateSpriteTo(dyn_arg_list args) {
	auto func = "rotate_sprite_to";
	bool okay;
	std::string id, direction;
	int angle, ms;
	std::tie(id, direction, angle, ms) = DynRpg::ParseArgs<std::string, std::string, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	graphics[id]->SetRotationEffect(direction == "cw", angle, ms);

	return true;
}

static bool RotateSpriteForever(dyn_arg_list args) {
	auto func = "rotate_sprite_forever";
	bool okay;
	std::string id, direction;
	int ms;
	std::tie(id, direction, ms) = DynRpg::ParseArgs<std::string, std::string, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	graphics[id]->SetRotationForever(direction == "cw", ms);

	return true;
}

static bool StopSpriteRotation(dyn_arg_list args) {
	auto func = "stop_sprite_rotation";
	bool okay;
	auto id = DynRpg::ParseSingleArg<std::string>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	graphics[id]->SetRotationEffect(true, 0, 0);

	return true;
}

static bool SetSpriteOpacity(dyn_arg_list args) {
	auto func = "set_sprite_opacity";
	bool okay;
	std::string id;
	int opacity;
	std::tie(id, opacity) = DynRpg::ParseArgs<std::string, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	graphics[id]->SetOpacity(opacity);

	return true;
}

static bool ShiftSpriteOpacityTo(dyn_arg_list args) {
	auto func = "shift_sprite_opacity_to";
	bool okay;
	std::string id;
	int opacity, ms;
	std::tie(id, opacity, ms) = DynRpg::ParseArgs<std::string, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	graphics[id]->SetOpacityEffect(opacity, ms);

	return true;
}

static bool SetSpriteColor(dyn_arg_list args) {
	auto func = "set_sprite_color";
	bool okay;
	std::string id;
	int red, green, blue;
	std::tie(id, red, green, blue) = DynRpg::ParseArgs<std::string, int, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	int sat = 100;

	if (args.size() > 4) {
		sat = DynRpg::ParseSingleArg<int>(func, args.subspan(3), &okay);
		if (!okay) {
			return true;
		}
	}

	graphics[id]->SetTone(Tone((int)(red * 128 / 100),
		(int)(green * 128 / 100),
		(int)(blue * 128 / 100),
		(int)(sat * 128 / 100)));

	return true;
}

static bool ShiftSpriteColorTo(dyn_arg_list args) {
	auto func = "shift_sprite_color_to";
	bool okay;
	std::string id;
	int red, green, blue, sat, ms;
	std::tie(id, red, green, blue, sat, ms) = DynRpg::ParseArgs<std::string, int, int, int, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	graphics[id]->SetToneEffect(Tone((int)(red * 128 / 100),
		(int)(green * 128 / 100),
		(int)(blue * 128 / 100),
		(int)(sat * 128 / 100)), ms);

	return true;
}

static bool SetZ(dyn_arg_list args) {
	auto func = "set_sprite_z";
	bool okay;
	std::string id;
	int z;
	std::tie(id, z) = DynRpg::ParseArgs<std::string, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	int layer_z = graphics[id]->GetZ() & 0xFFFF0000;

	graphics[id]->SetZ(layer_z - z);

	return true;
}

static bool SetLayer(dyn_arg_list args) {
	auto func = "set_sprite_layer";
	bool okay;
	std::string id;
	int layer;
	std::tie(id, layer) = DynRpg::ParseArgs<std::string, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	int z;

	switch (layer) {
		case 1:
			z = Priority_Background;
			break;
		case 2:
			z = Priority_TilesetBelow;
			break;
		case 3:
			z = Priority_EventsBelow;
			break;
		case 4:
			z = Priority_Player;
			break;
		case 5:
			z = Priority_TilesetAbove;
			break;
		case 6:
			z = Priority_EventsAbove;
			break;
		case 7:
			z = Priority_PictureNew;
			break;
		case 8:
			z = Priority_BattleAnimation;
			break;
		case 9:
			z = Priority_Window;
			break;
		case 10:
			z = Priority_Timer;
			break;
		default:
			z = 0;
	}

	int old_z = graphics[id]->GetZ() & 0x0000FFFF;

	graphics[id]->SetZ(z + layer_mask + old_z);

	return true;
}

static bool GetSpritePosition(dyn_arg_list args) {
	auto func = "get_sprite_position";
	bool okay;
	std::string id;
	int var_x, var_y;
	std::tie(id, var_x, var_y) = DynRpg::ParseArgs<std::string, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	Main_Data::game_variables->Set(var_x, graphics[id]->GetX());
	Main_Data::game_variables->Set(var_y, graphics[id]->GetY());

	return true;
}

static bool SetSpritePosition(dyn_arg_list args) {
	auto func = "set_sprite_position";
	bool okay;
	std::string id;
	int x, y;
	std::tie(id, x, y) = DynRpg::ParseArgs<std::string, int, int>(func, args, &okay);
	if (!okay) {
		return true;
	}
	if (graphics.find(id) == graphics.end()) {
		return true;
	}

	graphics[id]->SetX(x);
	graphics[id]->SetY(y);

	return true;
}

void DynRpg::Rpgss::RegisterFunctions() {
	DynRpg::RegisterFunction("add_sprite", AddSprite);
	DynRpg::RegisterFunction("set_sprite_blend_mode", SetSpriteBlendMode);
	DynRpg::RegisterFunction("remove_sprite", RemoveSprite);
	DynRpg::RegisterFunction("set_sprite_image", SetSpriteImage);
	DynRpg::RegisterFunction("bind_sprite_to", BindSpriteTo);
	DynRpg::RegisterFunction("move_x_sprite_by", MoveXSpriteBy);
	DynRpg::RegisterFunction("move_y_sprite_by", MoveYSpriteBy);
	DynRpg::RegisterFunction("move_sprite_by", MoveSpriteBy);
	DynRpg::RegisterFunction("move_x_sprite_to", MoveXSpriteTo);
	DynRpg::RegisterFunction("move_y_sprite_to", MoveYSpriteTo);
	DynRpg::RegisterFunction("move_sprite_to", MoveSpriteTo);
	DynRpg::RegisterFunction("scale_sprite_to", ScaleSpriteTo);
	DynRpg::RegisterFunction("scale_x_sprite_to", ScaleXSpriteTo);
	DynRpg::RegisterFunction("scale_y_sprite_to", ScaleYSpriteTo);
	DynRpg::RegisterFunction("rotate_sprite_by", RotateSpriteBy);
	DynRpg::RegisterFunction("rotate_sprite_to", RotateSpriteTo);
	DynRpg::RegisterFunction("rotate_sprite_forever", RotateSpriteForever);
	DynRpg::RegisterFunction("stop_sprite_rotation", StopSpriteRotation);
	DynRpg::RegisterFunction("set_sprite_opacity", SetSpriteOpacity);
	DynRpg::RegisterFunction("shift_sprite_opacity_to", ShiftSpriteOpacityTo);
	DynRpg::RegisterFunction("set_sprite_z", SetZ);
	DynRpg::RegisterFunction("set_sprite_layer", SetLayer);
	DynRpg::RegisterFunction("set_sprite_color", SetSpriteColor);
	DynRpg::RegisterFunction("shift_sprite_color_to", ShiftSpriteColorTo);
	DynRpg::RegisterFunction("get_sprite_position", GetSpritePosition);
	DynRpg::RegisterFunction("set_sprite_position", SetSpritePosition);

	easing_funcs["linear"] = linear_easing;
	easing_funcs["quadratic in"] = quadratic_in_easing;
	easing_funcs["quadratic out"] = quadratic_out_easing;
	easing_funcs["quadratic in/out"] = quadratic_in_out_easing;
	easing_funcs["cubic in"] = cubic_in_easing;
	easing_funcs["cubic out"] = cubic_out_easing;
	easing_funcs["cubic in/out"] = cubic_in_out_easing;
	easing_funcs["sinusoidal in"] = sinusoidal_in_easing;
	easing_funcs["sinusoidal out"] = sinusoidal_out_easing;
	easing_funcs["sinusoidal in/out"] = sinusoidal_in_out_easing;
	easing_funcs["exponential in"] = exponential_in_easing;
	easing_funcs["exponential out"] = exponential_out_easing;
	easing_funcs["exponential in/out"] = exponential_in_out_easing;
	easing_funcs["circular in"] = circular_in_easing;
	easing_funcs["circular out"] = circular_out_easing;
	easing_funcs["circular in/out"] = circular_in_out_easing;
}

void DynRpg::Rpgss::Update() {
	for (auto& g : graphics) {
		g.second->Update();
	}
}

DynRpg::Rpgss::~Rpgss() {
	graphics.clear();

	easing_funcs.clear();
}

void DynRpg::Rpgss::Load(const std::vector<uint8_t> &in) {
	picojson::value v;
	std::string s;
	s.resize(in.size());
	std::copy(in.begin(), in.end(), s.begin());

	picojson::parse(v, s);

	for (auto& k : v.get<picojson::object>()) {
		graphics[k.first] = RpgssSprite::Load(k.second.get<picojson::object>());
	}
}

std::vector<uint8_t> DynRpg::Rpgss::Save() {
	picojson::object o;

	for (auto& g : graphics) {
		o[g.first] = picojson::value(g.second->Save());
	}

	std::string s = picojson::value(o).serialize();

	std::vector<uint8_t> v;
	v.resize(s.size());

	std::copy(s.begin(), s.end(), v.begin());

	return v;
}

void DynRpg::Rpgss::OnMapChange() {
	graphics.clear();
}
