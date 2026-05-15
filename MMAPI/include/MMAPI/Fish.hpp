// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Hook.hpp"
#include "Log.hpp"
#include "Status.hpp"

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
