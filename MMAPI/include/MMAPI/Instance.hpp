// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Hook.hpp"
#include "Log.hpp"
#include "Status.hpp"
#include "Weather.hpp"

#include <map>
#include <string>
#include <string_view>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Instance
{
	enum class Objects : int
	{
		// Player
		Ari,

		// World
		AssetObject,
		Bug,
		Door,
		FarmBell,
		Fish,
		WorldFountain,

		// Monsters
		Monster,
		MonsterMimic,

		// Dungeon
		Dragonshrine,
		DungeonElevator,
		DungeonLadderDown,
		DungeonRitualAltar,

		// NPCs (matches NPC::Ids ordering)
		Adeline,
		Balor,
		Caldarus,
		Celine,
		Darcy,
		Dell,
		Dozy,
		Eiland,
		Elsie,
		Errol,
		Hayden,
		Hemlock,
		Henrietta,
		Holt,
		Josephine,
		Juniper,
		Landen,
		Louis,
		Luc,
		Maple,
		March,
		Merri,
		Nora,
		Olric,
		Reina,
		Ryis,
		Seridia,
		Stillwell,
		Taliferro,
		Terithia,
		Valen,
		Vera,
		Wheedle,
		Zorel
	};

	using OnObjectCallCallback = void(*)(YYTK::CInstance* self);

	/// Returns true if the instance's GML object name exactly matches object_name.
	/// @param instance The YYTK instance to check.
	/// @param object_name The exact GML object name to match.
	inline bool IsNamed(YYTK::CInstance* instance, std::string_view object_name)
	{
		return instance &&
		       instance->m_Object &&
		       instance->m_Object->m_Name &&
		       std::string_view(instance->m_Object->m_Name) == object_name;
	}

	struct AttemptInteractContext
	{
		YYTK::CInstance* m_self = nullptr;

		/// The interactable instance currently being interacted with (the script's Self at hook time).
		YYTK::CInstance* GetSelf() const { return m_self; }

		/// Returns the GameMaker object name of the interactable (e.g. "obj_dragonshrine",
		/// "obj_dungeon_elevator"), or an empty view if the instance, its object, or its name pointer
		/// is unavailable. The same guards MMAPI::Instance::IsNamed applies.
		std::string_view GetObjectName() const
		{
			if (!m_self || !m_self->m_Object || !m_self->m_Object->m_Name)
				return {};
			return m_self->m_Object->m_Name;
		}
	};

	struct InteractContext
	{
		YYTK::CInstance* m_target    = nullptr;
		bool             m_cancelled = false;

		/// The instance the player is interacting with (the interact script's Arguments[0] resolved
		/// to a CInstance*).
		YYTK::CInstance* GetTarget() const { return m_target; }

		/// Convenience: the object_id of the target, or -1 if it can't be resolved.
		int GetObjectId() const;

		/// Returns the GameMaker object name of the target (e.g. "obj_world_fountain"), or an
		/// empty view if the instance, its object, or its name pointer is unavailable.
		std::string_view GetObjectName() const
		{
			if (!m_target || !m_target->m_Object || !m_target->m_Object->m_Name)
				return {};
			return m_target->m_Object->m_Name;
		}

		/// Prevents the original interact from running. Use to fully override default interact
		/// behavior with mod-specific handling (e.g. show a confirmation dialog or teleport).
		void Cancel() { m_cancelled = true; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* INSTANCE_OBJ_ARI               = "obj_ari";
		inline constexpr const char* GML_SCRIPT_ATTEMPT_INTERACT    = "gml_Script_attempt_interact@gml_Object_par_interactable_Create_0";
		inline constexpr const char* GML_SCRIPT_INTERACT            = "gml_Script_interact";

		inline constexpr const char* ToObjectName(MMAPI::Instance::Objects obj)
		{
			switch (obj)
			{
				case MMAPI::Instance::Objects::Ari:                return "obj_ari";
				case MMAPI::Instance::Objects::AssetObject:        return "obj_assetobject";
				case MMAPI::Instance::Objects::Bug:                return "obj_bug";
				case MMAPI::Instance::Objects::Door:               return "obj_door";
				case MMAPI::Instance::Objects::FarmBell:           return "obj_farm_bell";
				case MMAPI::Instance::Objects::Fish:               return "obj_fish";
				case MMAPI::Instance::Objects::WorldFountain:      return "obj_world_fountain";
				case MMAPI::Instance::Objects::Monster:            return "obj_monster";
				case MMAPI::Instance::Objects::MonsterMimic:       return "obj_monster_mimic";
				case MMAPI::Instance::Objects::Dragonshrine:       return "obj_dragonshrine";
				case MMAPI::Instance::Objects::DungeonElevator:    return "obj_dungeon_elevator";
				case MMAPI::Instance::Objects::DungeonLadderDown:  return "obj_dungeon_ladder_down";
				case MMAPI::Instance::Objects::DungeonRitualAltar: return "obj_dungeon_ritual_altar";
				case MMAPI::Instance::Objects::Adeline:            return "obj_adeline";
				case MMAPI::Instance::Objects::Balor:              return "obj_balor";
				case MMAPI::Instance::Objects::Caldarus:           return "obj_caldarus";
				case MMAPI::Instance::Objects::Celine:             return "obj_celine";
				case MMAPI::Instance::Objects::Darcy:              return "obj_darcy";
				case MMAPI::Instance::Objects::Dell:               return "obj_dell";
				case MMAPI::Instance::Objects::Dozy:               return "obj_dozy";
				case MMAPI::Instance::Objects::Eiland:             return "obj_eiland";
				case MMAPI::Instance::Objects::Elsie:              return "obj_elsie";
				case MMAPI::Instance::Objects::Errol:              return "obj_errol";
				case MMAPI::Instance::Objects::Hayden:             return "obj_hayden";
				case MMAPI::Instance::Objects::Hemlock:            return "obj_hemlock";
				case MMAPI::Instance::Objects::Henrietta:          return "obj_henrietta";
				case MMAPI::Instance::Objects::Holt:               return "obj_holt";
				case MMAPI::Instance::Objects::Josephine:          return "obj_josephine";
				case MMAPI::Instance::Objects::Juniper:            return "obj_juniper";
				case MMAPI::Instance::Objects::Landen:             return "obj_landen";
				case MMAPI::Instance::Objects::Louis:              return "obj_louis";
				case MMAPI::Instance::Objects::Luc:                return "obj_luc";
				case MMAPI::Instance::Objects::Maple:              return "obj_maple";
				case MMAPI::Instance::Objects::March:              return "obj_march";
				case MMAPI::Instance::Objects::Merri:              return "obj_merri";
				case MMAPI::Instance::Objects::Nora:               return "obj_nora";
				case MMAPI::Instance::Objects::Olric:              return "obj_olric";
				case MMAPI::Instance::Objects::Reina:              return "obj_reina";
				case MMAPI::Instance::Objects::Ryis:               return "obj_ryis";
				case MMAPI::Instance::Objects::Seridia:            return "obj_seridia";
				case MMAPI::Instance::Objects::Stillwell:          return "obj_stillwell";
				case MMAPI::Instance::Objects::Taliferro:          return "obj_taliferro";
				case MMAPI::Instance::Objects::Terithia:           return "obj_terithia";
				case MMAPI::Instance::Objects::Valen:              return "obj_valen";
				case MMAPI::Instance::Objects::Vera:               return "obj_vera";
				case MMAPI::Instance::Objects::Wheedle:            return "obj_wheedle";
				case MMAPI::Instance::Objects::Zorel:              return "obj_zorel";
				default:                                           return nullptr;
			}
		}

		inline std::map<std::string, OnObjectCallCallback> object_call_callbacks;
		inline bool object_dispatcher_installed = false;

		// Set true on the first get_weather fire per session (via MMAPI's internal game-active
		// pub/sub in Core), reset to false on return-to-title (via MMAPI's setup_main_screen
		// pub/sub). Gates user OnObjectCall callbacks so they never run during the load-transition
		// window where instances exist but the world isn't settled — the same window that produces
		// `asset_has_tags(undefined, ...)` crashes from room-context GML scripts. Internal handlers
		// in `internal_object_call_handlers` are NOT gated — MMAPI-internal subscribers are
		// MMAPI-controlled and tolerate pre-active firing.
		inline bool game_active = false;

		inline void OnGameActiveSetFlag()
		{
			game_active = true;
		}

		inline void OnReturnToTitleClearFlag(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			game_active = false;
		}

		// Internal per-object handlers. Multiple modules may subscribe to the same object name
		// (e.g. Player subscribes to obj_ari for use-action edge detection while Telepop's user
		// callback also runs for obj_ari). All internal handlers for a matching object fire
		// before the public user callback, in registration order.
		inline std::map<std::string, std::vector<OnObjectCallCallback>> internal_object_call_handlers;

		inline void RegisterInternalOnObjectCall(const char* object_name, OnObjectCallCallback handler)
		{
			if (!object_name || !handler)
				return;
			auto& handlers = internal_object_call_handlers[object_name];
			for (auto existing : handlers)
				if (existing == handler)
					return;
			handlers.push_back(handler);
		}

		inline bool IsGamePaused()
		{
			return MMAPI::Internal::global_instance->GetRefMember("__pause_status")->m_i64 > 0;
		}

		/// Resolves Ari's GML calling context: Self = the global __ari struct, Other = the live obj_ari instance.
		/// @return True if both pointers were resolved, false if Instance::Enable() hasn't captured a tick yet.
		inline bool TryGetAriContext(YYTK::CInstance*& Self, YYTK::CInstance*& Other)
		{
			const auto& refs = MMAPI::Internal::instance_reference_map;
			if (!refs.contains(INSTANCE_OBJ_ARI))
				return false;
			Self  = MMAPI::Internal::global_instance->GetRefMember("__ari")->ToInstance();
			Other = refs.at(INSTANCE_OBJ_ARI)[0];
			return true;
		}

		inline void ObjectCallbackDispatcher(IN YYTK::FWCodeEvent& CodeEvent)
		{
			auto& [self, other, code, argc, argv] = CodeEvent.Arguments();

			if (!self || !self->m_Object || !self->m_Object->m_Name || IsGamePaused())
				return;

			if (IsNamed(self, "obj_ari") &&
			    !MMAPI::Internal::instance_reference_map.contains(INSTANCE_OBJ_ARI))
			{
				MMAPI::Internal::instance_reference_map[INSTANCE_OBJ_ARI] = { self };
			}

			// Fire all internal handlers whose object name matches, then the single user callback.
			// Internal handlers don't short-circuit each other; the user callback runs at most once.
			//
			// Internal handlers run regardless of game_active — they're MMAPI-controlled and may have
			// reasons to track ticks during the load transition (none currently do, but the
			// architectural seam is preserved). User callbacks are gated on game_active: the load
			// transition window has obj_* instances ticking against an unsettled world, which is
			// strictly garbage state from a mod's perspective — and worse, calling room-context
			// GML helpers there crashes the runner with `asset_has_tags(undefined, ...)`.
			for (const auto& [registered_name, handlers] : internal_object_call_handlers)
			{
				if (IsNamed(self, registered_name))
				{
					for (auto handler : handlers)
						handler(self);
				}
			}

			if (!game_active)
				return;

			for (const auto& [registered_name, callback] : object_call_callbacks)
			{
				if (IsNamed(self, registered_name))
				{
					callback(self);
					return;
				}
			}
		}

		using BeforeAttemptInteractCallback = void(*)(MMAPI::Instance::AttemptInteractContext&);
		using BeforeInteractCallback        = void(*)(MMAPI::Instance::InteractContext&);

		inline BeforeAttemptInteractCallback before_attempt_interact_callback = nullptr;
		inline BeforeInteractCallback        before_interact_callback         = nullptr;

		inline YYTK::RValue& GmlScriptBeforeAttemptInteractCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_attempt_interact_callback)
			{
				MMAPI::Instance::AttemptInteractContext context{ Self };
				before_attempt_interact_callback(context);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_ATTEMPT_INTERACT)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

		inline YYTK::RValue& GmlScriptBeforeInteractCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_interact_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::Instance::InteractContext context;
				context.m_target = Arguments[0]->ToInstance();
				before_interact_callback(context);

				if (context.m_cancelled)
					return Result;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_INTERACT)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}
	}

	inline int InteractContext::GetObjectId() const
	{
		if (!m_target) return -1;
		YYTK::RValue target = m_target->ToRValue();
		if (!MMAPI::Engine::StructVariableExists(target, "object_id")) return -1;
		YYTK::RValue id = target.GetMember("object_id");
		return MMAPI::Engine::IsNumeric(id) ? static_cast<int>(id.ToInt64()) : -1;
	}

	/// Activates the EVENT_OBJECT_CALL dispatcher used by Instance hooks and other modules' Enable(),
	/// plus the par_interactable attempt_interact script hook so registered callbacks fire when the
	/// player attempts to interact with an interactable object.
	/// @return Status::Success if both are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Instance::Enable() called");

		// Cascade to Weather so its get_weather hook fires our game-active handler and its
		// setup_main_screen hook fires our return-to-title handler. Without this cascade the
		// game_active flag stays false forever and user OnObjectCall callbacks never dispatch.
		MMAPI_ENABLE_DEPENDENCY(MMAPI::Instance, MMAPI::Weather);

		MMAPI::Internal::RegisterOnGameActiveHandler(Internal::OnGameActiveSetFlag);
		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::OnReturnToTitleClearFlag);

		if (!Internal::object_dispatcher_installed)
		{
			Aurie::AurieStatus status = MMAPI::Internal::module_interface->CreateCallback(
				MMAPI::Internal::self_module,
				YYTK::EVENT_OBJECT_CALL,
				Internal::ObjectCallbackDispatcher,
				0
			);

			if (!Aurie::AurieSuccess(status))
				return MMAPI::Status::InstallFailed;

			Internal::object_dispatcher_installed = true;
		}

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_ATTEMPT_INTERACT, reinterpret_cast<PVOID>(Internal::GmlScriptBeforeAttemptInteractCallback) },
			{ Internal::GML_SCRIPT_INTERACT,         reinterpret_cast<PVOID>(Internal::GmlScriptBeforeInteractCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	namespace Hooks
	{
		/// Registers a callback that runs once per object tick when the GML object name contains the given fragment.
		/// Skips paused-game ticks automatically.
		/// @param object_name The GML object name fragment to match (e.g. "obj_farm_bell").
		/// @param callback A function called with the live instance on each tick.
		/// @return Status::Success if the callback was registered; Status::AlreadyRegistered if a callback for this name is already registered; otherwise Status::InvalidParameter.
		inline MMAPI::Status OnObjectCall(const char* object_name, MMAPI::Instance::OnObjectCallCallback callback)
		{
			if (!callback || !object_name)
			{
				MMAPI::Log::Warn("RegisterHook(Instance::OnObjectCall): null callback or object_name");
				return MMAPI::Status::InvalidParameter;
			}

			if (Internal::object_call_callbacks.contains(object_name))
			{
				MMAPI::Log::Warn("RegisterHook(Instance::OnObjectCall): already registered for %s", object_name);
				return MMAPI::Status::AlreadyRegistered;
			}

			MMAPI_ENABLE_DEPENDENCY(MMAPI::Instance::Hooks::OnObjectCall, MMAPI::Instance);

			Internal::object_call_callbacks[object_name] = callback;
			MMAPI::Log::Trace("Registered OnObjectCall for: %s", object_name);
			return MMAPI::Status::Success;
		}

		/// Registers a callback that runs once per object tick for the given known game object.
		/// Skips paused-game ticks automatically.
		/// @param object The well-known game object to subscribe to.
		/// @param callback A function called with the live instance on each tick.
		/// @return Status::Success if the callback was registered; Status::AlreadyRegistered if a callback for this object is already registered; otherwise Status::InvalidParameter.
		inline MMAPI::Status OnObjectCall(MMAPI::Instance::Objects object, MMAPI::Instance::OnObjectCallCallback callback)
		{
			return OnObjectCall(Internal::ToObjectName(object), callback);
		}

		/// Registers a callback that runs before the game's `attempt_interact` script — fires
		/// repeatedly while the player is *near* an interactable (the game uses it to decide
		/// whether to show the interact prompt). Use this for proximity-driven side-effects
		/// (e.g. swapping the interact-key localization). Do NOT use it to react to actual
		/// interact presses; for that, use `BeforeInteract` below.
		/// @param callback A function called with a `MMAPI::Instance::AttemptInteractContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeAttemptInteract(Internal::BeforeAttemptInteractCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Instance::Hooks::BeforeAttemptInteract, MMAPI::Instance);

			return MMAPI::Internal::RegisterHook(
				"Instance::BeforeAttemptInteract",
				Internal::before_attempt_interact_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game's `interact` script — fires exactly
		/// once per interact-button press, on the object the player pressed against. Read
		/// `ctx.GetTarget()` for the instance, `ctx.GetObjectId()` / `ctx.GetObjectName()` for
		/// type filtering. Call `ctx.Cancel()` to fully override the game's interact handling
		/// (e.g. show a confirmation dialog or teleport instead of the default behavior).
		///
		/// This is the right hook for "react to the player using my custom object". The sibling
		/// `BeforeAttemptInteract` fires every frame the player is near an interactable, which
		/// would auto-trigger any action-style logic on a per-frame basis.
		///
		/// @param callback A function called with a mutable `MMAPI::Instance::InteractContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeInteract(Internal::BeforeInteractCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Instance::Hooks::BeforeInteract, MMAPI::Instance);

			return MMAPI::Internal::RegisterHook(
				"Instance::BeforeInteract",
				Internal::before_interact_callback,
				callback
			);
		}
	}
}
