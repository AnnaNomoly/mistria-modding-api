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

namespace MMAPI::Fish
{
	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_FISH_CELEBRATION_DATA = "gml_Script_get_celebration_data_essence_exp@anon@15053@Fish@Fish";
		inline constexpr const char* GML_SCRIPT_DIVE_CELEBRATION_DATA = "gml_Script_get_celebration_data@anon@15884@DiveSpot@Fish";

		using AfterFishCelebrationCallback = void(*)();
		using AfterDiveCelebrationCallback = void(*)();

		inline AfterFishCelebrationCallback after_fish_celebration_callback = nullptr;
		inline AfterDiveCelebrationCallback after_dive_celebration_callback = nullptr;

		inline YYTK::RValue& GmlScriptAfterFishCelebrationCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_FISH_CELEBRATION_DATA)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_fish_celebration_callback)
				after_fish_celebration_callback();

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterDiveCelebrationCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_DIVE_CELEBRATION_DATA)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_dive_celebration_callback)
				after_dive_celebration_callback();

			return Result;
		}
	}

	/// Activates Fish utility hooks. Eagerly installs the fish/dive celebration-data script hooks
	/// used by Hooks::AfterFishCelebration and Hooks::AfterDiveCelebration.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Fish::Enable() called");

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_FISH_CELEBRATION_DATA, reinterpret_cast<PVOID>(Internal::GmlScriptAfterFishCelebrationCallback) },
			{ Internal::GML_SCRIPT_DIVE_CELEBRATION_DATA, reinterpret_cast<PVOID>(Internal::GmlScriptAfterDiveCelebrationCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// True once the fish's loot data has been attached. Most other Fish
	/// accessors only succeed after this returns true; the game populates the
	/// loot a few frames after the obj_fish instance is created. Cheap; safe
	/// to call every tick as the gate in an OnObjectCall(Objects::Fish, ...)
	/// callback.
	/// @param fish A live obj_fish CInstance pointer.
	/// @return True if `fish.fish_loot` exists on the instance.
	inline bool HasLoot(YYTK::CInstance* fish)
	{
		if (!fish) return false;
		YYTK::RValue rv = fish->ToRValue();
		return MMAPI::Engine::StructVariableExists(rv, "fish_loot");
	}

	/// Returns the live obj_fish's `fish_loot` struct for advanced inspection
	/// (rarity, quality, any other fields the game stashes there). Returns an
	/// undefined RValue if `fish` is null or the loot hasn't been populated
	/// yet - use HasLoot() first to gate.
	/// @param fish A live obj_fish CInstance pointer.
	inline YYTK::RValue TryGetLoot(YYTK::CInstance* fish)
	{
		if (!fish) return {};
		YYTK::RValue rv = fish->ToRValue();
		if (!MMAPI::Engine::StructVariableExists(rv, "fish_loot")) return {};
		return rv.GetMember("fish_loot");
	}

	/// Reads `fish_loot.item.item_id` off the live obj_fish instance: the item
	/// id the player will receive on a successful catch. Returns std::nullopt
	/// if `fish` is null, the loot isn't populated yet, or any segment of the
	/// path is missing or non-numeric.
	/// @param fish A live obj_fish CInstance pointer.
	inline std::optional<int> TryGetItemId(YYTK::CInstance* fish)
	{
		if (!fish) return std::nullopt;
		YYTK::RValue rv = fish->ToRValue();
		if (!MMAPI::Engine::StructVariableExists(rv, "fish_loot")) return std::nullopt;
		YYTK::RValue loot = rv.GetMember("fish_loot");
		if (loot.m_Kind != YYTK::VALUE_OBJECT) return std::nullopt;
		if (!MMAPI::Engine::StructVariableExists(loot, "item")) return std::nullopt;
		YYTK::RValue item = loot.GetMember("item");
		if (item.m_Kind != YYTK::VALUE_OBJECT) return std::nullopt;
		if (!MMAPI::Engine::StructVariableExists(item, "item_id")) return std::nullopt;
		YYTK::RValue id = item.GetMember("item_id");
		if (!MMAPI::Engine::IsNumeric(id)) return std::nullopt;
		return static_cast<int>(id.ToInt64());
	}

	/// Returns true exactly once per fish instance: the first time it's called
	/// for a given obj_fish after that fish has loot populated. Subsequent
	/// calls (for the same fish, from the same mod) return false.
	///
	/// Internally stamps a per-mod struct tag onto the instance (named after
	/// `MMAPI::Internal::mod_name`, set by MMAPI::Initialize), so multiple
	/// mods each get independent once-per-fish signals without colliding.
	///
	/// Typical usage inside an OnObjectCall(Objects::Fish, ...) callback:
	///
	///   if (!MMAPI::Fish::TryMarkProcessed(fish))
	///       return;
	///   auto item_id = MMAPI::Fish::TryGetItemId(fish);  // safe: loot present
	///   // ... do once-per-fish work here ...
	///
	/// @param fish A live obj_fish CInstance pointer.
	/// @return True the first time loot is present and the tag isn't set yet;
	///         false on every subsequent call (or if fish is null / loot
	///         isn't populated yet).
	inline bool TryMarkProcessed(YYTK::CInstance* fish)
	{
		if (!fish) return false;
		YYTK::RValue rv = fish->ToRValue();
		if (!MMAPI::Engine::StructVariableExists(rv, "fish_loot")) return false;

		std::string tag = "__mmapi_fish_processed__" + MMAPI::Internal::mod_name;
		if (MMAPI::Engine::StructVariableExists(rv, tag.c_str())) return false;

		MMAPI::Engine::StructVariableSet(rv, tag.c_str(), YYTK::RValue(true));
		return true;
	}

	namespace Hooks
	{
		/// Registers a callback that runs after the game's fish celebration-data script. Fires when
		/// the game has computed the rewards for a successful fishing catch — the give_item that
		/// follows hands the player the celebration's items, so this is the right signal for "react
		/// to fishing success" patterns (e.g. flagging the next give_item for duplication).
		/// @param callback A parameter-less function called after the fish celebration script runs.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterFishCelebration(Internal::AfterFishCelebrationCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Fish::Hooks::AfterFishCelebration, MMAPI::Fish);

			return MMAPI::Internal::RegisterHook(
				"Fish::AfterFishCelebration",
				Internal::after_fish_celebration_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's dive celebration-data script. Fires when
		/// the game has computed the rewards for a successful dive — the give_item that follows
		/// hands the player the celebration's items. Same shape as AfterFishCelebration; the two
		/// are split because most mods care about one or the other.
		/// @param callback A parameter-less function called after the dive celebration script runs.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterDiveCelebration(Internal::AfterDiveCelebrationCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Fish::Hooks::AfterDiveCelebration, MMAPI::Fish);

			return MMAPI::Internal::RegisterHook(
				"Fish::AfterDiveCelebration",
				Internal::after_dive_celebration_callback,
				callback
			);
		}
	}
}
