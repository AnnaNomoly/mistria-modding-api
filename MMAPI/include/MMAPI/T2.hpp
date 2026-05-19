// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Hook.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::T2
{
	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_T2_READ       = "gml_Script_read@T2r@T2r";
		inline constexpr const char* GML_SCRIPT_T2_READ_WORLD = "gml_Script_read_world@T2r@T2r";

		// Live T2r Self/Other, latched from the read hook.
		// Used by TryGetT2Context for callers outside any hook frame.
		inline YYTK::CInstance* t2_self  = nullptr;
		inline YYTK::CInstance* t2_other = nullptr;

		// Cleared from the setup_main_screen pub/sub when the player returns to the title menu.
		// Registered by T2::Enable().
		inline void ClearT2OnReturnToTitle(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			t2_self  = nullptr;
			t2_other = nullptr;
		}

		inline YYTK::RValue& T2ReadContextCallback(IN YYTK::CInstance* Self, IN YYTK::CInstance* Other, OUT YYTK::RValue& Result, IN int ArgumentCount, IN YYTK::RValue** Arguments)
		{
			// Latch on first observation only (matches pre-MMAPI DD's pattern for this script).
			// See StatusEffect's manager-update comment for the failure mode of re-latching.
			if (!t2_self)
			{
				t2_self  = Self;
				t2_other = Other;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_T2_READ));
			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		/// Resolves the T2r's GML calling context, latched from the most recent read call.
		/// Cleared automatically when the game returns to the title menu.
		/// @return True if a T2 read has been observed this session, false otherwise.
		inline bool TryGetT2Context(YYTK::CInstance*& Self, YYTK::CInstance*& Other)
		{
			if (!t2_self)
				return false;
			Self  = t2_self;
			Other = t2_other;
			return true;
		}
	}

	/// Activates T2 utility functions. Installs the read hook so the live T2r Self/Other are latched for
	/// TryGetT2Context (cleared on return-to-title via the setup_main_screen pub/sub).
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::T2::Enable() called");

		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ClearT2OnReturnToTitle);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ MMAPI::Internal::GML_SCRIPT_SETUP_MAIN_SCREEN, reinterpret_cast<PVOID>(MMAPI::Internal::GmlScriptBeforeSetupMainScreenCallback) },
			{ Internal::GML_SCRIPT_T2_READ,                  reinterpret_cast<PVOID>(Internal::T2ReadContextCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Reads a value from the game's T2 database by key.
	/// @attention Requires MMAPI::T2::Enable() to have been called.
	/// @param key The T2 key to read.
	/// @return The T2 value as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue Read(const std::string& key)
	{
		MMAPI_REQUIRE_ENABLED("T2", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetT2Context(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_T2_READ, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		YYTK::RValue input = key.c_str();
		YYTK::RValue* args[1] = { &input };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
		return result;
	}

	/// Returns the full world-fact registry as a single struct whose members are fact names mapped
	/// to their values. World facts are the game's narrative-state store (typed: string, float,
	/// i64, or undefined; bools live as i64 0/1), used to gate dialogue, mail, and quests.
	///
	/// Wraps `gml_Script_read_world@T2r@T2r`, which despite its name takes no key and returns the
	/// entire fact map in one call — empirically verified that the script ignores any argument
	/// passed to it. Callers wanting a single fact should use ReadWorldFact below; this primitive
	/// is exposed for bulk inspection (e.g. dumping all set facts, scanning by prefix).
	///
	/// @note No write counterpart exists. The game only writes facts through T2 dialogue action
	/// expressions (parsed by `parse_t2_world_fact`), not a callable script.
	///
	/// @attention Requires MMAPI::T2::Enable() to have been called. The T2 calling context is
	/// latched from the first `read@T2r@T2r` of the session, so this returns undefined before any
	/// dialogue or schedule has run.
	/// @return A struct (VALUE_OBJECT) whose member names are fact names. Use `.ToMap()` to iterate
	///         safely, or pass through ReadWorldFact for single-name lookup. Returns undefined if
	///         the T2 context isn't yet latched.
	inline YYTK::RValue ReadAllWorldFacts()
	{
		MMAPI_REQUIRE_ENABLED("T2", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetT2Context(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_T2_READ_WORLD, reinterpret_cast<PVOID*>(&gml_script));
		if (!gml_script)
			return {};

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		return result;
	}

	/// Returns true if `name` is present in the world-fact registry, whether or not its value is
	/// currently set. This distinguishes two cases that ReadWorldFact collapses into one:
	///
	/// - **Fact not in registry** — the fact's referencing content isn't loaded in the current
	///   game session. The registry grows as new content references new fact names; the most
	///   common drivers are mods that add items/quests/etc. (e.g. installing a mod that adds
	///   item `telepop_blue` causes the game to auto-add `had_item_telepop_blue` to the
	///   registry), and less commonly vanilla game updates that ship new content. A fact can
	///   also be missing simply because the name is a typo or the fact has been removed.
	/// - **Fact in registry, value undefined** — fact is recognized but no T2 write has set it.
	///
	/// Both produce VALUE_UNDEFINED from ReadWorldFact. Use this predicate when the difference
	/// matters (e.g. detecting whether the content backing a fact is loaded, or whether a mod's
	/// own custom fact has been initialized).
	///
	/// @attention Requires MMAPI::T2::Enable() to have been called.
	/// @param name The world fact name.
	/// @return True if the registry struct has `name` as a member; false otherwise (including
	///         when the T2 context isn't yet latched).
	inline bool HasWorldFact(const std::string& name)
	{
		YYTK::RValue all_facts = ReadAllWorldFacts();
		if (all_facts.m_Kind != YYTK::VALUE_OBJECT)
			return false;

		auto members = all_facts.ToMap();
		return members.find(name) != members.end();
	}

	/// Reads a single world fact by name. Convenience over ReadAllWorldFacts() that fetches the
	/// full registry and looks up `name` in its member map. Examples of fact names:
	/// `"adeline_4h_materials"`, `"had_item_seed_tulip"`, `"opened_void_seal"`.
	///
	/// @note Each call fetches the full registry from the game. If you need to read many facts in
	/// a tight loop, prefer one ReadAllWorldFacts() + your own member lookups.
	///
	/// @note Both "fact not in the registry" and "fact in the registry but unset" return
	/// VALUE_UNDEFINED here. Use HasWorldFact to distinguish them when it matters.
	///
	/// @attention Requires MMAPI::T2::Enable() to have been called.
	/// @param name The world fact name.
	/// @return The fact value as an RValue, or undefined if the T2 context isn't latched, the
	///         registry isn't a struct, no fact with that name exists, or the fact exists but has
	///         not been written.
	inline YYTK::RValue ReadWorldFact(const std::string& name)
	{
		YYTK::RValue all_facts = ReadAllWorldFacts();
		if (all_facts.m_Kind != YYTK::VALUE_OBJECT)
			return {};

		auto members = all_facts.ToMap();
		auto it = members.find(name);
		if (it == members.end())
			return {};
		return it->second;
	}
}
