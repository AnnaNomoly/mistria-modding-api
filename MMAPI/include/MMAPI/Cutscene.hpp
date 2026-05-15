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

namespace MMAPI::Cutscene
{
	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_MIST_IS_RUNNING = "gml_Script_is_running@Mist@Mist";

		// Latest value observed from the is_running@Mist@Mist hook. The game itself polls this
		// predicate every frame to drive its cutscene state machine, so by the time any mod-side
		// draw or hook callback reads it, it reflects current-frame state. Cleared on return-to-title.
		inline bool is_running_cached = false;

		// Cleared from the setup_main_screen pub/sub when the player returns to the title menu.
		// Registered by Cutscene::Enable().
		inline void ClearOnReturnToTitle(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			is_running_cached = false;
		}

		inline YYTK::RValue& GmlScriptMistIsRunningCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_MIST_IS_RUNNING)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (Result.m_Kind == YYTK::VALUE_BOOL)
				is_running_cached = Result.ToBoolean();

			return Result;
		}
	}

	/// Activates Cutscene utility functions. Installs the `is_running@Mist@Mist` hook so the
	/// cached cutscene-running flag is kept up to date (cleared on return-to-title via the
	/// setup_main_screen pub/sub).
	/// @return Status::Success if the hook is installed (or already was); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Cutscene::Enable() called");

		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ClearOnReturnToTitle);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ MMAPI::Internal::GML_SCRIPT_SETUP_MAIN_SCREEN, reinterpret_cast<PVOID>(MMAPI::Internal::GmlScriptBeforeSetupMainScreenCallback) },
			{ Internal::GML_SCRIPT_MIST_IS_RUNNING,          reinterpret_cast<PVOID>(Internal::GmlScriptMistIsRunningCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Returns true if a Mist cutscene is currently running.
	///
	/// The value is cached from the most recent `is_running@Mist@Mist` call — the game polls this
	/// predicate every frame to drive its cutscene state machine, so by the time mod-side draw or
	/// hook callbacks read it, it reflects current-frame state. Returns false in fresh sessions
	/// before the game has run the predicate at least once (e.g. while still on the title screen).
	///
	/// @attention Requires MMAPI::Cutscene::Enable() to have been called.
	/// @return True if a cutscene is currently running.
	inline bool IsRunning()
	{
		MMAPI_REQUIRE_ENABLED("Cutscene", false);
		return Internal::is_running_cached;
	}
}
