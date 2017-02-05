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

class RpgssSprite;

namespace {
	std::map<std::string, std::unique_ptr<RpgssSprite>> graphics;
}

class RpgssSprite {
public:
	enum class BlendMode {
		Mix
	};

	enum class FixedTo {
		Map,
		Screen,
		Mouse
	};

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
		return (int)(Graphics::GetDefaultFps() * ms / 1000.0f);
	}

	void Update() {
		if (!sprite) {
			return;
		}

		if (fixed_to == FixedTo::Map) {
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

		if (rotate_forever_degree) {
			current_angle += (rotate_cw ? 1 : -1) * rotate_forever_degree;
		}

		sprite->SetZ(100000 + z);
		sprite->SetX(current_x);
		sprite->SetY(current_y);
		sprite->SetOx((int)(sprite->GetWidth() / 2));
		sprite->SetOy((int)(sprite->GetHeight() / 2));
		sprite->SetAngle(current_angle);
		sprite->SetZoomX(current_zoom / 100.0);
		sprite->SetZoomY(current_zoom / 100.0);
		sprite->SetOpacity((int)(current_opacity));
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

	void SetFixedTo(FixedTo to) {
		fixed_to = to;

		if (fixed_to == FixedTo::Mouse) {
			Output::Warning("Sprite: Fixed to mouse not supported");
		} else if (fixed_to == FixedTo::Map) {
			if (!sprite) {
				return;
			}
			old_map_x = current_x * TILE_SIZE;
			old_map_y = current_y * TILE_SIZE;
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

	void SetZ(int z) {
		this->z = z;
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

private:
	void SetSpriteDefaults() {
		if (!sprite) {
			return;
		}

		current_x = 160.0;
		current_y = 120.0;
		z = 0;
		current_zoom = 100.0;

		old_map_x = Game_Map::GetDisplayX();
		old_map_y = Game_Map::GetDisplayY();
	}

	bool SetSpriteImage(const std::string& filename) {
		// Does not go through the Cache code
		// No fancy stuff like checkerboard on load error :(

		std::string file = FileFinder::FindDefault(filename);

		if (file.empty()) {
			Output::Warning("Sprite not found: %s", filename.c_str());
			return false;
		}
		sprite.reset(new Sprite());
		sprite->SetBitmap(Bitmap::Create(file));

		return true;
	}

	std::unique_ptr<Sprite> sprite;

	BlendMode blendmode = BlendMode::Mix;
	FixedTo fixed_to = FixedTo::Screen;

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
			graphic->SetZ(Priority_Frame + (1 << 20) + z);
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

	graphics[id]->SetFixedTo(coordsys == "mouse" ? RpgssSprite::FixedTo::Mouse :
		coordsys == "map" ? RpgssSprite::FixedTo::Map : RpgssSprite::FixedTo::Screen);

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

	graphics[id]->SetRelativeRotationEffect(angle, ms);

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
