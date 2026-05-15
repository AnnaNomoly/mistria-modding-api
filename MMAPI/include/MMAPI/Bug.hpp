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

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Bug
{
	struct BugSpawnContext
	{
		YYTK::CInstance* m_bug     = nullptr;
		int              m_item_id = -1;

		YYTK::CInstance* GetBug() const { return m_bug; }
		int GetItemId() const { return m_item_id; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_SPAWN_BUG  = "gml_Script_spawn_bug";
		inline constexpr const char* GML_SCRIPT_CREATE_BUG = "gml_Script_setup@gml_Object_obj_bug_Create_0";

		inline bool override_next_bug_item_id = false;
		inline int  next_bug_item_id          = -1;

		using AfterBugSpawnCallback = void(*)(MMAPI::Bug::BugSpawnContext&);
		inline AfterBugSpawnCallback after_bug_spawn_callback = nullptr;

		inline void ClearPendingSpawn()
		{
			override_next_bug_item_id = false;
			next_bug_item_id          = -1;
		}

		inline YYTK::RValue& GmlScriptCreateBugCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (override_next_bug_item_id && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				Arguments[0]->m_i64 = next_bug_item_id;
				ClearPendingSpawn();
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_CREATE_BUG)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_bug_spawn_callback)
			{
				int item_id = (Arguments && ArgumentCount >= 1 && Arguments[0])
					? static_cast<int>(Arguments[0]->ToInt64())
					: -1;
				MMAPI::Bug::BugSpawnContext context{ Self, item_id };
				after_bug_spawn_callback(context);
			}

			return Result;
		}
	}

	/// Activates Bug utility functions. Cascades to MMAPI::Instance::Enable so Spawn / SpawnCustom can
	/// resolve Ari's calling context internally.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Bug::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Bug, MMAPI::Instance);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHook(
			Internal::GML_SCRIPT_CREATE_BUG,
			reinterpret_cast<PVOID>(Internal::GmlScriptCreateBugCallback)
		);
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Spawns a bug at the given room coordinates.
	/// @attention Requires MMAPI::Bug::Enable() to have been called.
	/// @param x_coord The X coordinate at which to spawn the bug.
	/// @param y_coord The Y coordinate at which to spawn the bug.
	/// @return The spawned bug as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue Spawn(int x_coord, int y_coord)
	{
		MMAPI_REQUIRE_ENABLED("Bug", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_SPAWN_BUG, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		YYTK::RValue x = x_coord;
		YYTK::RValue y = y_coord;
		YYTK::RValue undefined;
		YYTK::RValue* args[3] = { &x, &y, &undefined };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 3, args);
		return result;
	}

	/// Spawns a bug at the given room coordinates and overrides its item ID during the bug's setup script.
	/// @attention Requires MMAPI::Bug::Enable() to have been called.
	/// @param x_coord The X coordinate at which to spawn the bug.
	/// @param y_coord The Y coordinate at which to spawn the bug.
	/// @param bug_item_id The item ID to apply to the bug during its setup call.
	/// @return The spawned bug as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue SpawnCustom(int x_coord, int y_coord, int bug_item_id)
	{
		Internal::override_next_bug_item_id = true;
		Internal::next_bug_item_id          = bug_item_id;

		YYTK::RValue result = Spawn(x_coord, y_coord);
		if (result.m_Kind == YYTK::VALUE_UNDEFINED)
			Internal::ClearPendingSpawn();

		return result;
	}

	namespace Hooks
	{
		/// Registers a callback that runs after a bug's setup script finishes (i.e. the bug has just spawned).
		/// The callback receives the live bug instance and its item ID, and can read or modify the bug's properties (e.g. position) before the next tick.
		/// @param callback A function called with a `MMAPI::Bug::BugSpawnContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterBugSpawn(Internal::AfterBugSpawnCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Bug::Hooks::AfterBugSpawn, MMAPI::Bug);

			return MMAPI::Internal::RegisterHook(
				"Bug::AfterBugSpawn",
				Internal::after_bug_spawn_callback,
				callback
			);
		}
	}
}
