// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Engine.hpp"
#include "Hook.hpp"
#include "Log.hpp"
#include "NPC.hpp"
#include "Status.hpp"
#include "T2.hpp"

#include <string>
#include <vector>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Schedule
{
	struct BeforeScheduleStartContext
	{
		std::string m_schedule_name;

		std::string_view GetScheduleName() const { return m_schedule_name; }
	};

	struct AfterScheduleEndContext
	{
		std::string m_schedule_name;

		std::string_view GetScheduleName() const { return m_schedule_name; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_SCHEDULE_NAME                       = "gml_Script_schedule_name@T2r@T2r";
		inline constexpr const char* GML_SCRIPT_SCHEDULE_CURRENT_DESTINATION        = "gml_Script_schedule_current_destination@T2r@T2r";
		inline constexpr const char* GML_SCRIPT_SCHEDULE_CURRENT_ACTION             = "gml_Script_schedule_current_action@T2r@T2r";
		inline constexpr const char* GML_SCRIPT_SCHEDULE_CURRENT_ACTION_HAS_ARRIVED = "gml_Script_schedule_current_action_has_arrived@T2r@T2r";
		inline constexpr const char* GML_SCRIPT_SCHEDULE_NAMES                      = "gml_Script_schedule_names@T2r@T2r";
		inline constexpr const char* GML_SCRIPT_SCHEDULE_START                      = "gml_Script_schedule_start@T2r@T2r";
		inline constexpr const char* GML_SCRIPT_SCHEDULE_END                        = "gml_Script_schedule_end@T2r@T2r";

		using BeforeScheduleStartCallback = void(*)(MMAPI::Schedule::BeforeScheduleStartContext&);
		using AfterScheduleEndCallback    = void(*)(MMAPI::Schedule::AfterScheduleEndContext&);

		inline BeforeScheduleStartCallback before_schedule_start_callback = nullptr;
		inline AfterScheduleEndCallback    after_schedule_end_callback    = nullptr;

		/// Reads the first argument as a string (the schedule name) if present. Empty otherwise.
		inline std::string ExtractScheduleName(int ArgumentCount, YYTK::RValue** Arguments)
		{
			if (Arguments && ArgumentCount >= 1 && Arguments[0] && Arguments[0]->m_Kind == YYTK::VALUE_STRING)
				return Arguments[0]->ToString();
			return {};
		}

		inline YYTK::RValue& GmlScriptScheduleStartCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_schedule_start_callback)
			{
				MMAPI::Schedule::BeforeScheduleStartContext ctx{ ExtractScheduleName(ArgumentCount, Arguments) };
				before_schedule_start_callback(ctx);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_SCHEDULE_START)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		inline YYTK::RValue& GmlScriptScheduleEndCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			std::string schedule_name = ExtractScheduleName(ArgumentCount, Arguments);

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_SCHEDULE_END)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_schedule_end_callback)
			{
				MMAPI::Schedule::AfterScheduleEndContext ctx{ schedule_name };
				after_schedule_end_callback(ctx);
			}
			return Result;
		}

		/// Invokes a `schedule_*@T2r@T2r` script that takes a single NPC-id argument and returns
		/// its result. Returns undefined if the T2 context isn't latched or the script can't be
		/// resolved.
		inline YYTK::RValue CallScheduleScriptWithNpc(const char* script_name, MMAPI::NPC::Ids npc)
		{
			YYTK::CInstance* Self  = nullptr;
			YYTK::CInstance* Other = nullptr;
			if (!MMAPI::T2::Internal::TryGetT2Context(Self, Other))
				return {};

			YYTK::CScript* gml_script = nullptr;
			MMAPI::Internal::module_interface->GetNamedRoutinePointer(script_name, reinterpret_cast<PVOID*>(&gml_script));
			if (!gml_script)
				return {};

			YYTK::RValue input = static_cast<int>(npc);
			YYTK::RValue* args[1] = { &input };
			YYTK::RValue result;
			gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
			return result;
		}
	}

	/// Activates Schedule utility functions and installs hooks on `schedule_start` and
	/// `schedule_end` so Before/After callbacks fire. Depends on `MMAPI::T2::Enable()` for the
	/// latched calling context that all schedule_* queries use.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Schedule::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Schedule, MMAPI::T2);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_SCHEDULE_START, reinterpret_cast<PVOID>(Internal::GmlScriptScheduleStartCallback) },
			{ Internal::GML_SCRIPT_SCHEDULE_END,   reinterpret_cast<PVOID>(Internal::GmlScriptScheduleEndCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Returns the name of the NPC's currently-running schedule, e.g. "adeline_default" or
	/// "summer_festival_eve". Empty if the NPC isn't running a recognised schedule or the T2
	/// context isn't yet latched.
	///
	/// Wraps `schedule_name@T2r@T2r`. Argument signature inferred as `(npc_id)`; if testing
	/// reveals a different shape, update accordingly.
	inline std::string GetCurrentSchedule(MMAPI::NPC::Ids npc)
	{
		YYTK::RValue result = Internal::CallScheduleScriptWithNpc(Internal::GML_SCRIPT_SCHEDULE_NAME, npc);
		if (result.m_Kind == YYTK::VALUE_STRING)
			return result.ToString();
		return {};
	}

	/// Returns the destination of the NPC's current scheduled action — typically a location
	/// struct or string identifying where the NPC is heading. Schedule's notion of "destination"
	/// differs from the per-tick `activity_handler.next_location` (which is the granular routing
	/// target); this is the higher-level "I'm going to the manor" intent.
	///
	/// Wraps `schedule_current_destination@T2r@T2r`. Returns the raw RValue so callers can
	/// inspect whatever shape the game uses (likely a struct with location info).
	inline YYTK::RValue GetCurrentDestination(MMAPI::NPC::Ids npc)
	{
		return Internal::CallScheduleScriptWithNpc(Internal::GML_SCRIPT_SCHEDULE_CURRENT_DESTINATION, npc);
	}

	/// Returns the NPC's currently-scheduled action — typically the activity name they plan to
	/// perform when they arrive at the destination. Schedule's notion of "action" is the
	/// pre-arrival intent; the actual activity that runs is set later by the activity_handler.
	///
	/// Wraps `schedule_current_action@T2r@T2r`. Returns the raw RValue.
	inline YYTK::RValue GetCurrentAction(MMAPI::NPC::Ids npc)
	{
		return Internal::CallScheduleScriptWithNpc(Internal::GML_SCRIPT_SCHEDULE_CURRENT_ACTION, npc);
	}

	/// True when the NPC has reached the destination of their current scheduled action and is
	/// ready to perform the action there.
	///
	/// Wraps `schedule_current_action_has_arrived@T2r@T2r`.
	inline bool HasArrivedAtDestination(MMAPI::NPC::Ids npc)
	{
		YYTK::RValue result = Internal::CallScheduleScriptWithNpc(Internal::GML_SCRIPT_SCHEDULE_CURRENT_ACTION_HAS_ARRIVED, npc);
		return result.ToBoolean();
	}

	/// Returns every registered schedule name in the T2r database. Useful for discovery —
	/// modders can list available schedules to know what they can request.
	///
	/// Wraps `schedule_names@T2r@T2r`. Result is expected to be a string array; we iterate and
	/// collect into a vector.
	inline std::vector<std::string> ListScheduleNames()
	{
		std::vector<std::string> names;

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::T2::Internal::TryGetT2Context(Self, Other))
			return names;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_SCHEDULE_NAMES, reinterpret_cast<PVOID*>(&gml_script));
		if (!gml_script)
			return names;

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);

		if (result.m_Kind != YYTK::VALUE_ARRAY)
			return names;

		size_t length = 0;
		MMAPI::Internal::module_interface->GetArraySize(result, length);
		names.reserve(length);
		for (size_t i = 0; i < length; i++)
		{
			YYTK::RValue* entry = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(result, i, entry);
			if (entry && entry->m_Kind == YYTK::VALUE_STRING)
				names.emplace_back(entry->ToString());
		}
		return names;
	}

	namespace Hooks
	{
		/// Registers a callback that runs before the game starts a schedule for an NPC. The
		/// context exposes the schedule name (when the script's arg[0] is a string).
		inline MMAPI::Status BeforeScheduleStart(Internal::BeforeScheduleStartCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Schedule::Hooks::BeforeScheduleStart, MMAPI::Schedule);

			return MMAPI::Internal::RegisterHook(
				"Schedule::BeforeScheduleStart",
				Internal::before_schedule_start_callback,
				callback
			);
		}

		/// Registers a callback that runs after a schedule ends.
		inline MMAPI::Status AfterScheduleEnd(Internal::AfterScheduleEndCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Schedule::Hooks::AfterScheduleEnd, MMAPI::Schedule);

			return MMAPI::Internal::RegisterHook(
				"Schedule::AfterScheduleEnd",
				Internal::after_schedule_end_callback,
				callback
			);
		}
	}
}
