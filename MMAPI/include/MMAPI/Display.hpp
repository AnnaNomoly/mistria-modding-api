// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Engine.hpp"
#include "Hook.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Display
{
	struct DisplayResizeContext
	{
		double m_window_width  = 0.0;
		double m_window_height = 0.0;

		/// Returns the window width in pixels at the moment the resize completed.
		double GetWindowWidth() const { return m_window_width; }

		/// Returns the window height in pixels at the moment the resize completed.
		double GetWindowHeight() const { return m_window_height; }
	};

	struct VertigoDrawWithColorContext
	{
		std::string  m_sprite_name;
		YYTK::RValue m_sprite_asset;

		/// Returns the sprite name being drawn, extracted from `Arguments[0]` via `sprite_get_name`.
		/// Empty if `Arguments[0]` is not a sprite asset (e.g. it's a font, surface, or non-asset value).
		std::string_view GetSpriteName() const { return m_sprite_name; }

		/// Overrides the sprite asset the game will draw in screen space.
		/// Typically populated with `MMAPI::Engine::AssetGetIndex("spr_...")`.
		void SetSpriteAsset(YYTK::RValue sprite_asset) { m_sprite_asset = sprite_asset; }
	};

	struct PlayHealVfxContext
	{
		bool m_cancelled = false;

		/// Prevents the game's play_heal_vfx script from running this fire.
		void Cancel() { m_cancelled = true; }
	};

	struct HudShouldShowContext
	{
		bool m_result = true;

		/// Returns the hud-visibility result the game computed. True if the HUD should render.
		bool GetResult() const { return m_result; }

		/// Overrides whether the HUD renders this fire. Call SetResult(false) to hide the HUD.
		void SetResult(bool result) { m_result = result; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_DISPLAY_RESIZE           = "gml_Script_resize_amount@Display@Display";
		inline constexpr const char* GML_SCRIPT_VERTIGO_DRAW_WITH_COLOR  = "gml_Script_vertigo_draw_with_color";
		inline constexpr const char* GML_SCRIPT_PLAY_HEAL_VFX            = "gml_Script_play_heal_vfx";
		inline constexpr const char* GML_SCRIPT_ON_DRAW_GUI              = "gml_Script_on_draw_gui@Display@Display";
		inline constexpr const char* GML_SCRIPT_HUD_SHOULD_SHOW          = "gml_Script_hud_should_show";

		using AfterDisplayResizeCallback         = void(*)(MMAPI::Display::DisplayResizeContext&);
		using BeforeVertigoDrawWithColorCallback = void(*)(MMAPI::Display::VertigoDrawWithColorContext&);
		using BeforePlayHealVfxCallback          = void(*)(MMAPI::Display::PlayHealVfxContext&);
		using AfterDrawGuiCallback               = void(*)();
		using AfterHudShouldShowCallback         = void(*)(MMAPI::Display::HudShouldShowContext&);

		inline AfterDisplayResizeCallback         after_display_resize_callback           = nullptr;
		inline BeforeVertigoDrawWithColorCallback before_vertigo_draw_with_color_callback = nullptr;
		inline BeforePlayHealVfxCallback          before_play_heal_vfx_callback           = nullptr;
		inline AfterDrawGuiCallback               after_draw_gui_callback                 = nullptr;
		inline AfterHudShouldShowCallback         after_hud_should_show_callback          = nullptr;

		inline YYTK::RValue& GmlScriptAfterDisplayResizeCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_DISPLAY_RESIZE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_display_resize_callback)
			{
				MMAPI::Display::DisplayResizeContext context{
					MMAPI::Engine::GetWindowWidth(),
					MMAPI::Engine::GetWindowHeight()
				};
				after_display_resize_callback(context);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptBeforeVertigoDrawWithColorCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_vertigo_draw_with_color_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				// arg0 may be any asset (sprite/font/etc.). Resolve a sprite name only when it's a sprite.
				std::string sprite_name;
				YYTK::RValue asset_type = MMAPI::Internal::module_interface->CallBuiltin(
					"asset_get_type", { *Arguments[0] }
				);
				if (asset_type.ToInt64() == static_cast<int64_t>(MMAPI::Engine::AssetType::Sprite))
				{
					YYTK::RValue name = MMAPI::Internal::module_interface->CallBuiltin(
						"sprite_get_name", { *Arguments[0] }
					);
					if (name.m_Kind == YYTK::VALUE_STRING)
						sprite_name = name.ToString();
				}

				MMAPI::Display::VertigoDrawWithColorContext context{ std::move(sprite_name), *Arguments[0] };
				before_vertigo_draw_with_color_callback(context);
				*Arguments[0] = context.m_sprite_asset;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_VERTIGO_DRAW_WITH_COLOR)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

		inline YYTK::RValue& GmlScriptBeforePlayHealVfxCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_play_heal_vfx_callback)
			{
				MMAPI::Display::PlayHealVfxContext context;
				before_play_heal_vfx_callback(context);

				if (context.m_cancelled)
					return Result;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_PLAY_HEAL_VFX)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterDrawGuiCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_ON_DRAW_GUI)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_draw_gui_callback)
				after_draw_gui_callback();

			return Result;
		}

		inline YYTK::RValue& GmlScriptHudShouldShowCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_HUD_SHOULD_SHOW)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_hud_should_show_callback)
			{
				MMAPI::Display::HudShouldShowContext context{ Result.ToBoolean() };
				after_hud_should_show_callback(context);
				Result = context.m_result;
			}

			return Result;
		}
	}

	/// Activates Display hooks. Installs the `resize_amount@Display@Display`,
	/// `vertigo_draw_with_color`, `play_heal_vfx`, `on_draw_gui@Display@Display`, and
	/// `hud_should_show` script hooks so registered callbacks fire on display resize, screen-space
	/// sprite draws, heal visual effects, per-frame GUI draw, and HUD visibility checks
	/// respectively. Safe to call before any Hooks::* registration — each callback no-ops until a
	/// user callback is bound.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Display::Enable() called");

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_DISPLAY_RESIZE,          reinterpret_cast<PVOID>(Internal::GmlScriptAfterDisplayResizeCallback) },
			{ Internal::GML_SCRIPT_VERTIGO_DRAW_WITH_COLOR, reinterpret_cast<PVOID>(Internal::GmlScriptBeforeVertigoDrawWithColorCallback) },
			{ Internal::GML_SCRIPT_PLAY_HEAL_VFX,           reinterpret_cast<PVOID>(Internal::GmlScriptBeforePlayHealVfxCallback) },
			{ Internal::GML_SCRIPT_ON_DRAW_GUI,             reinterpret_cast<PVOID>(Internal::GmlScriptAfterDrawGuiCallback) },
			{ Internal::GML_SCRIPT_HUD_SHOULD_SHOW,         reinterpret_cast<PVOID>(Internal::GmlScriptHudShouldShowCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	namespace Hooks
	{
		/// Registers a callback that runs after the game's `resize_amount@Display@Display` script.
		/// Use `ctx.GetWindowWidth()` / `ctx.GetWindowHeight()` to read the post-resize dimensions.
		/// @param callback A function called with a `MMAPI::Display::DisplayResizeContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterDisplayResize(Internal::AfterDisplayResizeCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Display::Hooks::AfterDisplayResize, MMAPI::Display);

			return MMAPI::Internal::RegisterHook(
				"Display::AfterDisplayResize",
				Internal::after_display_resize_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game's `vertigo_draw_with_color` script —
		/// the screen-space sprite draw routine used for HUD elements and other UI overlays.
		/// Use `ctx.GetSpriteName()` to identify the sprite being drawn and `ctx.SetSpriteAsset(...)`
		/// to override it (typically `MMAPI::Engine::AssetGetIndex("spr_...")`).
		/// For world-space sprite overrides (items dropped in the game world, etc.), use
		/// [`Item::Hooks::AfterGetUiIcon`](API-MMAPI-Item-Hooks-AfterGetUiIcon.md) instead.
		/// @param callback A function called with a mutable `MMAPI::Display::VertigoDrawWithColorContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeVertigoDrawWithColor(Internal::BeforeVertigoDrawWithColorCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Display::Hooks::BeforeVertigoDrawWithColor, MMAPI::Display);

			return MMAPI::Internal::RegisterHook(
				"Display::BeforeVertigoDrawWithColor",
				Internal::before_vertigo_draw_with_color_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game's `play_heal_vfx` script. Call `ctx.Cancel()`
		/// to short-circuit the heal visual effect this fire (e.g. when a mod-controlled time-stop is
		/// active and the vfx shouldn't play).
		/// @param callback A function called with a mutable `MMAPI::Display::PlayHealVfxContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforePlayHealVfx(Internal::BeforePlayHealVfxCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Display::Hooks::BeforePlayHealVfx, MMAPI::Display);

			return MMAPI::Internal::RegisterHook(
				"Display::BeforePlayHealVfx",
				Internal::before_play_heal_vfx_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `on_draw_gui@Display@Display` script —
		/// the per-frame GUI draw pass. Use for input polling, overlay drawing, or any other
		/// per-frame mod logic gated on the game being actively drawing.
		/// @param callback A parameterless function called after each GUI draw step.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterDrawGui(Internal::AfterDrawGuiCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Display::Hooks::AfterDrawGui, MMAPI::Display);

			return MMAPI::Internal::RegisterHook(
				"Display::AfterDrawGui",
				Internal::after_draw_gui_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `hud_should_show` script — the
		/// per-frame predicate that decides whether the HUD renders. Use `ctx.SetResult(false)`
		/// to hide the HUD, or `ctx.GetResult()` to inspect the game's verdict.
		/// @param callback A function called with a mutable `MMAPI::Display::HudShouldShowContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterHudShouldShow(Internal::AfterHudShouldShowCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Display::Hooks::AfterHudShouldShow, MMAPI::Display);

			return MMAPI::Internal::RegisterHook(
				"Display::AfterHudShouldShow",
				Internal::after_hud_should_show_callback,
				callback
			);
		}
	}
}
