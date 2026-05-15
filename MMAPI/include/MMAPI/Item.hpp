// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Engine.hpp"
#include "Hook.hpp"
#include "Infusion.hpp"
#include "Instance.hpp"
#include "Log.hpp"
#include "Object.hpp"
#include "Status.hpp"
#include "Text.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Item
{
	struct UseItemContext
	{
		int               m_item_id   = -1;
		YYTK::CInstance*  m_self      = nullptr;
		YYTK::CInstance*  m_other     = nullptr;
		bool              m_cancelled = false;

		int GetItemId() const { return m_item_id; }
		YYTK::CInstance* GetSelf() const { return m_self; }
		YYTK::CInstance* GetOther() const { return m_other; }
		void Cancel() { m_cancelled = true; }

		/// Returns true when Ari is using the item from her inventory — i.e. a player-initiated
		/// use. Identified by Self being a non-object instance (`m_Object == nullptr`) and Other
		/// being `obj_ari`. Mods that should only react to player actions (most teleport / consume
		/// behaviors) should gate on this.
		bool IsAriUse() const
		{
			return m_self
				&& !m_self->m_Object
				&& MMAPI::Instance::IsNamed(m_other, "obj_ari");
		}

		/// Returns true when a world interactable (e.g. `obj_world_fountain`) is using the item
		/// rather than Ari. Identified by Self having a named `m_Object` (the world interactable)
		/// and Other being the global `Game` instance.
		bool IsWorldUse() const
		{
			return m_self
				&& m_self->m_Object
				&& m_self->m_Object->m_Name
				&& MMAPI::Instance::IsNamed(m_other, "Game");
		}
	};

	struct CreateItemPrototypesContext
	{
		YYTK::RValue m_item_prototypes;
		size_t       m_count = 0;

		/// Returns the item prototypes array the game's create_item_prototypes script produced.
		/// Indexed by item_id; iterate with GetArrayEntry from the YYTK module interface.
		YYTK::RValue GetItemPrototypes() const { return m_item_prototypes; }

		/// Returns the number of entries in the item prototypes array.
		size_t Count() const { return m_count; }
	};

	struct GetTreasureContext
	{
		int         m_object_id = -1;
		std::string m_object_name;

		/// Returns the object_id of the source that triggered the treasure roll, resolved from the
		/// script's Self (preferred) or Other. Returns -1 if neither exposes an `object_id` member.
		int GetObjectId() const { return m_object_id; }

		/// Returns the internal object name resolved from object_id (e.g. "obj_dungeon_treasure_chest"),
		/// or an empty view if object_id is -1 or the name could not be resolved.
		std::string_view GetObjectName() const { return m_object_name; }
	};

	struct GiveItemContext
	{
		YYTK::RValue* m_item     = nullptr;
		YYTK::RValue* m_quantity = nullptr;

		/// Returns the item_id of the item being given, or -1 if Arguments[0] is null/not a struct
		/// or lacks an `item_id` member.
		int GetItemId() const;

		/// Returns true if the item being given matches the given item_id.
		bool IsItem(int item_id) const { return GetItemId() == item_id && item_id >= 0; }

		/// Returns true if the item being given matches the given internal recipe key.
		bool IsItem(const std::string& internal_name) const;

		/// Sets the `infusion` member on the item struct.
		/// @return True if the struct has an `infusion` member that was updated; false otherwise.
		bool SetInfusion(MMAPI::Infusion::Ids infusion);

		/// Returns the quantity being given, or 0 if Arguments[1] is null or non-numeric.
		int GetQuantity() const;

		/// Overrides the quantity being given.
		void SetQuantity(int quantity);
	};

	struct DropItemContext
	{
		YYTK::CInstance* m_self  = nullptr;
		YYTK::CInstance* m_other = nullptr;
		YYTK::RValue*    m_arg0  = nullptr;

		/// Returns true if the drop refers to the item with the given item_id. Transparently handles
		/// all three Arguments[0] shapes the game uses: a struct with an `item_id` member, an array
		/// of such structs (any entry matching counts), or a raw numeric item_id.
		bool IsItem(int item_id) const
		{
			if (!m_arg0)
				return false;

			if (m_arg0->m_Kind == YYTK::VALUE_ARRAY)
			{
				size_t array_length = 0;
				MMAPI::Internal::module_interface->GetArraySize(*m_arg0, array_length);
				for (size_t i = 0; i < array_length; i++)
				{
					YYTK::RValue* entry = nullptr;
					MMAPI::Internal::module_interface->GetArrayEntry(*m_arg0, i, entry);
					if (!entry)
						continue;
					if (MMAPI::Engine::StructVariableExists(*entry, "item_id")
						&& static_cast<int>(entry->GetMember("item_id").ToInt64()) == item_id)
					{
						return true;
					}
				}
				return false;
			}

			if (m_arg0->m_Kind == YYTK::VALUE_OBJECT)
			{
				if (!MMAPI::Engine::StructVariableExists(*m_arg0, "item_id"))
					return false;
				return static_cast<int>(m_arg0->GetMember("item_id").ToInt64()) == item_id;
			}

			if (MMAPI::Engine::IsNumeric(*m_arg0))
				return static_cast<int>(m_arg0->ToInt64()) == item_id;

			return false;
		}

		/// Returns true if the drop refers to the item with the given internal recipe key
		/// (e.g. "ore_stone"). Resolves the name to an item_id via Item::GetIdFromInternalName.
		bool IsItem(const std::string& internal_name) const;

		/// Replaces the drop with the given item_id. Only effective on the non-array path
		/// (struct or raw numeric Arguments[0]). On the array path, mods should iterate and mutate
		/// individual entries directly, or invoke ctx.Drop(...) to add new drops.
		/// @return True if the item was substituted; false if Arguments[0] holds an array or the item lookup failed.
		bool SetItem(int item_id);

		/// Replaces the drop with the item identified by the given internal recipe key.
		/// Only effective on the non-array path.
		bool SetItem(const std::string& internal_name);

		/// Drops an item at the given coordinates using the script's Self/Other from this hook fire.
		/// Equivalent to MMAPI::Item::Drop, but reuses the latched Self/Other rather than the Ari context.
		/// > Note: this re-invokes drop_item and will retrigger any registered BeforeDropItem callback.
		/// > Callers are responsible for guarding against unbounded recursion.
		void Drop(int item_id, double x, double y);

		/// Drops an item by internal recipe key.
		void Drop(const std::string& internal_name, double x, double y);
	};

	struct GetUiIconContext
	{
		YYTK::CInstance* m_self          = nullptr;
		int              m_item_id       = -1;
		YYTK::RValue     m_sprite_asset;

		/// The live item instance the icon is being drawn for (the script's Self at hook time).
		YYTK::CInstance* GetSelf() const { return m_self; }

		/// The item_id read from the live item struct, or -1 if Self is null or the struct lacks an `item_id` member.
		int GetItemId() const { return m_item_id; }

		/// The sprite asset the game's get_ui_icon script produced.
		YYTK::RValue GetSpriteAsset() const { return m_sprite_asset; }

		/// Overrides the sprite asset the game will use to draw this item's UI icon.
		/// Typically populated with `MMAPI::Engine::AssetGetIndex("spr_...")`.
		void SetSpriteAsset(YYTK::RValue sprite_asset) { m_sprite_asset = sprite_asset; }
	};

	struct GetDisplayNameContext
	{
		YYTK::CInstance* m_self     = nullptr;
		int              m_item_id  = -1;
		std::string      m_resolved;

		/// The live item the display name was resolved for.
		YYTK::CInstance* GetSelf() const { return m_self; }

		/// The item_id read from the live item struct, or -1 if Self is null or the struct lacks an `item_id` member.
		int GetItemId() const { return m_item_id; }

		/// The localized display name the game's get_display_name script produced.
		std::string_view GetResolved() const { return m_resolved; }

		/// Overrides the display name the game receives — e.g. to append a "Penalty: 30%" tag.
		void SetResolved(std::string resolved) { m_resolved = std::move(resolved); }
	};

	struct GetDisplayDescriptionContext
	{
		YYTK::CInstance* m_self     = nullptr;
		int              m_item_id  = -1;
		std::string      m_resolved;

		/// The live item the description was resolved for.
		YYTK::CInstance* GetSelf() const { return m_self; }

		/// The item_id read from the live item struct, or -1 if Self is null or the struct lacks an `item_id` member.
		int GetItemId() const { return m_item_id; }

		/// The localized description the game's get_display_description script produced.
		std::string_view GetResolved() const { return m_resolved; }

		/// Overrides the description the game receives — e.g. to prepend "Recently Eaten: 3".
		void SetResolved(std::string resolved) { m_resolved = std::move(resolved); }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_DESERIALIZE_LIVE_ITEM         = "gml_Script_deserialize_live_item";
		inline constexpr const char* GML_SCRIPT_DROP_ITEM                     = "gml_Script_drop_item";
		inline constexpr const char* GML_SCRIPT_GIVE_ITEM                     = "gml_Script_give_item@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_USE_ITEM                      = "gml_Script_use_item";
		inline constexpr const char* GML_SCRIPT_GET_UI_ICON                   = "gml_Script_get_ui_icon@anon@4244@LiveItem@LiveItem";
		inline constexpr const char* GML_SCRIPT_GET_DISPLAY_NAME              = "gml_Script_get_display_name@anon@2420@LiveItem@LiveItem";
		inline constexpr const char* GML_SCRIPT_GET_DISPLAY_DESCRIPTION       = "gml_Script_get_display_description@anon@3696@LiveItem@LiveItem";
		inline constexpr const char* GML_SCRIPT_CREATE_ITEM_PROTOTYPES        = "gml_Script_create_item_prototypes";
		inline constexpr const char* GML_SCRIPT_GET_TREASURE_FROM_DISTRIBUTION = "gml_Script_get_treasure_from_distribution";

		using BeforeUseItemCallback                = void(*)(MMAPI::Item::UseItemContext&);
		using AfterGetUiIconCallback               = void(*)(MMAPI::Item::GetUiIconContext&);
		using AfterCreateItemPrototypesCallback    = void(*)(MMAPI::Item::CreateItemPrototypesContext&);
		using BeforeGetTreasureCallback            = void(*)(MMAPI::Item::GetTreasureContext&);
		using BeforeDropItemCallback               = void(*)(MMAPI::Item::DropItemContext&);
		using BeforeGiveItemCallback               = void(*)(MMAPI::Item::GiveItemContext&);
		using AfterGetDisplayNameCallback          = void(*)(MMAPI::Item::GetDisplayNameContext&);
		using AfterGetDisplayDescriptionCallback   = void(*)(MMAPI::Item::GetDisplayDescriptionContext&);

		inline BeforeUseItemCallback                before_use_item_callback                = nullptr;
		inline AfterGetUiIconCallback               after_get_ui_icon_callback              = nullptr;
		inline AfterCreateItemPrototypesCallback    after_create_item_prototypes_callback   = nullptr;
		inline BeforeGetTreasureCallback            before_get_treasure_callback            = nullptr;
		inline BeforeDropItemCallback               before_drop_item_callback               = nullptr;
		inline BeforeGiveItemCallback               before_give_item_callback               = nullptr;
		inline AfterGetDisplayNameCallback          after_get_display_name_callback         = nullptr;
		inline AfterGetDisplayDescriptionCallback   after_get_display_description_callback  = nullptr;

		// Item prototype cache, populated by GmlScriptAfterCreateItemPrototypesCallback when the game's
		// create_item_prototypes script runs once at startup. The prototype struct is what `LiveItem.prototype`
		// expects — distinct from `globalInstance.__item_data` which is the item-definition table.
		//
		// IMPORTANT: the game runs create_item_prototypes very early during startup. The hook must already be
		// installed by then or this cache stays empty. Item::Enable() installs the hook eagerly; mods must call
		// MMAPI::Initialize + Item::Enable from ModuleInitialize (which is the only path that runs early enough).
		inline std::map<int, YYTK::RValue> item_id_to_prototype_map;

		/// Returns the cached item prototype for the given item_id, or undefined if the cache is empty
		/// (Item::Enable was not called before the game's create_item_prototypes script ran) or item_id
		/// is not in the cache. Logs a one-shot Warn when the cache is empty, and per-call Warn when an
		/// item_id is missing from a populated cache.
		inline YYTK::RValue GetPrototype(int item_id)
		{
			if (item_id_to_prototype_map.empty())
			{
				static bool warned_empty_cache = false;
				if (!warned_empty_cache)
				{
					MMAPI::Log::Warn(
						"Item prototype cache is empty! Was Item::Enable() called before the game's "
						"create_item_prototypes script ran? Be sure to call it from ModuleInitialize."
					);
					warned_empty_cache = true;
				}
				return {};
			}

			auto it = item_id_to_prototype_map.find(item_id);
			if (it == item_id_to_prototype_map.end())
			{
				MMAPI::Log::Warn("Item prototype not found for item_id=%d", item_id);
				return {};
			}
			return it->second;
		}

		inline YYTK::RValue GetItemData()
		{
			return MMAPI::Internal::global_instance->GetMember("__item_data");
		}

		inline YYTK::RValue GetItemData(int item_id)
		{
			if (item_id < 0)
				return {};

			YYTK::RValue item_data = GetItemData();

			size_t item_count = 0;
			MMAPI::Internal::module_interface->GetArraySize(item_data, item_count);

			if (static_cast<size_t>(item_id) >= item_count)
				return {};

			YYTK::RValue* item = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(item_data, static_cast<size_t>(item_id), item);

			return *item;
		}

		inline YYTK::RValue GetItemValue(int item_id)
		{
			YYTK::RValue item = GetItemData(item_id);
			if (item.m_Kind == YYTK::VALUE_UNDEFINED)
				return {};

			if (!MMAPI::Engine::StructVariableExists(item, "value"))
				return {};

			return item.GetMember("value");
		}

		inline YYTK::RValue DeserializeLiveItem(YYTK::CInstance* Self, YYTK::CInstance* Other)
		{
			YYTK::CScript* gml_script = nullptr;
			MMAPI::Internal::module_interface->GetNamedRoutinePointer(GML_SCRIPT_DESERIALIZE_LIVE_ITEM, reinterpret_cast<PVOID*>(&gml_script));

			std::map<std::string, YYTK::RValue> live_item_data = {
				{ "cosmetic", YYTK::RValue() },
				{ "item_id", YYTK::RValue("sword_scrap_metal") },
				{ "infusion", YYTK::RValue() },
				{ "animal_cosmetic", YYTK::RValue() },
				{ "date_photo", YYTK::RValue() },
				{ "inner_item", YYTK::RValue() },
				{ "gold_to_gain", YYTK::RValue() },
				{ "auto_use", false },
				{ "pet_cosmetic_set_name", YYTK::RValue() }
			};

			YYTK::RValue input = live_item_data;
			YYTK::RValue result;
			YYTK::RValue* args[1] = { &input };
			gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
			return result;
		}

		inline YYTK::RValue& GmlScriptUseItemCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_use_item_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::Item::UseItemContext context{
					static_cast<int>(Arguments[0]->GetMember("item_id").ToInt64()),
					Self,
					Other
				};
				before_use_item_callback(context);

				if (context.m_cancelled)
					return Result;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_USE_ITEM)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterGetUiIconCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_GET_UI_ICON)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_get_ui_icon_callback)
			{
				int item_id = -1;
				if (Self)
				{
					YYTK::RValue self_rv = Self->ToRValue();
					if (MMAPI::Engine::StructVariableExists(self_rv, "item_id"))
						item_id = static_cast<int>(self_rv.GetMember("item_id").ToInt64());
				}

				MMAPI::Item::GetUiIconContext context{ Self, item_id, Result };
				after_get_ui_icon_callback(context);
				Result = context.m_sprite_asset;
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterGetDisplayNameCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_GET_DISPLAY_NAME)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_get_display_name_callback && Result.m_Kind == YYTK::VALUE_STRING)
			{
				int item_id = -1;
				if (Self)
				{
					YYTK::RValue self_rv = Self->ToRValue();
					if (MMAPI::Engine::StructVariableExists(self_rv, "item_id"))
						item_id = static_cast<int>(self_rv.GetMember("item_id").ToInt64());
				}

				MMAPI::Item::GetDisplayNameContext context{ Self, item_id, Result.ToString() };
				after_get_display_name_callback(context);
				Result = YYTK::RValue(context.m_resolved);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterGetDisplayDescriptionCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_GET_DISPLAY_DESCRIPTION)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_get_display_description_callback && Result.m_Kind == YYTK::VALUE_STRING)
			{
				int item_id = -1;
				if (Self)
				{
					YYTK::RValue self_rv = Self->ToRValue();
					if (MMAPI::Engine::StructVariableExists(self_rv, "item_id"))
						item_id = static_cast<int>(self_rv.GetMember("item_id").ToInt64());
				}

				MMAPI::Item::GetDisplayDescriptionContext context{ Self, item_id, Result.ToString() };
				after_get_display_description_callback(context);
				Result = YYTK::RValue(context.m_resolved);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterCreateItemPrototypesCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_CREATE_ITEM_PROTOTYPES)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			size_t array_length = 0;
			MMAPI::Internal::module_interface->GetArraySize(Result, array_length);

			// Always cache the prototypes, regardless of whether a user callback is registered.
			// Used by Item::Drop / DropItemContext::Drop / DropItemContext::SetItem to populate the
			// `prototype` member of a deserialized live item before drop_item runs.
			item_id_to_prototype_map.clear();
			for (size_t i = 0; i < array_length; ++i)
			{
				YYTK::RValue* entry = nullptr;
				MMAPI::Internal::module_interface->GetArrayEntry(Result, i, entry);
				if (entry)
					item_id_to_prototype_map[static_cast<int>(i)] = *entry;
			}
			MMAPI::Log::Debug("Cached %zu item prototypes", array_length);

			if (after_create_item_prototypes_callback)
			{
				MMAPI::Item::CreateItemPrototypesContext context{ Result, array_length };
				after_create_item_prototypes_callback(context);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptBeforeGetTreasureCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_get_treasure_callback)
			{
				// Resolve source: Self first, then Other. Both guarded for `object_id` member.
				int object_id = -1;
				if (Self)
				{
					YYTK::RValue self_rv = Self->ToRValue();
					if (MMAPI::Engine::StructVariableExists(self_rv, "object_id"))
						object_id = static_cast<int>(self_rv.GetMember("object_id").ToInt64());
				}
				if (object_id == -1 && Other)
				{
					YYTK::RValue other_rv = Other->ToRValue();
					if (MMAPI::Engine::StructVariableExists(other_rv, "object_id"))
						object_id = static_cast<int>(other_rv.GetMember("object_id").ToInt64());
				}

				std::string object_name;
				if (object_id != -1)
				{
					YYTK::RValue name = MMAPI::Object::GetInternalName(object_id);
					if (name.m_Kind == YYTK::VALUE_STRING)
						object_name = name.ToString();
				}

				MMAPI::Item::GetTreasureContext context{ object_id, std::move(object_name) };
				before_get_treasure_callback(context);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_GET_TREASURE_FROM_DISTRIBUTION)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

		inline YYTK::RValue& GmlScriptBeforeDropItemCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_drop_item_callback)
			{
				MMAPI::Item::DropItemContext context{
					Self,
					Other,
					(Arguments && ArgumentCount >= 1) ? Arguments[0] : nullptr
				};
				before_drop_item_callback(context);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_DROP_ITEM)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

		inline YYTK::RValue& GmlScriptBeforeGiveItemCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_give_item_callback)
			{
				MMAPI::Item::GiveItemContext context{
					(Arguments && ArgumentCount >= 1) ? Arguments[0] : nullptr,
					(Arguments && ArgumentCount >= 2) ? Arguments[1] : nullptr
				};
				before_give_item_callback(context);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_GIVE_ITEM)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

	}

	/// Returns the game's `__item_data` array containing every item's struct definition.
	/// @return The full item data collection as an RValue, or undefined if MMAPI has not been initialized.
	inline YYTK::RValue GetItemData()
	{
		return Internal::GetItemData();
	}

	/// Returns the struct definition for a single item.
	/// @param item_id The item ID to read.
	/// @return The item struct as an RValue, or undefined if the item ID is out of bounds.
	inline YYTK::RValue GetItemData(int item_id)
	{
		return Internal::GetItemData(item_id);
	}

	/// Gets the maximum stack size for an item.
	/// @param item_id The item ID to read.
	/// @return The max stack size as an RValue, or undefined if the item ID is out of bounds.
	inline YYTK::RValue GetMaxStack(int item_id)
	{
		YYTK::RValue item = Internal::GetItemData(item_id);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		return item.GetMember("max_stack");
	}

	/// Gets the shipping bin sale value for an item.
	/// @param item_id The item ID to read.
	/// @return The bin value as an RValue, or undefined if the item ID is out of bounds or has no bin value.
	inline YYTK::RValue GetBinValue(int item_id)
	{
		YYTK::RValue value = Internal::GetItemValue(item_id);
		if (value.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		if (!MMAPI::Engine::StructVariableExists(value, "bin"))
			return {};

		return value.GetMember("bin");
	}

	/// Sets the shipping bin sale value for an item.
	/// @param item_id The item ID to modify.
	/// @param bin_value The new bin sale value.
	inline void SetBinValue(int item_id, int bin_value)
	{
		YYTK::RValue value = Internal::GetItemValue(item_id);
		if (value.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		MMAPI::Engine::StructVariableSet(value, "bin", bin_value);
	}

	/// Gets the store purchase price for an item.
	/// @param item_id The item ID to read.
	/// @return The store value as an RValue, or undefined if the item ID is out of bounds or has no store value.
	inline YYTK::RValue GetStoreValue(int item_id)
	{
		YYTK::RValue value = Internal::GetItemValue(item_id);
		if (value.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		if (!MMAPI::Engine::StructVariableExists(value, "store"))
			return {};

		return value.GetMember("store");
	}

	/// Sets the store purchase price for an item.
	/// @param item_id The item ID to modify.
	/// @param store_value The new store price.
	inline void SetStoreValue(int item_id, int store_value)
	{
		YYTK::RValue value = Internal::GetItemValue(item_id);
		if (value.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		MMAPI::Engine::StructVariableSet(value, "store", store_value);
	}

	/// Gets the renown value awarded when an item is sold in the shipping bin.
	/// @param item_id The item ID to read.
	/// @return The renown value as an RValue, or undefined if the item ID is out of bounds or has no renown value.
	inline YYTK::RValue GetRenownValue(int item_id)
	{
		YYTK::RValue value = Internal::GetItemValue(item_id);
		if (value.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		if (!MMAPI::Engine::StructVariableExists(value, "renown"))
			return {};

		return value.GetMember("renown");
	}

	/// Gets the internal recipe key string for an item.
	/// @param item_id The item ID to read.
	/// @return The recipe key as an RValue string, or undefined if the item ID is out of bounds.
	inline YYTK::RValue GetInternalName(int item_id)
	{
		YYTK::RValue item = Internal::GetItemData(item_id);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		return item.GetMember("recipe_key");
	}

	/// Gets the localization key for an item's display name.
	/// @param item_id The item ID to read.
	/// @return The localization key as an RValue string, or undefined if the item ID is out of bounds.
	inline YYTK::RValue GetLocalizationKey(int item_id)
	{
		YYTK::RValue item = Internal::GetItemData(item_id);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		return item.GetMember("name_key");
	}

	/// Gets the localized display name for an item.
	/// @attention Requires MMAPI::Item::Enable() to have been called.
	/// @param item_id The item ID to read.
	/// @return The localized display name as an RValue string, or undefined if the required context is unavailable.
	inline YYTK::RValue GetLocalizedName(int item_id)
	{
		MMAPI_REQUIRE_ENABLED("Item", {});

		YYTK::RValue name_key = GetLocalizationKey(item_id);
		if (name_key.m_Kind != YYTK::VALUE_STRING)
			return {};

		return MMAPI::Text::GetLocalizedString(name_key.ToString());
	}

	/// Gets an item ID from its internal recipe key string.
	/// @param internal_name The recipe key to look up (e.g. "sword_scrap_metal").
	/// @return The item ID as an RValue, or undefined if no item with that recipe key exists.
	inline YYTK::RValue GetIdFromInternalName(std::string internal_name)
	{
		YYTK::RValue item_data = Internal::GetItemData();

		size_t item_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(item_data, item_count);

		for (size_t i = 0; i < item_count; i++)
		{
			YYTK::RValue* item = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(item_data, i, item);

			if (item->GetMember("recipe_key").ToString() == internal_name)
				return static_cast<int64_t>(i);
		}

		return {};
	}

	/// Returns true if the item has the given tag in its item data.
	/// @param item_id The item ID to check.
	/// @param tag The tag string to search for.
	inline bool HasTag(int item_id, const std::string& tag)
	{
		YYTK::RValue item = Internal::GetItemData(item_id);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return false;

		if (!MMAPI::Engine::StructVariableExists(item, "tags"))
			return false;

		YYTK::RValue tags = item.GetMember("tags");
		if (!MMAPI::Engine::StructVariableExists(tags, "__buffer"))
			return false;

		YYTK::RValue buffer = tags.GetMember("__buffer");
		size_t tag_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(buffer, tag_count);

		for (size_t i = 0; i < tag_count; i++)
		{
			YYTK::RValue* item_tag = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(buffer, i, item_tag);

			if (item_tag && item_tag->ToString() == tag)
				return true;
		}

		return false;
	}

	/// Gets all item IDs whose item data contains the given tag.
	/// @param tag The item tag to search for.
	/// @return A vector containing every matching item ID.
	inline std::vector<int> GetIdsWithTag(const std::string& tag)
	{
		std::vector<int> item_ids;

		YYTK::RValue item_data = Internal::GetItemData();

		size_t item_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(item_data, item_count);

		for (size_t i = 0; i < item_count; i++)
		{
			if (HasTag(static_cast<int>(i), tag))
				item_ids.push_back(static_cast<int>(i));
		}

		return item_ids;
	}

	/// Invokes fn with every item_id in `globalInstance.__item_data`, in id order.
	template <typename Fn>
	inline void ForEachItem(Fn fn)
	{
		YYTK::RValue item_data = Internal::GetItemData();

		size_t item_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(item_data, item_count);

		for (size_t i = 0; i < item_count; ++i)
			fn(static_cast<int>(i));
	}

	/// Finds every item whose internal recipe key OR localized display name case-insensitively
	/// matches any entry in `names`. Common pattern for resolving user-supplied item lists from
	/// config files (where users might write either internal keys like "ore_stone" or display
	/// names like "Stone").
	/// @attention Requires MMAPI::Item::Enable() to have been called.
	/// @param names The set of names to match against. Compared case-insensitively.
	/// @return All matching item_ids (no duplicates — a single item that matches multiple names
	///         appears once).
	inline std::vector<int> FindIdsByName(const std::vector<std::string>& names)
	{
		MMAPI_REQUIRE_ENABLED("Item", {});

		auto to_lower = [](std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		};

		std::vector<std::string> needles;
		needles.reserve(names.size());
		for (const auto& n : names)
			needles.push_back(to_lower(n));

		std::vector<int> matching_ids;
		ForEachItem([&](int item_id)
		{
			YYTK::RValue internal_rv = GetInternalName(item_id);
			std::string internal_lc = (internal_rv.m_Kind == YYTK::VALUE_STRING) ? to_lower(internal_rv.ToString()) : "";

			YYTK::RValue localized_rv = GetLocalizedName(item_id);
			std::string localized_lc = (localized_rv.m_Kind == YYTK::VALUE_STRING) ? to_lower(localized_rv.ToString()) : "";

			for (const auto& needle : needles)
			{
				if ((!internal_lc.empty() && needle == internal_lc) ||
					(!localized_lc.empty() && needle == localized_lc))
				{
					matching_ids.push_back(item_id);
					break;
				}
			}
		});

		return matching_ids;
	}

	/// Gets the stamina modifier for an item.
	/// @param item_id The item ID to read.
	/// @return The stamina modifier as an RValue, or undefined if the item ID is out of bounds.
	inline YYTK::RValue GetStaminaModifier(int item_id)
	{
		YYTK::RValue item = Internal::GetItemData(item_id);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		return item.GetMember("stamina_modifier");
	}

	/// Sets the stamina modifier for an item.
	/// @param item_id The item ID to modify.
	/// @param stamina_modifier The new stamina modifier value.
	inline void SetStaminaModifier(int item_id, double stamina_modifier)
	{
		YYTK::RValue item = Internal::GetItemData(item_id);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		MMAPI::Engine::StructVariableSet(item, "stamina_modifier", stamina_modifier);
	}

	/// Gets the health modifier for an item.
	/// @param item_id The item ID to read.
	/// @return The health modifier as an RValue, or undefined if the item ID is out of bounds.
	inline YYTK::RValue GetHealthModifier(int item_id)
	{
		YYTK::RValue item = Internal::GetItemData(item_id);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		return item.GetMember("health_modifier");
	}

	/// Sets the health modifier for an item.
	/// @param item_id The item ID to modify.
	/// @param health_modifier The new health modifier value.
	inline void SetHealthModifier(int item_id, double health_modifier)
	{
		YYTK::RValue item = Internal::GetItemData(item_id);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		MMAPI::Engine::StructVariableSet(item, "health_modifier", health_modifier);
	}

	/// Gets the mana modifier for an item.
	/// @param item_id The item ID to read.
	/// @return The mana modifier as an RValue, or undefined if the item ID is out of bounds.
	inline YYTK::RValue GetManaModifier(int item_id)
	{
		YYTK::RValue item = Internal::GetItemData(item_id);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		return item.GetMember("mana_modifier");
	}

	/// Sets the mana modifier for an item.
	/// @param item_id The item ID to modify.
	/// @param mana_modifier The new mana modifier value.
	inline void SetManaModifier(int item_id, double mana_modifier)
	{
		YYTK::RValue item = Internal::GetItemData(item_id);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		MMAPI::Engine::StructVariableSet(item, "mana_modifier", mana_modifier);
	}

	/// Gets the damage value for an item.
	/// @param item_id The item ID to read.
	/// @return The damage value as an RValue, or undefined if the item ID is out of bounds.
	inline YYTK::RValue GetDamage(int item_id)
	{
		YYTK::RValue item = Internal::GetItemData(item_id);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		return item.GetMember("damage");
	}

	/// Sets the damage value for an item.
	/// @param item_id The item ID to modify.
	/// @param damage The new damage value.
	inline void SetDamage(int item_id, double damage)
	{
		YYTK::RValue item = Internal::GetItemData(item_id);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		MMAPI::Engine::StructVariableSet(item, "damage", damage);
	}

	/// Returns true if the item has been donated to the museum.
	/// @param item_id The item ID to check.
	inline bool HasBeenDonated(int item_id)
	{
		if (item_id < 0)
			return false;

		YYTK::RValue museum_progress_data = MMAPI::Internal::global_instance->GetMember("__museum_progress_data");

		size_t item_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(museum_progress_data, item_count);
		if (static_cast<size_t>(item_id) >= item_count)
			return false;

		YYTK::RValue* item_donated = nullptr;
		MMAPI::Internal::module_interface->GetArrayEntry(museum_progress_data, static_cast<size_t>(item_id), item_donated);
		return item_donated->ToBoolean();
	}

	/// Returns true if the item is eligible to be donated to the museum — i.e. it appears in
	/// `globalInstance.__museum_data.data[*].sets.inner.*.items`. This is the static set of
	/// donatable items, independent of whether Ari has actually donated each one. Pair with
	/// `Item::HasBeenDonated` to check "still needs to be donated."
	///
	/// Lazily builds and caches the set of donatable item_ids on first call (the museum data is
	/// part of the game's static asset table, so the set doesn't change during a session). The
	/// first call after MMAPI::Initialize must occur once `global_instance` is populated; if it
	/// fires before then, the cache stays empty and subsequent calls return false until the data
	/// becomes available.
	/// @param item_id The item ID to check.
	inline bool IsDonatable(int item_id)
	{
		if (item_id < 0)
			return false;

		static std::set<int> donatable_ids;
		static bool loaded = false;

		if (!loaded && MMAPI::Internal::global_instance)
		{
			YYTK::RValue museum_data = MMAPI::Internal::global_instance->GetMember("__museum_data");
			if (museum_data.m_Kind != YYTK::VALUE_UNDEFINED)
			{
				YYTK::RValue data = museum_data.GetMember("data");
				size_t data_count = 0;
				MMAPI::Internal::module_interface->GetArraySize(data, data_count);

				for (size_t i = 0; i < data_count; i++)
				{
					YYTK::RValue* data_entry = nullptr;
					MMAPI::Internal::module_interface->GetArrayEntry(data, i, data_entry);
					if (!data_entry || data_entry->m_Kind != YYTK::VALUE_OBJECT) continue;

					if (!MMAPI::Engine::StructVariableExists(*data_entry, "sets")) continue;
					YYTK::RValue sets = data_entry->GetMember("sets");
					if (!MMAPI::Engine::StructVariableExists(sets, "inner")) continue;
					YYTK::RValue inner = sets.GetMember("inner");

					// `sets.inner` is keyed by user-defined set names ("fish", "bugs", "artifacts", etc.).
					// Enumerate its members to collect every set, then gather each set's items array.
					std::vector<std::string> set_names;
					MMAPI::Internal::module_interface->EnumInstanceMembers(inner,
						[&set_names](const char* name, YYTK::RValue* /*value*/) {
							set_names.emplace_back(name);
							return false;
						});

					for (const auto& set_name : set_names)
					{
						YYTK::RValue set = inner.GetMember(set_name.c_str());
						if (!MMAPI::Engine::StructVariableExists(set, "items")) continue;
						YYTK::RValue items = set.GetMember("items");

						size_t item_count = 0;
						MMAPI::Internal::module_interface->GetArraySize(items, item_count);
						for (size_t j = 0; j < item_count; j++)
						{
							YYTK::RValue* entry = nullptr;
							MMAPI::Internal::module_interface->GetArrayEntry(items, j, entry);
							if (entry && MMAPI::Engine::IsNumeric(*entry))
								donatable_ids.insert(static_cast<int>(entry->ToInt64()));
						}
					}
				}
				loaded = !donatable_ids.empty();
			}
		}

		return donatable_ids.contains(item_id);
	}

	/// Returns true if Ari has ever acquired the item (tracked in __ari.items_acquired).
	/// @param item_id The item ID to check.
	inline bool HasBeenAcquired(int item_id)
	{
		if (item_id < 0)
			return false;

		YYTK::RValue ari = MMAPI::Internal::global_instance->GetMember("__ari");
		YYTK::RValue items_acquired = ari.GetMember("items_acquired");

		size_t item_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(items_acquired, item_count);
		if (static_cast<size_t>(item_id) >= item_count)
			return false;

		YYTK::RValue* item_acquired = nullptr;
		MMAPI::Internal::module_interface->GetArrayEntry(items_acquired, static_cast<size_t>(item_id), item_acquired);
		return item_acquired->ToBoolean();
	}

	/// Activates Item utility functions. Cascades to MMAPI::Instance::Enable so Item::Drop can resolve Ari's
	/// GML calling context, and to MMAPI::Text::Enable so Item::GetLocalizedName can resolve the Localizer.
	/// Eagerly installs every Item script hook used by Hooks::* registrars (use_item, drop_item, give_item,
	/// get_ui_icon, create_item_prototypes, get_treasure_from_distribution).
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Item::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Item, MMAPI::Instance);

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Item, MMAPI::Text);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_USE_ITEM,                       reinterpret_cast<PVOID>(Internal::GmlScriptUseItemCallback) },
			{ Internal::GML_SCRIPT_DROP_ITEM,                      reinterpret_cast<PVOID>(Internal::GmlScriptBeforeDropItemCallback) },
			{ Internal::GML_SCRIPT_GIVE_ITEM,                      reinterpret_cast<PVOID>(Internal::GmlScriptBeforeGiveItemCallback) },
			{ Internal::GML_SCRIPT_GET_UI_ICON,                    reinterpret_cast<PVOID>(Internal::GmlScriptAfterGetUiIconCallback) },
			{ Internal::GML_SCRIPT_GET_DISPLAY_NAME,               reinterpret_cast<PVOID>(Internal::GmlScriptAfterGetDisplayNameCallback) },
			{ Internal::GML_SCRIPT_GET_DISPLAY_DESCRIPTION,        reinterpret_cast<PVOID>(Internal::GmlScriptAfterGetDisplayDescriptionCallback) },
			{ Internal::GML_SCRIPT_CREATE_ITEM_PROTOTYPES,         reinterpret_cast<PVOID>(Internal::GmlScriptAfterCreateItemPrototypesCallback) },
			{ Internal::GML_SCRIPT_GET_TREASURE_FROM_DISTRIBUTION, reinterpret_cast<PVOID>(Internal::GmlScriptBeforeGetTreasureCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Drops an item at the given room coordinates.
	/// @attention Requires MMAPI::Item::Enable() to have been called.
	/// @param item_id The item ID to drop.
	/// @param x The room x coordinate.
	/// @param y The room y coordinate.
	inline void Drop(int item_id, double x, double y)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Item");

		if (item_id < 0)
			return;

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return;

		YYTK::RValue item = Internal::DeserializeLiveItem(Self, Other);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		YYTK::RValue prototype = Internal::GetPrototype(item_id);
		if (prototype.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		*item.GetRefMember("prototype") = prototype;
		*item.GetRefMember("item_id") = item_id;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_DROP_ITEM, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue x_value = x;
		YYTK::RValue y_value = y;
		YYTK::RValue undefined;
		YYTK::RValue result;
		YYTK::RValue* args[4] = { &item, &x_value, &y_value, &undefined };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 4, args);
	}

	inline bool DropItemContext::IsItem(const std::string& internal_name) const
	{
		YYTK::RValue id = MMAPI::Item::GetIdFromInternalName(internal_name);
		if (!MMAPI::Engine::IsNumeric(id))
			return false;
		return IsItem(static_cast<int>(id.ToInt64()));
	}

	inline bool DropItemContext::SetItem(int item_id)
	{
		if (!m_arg0 || item_id < 0)
			return false;
		if (m_arg0->m_Kind == YYTK::VALUE_ARRAY)
			return false;

		if (m_arg0->m_Kind == YYTK::VALUE_OBJECT
			&& MMAPI::Engine::StructVariableExists(*m_arg0, "item_id"))
		{
			YYTK::RValue prototype = Internal::GetPrototype(item_id);
			if (prototype.m_Kind == YYTK::VALUE_UNDEFINED)
				return false;

			*m_arg0->GetRefMember("prototype") = prototype;
			*m_arg0->GetRefMember("item_id")   = item_id;
			return true;
		}

		if (MMAPI::Engine::IsNumeric(*m_arg0))
		{
			*m_arg0 = item_id;
			return true;
		}

		return false;
	}

	inline bool DropItemContext::SetItem(const std::string& internal_name)
	{
		YYTK::RValue id = MMAPI::Item::GetIdFromInternalName(internal_name);
		if (!MMAPI::Engine::IsNumeric(id))
			return false;
		return SetItem(static_cast<int>(id.ToInt64()));
	}

	inline void DropItemContext::Drop(int item_id, double x, double y)
	{
		if (item_id < 0 || !m_self)
			return;

		YYTK::RValue item = Internal::DeserializeLiveItem(m_self, m_other);
		if (item.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		YYTK::RValue prototype = Internal::GetPrototype(item_id);
		if (prototype.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		*item.GetRefMember("prototype") = prototype;
		*item.GetRefMember("item_id")   = item_id;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(
			Internal::GML_SCRIPT_DROP_ITEM,
			reinterpret_cast<PVOID*>(&gml_script)
		);

		YYTK::RValue x_value = x;
		YYTK::RValue y_value = y;
		YYTK::RValue undefined;
		YYTK::RValue result;
		YYTK::RValue* args[4] = { &item, &x_value, &y_value, &undefined };
		gml_script->m_Functions->m_ScriptFunction(m_self, m_other, result, 4, args);
	}

	inline void DropItemContext::Drop(const std::string& internal_name, double x, double y)
	{
		YYTK::RValue id = MMAPI::Item::GetIdFromInternalName(internal_name);
		if (!MMAPI::Engine::IsNumeric(id))
			return;
		Drop(static_cast<int>(id.ToInt64()), x, y);
	}

	inline int GiveItemContext::GetItemId() const
	{
		if (!m_item || m_item->m_Kind != YYTK::VALUE_OBJECT)
			return -1;
		if (!MMAPI::Engine::StructVariableExists(*m_item, "item_id"))
			return -1;
		return static_cast<int>(m_item->GetMember("item_id").ToInt64());
	}

	inline bool GiveItemContext::IsItem(const std::string& internal_name) const
	{
		YYTK::RValue id = MMAPI::Item::GetIdFromInternalName(internal_name);
		if (!MMAPI::Engine::IsNumeric(id))
			return false;
		return IsItem(static_cast<int>(id.ToInt64()));
	}

	inline bool GiveItemContext::SetInfusion(MMAPI::Infusion::Ids infusion)
	{
		if (!m_item || m_item->m_Kind != YYTK::VALUE_OBJECT)
			return false;
		if (!MMAPI::Engine::StructVariableExists(*m_item, "infusion"))
			return false;

		MMAPI::Engine::StructVariableSet(*m_item, "infusion", static_cast<int>(infusion));
		return true;
	}

	inline int GiveItemContext::GetQuantity() const
	{
		if (!m_quantity || !MMAPI::Engine::IsNumeric(*m_quantity))
			return 0;
		return static_cast<int>(m_quantity->ToInt64());
	}

	inline void GiveItemContext::SetQuantity(int quantity)
	{
		if (!m_quantity)
			return;
		*m_quantity = quantity;
	}

	namespace Hooks
	{
		/// Registers a callback that runs before the game processes an item use.
		/// Call ctx.Cancel() to prevent the game from processing the item use.
		/// @param callback A function called with a mutable use context before the game processes it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeUseItem(Internal::BeforeUseItemCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Item::Hooks::BeforeUseItem, MMAPI::Item);

			return MMAPI::Internal::RegisterHook(
				"Item::BeforeUseItem",
				Internal::before_use_item_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `get_ui_icon` script for a live item.
		/// Read `ctx.GetItemId()` to identify the item (or -1 if Self/item_id is unavailable) and
		/// `ctx.GetSpriteAsset()` to see the sprite asset the game computed; call `ctx.SetSpriteAsset(...)`
		/// to override it with a custom sprite (typically `MMAPI::Engine::AssetGetIndex("spr_...")`).
		/// @param callback A function called with a mutable `MMAPI::Item::GetUiIconContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterGetUiIcon(Internal::AfterGetUiIconCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Item::Hooks::AfterGetUiIcon, MMAPI::Item);

			return MMAPI::Internal::RegisterHook(
				"Item::AfterGetUiIcon",
				Internal::after_get_ui_icon_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `get_display_name` script for a live item.
		/// Read `ctx.GetResolved()` to see the localized name the game produced and call
		/// `ctx.SetResolved(...)` to override it (e.g. append a "[Penalty]" tag). Useful for
		/// reflecting mod state in the inventory UI.
		/// @param callback A function called with a mutable `MMAPI::Item::GetDisplayNameContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterGetDisplayName(Internal::AfterGetDisplayNameCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Item::Hooks::AfterGetDisplayName, MMAPI::Item);

			return MMAPI::Internal::RegisterHook(
				"Item::AfterGetDisplayName",
				Internal::after_get_display_name_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `get_display_description` script for a
		/// live item. Read `ctx.GetResolved()` to see the localized description and call
		/// `ctx.SetResolved(...)` to override it (e.g. prepend "Recently Eaten: 3" to a food item).
		/// @param callback A function called with a mutable `MMAPI::Item::GetDisplayDescriptionContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterGetDisplayDescription(Internal::AfterGetDisplayDescriptionCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Item::Hooks::AfterGetDisplayDescription, MMAPI::Item);

			return MMAPI::Internal::RegisterHook(
				"Item::AfterGetDisplayDescription",
				Internal::after_get_display_description_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `create_item_prototypes` script.
		/// Use `ctx.GetItemPrototypes()` to access the prototypes array (indexed by item_id) and
		/// `ctx.Count()` for its length — useful for snapshotting the full prototype table when the
		/// game finishes loading items.
		/// @param callback A function called with a `MMAPI::Item::CreateItemPrototypesContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterCreateItemPrototypes(Internal::AfterCreateItemPrototypesCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Item::Hooks::AfterCreateItemPrototypes, MMAPI::Item);

			return MMAPI::Internal::RegisterHook(
				"Item::AfterCreateItemPrototypes",
				Internal::after_create_item_prototypes_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game's `get_treasure_from_distribution` script.
		/// The wrapper resolves the source object from Self (preferred) or Other, both guarded for an
		/// `object_id` member. `ctx.GetObjectId()` returns the resolved id (or -1 if neither has one),
		/// and `ctx.GetObjectName()` returns its internal name (resolved via Object::GetInternalName).
		/// Use this to detect the originating chest/container and inject custom loot before the game's roll runs.
		/// @param callback A function called with a `MMAPI::Item::GetTreasureContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeGetTreasure(Internal::BeforeGetTreasureCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Item::Hooks::BeforeGetTreasure, MMAPI::Item);

			return MMAPI::Internal::RegisterHook(
				"Item::BeforeGetTreasure",
				Internal::before_get_treasure_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game's `drop_item` script. The script's
		/// Arguments[0] can be a single item struct, an array of item structs, or a raw numeric item_id —
		/// the context's helpers handle all three shapes transparently.
		/// Use `ctx.IsItem(id_or_name)` to recognize the drop, `ctx.SetItem(id_or_name)` to substitute it
		/// (non-array path only), and `ctx.Drop(id_or_name, x, y)` to issue additional drops using the
		/// hook's own Self/Other (warning: re-enters this hook — guard against recursion in your callback).
		/// @param callback A function called with a mutable `MMAPI::Item::DropItemContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeDropItem(Internal::BeforeDropItemCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Item::Hooks::BeforeDropItem, MMAPI::Item);

			return MMAPI::Internal::RegisterHook(
				"Item::BeforeDropItem",
				Internal::before_drop_item_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game's `give_item@Ari@Ari` script. The script
		/// awards an item struct (Arguments[0]) in a given quantity (Arguments[1]) to Ari. The
		/// callback can identify the item, override its infusion, and change the quantity — the item
		/// itself can't be swapped through this hook.
		/// Use `ctx.GetItemId()`/`ctx.IsItem(id_or_name)` to inspect, `ctx.SetInfusion(...)` to override
		/// the infusion (no-op if the struct has no `infusion` field), and
		/// `ctx.GetQuantity()`/`ctx.SetQuantity(int)` for the count.
		/// @param callback A function called with a mutable `MMAPI::Item::GiveItemContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeGiveItem(Internal::BeforeGiveItemCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Item::Hooks::BeforeGiveItem, MMAPI::Item);

			return MMAPI::Internal::RegisterHook(
				"Item::BeforeGiveItem",
				Internal::before_give_item_callback,
				callback
			);
		}
	}
}
