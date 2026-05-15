// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Renown
{
	/// Gets Ari's current renown points.
	/// @return Ari's current renown points as an RValue.
	inline YYTK::RValue GetPoints()
	{
		YYTK::RValue ari = *MMAPI::Internal::global_instance->GetRefMember("__ari");
		return ari.GetMember("renown");
	}

	/// Gets the renown points required for a single renown level.
	/// @param level The renown level to evaluate.
	/// @return The individual renown point cost for that level as an RValue.
	inline YYTK::RValue GetLevelIndividualCost(int level)
	{
		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer("gml_Script_renown_level_individual_cost", reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		YYTK::RValue renown_level = level;
		YYTK::RValue* args[1] = { &renown_level };
		gml_script->m_Functions->m_ScriptFunction(nullptr, nullptr, result, 1, args);
		return result;
	}

	/// Gets the cumulative renown points required to reach a renown level.
	/// @param level The renown level to evaluate.
	/// @return The cumulative renown point cost as an RValue, or undefined if the level is less than one.
	inline YYTK::RValue GetLevelCumulativeCost(int level)
	{
		if (level < 1)
			return {};

		int cumulative_cost = 0;
		for (int i = 1; i <= level; i++)
		{
			YYTK::RValue individual_cost = GetLevelIndividualCost(i);
			if (individual_cost.m_Kind == YYTK::VALUE_UNDEFINED)
				return {};

			cumulative_cost += static_cast<int>(individual_cost.ToInt64());
		}

		return cumulative_cost;
	}

	/// Gets Ari's current renown level based on current renown points.
	/// @return Ari's current renown level as an RValue, or undefined if MMAPI has not been initialized.
	inline YYTK::RValue GetCurrentLevel()
	{
		YYTK::RValue renown = GetPoints();
		if (renown.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		int current_level = 1;
		for (int i = 100; i > 0; i--)
		{
			YYTK::RValue cumulative_cost = GetLevelCumulativeCost(i);
			if (cumulative_cost.m_Kind == YYTK::VALUE_UNDEFINED)
				return {};

			if (renown.ToInt64() >= cumulative_cost.ToInt64())
			{
				current_level = i;
				break;
			}
		}

		return current_level;
	}
}
