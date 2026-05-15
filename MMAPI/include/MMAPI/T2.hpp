// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Hook.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::T2
{
	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_T2_READ = "gml_Script_read@T2r@T2r";

		// Live T2r Self/Other, latched from the read hook.
		// Used by TryGetT2Context for callers outside any hook frame.
		inline YYTK::CInstance* t2_self  = nullptr;
		inline YYTK::CInstance* t2_other = nullptr;

		// Cleared from the setup_main_screen pub/sub when the player returns to the title menu.
		// Registered by T2::Enable().
		inline void ClearT2OnReturnToTitle(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			t2_self  = nullptr;
			t2_other = nullptr;
		}

		inline YYTK::RValue& T2ReadContextCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			// Latch on first observation only (matches pre-MMAPI DD's pattern for this script).
			// See StatusEffect's manager-update comment for the failure mode of re-latching.
			if (!t2_self)
			{
				t2_self  = Self;
				t2_other = Other;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_T2_READ));
			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		/// Resolves the T2r's GML calling context, latched from the most recent read call.
		/// Cleared automatically when the game returns to the title menu.
		/// @return True if a T2 read has been observed this session, false otherwise.
		inline bool TryGetT2Context(YYTK::CInstance*& Self, YYTK::CInstance*& Other)
		{
			if (!t2_self)
				return false;
			Self  = t2_self;
			Other = t2_other;
			return true;
		}
	}

	/// Activates T2 utility functions. Installs the read hook so the live T2r Self/Other are latched for
	/// TryGetT2Context (cleared on return-to-title via the setup_main_screen pub/sub).
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::T2::Enable() called");

		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ClearT2OnReturnToTitle);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ MMAPI::Internal::GML_SCRIPT_SETUP_MAIN_SCREEN, reinterpret_cast<PVOID>(MMAPI::Internal::GmlScriptBeforeSetupMainScreenCallback) },
			{ Internal::GML_SCRIPT_T2_READ,                  reinterpret_cast<PVOID>(Internal::T2ReadContextCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Reads a value from the game's T2 database by key.
	/// @attention Requires MMAPI::T2::Enable() to have been called.
	/// @param key The T2 key to read.
	/// @return The T2 value as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue Read(const std::string& key)
	{
		MMAPI_REQUIRE_ENABLED("T2", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetT2Context(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_T2_READ, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		YYTK::RValue input = key.c_str();
		YYTK::RValue* args[1] = { &input };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
		return result;
	}
}
