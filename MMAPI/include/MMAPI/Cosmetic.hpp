// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Engine.hpp"

#include <string>
#include <utility>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Cosmetic
{
	namespace Internal
	{
		inline YYTK::RValue GetCosmeticUnlocks()
		{
			YYTK::RValue ari = MMAPI::Internal::global_instance->GetMember("__ari");
			if (ari.m_Kind == YYTK::VALUE_UNDEFINED)
				return {};

			YYTK::RValue cosmetic_unlocks = ari.GetMember("cosmetic_unlocks");
			if (cosmetic_unlocks.m_Kind == YYTK::VALUE_UNDEFINED)
				return {};

			return cosmetic_unlocks.GetMember("inner");
		}
	}

	/// Returns true if the named cosmetic is equipped in Ari's currently selected preset.
	/// @param cosmetic_name The internal cosmetic name to check.
	inline bool IsEquipped(const std::string& cosmetic_name)
	{
		YYTK::RValue ari = MMAPI::Internal::global_instance->GetMember("__ari");
		int preset_index = static_cast<int>(ari.GetMember("preset_index_selected").ToInt64());

		YYTK::RValue presets = ari.GetMember("presets");
		YYTK::RValue preset_buffer = presets.GetMember("__buffer");

		size_t preset_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(preset_buffer, preset_count);
		if (preset_index < 0 || static_cast<size_t>(preset_index) >= preset_count)
			return false;

		YYTK::RValue* selected_preset = nullptr;
		MMAPI::Internal::module_interface->GetArrayEntry(preset_buffer, static_cast<size_t>(preset_index), selected_preset);

		YYTK::RValue assets = selected_preset->GetMember("assets");
		YYTK::RValue asset_buffer = assets.GetMember("__buffer");

		size_t asset_count = 0;
		MMAPI::Internal::module_interface->GetArraySize(asset_buffer, asset_count);

		for (size_t i = 0; i < asset_count; i++)
		{
			YYTK::RValue* equipped_cosmetic = nullptr;
			MMAPI::Internal::module_interface->GetArrayEntry(asset_buffer, i, equipped_cosmetic);

			if (equipped_cosmetic->GetMember("name").ToString() == cosmetic_name)
				return true;
		}

		return false;
	}

	/// Returns true if Ari has unlocked the given cosmetic.
	/// @param cosmetic_name The internal cosmetic name to check.
	inline bool IsUnlocked(const std::string& cosmetic_name)
	{
		YYTK::RValue cosmetic_unlocks = Internal::GetCosmeticUnlocks();
		if (cosmetic_unlocks.m_Kind == YYTK::VALUE_UNDEFINED)
			return false;

		return MMAPI::Engine::StructVariableExists(cosmetic_unlocks, cosmetic_name.c_str());
	}

	/// Unlocks the given cosmetic for Ari.
	/// @param cosmetic_name The internal cosmetic name to unlock.
	/// @return True if the cosmetic was newly unlocked; otherwise false.
	inline bool Unlock(const std::string& cosmetic_name)
	{
		YYTK::RValue cosmetic_unlocks = Internal::GetCosmeticUnlocks();
		if (cosmetic_unlocks.m_Kind == YYTK::VALUE_UNDEFINED)
			return false;

		if (IsUnlocked(cosmetic_name))
			return false;

		MMAPI::Engine::StructVariableSet(cosmetic_unlocks, cosmetic_name.c_str(), 0.0);
		return true;
	}

	/// Invokes fn with every known cosmetic by walking `globalInstance.__pad.player_assets.inner` —
	/// the game's master cosmetic table. fn is called as `fn(cosmetic_name, cosmetic_data)` where
	/// `cosmetic_data` is the per-cosmetic struct (its `name` member holds the localization key for
	/// the cosmetic's display name).
	///
	/// No Enable required — this is a pure read of globalInstance.
	template <typename Fn>
	inline void ForEachCosmetic(Fn fn)
	{
		if (!MMAPI::Internal::global_instance)
			return;

		YYTK::RValue pad = MMAPI::Internal::global_instance->GetMember("__pad");
		if (pad.m_Kind == YYTK::VALUE_UNDEFINED) return;

		YYTK::RValue player_assets = pad.GetMember("player_assets");
		if (player_assets.m_Kind == YYTK::VALUE_UNDEFINED) return;

		YYTK::RValue inner = player_assets.GetMember("inner");
		if (inner.m_Kind == YYTK::VALUE_UNDEFINED) return;

		MMAPI::Internal::module_interface->EnumInstanceMembers(inner,
			[&fn](const char* member_name, YYTK::RValue* value)
			{
				if (member_name && value)
					fn(std::string(member_name), *value);
				return false;
			}
		);
	}
}
