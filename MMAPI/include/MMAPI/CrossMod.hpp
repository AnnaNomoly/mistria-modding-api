// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Engine.hpp"
#include "Log.hpp"

#include <string>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::CrossMod
{
	namespace Internal
	{
		// The GML global key used as the IPC root for inter-mod struct sharing.
		inline constexpr const char* YYTK_GLOBAL = "__YYTK";
	}

	/// Returns the global `__YYTK` cross-mod IPC root struct, lazily creating it as a global on
	/// first call. This is the canonical container mods use to publish state to (and read state
	/// from) other mods, keyed by mod name.
	/// @return The __YYTK struct, or an empty RValue if globalInstance is unavailable.
	inline YYTK::RValue GetOrCreateRoot()
	{
		if (!MMAPI::Internal::global_instance)
			return {};

		if (!MMAPI::Engine::GlobalVariableExists(Internal::YYTK_GLOBAL))
		{
			YYTK::RValue created;
			MMAPI::Internal::module_interface->GetRunnerInterface().StructCreate(&created);
			MMAPI::Engine::GlobalVariableSet(Internal::YYTK_GLOBAL, created);
			return created;
		}
		return MMAPI::Engine::GlobalVariableGet(Internal::YYTK_GLOBAL);
	}

	/// Returns the calling mod's sub-struct in `__YYTK`, lazily creating both `__YYTK` and the
	/// per-mod sub-struct on first call. Keyed by the `mod_name` passed to MMAPI::Initialize.
	///
	/// Use this from the producer side to publish state for other mods to read:
	/// @code
	/// YYTK::RValue me = MMAPI::CrossMod::GetOrCreateMyStruct();
	/// MMAPI::Engine::StructVariableSet(me, "version", VERSION);
	/// MMAPI::Engine::StructVariableSet(me, "is_active", false);
	/// @endcode
	///
	/// @return The calling mod's sub-struct, or an empty RValue if MMAPI is uninitialized or
	///         globalInstance is unavailable.
	inline YYTK::RValue GetOrCreateMyStruct()
	{
		if (MMAPI::Internal::mod_name.empty())
		{
			MMAPI::Log::Warn("CrossMod::GetOrCreateMyStruct called before MMAPI::Initialize.");
			return {};
		}

		YYTK::RValue root = GetOrCreateRoot();
		if (root.m_Kind != YYTK::VALUE_OBJECT) return {};

		const char* name = MMAPI::Internal::mod_name.c_str();
		if (!MMAPI::Engine::StructVariableExists(root, name))
		{
			YYTK::RValue created;
			MMAPI::Internal::module_interface->GetRunnerInterface().StructCreate(&created);
			MMAPI::Internal::module_interface->GetRunnerInterface().StructAddRValue(&root, name, &created);
			return created;
		}
		return MMAPI::Engine::StructVariableGet(root, name);
	}

	/// Returns another mod's sub-struct from `__YYTK`, or an empty RValue if the mod hasn't
	/// registered. Use this from the consumer side to read state another mod has published —
	/// always check the result's `m_Kind == YYTK::VALUE_OBJECT` before accessing members.
	///
	/// @code
	/// YYTK::RValue dd = MMAPI::CrossMod::TryGetModStruct("DeepDungeon");
	/// if (dd.m_Kind == YYTK::VALUE_OBJECT && MMAPI::Engine::StructVariableExists(dd, "floor"))
	/// {
	///     int floor = static_cast<int>(dd.GetMember("floor").ToInt64());
	///     ...
	/// }
	/// @endcode
	///
	/// @param mod_name The name of the other mod (e.g. "DeepDungeon").
	/// @return The mod's sub-struct, or an empty RValue if `__YYTK` or the mod's struct doesn't exist.
	inline YYTK::RValue TryGetModStruct(const std::string& mod_name)
	{
		if (!MMAPI::Internal::global_instance) return {};
		if (!MMAPI::Engine::GlobalVariableExists(Internal::YYTK_GLOBAL)) return {};

		YYTK::RValue root = MMAPI::Engine::GlobalVariableGet(Internal::YYTK_GLOBAL);
		if (root.m_Kind != YYTK::VALUE_OBJECT) return {};

		if (!MMAPI::Engine::StructVariableExists(root, mod_name.c_str())) return {};

		return MMAPI::Engine::StructVariableGet(root, mod_name.c_str());
	}
}
