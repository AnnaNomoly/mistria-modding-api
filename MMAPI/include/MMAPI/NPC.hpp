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

namespace MMAPI::NPC
{
	/// Source: globalInstance.__npc_id__
	enum class Ids : int
	{
		Adeline   = 0,
		Balor     = 1,
		Caldarus  = 2,
		Celine    = 3,
		Darcy     = 4,
		Dell      = 5,
		Dozy      = 6,
		Eiland    = 7,
		Elsie     = 8,
		Errol     = 9,
		Hayden    = 10,
		Hemlock   = 11,
		Henrietta = 12,
		Holt      = 13,
		Josephine = 14,
		Juniper   = 15,
		Landen    = 16,
		Louis     = 17,
		Luc       = 18,
		Maple     = 19,
		March     = 20,
		Merri     = 21,
		Nora      = 22,
		Olric     = 23,
		Reina     = 24,
		Ryis      = 25,
		Seridia   = 26,
		Stillwell = 27,
		Taliferro = 28,
		Terithia  = 29,
		Valen     = 30,
		Vera      = 31,
		Wheedle   = 32,
		Zorel     = 33
	};

	/// Total number of enumerators in Ids. Iterating [0, IdCount) covers every Ids value.
	inline constexpr int IdCount = 34;

	/// Invokes fn with every Ids value, in ascending order.
	template <typename Fn>
	inline void ForEachId(Fn fn)
	{
		for (int i = 0; i < IdCount; ++i)
			fn(static_cast<Ids>(i));
	}

	struct HeartPointsChangedContext
	{
		double m_amount = 0.0;

		double GetAmount() const { return m_amount; }
		void SetAmount(double amount) { m_amount = amount; }
	};

	struct FindBlipNoiseContext
	{
		std::string m_audio_asset_name;

		/// Returns the audio asset name the game's `find_npc_blip_noise` script resolved
		/// (e.g. "SoundEffects/NPCs/Vocal/TextBlipPriestess").
		std::string_view GetAudioAssetName() const { return m_audio_asset_name; }

		/// Overrides the audio asset name the game will use for this NPC's text blip.
		void SetAudioAssetName(std::string audio_asset_name) { m_audio_asset_name = std::move(audio_asset_name); }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_ADD_HEART_POINTS     = "gml_Script_add_heart_points@Npc@Npc";
		inline constexpr const char* GML_SCRIPT_NPC_RECEIVE_GIFT     = "gml_Script_receive_gift@gml_Object_par_NPC_Create_0";
		inline constexpr const char* GML_SCRIPT_FIND_NPC_BLIP_NOISE  = "gml_Script_find_npc_blip_noise";
		inline constexpr const char* GML_SCRIPT_HEART_LEVEL          = "gml_Script_heart_level@Npc@Npc";

		using BeforeHeartPointsChangeCallback = void(*)(MMAPI::NPC::HeartPointsChangedContext&);
		using BeforeReceiveGiftCallback       = void(*)(YYTK::CInstance* npc, int item_id);
		using AfterFindBlipNoiseCallback      = void(*)(MMAPI::NPC::FindBlipNoiseContext&);

		inline BeforeHeartPointsChangeCallback before_heart_points_change_callback = nullptr;
		inline BeforeReceiveGiftCallback       before_receive_gift_callback        = nullptr;
		inline AfterFindBlipNoiseCallback      after_find_blip_noise_callback      = nullptr;

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
				MMAPI::NPC::HeartPointsChangedContext context{ amount };
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

		inline YYTK::RValue& GmlScriptNpcReceiveGiftCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (Self && Arguments && ArgumentCount == 1 && Arguments[0] && Arguments[0]->m_Kind == YYTK::VALUE_OBJECT)
			{
				int item_id = static_cast<int>(Arguments[0]->GetMember("item_id").ToInt64());
				if (before_receive_gift_callback)
					before_receive_gift_callback(Self, item_id);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(
					MMAPI::Internal::self_module,
					GML_SCRIPT_NPC_RECEIVE_GIFT
				)
			);

			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		inline YYTK::RValue& GmlScriptAfterFindNpcBlipNoiseCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_FIND_NPC_BLIP_NOISE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_find_blip_noise_callback && Result.m_Kind == YYTK::VALUE_STRING)
			{
				MMAPI::NPC::FindBlipNoiseContext context{ Result.ToString() };
				after_find_blip_noise_callback(context);
				Result = YYTK::RValue(context.m_audio_asset_name);
			}

			return Result;
		}

		inline YYTK::RValue GetIdFromInternalName(const std::string& internal_name)
		{
			YYTK::RValue npc_ids = MMAPI::Internal::global_instance->GetMember("__npc_id__");
			size_t npc_count = 0;
			MMAPI::Internal::module_interface->GetArraySize(npc_ids, npc_count);

			for (size_t i = 0; i < npc_count; i++)
			{
				YYTK::RValue* npc_id = nullptr;
				MMAPI::Internal::module_interface->GetArrayEntry(npc_ids, i, npc_id);

				if (npc_id && npc_id->ToString() == internal_name.c_str())
					return static_cast<double>(i);
			}

			return {};
		}

		inline YYTK::RValue GetNpcDatabase()
		{
			return *MMAPI::Internal::global_instance->GetRefMember("__npc_database");
		}

		inline YYTK::RValue GetNpcPrototypes()
		{
			return *MMAPI::Internal::global_instance->GetRefMember("__npc_prototypes");
		}

		inline YYTK::RValue GetNpcPrototype(int npc_id)
		{
			if (npc_id < 0)
				return {};

			YYTK::RValue npc_prototypes = GetNpcPrototypes();
			if (npc_prototypes.m_Kind == YYTK::VALUE_UNDEFINED)
				return {};

			size_t npc_count = 0;
			MMAPI::Internal::module_interface->GetArraySize(npc_prototypes, npc_count);
			if (static_cast<size_t>(npc_id) >= npc_count)
				return {};

			YYTK::RValue* npc_prototype = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(npc_prototypes, static_cast<size_t>(npc_id), npc_prototype);
			if (!npc_prototype)
				return {};

			return *npc_prototype;
		}

		inline YYTK::RValue GetData(int npc_id)
		{
			if (npc_id < 0)
				return {};

			YYTK::RValue npc_database = GetNpcDatabase();
			if (npc_database.m_Kind == YYTK::VALUE_UNDEFINED)
				return {};

			size_t npc_count = 0;
			MMAPI::Internal::module_interface->GetArraySize(npc_database, npc_count);
			if (static_cast<size_t>(npc_id) >= npc_count)
				return {};

			YYTK::RValue* npc = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(npc_database, static_cast<size_t>(npc_id), npc);
			if (!npc)
				return {};

			return *npc;
		}

		inline YYTK::RValue GetGiftBuffer(int npc_id, const char* gift_member)
		{
			YYTK::RValue npc_prototype = GetNpcPrototype(npc_id);
			if (npc_prototype.m_Kind == YYTK::VALUE_UNDEFINED)
				return {};

			if (!MMAPI::Engine::StructVariableExists(npc_prototype, gift_member))
				return {};

			YYTK::RValue gifts = npc_prototype.GetMember(gift_member);
			if (!MMAPI::Engine::StructVariableExists(gifts, "__buffer"))
				return {};

			return gifts.GetMember("__buffer");
		}

		inline bool GiftBufferContains(YYTK::RValue gift_buffer, int item_id)
		{
			if (gift_buffer.m_Kind == YYTK::VALUE_UNDEFINED)
				return false;

			size_t gift_count = 0;
			MMAPI::Internal::module_interface->GetArraySize(gift_buffer, gift_count);

			for (size_t i = 0; i < gift_count; i++)
			{
				YYTK::RValue* gift = nullptr;
				MMAPI::Internal::module_interface->GetArrayEntry(gift_buffer, i, gift);

				if (gift && gift->ToInt64() == item_id)
					return true;
			}

			return false;
		}

		inline YYTK::RValue GetKnownGiftPreferences(YYTK::CInstance* npc)
		{
			if (!npc)
				return {};

			YYTK::RValue npc_value = npc->ToRValue();
			if (!MMAPI::Engine::StructVariableExists(npc_value, "me"))
				return {};

			YYTK::RValue me = npc_value.GetMember("me");
			if (!MMAPI::Engine::StructVariableExists(me, "known_gift_preferences"))
				return {};

			YYTK::RValue known_gift_preferences = me.GetMember("known_gift_preferences");
			if (!MMAPI::Engine::StructVariableExists(known_gift_preferences, "inner"))
				return {};

			return known_gift_preferences.GetMember("inner");
		}
	}

	/// Gets the internal numeric ID for an NPC.
	/// @param npc The NPC to convert.
	/// @return The NPC's internal numeric ID.
	inline int GetId(MMAPI::NPC::Ids npc)
	{
		return static_cast<int>(npc);
	}

	/// Resolves an NPC's game-internal name string (e.g. "adeline", "balor") by reading
	/// `globalInstance.__npc_id__[id]`.
	/// @param npc The NPC to resolve.
	/// @return The internal name as a string, or empty if the id is out of bounds.
	inline std::string GetInternalName(MMAPI::NPC::Ids npc)
	{
		YYTK::RValue npc_ids = MMAPI::Internal::global_instance->GetMember("__npc_id__");
		size_t npc_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(npc_ids, npc_count);

		int npc_id = static_cast<int>(npc);
		if (npc_id < 0 || npc_id >= static_cast<int>(npc_count))
			return {};

		YYTK::RValue* internal_name = nullptr;
		MMAPI::Internal::module_interface->GetArrayEntry(npc_ids, npc_id, internal_name);
		if (!internal_name || internal_name->m_Kind != YYTK::VALUE_STRING)
			return {};

		return internal_name->ToString();
	}

	/// Resolves an NPC::Ids from its game-internal name string. Useful for mods that persist NPC
	/// references by name (e.g. in JSON mod-save files) and need to round-trip back to the enum
	/// for the heart-points / data helpers.
	/// @param internal_name The game-internal NPC name (e.g. "adeline", "balor").
	/// @return The NPC::Ids enum value, or std::nullopt if no NPC matches.
	inline std::optional<MMAPI::NPC::Ids> TryFromInternalName(const std::string& internal_name)
	{
		YYTK::RValue id = Internal::GetIdFromInternalName(internal_name);
		if (!MMAPI::Engine::IsNumeric(id))
			return std::nullopt;
		return static_cast<MMAPI::NPC::Ids>(id.ToInt64());
	}

	/// Gets the NPC database entry for an NPC.
	/// @param npc The NPC to read.
	/// @return The NPC data struct as an RValue, or undefined if the ID is out of bounds.
	inline YYTK::RValue GetData(MMAPI::NPC::Ids npc)
	{
		return Internal::GetData(GetId(npc));
	}

	/// Gets the NPC's current location by reading
	/// `globalInstance.__npc_database[id].location_position.location_id`. This is the location
	/// the game considers the NPC to be in for schedule/itinerary purposes — it updates as the
	/// game advances the NPC's schedule and does not require a live `obj_npc` instance in the
	/// player's current room.
	/// @param npc The NPC to read.
	/// @return The NPC's current location, or std::nullopt if the npc data or location id can't
	///         be resolved (e.g. before the game has populated the NPC database).
	inline std::optional<MMAPI::Location::Ids> GetLocation(MMAPI::NPC::Ids npc)
	{
		YYTK::RValue npc_data = GetData(npc);
		if (npc_data.m_Kind != YYTK::VALUE_OBJECT)
			return std::nullopt;

		YYTK::RValue location_position = npc_data.GetMember("location_position");
		if (location_position.m_Kind != YYTK::VALUE_OBJECT)
			return std::nullopt;

		YYTK::RValue location_id_rv = location_position.GetMember("location_id");
		if (!MMAPI::Engine::IsNumeric(location_id_rv))
			return std::nullopt;

		int location_id = static_cast<int>(location_id_rv.ToInt64());
		if (location_id < 0 || location_id >= MMAPI::Location::IdCount)
			return std::nullopt;

		return static_cast<MMAPI::Location::Ids>(location_id);
	}

	/// Returns true if the NPC's current location matches the given location.
	/// @param npc The NPC to check.
	/// @param location The location to compare against.
	inline bool IsAtLocation(MMAPI::NPC::Ids npc, MMAPI::Location::Ids location)
	{
		auto current_location = GetLocation(npc);
		return current_location.has_value() && *current_location == location;
	}

	/// Gets the item IDs this NPC likes as gifts.
	/// @param npc The NPC to read.
	/// @return The liked gift item ID buffer as an RValue, or undefined if the NPC is not found.
	inline YYTK::RValue GetLikedGifts(MMAPI::NPC::Ids npc)
	{
		return Internal::GetGiftBuffer(GetId(npc), "liked_gifts");
	}

	/// Gets the item IDs this NPC loves as gifts.
	/// @param npc The NPC to read.
	/// @return The loved gift item ID buffer as an RValue, or undefined if the NPC is not found.
	inline YYTK::RValue GetLovedGifts(MMAPI::NPC::Ids npc)
	{
		return Internal::GetGiftBuffer(GetId(npc), "loved_gifts");
	}

	/// Returns true if the NPC likes the given item as a gift.
	/// @param npc The NPC to check.
	/// @param item_id The item ID to check.
	inline bool LikesGift(MMAPI::NPC::Ids npc, int item_id)
	{
		return Internal::GiftBufferContains(GetLikedGifts(npc), item_id);
	}

	/// Returns true if the NPC loves the given item as a gift.
	/// @param npc The NPC to check.
	/// @param item_id The item ID to check.
	inline bool LovesGift(MMAPI::NPC::Ids npc, int item_id)
	{
		return Internal::GiftBufferContains(GetLovedGifts(npc), item_id);
	}

	/// Returns true if the live NPC instance knows the given gift preference.
	/// @param npc The live NPC instance.
	/// @param item_id The item ID to check.
	inline bool KnowsGiftPreference(YYTK::CInstance* npc, int item_id)
	{
		YYTK::RValue known_gift_preferences = Internal::GetKnownGiftPreferences(npc);
		if (known_gift_preferences.m_Kind == YYTK::VALUE_UNDEFINED)
			return false;

		return MMAPI::Internal::module_interface->CallBuiltin("struct_exists", { known_gift_preferences, item_id }).ToBoolean();
	}

	/// Marks the given gift preference as learned on the live NPC instance.
	/// @param npc The live NPC instance.
	/// @param item_id The item ID to mark as learned.
	inline void LearnGiftPreference(YYTK::CInstance* npc, int item_id)
	{
		YYTK::RValue known_gift_preferences = Internal::GetKnownGiftPreferences(npc);
		if (known_gift_preferences.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		MMAPI::Engine::StructVariableSet(known_gift_preferences, item_id, 0.0);
	}

	/// Returns true if the NPC is currently able to receive a gift today.
	/// Reads `gift_flag` on the NPC's `__npc_database` entry — the game clears this when
	/// the NPC has already accepted their daily gift.
	/// @param npc The NPC to check.
	inline bool IsGiftable(MMAPI::NPC::Ids npc)
	{
		YYTK::RValue npc_data = GetData(npc);
		if (npc_data.m_Kind != YYTK::VALUE_OBJECT)
			return false;
		if (!MMAPI::Engine::StructVariableExists(npc_data, "gift_flag"))
			return false;
		return npc_data.GetMember("gift_flag").ToBoolean();
	}

	/// Returns the NPC's current heart level (0-10) by invoking the game's bound
	/// `heart_level@Npc@Npc` script on the NPC's `__npc_database` struct (each entry doubles as a
	/// callable instance with its method-script pointers populated). Defers to the game's own
	/// heart-points → heart-level ladder, so this stays correct across patches that re-tune the
	/// thresholds.
	/// @param npc The NPC to read.
	/// @return The heart level (0-10), or 0 if the NPC database entry can't be resolved.
	inline int GetHeartLevel(MMAPI::NPC::Ids npc)
	{
		YYTK::RValue npc_data = GetData(npc);
		if (npc_data.m_Kind != YYTK::VALUE_OBJECT)
			return 0;

		YYTK::CInstance* self = npc_data.ToInstance();
		if (!self)
			return 0;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(
			Internal::GML_SCRIPT_HEART_LEVEL,
			reinterpret_cast<PVOID*>(&gml_script)
		);
		if (!gml_script)
			return 0;

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(self, self, result, 0, nullptr);
		if (!MMAPI::Engine::IsNumeric(result))
			return 0;

		return static_cast<int>(result.ToInt64());
	}

	/// Gets an NPC's heart points.
	/// @param npc The NPC to read.
	/// @return The NPC's heart points, or 0 if the NPC is not found.
	inline int GetHeartPoints(MMAPI::NPC::Ids npc)
	{
		YYTK::RValue npc_data = GetData(npc);
		if (npc_data.m_Kind == YYTK::VALUE_UNDEFINED)
			return 0;

		return static_cast<int>(npc_data.GetMember("heart_points").ToInt64());
	}

	/// Sets an NPC's heart points.
	/// @param npc The NPC to modify.
	/// @param value The heart point value to assign.
	inline void SetHeartPoints(MMAPI::NPC::Ids npc, int value)
	{
		YYTK::RValue npc_data = GetData(npc);
		if (npc_data.m_Kind == YYTK::VALUE_UNDEFINED)
			return;

		MMAPI::Engine::StructVariableSet(npc_data, "heart_points", value);
	}

	/// Adjusts an NPC's heart points.
	/// @param npc The NPC to modify.
	/// @param amount The signed amount to add to the NPC's heart points.
	inline void ModifyHeartPoints(MMAPI::NPC::Ids npc, int amount)
	{
		SetHeartPoints(npc, GetHeartPoints(npc) + amount);
	}

	/// Activates NPC utility functions. Eagerly installs every NPC script hook used by Hooks::* registrars
	/// (add_heart_points, receive_gift, find_npc_blip_noise).
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::NPC::Enable() called");

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_ADD_HEART_POINTS,    reinterpret_cast<PVOID>(Internal::GmlScriptAddHeartPointsCallback) },
			{ Internal::GML_SCRIPT_NPC_RECEIVE_GIFT,    reinterpret_cast<PVOID>(Internal::GmlScriptNpcReceiveGiftCallback) },
			{ Internal::GML_SCRIPT_FIND_NPC_BLIP_NOISE, reinterpret_cast<PVOID>(Internal::GmlScriptAfterFindNpcBlipNoiseCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	namespace Hooks
	{
		/// Registers a callback that runs when an NPC's heart points change through the game's add heart points script.
		/// @param callback A function called with a mutable heart point change context. Use ctx.SetAmount() to modify the amount applied.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeHeartPointsChange(Internal::BeforeHeartPointsChangeCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::NPC::Hooks::BeforeHeartPointsChange, MMAPI::NPC);

			return MMAPI::Internal::RegisterHook(
				"NPC::BeforeHeartPointsChange",
				Internal::before_heart_points_change_callback,
				callback
			);
		}

		/// Registers a callback that runs when an NPC receives a gift.
		/// @param callback A function called with the live NPC instance and received item ID.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeReceiveGift(Internal::BeforeReceiveGiftCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::NPC::Hooks::BeforeReceiveGift, MMAPI::NPC);

			return MMAPI::Internal::RegisterHook(
				"NPC::BeforeReceiveGift",
				Internal::before_receive_gift_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game's `find_npc_blip_noise` script.
		/// Read `ctx.GetAudioAssetName()` to see the audio asset the game resolved for an NPC's
		/// text-blip noise, and call `ctx.SetAudioAssetName(...)` to override it (e.g. retarget
		/// a character to a different voice asset under specific conditions).
		/// @param callback A function called with a mutable `MMAPI::NPC::FindBlipNoiseContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterFindBlipNoise(Internal::AfterFindBlipNoiseCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::NPC::Hooks::AfterFindBlipNoise, MMAPI::NPC);

			return MMAPI::Internal::RegisterHook(
				"NPC::AfterFindBlipNoise",
				Internal::after_find_blip_noise_callback,
				callback
			);
		}
	}
}
