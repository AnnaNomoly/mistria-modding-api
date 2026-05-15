// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"

#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Engine
{
	namespace Internal
	{
		// GameMaker direction-in-degrees constants (0 = right, 90 = up, 180 = left, 270 = down).
		inline constexpr double DIRECTION_DEGREES_RIGHT = 0.0;
		inline constexpr double DIRECTION_DEGREES_UP    = 90.0;
		inline constexpr double DIRECTION_DEGREES_LEFT  = 180.0;
		inline constexpr double DIRECTION_DEGREES_DOWN  = 270.0;
	}

	/// Return values of GameMaker's `asset_get_type` builtin. Values match the GameMaker LTS manual:
	/// https://manual.gamemaker.io/lts/en/GameMaker_Language/GML_Reference/Asset_Management/Assets_And_Tags/asset_get_type.htm
	/// GML-standardized, not game-specific. `Unknown = -1` is the sentinel the builtin returns when
	/// the looked-up name doesn't refer to a known asset.
	enum class AssetType : int
	{
		Unknown        = -1,
		Object         = 0,
		Sprite         = 1,
		Sound          = 2,
		Room           = 3,
		Tiles          = 4,
		Path           = 5,
		Script         = 6,
		Font           = 7,
		Timeline       = 8,
		Shader         = 9,
		AnimationCurve = 10,
		Sequence       = 11,
		ParticleSystem = 12,
	};

	/// Returns true if the RValue holds a numeric type (int32, int64, or real).
	/// @param value The RValue to test.
	/// @return True if the value is VALUE_INT32, VALUE_INT64, or VALUE_REAL.
	inline bool IsNumeric(YYTK::RValue value)
	{
		return value.m_Kind == YYTK::VALUE_INT32 || value.m_Kind == YYTK::VALUE_INT64 || value.m_Kind == YYTK::VALUE_REAL;
	}

	/// Returns true if the RValue holds a GML object or struct.
	/// @param value The RValue to test.
	/// @return True if the value is VALUE_OBJECT.
	inline bool IsObject(YYTK::RValue value)
	{
		return value.m_Kind == YYTK::VALUE_OBJECT;
	}

	/// Returns true if the named member exists on the given GML struct.
	/// @param the_struct The GML struct to query.
	/// @param variable_name The member name to check for.
	/// @return True if the member exists on the struct.
	inline bool StructVariableExists(YYTK::RValue the_struct, const char* variable_name)
	{
		return MMAPI::Internal::module_interface->CallBuiltin("struct_exists", { the_struct, variable_name }).ToBoolean();
	}

	/// Gets the value of a named member from a GML struct.
	/// @param the_struct The GML struct to read from.
	/// @param variable_name The member name to retrieve.
	/// @return The RValue of the member, or undefined if it does not exist.
	inline YYTK::RValue StructVariableGet(YYTK::RValue the_struct, const char* variable_name)
	{
		return MMAPI::Internal::module_interface->CallBuiltin("struct_get", { the_struct, variable_name });
	}

	/// Sets a named member on a GML struct to the given value. Creates the member if it does not exist.
	/// @param the_struct The GML struct to write to.
	/// @param variable_name The member name to set.
	/// @param value The value to assign.
	/// @return The RValue returned by the underlying GML builtin.
	inline YYTK::RValue StructVariableSet(YYTK::RValue the_struct, const char* variable_name, YYTK::RValue value)
	{
		return MMAPI::Internal::module_interface->CallBuiltin("struct_set", { the_struct, variable_name, value });
	}

	/// Sets a numeric member on a GML struct to the given value. Creates the member if it does not exist.
	/// @param the_struct The GML struct to write to.
	/// @param variable_name The numeric member key to set.
	/// @param value The value to assign.
	/// @return The RValue returned by the underlying GML builtin.
	inline YYTK::RValue StructVariableSet(YYTK::RValue the_struct, int variable_name, YYTK::RValue value)
	{
		return MMAPI::Internal::module_interface->CallBuiltin("struct_set", { the_struct, variable_name, value });
	}

	/// Removes a named member from a GML struct. Does nothing if the member does not exist.
	/// @param the_struct The GML struct to modify.
	/// @param variable_name The member name to remove.
	inline void StructVariableRemove(YYTK::RValue the_struct, const char* variable_name)
	{
		if (StructVariableExists(the_struct, variable_name))
			MMAPI::Internal::module_interface->CallBuiltin("struct_remove", { the_struct, variable_name });
	}

	/// Returns true if the named GML global variable has been declared.
	/// @param variable_name The global variable name to check.
	/// @return True if the global variable exists.
	inline bool GlobalVariableExists(const char* variable_name)
	{
		return MMAPI::Internal::module_interface->CallBuiltin("variable_global_exists", { variable_name }).ToBoolean();
	}

	/// Gets the value of a GML global variable.
	/// @param variable_name The global variable name to retrieve.
	/// @return The current value of the global variable.
	inline YYTK::RValue GlobalVariableGet(const char* variable_name)
	{
		return MMAPI::Internal::module_interface->CallBuiltin("variable_global_get", { variable_name });
	}

	/// Sets a GML global variable to the given value.
	/// @param variable_name The global variable name to set.
	/// @param value The value to assign.
	/// @return The RValue returned by the underlying GML builtin.
	inline YYTK::RValue GlobalVariableSet(const char* variable_name, YYTK::RValue value)
	{
		return MMAPI::Internal::module_interface->CallBuiltin("variable_global_set", { variable_name, value });
	}

	/// Returns the current width of the game window in pixels.
	/// @return The window width in pixels.
	inline double GetWindowWidth()
	{
		return MMAPI::Internal::module_interface->CallBuiltin("window_get_width", {}).ToDouble();
	}

	/// Returns the current height of the game window in pixels.
	/// @return The window height in pixels.
	inline double GetWindowHeight()
	{
		return MMAPI::Internal::module_interface->CallBuiltin("window_get_height", {}).ToDouble();
	}

	/// Returns true if the game window currently has focus.
	inline bool WindowHasFocus()
	{
		return MMAPI::Internal::module_interface->CallBuiltin("window_has_focus", {}).ToBoolean();
	}

	/// Returns the mouse's current X position in window pixels (0 = left edge).
	inline double GetMouseX()
	{
		return MMAPI::Internal::module_interface->CallBuiltin("window_mouse_get_x", {}).ToDouble();
	}

	/// Returns the mouse's current Y position in window pixels (0 = top edge).
	inline double GetMouseY()
	{
		return MMAPI::Internal::module_interface->CallBuiltin("window_mouse_get_y", {}).ToDouble();
	}

	/// Gets a GML asset index by asset name.
	/// @param asset_name The GML asset name to look up.
	/// @return The asset index as an RValue.
	inline YYTK::RValue AssetGetIndex(const std::string& asset_name)
	{
		return MMAPI::Internal::module_interface->CallBuiltin("asset_get_index", { asset_name.c_str() });
	}

	/// Returns true if the given sound effect asset index is currently playing.
	/// @param sound_effect_index The sound effect asset index to check.
	/// @return True if the sound effect is currently playing.
	inline bool AudioIsPlaying(YYTK::RValue sound_effect_index)
	{
		return MMAPI::Internal::module_interface->CallBuiltin("audio_is_playing", { sound_effect_index }).ToBoolean();
	}

	/// Plays a game sound effect by asset name.
	/// @param sound_name The GML asset name of the sound (e.g. "snd_sword_swing").
	/// @param priority Playback priority. Higher values take precedence when the audio channel limit is reached.
	/// @param gain Volume multiplier (0.0 = silent, 1.0 = full volume).
	inline void PlaySoundEffect(const char* sound_name, int priority, double gain)
	{
		YYTK::RValue sound_index = AssetGetIndex(sound_name);
		MMAPI::Internal::module_interface->CallBuiltin("audio_play_sound", { sound_index, priority, false, gain });
	}

	/// Stops a currently playing game sound effect by asset name.
	/// @param sound_name The GML asset name of the sound (e.g. "snd_sword_swing").
	inline void StopSoundEffect(const char* sound_name)
	{
		YYTK::RValue sound_index = AssetGetIndex(sound_name);
		MMAPI::Internal::module_interface->CallBuiltin("audio_stop_sound", { sound_index });
	}

	/// Stops a currently playing game sound effect by its asset index. Use this overload when you
	/// already have the index cached (e.g. resolved once at startup) to skip the per-call name lookup.
	/// @param sound_effect_index The sound effect asset index.
	inline void StopSoundEffect(YYTK::RValue sound_effect_index)
	{
		MMAPI::Internal::module_interface->CallBuiltin("audio_stop_sound", { sound_effect_index });
	}

	/// Sets the active GameMaker draw color. Subsequent draw calls (rectangles, lines, text) use
	/// this color until it changes again. Color is a GM-format integer where the byte order is
	/// 0xBBGGRR (e.g. 0x00FF00 = green, 0x0000FF = red).
	/// @param color The GM-format color integer.
	inline void DrawSetColor(int color)
	{
		MMAPI::Internal::module_interface->CallBuiltin("draw_set_color", { color });
	}

	/// Draws a filled or outlined rectangle in the current draw target. Sets the draw color first,
	/// then issues the rectangle — pair-and-call is the typical usage pattern.
	/// @param color The GM-format color integer (see `DrawSetColor`).
	/// @param x1 Left edge in pixels.
	/// @param y1 Top edge in pixels.
	/// @param x2 Right edge in pixels.
	/// @param y2 Bottom edge in pixels.
	/// @param outline When true, draws only the rectangle border; otherwise draws a filled rectangle.
	inline void DrawRectangle(int color, double x1, double y1, double x2, double y2, bool outline)
	{
		DrawSetColor(color);
		MMAPI::Internal::module_interface->CallBuiltin("draw_rectangle", { x1, y1, x2, y2, outline });
	}

	/// Draws a sprite from its asset index. For the common "play the default frame" case, pass
	/// `subimg = -1` so the game advances animation normally.
	/// @param sprite_asset The sprite asset index (typically from `AssetGetIndex("spr_...")`).
	/// @param subimg The sprite sub-image to draw, or -1 to use the sprite's current animation frame.
	/// @param x X position in pixels.
	/// @param y Y position in pixels.
	inline void DrawSprite(YYTK::RValue sprite_asset, int subimg, double x, double y)
	{
		MMAPI::Internal::module_interface->CallBuiltin("draw_sprite", { sprite_asset, subimg, x, y });
	}

	/// Gets the value of a GML built-in instance variable by name.
	/// @param instance The instance to read from.
	/// @param variable_name The built-in variable name (e.g. "sprite_index", "x", "depth").
	/// @return The variable value as an RValue.
	inline YYTK::RValue InstanceVariableGet(YYTK::CInstance* instance, const char* variable_name)
	{
		return MMAPI::Internal::module_interface->CallBuiltin("variable_instance_get", { instance, variable_name });
	}

	/// Sets the value of a GML built-in instance variable by name.
	/// @param instance The instance to modify.
	/// @param variable_name The built-in variable name (e.g. "sprite_index", "image_speed", "depth").
	/// @param value The value to assign.
	inline void InstanceVariableSet(YYTK::CInstance* instance, const char* variable_name, YYTK::RValue value)
	{
		MMAPI::Internal::module_interface->CallBuiltin("variable_instance_set", { instance, variable_name, value });
	}

	/// Returns true if a GML layer with the given name exists in the current room.
	/// @param layer_name The GML layer name (e.g. "Impl_Ritual").
	inline bool LayerExists(const std::string& layer_name)
	{
		return MMAPI::Internal::module_interface->CallBuiltin("layer_exists", { layer_name.c_str() }).ToBoolean();
	}

	/// Creates an instance of the given GML object on the given layer at (x, y).
	/// @param x The X coordinate to spawn the instance at.
	/// @param y The Y coordinate to spawn the instance at.
	/// @param layer_name The GML layer name to spawn on (e.g. "Impl_Ritual").
	/// @param object_name The GML object asset name (e.g. "obj_dungeon_ritual_altar").
	/// @return The newly-created instance as an RValue.
	inline YYTK::RValue InstanceCreateLayer(double x, double y, const std::string& layer_name, const std::string& object_name)
	{
		YYTK::RValue object_index = AssetGetIndex(object_name);
		return MMAPI::Internal::module_interface->CallBuiltin(
			"instance_create_layer",
			{ x, y, YYTK::RValue(layer_name), object_index }
		);
	}
}
