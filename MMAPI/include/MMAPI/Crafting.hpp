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

#include <optional>
#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Crafting
{
	/// Source: globalInstance.__recipe_context__
	/// Mirrors the game's per-discipline crafting menu identity. The integer value is the index
	/// into `__recipe_context__` — re-dump after a game patch in case the order changes.
	enum class Context : int
	{
		Blacksmithing = 0,
		Woodcrafting  = 1,
		Cooking       = 2,
		Milling       = 3,
		Refining      = 4
	};

	/// Total number of enumerators in Context. Iterating [0, ContextCount) covers every Context value.
	inline constexpr int ContextCount = 5;

	/// Invokes fn with every Context value, in ascending order.
	template <typename Fn>
	inline void ForEachContext(Fn fn)
	{
		for (int i = 0; i < ContextCount; ++i)
			fn(static_cast<Context>(i));
	}

	struct BeforePayComponentCostsContext
	{
		bool m_cancelled = false;

		/// Prevents the game's pay_component_costs script from running this fire — the crafting
		/// proceeds but no materials are consumed.
		void Cancel() { m_cancelled = true; }
	};

	struct AfterMaximumCraftsContext
	{
		int m_result = 0;

		/// Returns the max-crafts count the game computed (usually based on available materials).
		int GetResult() const { return m_result; }

		/// Overrides the max-crafts count the game receives. Set high (e.g. 999) to effectively
		/// remove the cap when materials are also being suppressed.
		void SetResult(int value) { m_result = value; }
	};

	struct AfterCheckItemCraftableContext
	{
		bool m_result = false;

		/// Returns the game's verdict on whether the item is craftable right now.
		bool GetResult() const { return m_result; }

		/// Overrides the verdict the game receives. Set true to force craftability when
		/// materials are also being suppressed.
		void SetResult(bool value) { m_result = value; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_CRAFTING_MENU_INITIALIZE = "gml_Script_initialize@CraftingMenu@CraftingMenu";
		inline constexpr const char* GML_SCRIPT_CRAFTING_MENU_CLOSE      = "gml_Script_on_close@CraftingMenu@CraftingMenu";
		inline constexpr const char* GML_SCRIPT_PAY_COMPONENT_COSTS      = "gml_Script_pay_component_costs";
		inline constexpr const char* GML_SCRIPT_MAXIMUM_CRAFTS           = "gml_Script_maximum_crafts@CraftingMenu@CraftingMenu";
		inline constexpr const char* GML_SCRIPT_CHECK_ITEM_CRAFTABLE     = "gml_Script_check_item_craftable@CraftingMenu@CraftingMenu";

		using AfterMenuOpenCallback                = void(*)();
		using AfterMenuCloseCallback               = void(*)();
		using BeforePayComponentCostsCallback      = void(*)(MMAPI::Crafting::BeforePayComponentCostsContext&);
		using AfterMaximumCraftsCallback           = void(*)(MMAPI::Crafting::AfterMaximumCraftsContext&);
		using AfterCheckItemCraftableCallback      = void(*)(MMAPI::Crafting::AfterCheckItemCraftableContext&);

		inline AfterMenuOpenCallback           after_menu_open_callback            = nullptr;
		inline AfterMenuCloseCallback          after_menu_close_callback           = nullptr;
		inline BeforePayComponentCostsCallback before_pay_component_costs_callback = nullptr;
		inline AfterMaximumCraftsCallback      after_maximum_crafts_callback       = nullptr;
		inline AfterCheckItemCraftableCallback after_check_item_craftable_callback = nullptr;

		// Latched int from initialize@CraftingMenu's Arguments[0].inner.context. The value is
		// the index into globalInstance.__recipe_context__ (matches Context enum values).
		// Set on menu open, cleared on menu close.
		inline std::optional<int> current_context_id;

		inline YYTK::RValue& GmlScriptCraftingMenuInitializeCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			// Read the context id BEFORE the original runs — Arguments[0].inner.context is the
			// menu's input data and could in principle be mutated by initialize's body.
			if (ArgumentCount > 0 && Arguments && Arguments[0]
				&& Arguments[0]->m_Kind == YYTK::VALUE_OBJECT
				&& MMAPI::Engine::StructVariableExists(*Arguments[0], "inner"))
			{
				YYTK::RValue inner = Arguments[0]->GetMember("inner");
				if (inner.m_Kind == YYTK::VALUE_OBJECT
					&& MMAPI::Engine::StructVariableExists(inner, "context"))
				{
					YYTK::RValue ctx = inner.GetMember("context");
					if (MMAPI::Engine::IsNumeric(ctx))
						current_context_id = static_cast<int>(ctx.ToInt64());
				}
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_CRAFTING_MENU_INITIALIZE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_menu_open_callback)
				after_menu_open_callback();

			return Result;
		}

		inline YYTK::RValue& GmlScriptCraftingMenuCloseCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_CRAFTING_MENU_CLOSE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			current_context_id = std::nullopt;

			if (after_menu_close_callback)
				after_menu_close_callback();

			return Result;
		}

		inline YYTK::RValue& GmlScriptBeforePayComponentCostsCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_pay_component_costs_callback)
			{
				MMAPI::Crafting::BeforePayComponentCostsContext context;
				before_pay_component_costs_callback(context);
				if (context.m_cancelled)
					return Result;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_PAY_COMPONENT_COSTS)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterMaximumCraftsCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_MAXIMUM_CRAFTS)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_maximum_crafts_callback && MMAPI::Engine::IsNumeric(Result))
			{
				MMAPI::Crafting::AfterMaximumCraftsContext context{ static_cast<int>(Result.ToInt64()) };
				after_maximum_crafts_callback(context);
				Result = context.m_result;
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterCheckItemCraftableCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_CHECK_ITEM_CRAFTABLE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_check_item_craftable_callback)
			{
				MMAPI::Crafting::AfterCheckItemCraftableContext context{ Result.ToBoolean() };
				after_check_item_craftable_callback(context);
				Result = context.m_result;
			}

			return Result;
		}
	}

	/// Activates Crafting utility functions. Eagerly installs the menu initialize/close, pay_component_costs,
	/// maximum_crafts, and check_item_craftable script hooks. The initialize hook also latches the active
	/// crafting context for `TryGetCurrentContext()`.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Crafting::Enable() called");

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_CRAFTING_MENU_INITIALIZE, reinterpret_cast<PVOID>(Internal::GmlScriptCraftingMenuInitializeCallback) },
			{ Internal::GML_SCRIPT_CRAFTING_MENU_CLOSE,      reinterpret_cast<PVOID>(Internal::GmlScriptCraftingMenuCloseCallback) },
			{ Internal::GML_SCRIPT_PAY_COMPONENT_COSTS,      reinterpret_cast<PVOID>(Internal::GmlScriptBeforePayComponentCostsCallback) },
			{ Internal::GML_SCRIPT_MAXIMUM_CRAFTS,           reinterpret_cast<PVOID>(Internal::GmlScriptAfterMaximumCraftsCallback) },
			{ Internal::GML_SCRIPT_CHECK_ITEM_CRAFTABLE,     reinterpret_cast<PVOID>(Internal::GmlScriptAfterCheckItemCraftableCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Returns the active crafting menu's context, or nullopt if the menu isn't open. Latched
	/// from `initialize@CraftingMenu@CraftingMenu`'s `Arguments[0].inner.context`; cleared on
	/// `on_close@CraftingMenu@CraftingMenu`.
	/// @attention Requires MMAPI::Crafting::Enable() to have been called.
	inline std::optional<MMAPI::Crafting::Context> TryGetCurrentContext()
	{
		MMAPI_REQUIRE_ENABLED("Crafting", std::nullopt);

		if (!Internal::current_context_id) return std::nullopt;

		int id = *Internal::current_context_id;
		if (id < 0 || id >= ContextCount) return std::nullopt;
		return static_cast<MMAPI::Crafting::Context>(id);
	}

	/// Resolves a Context from its game-internal name string by consulting
	/// `globalInstance.__recipe_context__`. Useful for mods that accept discipline names from
	/// JSON config.
	/// @param internal_name The game-internal name (lowercase, e.g. "blacksmithing", "cooking").
	/// @return The Context enum value, or std::nullopt if no match.
	inline std::optional<MMAPI::Crafting::Context> TryContextFromInternalName(const std::string& internal_name)
	{
		if (!MMAPI::Internal::global_instance) return std::nullopt;

		YYTK::RValue contexts = MMAPI::Internal::global_instance->GetMember("__recipe_context__");
		if (contexts.m_Kind == YYTK::VALUE_UNDEFINED) return std::nullopt;

		size_t count = 0;
		MMAPI::Internal::module_interface->GetArraySize(contexts, count);
		for (size_t i = 0; i < count; ++i)
		{
			YYTK::RValue* entry = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(contexts, i, entry);
			if (entry && entry->m_Kind == YYTK::VALUE_STRING && entry->ToString() == internal_name)
				return static_cast<MMAPI::Crafting::Context>(i);
		}
		return std::nullopt;
	}

	namespace Hooks
	{
		/// Registers a callback that runs after the game's `initialize@CraftingMenu@CraftingMenu`
		/// script — i.e. after the crafting menu opens. The context-tracking dispatcher has
		/// already latched the menu's discipline by the time this fires, so callbacks can call
		/// `MMAPI::Crafting::TryGetCurrentContext()` from here to branch on discipline.
		/// @param callback A parameterless function called after the menu's initialize runs.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterMenuOpen(Internal::AfterMenuOpenCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Crafting::Hooks::AfterMenuOpen, MMAPI::Crafting);

			return MMAPI::Internal::RegisterHook(
				"Crafting::AfterMenuOpen",
				Internal::after_menu_open_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `on_close@CraftingMenu@CraftingMenu`
		/// script. The dispatcher clears the latched context immediately before invoking the
		/// callback, so `TryGetCurrentContext()` already returns nullopt by the time this fires.
		/// @param callback A parameterless function called after the menu closes.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterMenuClose(Internal::AfterMenuCloseCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Crafting::Hooks::AfterMenuClose, MMAPI::Crafting);

			return MMAPI::Internal::RegisterHook(
				"Crafting::AfterMenuClose",
				Internal::after_menu_close_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game's `pay_component_costs` script — the
		/// step that deducts crafting materials from inventory. Call `ctx.Cancel()` to skip the
		/// deduction (the craft still proceeds, but materials aren't consumed).
		/// @param callback A function called with a mutable `MMAPI::Crafting::BeforePayComponentCostsContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforePayComponentCosts(Internal::BeforePayComponentCostsCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Crafting::Hooks::BeforePayComponentCosts, MMAPI::Crafting);

			return MMAPI::Internal::RegisterHook(
				"Crafting::BeforePayComponentCosts",
				Internal::before_pay_component_costs_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `maximum_crafts@CraftingMenu@CraftingMenu`
		/// script — the per-recipe "how many of this can I make right now" computation, usually
		/// bounded by available materials. Use `ctx.SetResult(int)` to override the cap.
		/// @param callback A function called with a mutable `MMAPI::Crafting::AfterMaximumCraftsContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterMaximumCrafts(Internal::AfterMaximumCraftsCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Crafting::Hooks::AfterMaximumCrafts, MMAPI::Crafting);

			return MMAPI::Internal::RegisterHook(
				"Crafting::AfterMaximumCrafts",
				Internal::after_maximum_crafts_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `check_item_craftable@CraftingMenu@CraftingMenu`
		/// script — the per-recipe "can I craft this right now" predicate. Use `ctx.SetResult(true)`
		/// to force craftability (typically paired with cancelling `pay_component_costs`).
		/// @param callback A function called with a mutable `MMAPI::Crafting::AfterCheckItemCraftableContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterCheckItemCraftable(Internal::AfterCheckItemCraftableCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Crafting::Hooks::AfterCheckItemCraftable, MMAPI::Crafting);

			return MMAPI::Internal::RegisterHook(
				"Crafting::AfterCheckItemCraftable",
				Internal::after_check_item_craftable_callback,
				callback
			);
		}
	}
}
