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

#include <map>
#include <optional>
#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Object
{
	struct Position
	{
		int x = 0;
		int y = 0;
	};

	struct BeforeFurniturePlacedContext
	{
		YYTK::RValue m_furniture;
		bool         m_cancelled = false;

		/// Returns the furniture being placed (write_furniture_to_location's Arguments[3]). Has
		/// members like object_id, top_left_x/y, write_size_x/y.
		YYTK::RValue GetFurniture() const { return m_furniture; }

		/// Convenience: object_id from the furniture struct, or -1 if it can't be resolved.
		int GetObjectId() const;

		/// Convenience: top-left tile position of the furniture, or std::nullopt if unavailable.
		std::optional<MMAPI::Object::Position> GetTopLeftPosition() const;

		/// Prevents the placement from proceeding. Use to enforce per-object placement limits
		/// (e.g. "max 2 of this furniture per save").
		void Cancel() { m_cancelled = true; }
	};

	struct FurniturePlacedContext
	{
		YYTK::RValue m_furniture;

		/// Returns the placed-furniture struct (the script's Result). Has members like object_id,
		/// top_left_x/y, write_size_x/y.
		YYTK::RValue GetFurniture() const { return m_furniture; }

		/// Convenience: object_id from the furniture struct, or -1 if it can't be resolved.
		int GetObjectId() const;

		/// Convenience: top-left tile position of the furniture, or std::nullopt if unavailable.
		std::optional<MMAPI::Object::Position> GetTopLeftPosition() const;
	};

	struct ObjectErasedContext
	{
		YYTK::RValue m_object;

		/// Returns the object renderer being erased (the script's Arguments[0]).
		YYTK::RValue GetObject() const { return m_object; }

		/// Convenience: object_id of the object being erased, or -1 if it can't be resolved.
		int GetObjectId() const;

		/// Convenience: top-left tile position of the object, or std::nullopt if unavailable.
		std::optional<MMAPI::Object::Position> GetTopLeftPosition() const;
	};

	struct NodeRendererSetSpriteContext
	{
		YYTK::CInstance* m_self          = nullptr;
		YYTK::RValue*    m_sprite_asset  = nullptr;
		std::string      m_sprite_name;
		bool             m_cancelled     = false;

		/// The live obj_node_renderer instance whose sprite is being set. Use `MMAPI::Object::GetNode(self)`
		/// to read the node data (prototype, day_count, etc.) backing this renderer.
		YYTK::CInstance* GetSelf() const { return m_self; }

		/// The sprite name the renderer is about to switch to, resolved via `sprite_get_name`
		/// on Arguments[0]. Empty if Arguments[0] isn't a sprite asset.
		std::string_view GetSpriteName() const { return m_sprite_name; }

		/// The sprite asset (VALUE_REF) the renderer is about to switch to. Empty RValue if unavailable.
		YYTK::RValue GetSpriteAsset() const { return m_sprite_asset ? *m_sprite_asset : YYTK::RValue(); }

		/// Substitutes the sprite asset the renderer will use this fire. Typically populated with
		/// `MMAPI::Engine::AssetGetIndex("spr_...")`. Writes through to the script's Arguments[0] so
		/// the trampoline sees the new asset.
		void SetSpriteAsset(YYTK::RValue sprite_asset)
		{
			if (m_sprite_asset)
				*m_sprite_asset = sprite_asset;
		}

		/// Prevents the game's set_sprite script from running this fire — the renderer keeps its previous sprite.
		void Cancel() { m_cancelled = true; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_WRITE_FURNITURE_TO_LOCATION = "gml_Script_write_furniture_to_location";
		inline constexpr const char* GML_SCRIPT_ERASE_OBJECT_RENDERER       = "gml_Script_erase_object_renderer";
		inline constexpr const char* GML_SCRIPT_NODE_RENDERER_SET_SPRITE    = "gml_Script_set_sprite@gml_Object_obj_node_renderer_Create_0";

		using BeforeFurniturePlacedCallback        = void(*)(MMAPI::Object::BeforeFurniturePlacedContext&);
		using AfterFurniturePlacedCallback         = void(*)(MMAPI::Object::FurniturePlacedContext&);
		using AfterObjectErasedCallback            = void(*)(MMAPI::Object::ObjectErasedContext&);
		using BeforeNodeRendererSetSpriteCallback  = void(*)(MMAPI::Object::NodeRendererSetSpriteContext&);

		inline BeforeFurniturePlacedCallback       before_furniture_placed_callback        = nullptr;
		inline AfterFurniturePlacedCallback        after_furniture_placed_callback         = nullptr;
		inline AfterObjectErasedCallback           after_object_erased_callback            = nullptr;
		inline BeforeNodeRendererSetSpriteCallback before_node_renderer_set_sprite_callback = nullptr;

		inline std::map<int, std::string> object_id_to_internal_name_map;
		inline std::map<std::string, int> internal_name_to_object_id_map;

		inline YYTK::RValue ResolveInternalName(int object_id)
		{
			YYTK::CScript* gml_script = nullptr;
			MMAPI::Internal::module_interface->GetNamedRoutinePointer("gml_Script_try_object_id_to_string", reinterpret_cast<PVOID*>(&gml_script));

			YYTK::RValue result;
			YYTK::RValue id = object_id;
			YYTK::RValue* args[1] = { &id };
			gml_script->m_Functions->m_ScriptFunction(nullptr, nullptr, result, 1, args);
			return result;
		}

		// Custom mod objects are absent from globalInstance.__object_id__, so we resolve via
		// gml_Script_try_object_id_to_string instead. Custom mod items do appear in __item_data.
		inline bool EnsureObjectIdCache()
		{
			if (!object_id_to_internal_name_map.empty())
				return true;

			for (int i = 0; i < 10000; i++)
			{
				YYTK::RValue internal_name = ResolveInternalName(i);
				if (internal_name.m_Kind != YYTK::VALUE_STRING)
					continue;

				std::string name = internal_name.ToString();
				object_id_to_internal_name_map[i] = name;
				internal_name_to_object_id_map[name] = i;
			}

			return !object_id_to_internal_name_map.empty();
		}
	}

	/// Gets an object internal name from an object ID.
	/// @param object_id The object ID to resolve.
	/// @return The object internal name as an RValue, or undefined if the script context is unavailable or the ID cannot be resolved.
	inline YYTK::RValue GetInternalName(int object_id)
	{
		if (Internal::object_id_to_internal_name_map.contains(object_id))
			return Internal::object_id_to_internal_name_map.at(object_id).c_str();

		YYTK::RValue internal_name = Internal::ResolveInternalName(object_id);
		if (internal_name.m_Kind == YYTK::VALUE_STRING)
		{
			std::string name = internal_name.ToString();
			Internal::object_id_to_internal_name_map[object_id] = name;
			Internal::internal_name_to_object_id_map[name] = object_id;
		}

		return internal_name;
	}

	/// Gets an object ID from an internal object name.
	/// Uses gml_Script_try_object_id_to_string so custom objects can be discovered.
	/// @param internal_name The internal object name to resolve.
	/// @return The object ID as an RValue, or undefined if the object cannot be found.
	inline YYTK::RValue GetIdFromInternalName(const std::string& internal_name)
	{
		if (!Internal::EnsureObjectIdCache())
			return {};

		if (!Internal::internal_name_to_object_id_map.contains(internal_name))
			return {};

		return Internal::internal_name_to_object_id_map.at(internal_name);
	}

	/// Returns true if an object ID resolves to the exact internal name.
	/// @param object_id The object ID to check.
	/// @param internal_name The exact internal object name to compare.
	inline bool IsInternalName(int object_id, const std::string& internal_name)
	{
		YYTK::RValue resolved_name = GetInternalName(object_id);
		if (resolved_name.m_Kind != YYTK::VALUE_STRING)
			return false;

		return resolved_name.ToString() == internal_name;
	}

	/// Returns true if an object ID resolves to an internal name containing the given text.
	/// @param object_id The object ID to check.
	/// @param name_part The internal object name fragment to search for.
	inline bool InternalNameContains(int object_id, const std::string& name_part)
	{
		YYTK::RValue resolved_name = GetInternalName(object_id);
		if (resolved_name.m_Kind != YYTK::VALUE_STRING)
			return false;

		return resolved_name.ToString().find(name_part) != std::string::npos;
	}

	/// Gets an object category ID from an internal category name.
	/// @param internal_name The internal object category name to look up.
	/// @return The object category ID as an RValue, or undefined if the category is not found.
	inline YYTK::RValue GetCategoryIdFromInternalName(const std::string& internal_name)
	{
		YYTK::RValue object_categories = MMAPI::Internal::global_instance->GetMember("__object_category__");
		size_t category_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(object_categories, category_count);

		for (size_t i = 0; i < category_count; i++)
		{
			YYTK::RValue* category = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(object_categories, i, category);

			if (category && category->ToString() == internal_name.c_str())
				return static_cast<int64_t>(i);
		}

		return {};
	}

	/// Gets the node struct from a live object renderer instance.
	/// @param instance The instance to inspect.
	/// @return The node struct as an RValue, or undefined if the instance does not have a node.
	inline YYTK::RValue GetNode(YYTK::CInstance* instance)
	{
		if (!instance)
			return {};

		YYTK::RValue value = instance->ToRValue();
		if (!MMAPI::Engine::StructVariableExists(value, "node"))
			return {};

		return value.GetMember("node");
	}

	/// Gets a node's prototype struct.
	/// @param node The node struct to inspect.
	/// @return The prototype struct as an RValue, or undefined if the node does not have a prototype.
	inline YYTK::RValue GetPrototype(YYTK::RValue node)
	{
		if (!MMAPI::Engine::StructVariableExists(node, "prototype"))
			return {};

		return node.GetMember("prototype");
	}

	/// Gets an object ID from an object struct, node struct, or live object renderer instance value.
	/// @param object_or_node The value to inspect.
	/// @return The object ID as an RValue, or undefined if no object ID can be found.
	inline YYTK::RValue GetObjectId(YYTK::RValue object_or_node)
	{
		if (MMAPI::Engine::StructVariableExists(object_or_node, "object_id"))
			return object_or_node.GetMember("object_id");

		if (MMAPI::Engine::StructVariableExists(object_or_node, "node"))
			return GetObjectId(object_or_node.GetMember("node"));

		YYTK::RValue prototype = GetPrototype(object_or_node);
		if (prototype.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		if (!MMAPI::Engine::StructVariableExists(prototype, "object_id"))
			return {};

		return prototype.GetMember("object_id");
	}

	/// Gets an object ID from a live object renderer instance.
	/// @param instance The instance to inspect.
	/// @return The object ID as an RValue, or undefined if no object ID can be found.
	inline YYTK::RValue GetObjectId(YYTK::CInstance* instance)
	{
		if (!instance)
			return {};

		return GetObjectId(instance->ToRValue());
	}

	/// Gets a category ID from an object prototype, node struct, or live object renderer instance value.
	/// @param object_or_node The value to inspect.
	/// @return The category ID as an RValue, or undefined if no category ID can be found.
	inline YYTK::RValue GetCategoryId(YYTK::RValue object_or_node)
	{
		if (MMAPI::Engine::StructVariableExists(object_or_node, "category_id"))
			return object_or_node.GetMember("category_id");

		if (MMAPI::Engine::StructVariableExists(object_or_node, "node"))
			return GetCategoryId(object_or_node.GetMember("node"));

		YYTK::RValue prototype = GetPrototype(object_or_node);
		if (prototype.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		if (!MMAPI::Engine::StructVariableExists(prototype, "category_id"))
			return {};

		return prototype.GetMember("category_id");
	}

	/// Gets a category ID from a live object renderer instance.
	/// @param instance The instance to inspect.
	/// @return The category ID as an RValue, or undefined if no category ID can be found.
	inline YYTK::RValue GetCategoryId(YYTK::CInstance* instance)
	{
		if (!instance)
			return {};

		return GetCategoryId(instance->ToRValue());
	}

	/// Gets a node or object renderer's top-left tile position.
	/// @param object_or_node The value to inspect.
	/// @return The top-left tile position, or std::nullopt if the value does not expose top_left_x/top_left_y.
	inline std::optional<MMAPI::Object::Position> GetTopLeftPosition(YYTK::RValue object_or_node)
	{
		if (MMAPI::Engine::StructVariableExists(object_or_node, "node"))
			return GetTopLeftPosition(object_or_node.GetMember("node"));

		if (!MMAPI::Engine::StructVariableExists(object_or_node, "top_left_x") ||
		    !MMAPI::Engine::StructVariableExists(object_or_node, "top_left_y"))
		{
			return std::nullopt;
		}

		return MMAPI::Object::Position{
			static_cast<int>(object_or_node.GetMember("top_left_x").ToInt64()),
			static_cast<int>(object_or_node.GetMember("top_left_y").ToInt64())
		};
	}

	/// Gets a live object renderer instance's top-left tile position.
	/// @param instance The instance to inspect.
	/// @return The top-left tile position, or std::nullopt if the instance does not expose top_left_x/top_left_y.
	inline std::optional<MMAPI::Object::Position> GetTopLeftPosition(YYTK::CInstance* instance)
	{
		if (!instance)
			return std::nullopt;

		return GetTopLeftPosition(instance->ToRValue());
	}

	/// Returns true if the node, object renderer value, or instance has node hitpoints.
	/// @param object_or_node The value to inspect.
	inline bool HasNodeHitpoints(YYTK::RValue object_or_node)
	{
		if (MMAPI::Engine::StructVariableExists(object_or_node, "node"))
			return HasNodeHitpoints(object_or_node.GetMember("node"));

		return MMAPI::Engine::StructVariableExists(object_or_node, "hitpoints");
	}

	/// Returns true if the live object renderer instance has node hitpoints.
	/// @param instance The instance to inspect.
	inline bool HasNodeHitpoints(YYTK::CInstance* instance)
	{
		if (!instance)
			return false;

		return HasNodeHitpoints(instance->ToRValue());
	}

	/// Gets node hitpoints from a node, object renderer value, or instance.
	/// @param object_or_node The value to inspect.
	/// @return The node hitpoints as an RValue, or undefined if hitpoints are unavailable.
	inline YYTK::RValue GetNodeHitpoints(YYTK::RValue object_or_node)
	{
		if (MMAPI::Engine::StructVariableExists(object_or_node, "node"))
			return GetNodeHitpoints(object_or_node.GetMember("node"));

		if (!MMAPI::Engine::StructVariableExists(object_or_node, "hitpoints"))
			return {};

		return object_or_node.GetMember("hitpoints");
	}

	/// Gets node hitpoints from a live object renderer instance.
	/// @param instance The instance to inspect.
	/// @return The node hitpoints as an RValue, or undefined if hitpoints are unavailable.
	inline YYTK::RValue GetNodeHitpoints(YYTK::CInstance* instance)
	{
		if (!instance)
			return {};

		return GetNodeHitpoints(instance->ToRValue());
	}

	/// Sets node hitpoints on a node or object renderer value.
	/// @param object_or_node The value to modify.
	/// @param hitpoints The new hitpoints value.
	/// @return True if hitpoints were found and updated.
	inline bool SetNodeHitpoints(YYTK::RValue object_or_node, int hitpoints)
	{
		if (MMAPI::Engine::StructVariableExists(object_or_node, "node"))
			return SetNodeHitpoints(object_or_node.GetMember("node"), hitpoints);

		if (!MMAPI::Engine::StructVariableExists(object_or_node, "hitpoints"))
			return false;

		*object_or_node.GetRefMember("hitpoints") = hitpoints;
		return true;
	}

	/// Sets node hitpoints on a live object renderer instance.
	/// @param instance The instance to modify.
	/// @param hitpoints The new hitpoints value.
	/// @return True if hitpoints were found and updated.
	inline bool SetNodeHitpoints(YYTK::CInstance* instance, int hitpoints)
	{
		if (!instance)
			return false;

		return SetNodeHitpoints(instance->ToRValue(), hitpoints);
	}

	/// Returns true if the node, object renderer value, or instance has a prototype marked as a ladder candidate.
	/// @param object_or_node The value to inspect.
	inline bool IsLadderCandidate(YYTK::RValue object_or_node)
	{
		if (MMAPI::Engine::StructVariableExists(object_or_node, "node"))
			return IsLadderCandidate(object_or_node.GetMember("node"));

		YYTK::RValue prototype = GetPrototype(object_or_node);
		if (prototype.m_Kind == YYTK::VALUE_UNDEFINED)
			return false;

		if (!MMAPI::Engine::StructVariableExists(prototype, "ladder_candidate"))
			return false;

		return prototype.GetMember("ladder_candidate").ToBoolean();
	}

	/// Returns true if the live object renderer instance has a prototype marked as a ladder candidate.
	/// @param instance The instance to inspect.
	inline bool IsLadderCandidate(YYTK::CInstance* instance)
	{
		if (!instance)
			return false;

		return IsLadderCandidate(instance->ToRValue());
	}

	inline int BeforeFurniturePlacedContext::GetObjectId() const
	{
		YYTK::RValue id = MMAPI::Object::GetObjectId(m_furniture);
		return MMAPI::Engine::IsNumeric(id) ? static_cast<int>(id.ToInt64()) : -1;
	}

	inline std::optional<MMAPI::Object::Position> BeforeFurniturePlacedContext::GetTopLeftPosition() const
	{
		return MMAPI::Object::GetTopLeftPosition(m_furniture);
	}

	inline int FurniturePlacedContext::GetObjectId() const
	{
		YYTK::RValue id = MMAPI::Object::GetObjectId(m_furniture);
		return MMAPI::Engine::IsNumeric(id) ? static_cast<int>(id.ToInt64()) : -1;
	}

	inline std::optional<MMAPI::Object::Position> FurniturePlacedContext::GetTopLeftPosition() const
	{
		return MMAPI::Object::GetTopLeftPosition(m_furniture);
	}

	inline int ObjectErasedContext::GetObjectId() const
	{
		YYTK::RValue id = MMAPI::Object::GetObjectId(m_object);
		return MMAPI::Engine::IsNumeric(id) ? static_cast<int>(id.ToInt64()) : -1;
	}

	inline std::optional<MMAPI::Object::Position> ObjectErasedContext::GetTopLeftPosition() const
	{
		return MMAPI::Object::GetTopLeftPosition(m_object);
	}

	namespace Internal
	{
		inline YYTK::RValue& GmlScriptWriteFurnitureCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			// write_furniture_to_location takes the furniture struct as Arguments[3].
			if (before_furniture_placed_callback && Arguments && ArgumentCount >= 4 && Arguments[3])
			{
				MMAPI::Object::BeforeFurniturePlacedContext context{ *Arguments[3] };
				before_furniture_placed_callback(context);
				if (context.m_cancelled)
					return Result;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_WRITE_FURNITURE_TO_LOCATION)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_furniture_placed_callback
				&& Result.m_Kind != YYTK::VALUE_UNDEFINED
				&& Result.m_Kind != YYTK::VALUE_UNSET)
			{
				MMAPI::Object::FurniturePlacedContext context{ Result };
				after_furniture_placed_callback(context);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterEraseObjectRendererCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_ERASE_OBJECT_RENDERER)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_object_erased_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::Object::ObjectErasedContext context{ *Arguments[0] };
				after_object_erased_callback(context);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptBeforeNodeRendererSetSpriteCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_node_renderer_set_sprite_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				// Pre-resolve the sprite name once for the callback; arg0 can be any asset, so we
				// only attempt the sprite_get_name lookup when it's actually a sprite.
				std::string sprite_name;
				YYTK::RValue asset_type = MMAPI::Internal::module_interface->CallBuiltin(
					"asset_get_type", { *Arguments[0] }
				);
				if (asset_type.ToInt64() == static_cast<int64_t>(MMAPI::Engine::AssetType::Sprite))
				{
					YYTK::RValue name = MMAPI::Internal::module_interface->CallBuiltin(
						"sprite_get_name", { *Arguments[0] }
					);
					if (name.m_Kind == YYTK::VALUE_STRING)
						sprite_name = name.ToString();
				}

				MMAPI::Object::NodeRendererSetSpriteContext context{ Self, Arguments[0], std::move(sprite_name) };
				before_node_renderer_set_sprite_callback(context);

				if (context.m_cancelled)
					return Result;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_NODE_RENDERER_SET_SPRITE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}
	}

	/// Activates Object utility hooks. Eagerly installs the write_furniture_to_location and
	/// erase_object_renderer script hooks used by Hooks::AfterFurniturePlaced and
	/// Hooks::AfterObjectErased. Object's pure utility functions (GetObjectId,
	/// GetTopLeftPosition, etc.) do NOT require Enable() — they work standalone.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Object::Enable() called");

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_WRITE_FURNITURE_TO_LOCATION, reinterpret_cast<PVOID>(Internal::GmlScriptWriteFurnitureCallback) },
			{ Internal::GML_SCRIPT_ERASE_OBJECT_RENDERER,       reinterpret_cast<PVOID>(Internal::GmlScriptAfterEraseObjectRendererCallback) },
			{ Internal::GML_SCRIPT_NODE_RENDERER_SET_SPRITE,    reinterpret_cast<PVOID>(Internal::GmlScriptBeforeNodeRendererSetSpriteCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	namespace Hooks
	{
		/// Registers a callback that runs before the game writes a placed piece of furniture into
		/// its location data — fires for every placement attempt. Use `ctx.Cancel()` to prevent
		/// the placement (e.g. to enforce a max count of a custom object per save). Use
		/// `ctx.GetObjectId()` to filter by object type before deciding whether to cancel.
		/// @param callback A function called with a mutable `MMAPI::Object::BeforeFurniturePlacedContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeFurniturePlaced(Internal::BeforeFurniturePlacedCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Object::Hooks::BeforeFurniturePlaced, MMAPI::Object);

			return MMAPI::Internal::RegisterHook(
				"Object::BeforeFurniturePlaced",
				Internal::before_furniture_placed_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game writes a placed piece of furniture into
		/// its location data (i.e., the player just placed furniture and it has been committed to
		/// the location's persistent state). Use `ctx.GetObjectId()` to filter by object type and
		/// `ctx.GetTopLeftPosition()` to read where it was placed.
		/// @param callback A function called with a `MMAPI::Object::FurniturePlacedContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterFurniturePlaced(Internal::AfterFurniturePlacedCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Object::Hooks::AfterFurniturePlaced, MMAPI::Object);

			return MMAPI::Internal::RegisterHook(
				"Object::AfterFurniturePlaced",
				Internal::after_furniture_placed_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game erases an object renderer (e.g., the
		/// player picks up placed furniture and the game removes its on-screen renderer). Use
		/// `ctx.GetObjectId()` to filter by object type and `ctx.GetTopLeftPosition()` to read
		/// where it was. Pairs with AfterFurniturePlaced for furniture-lifecycle tracking.
		/// @param callback A function called with a `MMAPI::Object::ObjectErasedContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterObjectErased(Internal::AfterObjectErasedCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Object::Hooks::AfterObjectErased, MMAPI::Object);

			return MMAPI::Internal::RegisterHook(
				"Object::AfterObjectErased",
				Internal::after_object_erased_callback,
				callback
			);
		}

		/// Registers a callback that runs before an `obj_node_renderer` instance changes its sprite —
		/// fires every time a crop/forage/flower node (and similar node-rendered objects) wants to
		/// transition to a new sprite (e.g. growth stage advance, harvest, day rollover).
		///
		/// Use `ctx.GetSelf()` for the renderer instance (read `MMAPI::Object::GetNode(self)` for the
		/// underlying node data — prototype, day_count, regrow_cycle, etc.), `ctx.GetSpriteAsset()` for
		/// the sprite the game is about to switch to, `ctx.SetSpriteAsset(...)` to substitute a
		/// different sprite (e.g. an overlay variant), and `ctx.Cancel()` to keep the previous sprite.
		/// @param callback A function called with a mutable `MMAPI::Object::NodeRendererSetSpriteContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeNodeRendererSetSprite(Internal::BeforeNodeRendererSetSpriteCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Object::Hooks::BeforeNodeRendererSetSprite, MMAPI::Object);

			return MMAPI::Internal::RegisterHook(
				"Object::BeforeNodeRendererSetSprite",
				Internal::before_node_renderer_set_sprite_callback,
				callback
			);
		}
	}
}
