// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Engine.hpp"
#include "Game.hpp"
#include "Hook.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include <string>
#include <vector>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Quest
{
	/// Source: globalInstance.__quest_state__
	enum class State : int
	{
		Inactive  = 0,
		Active    = 1,
		Completed = 2
	};

	struct BeforeQuestStartContext
	{
		std::string m_quest_name;

		std::string_view GetQuestName() const { return m_quest_name; }
	};

	struct AfterQuestCompleteContext
	{
		std::string m_quest_name;

		std::string_view GetQuestName() const { return m_quest_name; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_QUEST_LOG_START    = "gml_Script_start@QuestLog@QuestLog";
		inline constexpr const char* GML_SCRIPT_QUEST_LOG_COMPLETE = "gml_Script_complete@QuestLog@QuestLog";

		using BeforeQuestStartCallback    = void(*)(MMAPI::Quest::BeforeQuestStartContext&);
		using AfterQuestCompleteCallback  = void(*)(MMAPI::Quest::AfterQuestCompleteContext&);

		inline BeforeQuestStartCallback    before_quest_start_callback    = nullptr;
		inline AfterQuestCompleteCallback  after_quest_complete_callback  = nullptr;

		// Internal pub/sub for quest-start events. Modules that need to react to quest starts
		// (RequestBoard for its accept hook, others in the future) register a handler here. The
		// hook on `start@QuestLog@QuestLog` fans out to every registered handler. This avoids the
		// "only one module can install a hook on a given script" constraint that MMAPI's
		// idempotent InstallScriptHook enforces.
		using OnQuestStartHandler = void(*)(const std::string& quest_name);
		inline std::vector<OnQuestStartHandler> on_quest_start_internal_handlers;

		inline void RegisterOnQuestStartHandler(OnQuestStartHandler handler)
		{
			for (auto existing : on_quest_start_internal_handlers)
				if (existing == handler)
					return;
			on_quest_start_internal_handlers.push_back(handler);
		}

		using OnQuestCompleteHandler = void(*)(const std::string& quest_name);
		inline std::vector<OnQuestCompleteHandler> on_quest_complete_internal_handlers;

		inline void RegisterOnQuestCompleteHandler(OnQuestCompleteHandler handler)
		{
			for (auto existing : on_quest_complete_internal_handlers)
				if (existing == handler)
					return;
			on_quest_complete_internal_handlers.push_back(handler);
		}

		/// Defensive extractor for the quest name from arg[0]. Empirically arg[0] is a string for
		/// both start@QuestLog and complete@QuestLog (e.g. "request_for_seaweed"). If a future
		/// caller passes a struct, probe its `quest_name` / `id` / `name` members as a fallback.
		inline std::string ExtractQuestName(YYTK::RValue* arg)
		{
			if (!arg)
				return {};

			if (arg->m_Kind == YYTK::VALUE_STRING)
				return arg->ToString();

			if (arg->m_Kind == YYTK::VALUE_OBJECT)
			{
				auto members = arg->ToMap();
				for (const char* key : { "quest_name", "id", "name" })
				{
					auto it = members.find(key);
					if (it != members.end() && it->second.m_Kind == YYTK::VALUE_STRING)
						return it->second.ToString();
				}
			}

			return {};
		}

		inline YYTK::RValue& GmlScriptQuestLogStartCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			std::string quest_name = (Arguments && ArgumentCount >= 1) ? ExtractQuestName(Arguments[0]) : std::string{};

			// Suppress callbacks/handlers during save-load: load_game re-registers every active
			// quest by calling QuestLog.start on each, which would fire BeforeQuestStart and the
			// internal handlers for every already-accepted quest. Mods using these hooks to grant
			// bonuses or trigger one-shot effects would re-trigger them on every load.
			bool fire = !MMAPI::Game::IsLoadingSave() && !quest_name.empty();

			if (fire && before_quest_start_callback)
			{
				MMAPI::Quest::BeforeQuestStartContext ctx{ quest_name };
				before_quest_start_callback(ctx);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_QUEST_LOG_START)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (fire)
			{
				for (auto handler : on_quest_start_internal_handlers)
					handler(quest_name);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptQuestLogCompleteCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			std::string quest_name = (Arguments && ArgumentCount >= 1) ? ExtractQuestName(Arguments[0]) : std::string{};

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_QUEST_LOG_COMPLETE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			// Save-load suppression isn't needed here — load_game doesn't re-fire completions.
			if (!quest_name.empty())
			{
				if (after_quest_complete_callback)
				{
					MMAPI::Quest::AfterQuestCompleteContext ctx{ quest_name };
					after_quest_complete_callback(ctx);
				}
				for (auto handler : on_quest_complete_internal_handlers)
					handler(quest_name);
			}

			return Result;
		}

		/// Returns the `inner` map of `globalInstance.__quest_log.active`, or undefined if the log
		/// isn't yet populated.
		inline YYTK::RValue GetActiveQuestMap()
		{
			YYTK::RValue quest_log = MMAPI::Internal::global_instance->GetMember("__quest_log");
			if (quest_log.m_Kind != YYTK::VALUE_OBJECT)
				return {};

			YYTK::RValue active = quest_log.GetMember("active");
			if (active.m_Kind != YYTK::VALUE_OBJECT)
				return {};

			YYTK::RValue inner = active.GetMember("inner");
			return inner;
		}
	}

	/// Activates Quest utility functions and the QuestLog hooks. Installs hooks on
	/// `start@QuestLog@QuestLog` and `complete@QuestLog@QuestLog` so Before/After callbacks fire.
	/// Other MMAPI modules (e.g. RequestBoard) register internal handlers via
	/// `Internal::RegisterOnQuestStartHandler` to react to the same events without each having to
	/// install their own hook on the same script.
	///
	/// Depends on Game::Enable() for the `IsLoadingSave()` predicate used to suppress callbacks
	/// during save-load (where every active quest gets re-registered via QuestLog.start).
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Quest::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Quest, MMAPI::Game);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_QUEST_LOG_START,    reinterpret_cast<PVOID>(Internal::GmlScriptQuestLogStartCallback) },
			{ Internal::GML_SCRIPT_QUEST_LOG_COMPLETE, reinterpret_cast<PVOID>(Internal::GmlScriptQuestLogCompleteCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Returns true if the named quest is currently in the active quest log.
	/// Reads `globalInstance.__quest_log.active.inner[quest_name]` membership directly — no
	/// script invocation needed, so this works without Enable().
	inline bool IsActive(const std::string& quest_name)
	{
		YYTK::RValue inner = Internal::GetActiveQuestMap();
		if (inner.m_Kind != YYTK::VALUE_OBJECT)
			return false;

		auto members = inner.ToMap();
		return members.find(quest_name) != members.end();
	}

	/// Returns the list of currently-active quest names.
	inline std::vector<std::string> GetActiveQuests()
	{
		std::vector<std::string> names;

		YYTK::RValue inner = Internal::GetActiveQuestMap();
		if (inner.m_Kind != YYTK::VALUE_OBJECT)
			return names;

		auto members = inner.ToMap();
		names.reserve(members.size());
		for (const auto& [key, value] : members)
		{
			// `entry` and `instance_ptr` are map-internal members — skip them.
			if (key == "entry" || key == "instance_ptr" || key == "toString")
				continue;
			if (value.m_Kind == YYTK::VALUE_OBJECT)
				names.push_back(key);
		}
		return names;
	}

	/// Returns the current stage of an active quest (0-indexed). Returns -1 if the quest isn't
	/// active or the stage can't be read.
	inline int GetCurrentStage(const std::string& quest_name)
	{
		YYTK::RValue inner = Internal::GetActiveQuestMap();
		if (inner.m_Kind != YYTK::VALUE_OBJECT)
			return -1;

		auto members = inner.ToMap();
		auto it = members.find(quest_name);
		if (it == members.end() || it->second.m_Kind != YYTK::VALUE_OBJECT)
			return -1;

		YYTK::RValue stage = it->second.GetMember("current_stage");
		if (!MMAPI::Engine::IsNumeric(stage))
			return -1;
		return static_cast<int>(stage.ToInt64());
	}

	namespace Hooks
	{
		/// Registers a callback that runs before a quest is started via `start@QuestLog@QuestLog`.
		/// Suppressed during save-load (when every active quest gets re-registered) so the hook
		/// only fires for fresh quest acceptances.
		inline MMAPI::Status BeforeQuestStart(Internal::BeforeQuestStartCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Quest::Hooks::BeforeQuestStart, MMAPI::Quest);

			return MMAPI::Internal::RegisterHook(
				"Quest::BeforeQuestStart",
				Internal::before_quest_start_callback,
				callback
			);
		}

		/// Registers a callback that runs after a quest is completed via
		/// `complete@QuestLog@QuestLog`.
		inline MMAPI::Status AfterQuestComplete(Internal::AfterQuestCompleteCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Quest::Hooks::AfterQuestComplete, MMAPI::Quest);

			return MMAPI::Internal::RegisterHook(
				"Quest::AfterQuestComplete",
				Internal::after_quest_complete_callback,
				callback
			);
		}
	}
}
