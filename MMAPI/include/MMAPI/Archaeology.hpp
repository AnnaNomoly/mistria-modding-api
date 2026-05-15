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

namespace MMAPI::Archaeology
{
	struct AfterChooseRandomArtifactContext
	{
		int  m_item_id   = -1;
		bool m_overridden = false;

		/// Returns the item_id of the artifact the game's choose_random_artifact script rolled.
		int GetItemId() const { return m_item_id; }

		/// Overrides the item the dig spot will yield. Use to swap the rolled artifact with a custom
		/// item (e.g. a seasonal collectible) — set the new item_id with `MMAPI::Item::GetIdFromInternalName(...)`
		/// and pass it here.
		void SetItemId(int item_id)
		{
			m_item_id    = item_id;
			m_overridden = true;
		}
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_CHOOSE_RANDOM_ARTIFACT = "gml_Script_choose_random_artifact@Archaeology@Archaeology";

		using AfterChooseRandomArtifactCallback = void(*)(MMAPI::Archaeology::AfterChooseRandomArtifactContext&);

		inline AfterChooseRandomArtifactCallback after_choose_random_artifact_callback = nullptr;

		inline YYTK::RValue& GmlScriptAfterChooseRandomArtifactCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_CHOOSE_RANDOM_ARTIFACT)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_choose_random_artifact_callback && MMAPI::Engine::IsNumeric(Result))
			{
				MMAPI::Archaeology::AfterChooseRandomArtifactContext context{ static_cast<int>(Result.ToInt64()) };
				after_choose_random_artifact_callback(context);
				if (context.m_overridden)
					Result = context.m_item_id;
			}

			return Result;
		}
	}

	/// Activates Archaeology utility functions. Installs the `choose_random_artifact@Archaeology@Archaeology`
	/// script hook so registered callbacks can override the artifact a dig spot yields.
	/// @return Status::Success if the hook is installed (or already was); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Archaeology::Enable() called");

		MMAPI::Status status = MMAPI::Internal::InstallScriptHook(
			Internal::GML_SCRIPT_CHOOSE_RANDOM_ARTIFACT,
			reinterpret_cast<PVOID>(Internal::GmlScriptAfterChooseRandomArtifactCallback)
		);
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	namespace Hooks
	{
		/// Registers a callback that runs after the game's `choose_random_artifact@Archaeology@Archaeology`
		/// script — the artifact roll that fires when Ari breaks open a dig spot. Read `ctx.GetItemId()`
		/// for the rolled artifact and call `ctx.SetItemId(...)` to substitute a different item
		/// (e.g. a seasonal collectible).
		/// @param callback A function called with a mutable `MMAPI::Archaeology::AfterChooseRandomArtifactContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterChooseRandomArtifact(Internal::AfterChooseRandomArtifactCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Archaeology::Hooks::AfterChooseRandomArtifact, MMAPI::Archaeology);

			return MMAPI::Internal::RegisterHook(
				"Archaeology::AfterChooseRandomArtifact",
				Internal::after_choose_random_artifact_callback,
				callback
			);
		}
	}
}
