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

namespace MMAPI::ToolbarMenu
{
	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_UPDATE_TOOLBAR_MENU = "gml_Script_update@ToolbarMenu@ToolbarMenu";

		// Live ToolbarMenu Self/Other, latched per-tick from the update hook.
		// Used by TryGetToolbarMenuContext for callers outside any hook frame.
		inline YYTK::CInstance* toolbar_menu_self  = nullptr;
		inline YYTK::CInstance* toolbar_menu_other = nullptr;

		// Cleared from the setup_main_screen pub/sub when the player returns to the title menu.
		// Registered by ToolbarMenu::Enable().
		inline void ClearToolbarMenuOnReturnToTitle(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			toolbar_menu_self  = nullptr;
			toolbar_menu_other = nullptr;
		}

		inline YYTK::RValue& GmlScriptToolbarMenuUpdateCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			// Latch on first observation only (matches pre-MMAPI DD's pattern for this script).
			// See StatusEffect's manager-update comment for the failure mode of re-latching.
			if (!toolbar_menu_self)
			{
				toolbar_menu_self  = Self;
				toolbar_menu_other = Other;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_UPDATE_TOOLBAR_MENU)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		/// Resolves the ToolbarMenu's GML calling context, latched from the most recent update tick.
		/// Cleared automatically when the game returns to the title menu.
		/// @return True if a ToolbarMenu tick has been observed this session, false otherwise.
		inline bool TryGetToolbarMenuContext(YYTK::CInstance*& Self, YYTK::CInstance*& Other)
		{
			if (!toolbar_menu_self)
				return false;
			Self  = toolbar_menu_self;
			Other = toolbar_menu_other;
			return true;
		}
	}

	/// Activates ToolbarMenu utility functions. Installs the update hook so the live ToolbarMenu Self/Other are latched
	/// for TryGetToolbarMenuContext (cleared on return-to-title via the setup_main_screen pub/sub).
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::ToolbarMenu::Enable() called");

		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ClearToolbarMenuOnReturnToTitle);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ MMAPI::Internal::GML_SCRIPT_SETUP_MAIN_SCREEN, reinterpret_cast<PVOID>(MMAPI::Internal::GmlScriptBeforeSetupMainScreenCallback) },
			{ Internal::GML_SCRIPT_UPDATE_TOOLBAR_MENU,      reinterpret_cast<PVOID>(Internal::GmlScriptToolbarMenuUpdateCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Forces the toolbar menu to re-run its update script (refreshes equipped-slot rendering, stack counts, etc.).
	/// @attention Requires MMAPI::ToolbarMenu::Enable() to have been called.
	/// @return True if the update script was invoked, false if the required context is unavailable.
	inline bool ForceUpdate()
	{
		MMAPI_REQUIRE_ENABLED("ToolbarMenu", false);

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetToolbarMenuContext(Self, Other))
			return false;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_UPDATE_TOOLBAR_MENU, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		return true;
	}
}
