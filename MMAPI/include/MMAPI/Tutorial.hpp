// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Tutorial
{
	/// Source: globalInstance.__tutorial__
	enum class Ids : int
	{
		AnimalSpriteStatue   = 0,
		ApiariesTerrariums   = 1,
		Blacksmithing        = 2,
		Cooking              = 3,
		Crafting             = 4,
		DarkMines            = 5,
		Dates                = 6,
		DragonsBreathSpell   = 7,
		ElsieGossip          = 8,
		Farming              = 9,
		Fishing              = 10,
		LavaCaves            = 11,
		LostAndFound         = 12,
		Magic                = 13,
		Milling              = 14,
		Mines                = 15,
		Mistmare             = 16,
		Museum               = 17,
		Pets                 = 18,
		Ranching             = 19,
		SacredLight          = 20,
		SkillPerks           = 21,
		SpringFestival       = 22,
		TeleportationChalice = 23,
		VoidSight            = 24,
		WaterSpriteStatue    = 25
	};

	/// Total number of enumerators in Ids. Iterating [0, IdCount) covers every Ids value.
	inline constexpr int IdCount = 26;

	/// Invokes fn with every Ids value, in ascending order.
	template <typename Fn>
	inline void ForEachId(Fn fn)
	{
		for (int i = 0; i < IdCount; ++i)
			fn(static_cast<Ids>(i));
	}

	namespace Internal
	{
		inline constexpr const char* GML_SCRIPT_SPAWN_TUTORIAL = "gml_Script_spawn_tutorial";
	}

	/// Spawns the given tutorial popup.
	/// @param tutorial The tutorial to show.
	inline void Spawn(MMAPI::Tutorial::Ids tutorial)
	{
		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_SPAWN_TUTORIAL, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue id = static_cast<int>(tutorial);
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &id };
		gml_script->m_Functions->m_ScriptFunction(nullptr, nullptr, result, 1, args);
	}

	/// Returns true if Ari has already seen the given tutorial.
	/// @param tutorial The tutorial to check.
	inline bool HasSeen(MMAPI::Tutorial::Ids tutorial)
	{
		int tutorial_id = static_cast<int>(tutorial);
		if (tutorial_id < 0)
			return false;

		YYTK::RValue ari = MMAPI::Internal::global_instance->GetMember("__ari");
		YYTK::RValue tutorials_seen = ari.GetMember("tutorials_seen");

		size_t tutorial_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(tutorials_seen, tutorial_count);
		if (static_cast<size_t>(tutorial_id) >= tutorial_count)
			return false;

		YYTK::RValue* seen = nullptr;
		MMAPI::Internal::module_interface->GetArrayEntry(tutorials_seen, static_cast<size_t>(tutorial_id), seen);
		return seen->ToBoolean();
	}

	/// Sets whether Ari has seen the given tutorial.
	/// @param tutorial The tutorial to update.
	/// @param seen True to mark seen, false to mark unseen.
	inline void SetSeen(MMAPI::Tutorial::Ids tutorial, bool seen)
	{
		int tutorial_id = static_cast<int>(tutorial);
		if (tutorial_id < 0)
			return;

		YYTK::RValue ari = *MMAPI::Internal::global_instance->GetRefMember("__ari");
		YYTK::RValue tutorials_seen = *ari.GetRefMember("tutorials_seen");

		size_t tutorial_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(tutorials_seen, tutorial_count);
		if (static_cast<size_t>(tutorial_id) >= tutorial_count)
			return;

		YYTK::RValue* tutorial_seen = nullptr;
		MMAPI::Internal::module_interface->GetArrayEntry(tutorials_seen, static_cast<size_t>(tutorial_id), tutorial_seen);
		*tutorial_seen = seen;
	}
}
