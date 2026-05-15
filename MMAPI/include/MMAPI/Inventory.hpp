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

namespace MMAPI::Inventory
{
	/// Context passed to AfterSlotPop callbacks. Fires after the game's
	/// `pop@InventorySlot@Inventory` script — the single-item operation typically used for
	/// trashing one item or popping the top item off a stack. The popped item struct is exposed
	/// via the Result.
	struct AfterSlotPopContext
	{
		YYTK::RValue m_popped_item;

		/// Returns true if a popped item was produced (Result was a struct).
		bool IsValid() const { return m_popped_item.m_Kind == YYTK::VALUE_OBJECT; }

		/// Returns the popped item struct (the script's Result). Has the usual live-item members
		/// (item_id, infusion, prototype, etc.). Empty if `IsValid()` is false.
		YYTK::RValue GetItem() const { return m_popped_item; }

		/// Returns the item_id of the popped item, or -1 if no item was produced or the struct
		/// lacks an `item_id` member.
		int GetItemId() const
		{
			if (!IsValid()) return -1;
			if (!MMAPI::Engine::StructVariableExists(m_popped_item, "item_id")) return -1;
			return static_cast<int>(m_popped_item.GetMember("item_id").ToInt64());
		}
	};

	/// Context passed to AfterSlotDrain callbacks. Fires after the game's
	/// `drain@InventorySlot@Inventory` script — the batch operation typically used for trashing
	/// a whole stack at once (e.g. via shift-click). The drained items are exposed via the
	/// Result, which is a wrapped container with a `__buffer` array of item structs.
	struct AfterSlotDrainContext
	{
		YYTK::RValue m_drained_items;

		/// Returns true if drained items were produced (Result was a struct).
		bool IsValid() const { return m_drained_items.m_Kind == YYTK::VALUE_OBJECT; }

		/// Returns the drained items container (the script's Result). Use ForEachItem or pull the
		/// `__buffer` array yourself.
		YYTK::RValue GetItems() const { return m_drained_items; }

		/// Returns the number of items in the drained set, or 0 if unavailable.
		size_t Count() const
		{
			if (!IsValid()) return 0;
			if (!MMAPI::Engine::StructVariableExists(m_drained_items, "__buffer")) return 0;
			YYTK::RValue buffer = m_drained_items.GetMember("__buffer");
			size_t count = 0;
			MMAPI::Internal::module_interface->GetArraySize(buffer, count);
			return count;
		}

		/// Invokes fn(item) with each drained item struct (`YYTK::RValue`), in buffer order.
		template <typename Fn>
		void ForEachItem(Fn fn) const
		{
			if (!IsValid()) return;
			if (!MMAPI::Engine::StructVariableExists(m_drained_items, "__buffer")) return;
			YYTK::RValue buffer = m_drained_items.GetMember("__buffer");
			size_t count = 0;
			MMAPI::Internal::module_interface->GetArraySize(buffer, count);
			for (size_t i = 0; i < count; ++i)
			{
				YYTK::RValue* entry = nullptr;
				MMAPI::Internal::module_interface->GetArrayEntry(buffer, i, entry);
				if (entry) fn(*entry);
			}
		}
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_DESERIALIZE_INVENTORY = "gml_Script_deserialize@anon@6096@__Inventory@Inventory";
		inline constexpr const char* GML_SCRIPT_COUNT_ITEM            = "gml_Script_item_id_quantity@anon@4106@__Inventory@Inventory";
		inline constexpr const char* GML_SCRIPT_REMOVE_ITEM           = "gml_Script_remove@anon@2021@__Inventory@Inventory";
		inline constexpr const char* GML_SCRIPT_SLOT_POP              = "gml_Script_pop@InventorySlot@Inventory";
		inline constexpr const char* GML_SCRIPT_SLOT_DRAIN            = "gml_Script_drain@InventorySlot@Inventory";

		using AfterSlotPopCallback   = void(*)(MMAPI::Inventory::AfterSlotPopContext&);
		using AfterSlotDrainCallback = void(*)(MMAPI::Inventory::AfterSlotDrainContext&);

		inline AfterSlotPopCallback   after_slot_pop_callback   = nullptr;
		inline AfterSlotDrainCallback after_slot_drain_callback = nullptr;

		inline YYTK::RValue& GmlScriptAfterSlotPopCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_SLOT_POP)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_slot_pop_callback)
			{
				MMAPI::Inventory::AfterSlotPopContext context{ Result };
				after_slot_pop_callback(context);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterSlotDrainCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_SLOT_DRAIN)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_slot_drain_callback)
			{
				MMAPI::Inventory::AfterSlotDrainContext context{ Result };
				after_slot_drain_callback(context);
			}

			return Result;
		}

		// Live Inventory Self/Other, latched from the deserialize hook.
		// Used by TryGetInventoryContext for callers outside any hook frame.
		inline YYTK::CInstance* inventory_self  = nullptr;
		inline YYTK::CInstance* inventory_other = nullptr;

		// Cleared from the setup_main_screen pub/sub when the player returns to the title menu.
		// Registered by Inventory::Enable().
		inline void ClearInventoryOnReturnToTitle(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			inventory_self  = nullptr;
			inventory_other = nullptr;
		}

		inline YYTK::RValue& DeserializeInventoryContextCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			// Latch on first observation only (matches pre-MMAPI DD's pattern for this script).
			// See StatusEffect's manager-update comment for the failure mode of re-latching.
			if (!inventory_self)
			{
				inventory_self  = Self;
				inventory_other = Other;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_DESERIALIZE_INVENTORY)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		/// Resolves the Inventory's GML calling context, latched from the most recent deserialize call.
		/// Cleared automatically when the game returns to the title menu.
		/// @return True if an Inventory deserialize has been observed this session, false otherwise.
		inline bool TryGetInventoryContext(YYTK::CInstance*& Self, YYTK::CInstance*& Other)
		{
			if (!inventory_self)
				return false;
			Self  = inventory_self;
			Other = inventory_other;
			return true;
		}
	}

	/// Activates Inventory utility functions. Installs the deserialize hook so the live Inventory Self/Other are latched
	/// for TryGetInventoryContext (cleared on return-to-title via the setup_main_screen pub/sub). Also eagerly installs
	/// the slot pop/drain hooks used by Hooks::AfterSlotPop and Hooks::AfterSlotDrain — each no-ops until a user
	/// callback is bound.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Inventory::Enable() called");

		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ClearInventoryOnReturnToTitle);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ MMAPI::Internal::GML_SCRIPT_SETUP_MAIN_SCREEN, reinterpret_cast<PVOID>(MMAPI::Internal::GmlScriptBeforeSetupMainScreenCallback) },
			{ Internal::GML_SCRIPT_DESERIALIZE_INVENTORY,    reinterpret_cast<PVOID>(Internal::DeserializeInventoryContextCallback) },
			{ Internal::GML_SCRIPT_SLOT_POP,                 reinterpret_cast<PVOID>(Internal::GmlScriptAfterSlotPopCallback) },
			{ Internal::GML_SCRIPT_SLOT_DRAIN,               reinterpret_cast<PVOID>(Internal::GmlScriptAfterSlotDrainCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Counts how many of an item Ari currently has.
	/// @attention Requires MMAPI::Inventory::Enable() to have been called.
	/// @param item_id The item ID to count.
	/// @return The count as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue CountItem(int item_id)
	{
		MMAPI_REQUIRE_ENABLED("Inventory", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetInventoryContext(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_COUNT_ITEM, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue item = item_id;
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &item };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
		return result;
	}

	/// Removes a quantity of an item from Ari's inventory.
	/// @attention Requires MMAPI::Inventory::Enable() to have been called.
	/// @param item_id The item ID to remove.
	/// @param quantity The quantity to remove.
	inline void RemoveItem(int item_id, int quantity)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Inventory");

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetInventoryContext(Self, Other))
			return;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_REMOVE_ITEM, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue item = item_id;
		YYTK::RValue amount = quantity;
		YYTK::RValue result;
		YYTK::RValue* args[2] = { &item, &amount };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 2, args);
	}

	namespace Hooks
	{
		/// Registers a callback that runs after the game's `pop@InventorySlot@Inventory` script —
		/// the single-item operation typically used for trashing one item or popping the top item
		/// off a stack. Read `ctx.GetItemId()` / `ctx.GetItem()` to inspect the popped item.
		///
		/// Note: pop fires for any single-item removal, not just trashes. Mods that only care
		/// about the trash flow typically gate on `Engine::AudioIsPlaying(<trash sound asset>)`
		/// inside the callback, since the trash sound effect plays around the pop.
		///
		/// @param callback A function called with a `MMAPI::Inventory::AfterSlotPopContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterSlotPop(Internal::AfterSlotPopCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Inventory::Hooks::AfterSlotPop, MMAPI::Inventory);

			return MMAPI::Internal::RegisterHook(
				"Inventory::AfterSlotPop",
				Internal::after_slot_pop_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `drain@InventorySlot@Inventory` script —
		/// the batch operation typically used for trashing a whole stack at once (e.g. via
		/// shift-click). Read `ctx.Count()` for the drained item count and use
		/// `ctx.ForEachItem([](YYTK::RValue item) { ... })` to inspect each.
		///
		/// Note: drain fires for any multi-item removal, not just trashes. Same gating advice as
		/// `AfterSlotPop`.
		///
		/// @param callback A function called with a `MMAPI::Inventory::AfterSlotDrainContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterSlotDrain(Internal::AfterSlotDrainCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Inventory::Hooks::AfterSlotDrain, MMAPI::Inventory);

			return MMAPI::Internal::RegisterHook(
				"Inventory::AfterSlotDrain",
				Internal::after_slot_drain_callback,
				callback
			);
		}
	}
}
