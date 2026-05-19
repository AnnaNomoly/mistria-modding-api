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

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Dates
{
	/// Source: globalInstance.__date__
	enum class Ids : int
	{
		Bathhouse        = 0,
		Beach            = 1,
		DeepWoodsPicnic  = 2,
		GemCutting       = 3,
		HarvestDance     = 4,
		InnMeal          = 5,
		Park             = 6,
		ShootingStar     = 7
	};

	/// Total number of enumerators in Ids. Iterating [0, IdCount) covers every Ids value.
	inline constexpr int IdCount = 8;

	/// Invokes fn with every Ids value, in ascending order.
	template <typename Fn>
	inline void ForEachId(Fn fn)
	{
		for (int i = 0; i < IdCount; ++i)
			fn(static_cast<Ids>(i));
	}

	struct BeforeRunDateContext
	{
		int m_date_id = 0;
		int m_npc_id  = -1;
		bool m_has_npc = false;

		int  GetDateId() const   { return m_date_id; }
		Ids  GetDate() const     { return static_cast<Ids>(m_date_id); }
		bool HasNpc() const      { return m_has_npc; }
		int  GetNpcId() const    { return m_npc_id; }
		MMAPI::NPC::Ids GetNpc() const { return static_cast<MMAPI::NPC::Ids>(m_npc_id); }
	};

	struct AfterRunDateContext
	{
		int m_date_id = 0;
		int m_npc_id  = -1;
		bool m_has_npc = false;

		int  GetDateId() const   { return m_date_id; }
		Ids  GetDate() const     { return static_cast<Ids>(m_date_id); }
		bool HasNpc() const      { return m_has_npc; }
		int  GetNpcId() const    { return m_npc_id; }
		MMAPI::NPC::Ids GetNpc() const { return static_cast<MMAPI::NPC::Ids>(m_npc_id); }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_OUTFIT_FOR_DATE      = "gml_Script_outfit_for_date";
		inline constexpr const char* GML_SCRIPT_ARI_ELIGIBLE         = "gml_Script_ari_eligible_for_date";
		inline constexpr const char* GML_SCRIPT_NPC_DATE_ELIGIBILITY = "gml_Script_npc_date_eligibility";
		inline constexpr const char* GML_SCRIPT_RUN_DATE             = "gml_Script_run_date";
		inline constexpr const char* GML_SCRIPT_CAN_GO_ON_DATES      = "gml_Script_can_go_on_dates@Npc@Npc";

		// Override stores. Outfit and NPC-eligibility are keyed by (npc_id, date_id) pairs;
		// Ari eligibility and can-go-on-dates are keyed by npc_id (both per-NPC predicates).
		// Reads of the underlying scripts go through hooks that consult these maps after the
		// original returns and replace Result if a matching override is present.
		inline std::unordered_map<int, std::string>             outfit_overrides_by_npc_date;     // key = npc_id*256 + date_id
		inline std::unordered_map<int, bool>                    ari_eligibility_overrides;        // key = npc_id
		inline std::unordered_map<int, bool>                    npc_eligibility_overrides;        // key = npc_id*256 + date_id
		inline std::unordered_map<int, bool>                    can_go_on_dates_overrides;        // key = npc_id

		inline int PackNpcDate(int npc_id, int date_id) { return npc_id * 256 + date_id; }

		using BeforeRunDateCallback = void(*)(MMAPI::Dates::BeforeRunDateContext&);
		using AfterRunDateCallback  = void(*)(MMAPI::Dates::AfterRunDateContext&);

		inline BeforeRunDateCallback before_run_date_callback = nullptr;
		inline AfterRunDateCallback  after_run_date_callback  = nullptr;

		// Read arg[0] as date_id and arg[1] as npc_id. Matches run_date's signature, confirmed
		// empirically (Dates::Start(Beach=1, Reina=24) triggered the beach + Reina cutscene).
		// NOTE: `outfit_for_date` and `ari_eligible_for_date` use different signatures — see their
		// hooks below.
		inline std::pair<int, std::optional<int>> ExtractDateNpcArgs(int ArgumentCount, YYTK::RValue** Arguments)
		{
			int date_id = -1;
			std::optional<int> npc_id;

			if (Arguments && ArgumentCount >= 1 && Arguments[0] && MMAPI::Engine::IsNumeric(*Arguments[0]))
				date_id = static_cast<int>(Arguments[0]->ToInt64());

			if (Arguments && ArgumentCount >= 2 && Arguments[1] && MMAPI::Engine::IsNumeric(*Arguments[1]))
				npc_id = static_cast<int>(Arguments[1]->ToInt64());

			return { date_id, npc_id };
		}

		inline YYTK::RValue& GmlScriptOutfitForDateCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_OUTFIT_FOR_DATE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			// Signature: outfit_for_date(npc_id, date_id) — confirmed empirically
			// (call returned 'beach' for arg[0]=24=Reina, arg[1]=1=Beach).
			int npc_id = -1, date_id = -1;
			if (Arguments && ArgumentCount >= 1 && Arguments[0] && MMAPI::Engine::IsNumeric(*Arguments[0]))
				npc_id = static_cast<int>(Arguments[0]->ToInt64());
			if (Arguments && ArgumentCount >= 2 && Arguments[1] && MMAPI::Engine::IsNumeric(*Arguments[1]))
				date_id = static_cast<int>(Arguments[1]->ToInt64());

			if (npc_id >= 0 && date_id >= 0)
			{
				auto it = outfit_overrides_by_npc_date.find(PackNpcDate(npc_id, date_id));
				if (it != outfit_overrides_by_npc_date.end())
					Result = it->second.c_str();
			}
			return Result;
		}

		inline YYTK::RValue& GmlScriptAriEligibleCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_ARI_ELIGIBLE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			// Signature: ari_eligible_for_date(npc_id). The script name suggests it takes a date
			// but empirically arg[0] is always an NPC id (the script is called as part of the
			// per-frame "can the player initiate a date with this NPC right now?" check, with
			// the NPC's id as the argument).
			int npc_id = -1;
			if (Arguments && ArgumentCount >= 1 && Arguments[0] && MMAPI::Engine::IsNumeric(*Arguments[0]))
				npc_id = static_cast<int>(Arguments[0]->ToInt64());

			if (npc_id >= 0)
			{
				auto it = ari_eligibility_overrides.find(npc_id);
				if (it != ari_eligibility_overrides.end())
					Result = it->second;
			}
			return Result;
		}

		inline YYTK::RValue& GmlScriptNpcDateEligibilityCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_NPC_DATE_ELIGIBILITY)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			// npc_date_eligibility signature follows its name: arg[0]=npc_id, arg[1]=date_id.
			int npc_id = -1, date_id = -1;
			if (Arguments && ArgumentCount >= 1 && Arguments[0] && MMAPI::Engine::IsNumeric(*Arguments[0]))
				npc_id = static_cast<int>(Arguments[0]->ToInt64());
			if (Arguments && ArgumentCount >= 2 && Arguments[1] && MMAPI::Engine::IsNumeric(*Arguments[1]))
				date_id = static_cast<int>(Arguments[1]->ToInt64());

			if (npc_id >= 0 && date_id >= 0)
			{
				auto it = npc_eligibility_overrides.find(PackNpcDate(npc_id, date_id));
				if (it != npc_eligibility_overrides.end())
					Result = it->second;
			}
			return Result;
		}

		inline YYTK::RValue& GmlScriptCanGoOnDatesCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_CAN_GO_ON_DATES)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			// Self is the Npc instance; its `id` member is the NPC index that aligns with
			// MMAPI::NPC::Ids.
			if (Self)
			{
				YYTK::RValue id_rv = Self->GetMember("id");
				if (MMAPI::Engine::IsNumeric(id_rv))
				{
					int npc_id = static_cast<int>(id_rv.ToInt64());
					auto it = can_go_on_dates_overrides.find(npc_id);
					if (it != can_go_on_dates_overrides.end())
						Result = it->second;
				}
			}
			return Result;
		}

		inline YYTK::RValue& GmlScriptRunDateCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			auto [date_id, npc_id] = ExtractDateNpcArgs(ArgumentCount, Arguments);

			if (before_run_date_callback && date_id >= 0)
			{
				MMAPI::Dates::BeforeRunDateContext ctx;
				ctx.m_date_id = date_id;
				ctx.m_has_npc = npc_id.has_value();
				ctx.m_npc_id  = npc_id.value_or(-1);
				before_run_date_callback(ctx);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_RUN_DATE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_run_date_callback && date_id >= 0)
			{
				MMAPI::Dates::AfterRunDateContext ctx;
				ctx.m_date_id = date_id;
				ctx.m_has_npc = npc_id.has_value();
				ctx.m_npc_id  = npc_id.value_or(-1);
				after_run_date_callback(ctx);
			}

			return Result;
		}

		inline YYTK::RValue GetDateData(Ids id)
		{
			YYTK::RValue dates = MMAPI::Internal::global_instance->GetMember("__dates");
			if (dates.m_Kind != YYTK::VALUE_ARRAY)
				return {};

			size_t length = 0;
			MMAPI::Internal::module_interface->GetArraySize(dates, length);

			int idx = static_cast<int>(id);
			if (idx < 0 || idx >= static_cast<int>(length))
				return {};

			YYTK::RValue* entry = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(dates, idx, entry);
			if (!entry)
				return {};
			return *entry;
		}

		inline std::string ReadStringMember(Ids id, const char* member_name)
		{
			YYTK::RValue data = GetDateData(id);
			if (data.m_Kind != YYTK::VALUE_OBJECT)
				return {};

			auto members = data.ToMap();
			auto it = members.find(member_name);
			if (it == members.end() || it->second.m_Kind != YYTK::VALUE_STRING)
				return {};
			return it->second.ToString();
		}
	}

	/// Activates the Dates module: installs hooks on the three eligibility/outfit scripts (so
	/// Set*Override functions work) and on `run_date` (so BeforeRunDate/AfterRunDate fire).
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Dates::Enable() called");

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_OUTFIT_FOR_DATE,      reinterpret_cast<PVOID>(Internal::GmlScriptOutfitForDateCallback) },
			{ Internal::GML_SCRIPT_ARI_ELIGIBLE,         reinterpret_cast<PVOID>(Internal::GmlScriptAriEligibleCallback) },
			{ Internal::GML_SCRIPT_NPC_DATE_ELIGIBILITY, reinterpret_cast<PVOID>(Internal::GmlScriptNpcDateEligibilityCallback) },
			{ Internal::GML_SCRIPT_CAN_GO_ON_DATES,      reinterpret_cast<PVOID>(Internal::GmlScriptCanGoOnDatesCallback) },
			{ Internal::GML_SCRIPT_RUN_DATE,             reinterpret_cast<PVOID>(Internal::GmlScriptRunDateCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Returns the cutscene name for a date (e.g. "bathhouse_date"). Read directly from
	/// globalInstance.__dates; does NOT require Enable().
	inline std::string GetCutsceneName(Ids id)
	{
		return Internal::ReadStringMember(id, "cutscene");
	}

	/// Returns the date's localized-name text key (e.g. "dates/bathhouse/name"). This is the raw
	/// key, not the translated string — resolve via the game's text system if needed.
	inline std::string GetName(Ids id)
	{
		return Internal::ReadStringMember(id, "name");
	}

	/// Returns the date's localized-description text key (e.g. "dates/bathhouse/description").
	/// Raw key, not translated.
	inline std::string GetDescription(Ids id)
	{
		return Internal::ReadStringMember(id, "description");
	}

	/// Wraps `gml_Script_outfit_for_date(npc_id, date_id)`. Returns the outfit name (e.g.
	/// "beach") the named NPC will wear for this date, or empty if the game's script returns
	/// undefined/null (meaning the NPC's default outfit is used).
	inline std::string GetOutfitForDate(MMAPI::NPC::Ids npc, Ids date)
	{
		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_OUTFIT_FOR_DATE, reinterpret_cast<PVOID*>(&gml_script));
		if (!gml_script)
			return {};

		YYTK::RValue n = static_cast<int>(npc);
		YYTK::RValue d = static_cast<int>(date);
		YYTK::RValue* args[2] = { &n, &d };
		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(nullptr, nullptr, result, 2, args);

		if (result.m_Kind == YYTK::VALUE_STRING)
			return result.ToString();
		return {};
	}

	/// Wraps `gml_Script_ari_eligible_for_date(npc_id)`. True when Ari currently meets the
	/// per-NPC requirements to start a date with this NPC (daily cooldown, day-of-week, story
	/// state, etc.). The script's name is misleading — its argument is an NPC id, not a date id;
	/// it asks "can Ari date this person right now?" rather than "is this date type valid?"
	inline bool IsAriEligibleForDate(MMAPI::NPC::Ids npc)
	{
		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_ARI_ELIGIBLE, reinterpret_cast<PVOID*>(&gml_script));
		if (!gml_script)
			return false;

		YYTK::RValue n = static_cast<int>(npc);
		YYTK::RValue* args[1] = { &n };
		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(nullptr, nullptr, result, 1, args);

		return result.ToBoolean();
	}

	/// Wraps `gml_Script_npc_date_eligibility(npc_id, date_id)`. True when the named NPC can
	/// currently be invited on this specific date type.
	inline bool IsNpcEligibleForDate(MMAPI::NPC::Ids npc, Ids date)
	{
		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_NPC_DATE_ELIGIBILITY, reinterpret_cast<PVOID*>(&gml_script));
		if (!gml_script)
			return false;

		YYTK::RValue n = static_cast<int>(npc);
		YYTK::RValue d = static_cast<int>(date);
		YYTK::RValue* args[2] = { &n, &d };
		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(nullptr, nullptr, result, 2, args);

		return result.ToBoolean();
	}

	/// Forces a specific outfit for the named NPC on the named date. Persists until cleared.
	/// Requires Enable() so the underlying hook is installed.
	inline void SetOutfitOverride(MMAPI::NPC::Ids npc, Ids date, const std::string& outfit)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Dates");
		Internal::outfit_overrides_by_npc_date[Internal::PackNpcDate(static_cast<int>(npc), static_cast<int>(date))] = outfit;
	}

	/// Removes any active outfit override for the (npc, date) pair, restoring the game's default
	/// `outfit_for_date` behaviour.
	inline void ClearOutfitOverride(MMAPI::NPC::Ids npc, Ids date)
	{
		Internal::outfit_overrides_by_npc_date.erase(Internal::PackNpcDate(static_cast<int>(npc), static_cast<int>(date)));
	}

	/// Forces Ari's eligibility result for dating a specific NPC (true = always eligible,
	/// false = never). This overrides the daily-cooldown / day-of-week / etc. gate inside
	/// `ari_eligible_for_date`. Persists until cleared. Requires Enable().
	inline void SetAriEligibilityOverride(MMAPI::NPC::Ids npc, bool eligible)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Dates");
		Internal::ari_eligibility_overrides[static_cast<int>(npc)] = eligible;
	}

	/// Removes Ari's eligibility override for the named NPC, restoring the game's default
	/// `ari_eligible_for_date` behaviour.
	inline void ClearAriEligibilityOverride(MMAPI::NPC::Ids npc)
	{
		Internal::ari_eligibility_overrides.erase(static_cast<int>(npc));
	}

	/// Forces the named NPC's eligibility for the named date. Persists until cleared. Requires
	/// Enable() so the underlying hook is installed.
	inline void SetNpcEligibilityOverride(MMAPI::NPC::Ids npc, Ids date, bool eligible)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Dates");
		Internal::npc_eligibility_overrides[Internal::PackNpcDate(static_cast<int>(npc), static_cast<int>(date))] = eligible;
	}

	/// Removes the NPC's eligibility override for the named date, restoring the game's default
	/// `npc_date_eligibility` behaviour.
	inline void ClearNpcEligibilityOverride(MMAPI::NPC::Ids npc, Ids date)
	{
		Internal::npc_eligibility_overrides.erase(Internal::PackNpcDate(static_cast<int>(npc), static_cast<int>(date)));
	}

	/// Forces the result of the NPC's `can_go_on_dates` method — the per-NPC "is a date even
	/// possible today?" predicate. This is a separate gate from `npc_date_eligibility` (which
	/// is per-date); both have to pass for a date to be offered. The default check almost
	/// certainly enforces the in-game day-of-week constraint (SAT/SUN only), so overriding
	/// this is the key to making dates testable from a save that isn't on a date day.
	///
	/// Persists until cleared. Requires Enable().
	inline void SetCanGoOnDatesOverride(MMAPI::NPC::Ids npc, bool can_go)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Dates");
		Internal::can_go_on_dates_overrides[static_cast<int>(npc)] = can_go;
	}

	/// Removes the can_go_on_dates override for an NPC, restoring the game's default behaviour.
	inline void ClearCanGoOnDatesOverride(MMAPI::NPC::Ids npc)
	{
		Internal::can_go_on_dates_overrides.erase(static_cast<int>(npc));
	}

	/// Directly invokes `gml_Script_run_date(date_id, npc_id)`, bypassing every eligibility gate
	/// (day-of-week, heart level, schedule, etc.). Intended for two use cases:
	///   1. Scripted story content — a mod-authored cutscene that ends with a date.
	///   2. Testing — verifying that the BeforeRunDate / AfterRunDate hooks fire and the date
	///      itself plays correctly without needing to set up the in-game preconditions.
	///
	/// @attention Bypasses ALL gates. If the player isn't actually in a state where the date
	/// makes narrative sense, you may end up with strange interactions (e.g. NPC schedule still
	/// running, time-of-day wrong). Use carefully.
	///
	/// @attention Requires Enable() so the underlying script pointer is resolvable.
	/// @return Status::Success if the script was invoked; Status::InvalidParameter if the script
	///         couldn't be resolved.
	inline MMAPI::Status Start(Ids date, MMAPI::NPC::Ids npc)
	{
		MMAPI_REQUIRE_ENABLED("Dates", MMAPI::Status::NotInitialized);

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_RUN_DATE, reinterpret_cast<PVOID*>(&gml_script));
		if (!gml_script)
		{
			MMAPI::Log::Warn("Dates::Start: %s not resolvable", Internal::GML_SCRIPT_RUN_DATE);
			return MMAPI::Status::InvalidParameter;
		}

		YYTK::RValue d = static_cast<int>(date);
		YYTK::RValue n = static_cast<int>(npc);
		YYTK::RValue* args[2] = { &d, &n };
		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(nullptr, nullptr, result, 2, args);
		return MMAPI::Status::Success;
	}

	namespace Hooks
	{
		/// Registers a callback that runs before `gml_Script_run_date` — the top-level date
		/// execution entry point. The context exposes date and (when present in args) NPC ids.
		inline MMAPI::Status BeforeRunDate(Internal::BeforeRunDateCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Dates::Hooks::BeforeRunDate, MMAPI::Dates);

			return MMAPI::Internal::RegisterHook(
				"Dates::BeforeRunDate",
				Internal::before_run_date_callback,
				callback
			);
		}

		/// Registers a callback that runs after `gml_Script_run_date` returns.
		inline MMAPI::Status AfterRunDate(Internal::AfterRunDateCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Dates::Hooks::AfterRunDate, MMAPI::Dates);

			return MMAPI::Internal::RegisterHook(
				"Dates::AfterRunDate",
				Internal::after_run_date_callback,
				callback
			);
		}
	}
}
