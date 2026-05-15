// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Engine.hpp"
#include "Hook.hpp"
#include "Infusion.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include <map>
#include <string>
#include <vector>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Equipment
{
	struct EquipmentBonusContext
	{
		int    m_infusion_id = -1;
		double m_bonus_value = 0.0;

		/// Returns the infusion the game is computing the equipment bonus for.
		MMAPI::Infusion::Ids GetInfusion() const { return static_cast<MMAPI::Infusion::Ids>(m_infusion_id); }

		/// Returns the bonus value the game's get_equipment_bonus_from script produced
		/// (summed across every equipped piece carrying this infusion).
		double GetBonusValue() const { return m_bonus_value; }

		/// Overrides the bonus value the game will see — set effects, set bonuses, etc.
		void SetBonusValue(double value) { m_bonus_value = value; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_GET_EQUIPMENT_BONUS_FROM = "gml_Script_get_equipment_bonus_from@Ari@Ari";

		using AfterGetEquipmentBonusCallback = void(*)(MMAPI::Equipment::EquipmentBonusContext&);
		inline AfterGetEquipmentBonusCallback after_get_equipment_bonus_callback = nullptr;

		inline YYTK::RValue& GmlScriptAfterGetEquipmentBonusCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_GET_EQUIPMENT_BONUS_FROM)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_get_equipment_bonus_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::Equipment::EquipmentBonusContext context{
					static_cast<int>(Arguments[0]->ToInt64()),
					Result.ToDouble()
				};
				after_get_equipment_bonus_callback(context);
				Result = context.m_bonus_value;
			}

			return Result;
		}

		inline YYTK::RValue GetArmorSlots()
		{
			YYTK::RValue ari = MMAPI::Internal::global_instance->GetMember("__ari");
			YYTK::RValue armor = ari.GetMember("armor");
			YYTK::RValue slots = armor.GetMember("slots");
			return slots.GetMember("__buffer");
		}
	}

	/// Activates Equipment hooks. Installs the get_equipment_bonus_from hook so registered callbacks
	/// can observe and override Ari's per-infusion equipment bonuses. Safe to call before any
	/// Hooks::* registration — the callback no-ops until a user callback is bound.
	/// The existing pull-style helpers (GetEquippedArmor, GetEquippedArmorInfusions, etc.) do not require Enable().
	/// @return Status::Success if the hook is installed (or already was); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Equipment::Enable() called");

		MMAPI::Status status = MMAPI::Internal::InstallScriptHook(
			Internal::GML_SCRIPT_GET_EQUIPMENT_BONUS_FROM,
			reinterpret_cast<PVOID>(Internal::GmlScriptAfterGetEquipmentBonusCallback)
		);
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Gets the live item structs currently equipped in Ari's armor slots.
	/// @return A vector containing the equipped armor item structs.
	inline std::vector<YYTK::RValue> GetEquippedArmor()
	{
		std::vector<YYTK::RValue> equipped_armor;
		YYTK::RValue buffer = Internal::GetArmorSlots();

		size_t slot_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(buffer, slot_count);

		for (size_t i = 0; i < slot_count; i++)
		{
			YYTK::RValue* slot = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(buffer, i, slot);

			if (!slot || !MMAPI::Engine::StructVariableExists(*slot, "item"))
				continue;

			YYTK::RValue item = slot->GetMember("item");
			if (item.m_Kind == YYTK::VALUE_OBJECT)
				equipped_armor.push_back(item);
		}

		return equipped_armor;
	}

	/// Gets the internal item names for Ari's equipped armor.
	/// @return A vector of internal item recipe keys.
	inline std::vector<std::string> GetEquippedArmorInternalNames()
	{
		std::vector<std::string> internal_names;

		for (YYTK::RValue item : GetEquippedArmor())
		{
			if (!MMAPI::Engine::StructVariableExists(item, "prototype"))
				continue;

			YYTK::RValue prototype = item.GetMember("prototype");
			if (MMAPI::Engine::StructVariableExists(prototype, "recipe_key"))
				internal_names.push_back(prototype.GetMember("recipe_key").ToString());
		}

		return internal_names;
	}

	/// Counts infusions found on Ari's equipped armor.
	/// @return A map from infusion ID to count.
	inline std::map<MMAPI::Infusion::Ids, int> GetEquippedArmorInfusions()
	{
		std::map<MMAPI::Infusion::Ids, int> infusions;

		for (YYTK::RValue item : GetEquippedArmor())
		{
			if (!MMAPI::Engine::StructVariableExists(item, "infusion"))
				continue;

			YYTK::RValue infusion = item.GetMember("infusion");
			if (!MMAPI::Engine::IsNumeric(infusion))
				continue;

			int infusion_id = static_cast<int>(infusion.ToInt64());
			if (infusion_id < 0 || infusion_id > static_cast<int>(MMAPI::Infusion::Ids::VenomSword))
				continue;

			infusions[static_cast<MMAPI::Infusion::Ids>(infusion_id)]++;
		}

		return infusions;
	}

	namespace Hooks
	{
		/// Registers a callback that runs after the game's `get_equipment_bonus_from@Ari@Ari` script.
		/// Use `ctx.GetInfusion()` to identify which infusion the game is summing across equipped gear,
		/// `ctx.GetBonusValue()` to read the game's computed sum, and `ctx.SetBonusValue(double)` to
		/// override it (e.g. force a leeching bonus, suppress restricted slots, or apply class-set bonuses).
		/// @param callback A function called with a mutable `MMAPI::Equipment::EquipmentBonusContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterGetEquipmentBonus(Internal::AfterGetEquipmentBonusCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Equipment::Hooks::AfterGetEquipmentBonus, MMAPI::Equipment);

			return MMAPI::Internal::RegisterHook(
				"Equipment::AfterGetEquipmentBonus",
				Internal::after_get_equipment_bonus_callback,
				callback
			);
		}
	}
}
