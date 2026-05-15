// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Engine.hpp"
#include "Hook.hpp"
#include "Instance.hpp"
#include "Item.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Recipe
{
	struct GenerateInfusionsContext
	{
		int          m_item_id = -1;
		YYTK::RValue m_infusions;

		/// The item_id of the recipe whose infusions are being generated, resolved from the script's
		/// Self. Returns -1 if Self is null or its `item_id` member is missing.
		int GetItemId() const { return m_item_id; }

		/// Returns the infusions array container the game's generate_infusions script produced
		/// (the Result of the script). The container has `__count`, `__internal_size`, and `__buffer`
		/// members typical of GameMaker dynamic arrays — mutate via the YYTK module interface if needed.
		/// TODO: Reverse-engineer gml_Script_generate_infusions to confirm the exact Result shape and
		///       per-entry layout, then tighten this accessor (typed entries, named getters, etc.).
		YYTK::RValue GetInfusions() const { return m_infusions; }

		/// Returns the current number of infusions in the array (live read from `__count`).
		size_t Count() const
		{
			if (m_infusions.m_Kind != YYTK::VALUE_OBJECT)
				return 0;
			if (!MMAPI::Engine::StructVariableExists(m_infusions, "__count"))
				return 0;
			return static_cast<size_t>(m_infusions.GetMember("__count").ToInt64());
		}

		/// Clears the infusions array — replaces `__buffer` with an empty array and zeroes
		/// `__count` / `__internal_size`. Use to suppress all infusions for a recipe.
		void Clear()
		{
			if (m_infusions.m_Kind != YYTK::VALUE_OBJECT)
				return;

			YYTK::RValue empty_array = MMAPI::Internal::module_interface->CallBuiltin("array_create", { 0 });
			*m_infusions.GetRefMember("__count")         = 0;
			*m_infusions.GetRefMember("__internal_size") = 0;
			*m_infusions.GetRefMember("__buffer")        = empty_array;
		}
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_UNLOCK_RECIPE      = "gml_Script_unlock_recipe@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_GENERATE_INFUSIONS = "gml_Script_generate_infusions@Recipe@Recipe";

		using AfterGenerateInfusionsCallback = void(*)(MMAPI::Recipe::GenerateInfusionsContext&);
		inline AfterGenerateInfusionsCallback after_generate_infusions_callback = nullptr;

		inline YYTK::RValue& GmlScriptAfterGenerateInfusionsCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_GENERATE_INFUSIONS)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_generate_infusions_callback)
			{
				int item_id = -1;
				if (Self)
				{
					YYTK::RValue self_rv = Self->ToRValue();
					if (MMAPI::Engine::StructVariableExists(self_rv, "item_id"))
						item_id = static_cast<int>(self_rv.GetMember("item_id").ToInt64());
				}

				MMAPI::Recipe::GenerateInfusionsContext context{ item_id, Result };
				after_generate_infusions_callback(context);
			}

			return Result;
		}

		inline YYTK::RValue GetRecipeUnlocks()
		{
			YYTK::RValue ari = MMAPI::Internal::global_instance->GetMember("__ari");
			if (ari.m_Kind == YYTK::VALUE_UNDEFINED)
				return {};

			return ari.GetMember("recipe_unlocks");
		}

		inline YYTK::RValue GetRecipeComponent(int item_id, size_t component_index)
		{
			YYTK::RValue item = MMAPI::Item::GetItemData(item_id);
			if (item.m_Kind == YYTK::VALUE_UNDEFINED)
				return {};

			if (!MMAPI::Engine::StructVariableExists(item, "recipe"))
				return {};

			YYTK::RValue recipe = item.GetMember("recipe");
			if (!MMAPI::Engine::StructVariableExists(recipe, "components"))
				return {};

			YYTK::RValue components = recipe.GetMember("components");
			if (!MMAPI::Engine::StructVariableExists(components, "__buffer"))
				return {};

			YYTK::RValue buffer = components.GetMember("__buffer");

			size_t component_count = 0;
			MMAPI::Internal::module_interface->GetArraySize(buffer, component_count);
			if (component_index >= component_count)
				return {};

			YYTK::RValue* component = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(buffer, component_index, component);

			if (!component)
				return {};

			return *component;
		}
	}

	/// Activates Recipe utility functions that resolve Ari context. Eagerly installs the generate_infusions
	/// script hook used by Hooks::AfterGenerateInfusions.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Recipe::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Recipe, MMAPI::Instance);

		// Recipe::SetComponentCount/SetComponentDuration call Item::GetItemData, which requires Item::Enable().
		MMAPI_ENABLE_DEPENDENCY(MMAPI::Recipe, MMAPI::Item);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHook(
			Internal::GML_SCRIPT_GENERATE_INFUSIONS,
			reinterpret_cast<PVOID>(Internal::GmlScriptAfterGenerateInfusionsCallback)
		);
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Returns true if Ari has unlocked the recipe for the given item ID.
	/// @param item_id The item ID for the recipe to check.
	inline bool IsUnlocked(int item_id)
	{
		if (item_id < 0)
			return false;

		YYTK::RValue recipe_unlocks = Internal::GetRecipeUnlocks();
		if (recipe_unlocks.m_Kind == YYTK::VALUE_UNDEFINED)
			return false;

		size_t unlock_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(recipe_unlocks, unlock_count);
		if (static_cast<size_t>(item_id) >= unlock_count)
			return false;

		YYTK::RValue* recipe_unlocked = nullptr;
		MMAPI::Internal::module_interface->GetArrayEntry(recipe_unlocks, static_cast<size_t>(item_id), recipe_unlocked);
		if (!recipe_unlocked)
			return false;

		return recipe_unlocked->ToDouble() != 0.0;
	}

	/// Unlocks a cooking recipe by item ID.
	/// @attention When show_popup is true, requires MMAPI::Recipe::Enable() to have been called; if the required
	/// context is unavailable, the recipe is still unlocked but the popup is skipped.
	/// @param item_id The item ID for the recipe to unlock.
	/// @param show_popup When true, displays the game's recipe unlocked popup if the recipe was newly unlocked.
	/// @return True if the recipe was newly unlocked; otherwise false.
	inline bool Unlock(int item_id, bool show_popup = true)
	{
		MMAPI_REQUIRE_ENABLED("Recipe", false);

		if (item_id < 0)
			return false;

		YYTK::RValue recipe_unlocks = Internal::GetRecipeUnlocks();
		if (recipe_unlocks.m_Kind == YYTK::VALUE_UNDEFINED)
			return false;

		size_t unlock_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(recipe_unlocks, unlock_count);
		if (static_cast<size_t>(item_id) >= unlock_count)
			return false;

		if (IsUnlocked(item_id))
			return false;

		recipe_unlocks[item_id] = 1.0;

		if (show_popup)
		{
			YYTK::CInstance* Self  = nullptr;
			YYTK::CInstance* Other = nullptr;
			if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
				return true;

			YYTK::CScript* gml_script = nullptr;
			MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_UNLOCK_RECIPE, reinterpret_cast<PVOID*>(&gml_script));

			YYTK::RValue id = static_cast<double>(item_id);
			YYTK::RValue result;
			YYTK::RValue* args[1] = { &id };
			gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
		}

		return true;
	}

	/// Sets the required count for a recipe component.
	/// @param item_id The item ID whose recipe should be modified.
	/// @param component_index The index of the recipe component to modify.
	/// @param count The new required component count.
	inline void SetComponentCount(int item_id, size_t component_index, int count)
	{
		YYTK::RValue component = Internal::GetRecipeComponent(item_id, component_index);
		if (component.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		MMAPI::Engine::StructVariableSet(component, "count", count);
	}

	/// Sets the crafting duration for a recipe component.
	/// @param item_id The item ID whose recipe should be modified.
	/// @param component_index The index of the recipe component to modify.
	/// @param duration The new component duration value.
	inline void SetComponentDuration(int item_id, size_t component_index, int duration)
	{
		YYTK::RValue component = Internal::GetRecipeComponent(item_id, component_index);
		if (component.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		MMAPI::Engine::StructVariableSet(component, "duration", duration);
	}

	/// Invokes fn(component_index, component) with each component in the item's recipe, in
	/// buffer order. Use with SetComponentDuration / SetComponentCount for per-component
	/// mutation, or just inspect the component struct fields directly.
	///
	/// No-op if the item has no recipe or the recipe has no components buffer.
	template <typename Fn>
	inline void ForEachComponent(int item_id, Fn fn)
	{
		YYTK::RValue item = MMAPI::Item::GetItemData(item_id);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED) return;
		if (!MMAPI::Engine::StructVariableExists(item, "recipe")) return;

		YYTK::RValue recipe = item.GetMember("recipe");
		if (recipe.m_Kind != YYTK::VALUE_OBJECT) return;
		if (!MMAPI::Engine::StructVariableExists(recipe, "components")) return;

		YYTK::RValue components = recipe.GetMember("components");
		if (components.m_Kind != YYTK::VALUE_OBJECT) return;
		if (!MMAPI::Engine::StructVariableExists(components, "__buffer")) return;

		YYTK::RValue buffer = components.GetMember("__buffer");
		size_t count = 0;
		MMAPI::Internal::module_interface->GetArraySize(buffer, count);

		for (size_t i = 0; i < count; ++i)
		{
			YYTK::RValue* entry = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(buffer, i, entry);
			if (entry) fn(i, *entry);
		}
	}

	namespace Hooks
	{
		/// Registers a callback that runs after the game's `generate_infusions@Recipe@Recipe` script.
		/// The wrapper resolves the recipe's `item_id` from Self (null-guarded for the `item_id` member).
		/// Use `ctx.GetItemId()` to identify the recipe, `ctx.GetInfusions()` to inspect the generated
		/// infusions array, `ctx.Count()` for its size, and `ctx.Clear()` to suppress all infusions for
		/// restricted items.
		/// @param callback A function called with a `MMAPI::Recipe::GenerateInfusionsContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterGenerateInfusions(Internal::AfterGenerateInfusionsCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Recipe::Hooks::AfterGenerateInfusions, MMAPI::Recipe);

			return MMAPI::Internal::RegisterHook(
				"Recipe::AfterGenerateInfusions",
				Internal::after_generate_infusions_callback,
				callback
			);
		}
	}
}
