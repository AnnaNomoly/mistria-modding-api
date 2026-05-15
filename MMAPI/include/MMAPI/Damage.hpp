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

namespace MMAPI::Damage
{
	struct BeforeDamageContext
	{
		YYTK::RValue* damage_data = nullptr;

		/// Returns true if the damage packet is available.
		bool IsValid() const { return damage_data != nullptr; }

		/// Gets the raw damage packet passed to the game's damage script.
		YYTK::RValue GetRawDamageData() const
		{
			if (!damage_data)
				return {};

			return *damage_data;
		}

		/// Gets the current damage amount.
		double GetAmount() const
		{
			if (!damage_data || !MMAPI::Engine::StructVariableExists(*damage_data, "damage"))
				return 0.0;

			return damage_data->GetMember("damage").ToDouble();
		}

		/// Sets the damage amount.
		void SetAmount(double amount)
		{
			if (!damage_data)
				return;

			MMAPI::Engine::StructVariableSet(*damage_data, "damage", amount);
		}

		/// Returns true if the damage packet is marked as a critical hit.
		bool IsCritical() const
		{
			if (!damage_data || !MMAPI::Engine::StructVariableExists(*damage_data, "critical"))
				return false;

			return damage_data->GetMember("critical").ToBoolean();
		}

		/// Sets whether the damage packet is marked as a critical hit.
		void SetCritical(bool critical)
		{
			if (!damage_data)
				return;

			MMAPI::Engine::StructVariableSet(*damage_data, "critical", critical);
		}

		/// Returns true if the damage packet applies knockback.
		bool GetKnockback() const
		{
			if (!damage_data || !MMAPI::Engine::StructVariableExists(*damage_data, "knockback"))
				return false;

			return damage_data->GetMember("knockback").ToBoolean();
		}

		/// Sets whether the damage packet applies knockback.
		void SetKnockback(bool knockback)
		{
			if (!damage_data)
				return;

			MMAPI::Engine::StructVariableSet(*damage_data, "knockback", knockback);
		}

		/// Returns true if the damage source is permitted to break "pickable" grid objects
		/// (stones, dig spots, etc.). The game sets this per-tool — pickaxes get true, swords get
		/// false by default. Mods can flip it on for non-pickaxe tools (e.g. a utility sword).
		bool GetCanPickGridObjects() const
		{
			if (!damage_data || !MMAPI::Engine::StructVariableExists(*damage_data, "can_pick_grid_objects"))
				return false;

			return damage_data->GetMember("can_pick_grid_objects").ToBoolean();
		}

		/// Sets whether the damage source is permitted to break "pickable" grid objects.
		void SetCanPickGridObjects(bool can_pick)
		{
			if (!damage_data)
				return;

			MMAPI::Engine::StructVariableSet(*damage_data, "can_pick_grid_objects", can_pick);
		}

		/// Returns true if the damage source is permitted to chop "choppable" grid objects
		/// (trees, bushes, etc.). The game sets this per-tool — axes get true, swords get false
		/// by default. Mods can flip it on for non-axe tools.
		bool GetCanChopGridObjects() const
		{
			if (!damage_data || !MMAPI::Engine::StructVariableExists(*damage_data, "can_chop_grid_objects"))
				return false;

			return damage_data->GetMember("can_chop_grid_objects").ToBoolean();
		}

		/// Sets whether the damage source is permitted to chop "choppable" grid objects.
		void SetCanChopGridObjects(bool can_chop)
		{
			if (!damage_data)
				return;

			MMAPI::Engine::StructVariableSet(*damage_data, "can_chop_grid_objects", can_chop);
		}

		/// Gets the target value from the damage packet.
		YYTK::RValue GetTarget() const
		{
			if (!damage_data || !MMAPI::Engine::StructVariableExists(*damage_data, "target"))
				return {};

			return damage_data->GetMember("target");
		}

		/// Returns true if the damage target appears to be Ari.
		bool IsTargetAri() const
		{
			YYTK::RValue target = GetTarget();
			if (!MMAPI::Engine::IsNumeric(target))
				return false;

			return target.ToInt64() == 1;
		}

		/// Returns true if the damage amount is zero.
		bool IsMiss() const { return GetAmount() == 0.0; }
	};

	struct AfterDamageContext
	{
		YYTK::RValue* damage_data = nullptr;
		bool          m_result    = false;

		/// Returns the boolean result returned by the game's damage script. Typically `true` indicates
		/// the hit landed (was applied to the target); `false` indicates it was rejected.
		bool GetResult() const { return m_result; }

		/// Returns true if the damage packet is available.
		bool IsValid() const { return damage_data != nullptr; }

		/// Gets the damage amount the game's damage script saw (post any BeforeDamage mutations).
		double GetAmount() const
		{
			if (!damage_data || !MMAPI::Engine::StructVariableExists(*damage_data, "damage"))
				return 0.0;
			return damage_data->GetMember("damage").ToDouble();
		}

		/// Returns true if the damage packet was marked as a critical hit.
		bool IsCritical() const
		{
			if (!damage_data || !MMAPI::Engine::StructVariableExists(*damage_data, "critical"))
				return false;
			return damage_data->GetMember("critical").ToBoolean();
		}

		/// Returns true if the damage packet applied knockback.
		bool GetKnockback() const
		{
			if (!damage_data || !MMAPI::Engine::StructVariableExists(*damage_data, "knockback"))
				return false;
			return damage_data->GetMember("knockback").ToBoolean();
		}

		/// Gets the target value from the damage packet.
		YYTK::RValue GetTarget() const
		{
			if (!damage_data || !MMAPI::Engine::StructVariableExists(*damage_data, "target"))
				return {};
			return damage_data->GetMember("target");
		}

		/// Returns true if the damage target was Ari.
		bool IsTargetAri() const
		{
			YYTK::RValue target = GetTarget();
			if (!MMAPI::Engine::IsNumeric(target))
				return false;
			return target.ToInt64() == 1;
		}

		/// Returns true if the damage amount was zero.
		bool IsMiss() const { return GetAmount() == 0.0; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_DAMAGE = "gml_Script_damage@gml_Object_obj_damage_receiver_Create_0";

		using BeforeDamageCallback = void(*)(MMAPI::Damage::BeforeDamageContext&);
		using AfterDamageCallback  = void(*)(MMAPI::Damage::AfterDamageContext&);

		inline BeforeDamageCallback before_damage_callback = nullptr;
		inline AfterDamageCallback  after_damage_callback  = nullptr;

		inline YYTK::RValue& GmlScriptDamageCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_damage_callback && Arguments && ArgumentCount >= 1 && Arguments[0] && Arguments[0]->m_Kind == YYTK::VALUE_OBJECT)
			{
				MMAPI::Damage::BeforeDamageContext context{ Arguments[0] };
				before_damage_callback(context);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(
					MMAPI::Internal::self_module,
					GML_SCRIPT_DAMAGE
				)
			);

			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_damage_callback)
			{
				YYTK::RValue* damage_data =
					(Arguments && ArgumentCount >= 1 && Arguments[0] && Arguments[0]->m_Kind == YYTK::VALUE_OBJECT)
						? Arguments[0]
						: nullptr;
				MMAPI::Damage::AfterDamageContext context{ damage_data, Result.ToBoolean() };
				after_damage_callback(context);
			}

			return Result;
		}
	}

	/// Activates Damage utility functions. Eagerly installs the damage script hook shared by
	/// Hooks::BeforeDamage and Hooks::AfterDamage.
	/// @return Status::Success if the hook is installed (or already was); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Damage::Enable() called");

		MMAPI::Status status = MMAPI::Internal::InstallScriptHook(
			Internal::GML_SCRIPT_DAMAGE,
			reinterpret_cast<PVOID>(Internal::GmlScriptDamageCallback)
		);
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	namespace Hooks
	{
		/// Registers a callback that can modify a damage packet before the game applies it.
		/// @param callback A function called with a mutable damage context.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeDamage(Internal::BeforeDamageCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Damage::Hooks::BeforeDamage, MMAPI::Damage);

			return MMAPI::Internal::RegisterHook(
				"Damage::BeforeDamage",
				Internal::before_damage_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's damage script. The context's `GetResult()`
		/// returns the boolean the script produced — typically `true` if the hit was applied to the
		/// target, `false` otherwise. Use this to gate downstream effects that should fire only when the
		/// damage actually landed (counter-damage, lifesteal commits, etc.).
		/// @param callback A function called with a `MMAPI::Damage::AfterDamageContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterDamage(Internal::AfterDamageCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Damage::Hooks::AfterDamage, MMAPI::Damage);

			return MMAPI::Internal::RegisterHook(
				"Damage::AfterDamage",
				Internal::after_damage_callback,
				callback
			);
		}
	}
}
