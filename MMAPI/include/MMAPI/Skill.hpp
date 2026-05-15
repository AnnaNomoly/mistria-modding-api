// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Instance.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Skill
{
	/// Source: globalInstance.__skill__
	enum class Ids : int
	{
		Archaeology   = 0,
		Blacksmithing = 1,
		Combat        = 2,
		Cooking       = 3,
		Farming       = 4,
		Fishing       = 5,
		Mining        = 6,
		Ranching      = 7,
		Woodcrafting  = 8
	};

	/// Total number of enumerators in Ids. Iterating [0, IdCount) covers every Ids value.
	inline constexpr int IdCount = 9;

	/// Invokes fn with every Ids value, in ascending order.
	template <typename Fn>
	inline void ForEachId(Fn fn)
	{
		for (int i = 0; i < IdCount; ++i)
			fn(static_cast<Ids>(i));
	}

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_GAIN_XP = "gml_Script_gain_xp@Ari@Ari";
	}

	/// Activates Skill utility functions. Cascades to MMAPI::Instance::Enable so GainExperience can resolve
	/// Ari's calling context internally.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Skill::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Skill, MMAPI::Instance);

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Adds experience to one of Ari's player skills.
	/// @attention Requires MMAPI::Skill::Enable() to have been called.
	/// @param skill The player skill to receive experience.
	/// @param experience The amount of experience to add.
	inline void GainExperience(MMAPI::Skill::Ids skill, double experience)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Skill");

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_GAIN_XP, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue skill_id = static_cast<int>(skill);
		YYTK::RValue xp = experience;
		YYTK::RValue result;
		YYTK::RValue* args[2] = { &skill_id, &xp };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 2, args);
	}
}
