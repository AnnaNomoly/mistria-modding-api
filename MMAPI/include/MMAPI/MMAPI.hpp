// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Status.hpp"
#include "Core.hpp"
#include "Log.hpp"
#include "Hook.hpp"
#include "Anchor.hpp"
#include "Animal.hpp"
#include "Archaeology.hpp"
#include "Bark.hpp"
#include "Bug.hpp"
#include "Calendar.hpp"
// Config.hpp is opt-in — requires nlohmann/json.hpp. Include it explicitly if your mod uses MMAPI::Config.
#include "Cosmetic.hpp"
#include "Crafting.hpp"
#include "CrossMod.hpp"
#include "Cutscene.hpp"
#include "Damage.hpp"
#include "Display.hpp"
#include "Dungeon.hpp"
#include "Engine.hpp"
#include "Equipment.hpp"
#include "Fish.hpp"
#include "Game.hpp"
#include "Gossip.hpp"
#include "Grid.hpp"
#include "Infusion.hpp"
#include "Input.hpp"
#include "Inventory.hpp"
#include "Instance.hpp"
#include "Item.hpp"
#include "Location.hpp"
#include "Mail.hpp"
#include "Math.hpp"
// ModSave.hpp is opt-in — requires nlohmann/json.hpp. Include it explicitly if your mod uses MMAPI::ModSave.
#include "Monster.hpp"
#include "NPC.hpp"
#include "Object.hpp"
#include "Perk.hpp"
#include "Pet.hpp"
#include "Player.hpp"
#include "Recipe.hpp"
#include "Renown.hpp"
#include "Room.hpp"
#include "Skill.hpp"
#include "Spell.hpp"
#include "StatusEffect.hpp"
#include "T2.hpp"
#include "Text.hpp"
#include "ToolbarMenu.hpp"
#include "Tutorial.hpp"
#include "VitalsMenu.hpp"
#include "Weather.hpp"

// Init.hpp must come last so MMAPI::Initialize can reference Log::Internal helpers — Log.hpp
// can't be included from Core.hpp (cyclic), so Initialize lives here instead of in Core.
#include "Init.hpp"
