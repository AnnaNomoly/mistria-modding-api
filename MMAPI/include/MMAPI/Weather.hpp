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

#include <optional>
#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Weather
{
	/// Context passed to AfterRoomStart callbacks. Exposes the WeatherManager Self/Other so the
	/// callback can invoke follow-up scripts that prefer a valid GML calling context.
	struct AfterRoomStartContext
	{
		YYTK::CInstance* m_self  = nullptr;
		YYTK::CInstance* m_other = nullptr;

		/// The WeatherManager instance (the script's Self at hook time).
		YYTK::CInstance* GetSelf() const { return m_self; }

		/// The script's Other at hook time.
		YYTK::CInstance* GetOther() const { return m_other; }
	};

	/// Forward-declared; full definition follows the Ids enum.
	enum class Ids : int;

	struct AfterGetWeatherContext
	{
		int m_weather_id = -1;

		/// Returns the weather kind the game's get_weather script just resolved. Unknown if the
		/// raw result was out of the documented range (e.g. a value introduced by a future patch).
		MMAPI::Weather::Ids GetWeather() const;

		/// Returns the raw weather id as the script produced it. Useful when a new weather id
		/// shows up that isn't in `Weather::Ids` yet.
		int GetRawId() const { return m_weather_id; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_GET_WEATHER    = "gml_Script_get_weather@WeatherManager@Weather";
		inline constexpr const char* GML_SCRIPT_ON_ROOM_START  = "gml_Script_on_room_start@WeatherManager@Weather";

		using BeforeGetWeatherCallback = void(*)();
		using AfterGetWeatherCallback  = void(*)(MMAPI::Weather::AfterGetWeatherContext&);
		using AfterRoomStartCallback   = void(*)(MMAPI::Weather::AfterRoomStartContext&);
		inline BeforeGetWeatherCallback before_get_weather_callback = nullptr;
		inline AfterGetWeatherCallback  after_get_weather_callback  = nullptr;
		inline AfterRoomStartCallback   after_room_start_callback   = nullptr;

		// Live WeatherManager Self/Other, latched from the get_weather hook.
		// Used by TryGetWeatherManagerContext for callers outside any hook frame.
		inline YYTK::CInstance* weather_manager_self  = nullptr;
		inline YYTK::CInstance* weather_manager_other = nullptr;

		// Slot for the Game::Hooks::AfterGameActive user-facing hook. Slot lives here because the
		// signal we use to detect "game is active" is Weather's get_weather first-fire-per-session,
		// but the public registrar lives on Game (where lifecycle hooks logically belong).
		using GameActiveCallback = void(*)();
		inline GameActiveCallback game_active_callback = nullptr;
		inline bool game_active_observed_this_session = false;

		// Cleared from the setup_main_screen pub/sub when the player returns to the title menu.
		// Registered by Weather::Enable().
		inline void ClearWeatherManagerOnReturnToTitle(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			weather_manager_self  = nullptr;
			weather_manager_other = nullptr;
			game_active_observed_this_session = false;
		}

		/// Resolves the WeatherManager's GML calling context, latched from the most recent get_weather call.
		/// Cleared automatically when the game returns to the title menu.
		/// @return True if a get_weather call has been observed this session, false otherwise.
		inline bool TryGetWeatherManagerContext(YYTK::CInstance*& Self, YYTK::CInstance*& Other)
		{
			if (!weather_manager_self)
				return false;
			Self  = weather_manager_self;
			Other = weather_manager_other;
			return true;
		}

		inline YYTK::RValue& GmlScriptAfterRoomStartCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_ON_ROOM_START)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_room_start_callback)
			{
				MMAPI::Weather::AfterRoomStartContext context{ Self, Other };
				after_room_start_callback(context);
			}

			return Result;
		}

		inline YYTK::RValue& GetWeatherContextCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			// Latch on first observation only. The WeatherManager is a singleton; later-tick
			// Self/Other can feed downstream script calls a context the script doesn't accept —
			// see StatusEffect's manager-update comment.
			if (!weather_manager_self)
			{
				weather_manager_self  = Self;
				weather_manager_other = Other;
			}

			// First get_weather fire per session signals "game is interactive". Drives the
			// user-facing Game::Hooks::AfterGameActive hook AND the internal pub/sub registered
			// via MMAPI::Internal::RegisterOnGameActiveHandler (used by Instance::Hooks::OnObjectCall
			// to gate user dispatch on game-active state). Fires exactly once between title
			// transitions — game_active_observed_this_session is cleared on return-to-title.
			if (!game_active_observed_this_session)
			{
				game_active_observed_this_session = true;
				for (auto handler : MMAPI::Internal::on_game_active_internal_handlers)
					handler();
				if (game_active_callback)
					game_active_callback();
			}

			if (before_get_weather_callback)
				before_get_weather_callback();

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_GET_WEATHER));
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_get_weather_callback && MMAPI::Engine::IsNumeric(Result))
			{
				MMAPI::Weather::AfterGetWeatherContext context{ static_cast<int>(Result.ToInt64()) };
				after_get_weather_callback(context);
			}

			return Result;
		}
	}

	/// Activates Weather utility functions. Installs the get_weather hook so the live WeatherManager Self/Other are latched
	/// for TryGetWeatherManagerContext (cleared on return-to-title via the setup_main_screen pub/sub).
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Weather::Enable() called");

		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ClearWeatherManagerOnReturnToTitle);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ MMAPI::Internal::GML_SCRIPT_SETUP_MAIN_SCREEN, reinterpret_cast<PVOID>(MMAPI::Internal::GmlScriptBeforeSetupMainScreenCallback) },
			{ Internal::GML_SCRIPT_GET_WEATHER,              reinterpret_cast<PVOID>(Internal::GetWeatherContextCallback) },
			{ Internal::GML_SCRIPT_ON_ROOM_START,            reinterpret_cast<PVOID>(Internal::GmlScriptAfterRoomStartCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Source: globalInstance.__weather__
	enum class Ids : int
	{
		Calm            = 0,
		Inclement       = 1,
		HeavyInclement  = 2,
		Special         = 3
	};

	/// Total number of enumerators in Ids. Iterating [0, IdCount) covers every Ids value.
	inline constexpr int IdCount = 4;

	inline MMAPI::Weather::Ids AfterGetWeatherContext::GetWeather() const
	{
		if (m_weather_id < static_cast<int>(MMAPI::Weather::Ids::Calm)
		    || m_weather_id > static_cast<int>(MMAPI::Weather::Ids::Special))
			return MMAPI::Weather::Ids::Calm;  // safest default for out-of-range values
		return static_cast<MMAPI::Weather::Ids>(m_weather_id);
	}

	/// Invokes fn with every Ids value, in ascending order.
	template <typename Fn>
	inline void ForEachId(Fn fn)
	{
		for (int i = 0; i < IdCount; ++i)
			fn(static_cast<Ids>(i));
	}

	/// Gets the current weather from the game's WeatherManager.
	/// @attention Requires MMAPI::Weather::Enable() to have been called.
	/// @param weather Receives the current weather when the call succeeds.
	/// @return True if the context and script were available; otherwise false.
	inline bool TryGetWeather(MMAPI::Weather::Ids& weather)
	{
		MMAPI_REQUIRE_ENABLED("Weather", false);

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetWeatherManagerContext(Self, Other))
			return false;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_GET_WEATHER, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		int weather_id = static_cast<int>(result.ToInt64());
		if (weather_id < static_cast<int>(MMAPI::Weather::Ids::Calm) ||
		    weather_id > static_cast<int>(MMAPI::Weather::Ids::Special))
			return false;

		weather = static_cast<MMAPI::Weather::Ids>(weather_id);
		return true;
	}

	/// Returns true if the current weather matches weather.
	/// @attention Requires MMAPI::Weather::Enable() to have been called.
	/// @param weather The weather type to compare against.
	inline bool IsWeather(MMAPI::Weather::Ids weather)
	{
		MMAPI::Weather::Ids current_weather;
		if (!TryGetWeather(current_weather))
			return false;

		return current_weather == weather;
	}

	/// Returns true if the current weather is not inclement or heavy inclement.
	/// @attention Requires MMAPI::Weather::Enable() to have been called.
	inline bool IsSunny()
	{
		MMAPI::Weather::Ids weather;
		if (!TryGetWeather(weather))
			return false;

		return weather != MMAPI::Weather::Ids::HeavyInclement &&
		       weather != MMAPI::Weather::Ids::Inclement;
	}

	/// Resolves a weather Ids from its game-internal name string by consulting `globalInstance.__weather__`.
	/// Useful for mods that take user-supplied weather names from JSON config and need to round-trip
	/// back to the enum.
	/// @param internal_name The game-internal weather name (lowercase, e.g. "calm", "rainy"). Match is case-sensitive.
	/// @return The Ids enum value, or std::nullopt if no weather matches.
	inline std::optional<MMAPI::Weather::Ids> TryFromInternalName(const std::string& internal_name)
	{
		if (!MMAPI::Internal::global_instance)
			return std::nullopt;

		YYTK::RValue weather = MMAPI::Internal::global_instance->GetMember("__weather__");
		if (weather.m_Kind == YYTK::VALUE_UNDEFINED)
			return std::nullopt;

		size_t count = 0;
		MMAPI::Internal::module_interface->GetArraySize(weather, count);
		for (size_t i = 0; i < count; ++i)
		{
			YYTK::RValue* entry = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(weather, i, entry);
			if (entry && entry->m_Kind == YYTK::VALUE_STRING && entry->ToString() == internal_name)
				return static_cast<MMAPI::Weather::Ids>(i);
		}
		return std::nullopt;
	}

	namespace Hooks
	{
		/// Registers a callback that runs before the game's WeatherManager get_weather script.
		/// Useful as a "game is active" signal: get_weather runs continuously during gameplay but never on the title screen,
		/// so the first invocation after returning from the title screen reliably indicates the save has loaded.
		/// @param callback A function called before the original get_weather script runs.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeGetWeather(Internal::BeforeGetWeatherCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Weather::Hooks::BeforeGetWeather, MMAPI::Weather);

			return MMAPI::Internal::RegisterHook(
				"Weather::BeforeGetWeather",
				Internal::before_get_weather_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's WeatherManager get_weather script.
		/// Read `ctx.GetWeather()` to see what kind of weather just resolved. Useful for tracking
		/// the current weather state without re-calling the script (which would recurse if done
		/// from inside `BeforeGetWeather`).
		/// @param callback A function called with a `MMAPI::Weather::AfterGetWeatherContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterGetWeather(Internal::AfterGetWeatherCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Weather::Hooks::AfterGetWeather, MMAPI::Weather);

			return MMAPI::Internal::RegisterHook(
				"Weather::AfterGetWeather",
				Internal::after_get_weather_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `on_room_start@WeatherManager@Weather` script.
		/// Fires on every room load (any room, not just dungeon rooms) — use this for cross-cutting
		/// room-entry work that must happen after the WeatherManager has handled the transition.
		/// Read `ctx.GetSelf()` / `ctx.GetOther()` to invoke follow-up scripts that need the
		/// WeatherManager calling context.
		/// @param callback A function called with a `MMAPI::Weather::AfterRoomStartContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterRoomStart(Internal::AfterRoomStartCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Weather::Hooks::AfterRoomStart, MMAPI::Weather);

			return MMAPI::Internal::RegisterHook(
				"Weather::AfterRoomStart",
				Internal::after_room_start_callback,
				callback
			);
		}
	}
}
