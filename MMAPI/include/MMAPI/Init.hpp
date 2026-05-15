// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Log.hpp"

namespace MMAPI
{
	/// Initializes MMAPI. Call once from ModuleInitialize.
	///
	/// Emits an INFO-level "MMAPI v<version> starting" banner as the very first output from MMAPI:
	/// - Console: always written, regardless of the Sinks bitmask.
	/// - File: written only if File is already in the Sinks bitmask at the time of this call.
	///   Default sinks are Console-only, so mods that want the banner captured to file must call
	///   `MMAPI::Log::SetSinks(Console | File)` BEFORE calling Initialize. Subsequent regular
	///   logging follows the bitmask as normal.
	///
	/// @param module_interface The YYTKInterface pointer received in ModuleInitialize.
	/// @param global The GML global instance pointer obtained from GetGlobalInstance.
	/// @param module The Aurie module pointer received in ModuleInitialize.
	/// @param mod_name The mod's name, used for log attribution and the per-mod log file path.
	/// @param mod_version The mod's version string, used in console log prefixes.
	inline void Initialize(
		YYTK::YYTKInterface* module_interface,
		YYTK::CInstance* global,
		Aurie::AurieModule* module,
		const char* mod_name,
		const char* mod_version
	)
	{
		// Diagnostic: surface missing pointers loudly. Without these, every downstream MMAPI call is a
		// silent no-op (utility functions fail their preconditions; hook installs return NotInitialized).
		// Bypass MMAPI::Log here because Log itself reads module_interface — if that's the missing one,
		// Log has nowhere to write. Fall back to YYTK Print directly when possible.
		if (!module_interface || !global || !module)
		{
			if (module_interface)
			{
				module_interface->Print(YYTK::CM_LIGHTRED,
					"[MMAPI::Initialize] missing required pointer "
					"(module_interface=%p, global=%p, module=%p) -- downstream MMAPI calls will fail",
					static_cast<void*>(module_interface),
					static_cast<void*>(global),
					static_cast<void*>(module));
			}
			// No interface at all means we can't log; the only signal will be downstream failures.
		}

		MMAPI::Internal::module_interface = module_interface;
		MMAPI::Internal::global_instance  = global;
		MMAPI::Internal::self_module      = module;
		MMAPI::Internal::mod_name         = mod_name    ? mod_name    : "";
		MMAPI::Internal::mod_version      = mod_version ? mod_version : "";

		// Announce MMAPI startup. Console always; file conditional on the Sinks bitmask at this
		// point (mods that want the banner in their log file should call SetSinks(Console|File)
		// BEFORE Initialize). Bypasses Log::Info so we can apply per-sink rules differently from
		// the normal Dispatch path — and so the banner is the very first MMAPI output regardless
		// of how subsequent logging is configured.
		if (module_interface)
		{
			char banner[96];
			std::snprintf(banner, sizeof(banner), ">>> Using MMAPI version %s <<<", MMAPI::VersionString);

			// Console — always, regardless of Sinks bitmask.
			module_interface->Print(YYTK::CM_LIGHTGREEN,
				"[%s %s] [INFO] %s",
				MMAPI::Internal::mod_name.c_str(),
				MMAPI::Internal::mod_version.c_str(),
				banner);

			// File — only if the mod has already enabled the File sink at this point.
			if (MMAPI::Log::HasSink(MMAPI::Log::Internal::current_sinks, MMAPI::Log::Sinks::File))
				MMAPI::Log::Internal::WriteTimestampedLine(MMAPI::Log::Level::Info, banner);
		}
	}
}
