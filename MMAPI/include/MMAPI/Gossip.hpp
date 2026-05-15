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

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Gossip
{
	struct AfterGetSelectionsContext
	{
		YYTK::RValue* m_result_ptr = nullptr;

		/// The gossip selections array the game's `get_gossip_selections` script produced.
		/// A raw GM array — use the YYTK module interface's `GetArraySize` / `GetArrayEntry` to iterate.
		YYTK::RValue GetSelections() const { return m_result_ptr ? *m_result_ptr : YYTK::RValue(); }

		/// Returns the number of selections currently in the array, or 0 if the array is unavailable.
		size_t Count() const
		{
			if (!m_result_ptr)
				return 0;
			size_t count = 0;
			MMAPI::Internal::module_interface->GetArraySize(*m_result_ptr, count);
			return count;
		}

		/// Replaces the selections array with an empty one. Use to suppress gossip for this fire —
		/// the game falls through to play the "no_gossip" conversation when the array is empty,
		/// which mods can intercept via `Text::Hooks::BeforePlayConversation` to substitute custom text.
		void Clear()
		{
			if (m_result_ptr)
				*m_result_ptr = MMAPI::Internal::module_interface->CallBuiltin("array_create", { 0 });
		}
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_GET_GOSSIP_SELECTIONS = "gml_Script_get_gossip_selections";
		inline constexpr const char* GML_SCRIPT_GOSSIP_MENU_ON_CLOSE  = "gml_Script_on_close@GossipMenu@GossipMenu";

		using AfterGetSelectionsCallback = void(*)(MMAPI::Gossip::AfterGetSelectionsContext&);
		using AfterCloseCallback         = void(*)();

		inline AfterGetSelectionsCallback after_get_selections_callback = nullptr;
		inline AfterCloseCallback         after_close_callback          = nullptr;

		inline YYTK::RValue& GmlScriptAfterGetSelectionsCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_GET_GOSSIP_SELECTIONS)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_get_selections_callback)
			{
				MMAPI::Gossip::AfterGetSelectionsContext context{ &Result };
				after_get_selections_callback(context);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterCloseCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_GOSSIP_MENU_ON_CLOSE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_close_callback)
				after_close_callback();

			return Result;
		}
	}

	/// Activates Gossip utility functions. Eagerly installs the `get_gossip_selections` and
	/// `on_close@GossipMenu@GossipMenu` script hooks used by `Hooks::AfterGetSelections` and
	/// `Hooks::AfterClose`.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Gossip::Enable() called");

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_GET_GOSSIP_SELECTIONS, reinterpret_cast<PVOID>(Internal::GmlScriptAfterGetSelectionsCallback) },
			{ Internal::GML_SCRIPT_GOSSIP_MENU_ON_CLOSE,  reinterpret_cast<PVOID>(Internal::GmlScriptAfterCloseCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Returns true if Ari has already gossiped today. Read from `__ari.has_gossiped_today`.
	inline bool HasGossipedToday()
	{
		if (!MMAPI::Internal::global_instance)
			return false;

		YYTK::RValue ari = MMAPI::Internal::global_instance->GetMember("__ari");
		if (ari.m_Kind == YYTK::VALUE_UNDEFINED)
			return false;

		if (!MMAPI::Engine::StructVariableExists(ari, "has_gossiped_today"))
			return false;

		return ari.GetMember("has_gossiped_today").ToBoolean();
	}

	/// Sets the `has_gossiped_today` flag on Ari. Set to false to allow the gossip menu to be
	/// used again today (e.g. as a perk reward).
	/// @param value The new value of the flag.
	inline void SetHasGossipedToday(bool value)
	{
		if (!MMAPI::Internal::global_instance)
			return;

		YYTK::RValue ari = *MMAPI::Internal::global_instance->GetRefMember("__ari");
		*ari.GetRefMember("has_gossiped_today") = value;
	}

	namespace Hooks
	{
		/// Registers a callback that runs after the game's `get_gossip_selections` script. Read
		/// `ctx.GetSelections()` to inspect the selections array, `ctx.Count()` for its size, and
		/// call `ctx.Clear()` to suppress gossip for this fire (the game falls through to play
		/// the "no_gossip" conversation, which mods can intercept via `Text::Hooks::BeforePlayConversation`).
		/// @param callback A function called with a mutable `MMAPI::Gossip::AfterGetSelectionsContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterGetSelections(Internal::AfterGetSelectionsCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Gossip::Hooks::AfterGetSelections, MMAPI::Gossip);

			return MMAPI::Internal::RegisterHook(
				"Gossip::AfterGetSelections",
				Internal::after_get_selections_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `on_close@GossipMenu@GossipMenu` script —
		/// fires whenever the gossip menu closes. Use to react after dismissal (e.g. to grant a
		/// "gossip again" perk by resetting `has_gossiped_today`).
		/// @param callback A parameterless function called after the gossip menu closes.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterClose(Internal::AfterCloseCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Gossip::Hooks::AfterClose, MMAPI::Gossip);

			return MMAPI::Internal::RegisterHook(
				"Gossip::AfterClose",
				Internal::after_close_callback,
				callback
			);
		}
	}
}
