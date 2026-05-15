// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Instance.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Perk
{
	/// Source: globalInstance.__perk__
	enum class Ids : int
	{
		ADayWellSpent            = 0,
		AWayToTheHeart           = 1,
		AbyssalAscendence        = 2,
		AncientInspiration       = 3,
		AppealingReeling         = 4,
		AppealingReelingTwo      = 5,
		AquaticAntiquities       = 6,
		AwardWinning             = 7,
		BackInVogue              = 8,
		BackInVogueTwo           = 9,
		BarnyardBounty           = 10,
		BarnyardBountyThree      = 11,
		BarnyardBountyTwo        = 12,
		BigWaterSprites          = 13,
		Bountiful                = 14,
		BreakOneGetTwo           = 15,
		CaffeineCrimes           = 16,
		CatchOfTheDay            = 17,
		CloseBond                = 18,
		CopperExpert             = 19,
		CurrencyOfCare           = 20,
		CurrencyOfCareThree      = 21,
		CurrencyOfCareTwo        = 22,
		DeliberateDebris         = 23,
		DinnerForTwo             = 24,
		DiscountTreats           = 25,
		DungeonDelicacies        = 26,
		EarthBreaker             = 27,
		EarthBreakerTwo          = 28,
		EarthlyEssence           = 29,
		EasternRoadScholar       = 30,
		Empowered                = 31,
		EmpoweredTwo             = 32,
		FairyCooking             = 33,
		FantasticFinds           = 34,
		FeedPrepper              = 35,
		FeedingFrenzy            = 36,
		Forager                  = 37,
		FormerFarmers            = 38,
		FortifiedBlacksmithing   = 39,
		Frenzy                   = 40,
		FriendShaped             = 41,
		FullClass                = 42,
		GeminiSeason             = 43,
		GenerousInDefeat         = 44,
		GenerousInDefeatTwo      = 45,
		GiftExchange             = 46,
		GoldExpert               = 47,
		GoodAsGold               = 48,
		GreenThumb               = 49,
		GuardiansShield          = 50,
		GuardiansShieldTwo       = 51,
		HammerTiming             = 52,
		HammerTimingThree        = 53,
		HammerTimingTwo          = 54,
		HarvestHorse             = 55,
		HarvestTime              = 56,
		HastyBlacksmithing       = 57,
		HeavyDuty                = 58,
		Horsepower               = 59,
		InMotion                 = 60,
		IronExpert               = 61,
		IronHound                = 62,
		JumpAttack               = 63,
		LeechBlacksmithing       = 64,
		Legendary                = 65,
		LightweightBlacksmithing = 66,
		LikableCooking           = 67,
		LivingOffTheLand         = 68,
		LostToHistory            = 69,
		LostToHistoryTwo         = 70,
		LoveableCooking          = 71,
		LuckyHaul                = 72,
		LuckyHaulTwo             = 73,
		Lumberjack               = 74,
		LumberjackTwo            = 75,
		MagicDesign              = 76,
		MagicalMeals             = 77,
		Masonry                  = 78,
		MaterialWorld            = 79,
		MaximumMilling           = 80,
		MineTime                 = 81,
		MistSight                = 82,
		MistrilExpert            = 83,
		MistrilMastery           = 84,
		MuseumQualityOne         = 85,
		MuseumQualityThree       = 86,
		MuseumQualityTwo         = 87,
		Natural                  = 88,
		NaturalBeauty            = 89,
		NaturalBeautyTwo         = 90,
		NiceRide                 = 91,
		NiceSwing                = 92,
		OreRiginal               = 93,
		Ornamental               = 94,
		OutOfJuice               = 95,
		PerfectCatch             = 96,
		PerfectPick              = 97,
		PerfectPollinators       = 98,
		PerfectPrefix            = 99,
		PlaytimeOne              = 100,
		PreparedPicker           = 101,
		PrizeWinning             = 102,
		Pursuit                  = 103,
		QualityCrafting          = 104,
		QuickFooted              = 105,
		Reclaimer                = 106,
		RefinedRockery           = 107,
		Refreshing               = 108,
		Resonance                = 109,
		RestorativeCooking       = 110,
		Rocking                  = 111,
		SchoolCrasher            = 112,
		Seasoned                 = 113,
		SetPieces                = 114,
		SharpBlacksmithing       = 115,
		ShrineSavant             = 116,
		SickleSword              = 117,
		SilverExpert             = 118,
		SilverSeeker             = 119,
		Snacktime                = 120,
		SonicBoom                = 121,
		SpeedyCooking            = 122,
		SteadySupplies           = 123,
		Stoneturner              = 124,
		SunkenSecrets            = 125,
		SunkenTreasure           = 126,
		SuperbSower              = 127,
		SureStrike               = 128,
		TasteMaker               = 129,
		TheBellTolls             = 130,
		TimeSensitive            = 131,
		TimeSensitiveFour        = 132,
		TimeSensitiveThree       = 133,
		TimeSensitiveTwo         = 134,
		TimeToEat                = 135,
		TimeToEatThree           = 136,
		TimeToEatTwo             = 137,
		TirelessBlacksmithing    = 138,
		TreasureHunter           = 139,
		TreasureTrove            = 140,
		Treasured                = 141,
		TrueBlue                 = 142,
		TrueStrike               = 143,
		TrueStrikeTwo            = 144,
		UndergroundInspiration   = 145,
		UnexpectedHaul           = 146,
		Unpeatable               = 147,
		VoidCrafting             = 148,
		VoidValue                = 149,
		WasteNotWantNot          = 150,
		WeedlineWatcher          = 151,
		WeedlineWatcherTwo       = 152,
		WelcomeHome              = 153,
		WelcomeHomeTwo           = 154,
		WellArmed                = 155,
		WellPlaced               = 156,
		WellWatered              = 157,
		WesternRuinsScholar      = 158,
		WhatACatch               = 159,
		WindDown                 = 160,
		WorkingWithTheGrain      = 161,
		WorkingWithTheGrainTwo   = 162
	};

	/// Total number of enumerators in Ids. Iterating [0, IdCount) covers every Ids value.
	inline constexpr int IdCount = 163;

	/// Invokes fn with every Ids value, in ascending order.
	template <typename Fn>
	inline void ForEachId(Fn fn)
	{
		for (int i = 0; i < IdCount; ++i)
			fn(static_cast<Ids>(i));
	}

	namespace Internal
	{
		inline bool enabled = false;

		inline constexpr const char* GML_SCRIPT_PERK_ACTIVE = "gml_Script_perk_active@Ari@Ari";
	}

	/// Activates Perk utility functions. Cascades to MMAPI::Instance::Enable so IsActive can resolve Ari's
	/// calling context internally.
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Perk::Enable() called");

		MMAPI_ENABLE_DEPENDENCY(MMAPI::Perk, MMAPI::Instance);

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Returns true if Ari has the given perk active.
	/// @attention Requires MMAPI::Perk::Enable() to have been called.
	/// @param perk The perk to check.
	inline bool IsActive(MMAPI::Perk::Ids perk)
	{
		MMAPI_REQUIRE_ENABLED("Perk", false);

		YYTK::CInstance* Self  = nullptr;
		YYTK::CInstance* Other = nullptr;
		if (!MMAPI::Instance::Internal::TryGetAriContext(Self, Other))
			return false;

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(Internal::GML_SCRIPT_PERK_ACTIVE, reinterpret_cast<PVOID*>(&gml_script));

		YYTK::RValue perk_id = static_cast<int>(perk);
		YYTK::RValue result;
		YYTK::RValue* args[1] = { &perk_id };
		gml_script->m_Functions->m_ScriptFunction(Self, Other, result, 1, args);
		return result.ToBoolean();
	}
}
