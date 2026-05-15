// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Calendar.hpp"
#include "Core.hpp"
#include "Engine.hpp"
#include "Hook.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::StatusEffect
{
	inline constexpr int InfiniteDuration = 2147483647;

	/// Source: globalInstance.__status_effect_id__
	enum class Ids : int
	{
		Restorative     = 0,
		Speedy          = 1,
		Fairy           = 2,
		GuardiansShield = 3,
		MineTime        = 4,
		SlimeDash       = 5,
		ShrineBoon      = 6,
		KillHaste       = 7,
		FlameBreath     = 8,
		StackingSpeed   = 9,
		Venomous        = 10,
		VenomSword      = 11,
		Frozen          = 12,
		IceSword        = 13,
		FireSword       = 14,
		SureStrike      = 15,
		SacredLight     = 16
	};

	/// Total number of enumerators in Ids. Iterating [0, IdCount) covers every Ids value.
	inline constexpr int IdCount = 17;

	/// Invokes fn with every Ids value, in ascending order.
	template <typename Fn>
	inline void ForEachId(Fn fn)
	{
		for (int i = 0; i < IdCount; ++i)
			fn(static_cast<Ids>(i));
	}

	struct RegisterStatusEffectContext
	{
		int    m_status_id        = 0;
		double m_amount           = 0.0;
		int    m_start            = 0;
		int    m_finish           = 0;
		bool   m_cancelled        = false;

		// Tracks whether the user callback explicitly modified the value via Set*. The hook
		// only writes back to the underlying script args when these are true, which preserves
		// the original RValue kind (including VALUE_UNDEFINED) when the callback didn't touch
		// the field. Without this, every register_status_effect call gets its args silently
		// rebuilt as RValue(double)/RValue(int) — turning undefined amounts (used by effects
		// like Fairy that have no numeric magnitude) into 0.0, which the game's register
		// script handles differently and which crashes the hook's own ToDouble read.
		bool   m_amount_modified  = false;
		bool   m_finish_modified  = false;

		MMAPI::StatusEffect::Ids GetStatusEffect() const { return static_cast<MMAPI::StatusEffect::Ids>(m_status_id); }
		double GetAmount() const { return m_amount; }
		void SetAmount(double amount) { m_amount = amount; m_amount_modified = true; }
		int GetStart() const { return m_start; }
		int GetFinish() const { return m_finish; }
		void SetFinish(int finish) { m_finish = finish; m_finish_modified = true; }
		void Cancel() { m_cancelled = true; }
	};

	struct CancelStatusEffectContext
	{
		int  m_status_id = 0;
		bool m_cancelled = false;

		MMAPI::StatusEffect::Ids GetStatusEffect() const { return static_cast<MMAPI::StatusEffect::Ids>(m_status_id); }
		void Cancel() { m_cancelled = true; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_STATUS_EFFECT_MANAGER_UPDATE = "gml_Script_update@StatusEffectManager@StatusEffectManager";
		inline constexpr const char* GML_SCRIPT_CANCEL_STATUS_EFFECT         = "gml_Script_cancel@StatusEffectManager@StatusEffectManager";
		inline constexpr const char* GML_SCRIPT_REGISTER_STATUS_EFFECT       = "gml_Script_register@StatusEffectManager@StatusEffectManager";

		// Live StatusEffectManager Self/Other, latched per-tick from the update hook.
		// Used by TryGetStatusEffectManagerContext for callers outside any hook frame.
		inline YYTK::CInstance* status_effect_manager_self  = nullptr;
		inline YYTK::CInstance* status_effect_manager_other = nullptr;

		// Cleared from the setup_main_screen pub/sub when the player returns to the title menu.
		// Registered by StatusEffect::Enable().
		inline void ClearStatusEffectManagerOnReturnToTitle(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			status_effect_manager_self  = nullptr;
			status_effect_manager_other = nullptr;
		}

		inline YYTK::RValue& StatusEffectManagerUpdateContextCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			// Latch Self/Other from the FIRST manager update tick we observe and keep them frozen
			// thereafter (cleared back to nullptr on return-to-title via ClearStatusEffectManagerOnReturnToTitle).
			//
			// Re-latching every tick (the previous implementation) caused register_status_effect
			// calls routed through this pair to leave inner[id] as VALUE_UNDEFINED — the effect's
			// icon would still show but its lifecycle never set up, so the in-game effect was inert.
			// The register script is sensitive to which `Other` it receives, and a later-tick Other
			// is not accepted the same way the first-tick Other is. Pre-MMAPI DD used the same
			// first-only latch pattern for this script and its registrations worked.
			if (!status_effect_manager_self)
			{
				status_effect_manager_self  = Self;
				status_effect_manager_other = Other;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_STATUS_EFFECT_MANAGER_UPDATE));
			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		/// Resolves the StatusEffectManager's GML calling context, latched from the most recent update tick.
		/// Cleared automatically when the game returns to the title menu.
		/// @return True if a StatusEffectManager tick has been observed this session, false otherwise.
		inline bool TryGetStatusEffectManagerContext(YYTK::CInstance*& Self, YYTK::CInstance*& Other)
		{
			if (!status_effect_manager_self)
				return false;
			Self  = status_effect_manager_self;
			Other = status_effect_manager_other;
			return true;
		}

		inline constexpr double FireSwordAmount   = 1.25;
		inline constexpr double IceSwordAmount    = 1.0;
		inline constexpr double VenomSwordAmount  = 1.0;
		inline constexpr double MineTimeAmount    = 2.0;
		inline constexpr double SacredLightAmount = 0.0;

		inline YYTK::RValue GetDefaultAmount(MMAPI::StatusEffect::Ids status_effect)
		{
			switch (status_effect)
			{
				case MMAPI::StatusEffect::Ids::FireSword:   return FireSwordAmount;
				case MMAPI::StatusEffect::Ids::IceSword:    return IceSwordAmount;
				case MMAPI::StatusEffect::Ids::VenomSword:  return VenomSwordAmount;
				case MMAPI::StatusEffect::Ids::MineTime:    return MineTimeAmount;
				case MMAPI::StatusEffect::Ids::SacredLight: return SacredLightAmount;
				default:                                     return {};
			}
		}

		using BeforeRegisterStatusEffectCallback = void(*)(MMAPI::StatusEffect::RegisterStatusEffectContext&);
		using BeforeCancelStatusEffectCallback   = void(*)(MMAPI::StatusEffect::CancelStatusEffectContext&);

		inline BeforeRegisterStatusEffectCallback before_register_status_effect_callback = nullptr;
		inline BeforeCancelStatusEffectCallback   before_cancel_status_effect_callback   = nullptr;

		inline void RegisterById(int status_effect_id, YYTK::RValue amount, int start, int finish)
		{
			YYTK::CInstance* Self  = nullptr;
			YYTK::CInstance* Other = nullptr;
			if (!TryGetStatusEffectManagerContext(Self, Other))
				return;

			YYTK::CScript* gml_script = nullptr;
			MMAPI::Internal::module_interface->GetNamedRoutinePointer(GML_SCRIPT_REGISTER_STATUS_EFFECT, reinterpret_cast<PVOID*>(&gml_script));

			YYTK::RValue id     = status_effect_id;
			YYTK::RValue st     = start;
			YYTK::RValue fn     = finish;
			YYTK::RValue result;
			YYTK::RValue* args[4] = { &id, &amount, &st, &fn };
			gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 4, args);
		}

		inline void CancelById(int status_effect_id)
		{
			YYTK::CInstance* Self  = nullptr;
			YYTK::CInstance* Other = nullptr;
			if (!TryGetStatusEffectManagerContext(Self, Other))
				return;

			YYTK::CScript* gml_script = nullptr;
			MMAPI::Internal::module_interface->GetNamedRoutinePointer(GML_SCRIPT_CANCEL_STATUS_EFFECT, reinterpret_cast<PVOID*>(&gml_script));

			YYTK::RValue id = status_effect_id;
			YYTK::RValue result;
			YYTK::RValue* args[1] = { &id };
			gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
		}

		inline YYTK::RValue GetActiveEffectById(int status_effect_id)
		{
			YYTK::RValue ari = MMAPI::Internal::global_instance->GetMember("__ari");
			YYTK::RValue status_effects = ari.GetMember("status_effects");
			YYTK::RValue effects = status_effects.GetMember("effects");
			YYTK::RValue inner = effects.GetMember("inner");

			std::string id = std::to_string(status_effect_id);
			if (!MMAPI::Engine::StructVariableExists(inner, id.c_str()))
				return {};

			return inner.GetMember(id);
		}

		inline YYTK::RValue& GmlScriptRegisterStatusEffectCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_register_status_effect_callback && Arguments && ArgumentCount >= 4)
			{
				// Safe-read amount: VALUE_UNDEFINED args (e.g. effects like Fairy that have
				// no numeric magnitude) would crash inside ToDouble's PTR_RValue macro with
				// "REAL argument incorrect type undefined". Default to 0.0 for the context;
				// the writeback below only runs if the user callback explicitly sets it, so
				// the original undefined RValue passes through to the game script untouched.
				const bool amount_was_undefined = (Arguments[1]->m_Kind == YYTK::VALUE_UNDEFINED);
				const double amount_for_context = amount_was_undefined ? 0.0 : Arguments[1]->ToDouble();

				MMAPI::StatusEffect::RegisterStatusEffectContext context{
					static_cast<int>(Arguments[0]->ToInt64()),
					amount_for_context,
					static_cast<int>(Arguments[2]->ToInt64()),
					static_cast<int>(Arguments[3]->ToInt64())
				};
				before_register_status_effect_callback(context);

				if (context.m_cancelled)
					return Result;

				// Conditional writeback — only mutate the original script args if the user
				// callback explicitly called SetAmount/SetFinish. Preserves the original
				// RValue kind (including undefined amounts) when the callback didn't touch
				// the field, instead of silently rebuilding every call's args.
				if (context.m_amount_modified)
					*Arguments[1] = YYTK::RValue(context.m_amount);
				if (context.m_finish_modified)
					*Arguments[3] = YYTK::RValue(context.m_finish);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_REGISTER_STATUS_EFFECT)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

		inline YYTK::RValue& GmlScriptCancelStatusEffectCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_cancel_status_effect_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::StatusEffect::CancelStatusEffectContext context{ static_cast<int>(Arguments[0]->ToInt64()) };
				before_cancel_status_effect_callback(context);

				if (context.m_cancelled)
					return Result;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_CANCEL_STATUS_EFFECT)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}
	}

	/// Activates StatusEffect utility functions that directly call game scripts.
	/// Installs the manager update hook so the live StatusEffectManager Self/Other are latched for
	/// TryGetStatusEffectManagerContext (cleared on return-to-title via the setup_main_screen pub/sub).
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::StatusEffect::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::StatusEffect, MMAPI::Calendar);

		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ClearStatusEffectManagerOnReturnToTitle);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ MMAPI::Internal::GML_SCRIPT_SETUP_MAIN_SCREEN,        reinterpret_cast<PVOID>(MMAPI::Internal::GmlScriptBeforeSetupMainScreenCallback) },
			{ Internal::GML_SCRIPT_STATUS_EFFECT_MANAGER_UPDATE,    reinterpret_cast<PVOID>(Internal::StatusEffectManagerUpdateContextCallback) },
			{ Internal::GML_SCRIPT_REGISTER_STATUS_EFFECT,          reinterpret_cast<PVOID>(Internal::GmlScriptRegisterStatusEffectCallback) },
			{ Internal::GML_SCRIPT_CANCEL_STATUS_EFFECT,            reinterpret_cast<PVOID>(Internal::GmlScriptCancelStatusEffectCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Registers a persistent status effect on Ari.
	/// @attention Requires MMAPI::StatusEffect::Enable() to have been called.
	/// @param status_effect The status effect to register.
	/// @return True if the effect was registered, false if the required context is unavailable.
	inline bool RegisterPersistent(MMAPI::StatusEffect::Ids status_effect)
	{
		MMAPI_REQUIRE_ENABLED("StatusEffect", false);

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetStatusEffectManagerContext(Self, Other))
			return false;

		Internal::RegisterById(static_cast<int>(status_effect), Internal::GetDefaultAmount(status_effect), 1, MMAPI::StatusEffect::InfiniteDuration);
		return true;
	}

	/// Registers a status effect on Ari for the given duration.
	/// @attention Requires MMAPI::StatusEffect::Enable() to have been called.
	/// @param status_effect The status effect to register.
	/// @param duration The duration of the status effect in unified game-time seconds.
	/// @return True if the effect was registered, false if the required context is unavailable.
	inline bool RegisterForDuration(MMAPI::StatusEffect::Ids status_effect, int duration)
	{
		MMAPI_REQUIRE_ENABLED("StatusEffect", false);

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetStatusEffectManagerContext(Self, Other))
			return false;

		YYTK::RValue unified_time = MMAPI::Calendar::GetUnifiedTime();
		if (unified_time.m_Kind == YYTK::VALUE_UNDEFINED)
			return false;

		int start = static_cast<int>(unified_time.ToInt64());
		Internal::RegisterById(static_cast<int>(status_effect), Internal::GetDefaultAmount(status_effect), start, start + duration);
		return true;
	}

	/// Cancels an active status effect on Ari.
	/// @attention Requires MMAPI::StatusEffect::Enable() to have been called.
	/// @param status_effect The status effect to cancel.
	inline void Cancel(MMAPI::StatusEffect::Ids status_effect)
	{
		MMAPI_REQUIRE_ENABLED_VOID("StatusEffect");
		Internal::CancelById(static_cast<int>(status_effect));
	}

	/// Cancels all known status effects on Ari.
	/// @attention Requires MMAPI::StatusEffect::Enable() to have been called.
	inline void CancelAll()
	{
		MMAPI_REQUIRE_ENABLED_VOID("StatusEffect");
		for (int i = 0; i <= static_cast<int>(MMAPI::StatusEffect::Ids::SacredLight); i++)
			Internal::CancelById(i);
	}

	/// Gets Ari's active status effect data for the given status effect.
	/// @param status_effect The status effect to retrieve.
	/// @return The active status effect struct, or undefined if it is not active.
	inline YYTK::RValue GetActive(MMAPI::StatusEffect::Ids status_effect)
	{
		return Internal::GetActiveEffectById(static_cast<int>(status_effect));
	}

	/// Returns true if Ari currently has the given status effect active.
	/// @param status_effect The status effect to check.
	inline bool IsActive(MMAPI::StatusEffect::Ids status_effect)
	{
		return GetActive(status_effect).m_Kind == YYTK::VALUE_OBJECT;
	}

	namespace Hooks
	{
		/// Registers a callback that runs before the game registers a status effect on Ari.
		/// Use ctx.SetAmount() to modify the effect's magnitude, ctx.SetFinish() to adjust its end time, or ctx.Cancel() to prevent registration entirely.
		/// @param callback A function called with a mutable register context before the game processes it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeRegisterStatusEffect(Internal::BeforeRegisterStatusEffectCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::StatusEffect::Hooks::BeforeRegisterStatusEffect, MMAPI::StatusEffect);

			return MMAPI::Internal::RegisterHook(
				"StatusEffect::BeforeRegisterStatusEffect",
				Internal::before_register_status_effect_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game cancels a status effect on Ari.
		/// Call ctx.Cancel() to prevent the status effect from being removed.
		/// @param callback A function called with a mutable cancel context before the game processes it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeCancelStatusEffect(Internal::BeforeCancelStatusEffectCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::StatusEffect::Hooks::BeforeCancelStatusEffect, MMAPI::StatusEffect);

			return MMAPI::Internal::RegisterHook(
				"StatusEffect::BeforeCancelStatusEffect",
				Internal::before_cancel_status_effect_callback,
				callback
			);
		}
	}
}
