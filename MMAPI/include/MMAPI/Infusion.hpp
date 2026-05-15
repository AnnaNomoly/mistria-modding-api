// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

namespace MMAPI::Infusion
{
	/// Source: globalInstance.__infusion__
	enum class Ids : int
	{
		Fairy         = 0,
		FireSword     = 1,
		Fortified     = 2,
		Hasty         = 3,
		IceSword      = 4,
		InstantKill   = 5,
		Leeching      = 6,
		Lightweight   = 7,
		Likeable      = 8,
		Loveable      = 9,
		Magical       = 10,
		Quality       = 11,
		Restorative   = 12,
		Sharp         = 13,
		Speedy        = 14,
		StackingSpeed = 15,
		SureStrike    = 16,
		Tireless      = 17,
		VenomSword    = 18
	};

	/// Total number of enumerators in Ids. Iterating [0, IdCount) covers every Ids value.
	inline constexpr int IdCount = 19;

	/// Invokes fn with every Ids value, in ascending order.
	template <typename Fn>
	inline void ForEachId(Fn fn)
	{
		for (int i = 0; i < IdCount; ++i)
			fn(static_cast<Ids>(i));
	}

	namespace Internal
	{
		inline constexpr const char* ToGameKey(MMAPI::Infusion::Ids infusion)
		{
			switch (infusion)
			{
				case MMAPI::Infusion::Ids::Fairy:
					return "fairy";
				case MMAPI::Infusion::Ids::FireSword:
					return "fire_sword";
				case MMAPI::Infusion::Ids::Fortified:
					return "fortified";
				case MMAPI::Infusion::Ids::Hasty:
					return "hasty";
				case MMAPI::Infusion::Ids::IceSword:
					return "ice_sword";
				case MMAPI::Infusion::Ids::InstantKill:
					return "instant_kill";
				case MMAPI::Infusion::Ids::Leeching:
					return "leeching";
				case MMAPI::Infusion::Ids::Lightweight:
					return "lightweight";
				case MMAPI::Infusion::Ids::Likeable:
					return "likeable";
				case MMAPI::Infusion::Ids::Loveable:
					return "loveable";
				case MMAPI::Infusion::Ids::Magical:
					return "magical";
				case MMAPI::Infusion::Ids::Quality:
					return "quality";
				case MMAPI::Infusion::Ids::Restorative:
					return "restorative";
				case MMAPI::Infusion::Ids::Sharp:
					return "sharp";
				case MMAPI::Infusion::Ids::Speedy:
					return "speedy";
				case MMAPI::Infusion::Ids::StackingSpeed:
					return "stacking_speed";
				case MMAPI::Infusion::Ids::SureStrike:
					return "sure_strike";
				case MMAPI::Infusion::Ids::Tireless:
					return "tireless";
				case MMAPI::Infusion::Ids::VenomSword:
					return "venom_sword";
				default:
					return nullptr;
			}
		}
	}

	/// Returns the game's internal string key for an infusion.
	/// @param infusion The infusion ID.
	/// @return The internal game key, or nullptr if the value is unknown.
	inline constexpr const char* GetInternalName(MMAPI::Infusion::Ids infusion)
	{
		return Internal::ToGameKey(infusion);
	}
}
