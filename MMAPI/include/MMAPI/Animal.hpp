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

namespace MMAPI::Animal
{
	/// Source: globalInstance.__animal_xp
	/// Struct-backed enum; values follow alphabetical key order.
	enum class XpValues : int
	{
		Breed     = 0,
		Feed      = 1,
		GainHeart = 2,
		GoOutside = 3,
		Pet       = 4
	};

	/// Total number of enumerators in XpValues. Iterating [0, XpValueCount) covers every XpValues value.
	inline constexpr int XpValueCount = 5;

	/// Invokes fn with every XpValues value, in ascending order.
	template <typename Fn>
	inline void ForEachXpValue(Fn fn)
	{
		for (int i = 0; i < XpValueCount; ++i)
			fn(static_cast<XpValues>(i));
	}

	struct HeartPointsChangedContext
	{
		double m_amount = 0.0;

		double GetAmount() const { return m_amount; }
		void SetAmount(double amount) { m_amount = amount; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_ADD_HEART_POINTS = "gml_Script_add_heart_points@PlayerAnimal@Animal";
		inline constexpr const char* GML_SCRIPT_GET_ALL_ANIMALS  = "gml_Script_get_all_animals";
		inline constexpr const char* GML_SCRIPT_PUT_DOWN         = "gml_Script_put_down@gml_Object_obj_player_animal_Create_0";
		inline constexpr const char* GML_SCRIPT_ON_PET           = "gml_Script_on_pet@gml_Object_obj_player_animal_Create_0";

		using BeforeHeartPointsChangeCallback = void(*)(MMAPI::Animal::HeartPointsChangedContext&);
		inline BeforeHeartPointsChangeCallback before_heart_points_change_callback = nullptr;

		inline bool TryGetNumericArgument(YYTK::RValue** Arguments, int ArgumentCount, int index, double& value)
		{
			if (!Arguments || index < 0 || index >= ArgumentCount || !Arguments[index])
				return false;

			if (Arguments[index]->m_Kind != YYTK::VALUE_REAL &&
			    Arguments[index]->m_Kind != YYTK::VALUE_INT32 &&
			    Arguments[index]->m_Kind != YYTK::VALUE_INT64)
				return false;

			value = Arguments[index]->ToDouble();
			return true;
		}

		inline void SetNumericArgument(YYTK::RValue** Arguments, int index, double value)
		{
			*Arguments[index] = value;
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
			if (before_heart_points_change_callback && TryGetNumericArgument(Arguments, ArgumentCount, 0, amount))
			{
				MMAPI::Animal::HeartPointsChangedContext context{ amount };
				before_heart_points_change_callback(context);
				SetNumericArgument(Arguments, 0, context.m_amount);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(
					MMAPI::Internal::self_module,
					GML_SCRIPT_ADD_HEART_POINTS
				)
			);

			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		using AfterPutDownCallback = void(*)(YYTK::CInstance* animal);
		using AfterPetCallback     = void(*)(YYTK::CInstance* animal);

		inline AfterPutDownCallback after_put_down_callback = nullptr;
		inline AfterPetCallback     after_pet_callback      = nullptr;

		inline YYTK::RValue& GmlScriptPutDownCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_PUT_DOWN)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_put_down_callback)
				after_put_down_callback(Self);

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
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_ON_PET)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_pet_callback)
				after_pet_callback(Self);

			return Result;
		}

		inline constexpr const char* GML_SCRIPT_BELL_IN         = "gml_Script_bell_in@gml_Object_obj_farm_bell_Create_0";
		inline constexpr const char* GML_SCRIPT_BELL_OUT        = "gml_Script_bell_out@gml_Object_obj_farm_bell_Create_0";
		inline constexpr const char* GML_SCRIPT_CREATE_ANIMAL_CURRENCY_DANCE =
			"gml_Script_create_animal_currency_dance@gml_Object_obj_player_animal_Create_0";

		inline void SpawnShinyBeads(YYTK::CInstance* Self, YYTK::CInstance* Other, int amount)
		{
			YYTK::CScript* gml_script = nullptr;
			MMAPI::Internal::module_interface->GetNamedRoutinePointer(
				GML_SCRIPT_CREATE_ANIMAL_CURRENCY_DANCE,
				reinterpret_cast<PVOID*>(&gml_script)
			);

			YYTK::RValue num_beads = amount;
			YYTK::RValue spawn_beads = true;
			YYTK::RValue result;
			YYTK::RValue* args[2] = { &num_beads, &spawn_beads };
			gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 2, args);
		}

		inline constexpr const char* ToGameKey(MMAPI::Animal::XpValues value)
		{
			switch (value)
			{
				case MMAPI::Animal::XpValues::Breed:
					return "breed";
				case MMAPI::Animal::XpValues::Feed:
					return "feed";
				case MMAPI::Animal::XpValues::GainHeart:
					return "gain_heart";
				case MMAPI::Animal::XpValues::GoOutside:
					return "go_outside";
				case MMAPI::Animal::XpValues::Pet:
					return "pet";
				default:
					return nullptr;
			}
		}
	}

	/// Activates Animal utility functions. Eagerly installs every Animal script hook used by Hooks::*
	/// registrars (add_heart_points, put_down, on_pet). Idempotent — safe to call multiple times.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Animal::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Animal, MMAPI::Instance);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_ADD_HEART_POINTS, reinterpret_cast<PVOID>(Internal::GmlScriptAddHeartPointsCallback) },
			{ Internal::GML_SCRIPT_PUT_DOWN,         reinterpret_cast<PVOID>(Internal::GmlScriptPutDownCallback) },
			{ Internal::GML_SCRIPT_ON_PET,           reinterpret_cast<PVOID>(Internal::GmlScriptAfterPetCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Returns the game's array-like collection of all player animals.
	/// @attention Requires MMAPI::Animal::Enable() to have been called.
	/// @return The animal collection as an RValue, or an empty RValue if the required context is unavailable.
	inline YYTK::RValue GetAllAnimals()
	{
		MMAPI_REQUIRE_ENABLED("Animal", {});

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_GET_ALL_ANIMALS, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::CInstance* Self = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return {};

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		return result;
	}

	/// Returns the number of player animals.
	/// @attention Requires MMAPI::Animal::Enable() to have been called.
	/// @return The animal count, or 0 if the required context is unavailable.
	inline int GetAnimalCount()
	{
		YYTK::RValue all_animals = GetAllAnimals();
		if (all_animals.m_Kind != YYTK::VALUE_OBJECT)
			return 0;

		YYTK::RValue buffer = all_animals.GetMember("__buffer");
		if (buffer.m_Kind != YYTK::VALUE_ARRAY)
			return 0;

		size_t size = 0;
		MMAPI::Internal::module_interface->GetArraySize(buffer, size);
		return static_cast<int>(size);
	}

	/// Invokes fn with every live player animal, in spawn order. The argument is the live animal
	/// struct (`YYTK::RValue&`) — mutations via MMAPI::Animal accessors are visible to the game.
	/// @attention Requires MMAPI::Animal::Enable() to have been called.
	template <typename Fn>
	inline void ForEachAnimal(Fn fn)
	{
		YYTK::RValue all_animals = GetAllAnimals();
		if (all_animals.m_Kind != YYTK::VALUE_OBJECT)
			return;

		YYTK::RValue buffer = all_animals.GetMember("__buffer");
		if (buffer.m_Kind != YYTK::VALUE_ARRAY)
			return;

		size_t size = 0;
		MMAPI::Internal::module_interface->GetArraySize(buffer, size);

		for (size_t i = 0; i < size; ++i)
		{
			YYTK::RValue* entry = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(buffer, i, entry);
			if (!entry)
				continue;
			fn(*entry);
		}
	}

	/// Gets an animal XP value from MMAPI::Internal::global_instance.__animal_xp.
	/// @param xp_value The animal XP value to read.
	/// @return The XP value as an RValue, or undefined if unavailable.
	inline YYTK::RValue GetXpValue(MMAPI::Animal::XpValues xp_value)
	{
		const char* xp_value_key = Internal::ToGameKey(xp_value);
		if (!xp_value_key)
			return {};

		YYTK::RValue animal_xp = MMAPI::Internal::global_instance->GetMember("__animal_xp");
		if (animal_xp.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		return animal_xp.GetMember(xp_value_key);
	}

	/// Gets an animal XP value, falling back to a caller-provided default if unavailable.
	/// @param xp_value The animal XP value to read.
	/// @param default_value The value to return if the game data cannot be read.
	/// @return The animal XP value, or default_value if unavailable.
	inline int GetXpValueOrDefault(MMAPI::Animal::XpValues xp_value, int default_value)
	{
		YYTK::RValue xp = GetXpValue(xp_value);
		if (xp.m_Kind == YYTK::VALUE_UNDEFINED)
			return default_value;

		return static_cast<int>(xp.ToInt64());
	}

	/// Returns true if the animal has been pet today.
	inline bool HasBeenPet(YYTK::RValue animal)
	{
		return MMAPI::Engine::StructVariableGet(animal, "has_been_pat").ToBoolean();
	}

	/// Sets whether the animal has been pet today.
	inline void SetHasBeenPet(YYTK::RValue animal, bool value)
	{
		MMAPI::Engine::StructVariableSet(animal, "has_been_pat", value);
	}

	/// Returns true if the animal has eaten today.
	inline bool HasEaten(YYTK::RValue animal)
	{
		return MMAPI::Engine::StructVariableGet(animal, "has_eaten").ToBoolean();
	}

	/// Sets whether the animal has eaten today.
	inline void SetHasEaten(YYTK::RValue animal, bool value)
	{
		MMAPI::Engine::StructVariableSet(animal, "has_eaten", value);
	}

	/// Returns the animal's current heart points.
	inline int GetHeartPoints(YYTK::RValue animal)
	{
		return static_cast<int>(MMAPI::Engine::StructVariableGet(animal, "heart_points").ToInt64());
	}

	/// Sets the animal's heart points.
	inline void SetHeartPoints(YYTK::RValue animal, int value)
	{
		MMAPI::Engine::StructVariableSet(animal, "heart_points", value);
	}

	/// Adjusts the animal's heart points by the given signed amount.
	inline void ModifyHeartPoints(YYTK::RValue animal, int amount)
	{
		SetHeartPoints(animal, GetHeartPoints(animal) + amount);
	}

	/// Rings a farm bell to bring animals inside.
	/// @param farm_bell The obj_farm_bell instance to ring.
	inline void RingBellIn(YYTK::CInstance* farm_bell)
	{
		if (!farm_bell)
			return;

		YYTK::RValue bell = farm_bell->ToRValue();
		if (!MMAPI::Engine::StructVariableExists(bell, "bell_in"))
			return;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_BELL_IN, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(farm_bell, farm_bell, result, 0, nullptr);
	}

	/// Rings a farm bell to send animals outside.
	/// @param farm_bell The obj_farm_bell instance to ring.
	inline void RingBellOut(YYTK::CInstance* farm_bell)
	{
		if (!farm_bell)
			return;

		YYTK::RValue bell = farm_bell->ToRValue();
		if (!MMAPI::Engine::StructVariableExists(bell, "bell_out"))
			return;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_BELL_OUT, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(farm_bell, farm_bell, result, 0, nullptr);
	}

	/// Spawns shiny beads from an animal interaction.
	/// @attention Requires MMAPI::Animal::Enable() to have been called.
	/// @param animal The live animal instance to spawn beads from.
	/// @param amount The number of beads to spawn. Clamped to [1, 999].
	inline void SpawnShinyBeads(YYTK::CInstance* animal, int amount)
	{
		MMAPI_REQUIRE_ENABLED_VOID("Animal");

		YYTK::CInstance* ari_struct = nullptr;
		YYTK::CInstance* ari_instance = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(ari_struct, ari_instance))
			return;

		if (amount <= 0)
			amount = 1;
		if (amount > 999)
			amount = 999;

		Internal::SpawnShinyBeads(animal, ari_instance, amount);
	}

	namespace Hooks
	{
		/// Registers a callback that runs before the game applies a heart-point delta to an animal.
		/// @param callback A function called with a mutable context. Use ctx.SetAmount() to modify the amount.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeHeartPointsChange(Internal::BeforeHeartPointsChangeCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Animal::Hooks::BeforeHeartPointsChange, MMAPI::Animal);

			return MMAPI::Internal::RegisterHook(
				"Animal::BeforeHeartPointsChange",
				Internal::before_heart_points_change_callback,
				callback
			);
		}

		/// Registers a callback that runs after the player puts an animal down.
		/// @param callback A function called with the live player animal instance.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterPutDown(Internal::AfterPutDownCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Animal::Hooks::AfterPutDown, MMAPI::Animal);

			return MMAPI::Internal::RegisterHook(
				"Animal::AfterPutDown",
				Internal::after_put_down_callback,
				callback
			);
		}

		/// Registers a callback that runs after the player pets an animal.
		/// @param callback A function called with the live player animal instance.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterPet(Internal::AfterPetCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Animal::Hooks::AfterPet, MMAPI::Animal);

			return MMAPI::Internal::RegisterHook(
				"Animal::AfterPet",
				Internal::after_pet_callback,
				callback
			);
		}
	}
}
