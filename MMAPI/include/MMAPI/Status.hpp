// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

namespace MMAPI
{
	/// Result code returned by MMAPI public APIs. Replaces the boolean and Aurie::AurieStatus return
	/// conventions used in earlier MMAPI versions, decoupling MMAPI's public surface from Aurie.
	enum class Status : int
	{
		Success           = 0,
		InvalidParameter  = 1,  ///< Caller passed a null/invalid argument.
		NotInitialized    = 2,  ///< MMAPI::Initialize was never called.
		AlreadyRegistered = 3,  ///< A callback is already bound to this hook.
		InstallFailed     = 4,  ///< Underlying Aurie hook install failed.
	};

	/// Returns true if the status indicates success.
	constexpr bool IsSuccess(Status s) noexcept { return s == Status::Success; }

	/// Returns the canonical string representation of a status code.
	inline constexpr const char* ToString(Status s) noexcept
	{
		switch (s)
		{
			case Status::Success:           return "Success";
			case Status::InvalidParameter:  return "InvalidParameter";
			case Status::NotInitialized:    return "NotInitialized";
			case Status::AlreadyRegistered: return "AlreadyRegistered";
			case Status::InstallFailed:     return "InstallFailed";
		}
		return "Unknown";
	}
}
