// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Hook.hpp"
#include "Instance.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Bark
{
	/// Source: globalInstance.__bark_id__
	enum class Icons : int
	{
		Adeline            = 0,
		Angry              = 1,
		Annoyed            = 2,
		Archaeology        = 3,
		Balor              = 4,
		Bathhouse          = 5,
		Beach              = 6,
		Blush              = 7,
		Book               = 8,
		BreathOfFlame      = 9,
		BreedFemale        = 10,
		BreedMale          = 11,
		Caldarus           = 12,
		Celebration        = 13,
		Celine             = 14,
		CherryBlossoms     = 15,
		Chicken            = 16,
		Coin               = 17,
		ColdDrink          = 18,
		Cooking            = 19,
		Cow                = 20,
		Crafting           = 21,
		Crown              = 22,
		CuteFace           = 23,
		Darcy              = 24,
		Dell               = 25,
		Dozy               = 26,
		Eight              = 27,
		Eiland             = 28,
		Ellipses           = 29,
		Elsie              = 30,
		EmptyHeart         = 31,
		Errol              = 32,
		ExclamationMark    = 33,
		Fall               = 34,
		Farming            = 35,
		Fire               = 36,
		Firesail           = 37,
		FishBait           = 38,
		Fishing            = 39,
		Five               = 40,
		Forest             = 41,
		Four               = 42,
		Gem                = 43,
		Hay                = 44,
		Hayden             = 45,
		Heart              = 46,
		Heatwave           = 47,
		Hemlock            = 48,
		Henrietta          = 49,
		Holt               = 50,
		Horse              = 51,
		HotDrink           = 52,
		Hungry             = 53,
		Josephine          = 54,
		Juniper            = 55,
		Lake               = 56,
		Landen             = 57,
		Louis              = 58,
		Luc                = 59,
		Maple              = 60,
		March              = 61,
		Merri              = 62,
		Mining             = 63,
		Mist               = 64,
		Moon               = 65,
		Mountain           = 66,
		Music              = 67,
		Nine               = 68,
		NoCoin             = 69,
		Nora               = 70,
		Obsidian           = 71,
		Olric              = 72,
		One                = 73,
		PlantTonic         = 74,
		Priestess          = 75,
		QuestionMark       = 76,
		Rainy              = 77,
		Reina              = 78,
		RelationshipStatus = 79,
		Ryis               = 80,
		Seed               = 81,
		Seven              = 82,
		Six                = 83,
		Sleepy             = 84,
		SmokeMoth          = 85,
		Snow               = 86,
		Spring             = 87,
		Stars              = 88,
		Summer             = 89,
		Sunny              = 90,
		SweatDrop          = 91,
		Terithia           = 92,
		Thread             = 93,
		Three              = 94,
		Thunderstorm       = 95,
		Two                = 96,
		Valen              = 97,
		Vera               = 98,
		Wheedle            = 99,
		Winter             = 100,
		Woodcrafting       = 101,
		Yum                = 102
	};

	/// Total number of enumerators in Icons. Iterating [0, IconCount) covers every Icons value.
	inline constexpr int IconCount = 103;

	/// Invokes fn with every Icons value, in ascending order.
	template <typename Fn>
	inline void ForEachIcon(Fn fn)
	{
		for (int i = 0; i < IconCount; ++i)
			fn(static_cast<Icons>(i));
	}

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_BARK_EMITTER      = "gml_Script_BarkEmitter";
		inline constexpr const char* GML_SCRIPT_BARK_EMITTER_EMIT = "gml_Script_emit@BarkEmitter@BarkEmitter";

		// Live Self/Other for the *player's* BarkEmitter, latched when the hook fires with Other being
		// the Ari struct (identified by the presence of `god_mode` or `wimp_mode` cheat flags — those
		// members live on Ari's struct and reliably distinguish the player emitter from NPC/animal
		// emitters that also call BarkEmitter). Without this filter the latched context would be
		// overwritten by any non-player bark emitter.
		inline YYTK::CInstance* player_bark_emitter_self  = nullptr;
		inline YYTK::CInstance* player_bark_emitter_other = nullptr;

		// Cleared from the setup_main_screen pub/sub when the player returns to the title menu.
		// Registered by Bark::Enable().
		inline void ClearBarkEmitterOnReturnToTitle(YYTK::CInstance* /*Self*/, YYTK::CInstance* /*Other*/)
		{
			player_bark_emitter_self  = nullptr;
			player_bark_emitter_other = nullptr;
		}

		inline YYTK::RValue& BarkEmitterContextCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			// Only latch when Other is the Ari struct (filtered by god_mode/wimp_mode presence).
			// Non-player bark emitters (NPCs, animals, etc.) fire this hook too — ignore those.
			if (Other)
			{
				YYTK::RValue other_rv = Other->ToRValue();
				if (MMAPI::Engine::StructVariableExists(other_rv, "god_mode")
					|| MMAPI::Engine::StructVariableExists(other_rv, "wimp_mode"))
				{
					player_bark_emitter_self  = Self;
					player_bark_emitter_other = Other;
				}
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_BARK_EMITTER)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}

		/// Resolves the calling context for `emit@BarkEmitter@BarkEmitter` — the latched player BarkEmitter
		/// Self/Other captured the first time the player's BarkEmitter ran with Ari as Other (cleared on
		/// return-to-title). NPC/animal bark emitters are not latched here — see
		/// `BarkEmitterContextCallback` for the filter rationale.
		/// @return True if both pointers were resolved, false if the player's BarkEmitter hasn't fired yet.
		inline bool TryGetPlayerBarkEmitterContext(YYTK::CInstance*& Self, YYTK::CInstance*& Other)
		{
			if (!player_bark_emitter_self)
				return false;

			Self  = player_bark_emitter_self;
			Other = player_bark_emitter_other;
			return true;
		}
	}

	/// Activates Bark utility functions. Installs the BarkEmitter hook so the live *player* BarkEmitter
	/// Self/Other are latched for TryGetPlayerBarkEmitterContext (cleared on return-to-title via the
	/// setup_main_screen pub/sub). Cascades to MMAPI::Instance::Enable.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Bark::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Bark, MMAPI::Instance);

		MMAPI::Internal::RegisterOnSetupMainScreenHandler(Internal::ClearBarkEmitterOnReturnToTitle);

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ MMAPI::Internal::GML_SCRIPT_SETUP_MAIN_SCREEN, reinterpret_cast<PVOID>(MMAPI::Internal::GmlScriptBeforeSetupMainScreenCallback) },
			{ Internal::GML_SCRIPT_BARK_EMITTER,             reinterpret_cast<PVOID>(Internal::BarkEmitterContextCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Emits a bark (voiced dialogue bubble) over Ari with the given icon.
	/// @attention Requires MMAPI::Bark::Enable() to have been called.
	/// @param icon The bark icon to display.
	/// @param bark_type An engine-specific bark type code; defaults to 0.
	/// @return True if the bark was emitted, false if the required context is unavailable.
	inline bool Emit(MMAPI::Bark::Icons icon, int bark_type = 0)
	{
		MMAPI_REQUIRE_ENABLED("Bark", false);

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!Internal::TryGetPlayerBarkEmitterContext(Self, Other))
			return false;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_BARK_EMITTER_EMIT, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue bark_id    = static_cast<int>(icon);
		YYTK::RValue bark_type_value = bark_type;
		YYTK::RValue result;
		YYTK::RValue* args[2] = { &bark_id, &bark_type_value };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 2, args);
		return true;
	}
}
