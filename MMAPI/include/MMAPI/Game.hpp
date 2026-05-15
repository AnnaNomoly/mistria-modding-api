// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Hook.hpp"
#include "Instance.hpp"
#include "Weather.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include <string>
#include <unordered_map>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Game
{
	/// Source: globalInstance.__xp_values
	/// Struct-backed enum; values follow alphabetical key order.
	enum class XpValues : int
	{
		BreakDigSpot     = 0,
		CatchGiantFish   = 1,
		CatchLargeFish   = 2,
		CatchMediumFish  = 3,
		CatchSmallFish   = 4,
		HarvestCrop      = 5,
		HarvestDiveSpot  = 6,
		IdentifyArtifact = 7,
		PlantSeed        = 8,
		TillDirt         = 9,
		WaterDirt        = 10
	};

	/// Total number of enumerators in XpValues. Iterating [0, XpValueCount) covers every XpValues value.
	inline constexpr int XpValueCount = 11;

	/// Invokes fn with every XpValues value, in ascending order.
	template <typename Fn>
	inline void ForEachXpValue(Fn fn)
	{
		for (int i = 0; i < XpValueCount; ++i)
			fn(static_cast<XpValues>(i));
	}

	namespace Internal
	{
		// Extracts the stable per-save identifier from an absolute save file path. The identifier
		// is the first segment between hyphens in the filename — for "game-543230633-autosave.sav"
		// and "game-543230633-789877084.sav" this returns "543230633" in both cases, so a mod's
		// per-save data file lines up across autosaves and manual saves of the same slot.
		// Returns empty when the path is empty, malformed (missing hyphens), or contains
		// "undefined" (the placeholder the game emits before a brand-new save commits to disk).
		inline std::string ExtractSavePrefix(std::string_view save_path)
		{
			if (save_path.empty() || save_path.find("undefined") != std::string_view::npos)
				return {};

			std::size_t last_slash = save_path.find_last_of("/\\");
			std::string_view name = (last_slash == std::string_view::npos)
				? save_path
				: save_path.substr(last_slash + 1);

			std::size_t first = name.find_first_of("-");
			std::size_t last  = name.find_last_of("-");
			if (first == std::string_view::npos || last == std::string_view::npos || first >= last)
				return {};
			return std::string(name.substr(first + 1, last - first - 1));
		}
	}

	struct SaveGameContext
	{
		std::string m_save_path;
		bool        m_cancelled = false;

		/// Returns the absolute path the game is about to save to (the script's Arguments[0]).
		/// Empty if Arguments[0] was unavailable or not a string. Note that the game emits
		/// save_game with a placeholder path containing "undefined" before a brand-new save has
		/// actually committed to disk — prefer GetSavePrefix() over parsing this directly.
		std::string_view GetSavePath() const { return m_save_path; }

		/// Returns the stable per-save identifier extracted from the save path (the segment
		/// between the first and last hyphens in the filename). Useful as the key for per-save
		/// mod data files. Empty when the path is a placeholder ("undefined") or malformed —
		/// callers can treat an empty result as "save isn't ready to be keyed yet".
		std::string GetSavePrefix() const { return Internal::ExtractSavePrefix(m_save_path); }

		void Cancel() { m_cancelled = true; }
	};

	struct LoadGameContext
	{
		std::string m_save_path;

		/// Returns the absolute path of the save file the game just loaded, read from
		/// `Arguments[0].save_path` (the load_game script's input struct). Empty if the
		/// member was missing or not a string.
		std::string_view GetSavePath() const { return m_save_path; }

		/// Returns the stable per-save identifier extracted from the save path (the segment
		/// between the first and last hyphens in the filename). Useful as the key for per-save
		/// mod data files. Empty when the path is malformed.
		std::string GetSavePrefix() const { return Internal::ExtractSavePrefix(m_save_path); }
	};

	struct PlayAudioContext
	{
		std::string m_audio_name;
		bool m_cancelled = false;

		std::string_view GetAudioName() const { return m_audio_name; }
		void SetAudioName(std::string name) { m_audio_name = std::move(name); }
		void Cancel() { m_cancelled = true; }
	};

	struct AfterStoreMenuAddItemContext
	{
		int m_repeat_count = 0;

		/// Schedules N additional invocations of the game's store-menu add-item script with the same
		/// arguments and calling context. The script has already run once before this callback fires,
		/// so passing 9 here makes the player's single click purchase 10 items total. Use for
		/// bulk-purchase mods (e.g. "click + hold Shift" buys a stack of an item).
		void RepeatOriginal(int additional_count) { m_repeat_count = additional_count; }

		/// Returns the additional-invocation count currently scheduled. Zero means the original ran
		/// once with no extras.
		int GetRepeatCount() const { return m_repeat_count; }
	};

	struct BeforeErrorContext
	{
		std::string m_message;
		bool        m_cancelled = false;

		/// Returns the error message the game's `error` script is about to log.
		std::string_view GetMessage() const { return m_message; }

		/// Prevents the game's `error` script from running, suppressing this specific error.
		/// Use to silence specific game-emitted errors that mods deliberately trigger
		/// (e.g. when a probing script call fails in an expected way).
		void Cancel() { m_cancelled = true; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		inline std::unordered_map<std::string, uint64_t> notification_last_display_time;
		inline constexpr const char* GML_SCRIPT_CREATE_NOTIFICATION     = "gml_Script_create_notification";
		inline constexpr const char* GML_SCRIPT_SELL_SHIPPING_BIN_ITEMS = "gml_Script_sell_shipping_bin_items";
		inline constexpr const char* GML_SCRIPT_END_DAY                 = "gml_Script_end_day";
		inline constexpr const char* GML_SCRIPT_ARI_ON_NEW_DAY          = "gml_Script_on_new_day@Ari@Ari";
		inline constexpr const char* GML_SCRIPT_LOAD_GAME               = "gml_Script_load_game";
		inline constexpr const char* GML_SCRIPT_SAVE_GAME               = "gml_Script_save_game";
		inline constexpr const char* GML_SCRIPT_SCENE_AUDIO_PLAYER_PLAY = "gml_Script_play@SceneAudioPlayer@SceneAudioPlayer";
		inline constexpr const char* GML_SCRIPT_SCENE_AUDIO_PLAYER_STOP = "gml_Script_stop@SceneAudioPlayer@SceneAudioPlayer";
		inline constexpr const char* GML_SCRIPT_JOURNAL_MENU_OPEN       = "gml_Script_initialize@JournalMenu@JournalMenu";
		inline constexpr const char* GML_SCRIPT_JOURNAL_MENU_CLOSE      = "gml_Script_on_close@JournalMenu@JournalMenu";
		inline constexpr const char* GML_SCRIPT_STORE_MENU_OPEN         = "gml_Script_init@StoreMenu@StoreMenu";
		inline constexpr const char* GML_SCRIPT_STORE_MENU_CLOSE        = "gml_Script_anon@10878@StoreMenu@StoreMenu";
		inline constexpr const char* GML_SCRIPT_STORE_MENU_ADD_ITEM     = "gml_Script_anon@6279@StoreMenu@StoreMenu";
		inline constexpr const char* GML_SCRIPT_ERROR                   = "gml_Script_error";

		using EndDayCallback = void(*)();
		using NewDayCallback = void(*)();
		using BeforeErrorCallback = void(*)(MMAPI::Game::BeforeErrorContext&);
		inline BeforeErrorCallback before_error_callback = nullptr;

		inline EndDayCallback after_end_day_callback = nullptr;
		inline NewDayCallback before_new_day_callback = nullptr;

		using AfterLoadGameCallback   = void(*)(MMAPI::Game::LoadGameContext&);
		using BeforeSaveGameCallback  = void(*)(MMAPI::Game::SaveGameContext&);
		using BeforePlayAudioCallback = void(*)(MMAPI::Game::PlayAudioContext&);

		inline AfterLoadGameCallback   after_load_game_callback   = nullptr;
		inline BeforeSaveGameCallback  before_save_game_callback  = nullptr;
		inline BeforePlayAudioCallback before_play_audio_callback = nullptr;

		using AfterJournalMenuOpenCallback   = void(*)();
		using AfterJournalMenuCloseCallback  = void(*)();
		using AfterStoreMenuOpenCallback     = void(*)();
		using AfterStoreMenuCloseCallback    = void(*)();
		using AfterStoreMenuAddItemCallback  = void(*)(MMAPI::Game::AfterStoreMenuAddItemContext&);

		inline AfterJournalMenuOpenCallback   after_journal_menu_open_callback     = nullptr;
		inline AfterJournalMenuCloseCallback  after_journal_menu_close_callback    = nullptr;
		inline AfterStoreMenuOpenCallback     after_store_menu_open_callback       = nullptr;
		inline AfterStoreMenuCloseCallback    after_store_menu_close_callback      = nullptr;
		inline AfterStoreMenuAddItemCallback  after_store_menu_add_item_callback   = nullptr;

		// Live SceneAudioPlayer Self/Other, latched from the play hook.
		// Used by TryGetSceneAudioPlayerContext for callers that need to invoke follow-up
		// scripts (e.g. stop@SceneAudioPlayer) outside any hook frame.
		inline YYTK::CInstance* scene_audio_player_self  = nullptr;
		inline YYTK::CInstance* scene_audio_player_other = nullptr;

		// Cleared from the setup_main_screen pub/sub when the player returns to the title menu.
		// Registered by Game::Enable().
		inline void ClearSceneAudioPlayerOnReturnToTitle(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			scene_audio_player_self  = nullptr;
			scene_audio_player_other = nullptr;
		}

		/// Resolves the SceneAudioPlayer's GML calling context, latched from the most recent
		/// play@SceneAudioPlayer fire. Cleared automatically when the game returns to the title menu.
		/// @return True if a play has been observed this session, false otherwise.
		inline bool TryGetSceneAudioPlayerContext(YYTK::CInstance*& Self, YYTK::CInstance*& Other)
		{
			if (!scene_audio_player_self)
				return false;
			Self  = scene_audio_player_self;
			Other = scene_audio_player_other;
			return true;
		}

		inline constexpr const char* ToGameKey(MMAPI::Game::XpValues value)
		{
			switch (value)
			{
				case MMAPI::Game::XpValues::BreakDigSpot:
					return "break_dig_spot";
				case MMAPI::Game::XpValues::CatchGiantFish:
					return "catch_giant_fish";
				case MMAPI::Game::XpValues::CatchLargeFish:
					return "catch_large_fish";
				case MMAPI::Game::XpValues::CatchMediumFish:
					return "catch_medium_fish";
				case MMAPI::Game::XpValues::CatchSmallFish:
					return "catch_small_fish";
				case MMAPI::Game::XpValues::HarvestCrop:
					return "harvest_crop";
				case MMAPI::Game::XpValues::HarvestDiveSpot:
					return "harvest_dive_spot";
				case MMAPI::Game::XpValues::IdentifyArtifact:
					return "identify_artifact";
				case MMAPI::Game::XpValues::PlantSeed:
					return "plant_seed";
				case MMAPI::Game::XpValues::TillDirt:
					return "till_dirt";
				case MMAPI::Game::XpValues::WaterDirt:
					return "water_dirt";
				default:
					return nullptr;
			}
		}

		inline YYTK::RValue& GmlScriptEndDayCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(
					MMAPI::Internal::self_module,
					GML_SCRIPT_END_DAY
				)
			);

			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_end_day_callback)
				after_end_day_callback();

			return Result;
		}

		inline YYTK::RValue& GmlScriptAriBeforeNewDayCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_new_day_callback)
				before_new_day_callback();

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(
					MMAPI::Internal::self_module,
					GML_SCRIPT_ARI_ON_NEW_DAY
				)
			);

			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

		inline YYTK::RValue& GmlScriptLoadGameCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_LOAD_GAME)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_load_game_callback)
			{
				MMAPI::Game::LoadGameContext context;
				if (Arguments && ArgumentCount >= 1 && Arguments[0])
				{
					YYTK::RValue* save_path_member = Arguments[0]->GetRefMember("save_path");
					if (save_path_member && save_path_member->m_Kind == YYTK::VALUE_STRING)
						context.m_save_path = save_path_member->ToString();
				}
				after_load_game_callback(context);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptSaveGameCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_save_game_callback)
			{
				MMAPI::Game::SaveGameContext context;
				if (Arguments && ArgumentCount >= 1 && Arguments[0] && Arguments[0]->m_Kind == YYTK::VALUE_STRING)
					context.m_save_path = Arguments[0]->ToString();

				before_save_game_callback(context);

				if (context.m_cancelled)
					return Result;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_SAVE_GAME)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

		inline YYTK::RValue& GmlScriptPlayAudioCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			// Latch on first observation only (matches pre-MMAPI DD's pattern for this script).
			// Re-latching every tick can feed a later-tick Other into downstream script calls
			// that the script doesn't accept — see StatusEffect's manager-update comment.
			if (!scene_audio_player_self)
			{
				scene_audio_player_self  = Self;
				scene_audio_player_other = Other;
			}

			if (before_play_audio_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::Game::PlayAudioContext context{ Arguments[0]->ToString() };
				before_play_audio_callback(context);

				if (context.m_cancelled)
					return Result;

				*Arguments[0] = YYTK::RValue(context.m_audio_name);
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_SCENE_AUDIO_PLAYER_PLAY)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

		inline YYTK::RValue& GmlScriptJournalMenuOpenCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_JOURNAL_MENU_OPEN)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);
			if (after_journal_menu_open_callback)
				after_journal_menu_open_callback();
			return Result;
		}

		inline YYTK::RValue& GmlScriptJournalMenuCloseCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_JOURNAL_MENU_CLOSE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);
			if (after_journal_menu_close_callback)
				after_journal_menu_close_callback();
			return Result;
		}

		inline YYTK::RValue& GmlScriptStoreMenuOpenCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_STORE_MENU_OPEN)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);
			if (after_store_menu_open_callback)
				after_store_menu_open_callback();
			return Result;
		}

		inline YYTK::RValue& GmlScriptStoreMenuCloseCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_STORE_MENU_CLOSE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);
			if (after_store_menu_close_callback)
				after_store_menu_close_callback();
			return Result;
		}

		inline YYTK::RValue& GmlScriptStoreMenuAddItemCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_STORE_MENU_ADD_ITEM)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_store_menu_add_item_callback)
			{
				MMAPI::Game::AfterStoreMenuAddItemContext context;
				after_store_menu_add_item_callback(context);
				// Replays go through the trampoline as well, so the hook doesn't re-enter on each
				// loop iteration — the user callback fires exactly once per real player click.
				for (int i = 0; i < context.m_repeat_count; ++i)
					original(Self, Other, Result, ArgumentCount, Arguments);
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptBeforeErrorCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_error_callback && Arguments && ArgumentCount >= 1 && Arguments[0]
				&& Arguments[0]->m_Kind == YYTK::VALUE_STRING)
			{
				MMAPI::Game::BeforeErrorContext context{ Arguments[0]->ToString() };
				before_error_callback(context);
				if (context.m_cancelled)
					return Result;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_ERROR)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

	}

	/// Returns true if the game is currently paused.
	inline bool IsPaused()
	{
		YYTK::RValue pause_status = MMAPI::Internal::global_instance->GetMember("__pause_status");
		return pause_status.ToInt64() > 0;
	}

	/// Returns true when the `room` builtin holds a valid current-room asset_id.
	///
	/// Use this as a precondition gate before calling any GML script that implicitly reads
	/// the `room` builtin and would crash on `undefined` (e.g. `is_dungeon_room` and friends).
	/// During save-load transitions, before the title screen settles, or between rooms in
	/// some flows, `room` can be unset/undefined and the underlying scripts will pass it to
	/// `asset_has_tags(room, ...)` which throws "argument 1 incorrect type (undefined)".
	///
	/// MMAPI helpers that wrap such scripts call this first and short-circuit safely; mod
	/// callers can use it too for early-tick guards.
	/// @return True if `room` is a VALUE_REF (a real asset reference); false otherwise.
	inline bool IsRoomReady()
	{
		YYTK::RValue room_id;
		Aurie::AurieStatus status = MMAPI::Internal::module_interface->GetBuiltin("room", nullptr, NULL_INDEX, room_id);
		if (!Aurie::AurieSuccess(status))
			return false;
		// `room` in modern GameMaker comes back as VALUE_REF (an asset reference) when the room
		// is valid, and VALUE_UNDEFINED / VALUE_UNSET during pre-load transitions. Checking
		// explicitly for VALUE_REF matches what real game scripts expect downstream.
		return room_id.m_Kind == YYTK::VALUE_REF;
	}

	/// Returns the current GM room name.
	/// @return The current GM room name, or an empty string if it cannot be read.
	inline std::string GetCurrentRoomName()
	{
		YYTK::RValue room_id;
		Aurie::AurieStatus status = MMAPI::Internal::module_interface->GetBuiltin("room", nullptr, NULL_INDEX, room_id);
		if (!Aurie::AurieSuccess(status))
			return "";

		YYTK::RValue room_name = MMAPI::Internal::module_interface->CallBuiltin("room_get_name", { room_id });
		if (room_name.m_Kind == YYTK::VALUE_UNDEFINED || room_name.m_Kind == YYTK::VALUE_UNSET)
			return "";

		return room_name.ToString();
	}

	/// Returns true if the current GM room name exactly matches room_name.
	/// @param room_name The GM room name to compare against.
	inline bool IsCurrentRoom(const std::string& room_name)
	{
		return GetCurrentRoomName() == room_name;
	}

	/// Returns true if the current GM room name contains room_name_part.
	/// @param room_name_part The text to search for in the current GM room name.
	inline bool CurrentRoomNameContains(const std::string& room_name_part)
	{
		return GetCurrentRoomName().contains(room_name_part);
	}

	/// Gets an XP value from MMAPI::Internal::global_instance.__xp_values.
	/// @param xp_value The XP value to read.
	/// @return The XP value as an RValue, or undefined if unavailable.
	inline YYTK::RValue GetXpValue(MMAPI::Game::XpValues xp_value)
	{
		const char* xp_value_key = Internal::ToGameKey(xp_value);
		if (!xp_value_key)
			return {};

		YYTK::RValue xp_values = MMAPI::Internal::global_instance->GetMember("__xp_values");
		if (xp_values.m_Kind == YYTK::VALUE_UNDEFINED)
			return {};

		return xp_values.GetMember(xp_value_key);
	}

	/// Displays a localized notification popup.
	/// @param ignore_cooldown When true, bypasses the 5-second per-key cooldown and always displays the notification.
	/// @param notification_key Localization string key for the notification text.
	inline void CreateNotification(bool ignore_cooldown, const std::string& notification_key)
	{
		uint64_t now = MMAPI::Internal::GetCurrentSystemTime();
		if (!ignore_cooldown && now <= Internal::notification_last_display_time[notification_key] + 5000)
			return;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(
			Internal::GML_SCRIPT_CREATE_NOTIFICATION,
			reinterpret_cast<PVOID*>(&gml_script)
		);

		YYTK::RValue result;
		YYTK::RValue notification_rv(notification_key);
		YYTK::RValue* notification_ptr = &notification_rv;
		gml_script->m_Functions->m_ScriptFunction(nullptr, nullptr, result, 1, { &notification_ptr });

		Internal::notification_last_display_time[notification_key] = now;
	}

	/// Activates Game utility functions that directly call game scripts. Eagerly installs every Game
	/// script hook used by Hooks::* registrars (end_day, on_new_day, load/save_game, scene audio play,
	/// journal/store menu open/close).
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Game::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Game, MMAPI::Instance);

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Game, MMAPI::Weather);

		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ClearSceneAudioPlayerOnReturnToTitle);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_END_DAY,                 reinterpret_cast<PVOID>(Internal::GmlScriptEndDayCallback) },
			{ Internal::GML_SCRIPT_ARI_ON_NEW_DAY,          reinterpret_cast<PVOID>(Internal::GmlScriptAriBeforeNewDayCallback) },
			{ Internal::GML_SCRIPT_LOAD_GAME,               reinterpret_cast<PVOID>(Internal::GmlScriptLoadGameCallback) },
			{ Internal::GML_SCRIPT_SAVE_GAME,               reinterpret_cast<PVOID>(Internal::GmlScriptSaveGameCallback) },
			{ Internal::GML_SCRIPT_SCENE_AUDIO_PLAYER_PLAY, reinterpret_cast<PVOID>(Internal::GmlScriptPlayAudioCallback) },
			{ Internal::GML_SCRIPT_JOURNAL_MENU_OPEN,       reinterpret_cast<PVOID>(Internal::GmlScriptJournalMenuOpenCallback) },
			{ Internal::GML_SCRIPT_JOURNAL_MENU_CLOSE,      reinterpret_cast<PVOID>(Internal::GmlScriptJournalMenuCloseCallback) },
			{ Internal::GML_SCRIPT_STORE_MENU_OPEN,         reinterpret_cast<PVOID>(Internal::GmlScriptStoreMenuOpenCallback) },
			{ Internal::GML_SCRIPT_STORE_MENU_CLOSE,        reinterpret_cast<PVOID>(Internal::GmlScriptStoreMenuCloseCallback) },
			{ Internal::GML_SCRIPT_STORE_MENU_ADD_ITEM,     reinterpret_cast<PVOID>(Internal::GmlScriptStoreMenuAddItemCallback) },
			{ Internal::GML_SCRIPT_ERROR,                   reinterpret_cast<PVOID>(Internal::GmlScriptBeforeErrorCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Stops the currently-playing SceneAudioPlayer track by invoking the game's
	/// `stop@SceneAudioPlayer@SceneAudioPlayer` script with the most recently observed
	/// SceneAudioPlayer Self/Other (latched on every play). Useful for forcing the audio
	/// system to pick a new track on the next play call (e.g. between dungeon floors).
	/// @attention Requires MMAPI::Game::Enable() to have been called.
	/// @return True if the stop script was invoked, false if no play has been observed yet this session.
	inline bool StopSceneAudio()
	{
		MMAPI_REQUIRE_ENABLED("Game", false);

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetSceneAudioPlayerContext(Self, Other))
			return false;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_SCENE_AUDIO_PLAYER_STOP, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		return true;
	}

	/// Sells the current shipping bin contents.
	/// @attention Requires MMAPI::Game::Enable() to have been called.
	/// @return The sale result as an RValue, or undefined if the required context is unavailable.
	inline YYTK::RValue SellShippingBinItems()
	{
		MMAPI_REQUIRE_ENABLED("Game", {});

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return {};

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_SELL_SHIPPING_BIN_ITEMS, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 0, nullptr);
		return result;
	}

	namespace Hooks
	{
		/// Registers a callback that runs after the game ends the current day.
		/// @param callback A function called after the game's end day script runs.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterEndDay(Internal::EndDayCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Game::Hooks::AfterEndDay, MMAPI::Game);

			return MMAPI::Internal::RegisterHook(
				"Game::AfterEndDay",
				Internal::after_end_day_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game starts a new day for Ari.
		/// @param callback A function called before Ari's on new day script runs.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeNewDay(Internal::NewDayCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Game::Hooks::BeforeNewDay, MMAPI::Game);

			return MMAPI::Internal::RegisterHook(
				"Game::BeforeNewDay",
				Internal::before_new_day_callback,
				callback
			);
		}

		/// Registers a callback that runs after the game loads a save file.
		/// Read `ctx.GetSavePath()` to access the absolute path of the loaded save file.
		/// @param callback A function called with a `MMAPI::Game::LoadGameContext` after the game's load_game script runs.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterLoadGame(Internal::AfterLoadGameCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Game::Hooks::AfterLoadGame, MMAPI::Game);

			return MMAPI::Internal::RegisterHook(
				"Game::AfterLoadGame",
				Internal::after_load_game_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game saves.
		/// Read `ctx.GetSavePath()` to access the absolute path the game is about to save to.
		/// Call ctx.Cancel() to prevent the game from saving.
		/// @param callback A function called with a mutable save context before the game processes it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeSaveGame(Internal::BeforeSaveGameCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Game::Hooks::BeforeSaveGame, MMAPI::Game);

			return MMAPI::Internal::RegisterHook(
				"Game::BeforeSaveGame",
				Internal::before_save_game_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game plays an audio track.
		/// Use ctx.SetAudioName() to redirect to a different audio asset, or ctx.Cancel() to prevent playback.
		/// @param callback A function called with a mutable audio context before the game processes it.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforePlayAudio(Internal::BeforePlayAudioCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Game::Hooks::BeforePlayAudio, MMAPI::Game);

			return MMAPI::Internal::RegisterHook(
				"Game::BeforePlayAudio",
				Internal::before_play_audio_callback,
				callback
			);
		}

		/// Registers a callback that runs after the journal menu opens.
		/// @param callback A function called after the journal menu's initialize script runs.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterJournalMenuOpen(Internal::AfterJournalMenuOpenCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Game::Hooks::AfterJournalMenuOpen, MMAPI::Game);

			return MMAPI::Internal::RegisterHook(
				"Game::AfterJournalMenuOpen",
				Internal::after_journal_menu_open_callback,
				callback
			);
		}

		/// Registers a callback that runs after the journal menu closes.
		/// @param callback A function called after the journal menu's close script runs.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterJournalMenuClose(Internal::AfterJournalMenuCloseCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Game::Hooks::AfterJournalMenuClose, MMAPI::Game);

			return MMAPI::Internal::RegisterHook(
				"Game::AfterJournalMenuClose",
				Internal::after_journal_menu_close_callback,
				callback
			);
		}

		/// Registers a callback that runs after the store menu opens.
		/// @param callback A function called after the store menu's initialize script runs.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterStoreMenuOpen(Internal::AfterStoreMenuOpenCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Game::Hooks::AfterStoreMenuOpen, MMAPI::Game);

			return MMAPI::Internal::RegisterHook(
				"Game::AfterStoreMenuOpen",
				Internal::after_store_menu_open_callback,
				callback
			);
		}

		/// Registers a callback that runs after the store menu closes.
		/// @param callback A function called after the store menu's close script runs.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterStoreMenuClose(Internal::AfterStoreMenuCloseCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Game::Hooks::AfterStoreMenuClose, MMAPI::Game);

			return MMAPI::Internal::RegisterHook(
				"Game::AfterStoreMenuClose",
				Internal::after_store_menu_close_callback,
				callback
			);
		}

		/// Registers a callback that runs after the player adds an item to their store cart. The script
		/// hooked is the anonymous "buy" handler — it fires once per click on a store entry, after the
		/// game has applied the standard 1-item purchase. Call `ctx.RepeatOriginal(N)` to fire N more
		/// invocations with the same arguments, multiplying the purchase quantity by `N + 1` (useful for
		/// bulk-purchase mods that key off a held modifier). Replays run through the trampoline, so the
		/// user callback never re-fires from within its own scope.
		/// @param callback A function called with a mutable `MMAPI::Game::AfterStoreMenuAddItemContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterStoreMenuAddItem(Internal::AfterStoreMenuAddItemCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Game::Hooks::AfterStoreMenuAddItem, MMAPI::Game);

			return MMAPI::Internal::RegisterHook(
				"Game::AfterStoreMenuAddItem",
				Internal::after_store_menu_add_item_callback,
				callback
			);
		}

		/// Registers a callback that runs before the title menu's setup_main_screen script.
		/// MMAPI automatically clears its instance reference map and fires module-local "return-to-title"
		/// handlers before this callback runs, so mods do not need to reset captured state manually.
		/// @param callback A function called before the title menu setup script runs (after MMAPI's auto-clears).
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeSetupMainScreen(MMAPI::Internal::BeforeSetupMainScreenCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Game::Hooks::BeforeSetupMainScreen, MMAPI::Game);

			return MMAPI::Internal::RegisterHook(
				"Game::BeforeSetupMainScreen",
				MMAPI::Internal::before_setup_main_screen_callback,
				callback
			);
		}

		/// Registers a callback that fires once per session when the game becomes interactive — defined as
		/// the first `get_weather@WeatherManager@Weather` script call after a save load. Re-fires on each
		/// subsequent save load (cleared on return-to-title alongside other session-scoped Weather state).
		///
		/// @attention Localizer is not guaranteed to be populated yet at this point. Mods that need to
		/// resolve localized item/text names should still pair this with `Text::Hooks::AfterLocalizedString`
		/// (e.g. set a flag in AfterGameActive, consume it in AfterLocalizedString) so the work runs after
		/// at least one localized string has been resolved.
		///
		/// @param callback A function called once per session when the game becomes interactive.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterGameActive(MMAPI::Weather::Internal::GameActiveCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Game::Hooks::AfterGameActive, MMAPI::Game);

			return MMAPI::Internal::RegisterHook(
				"Game::AfterGameActive",
				MMAPI::Weather::Internal::game_active_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game's `error` script. Read `ctx.GetMessage()`
		/// to inspect the error message; call `ctx.Cancel()` to swallow it (the original error script
		/// won't run for this fire). Use sparingly — silencing legitimate game errors masks bugs.
		/// @param callback A function called with a mutable `MMAPI::Game::BeforeErrorContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeError(Internal::BeforeErrorCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Game::Hooks::BeforeError, MMAPI::Game);

			return MMAPI::Internal::RegisterHook(
				"Game::BeforeError",
				Internal::before_error_callback,
				callback
			);
		}
	}
}
