// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Engine.hpp"
#include "Location.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Grid
{
	namespace Internal
	{
		inline constexpr const char* GML_SCRIPT_NEW_DAY = "gml_Script_new_day@Grid@Grid";

		/// Formats a Location::Ids as `name (id=N)` for log messages, falling back to `<unknown>`
		/// if the internal-name lookup returns empty.
		inline std::string LocationLabel(MMAPI::Location::Ids location)
		{
			std::string name = MMAPI::Location::LocationIdToString(location);
			if (name.empty())
				name = "<unknown>";
			return name + " (id=" + std::to_string(static_cast<int>(location)) + ")";
		}
	}

	/// Runs one in-game day's worth of progression for a single location's grid by invoking
	/// `gml_Script_new_day@Grid@Grid` with `Self = globalInstance.__grids[location]`. This is the
	/// same per-grid daily tick the game runs when the player sleeps — it advances bush tiers,
	/// regrows trees and stumps, restores grass, and progresses crops/forage that live in that
	/// grid. Internally it appears to clear node state and call `bush_node_new_day` /
	/// `tree_node_new_day` / `crop_node_new_day` on each cell to advance + restore it.
	///
	/// @attention **This is a holistic daily tick, not a narrow "advance bushes only" call.** Trees
	/// that were chopped will re-stump/regrow, grass will return, crops in the grid will progress.
	/// Behaviour matches the natural sleep cycle for that grid. There is no narrower built-in
	/// mechanism — suppressing the per-node-type scripts during this call deletes the affected
	/// nodes (their restoration step never runs), which then crashes Ari's next collision check
	/// when he walks onto an emptied cell.
	///
	/// @attention **The player must currently be in the target location.** Outside that, the
	/// grid's runtime node arrays are unpopulated even though the struct still exists, and
	/// running Grid::new_day on that empty data wipes the grid permanently (verified empirically:
	/// pressing F8 from the farmhouse to advance Narrows emptied Narrows of all trees/stumps/
	/// grass/forage and corrupted the save). The grid's `is_setup` flag does NOT distinguish
	/// these cases — it stays true for every initialized grid regardless of whether its nodes
	/// are currently loaded. We have no known runtime signal that does distinguish them, so this
	/// function takes the only demonstrably-safe path: refuse unless the player is in the target
	/// location.
	///
	/// @attention Requires `MMAPI::Location::Enable()` to have been called.
	///
	/// @param location The grid to advance by one day. Must equal the player's current location.
	/// @return Status::Success if the script was invoked; Status::InvalidParameter if the player
	///         isn't in `location`, or a resolution step failed; Status::NotInitialized if
	///         `MMAPI::Location::Enable()` wasn't called.
	inline MMAPI::Status AdvanceDay(MMAPI::Location::Ids location)
	{
		MMAPI::Location::Ids current;
		if (!MMAPI::Location::TryGetCurrentLocation(current))
		{
			MMAPI::Log::Warn("Grid::AdvanceDay refused: current location unresolved");
			return MMAPI::Status::InvalidParameter;
		}
		if (current != location)
		{
			MMAPI::Log::Warn("Grid::AdvanceDay refused: player is in %s but tried to advance %s. "
				"Running Grid::new_day on a non-active grid wipes its node data permanently. "
				"Walk to the target location first, then call this.",
				Internal::LocationLabel(current).c_str(),
				Internal::LocationLabel(location).c_str());
			return MMAPI::Status::InvalidParameter;
		}

		YYTK::RValue grids = MMAPI::Internal::global_instance->GetMember("__grids");
		if (grids.m_Kind != YYTK::VALUE_ARRAY)
		{
			MMAPI::Log::Warn("Grid::AdvanceDay: globalInstance.__grids is not an array");
			return MMAPI::Status::InvalidParameter;
		}

		YYTK::RValue* grid_entry = nullptr;
		MMAPI::Internal::module_interface->GetArrayEntry(grids, static_cast<int>(location), grid_entry);
		if (!grid_entry || grid_entry->m_Kind != YYTK::VALUE_OBJECT)
		{
			MMAPI::Log::Warn("Grid::AdvanceDay: __grids[%s] is not an object",
				Internal::LocationLabel(location).c_str());
			return MMAPI::Status::InvalidParameter;
		}

		YYTK::CInstance* grid_self = grid_entry->ToInstance();
		if (!grid_self)
		{
			MMAPI::Log::Warn("Grid::AdvanceDay: couldn't resolve __grids[%s] to a CInstance",
				Internal::LocationLabel(location).c_str());
			return MMAPI::Status::InvalidParameter;
		}

		YYTK::CScript* gml_script = nullptr;
		MMAPI::Internal::module_interface->GetNamedRoutinePointer(
			Internal::GML_SCRIPT_NEW_DAY,
			reinterpret_cast<PVOID*>(&gml_script));
		if (!gml_script)
		{
			MMAPI::Log::Warn("Grid::AdvanceDay: %s not resolvable", Internal::GML_SCRIPT_NEW_DAY);
			return MMAPI::Status::InvalidParameter;
		}

		YYTK::RValue result;
		gml_script->m_Functions->m_ScriptFunction(grid_self, grid_self, result, 0, nullptr);
		return MMAPI::Status::Success;
	}
}
