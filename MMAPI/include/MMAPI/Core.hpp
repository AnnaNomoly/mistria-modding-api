// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Status.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI
{
	/// MMAPI library version. Bump on each release. Mirrored by the NuGet package version
	/// in MMAPI.nuspec — keep them in sync. Available to mods at compile time for feature
	/// gating (`if constexpr (MMAPI::VersionMajor >= 1) { ... }`) or runtime reporting.
	inline constexpr int         VersionMajor  = 0;
	inline constexpr int         VersionMinor  = 1;
	inline constexpr int         VersionPatch  = 0;
	inline constexpr const char* VersionString = "0.1.0";

	namespace Internal
	{
		inline uint64_t GetCurrentSystemTime()
		{
			using namespace std::chrono;
			return static_cast<uint64_t>(
				duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()
			);
		}

		// Set by the mod at startup via Initialize().
		inline YYTK::YYTKInterface* module_interface = nullptr;

		// The GML global instance, typically obtained from the YYTK interface during initialization.
		inline YYTK::CInstance* global_instance = nullptr;

		// The Aurie module using MMAPI. Required for MMAPI-owned hooks.
		inline Aurie::AurieModule* self_module = nullptr;

		// Mod identity, passed to Initialize. Used for log attribution and per-mod log file paths.
		inline std::string mod_name;
		inline std::string mod_version;

		// Persisted game object instance pairs, keyed by a descriptive instance name.
		// Used for objects like obj_ari whose context is captured from an object callback rather than a script hook.
		inline std::map<std::string, std::vector<YYTK::CInstance*>> instance_reference_map;

		inline std::map<std::string, bool> owned_script_hook_installed_map;

		// Internal pub/sub list of handlers invoked from MMAPI's setup_main_screen hook,
		// before any user callback runs. Used by modules that need to (re)build state
		// when the game returns to the title screen with valid Self/Other context.
		using OnSetupMainScreenHandler = void(*)(YYTK::CInstance* Self, YYTK::CInstance* Other);
		inline std::vector<OnSetupMainScreenHandler> on_setup_main_screen_internal_handlers;

		inline void RegisterOnSetupMainScreenHandler(OnSetupMainScreenHandler handler)
		{
			for (auto existing : on_setup_main_screen_internal_handlers)
				if (existing == handler)
					return;
			on_setup_main_screen_internal_handlers.push_back(handler);
		}

		// Internal pub/sub list of handlers invoked when the game first becomes interactive each
		// session — fired from Weather's get_weather callback alongside the user-facing
		// MMAPI::Game::Hooks::AfterGameActive registration. Used by modules that need to flip
		// internal "game is settled" gating without claiming the single-slot user callback.
		// Reset to "not active" via the setup_main_screen pub/sub on return-to-title.
		using OnGameActiveHandler = void(*)();
		inline std::vector<OnGameActiveHandler> on_game_active_internal_handlers;

		inline void RegisterOnGameActiveHandler(OnGameActiveHandler handler)
		{
			for (auto existing : on_game_active_internal_handlers)
				if (existing == handler)
					return;
			on_game_active_internal_handlers.push_back(handler);
		}

		/// Captures a pair of game object instance pointers the first time it is seen.
		/// Used internally by MMAPI::Instance::Enable()'s EVENT_OBJECT_CALL dispatcher.
		inline void RegisterInstanceContext(const char* instance_name, YYTK::CInstance* Self, YYTK::CInstance* Other)
		{
			if (!instance_reference_map.contains(instance_name))
				instance_reference_map[instance_name] = { Self, Other };
		}

		// MMAPI-wide setup_main_screen hook. Owned by Core because its primary job is core infrastructure:
		// clearing the instance reference map on return-to-title and dispatching the internal handler pub/sub list.
		// `MMAPI::Game::Hooks::BeforeSetupMainScreen` is a thin user-facing wrapper that registers a callback here.
		inline constexpr const char* GML_SCRIPT_SETUP_MAIN_SCREEN = "gml_Script_setup_main_screen@TitleMenu@TitleMenu";

		using BeforeSetupMainScreenCallback = void(*)();
		inline BeforeSetupMainScreenCallback before_setup_main_screen_callback = nullptr;

		inline YYTK::RValue& GmlScriptBeforeSetupMainScreenCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			instance_reference_map.clear();

			for (auto handler : on_setup_main_screen_internal_handlers)
				handler(Self, Other);

			if (before_setup_main_screen_callback)
				before_setup_main_screen_callback();

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(self_module, GML_SCRIPT_SETUP_MAIN_SCREEN)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);
			return Result;
		}
	}

	// MMAPI::Initialize lives in Init.hpp (loaded after Log.hpp via MMAPI.hpp) so the startup
	// banner it emits can route through Log infrastructure. Core.hpp can't include Log.hpp
	// because Log.hpp itself depends on Core.hpp.
}

// =============================================================================
// Enable-contract macros.
//
// These reference MMAPI::Log::Warn / MMAPI::Log::DebugFile / MMAPI::IsSuccess at
// expansion site. Every MMAPI module file already includes Log.hpp (which pulls
// Status.hpp and Core.hpp in turn), so no extra includes are required at any
// call site.
// =============================================================================

// Contract enforcement: use at the top of public utility functions to warn when
// callers invoke the function before the module's Enable(). The Internal::enabled
// flag must be defined in the surrounding Internal namespace by each module.
//
// Non-void return form — provide a fallback return value.
//   MMAPI_REQUIRE_ENABLED("Animal", false);
//   MMAPI_REQUIRE_ENABLED("Spell", {});
#define MMAPI_REQUIRE_ENABLED(module_label, return_value)                          \
	do {                                                                           \
		if (!Internal::enabled)                                                    \
		{                                                                          \
			MMAPI::Log::Warn("%s called before " module_label "::Enable()",        \
				__FUNCTION__);                                                     \
			return return_value;                                                   \
		}                                                                          \
	} while (0)

// Void return form.
//   MMAPI_REQUIRE_ENABLED_VOID("Animal");
#define MMAPI_REQUIRE_ENABLED_VOID(module_label)                                   \
	do {                                                                           \
		if (!Internal::enabled)                                                    \
		{                                                                          \
			MMAPI::Log::Warn("%s called before " module_label "::Enable()",        \
				__FUNCTION__);                                                     \
			return;                                                                \
		}                                                                          \
	} while (0)

// Cascade enabling of a dependency module. Use inside the caller's Enable() body
// or inside a Hooks::* registrar (where the caller's own Enable must run first).
//
// First arg is the caller's fully-qualified identifier (e.g. `MMAPI::Item` for an
// Enable() body, `MMAPI::Item::Hooks::BeforeUseItem` for a registrar). The arg is
// stringified for graph keys and log messages — it is not a real C++ identifier
// reference, so any token sequence will work, but conventionally matches the
// surrounding scope so the graph rendering is meaningful.
//
// Second arg is the dependency module (must be a real namespace expression — the
// macro emits `<arg>::Enable()`).
//
// On success: emits a TRACE dependency-edge entry (file-only by TRACE policy) AND
// records the edge into MMAPI::Log::Internal::dependency_graph for later structured
// rendering via MMAPI::Log::DumpDependencyGraphFlat() or DumpDependencyGraphTree().
// Console stays quiet regardless of sink config — these are noisy by design and only
// useful when debugging initialization order.
// On failure: emits a WARN entry to both sinks naming the failed dependency and
// short-circuits the caller with the failure status.
//
// The local status is scoped via do-while so adjacent invocations don't collide,
// and the prefixed name avoids clashing with any caller-local `status` variable.
//
//   MMAPI_ENABLE_DEPENDENCY(MMAPI::Item, MMAPI::Instance);
//   MMAPI_ENABLE_DEPENDENCY(MMAPI::Item, MMAPI::Text);
//   MMAPI_ENABLE_DEPENDENCY(MMAPI::Item::Hooks::BeforeUseItem, MMAPI::Item);
#define MMAPI_ENABLE_DEPENDENCY(caller_qualified_name, module_qualified_name)      \
	do {                                                                           \
		MMAPI::Log::Internal::RecordDependencyEdge(                                \
			#caller_qualified_name, #module_qualified_name);                       \
		MMAPI::Log::Trace(                                                         \
			#caller_qualified_name " -> " #module_qualified_name);                 \
		MMAPI::Status mmapi_dep_status_ = module_qualified_name::Enable();         \
		if (!MMAPI::IsSuccess(mmapi_dep_status_))                                  \
		{                                                                          \
			MMAPI::Log::Warn(                                                      \
				#caller_qualified_name ": dependency "                             \
				#module_qualified_name "::Enable() failed");                       \
			return mmapi_dep_status_;                                              \
		}                                                                          \
	} while (0)
