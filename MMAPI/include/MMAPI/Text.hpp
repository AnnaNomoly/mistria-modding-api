// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Hook.hpp"
#include "Instance.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Text
{
	struct LocalizedStringContext
	{
		std::string m_key;

		std::string_view GetKey() const { return m_key; }
		void SetKey(std::string key) { m_key = std::move(key); }
	};

	struct AfterLocalizedStringContext
	{
		std::string m_key;
		std::string m_resolved;

		std::string_view GetKey() const { return m_key; }
		std::string_view GetResolved() const { return m_resolved; }
		void SetResolved(std::string resolved) { m_resolved = std::move(resolved); }
	};

	struct PlayConversationContext
	{
		std::string m_key;
		bool m_cancelled = false;

		std::string_view GetKey() const { return m_key; }
		void SetKey(std::string key) { m_key = std::move(key); }
		void Cancel() { m_cancelled = true; }
	};

	struct PlayTextContext
	{
		std::string m_key;
		bool m_cancelled = false;

		std::string_view GetKey() const { return m_key; }
		void SetKey(std::string key) { m_key = std::move(key); }
		void Cancel() { m_cancelled = true; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_GET_LOCALIZER     = "gml_Script_get@Localizer@Localizer";
		inline constexpr const char* GML_SCRIPT_PLAY_CONVERSATION = "gml_Script_play_conversation";
		inline constexpr const char* GML_SCRIPT_CLOSE_TEXTBOX     = "gml_Script_begin_close@TextboxMenu@TextboxMenu";
		inline constexpr const char* GML_SCRIPT_PLAY_TEXT         = "gml_Script_play_text@TextboxMenu@TextboxMenu";

		using BeforeLocalizedStringCallback  = void(*)(MMAPI::Text::LocalizedStringContext&);
		using AfterLocalizedStringCallback   = void(*)(MMAPI::Text::AfterLocalizedStringContext&);
		using BeforePlayConversationCallback = void(*)(MMAPI::Text::PlayConversationContext&);
		using BeforePlayTextCallback         = void(*)(MMAPI::Text::PlayTextContext&);

		inline BeforeLocalizedStringCallback  before_localized_string_callback  = nullptr;
		inline AfterLocalizedStringCallback   after_localized_string_callback   = nullptr;
		inline BeforePlayConversationCallback before_play_conversation_callback = nullptr;
		inline BeforePlayTextCallback         before_play_text_callback         = nullptr;

		// Live Localizer Self/Other, latched from the get_localizer hook
		// or pulled from globalInstance.__localizer on first request.
		// Used by TryGetLocalizerContext for callers outside any hook frame.
		inline YYTK::CInstance* localizer_self  = nullptr;
		inline YYTK::CInstance* localizer_other = nullptr;

		// Cleared from the setup_main_screen pub/sub when the player returns to the title menu.
		// Registered by Text::Enable().
		inline void ClearLocalizerOnReturnToTitle(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			localizer_self  = nullptr;
			localizer_other = nullptr;
		}

		/// Reads globalInstance.__localizer and converts it to a CInstance pointer.
		/// @return The Localizer instance pointer if available, nullptr otherwise.
		inline YYTK::CInstance* TryGetLocalizerFromGlobal()
		{
			if (!MMAPI::Internal::global_instance)
				return nullptr;

			YYTK::RValue* localizer_rv = MMAPI::Internal::global_instance->GetRefMember("__localizer");
			if (!localizer_rv)
				return nullptr;

			return localizer_rv->ToInstance();
		}

		/// Resolves the Localizer's GML calling context. Prefers the value latched from the most recent
		/// get_localizer hook fire; falls back to globalInstance.__localizer so callers running before the
		/// game's first localized-string call still resolve correctly. Cleared on return-to-title; the next
		/// call after that will re-latch from globalInstance.
		/// @return True if a Localizer instance was resolved, false if neither source was available.
		inline bool TryGetLocalizerContext(YYTK::CInstance*& Self, YYTK::CInstance*& Other)
		{
			if (!localizer_self)
			{
				YYTK::CInstance* localizer = TryGetLocalizerFromGlobal();
				if (!localizer)
					return false;

				localizer_self  = localizer;
				localizer_other = localizer;
			}

			Self  = localizer_self;
			Other = localizer_other;
			return true;
		}

		inline YYTK::RValue& GmlScriptGetLocalizerCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			// Refresh on every fire.
			localizer_self  = Self;
			localizer_other = Other;

			if (before_localized_string_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::Text::LocalizedStringContext context{ Arguments[0]->ToString() };
				before_localized_string_callback(context);
				*Arguments[0] = YYTK::RValue(context.m_key);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(
					MMAPI::Internal::self_module,
					GML_SCRIPT_GET_LOCALIZER
				)
			);

			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_localized_string_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::Text::AfterLocalizedStringContext context{
					Arguments[0]->ToString(),
					Result.m_Kind == YYTK::VALUE_STRING ? Result.ToString() : ""
				};
				after_localized_string_callback(context);
				Result = YYTK::RValue(context.m_resolved);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptPlayConversationCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_play_conversation_callback && Arguments && ArgumentCount >= 2 && Arguments[1])
			{
				MMAPI::Text::PlayConversationContext context{ Arguments[1]->ToString() };
				before_play_conversation_callback(context);

				if (context.m_cancelled)
					return Result;

				*Arguments[1] = YYTK::RValue(context.m_key);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(
					MMAPI::Internal::self_module,
					GML_SCRIPT_PLAY_CONVERSATION
				)
			);

			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		// Live TextboxMenu Self/Other, latched from the play_text hook (fires whenever a textbox displays).
		// Used by TryGetTextboxMenuContext for callers outside any hook frame (e.g. CloseTextbox from arbitrary code paths).
		inline YYTK::CInstance* textbox_menu_self  = nullptr;
		inline YYTK::CInstance* textbox_menu_other = nullptr;

		// Cleared from the setup_main_screen pub/sub when the player returns to the title menu.
		// Registered by Text::Enable().
		inline void ClearTextboxMenuOnReturnToTitle(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			textbox_menu_self  = nullptr;
			textbox_menu_other = nullptr;
		}

		/// Resolves the TextboxMenu's GML calling context, latched from the most recent play_text call.
		/// Cleared automatically when the game returns to the title menu.
		/// @return True if a play_text call has been observed this session, false otherwise.
		inline bool TryGetTextboxMenuContext(YYTK::CInstance*& Self, YYTK::CInstance*& Other)
		{
			if (!textbox_menu_self)
				return false;
			Self  = textbox_menu_self;
			Other = textbox_menu_other;
			return true;
		}

		inline YYTK::RValue& GmlScriptPlayTextCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			// Refresh on every fire — Self is the live TextboxMenu instance.
			textbox_menu_self  = Self;
			textbox_menu_other = Other;

			if (before_play_text_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::Text::PlayTextContext context{ Arguments[0]->ToString() };
				before_play_text_callback(context);

				if (context.m_cancelled)
					return Result;

				*Arguments[0] = YYTK::RValue(context.m_key);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_PLAY_TEXT)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}
	}

	/// Activates Text utility functions. Installs the get_localizer and play_text hooks so the Localizer and
	/// TextboxMenu Self/Other pairs are latched for TryGetLocalizerContext / TryGetTextboxMenuContext (both
	/// cleared on return-to-title via the setup_main_screen pub/sub). Cascades to MMAPI::Instance::Enable so
	/// PlayConversation can resolve Ari's calling context.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Text::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Text, MMAPI::Instance);

		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ClearLocalizerOnReturnToTitle);
		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ClearTextboxMenuOnReturnToTitle);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ MMAPI::Internal::GML_SCRIPT_SETUP_MAIN_SCREEN, reinterpret_cast<PVOID>(MMAPI::Internal::GmlScriptBeforeSetupMainScreenCallback) },
			{ Internal::GML_SCRIPT_GET_LOCALIZER,            reinterpret_cast<PVOID>(Internal::GmlScriptGetLocalizerCallback) },
			{ Internal::GML_SCRIPT_PLAY_CONVERSATION,        reinterpret_cast<PVOID>(Internal::GmlScriptPlayConversationCallback) },
			{ Internal::GML_SCRIPT_PLAY_TEXT,                reinterpret_cast<PVOID>(Internal::GmlScriptPlayTextCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Gets a localized string by localization key.
	/// @attention Requires MMAPI::Text::Enable() to have been called.
	/// @param localization_key The localization key to resolve.
	/// @return The localized string as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue GetLocalizedString(const std::string& localization_key)
	{
		MMAPI_REQUIRE_ENABLED("Text", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetLocalizerContext(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_GET_LOCALIZER, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		YYTK::RValue key = localization_key.c_str();
		YYTK::RValue* args[1] = { &key };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
		return result;
	}

	/// Plays a conversation by localization key.
	/// @attention Requires MMAPI::Text::Enable() to have been called.
	/// @param conversation_key The conversation localization key to play.
	/// @return True if the conversation was played, false if the required context is unavailable.
	inline bool PlayConversation(const std::string& conversation_key)
	{
		MMAPI_REQUIRE_ENABLED("Text", false);

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return false;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_PLAY_CONVERSATION, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue mode = 0;
		YYTK::RValue conversation = conversation_key.c_str();
		YYTK::RValue undefined;
		YYTK::RValue result;
		YYTK::RValue* args[4] = { &mode, &conversation, &undefined, &undefined };
		// The play_conversation script ignores Other; pass Self for both to match the in-game calling convention.
		gml_script->m_Functions->m_ScriptFunction(Self, Self, result, 4, args);
		return true;
	}

	/// Begins closing the current textbox.
	/// @attention Requires MMAPI::Text::Enable() to have been called.
	/// @return True if the close was issued, false if the required context is unavailable.
	inline bool CloseTextbox()
	{
		MMAPI_REQUIRE_ENABLED("Text", false);

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetTextboxMenuContext(Self, Other))
			return false;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_CLOSE_TEXTBOX, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		return true;
	}

	namespace Hooks
	{
		/// Registers a callback that can modify a localization key before the game resolves the localized string.
		/// @param callback A function called with a mutable localized string context.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeLocalizedString(Internal::BeforeLocalizedStringCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Text::Hooks::BeforeLocalizedString, MMAPI::Text);

			return MMAPI::Internal::RegisterHook(
				"Text::BeforeLocalizedString",
				Internal::before_localized_string_callback,
				callback
			);
		}

		/// Registers a callback that can modify a resolved localized string after the game's Localizer returns it.
		/// Use ctx.SetResolved() to substitute the string the game receives — useful for placeholder substitution in localized templates.
		/// @param callback A function called with a mutable `MMAPI::Text::AfterLocalizedStringContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterLocalizedString(Internal::AfterLocalizedStringCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Text::Hooks::AfterLocalizedString, MMAPI::Text);

			return MMAPI::Internal::RegisterHook(
				"Text::AfterLocalizedString",
				Internal::after_localized_string_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game plays a conversation.
		/// Use ctx.SetKey() to redirect the conversation, or ctx.Cancel() to prevent it from playing entirely.
		/// @param callback A function called with a mutable conversation context before the game processes it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforePlayConversation(Internal::BeforePlayConversationCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Text::Hooks::BeforePlayConversation, MMAPI::Text);

			return MMAPI::Internal::RegisterHook(
				"Text::BeforePlayConversation",
				Internal::before_play_conversation_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game plays a conversation text node — fires once
		/// per dialogue line as a conversation plays out. Catches every line shape: NPC banked lines,
		/// gift_lines responses, speakerless textboxes (signs/furniture), and cutscene lines.
		/// Use ctx.SetKey() to redirect to a different text node, or ctx.Cancel() to prevent it from playing entirely.
		/// @param callback A function called with a mutable text context before the game processes it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforePlayText(Internal::BeforePlayTextCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Text::Hooks::BeforePlayText, MMAPI::Text);

			return MMAPI::Internal::RegisterHook(
				"Text::BeforePlayText",
				Internal::before_play_text_callback,
				callback
			);
		}
	}
}
