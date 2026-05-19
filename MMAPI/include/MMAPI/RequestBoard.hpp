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
#include "Quest.hpp"
#include "Status.hpp"

#include <string>
#include <vector>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::RequestBoard
{
	struct BeforeAcceptRequestContext
	{
		std::string m_request_id;

		/// The request ID the player is about to accept (e.g. "request_for_haybale").
		std::string_view GetRequestId() const { return m_request_id; }
	};

	struct AfterAcceptRequestContext
	{
		std::string m_request_id;

		/// The request ID the player just accepted.
		std::string_view GetRequestId() const { return m_request_id; }
	};

	/// Returns true if `request_id` is defined in `globalInstance.__request_board_entries`.
	/// Used by the accept hook to filter QuestLog start events down to request-board-sourced ones.
	inline bool HasEntry(const std::string& request_id)
	{
		YYTK::RValue entries = MMAPI::Internal::global_instance->GetMember("__request_board_entries");
		if (entries.m_Kind != YYTK::VALUE_OBJECT)
			return false;

		YYTK::RValue inner = entries.GetMember("inner");
		if (inner.m_Kind != YYTK::VALUE_OBJECT)
			return false;

		auto members = inner.ToMap();
		return members.find(request_id) != members.end();
	}

	namespace Internal
	{
		inline bool enabled = false;

		// Why this module doesn't install its own hook:
		// 1. `accept_from_request_board@StoryExecutor` looks obvious but never fires — it's a
		//    queued scene-action dispatched through StoryExecutor's execute loop in a way that
		//    bypasses named-script hooks.
		// 2. `start@QuestLog@QuestLog` is the entry point for ALL quest acceptances, but only one
		//    MMAPI module can install a hook on a given script (idempotent install). The Quest
		//    module owns that hook now and exposes a pub/sub via
		//    `MMAPI::Quest::Internal::RegisterOnQuestStartHandler`. RequestBoard registers a
		//    handler there that filters by `HasEntry(quest_id)` so this module's hook stays
		//    request-board-specific.
		//
		// Caveat retained: if a request-board-defined quest is also triggered from dialogue, the
		// handler still fires — true UI-acceptance gating would need a different mechanism.

		using BeforeAcceptRequestCallback = void(*)(MMAPI::RequestBoard::BeforeAcceptRequestContext&);
		using AfterAcceptRequestCallback  = void(*)(MMAPI::RequestBoard::AfterAcceptRequestContext&);

		inline BeforeAcceptRequestCallback before_accept_request_callback = nullptr;
		inline AfterAcceptRequestCallback  after_accept_request_callback  = nullptr;

		/// Internal handler registered with Quest's pub/sub. Filters quest starts down to
		/// request-board-defined quests, then fires this module's Before/After callbacks.
		///
		/// Save-load suppression is handled by Quest's own hook (which guards via
		/// `Game::IsLoadingSave()`), so we don't need to re-check it here.
		inline void OnQuestStartHandler(const std::string& quest_name)
		{
			if (!MMAPI::RequestBoard::HasEntry(quest_name))
				return;

			if (before_accept_request_callback)
			{
				MMAPI::RequestBoard::BeforeAcceptRequestContext context{ quest_name };
				before_accept_request_callback(context);
			}

			// Note: Quest's hook invokes handlers AFTER the trampoline runs, so we're effectively
			// firing the "After" handler at this point too. Before/After distinction is preserved
			// in the API for forward-compatibility but currently both fire at the same point.
			if (after_accept_request_callback)
			{
				MMAPI::RequestBoard::AfterAcceptRequestContext context{ quest_name };
				after_accept_request_callback(context);
			}
		}
	}

	/// Activates RequestBoard utility functions. Registers an internal handler with the Quest
	/// module's quest-start pub/sub; the handler filters by `HasEntry(quest_id)` so Before/After
	/// callbacks only fire for request-board-defined quests. The Quest module owns the actual
	/// `start@QuestLog@QuestLog` hook (and its save-load suppression).
	/// @return Status::Success if the dependency chain was enabled; otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::RequestBoard::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::RequestBoard, MMAPI::Quest);

		MMAPI::Quest::Internal::RegisterOnQuestStartHandler(Internal::OnQuestStartHandler);

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Returns the list of request IDs currently posted to the request board. The board is a
	/// circular buffer of fixed capacity; this trims to `__count` so stale entries past the active
	/// region aren't returned.
	///
	/// Does NOT require Enable() — reads `globalInstance.__request_board` directly.
	///
	/// @return The active request IDs in board order, or an empty list if the board hasn't been
	///         populated yet (pre-title-screen or save not yet loaded).
	inline std::vector<std::string> GetActiveRequestIds()
	{
		std::vector<std::string> ids;

		YYTK::RValue board = MMAPI::Internal::global_instance->GetMember("__request_board");
		if (board.m_Kind != YYTK::VALUE_OBJECT)
			return ids;

		YYTK::RValue buffer = board.GetMember("__buffer");
		if (buffer.m_Kind != YYTK::VALUE_ARRAY)
			return ids;

		YYTK::RValue count_rv = board.GetMember("__count");
		int count = static_cast<int>(count_rv.ToDouble());
		if (count <= 0)
			return ids;

		size_t buffer_size = 0;
		MMAPI::Internal::module_interface->GetArraySize(buffer, buffer_size);
		int usable = (count < static_cast<int>(buffer_size)) ? count : static_cast<int>(buffer_size);

		ids.reserve(static_cast<size_t>(usable));
		for (int i = 0; i < usable; i++)
		{
			YYTK::RValue* entry = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(buffer, i, entry);
			if (entry && entry->m_Kind == YYTK::VALUE_STRING)
				ids.emplace_back(entry->ToString());
		}
		return ids;
	}

	/// Returns true if the request board currently has any active requests posted.
	/// Convenience over GetActiveRequestIds().
	inline bool HasActiveRequests()
	{
		YYTK::RValue board = MMAPI::Internal::global_instance->GetMember("__request_board");
		if (board.m_Kind != YYTK::VALUE_OBJECT)
			return false;

		YYTK::RValue count_rv = board.GetMember("__count");
		return count_rv.ToDouble() > 0.0;
	}

	/// Returns true if the player has read the named request board entry. Reads
	/// `globalInstance.__request_board_read_entries.inner[request_id]` and interprets any value
	/// >= 1.0 as read. Returns false if the request ID isn't tracked or the player hasn't viewed it.
	///
	/// @param request_id The request ID (e.g. "request_for_haybale", "cop_some_ore").
	/// @return True if the request has been marked read by the game.
	inline bool IsRequestRead(const std::string& request_id)
	{
		YYTK::RValue read_entries = MMAPI::Internal::global_instance->GetMember("__request_board_read_entries");
		if (read_entries.m_Kind != YYTK::VALUE_OBJECT)
			return false;

		YYTK::RValue inner = read_entries.GetMember("inner");
		if (inner.m_Kind != YYTK::VALUE_OBJECT)
			return false;

		auto members = inner.ToMap();
		auto it = members.find(request_id);
		if (it == members.end())
			return false;
		return it->second.ToDouble() >= 1.0;
	}

	namespace Hooks
	{
		/// Registers a callback that runs before the game processes acceptance of a request from
		/// the board. The context exposes the request ID being accepted. Read-only for v1 — the
		/// callback can't cancel or redirect the acceptance.
		/// @param callback A function called with the BeforeAcceptRequest context before the original script runs.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeAcceptRequest(Internal::BeforeAcceptRequestCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::RequestBoard::Hooks::BeforeAcceptRequest, MMAPI::RequestBoard);

			return MMAPI::Internal::RegisterHook(
				"RequestBoard::BeforeAcceptRequest",
				Internal::before_accept_request_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game has processed acceptance of a request
		/// from the board (quest registered, board buffer updated). The context exposes the
		/// accepted request ID.
		/// @param callback A function called with the AfterAcceptRequest context after the original script runs.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterAcceptRequest(Internal::AfterAcceptRequestCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::RequestBoard::Hooks::AfterAcceptRequest, MMAPI::RequestBoard);

			return MMAPI::Internal::RegisterHook(
				"RequestBoard::AfterAcceptRequest",
				Internal::after_accept_request_callback,
				callback
			);
		}
	}
}
