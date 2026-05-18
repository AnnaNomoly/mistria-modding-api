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
#include <unordered_map>

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

	/// Context for the `BeforeActivityChange` hook. Mods inspect which NPC is about to be
	/// assigned which activity, and can choose to:
	///   - **Allow** (default): let the routine's chosen activity proceed unmodified.
	///   - **Deny**: skip the activity assignment entirely; the activity_handler is left in its
	///     current state. Use this when an NPC should be paused at whatever they're doing
	///     (note: the routine may immediately try again on the next tick, so a persistent Deny
	///     may need a `SetActivityOverride` or a per-tick callback that keeps re-denying).
	///   - **Substitute**: replace the activity to assign with a different one. Use this for
	///     "Reina is busy with mod content" — pin her to a specific activity instead.
	///
	/// Decisions stack only with the override map: the override map's decision (if any) is
	/// applied first, then this callback runs and can change it. The most recent action wins.
	struct BeforeActivityChangeContext
	{
		enum class Decision { Allow, Deny, Substitute };

		MMAPI::NPC::Ids m_npc             = static_cast<MMAPI::NPC::Ids>(0);
		std::string     m_activity_name;
		Decision        m_decision        = Decision::Allow;
		std::string     m_substitute_name;

		/// The NPC whose activity is about to change.
		MMAPI::NPC::Ids GetNpc() const { return m_npc; }

		/// The activity's internal name (e.g. "narrows_workout") the routine wants to assign.
		/// Empty if begin_activity was called with no string argument (rare).
		std::string_view GetActivityName() const { return m_activity_name; }

		/// Let the activity proceed unmodified. Default; explicit Allow() is only needed to undo
		/// a prior Deny/Substitute on the same context within a callback.
		void Allow() { m_decision = Decision::Allow; }

		/// Skip the activity assignment entirely. The original begin_activity is not called.
		void Deny()  { m_decision = Decision::Deny; }

		/// Substitute the activity with a different one by name (e.g. "study"). The name must be
		/// a valid member of `globalInstance.__activity__` — names aren't validated at substitution
		/// time, so an invalid name will cause the game to fail downstream lookups.
		void Substitute(const std::string& activity_name) { m_decision = Decision::Substitute; m_substitute_name = activity_name; }

		Decision         GetDecision() const { return m_decision; }
		std::string_view GetSubstituteName() const { return m_substitute_name; }
	};

	/// Context for the `AfterActivityChange` hook. Fires after `begin_activity` has run — read
	/// `GetActivityName()` to see what activity actually got assigned (which may differ from the
	/// BeforeActivityChange input if a Substitute fired).
	struct AfterActivityChangeContext
	{
		MMAPI::NPC::Ids m_npc = static_cast<MMAPI::NPC::Ids>(0);
		std::string     m_activity_name;

		MMAPI::NPC::Ids  GetNpc() const { return m_npc; }
		std::string_view GetActivityName() const { return m_activity_name; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_ADD_HEART_POINTS     = "gml_Script_add_heart_points@Npc@Npc";
		inline constexpr const char* GML_SCRIPT_NPC_RECEIVE_GIFT     = "gml_Script_receive_gift@gml_Object_par_NPC_Create_0";
		inline constexpr const char* GML_SCRIPT_FIND_NPC_BLIP_NOISE  = "gml_Script_find_npc_blip_noise";
		inline constexpr const char* GML_SCRIPT_HEART_LEVEL          = "gml_Script_heart_level@Npc@Npc";
		// `request_activity` is the entry point that selects + stages the next activity (writes
		// Self.activity and Self.request). Hooking `begin_activity` is too late — by then
		// Self.activity is already a full struct (id, animations, etc.) and arg[0] is empty.
		inline constexpr const char* GML_SCRIPT_REQUEST_ACTIVITY     = "gml_Script_request_activity@ActivityHandler@ActivitiesAndRoutines";

		using BeforeHeartPointsChangeCallback = void(*)(MMAPI::NPC::HeartPointsChangedContext&);
		using BeforeReceiveGiftCallback       = void(*)(YYTK::CInstance* npc, int item_id);
		using AfterFindBlipNoiseCallback      = void(*)(MMAPI::NPC::FindBlipNoiseContext&);
		using BeforeActivityChangeCallback    = void(*)(MMAPI::NPC::BeforeActivityChangeContext&);
		using AfterActivityChangeCallback     = void(*)(MMAPI::NPC::AfterActivityChangeContext&);

		inline BeforeHeartPointsChangeCallback before_heart_points_change_callback = nullptr;
		inline BeforeReceiveGiftCallback       before_receive_gift_callback        = nullptr;
		inline AfterFindBlipNoiseCallback      after_find_blip_noise_callback      = nullptr;
		inline BeforeActivityChangeCallback    before_activity_change_callback     = nullptr;
		inline AfterActivityChangeCallback     after_activity_change_callback      = nullptr;

		// Override map: when an entry exists for an NPC, the begin_activity hook auto-substitutes
		// to the override's activity name. Stored as the activity NAME (string) rather than the
		// id because begin_activity's arg[0] is a string the game uses as a struct member key
		// (writing an int there crashes downstream `variable_struct_set` calls).
		inline std::unordered_map<int, std::string> activity_overrides;

		/// Returns the activity's internal name from `globalInstance.__activity__[id]`. Empty if
		/// id is out of range.
		inline std::string ActivityIdToName(int activity_id)
		{
			YYTK::RValue activities = MMAPI::Internal::global_instance->GetMember("__activity__");
			if (activities.m_Kind != YYTK::VALUE_ARRAY)
				return {};

			size_t length = 0;
			MMAPI::Internal::module_interface->GetArraySize(activities, length);
			if (activity_id < 0 || activity_id >= static_cast<int>(length))
				return {};

			YYTK::RValue* entry = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(activities, activity_id, entry);
			if (!entry || entry->m_Kind != YYTK::VALUE_STRING)
				return {};
			return entry->ToString();
		}

		/// Resolves an activity name to its id, or std::nullopt if the name isn't in `__activity__`.
		inline std::optional<int> ActivityNameToId(const std::string& name)
		{
			YYTK::RValue activities = MMAPI::Internal::global_instance->GetMember("__activity__");
			if (activities.m_Kind != YYTK::VALUE_ARRAY)
				return std::nullopt;

			size_t length = 0;
			MMAPI::Internal::module_interface->GetArraySize(activities, length);

			for (size_t i = 0; i < length; i++)
			{
				YYTK::RValue* entry = nullptr;
				MMAPI::Internal::module_interface->GetArrayEntry(activities, i, entry);
				if (entry && entry->m_Kind == YYTK::VALUE_STRING && entry->ToString() == name)
					return static_cast<int>(i);
			}
			return std::nullopt;
		}

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

		/// Hook for `request_activity@ActivityHandler@ActivitiesAndRoutines`. Self is the
		/// ActivityHandler instance; its `npc` member back-references the owning NPC, whose `id`
		/// is the index aligned with MMAPI::NPC::Ids. This is the script that *chooses* the next
		/// activity — `begin_activity` is its downstream consumer and is too late to override.
		///
		/// Arg[0] is the integer activity id (index into `globalInstance.__activity__`); the
		/// substitution path resolves the user-supplied activity name back to that id before
		/// writing it into arg[0].
		inline YYTK::RValue& GmlScriptRequestActivityCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			int npc_id = -1;
			if (Self)
			{
				YYTK::RValue npc = Self->GetMember("npc");
				if (npc.m_Kind == YYTK::VALUE_OBJECT)
				{
					YYTK::RValue id = npc.GetMember("id");
					if (MMAPI::Engine::IsNumeric(id))
						npc_id = static_cast<int>(id.ToInt64());
				}
			}

			std::string activity_name;
			if (Arguments && ArgumentCount >= 1 && Arguments[0] && Arguments[0]->m_Kind == YYTK::VALUE_STRING)
				activity_name = Arguments[0]->ToString();

			MMAPI::NPC::BeforeActivityChangeContext ctx;
			if (npc_id >= 0)
				ctx.m_npc = static_cast<MMAPI::NPC::Ids>(npc_id);
			ctx.m_activity_name = activity_name;

			if (npc_id >= 0)
			{
				auto it = activity_overrides.find(npc_id);
				if (it != activity_overrides.end())
					ctx.Substitute(it->second);
			}

			if (before_activity_change_callback && npc_id >= 0)
				before_activity_change_callback(ctx);

			std::string final_activity_name = activity_name;
			bool call_trampoline = true;

			switch (ctx.m_decision)
			{
				case MMAPI::NPC::BeforeActivityChangeContext::Decision::Allow:
					break;
				case MMAPI::NPC::BeforeActivityChangeContext::Decision::Deny:
					call_trampoline = false;
					break;
				case MMAPI::NPC::BeforeActivityChangeContext::Decision::Substitute:
					// `request_activity` takes arg[0] as an INTEGER activity id (index into
					// `globalInstance.__activity__`), not a name. Resolve the user-supplied name
					// to id here. If the name doesn't resolve to a known activity, drop the
					// substitution (writing a string would crash downstream npc_fulfills_activity
					// which expects an int-keyed lookup).
					if (Arguments && ArgumentCount >= 1 && Arguments[0] && !ctx.m_substitute_name.empty())
					{
						auto id = ActivityNameToId(ctx.m_substitute_name);
						if (id.has_value())
						{
							*Arguments[0] = *id;
							final_activity_name = ctx.m_substitute_name;
						}
					}
					break;
			}

			if (call_trampoline)
			{
				const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
					Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_REQUEST_ACTIVITY)
				);
				original(Self, Other, Result, ArgumentCount, Arguments);
			}

			if (after_activity_change_callback && npc_id >= 0 && call_trampoline)
			{
				MMAPI::NPC::AfterActivityChangeContext after_ctx;
				after_ctx.m_npc           = static_cast<MMAPI::NPC::Ids>(npc_id);
				after_ctx.m_activity_name = final_activity_name;
				after_activity_change_callback(after_ctx);
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
			{ Internal::GML_SCRIPT_REQUEST_ACTIVITY,    reinterpret_cast<PVOID>(Internal::GmlScriptRequestActivityCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Reads the NPC's current activity from `npc.activity_handler.activity`. The runtime stores
	/// this as the activity name string (game keeps the resolved name on the handler), so this
	/// returns the name directly. Returns empty if the NPC instance can't be resolved or has
	/// no current activity.
	///
	/// Iterates `obj_npc` instances to find the one with matching `id`. Cost is O(NPC count)
	/// per call; cache the result if used in a hot loop.
	inline std::string GetCurrentActivity(MMAPI::NPC::Ids npc)
	{
		// Note: NPC instances live as `obj_npc` objects in the game world. Without an instance
		// table iterator on hand, we fall back to scanning the npc database under
		// `globalInstance.__npc_database` (if that route exposes the activity_handler) or
		// returning empty if not. For v1 this is read-only and best-effort.
		YYTK::RValue database = MMAPI::Internal::global_instance->GetMember("__npc_database");
		if (database.m_Kind != YYTK::VALUE_OBJECT)
			return {};

		auto members = database.ToMap();
		for (const auto& [key, value] : members)
		{
			if (value.m_Kind != YYTK::VALUE_OBJECT)
				continue;
			YYTK::RValue npc_id = value.GetMember("id");
			if (!MMAPI::Engine::IsNumeric(npc_id))
				continue;
			if (static_cast<int>(npc_id.ToInt64()) != static_cast<int>(npc))
				continue;

			YYTK::RValue handler = value.GetMember("activity_handler");
			if (handler.m_Kind != YYTK::VALUE_OBJECT)
				return {};
			YYTK::RValue activity = handler.GetMember("activity");
			if (activity.m_Kind == YYTK::VALUE_STRING)
				return activity.ToString();
			if (MMAPI::Engine::IsNumeric(activity))
				return Internal::ActivityIdToName(static_cast<int>(activity.ToInt64()));
			return {};
		}
		return {};
	}

	/// Pins an NPC to a specific activity. Whenever the routine system tries to assign a new
	/// activity to this NPC via `begin_activity`, the hook substitutes `activity_name` instead.
	///
	/// Use this for "the NPC is busy with mod content" scenarios — e.g. a multi-day event during
	/// which Reina should be locked into "study" or "reading". The NPC stays in the world and
	/// remains interactive; the routine still ticks underneath but its activity choice is
	/// always overridden.
	///
	/// Activity names are members of `globalInstance.__activity__` (e.g. "reading", "narrows_workout").
	/// Names aren't validated at set time — invalid names are silently ignored by the hook
	/// (which resolves name→id and skips substitution if the id isn't found). Requires Enable().
	/// Can be called before save load (the override map only stores the name; resolution against
	/// `__activity__` happens at hook time when the game is running).
	///
	/// @attention **Activity-NPC compatibility matters.** Each NPC ships with animation assets
	/// for only a subset of activities (the ones their routine normally uses). Overriding to an
	/// activity the NPC has no animations for will:
	///   1. **Visually fall back** to an idle / T-pose / placeholder sprite — the substitution
	///      reaches the game but the renderer can't draw the activity.
	///   2. **Re-fire continuously** — downstream `npc_fulfills_activity` returns false, the
	///      activity never transitions to `active`, and the routine asks for the next activity
	///      on every tick. Our hook substitutes again → tight loop. Functionally harmless but
	///      a performance concern and a sign the override "didn't take".
	///
	/// Prefer overrides that align with the NPC's normal activity vocabulary (look at their
	/// routine's `activities` array for inspiration). If an arbitrary activity is needed (e.g.
	/// "Reina at the bookshelf for a story event"), the more reliable approach is to drive the
	/// scene via `Cutscene` / dialogue rather than an activity override.
	inline void SetActivityOverride(MMAPI::NPC::Ids npc, const std::string& activity_name)
	{
		MMAPI_REQUIRE_ENABLED_VOID("NPC");
		Internal::activity_overrides[static_cast<int>(npc)] = activity_name;
	}

	/// Removes any active activity override for the named NPC, restoring routine-driven behaviour.
	inline void ClearActivityOverride(MMAPI::NPC::Ids npc)
	{
		Internal::activity_overrides.erase(static_cast<int>(npc));
	}

	/// Returns true if an activity override is currently registered for this NPC.
	inline bool HasActivityOverride(MMAPI::NPC::Ids npc)
	{
		return Internal::activity_overrides.contains(static_cast<int>(npc));
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

		/// Registers a callback that runs before an NPC's activity changes via `begin_activity`.
		/// The context exposes the NPC + intended activity id/name, and provides Allow / Deny /
		/// Substitute methods to control what happens. Use SetActivityOverride for the common
		/// "always pin this NPC to X" case; this hook is for callback-driven logic (conditional
		/// substitution, multi-NPC coordination, logging, etc.).
		///
		/// The override map's decision (if any) is applied to the context BEFORE this callback
		/// runs, so the callback sees what the override map intends to do and can change it.
		/// The most recent action wins.
		///
		/// @param callback A function called with a mutable `BeforeActivityChangeContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeActivityChange(Internal::BeforeActivityChangeCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::NPC::Hooks::BeforeActivityChange, MMAPI::NPC);

			return MMAPI::Internal::RegisterHook(
				"NPC::BeforeActivityChange",
				Internal::before_activity_change_callback,
				callback
			);
		}

		/// Registers a callback that runs after `begin_activity` has attached an activity.
		/// The context exposes the NPC + the activity id/name that actually got assigned (which
		/// may differ from the BeforeActivityChange input if a Substitute fired).
		///
		/// Doesn't fire when BeforeActivityChange's decision was Deny (since no activity was
		/// actually assigned).
		///
		/// @param callback A function called with a `AfterActivityChangeContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterActivityChange(Internal::AfterActivityChangeCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::NPC::Hooks::AfterActivityChange, MMAPI::NPC);

			return MMAPI::Internal::RegisterHook(
				"NPC::AfterActivityChange",
				Internal::after_activity_change_callback,
				callback
			);
		}
	}
}
