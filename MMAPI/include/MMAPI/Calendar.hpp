// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Game.hpp"
#include "Hook.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include <optional>
#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Calendar
{
	inline constexpr int NightStartTimeInSeconds = 72000;

	/// Source: globalInstance.__day__
	enum class Weekdays : int
	{
		Monday    = 0,
		Tuesday   = 1,
		Wednesday = 2,
		Thursday  = 3,
		Friday    = 4,
		Saturday  = 5,
		Sunday    = 6
	};

	/// Total number of enumerators in Weekdays. Iterating [0, WeekdayCount) covers every Weekdays value.
	inline constexpr int WeekdayCount = 7;

	/// Invokes fn with every Weekdays value, in ascending order.
	template <typename Fn>
	inline void ForEachWeekday(Fn fn)
	{
		for (int i = 0; i < WeekdayCount; ++i)
			fn(static_cast<Weekdays>(i));
	}

	/// Source: globalInstance.__season__
	enum class Seasons : int
	{
		Spring = 0,
		Summer = 1,
		Fall   = 2,
		Winter = 3
	};

	/// Total number of enumerators in Seasons. Iterating [0, SeasonCount) covers every Seasons value.
	inline constexpr int SeasonCount = 4;

	/// Invokes fn with every Seasons value, in ascending order.
	template <typename Fn>
	inline void ForEachSeason(Fn fn)
	{
		for (int i = 0; i < SeasonCount; ++i)
			fn(static_cast<Seasons>(i));
	}

	struct ClockUpdateContext
	{
		int64_t m_old_time = 0;

		/// Returns the clock time (in seconds) captured before the game's update@Clock@Clock script ran.
		/// Pair with GetCurrentTimeInSeconds() inside an AfterClockUpdate callback to compute
		/// the per-tick delta (e.g. how much the clock advanced this frame).
		int64_t GetOldTime() const { return m_old_time; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_GET_UNIFIED_TIME = "gml_Script_unified_time@Calendar@Calendar";
		inline constexpr const char* GML_SCRIPT_GET_DAY          = "gml_Script_day@Calendar@Calendar";
		inline constexpr const char* GML_SCRIPT_GET_SEASON       = "gml_Script_season@Calendar@Calendar";
		inline constexpr const char* GML_SCRIPT_GET_YEAR         = "gml_Script_year@Calendar@Calendar";
		inline constexpr const char* GML_SCRIPT_UPDATE_CLOCK     = "gml_Script_update@Clock@Clock";

		// Live Calendar Self/Other, latched from the unified_time hook (fires every frame during gameplay).
		// All four Calendar scripts are bound to the same Calendar singleton, so a single latch serves all of them.
		// Used by TryGetCalendarContext for callers outside any hook frame.
		inline YYTK::CInstance* calendar_self  = nullptr;
		inline YYTK::CInstance* calendar_other = nullptr;

		// Cleared from the setup_main_screen pub/sub when the player returns to the title menu.
		// Registered by Calendar::Enable().
		inline void ClearCalendarOnReturnToTitle(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			calendar_self  = nullptr;
			calendar_other = nullptr;
		}

		inline YYTK::RValue& UnifiedTimeContextCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			// Latch on first observation and freeze. The Calendar is a singleton; later-tick
			// Self/Other can feed downstream script calls a context the script doesn't accept
			// (the same pattern that bit StatusEffect's register script).
			if (!calendar_self)
			{
				calendar_self  = Self;
				calendar_other = Other;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_GET_UNIFIED_TIME));
			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		using BeforeClockUpdateCallback = void(*)();
		using AfterClockUpdateCallback  = void(*)(MMAPI::Calendar::ClockUpdateContext&);

		inline BeforeClockUpdateCallback before_clock_update_callback = nullptr;
		inline AfterClockUpdateCallback  after_clock_update_callback  = nullptr;

		inline YYTK::RValue& GmlScriptClockUpdateCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_clock_update_callback)
				before_clock_update_callback();

			// Capture old time after Before runs (so Before-side modifications are reflected)
			// but before the original advances it. Read straight from globalInstance.__clock.time.
			int64_t old_time = MMAPI::Internal::global_instance
				->GetMember("__clock")
				.GetMember("time")
				.ToInt64();

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_UPDATE_CLOCK)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_clock_update_callback)
			{
				MMAPI::Calendar::ClockUpdateContext context{ old_time };
				after_clock_update_callback(context);
			}

			return Result;
		}

		/// Resolves the Calendar's GML calling context, latched from the most recent unified_time tick.
		/// Cleared automatically when the game returns to the title menu.
		/// @return True if a Calendar unified_time call has been observed this session, false otherwise.
		inline bool TryGetCalendarContext(YYTK::CInstance*& Self, YYTK::CInstance*& Other)
		{
			if (!calendar_self)
				return false;
			Self  = calendar_self;
			Other = calendar_other;
			return true;
		}

		inline YYTK::RValue CallCalendarScript(const char* script_name)
		{
			YYTK::CInstance* Self  = nullptr;
			YYTK::CInstance* Other = nullptr;
			if (!TryGetCalendarContext(Self, Other))
				return {};

			YYTK::CScript* gml_script = nullptr;
			MMAPI::Internal::module_interface->GetNamedRoutinePointer(script_name, reinterpret_cast<PVOID*>(&gml_script));

			YYTK::RValue result;
			gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
			return result;
		}

		inline YYTK::RValue GetDay()
		{
			return CallCalendarScript(GML_SCRIPT_GET_DAY);
		}

		inline YYTK::RValue GetSeason()
		{
			return CallCalendarScript(GML_SCRIPT_GET_SEASON);
		}

		inline YYTK::RValue GetYear()
		{
			return CallCalendarScript(GML_SCRIPT_GET_YEAR);
		}
	}

	/// Activates Calendar utility functions. Installs the unified_time hook so the live Calendar Self/Other are latched
	/// for TryGetCalendarContext (cleared on return-to-title via the setup_main_screen pub/sub). All Calendar scripts
	/// share the same singleton context, so a single latch serves day/season/year as well.
	/// Also installs the Clock.update hook used by BeforeClockUpdate / AfterClockUpdate; the callbacks no-op until bound.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Calendar::Enable() called");

		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ClearCalendarOnReturnToTitle);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ MMAPI::Internal::GML_SCRIPT_SETUP_MAIN_SCREEN, reinterpret_cast<PVOID>(MMAPI::Internal::GmlScriptBeforeSetupMainScreenCallback) },
			{ Internal::GML_SCRIPT_GET_UNIFIED_TIME,         reinterpret_cast<PVOID>(Internal::UnifiedTimeContextCallback) },
			{ Internal::GML_SCRIPT_UPDATE_CLOCK,             reinterpret_cast<PVOID>(Internal::GmlScriptClockUpdateCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Gets the current 1-indexed day of the month from the Calendar script context.
	/// @attention Requires MMAPI::Calendar::Enable() to have been called.
	/// @return The current day of the month from 1 to 28 as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue GetDay()
	{
		MMAPI_REQUIRE_ENABLED("Calendar", {});

		YYTK::RValue day = Internal::GetDay();
		if (day.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		return static_cast<int>(day.ToInt64()) + 1;
	}

	/// Gets the current weekday from the Calendar script context.
	/// @attention Requires MMAPI::Calendar::Enable() to have been called.
	/// @param weekday The current weekday.
	/// @return True if the weekday was resolved, false if the required context is unavailable.
	inline bool TryGetWeekday(MMAPI::Calendar::Weekdays& weekday)
	{
		YYTK::RValue day_of_month = GetDay();
		if (day_of_month.m_Kind == YYTK::VALUE_UNDEFINED || day_of_month.m_Kind == YYTK::VALUE_UNSET)
			return false;

		int weekday_id = (static_cast<int>(day_of_month.ToInt64()) - 1) % 7;
		if (weekday_id < static_cast<int>(MMAPI::Calendar::Weekdays::Monday) ||
		    weekday_id > static_cast<int>(MMAPI::Calendar::Weekdays::Sunday))
			return false;

		weekday = static_cast<MMAPI::Calendar::Weekdays>(weekday_id);
		return true;
	}

	/// Returns true if the current weekday matches weekday.
	/// @attention Requires MMAPI::Calendar::Enable() to have been called.
	/// @param weekday The weekday to compare against.
	inline bool IsWeekday(MMAPI::Calendar::Weekdays weekday)
	{
		MMAPI::Calendar::Weekdays current_weekday;
		if (!TryGetWeekday(current_weekday))
			return false;

		return current_weekday == weekday;
	}

	/// Resolves a Weekdays from its game-internal name string by consulting `globalInstance.__day__`.
	/// Useful for mods that take user-supplied day names from JSON config and need to round-trip
	/// back to the enum.
	/// @param internal_name The game-internal day name (lowercase, e.g. "monday"). Match is case-sensitive.
	/// @return The Weekdays enum value, or std::nullopt if no weekday matches.
	inline std::optional<MMAPI::Calendar::Weekdays> TryWeekdayFromInternalName(const std::string& internal_name)
	{
		if (!MMAPI::Internal::global_instance)
			return std::nullopt;

		YYTK::RValue days = MMAPI::Internal::global_instance->GetMember("__day__");
		if (days.m_Kind == YYTK::VALUE_UNDEFINED)
			return std::nullopt;

		size_t count = 0;
		MMAPI::Internal::module_interface->GetArraySize(days, count);
		for (size_t i = 0; i < count; ++i)
		{
			YYTK::RValue* entry = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(days, i, entry);
			if (entry && entry->m_Kind == YYTK::VALUE_STRING && entry->ToString() == internal_name)
				return static_cast<MMAPI::Calendar::Weekdays>(i);
		}
		return std::nullopt;
	}

	/// Gets the current season from the Calendar script context.
	/// @attention Requires MMAPI::Calendar::Enable() to have been called.
	/// @param season The current season.
	/// @return True if the season was resolved, false if the required context is unavailable.
	inline bool TryGetSeason(MMAPI::Calendar::Seasons& season)
	{
		MMAPI_REQUIRE_ENABLED("Calendar", false);

		YYTK::RValue current_season = Internal::GetSeason();
		if (current_season.m_Kind == YYTK::VALUE_UNDEFINED || current_season.m_Kind == YYTK::VALUE_UNSET)
			return false;

		int season_id = static_cast<int>(current_season.ToInt64());
		if (season_id < static_cast<int>(MMAPI::Calendar::Seasons::Spring) ||
		    season_id > static_cast<int>(MMAPI::Calendar::Seasons::Winter))
			return false;

		season = static_cast<MMAPI::Calendar::Seasons>(season_id);
		return true;
	}

	/// Returns true if the current season matches season.
	/// @attention Requires MMAPI::Calendar::Enable() to have been called.
	/// @param season The season to compare against.
	inline bool IsSeason(MMAPI::Calendar::Seasons season)
	{
		MMAPI::Calendar::Seasons current_season;
		if (!TryGetSeason(current_season))
			return false;

		return current_season == season;
	}

	/// Resolves a Seasons from its game-internal name string by consulting `globalInstance.__season__`.
	/// Useful for mods that take user-supplied season names from JSON config and need to round-trip
	/// back to the enum.
	/// @param internal_name The game-internal season name (lowercase, e.g. "spring", "summer", "fall", "winter").
	/// @return The Seasons enum value, or std::nullopt if no season matches.
	inline std::optional<MMAPI::Calendar::Seasons> TrySeasonFromInternalName(const std::string& internal_name)
	{
		if (!MMAPI::Internal::global_instance)
			return std::nullopt;

		YYTK::RValue seasons = MMAPI::Internal::global_instance->GetMember("__season__");
		if (seasons.m_Kind == YYTK::VALUE_UNDEFINED)
			return std::nullopt;

		size_t count = 0;
		MMAPI::Internal::module_interface->GetArraySize(seasons, count);
		for (size_t i = 0; i < count; ++i)
		{
			YYTK::RValue* entry = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(seasons, i, entry);
			if (entry && entry->m_Kind == YYTK::VALUE_STRING && entry->ToString() == internal_name)
				return static_cast<MMAPI::Calendar::Seasons>(i);
		}
		return std::nullopt;
	}

	/// Gets the current 1-indexed calendar year from the Calendar script context.
	/// @attention Requires MMAPI::Calendar::Enable() to have been called.
	/// @return The current calendar year as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue GetYear()
	{
		MMAPI_REQUIRE_ENABLED("Calendar", {});

		YYTK::RValue year = Internal::GetYear();
		if (year.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		return static_cast<int>(year.ToInt64()) + 1;
	}

	/// Returns the current in-game clock time in seconds, read directly from
	/// `globalInstance.__clock.time`. Use this for time-of-day checks, deadline comparisons,
	/// or to compute the per-tick delta inside an AfterClockUpdate callback (pair with
	/// `ClockUpdateContext::GetOldTime()`).
	///
	/// @note Safe to call without MMAPI::Calendar::Enable() — reads a global directly,
	///       does not depend on Calendar's hook latch.
	inline int GetCurrentTimeInSeconds()
	{
		return static_cast<int>(
			MMAPI::Internal::global_instance
				->GetMember("__clock")
				.GetMember("time")
				.ToInt64()
		);
	}

	/// Returns true if the current game clock time is night.
	/// Night starts at 8 PM, matching the game's daytime/nighttime split.
	inline bool IsNight()
	{
		return GetCurrentTimeInSeconds() >= NightStartTimeInSeconds;
	}

	/// Returns true if the current game clock time is day.
	/// Day is any time before 8 PM, matching the game's daytime/nighttime split.
	inline bool IsDay()
	{
		return !IsNight();
	}

	/// Returns true if the current game clock time is within `[start_seconds, end_seconds)`.
	/// The game's clock is monotonic within a day-cycle and does not wrap at midnight. To
	/// express an overnight range, use times past 86400 for `end_seconds`.
	///
	/// Examples:
	///   IsInTimeRange(28800, 64800)   // true between 08:00 and 18:00
	///   IsInTimeRange(79200, 93600)   // true between 22:00 and 02:00 the next morning
	///
	/// @param start_seconds Inclusive start of the range, in game-clock seconds.
	/// @param end_seconds Exclusive end of the range, in game-clock seconds.
	inline bool IsInTimeRange(int start_seconds, int end_seconds)
	{
		int current = GetCurrentTimeInSeconds();
		return current >= start_seconds && current < end_seconds;
	}

	/// Gets the current unified game time from the Calendar script context.
	/// @attention Requires MMAPI::Calendar::Enable() to have been called.
	/// @return The current unified time as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue GetUnifiedTime()
	{
		MMAPI_REQUIRE_ENABLED("Calendar", {});
		return Internal::CallCalendarScript(Internal::GML_SCRIPT_GET_UNIFIED_TIME);
	}

	namespace Hooks
	{
		/// Registers a callback that runs before the game's `update@Clock@Clock` script.
		/// The callback takes no arguments and is intended for per-tick state preparation that must complete
		/// before the game advances the clock. To inspect the pre-original time, read it via
		/// `GetCurrentTimeInSeconds()` (or directly via `globalInstance.__clock.time`).
		/// @param callback A parameterless function called before each clock update.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeClockUpdate(Internal::BeforeClockUpdateCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Calendar::Hooks::BeforeClockUpdate, MMAPI::Calendar);

			return MMAPI::Internal::RegisterHook(
				"Calendar::BeforeClockUpdate",
				Internal::before_clock_update_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `update@Clock@Clock` script.
		/// The context exposes `ctx.GetOldTime()` — the clock time (seconds) captured between any
		/// BeforeClockUpdate callback and the original script. Compare it to
		/// `GetCurrentTimeInSeconds()` to determine how much the clock advanced this tick.
		/// @param callback A function called with a `MMAPI::Calendar::ClockUpdateContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterClockUpdate(Internal::AfterClockUpdateCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Calendar::Hooks::AfterClockUpdate, MMAPI::Calendar);

			return MMAPI::Internal::RegisterHook(
				"Calendar::AfterClockUpdate",
				Internal::after_clock_update_callback,
				callback
			);
		}
	}
}
