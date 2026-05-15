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

namespace MMAPI::VitalsMenu
{
	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_SET_MAX_HEALTH = "gml_Script_set_max_health@VitalsMenu@VitalsMenu";
		inline constexpr const char* GML_SCRIPT_SET_HEALTH     = "gml_Script_set_health@VitalsMenu@VitalsMenu";

		// Live VitalsMenu Self/Other, latched from the set_max_health hook (fires whenever the HUD reflects a health-bar change).
		// Used by TryGetVitalsMenuContext for callers outside any hook frame.
		inline YYTK::CInstance* vitals_menu_self  = nullptr;
		inline YYTK::CInstance* vitals_menu_other = nullptr;

		// Cleared from the setup_main_screen pub/sub when the player returns to the title menu.
		// Registered by VitalsMenu::Enable().
		inline void ClearVitalsMenuOnReturnToTitle(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			vitals_menu_self  = nullptr;
			vitals_menu_other = nullptr;
		}

		inline YYTK::RValue& GmlScriptVitalsMenuSetMaxHealthCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			// Latch on first observation only (matches pre-MMAPI DD's pattern for this script).
			// See StatusEffect's manager-update comment for the failure mode of re-latching.
			if (!vitals_menu_self)
			{
				vitals_menu_self  = Self;
				vitals_menu_other = Other;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_SET_MAX_HEALTH)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		/// Resolves the VitalsMenu's GML calling context, latched from the most recent set_max_health call.
		/// Cleared automatically when the game returns to the title menu.
		/// @return True if a VitalsMenu set_max_health call has been observed this session, false otherwise.
		inline bool TryGetVitalsMenuContext(YYTK::CInstance*& Self, YYTK::CInstance*& Other)
		{
			if (!vitals_menu_self)
				return false;
			Self  = vitals_menu_self;
			Other = vitals_menu_other;
			return true;
		}
	}

	/// Activates VitalsMenu utility functions. Installs the set_max_health hook so the live VitalsMenu Self/Other
	/// are latched for TryGetVitalsMenuContext (cleared on return-to-title via the setup_main_screen pub/sub).
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::VitalsMenu::Enable() called");

		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ClearVitalsMenuOnReturnToTitle);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ MMAPI::Internal::GML_SCRIPT_SETUP_MAIN_SCREEN, reinterpret_cast<PVOID>(MMAPI::Internal::GmlScriptBeforeSetupMainScreenCallback) },
			{ Internal::GML_SCRIPT_SET_MAX_HEALTH,           reinterpret_cast<PVOID>(Internal::GmlScriptVitalsMenuSetMaxHealthCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Updates the HUD health bar's maximum value. Does not modify Ari's actual max health — only the displayed bar's max.
	/// Typically paired with SetHealth() to also refresh the current-value fill.
	/// @attention Requires MMAPI::VitalsMenu::Enable() to have been called.
	/// @param max_health The new max health value to display.
	/// @return True if the script was invoked, false if the required context is unavailable.
	inline bool SetMaxHealth(int max_health)
	{
		MMAPI_REQUIRE_ENABLED("VitalsMenu", false);

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetVitalsMenuContext(Self, Other))
			return false;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_SET_MAX_HEALTH, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue value = max_health;
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &value };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
		return true;
	}

	/// Updates the HUD health bar's current-value fill. Does not modify Ari's actual health — only the displayed bar.
	/// @attention Requires MMAPI::VitalsMenu::Enable() to have been called.
	/// @param current_health The current health value to display.
	/// @param max_health The max health value to display.
	/// @return True if the script was invoked, false if the required context is unavailable.
	inline bool SetHealth(int current_health, int max_health)
	{
		MMAPI_REQUIRE_ENABLED("VitalsMenu", false);

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetVitalsMenuContext(Self, Other))
			return false;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_SET_HEALTH, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue current = current_health;
		YYTK::RValue maximum = max_health;
		YYTK::RValue result;
		YYTK::RValue* args[2] = { &current, &maximum };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 2, args);
		return true;
	}
}
