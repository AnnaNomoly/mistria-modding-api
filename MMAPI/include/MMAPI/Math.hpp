// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Math
{
	/// Returns the Euclidean distance between two points.
	/// @param x1 The X coordinate of the first point.
	/// @param y1 The Y coordinate of the first point.
	/// @param x2 The X coordinate of the second point.
	/// @param y2 The Y coordinate of the second point.
	/// @return The distance between the two points.
	inline double GetDistance(double x1, double y1, double x2, double y2)
	{
		return MMAPI::Internal::module_interface->CallBuiltin("point_distance", { x1, y1, x2, y2 }).ToDouble();
	}
}
