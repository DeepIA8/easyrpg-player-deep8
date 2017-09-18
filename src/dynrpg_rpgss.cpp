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
#include <map>

#include "dynrpg_rpgss.h"
#include "baseui.h"
#include "bitmap.h"
#include "filefinder.h"
#include "game_map.h"
#include "game_party.h"
#include "graphics.h"
#include "sprite.h"
#include "picojson.h"

class RpgssSprite;

// Lowest Z-order is drawn above. wtf
constexpr int layer_mask = (5 << 16);
constexpr int layer_offset = 0xFFFF / 2;
constexpr int default_priority = Priority_Timer + layer_mask + layer_offset;

namespace {
	std::map<std::string, std::unique_ptr<RpgssSprite>> graphics;
}

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

		if (fixed_to == FixedTo_Map) {
			if (old_map_x != Game_Map::GetDisplayX()) {
				double mx = (old_map_x - Game_Map::GetDisplayX()) / (double)TILE_SIZE;

				finish_x += mx;
				current_x += mx;
			}
			if (old_map_y != Game_Map::GetDisplayY()) {
				double my = (old_map_y - Game_Map::GetDisplayY()) / (double)TILE_SIZE;

				finish_y += my;
				current_y += my;
			}

			old_map_x = Game_Map::GetDisplayX();
			old_map_y = Game_Map::GetDisplayY();
		}

		if (movement_time_left > 0) {
			current_x = interpolate(movement_time_left, current_x, finish_x);
			current_y = interpolate(movement_time_left, current_y, finish_y);
			--movement_time_left;
		}

		if (rotation_time_left > 0) {
			// TODO: Rotate ccw
			current_angle = interpolate(rotation_time_left, current_angle, finish_angle);
			--rotation_time_left;
		}

		if (zoom_time_left > 0) {
			current_zoom = interpolate(zoom_time_left, current_zoom, finish_zoom);
			--zoom_time_left;
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

		sprite->SetX(current_x);
		sprite->SetY(current_y);
		sprite->SetOx((int)(sprite->GetWidth() / 2));
		sprite->SetOy((int)(sprite->GetHeight() / 2));
		sprite->SetAngle(current_angle);
		sprite->SetZoomX(current_zoom / 100.0);
		sprite->SetZoomY(current_zoom / 100.0);
		sprite->SetOpacity((int)(current_opacity));
		sprite->SetTone(Tone(current_red, current_green, current_blue, current_sat));
		sprite->SetVisible(visible);
	}

	void SetRelativeMovementEffect(int ox, int oy, int ms) {
		SetMovementEffect(ox + current_x, oy + current_y, ms);
	}

	void SetMovementEffect(int x, int y, int ms) {
		finish_x = (double)x;
		finish_y = (double)y;
		movement_time_left = frames(ms);
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

	void SetZoomEffect(int new_zoom, int ms) {
		finish_zoom = new_zoom;
		zoom_time_left = frames(ms);
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
		fixed_to = to;

		if (fixed_to == FixedTo_Mouse) {
			Output::Warning("Sprite: Fixed to mouse not supported");
		} else if (fixed_to == FixedTo_Map) {
			if (!sprite) {
				return;
			}
			old_map_x = current_x * TILE_SIZE;
			old_map_y = current_y * TILE_SIZE;

			current_x = 0.0;
			current_y = 0.0;
		}
	}

	void SetX(int x) {
		current_x = x;
		movement_time_left = 0;
	}

	void SetY(int y) {
		current_y = y;
		movement_time_left = 0;
	}

	int GetZ() {
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

	void SetZoom(double zoom) {
		current_zoom = zoom;
		zoom_time_left = 0;
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
		o["blendmode"] = picojson::value((double)blendmode);
		o["fixed_to"] = picojson::value((double)fixed_to);
		o["current_x"] = picojson::value(current_x);
		o["current_y"] = picojson::value(current_y);
		o["finish_x"] = picojson::value(finish_x);
		o["finish_y"] = picojson::value(finish_y);
		o["movement_time_left"] = picojson::value((double)movement_time_left);
		o["current_zoom"] = picojson::value(current_zoom);
		o["finish_zoom"] = picojson::value(finish_zoom);
		o["zoom_time_left"] = picojson::value((double)zoom_time_left);
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
		o["old_map_x"] = picojson::value((double)old_map_x);
		o["old_map_y"] = picojson::value((double)old_map_y);
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
		auto sprite = std::unique_ptr<RpgssSprite>(new RpgssSprite(o["filename"].get<std::string>()));

		sprite->blendmode = (int)(o["blendmode"].get<double>());
		sprite->fixed_to = (int)(o["fixed_to"].get<double>());
		sprite->current_x = o["current_x"].get<double>();
		sprite->current_y = o["current_y"].get<double>();
		sprite->finish_x = o["finish_x"].get<double>();
		sprite->finish_y = o["finish_y"].get<double>();
		sprite->movement_time_left = (int)o["movement_time_left"].get<double>();
		sprite->current_zoom = o["current_zoom"].get<double>();
		sprite->finish_zoom = o["finish_zoom"].get<double>();
		sprite->zoom_time_left = (int)o["zoom_time_left"].get<double>();
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
		sprite->old_map_x = (int)o["old_map_x"].get<double>();
		sprite->old_map_y = (int)o["old_map_y"].get<double>();

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

		current_x = 160.0;
		current_y = 120.0;
		z = default_priority;
		current_zoom = 100.0;

		old_map_x = Game_Map::GetDisplayX();
		old_map_y = Game_Map::GetDisplayY();
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

	double current_x = 0.0;
	double current_y = 0.0;
	double finish_x = 0.0;
	double finish_y = 0.0;
	int movement_time_left = 0;
	double current_zoom = 100.0;
	double finish_zoom = 0.0;
	int zoom_time_left = 0;
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

	int old_map_x;
	int old_map_y;

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

static bool AddSprite(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("add_sprite")

	DYNRPG_CHECK_ARG_LENGTH(2);

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_STR_ARG(1, filename)

	graphics[id].reset(new RpgssSprite(filename));

	Sprite* const sprite = graphics[id]->GetSprite();

	if (!sprite) {
		return true;
	}

	RpgssSprite* graphic = graphics[id].get();

	switch (args.size()) {
		default:
		case 9:
		{
			DYNRPG_GET_FLOAT_ARG(8, angle)
			graphic->SetAngle(angle);
		}
		case 8:
		{
			DYNRPG_GET_FLOAT_ARG(7, scale)
			graphic->SetZoom(scale);
		}
		case 7:
		{
			DYNRPG_GET_INT_ARG(6, y)
			graphic->SetY(y);
		}
		case 6:
		{
			DYNRPG_GET_INT_ARG(5, x)
			graphic->SetX(x);
		}
		case 5:
		{
			DYNRPG_GET_INT_ARG(4, z)
			graphic->SetZ(default_priority - z);
		}
		case 4:
		{
			DYNRPG_GET_INT_ARG(3, visible)
			graphic->SetVisible(visible > 0);
		}
		case 3:
			// Blend Mode
			break;
	}

	return true;
}

static bool RemoveSprite(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("remove_sprite")

	DYNRPG_CHECK_ARG_LENGTH(1)

	DYNRPG_GET_STR_ARG(0, id)

	auto it = graphics.find(id);
	if (it != graphics.end()) {
		graphics.erase(it);
	}

	return true;
}

static bool SetSpriteBlendMode(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("set_sprite_blend_mode")

	DYNRPG_CHECK_ARG_LENGTH(2)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_STR_ARG(1, blendmode)

	Output::Post("blendmode %s", blendmode.c_str());

	// ERRORCHK

	//graphics[id]->SetSprite(filename);

	return true;
}

static bool SetSpriteImage(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("set_sprite_image")

	DYNRPG_CHECK_ARG_LENGTH(2)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_STR_ARG(1, filename)

	// ERRORCHK

	graphics[id]->SetSprite(filename);

	return true;
}

static bool BindSpriteTo(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("bind_sprite_to")

	DYNRPG_CHECK_ARG_LENGTH(2)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_STR_ARG(1, coordsys)

	// ERRORCHK

	graphics[id]->SetFixedTo(coordsys == "mouse" ? RpgssSprite::FixedTo_Mouse :
		coordsys == "map" ? RpgssSprite::FixedTo_Map : RpgssSprite::FixedTo_Screen);

	return true;
}

static bool MoveSpriteBy(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("move_sprite_by")

	DYNRPG_CHECK_ARG_LENGTH(4)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_INT_ARG(1, ox)
	DYNRPG_GET_INT_ARG(2, oy)
	DYNRPG_GET_INT_ARG(3, ms)
	//DYNRPG_GET_INT_ARG(4, easing)

	// ERRORCHK

	graphics[id]->SetRelativeMovementEffect(ox, oy, ms);

	return true;
}

static bool MoveSpriteTo(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("move_sprite_to")

	DYNRPG_CHECK_ARG_LENGTH(4)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_INT_ARG(1, ox)
	DYNRPG_GET_INT_ARG(2, oy)
	DYNRPG_GET_INT_ARG(3, ms)
	//DYNRPG_GET_INT_ARG(4, easing)

	// ERRORCHK

	graphics[id]->SetMovementEffect(ox, oy, ms);

	return true;
}

static bool ScaleSpriteTo(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("scale_sprite_to")

	DYNRPG_CHECK_ARG_LENGTH(3)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_INT_ARG(1, scale)
	DYNRPG_GET_INT_ARG(2, ms)

	// ERRORCHK

	graphics[id]->SetZoomEffect(scale, ms);

	return true;
}

static bool RotateSpriteBy(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("rotate_sprite_by")

	DYNRPG_CHECK_ARG_LENGTH(3)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_INT_ARG(1, angle)
	DYNRPG_GET_INT_ARG(2, ms)

	// ERRORCHK

	graphics[id]->SetRelativeRotationEffect(-angle, ms);

	return true;
}

static bool RotateSpriteTo(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("rotate_sprite_to")

	DYNRPG_CHECK_ARG_LENGTH(4)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_STR_ARG(1, direction)
	DYNRPG_GET_INT_ARG(2, angle)
	DYNRPG_GET_INT_ARG(3, ms)

	// ERRORCHK

	graphics[id]->SetRotationEffect(direction == "cw", angle, ms);

	return true;
}

static bool RotateSpriteForever(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("rotate_sprite_forever")

	DYNRPG_CHECK_ARG_LENGTH(3)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_STR_ARG(1, direction)
	DYNRPG_GET_INT_ARG(2, ms)

	// ERRORCHK

	graphics[id]->SetRotationForever(direction == "cw", ms);

	return true;
}

static bool StopSpriteRotation(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("stop_sprite_rotation")

	DYNRPG_CHECK_ARG_LENGTH(1)

	DYNRPG_GET_STR_ARG(0, id)

	// ERRORCHK

	graphics[id]->SetRotationEffect(true, 0, 0);

	return true;
}

static bool SetSpriteOpacity(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("set_sprite_opacity")

	DYNRPG_CHECK_ARG_LENGTH(2)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_INT_ARG(1, opacity)

	// ERRORCHK

	graphics[id]->SetOpacity(opacity);

	return true;
}

static bool ShiftSpriteOpacityTo(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("shift_sprite_opacity_to")

	DYNRPG_CHECK_ARG_LENGTH(3)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_INT_ARG(1, opacity)
	DYNRPG_GET_INT_ARG(2, ms)

	// ERRORCHK

	graphics[id]->SetOpacityEffect(opacity, ms);

	return true;
}

static bool SetSpriteColor(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("set_sprite_color")

	DYNRPG_CHECK_ARG_LENGTH(4)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_INT_ARG(1, red)
	DYNRPG_GET_INT_ARG(2, green)
	DYNRPG_GET_INT_ARG(3, blue)

	int sat = 100;

	if (args.size() > 4) {
		DYNRPG_GET_INT_ARG(4, s);
		sat = s;
	}

	// ERRORCHK

	graphics[id]->SetTone(Tone((int)(red * 128 / 100),
		(int)(green * 128 / 100),
		(int)(blue * 128 / 100),
		(int)(sat * 128 / 100)));

	return true;
}

static bool ShiftSpriteColorTo(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("shift_sprite_color_to")

	DYNRPG_CHECK_ARG_LENGTH(6)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_INT_ARG(1, red)
	DYNRPG_GET_INT_ARG(2, green)
	DYNRPG_GET_INT_ARG(3, blue)
	DYNRPG_GET_INT_ARG(4, sat)
	DYNRPG_GET_INT_ARG(5, ms)

	// ERRORCHK

	graphics[id]->SetToneEffect(Tone((int)(red * 128 / 100),
		(int)(green * 128 / 100),
		(int)(blue * 128 / 100),
		(int)(sat * 128 / 100)), ms);

	return true;
}

static bool SetZ(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("set_sprite_z")

	DYNRPG_CHECK_ARG_LENGTH(3)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_INT_ARG(1, z)

	// ERRORCHK

	int layer_z = graphics[id]->GetZ() & 0xFFFF0000 + layer_offset;

	graphics[id]->SetZ(layer_z - z);

	return true;
}

static bool SetLayer(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("set_sprite_layer")

	DYNRPG_CHECK_ARG_LENGTH(2)

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_INT_ARG(1, layer)

	// ERRORCHK

	int z = 0;

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
	}

	int old_z = graphics[id]->GetZ() & 0x00FFFFFF;

	graphics[id]->SetZ(z + layer_mask + old_z);

	return true;
}

std::string DynRpg::Rpgss::GetIdentifier() {
	return "RpgssDeep8";
}

void DynRpg::Rpgss::RegisterFunctions() {
	DynRpg::RegisterFunction("add_sprite", AddSprite);
	DynRpg::RegisterFunction("set_sprite_blend_mode", SetSpriteBlendMode);
	DynRpg::RegisterFunction("remove_sprite", RemoveSprite);
	DynRpg::RegisterFunction("set_sprite_image", SetSpriteImage);
	DynRpg::RegisterFunction("bind_sprite_to", BindSpriteTo);
	DynRpg::RegisterFunction("move_sprite_by", MoveSpriteBy);
	DynRpg::RegisterFunction("move_sprite_to", MoveSpriteTo);
	DynRpg::RegisterFunction("scale_sprite_to", ScaleSpriteTo);
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

	// TODO : set/shift_sprite_color
	// TODO : Rotation ist komisch
	// set_sprite_color(name, r, g, b)
	// shift_sprite_color_to(name, r, g, b)
}

void DynRpg::Rpgss::Update() {
	for (auto& g : graphics) {
		g.second->Update();
	}
}

DynRpg::Rpgss::~Rpgss() {
	graphics.clear();
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
