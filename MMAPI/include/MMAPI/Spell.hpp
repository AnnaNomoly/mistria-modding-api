// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Engine.hpp"
#include "Hook.hpp"
#include "Instance.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Spell
{
	/// Source: globalInstance.__spell__
	enum class Ids : int
	{
		FireBreath  = 0,
		FullRestore = 1,
		Growth      = 2,
		SacredLight = 3,
		SummonRain  = 4
	};

	/// Total number of enumerators in Ids. Iterating [0, IdCount) covers every Ids value.
	inline constexpr int IdCount = 5;

	/// Invokes fn with every Ids value, in ascending order.
	template <typename Fn>
	inline void ForEachId(Fn fn)
	{
		for (int i = 0; i < IdCount; ++i)
			fn(static_cast<Ids>(i));
	}

	struct CanCastSpellContext
	{
		int m_spell_id = 0;
		bool m_result = false;

		/// Returns the spell being checked.
		MMAPI::Spell::Ids GetSpell() const { return static_cast<MMAPI::Spell::Ids>(m_spell_id); }

		/// Returns the can-cast result as computed by the game.
		bool GetResult() const { return m_result; }

		/// Overrides whether the game allows casting this spell.
		void SetResult(bool can_cast) { m_result = can_cast; }
	};

	struct BeforeSpellCastContext
	{
		int m_spell_id = 0;
		bool m_cancelled = false;

		/// Returns the spell being cast.
		MMAPI::Spell::Ids GetSpell() const { return static_cast<MMAPI::Spell::Ids>(m_spell_id); }

		/// Substitutes the spell the game will cast — the trampoline runs with the new id rather than
		/// the original. Useful for "redirect this spell to a different one" patterns (e.g. routing a
		/// custom variant cast to fire_breath so it triggers fire breath's status effect chain).
		void SetSpell(MMAPI::Spell::Ids spell) { m_spell_id = static_cast<int>(spell); }

		/// Prevents the game's cast_spell script from running.
		void Cancel() { m_cancelled = true; }
	};

	struct AfterSpellCastContext
	{
		int m_spell_id = 0;

		/// Returns the spell that was cast.
		MMAPI::Spell::Ids GetSpell() const { return static_cast<MMAPI::Spell::Ids>(m_spell_id); }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_CAN_CAST_SPELL = "gml_Script_can_cast_spell";
		inline constexpr const char* GML_SCRIPT_CAST_SPELL      = "gml_Script_cast_spell";

		using AfterCanCastSpellCallback = void(*)(MMAPI::Spell::CanCastSpellContext&);
		using BeforeSpellCastCallback    = void(*)(MMAPI::Spell::BeforeSpellCastContext&);
		using AfterSpellCastCallback     = void(*)(MMAPI::Spell::AfterSpellCastContext&);

		inline AfterCanCastSpellCallback after_can_cast_spell_callback = nullptr;
		inline BeforeSpellCastCallback    before_spell_cast_callback     = nullptr;
		inline AfterSpellCastCallback     after_spell_cast_callback      = nullptr;

		inline YYTK::RValue& GmlScriptCanCastSpellCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_CAN_CAST_SPELL)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_can_cast_spell_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::Spell::CanCastSpellContext context{ static_cast<int>(Arguments[0]->ToInt64()), Result.ToBoolean() };
				after_can_cast_spell_callback(context);
				Result = context.m_result;
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptCastSpellCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_spell_cast_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::Spell::BeforeSpellCastContext context{ static_cast<int>(Arguments[0]->ToInt64()) };
				before_spell_cast_callback(context);

				if (context.m_cancelled)
					return Result;

				*Arguments[0] = context.m_spell_id;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_CAST_SPELL)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_spell_cast_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::Spell::AfterSpellCastContext context{ static_cast<int>(Arguments[0]->ToInt64()) };
				after_spell_cast_callback(context);
			}

			return Result;
		}

		inline YYTK::RValue GetSpellData()
		{
			return MMAPI::Internal::global_instance->GetMember("__spells");
		}

		inline YYTK::RValue GetSpellData(int spell_id)
		{
			if (spell_id < 0)
				return {};

			YYTK::RValue spells = GetSpellData();

			size_t spell_count = 0;
			MMAPI::Internal::module_interface->GetArraySize(spells, spell_count);

			if (static_cast<size_t>(spell_id) >= spell_count)
				return {};

			YYTK::RValue* spell = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(spells, static_cast<size_t>(spell_id), spell);

			return *spell;
		}

		inline YYTK::RValue GetCost(int spell_id)
		{
			YYTK::RValue spell = GetSpellData(spell_id);
			if (spell.m_Kind == YYTK::VALUE_UNDEFINED)
				return {};

			return spell.GetMember("cost");
		}

		inline void SetCost(int spell_id, int cost)
		{
			YYTK::RValue spell = GetSpellData(spell_id);
			if (spell.m_Kind == YYTK::VALUE_UNDEFINED)
				return;

			MMAPI::Engine::StructVariableSet(spell, "cost", cost < 0 ? 0 : cost);
		}
	}

	/// Gets the mana cost of a spell from globalInstance.__spells.
	/// @param spell The spell to read.
	/// @return The spell's mana cost as an RValue, or undefined if the spell ID is out of bounds.
	inline YYTK::RValue GetCost(MMAPI::Spell::Ids spell)
	{
		return Internal::GetCost(static_cast<int>(spell));
	}

	/// Sets the mana cost of a spell in globalInstance.__spells. Negative values are clamped to 0.
	/// @param spell The spell to modify.
	/// @param cost The new mana cost.
	inline void SetCost(MMAPI::Spell::Ids spell, int cost)
	{
		Internal::SetCost(static_cast<int>(spell), cost);
	}

	/// Activates Spell utility functions and installs the can_cast_spell / cast_spell hook callbacks.
	/// Safe to call before any Hooks::* registration — the callbacks no-op until a user callback is bound.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Spell::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Spell, MMAPI::Instance);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_CAN_CAST_SPELL, reinterpret_cast<PVOID>(Internal::GmlScriptCanCastSpellCallback) },
			{ Internal::GML_SCRIPT_CAST_SPELL,     reinterpret_cast<PVOID>(Internal::GmlScriptCastSpellCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Returns true if Ari can currently cast the given spell.
	/// @attention Requires MMAPI::Spell::Enable() to have been called.
	/// @param spell The spell to check.
	inline bool CanCast(MMAPI::Spell::Ids spell)
	{
		MMAPI_REQUIRE_ENABLED("Spell", false);

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return false;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_CAN_CAST_SPELL, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue spell_id = static_cast<int>(spell);
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &spell_id };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
		return result.ToBoolean();
	}

	/// Casts the given spell as Ari.
	/// @attention Requires MMAPI::Spell::Enable() to have been called.
	/// @param spell The spell to cast.
	inline void Cast(MMAPI::Spell::Ids spell)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Spell");

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_CAST_SPELL, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue spell_id = static_cast<int>(spell);
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &spell_id };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
	}

	namespace Hooks
	{
		/// Registers a callback that can override the game's can_cast_spell result after it runs.
		/// Use ctx.SetResult(bool) to allow or block casting the spell.
		/// @param callback A function called with the can-cast context after the game evaluates it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterCanCastSpell(Internal::AfterCanCastSpellCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Spell::Hooks::AfterCanCastSpell, MMAPI::Spell);

			return MMAPI::Internal::RegisterHook(
				"Spell::AfterCanCastSpell",
				Internal::after_can_cast_spell_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game's cast_spell script executes.
		/// Call ctx.Cancel() to prevent the game from processing the spell cast.
		/// @param callback A function called with the cast spell context before the game processes it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeSpellCast(Internal::BeforeSpellCastCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Spell::Hooks::BeforeSpellCast, MMAPI::Spell);

			return MMAPI::Internal::RegisterHook(
				"Spell::BeforeSpellCast",
				Internal::before_spell_cast_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's cast_spell script executes.
		/// Use `ctx.GetSpell()` to identify which spell was cast — pair with set-bonus/counter logic
		/// that should fire only once the game has processed the cast.
		/// @param callback A function called with the cast spell context after the game processes it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterSpellCast(Internal::AfterSpellCastCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Spell::Hooks::AfterSpellCast, MMAPI::Spell);

			return MMAPI::Internal::RegisterHook(
				"Spell::AfterSpellCast",
				Internal::after_spell_cast_callback,
				callback
			);
		}
	}
}
