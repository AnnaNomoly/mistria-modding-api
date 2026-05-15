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

namespace MMAPI::Anchor
{
	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_ON_BEGIN_STEP = "gml_Script_on_begin_step@Anchor@Anchor";

		using BeforeBeginStepCallback = void(*)();
		inline BeforeBeginStepCallback before_begin_step_callback = nullptr;

		inline YYTK::RValue& GmlScriptBeforeBeginStepCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_begin_step_callback)
				before_begin_step_callback();

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(
					MMAPI::Internal::self_module,
					GML_SCRIPT_ON_BEGIN_STEP
				)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}
	}

	/// Activates Anchor hooks. Installs the Anchor on_begin_step hook so registered callbacks
	/// run before the game's per-frame physics tick. Safe to call before any Hooks::* registration —
	/// the callback no-ops until a user callback is bound.
	/// @return Status::Success if the hook is installed (or already was); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Anchor::Enable() called");

		MMAPI::Status status = MMAPI::Internal::InstallScriptHook(
			Internal::GML_SCRIPT_ON_BEGIN_STEP,
			reinterpret_cast<PVOID>(Internal::GmlScriptBeforeBeginStepCallback)
		);
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	namespace Hooks
	{
		/// Registers a callback that runs before each Anchor `on_begin_step` — the game's per-frame
		/// world physics tick. Fires roughly 60 times per second.
		/// The callback receives no context; use it for per-frame state updates (e.g. applying sprite
		/// overrides to tracked instances).
		/// @param callback A parameterless function called every frame before the game's begin-step.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeBeginStep(Internal::BeforeBeginStepCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Anchor::Hooks::BeforeBeginStep, MMAPI::Anchor);

			return MMAPI::Internal::RegisterHook(
				"Anchor::BeforeBeginStep",
				Internal::before_begin_step_callback,
				callback
			);
		}
	}
}
