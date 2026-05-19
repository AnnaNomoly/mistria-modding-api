// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Engine.hpp"
#include "Hook.hpp"
#include "Location.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include <optional>
#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Pet
{
	/// Source: globalInstance.__pet_kind__
	/// `Sapling` is the internal name for the Friend Shaped perk monster-skin variant —
	/// not a third species peer to Cat/Dog. Sapling color variants (sapling_blue, sapling_cool, etc.)
	/// are palette swaps of the same monster appearance.
	enum class Kinds : int
	{
		Cat     = 0,
		Dog     = 1,
		Sapling = 2
	};

	/// Total number of enumerators in Kinds.
	inline constexpr int KindCount = 3;

	/// Source: globalInstance.__pet_job__
	enum class Jobs : int
	{
		None        = 0,
		Wood        = 1,
		Stone       = 2,
		Forageables = 3
	};

	/// Total number of enumerators in Jobs.
	inline constexpr int JobCount = 4;

	/// Source: globalInstance.__pet_management__
	/// Time-of-day slot the pet is scheduled into.
	enum class Managements : int
	{
		Morning   = 0,
		Afternoon = 1,
		Night     = 2
	};

	/// Total number of enumerators in Managements.
	inline constexpr int ManagementCount = 3;

	/// Source: globalInstance.__pet_initialization__
	/// How the pet entered its current room/state at scene/room start.
	enum class Initializations : int
	{
		Normal       = 0,
		Bed          = 1,
		Held         = 2,
		EatDish      = 3,
		EnterViaDoor = 4,
		DoJob        = 5,
		Cutscene     = 6
	};

	/// Total number of enumerators in Initializations.
	inline constexpr int InitializationCount = 7;

	/// Source: globalInstance.__petting_kind__
	/// What the player is doing when they trigger a pet interaction.
	enum class PettingKinds : int
	{
		Pet    = 0,
		PickUp = 1
	};

	/// Total number of enumerators in PettingKinds.
	inline constexpr int PettingKindCount = 2;

	struct HeartPointsChangedContext
	{
		double m_amount = 0.0;

		double GetAmount() const { return m_amount; }
		void SetAmount(double amount) { m_amount = amount; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		// Pet@Pet bound-method scripts (singleton method calls).
		inline constexpr const char* GML_SCRIPT_ADD_HEART_POINTS   = "gml_Script_add_heart_points@Pet@Pet";
		inline constexpr const char* GML_SCRIPT_FEED               = "gml_Script_feed@Pet@Pet";
		inline constexpr const char* GML_SCRIPT_PET                = "gml_Script_pet@Pet@Pet";
		inline constexpr const char* GML_SCRIPT_UNLOCKED           = "gml_Script_unlocked@Pet@Pet";
		inline constexpr const char* GML_SCRIPT_IS_HOME            = "gml_Script_is_home@Pet@Pet";
		inline constexpr const char* GML_SCRIPT_IS_BABY            = "gml_Script_is_baby@Pet@Pet";
		inline constexpr const char* GML_SCRIPT_PET_KIND           = "gml_Script_pet_kind@Pet@Pet";

		// File-scope scripts (not @Pet@Pet bound methods).
		inline constexpr const char* GML_SCRIPT_PET_ON_ROOM_START  = "gml_Script_pet_on_room_start";
		inline constexpr const char* GML_SCRIPT_PET_UPDATE_AT_TIME = "gml_Script_pet_update_at_time";
		inline constexpr const char* GML_SCRIPT_PET_KIND_UNLOCKED  = "gml_Script_pet_kind_unlocked";

		using BeforeHeartPointsChangeCallback = void(*)(MMAPI::Pet::HeartPointsChangedContext&);
		using AfterPetCallback                = void(*)();
		using AfterFeedCallback               = void(*)();
		using AfterRoomStartCallback          = void(*)();
		using OnTimeUpdateCallback            = void(*)();

		inline BeforeHeartPointsChangeCallback before_heart_points_change_callback = nullptr;
		inline AfterPetCallback                after_pet_callback                  = nullptr;
		inline AfterFeedCallback               after_feed_callback                 = nullptr;
		inline AfterRoomStartCallback          after_room_start_callback           = nullptr;
		inline OnTimeUpdateCallback            on_time_update_callback             = nullptr;

		/// Returns the Pet singleton data struct (`globalInstance.__pet`). Returns undefined when
		/// `__pet` has not been populated (pre-pet-acquisition) — callers must check `m_Kind == VALUE_OBJECT`.
		inline YYTK::RValue GetData()
		{
			return MMAPI::Internal::global_instance->GetMember("__pet");
		}

		/// Invokes a @Pet@Pet bound method on the singleton with no arguments. Used for read-only
		/// queries (unlocked, is_home, is_baby, pet_kind).
		/// @return The script result, or undefined if the pet data struct can't be resolved.
		inline YYTK::RValue InvokePetMethod(const char* script_name)
		{
			YYTK::RValue pet_data = GetData();
			if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
				return {};

			YYTK::CInstance* self = pet_data.ToInstance();
			if (!self)
				return {};

			YYTK::CScript* gml_script = nullptr;
			MMAPI::Internal::module_interface->GetNamedRoutinePointer(
				script_name,
				reinterpret_cast<PVOID*>(&gml_script)
			);
			if (!gml_script)
				return {};

			YYTK::RValue result;
			gml_script->m_Functions->m_ScriptFunction(self, self, result, 0, nullptr);
			return result;
		}

		// Guards numeric arg reads against VALUE_UNDEFINED — required for any add_heart_points-style
		// callback where the game may pass undefined when no delta is being applied (see
		// mmapi_hook_undefined_arg_pattern memory).
		inline bool TryGetNumericArgument(YYTK::RValue** Arguments, int ArgumentCount, int index, double& value)
		{
			if (!Arguments || index < 0 || index >= ArgumentCount || !Arguments[index])
				return false;
			if (!MMAPI::Engine::IsNumeric(*Arguments[index]))
				return false;
			value = Arguments[index]->ToDouble();
			return true;
		}

		inline YYTK::RValue& GmlScriptAddHeartPointsCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			double amount = 0.0;
			bool amount_resolved = TryGetNumericArgument(Arguments, ArgumentCount, 0, amount);
			if (before_heart_points_change_callback && amount_resolved)
			{
				MMAPI::Pet::HeartPointsChangedContext context{ amount };
				before_heart_points_change_callback(context);
				if (context.m_amount != amount)
					*Arguments[0] = context.m_amount;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_ADD_HEART_POINTS)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterPetCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_PET)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_pet_callback)
				after_pet_callback();

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterFeedCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_FEED)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_feed_callback)
				after_feed_callback();

			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterRoomStartCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_PET_ON_ROOM_START)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_room_start_callback)
				after_room_start_callback();

			return Result;
		}

		inline YYTK::RValue& GmlScriptOnTimeUpdateCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_PET_UPDATE_AT_TIME)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (on_time_update_callback)
				on_time_update_callback();

			return Result;
		}
	}

	/// Returns the Pet singleton's data struct (`globalInstance.__pet`).
	/// @return The RValue, or undefined if `__pet` is not yet populated (pre-acquisition).
	inline YYTK::RValue GetData()
	{
		return Internal::GetData();
	}

	/// Returns true if the player has acquired a pet (the pet_arrival cutscene has run).
	/// Wraps `unlocked@Pet@Pet` so this remains correct across patches that re-tune unlock criteria.
	inline bool IsAcquired()
	{
		YYTK::RValue result = Internal::InvokePetMethod(Internal::GML_SCRIPT_UNLOCKED);
		if (result.m_Kind == YYTK::VALUE_UNDEFINED || result.m_Kind == YYTK::VALUE_UNSET)
			return false;
		return result.ToBoolean();
	}

	/// Gets the pet's name (e.g. "Yuki").
	/// @return The name, or std::nullopt if the pet is not acquired.
	inline std::optional<std::string> GetName()
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return std::nullopt;

		YYTK::RValue name = pet_data.GetMember("name");
		if (name.m_Kind != YYTK::VALUE_STRING)
			return std::nullopt;

		return name.ToString();
	}

	/// Gets the pet's variant identifier string (e.g. "dog_white", "cat_calico", "sapling_blue").
	/// Encodes both kind and palette in a single token.
	/// @return The variant, or std::nullopt if the pet is not acquired.
	inline std::optional<std::string> GetVariant()
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return std::nullopt;

		YYTK::RValue variant = pet_data.GetMember("variant");
		if (variant.m_Kind != YYTK::VALUE_STRING)
			return std::nullopt;

		return variant.ToString();
	}

	/// Gets the pet's kind by invoking `pet_kind@Pet@Pet`. Delegates kind derivation to the game
	/// rather than parsing the variant prefix locally, so this stays correct if the game changes
	/// how variant strings map to kinds.
	///
	/// `pet_kind@Pet@Pet` returns the integer index into `__pet_kind__` (consistent with the
	/// `pet_kind_to_string` / `string_to_pet_kind` converter scripts the game also defines).
	/// We accept either numeric or string returns so this remains robust to a patch that swaps
	/// the return shape.
	///
	/// @return The kind, or std::nullopt if the pet is not acquired or the return is out of range.
	inline std::optional<Kinds> GetKind()
	{
		YYTK::RValue result = Internal::InvokePetMethod(Internal::GML_SCRIPT_PET_KIND);

		if (MMAPI::Engine::IsNumeric(result))
		{
			int k = static_cast<int>(result.ToInt64());
			if (k < 0 || k >= KindCount)
				return std::nullopt;
			return static_cast<Kinds>(k);
		}

		if (result.m_Kind == YYTK::VALUE_STRING)
		{
			std::string kind_str = result.ToString();
			if (kind_str == "cat")     return Kinds::Cat;
			if (kind_str == "dog")     return Kinds::Dog;
			if (kind_str == "sapling") return Kinds::Sapling;
		}

		return std::nullopt;
	}

	/// Gets the pet's current heart points.
	/// @return The heart points, or 0 if the pet is not acquired.
	inline int GetHeartPoints()
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return 0;

		return static_cast<int>(pet_data.GetMember("heart_points").ToInt64());
	}

	/// Sets the pet's heart points. Writes the field directly — does not fire BeforeHeartPointsChange.
	inline void SetHeartPoints(int value)
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return;

		MMAPI::Engine::StructVariableSet(pet_data, "heart_points", value);
	}

	/// Adjusts the pet's heart points by the given signed amount. Writes the field directly —
	/// does not fire BeforeHeartPointsChange.
	inline void ModifyHeartPoints(int amount)
	{
		SetHeartPoints(GetHeartPoints() + amount);
	}

	/// Returns the pet's currently assigned job.
	inline Jobs GetJob()
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return Jobs::None;

		return static_cast<Jobs>(static_cast<int>(pet_data.GetMember("job").ToInt64()));
	}

	/// Sets the pet's assigned job.
	inline void SetJob(Jobs job)
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return;

		MMAPI::Engine::StructVariableSet(pet_data, "job", static_cast<int>(job));
	}

	/// Returns true if the pet's assigned job is currently active.
	inline bool IsJobActive()
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return false;

		return pet_data.GetMember("job_active").ToBoolean();
	}

	/// Returns the pet's current management slot (which time-of-day band the pet is scheduled into).
	inline Managements GetManagement()
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return Managements::Morning;

		return static_cast<Managements>(static_cast<int>(pet_data.GetMember("management").ToInt64()));
	}

	/// Sets the pet's management slot.
	inline void SetManagement(Managements management)
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return;

		MMAPI::Engine::StructVariableSet(pet_data, "management", static_cast<int>(management));
	}

	/// Gets the pet's current location by reading `__pet.location_id`. Mirrors the NPC location
	/// pattern — the game updates this as the pet's schedule advances and does not require a live
	/// `obj_pet` instance in the player's current room.
	/// @return The pet's location, or std::nullopt if not acquired or the id is out of range.
	inline std::optional<MMAPI::Location::Ids> GetLocation()
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return std::nullopt;

		YYTK::RValue loc_rv = pet_data.GetMember("location_id");
		if (!MMAPI::Engine::IsNumeric(loc_rv))
			return std::nullopt;

		int location_id = static_cast<int>(loc_rv.ToInt64());
		if (location_id < 0 || location_id >= MMAPI::Location::IdCount)
			return std::nullopt;

		return static_cast<MMAPI::Location::Ids>(location_id);
	}

	/// Returns true if the pet has been patted today.
	inline bool HasBeenPatToday()
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return false;

		return pet_data.GetMember("has_been_pat").ToBoolean();
	}

	/// Sets whether the pet has been patted today.
	inline void SetHasBeenPatToday(bool value)
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return;

		MMAPI::Engine::StructVariableSet(pet_data, "has_been_pat", value);
	}

	/// Returns true if the pet has eaten today.
	inline bool HasEatenToday()
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return false;

		return pet_data.GetMember("has_eaten").ToBoolean();
	}

	/// Sets whether the pet has eaten today.
	inline void SetHasEatenToday(bool value)
	{
		YYTK::RValue pet_data = GetData();
		if (pet_data.m_Kind != YYTK::VALUE_OBJECT)
			return;

		MMAPI::Engine::StructVariableSet(pet_data, "has_eaten", value);
	}

	/// Returns true if the pet is currently *physically* at its home spot (bed/dish position).
	/// Wraps `is_home@Pet@Pet`.
	///
	/// This is a stricter check than `GetLocation() == Location::Ids::PlayerHome`: the location
	/// id is the pet's *scheduled* room (set by the game's schedule logic), while `is_home`
	/// reflects whether the pet has physically reached its home position. The two can disagree
	/// in transit — e.g. scheduled-home but still pathing.
	inline bool IsHome()
	{
		YYTK::RValue result = Internal::InvokePetMethod(Internal::GML_SCRIPT_IS_HOME);
		if (result.m_Kind == YYTK::VALUE_UNDEFINED || result.m_Kind == YYTK::VALUE_UNSET)
			return false;
		return result.ToBoolean();
	}

	/// Returns true if the pet is in its baby stage. Wraps `is_baby@Pet@Pet`.
	inline bool IsBaby()
	{
		YYTK::RValue result = Internal::InvokePetMethod(Internal::GML_SCRIPT_IS_BABY);
		if (result.m_Kind == YYTK::VALUE_UNDEFINED || result.m_Kind == YYTK::VALUE_UNSET)
			return false;
		return result.ToBoolean();
	}

	/// Returns true if the given pet kind has been unlocked for selection. Wraps `pet_kind_unlocked`.
	/// @note `Kinds::Sapling` requires the Friend Shaped perk.
	inline bool IsKindUnlocked(Kinds kind)
	{
		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(
			Internal::GML_SCRIPT_PET_KIND_UNLOCKED,
			reinterpret_cast<PVOID*>(&gml_script)
		);
		if (!gml_script)
			return false;

		YYTK::RValue kind_id = static_cast<int>(kind);
		YYTK::RValue* arg = &kind_id;
		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(nullptr, nullptr, result, 1, &arg);
		return result.ToBoolean();
	}

	/// Activates Pet utility functions. Eagerly installs every Pet script hook used by Hooks::* registrars
	/// (add_heart_points, pet, feed, pet_on_room_start, pet_update_at_time).
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Pet::Enable() called");

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_ADD_HEART_POINTS,   reinterpret_cast<PVOID>(Internal::GmlScriptAddHeartPointsCallback) },
			{ Internal::GML_SCRIPT_PET,                reinterpret_cast<PVOID>(Internal::GmlScriptAfterPetCallback) },
			{ Internal::GML_SCRIPT_FEED,               reinterpret_cast<PVOID>(Internal::GmlScriptAfterFeedCallback) },
			{ Internal::GML_SCRIPT_PET_ON_ROOM_START,  reinterpret_cast<PVOID>(Internal::GmlScriptAfterRoomStartCallback) },
			{ Internal::GML_SCRIPT_PET_UPDATE_AT_TIME, reinterpret_cast<PVOID>(Internal::GmlScriptOnTimeUpdateCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	namespace Hooks
	{
		/// Registers a callback that runs before the game applies a heart-point delta to the pet.
		/// Use `ctx.SetAmount()` to modify the amount.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeHeartPointsChange(Internal::BeforeHeartPointsChangeCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Pet::Hooks::BeforeHeartPointsChange, MMAPI::Pet);

			return MMAPI::Internal::RegisterHook(
				"Pet::BeforeHeartPointsChange",
				Internal::before_heart_points_change_callback,
				callback
			);
		}

		/// Registers a callback that runs after the player pats the pet (`pet@Pet@Pet`).
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterPet(Internal::AfterPetCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Pet::Hooks::AfterPet, MMAPI::Pet);

			return MMAPI::Internal::RegisterHook(
				"Pet::AfterPet",
				Internal::after_pet_callback,
				callback
			);
		}

		/// Registers a callback that runs after the pet has been fed (`feed@Pet@Pet`).
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterFeed(Internal::AfterFeedCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Pet::Hooks::AfterFeed, MMAPI::Pet);

			return MMAPI::Internal::RegisterHook(
				"Pet::AfterFeed",
				Internal::after_feed_callback,
				callback
			);
		}

		/// Registers a callback that runs after `pet_on_room_start` — fires whenever the pet
		/// is set up at the start of a room.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterRoomStart(Internal::AfterRoomStartCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Pet::Hooks::AfterRoomStart, MMAPI::Pet);

			return MMAPI::Internal::RegisterHook(
				"Pet::AfterRoomStart",
				Internal::after_room_start_callback,
				callback
			);
		}

		/// Registers a callback that runs after `pet_update_at_time` — fires on each scheduling tick
		/// that advances the pet's management slot / job state.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status OnTimeUpdate(Internal::OnTimeUpdateCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Pet::Hooks::OnTimeUpdate, MMAPI::Pet);

			return MMAPI::Internal::RegisterHook(
				"Pet::OnTimeUpdate",
				Internal::on_time_update_callback,
				callback
			);
		}
	}
}
