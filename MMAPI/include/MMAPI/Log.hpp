// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Status.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Log
{
	/// Severity levels. Trace is the noisiest (per-fire), Error the rarest (unrecoverable).
	enum class Level : int
	{
		Trace = 0,
		Debug = 1,
		Info  = 2,
		Warn  = 3,
		Error = 4,
	};

	/// Bitmask of active log destinations. Combine with operator|.
	enum class Sinks : int
	{
		None    = 0,
		Console = 1 << 0,
		File    = 1 << 1,
	};

	constexpr Sinks operator|(Sinks a, Sinks b) noexcept
	{
		return static_cast<Sinks>(static_cast<int>(a) | static_cast<int>(b));
	}

	constexpr Sinks operator&(Sinks a, Sinks b) noexcept
	{
		return static_cast<Sinks>(static_cast<int>(a) & static_cast<int>(b));
	}

	constexpr bool HasSink(Sinks all, Sinks one) noexcept
	{
		return (static_cast<int>(all) & static_cast<int>(one)) != 0;
	}

	namespace Internal
	{
		// Maximum formatted message length. Messages exceeding this are truncated.
		// Chosen to fit a typical stack buffer while covering >99% of MMAPI log lines.
		inline constexpr size_t kMessageBufferSize = 1024;

		inline Level                  current_level   = Level::Info;
		inline Sinks                  current_sinks   = Sinks::Console;
		inline std::filesystem::path  file_path;
		inline std::ofstream          file_stream;
		inline bool                   file_path_initialized = false;

		// Adjacency list of recorded dependency edges, keyed by caller name (typically a
		// Hooks::* registrar name or another module's Enable). Populated by the
		// MMAPI_ENABLE_DEPENDENCY macro and rendered by Log::DumpDependencyGraphFlat() or
		// Log::DumpDependencyGraphTree(). Edges are deduplicated per caller so repeated
		// cascades don't bloat the structural view.
		//
		// Two parallel structures so the dumps can render in invocation order:
		// - `dependency_graph_caller_order` preserves the order in which distinct callers
		//   first appeared (read top-to-bottom = cascade sequence during init).
		// - `dependency_graph` provides O(log n) lookup for dedup checks.
		// Within a caller's adjacency vector, dependency entries are themselves in
		// invocation order (vector preserves push order).
		inline std::vector<std::string> dependency_graph_caller_order;
		inline std::map<std::string, std::vector<std::string>> dependency_graph;

		inline void RecordDependencyEdge(const char* caller, const char* dependency)
		{
			if (!caller || !dependency) return;
			auto [it, inserted] = dependency_graph.try_emplace(caller);
			if (inserted)
				dependency_graph_caller_order.push_back(caller);
			auto& deps = it->second;
			if (std::find(deps.begin(), deps.end(), dependency) == deps.end())
				deps.push_back(dependency);
		}

		constexpr YYTK::CmColor LevelColor(Level level) noexcept
		{
			switch (level)
			{
				case Level::Trace: return YYTK::CM_WHITE;
				case Level::Debug: return YYTK::CM_AQUA;
				case Level::Info:  return YYTK::CM_LIGHTGREEN;
				case Level::Warn:  return YYTK::CM_LIGHTYELLOW;
				case Level::Error: return YYTK::CM_LIGHTRED;
			}
			return YYTK::CM_WHITE;
		}

		constexpr const char* LevelTag(Level level) noexcept
		{
			switch (level)
			{
				case Level::Trace: return "TRACE";
				case Level::Debug: return "DEBUG";
				case Level::Info:  return "INFO";
				case Level::Warn:  return "WARN";
				case Level::Error: return "ERROR";
			}
			return "?????";
		}

		constexpr const char* LevelTagPadded(Level level) noexcept
		{
			switch (level)
			{
				case Level::Trace: return "TRACE";
				case Level::Debug: return "DEBUG";
				case Level::Info:  return "INFO ";
				case Level::Warn:  return "WARN ";
				case Level::Error: return "ERROR";
			}
			return "?????";
		}

		inline std::filesystem::path DefaultFilePath(const std::string& mod_name)
		{
			return std::filesystem::current_path()
				/ "mod_data"
				/ mod_name
				/ "logs"
				/ ("MMAPI-" + mod_name + ".log");
		}

		inline void EnsureFilePathInitialized()
		{
			if (file_path_initialized)
				return;

			if (MMAPI::Internal::mod_name.empty())
				return;

			file_path             = DefaultFilePath(MMAPI::Internal::mod_name);
			file_path_initialized = true;
		}

		// One-shot console reports on file-sink failure. Without these, an unwritable file path
		// (e.g. UAC-protected Program Files installs, antivirus blocks, missing permissions)
		// produces zero file output and zero diagnostics — the user sees "log file missing" with
		// no way to tell why. Console-only because the file sink is by definition broken at this
		// point; the reports use module_interface->Print directly to avoid recursion through Dispatch.
		inline bool file_failure_reported = false;

		inline void ReportFileSinkFailureOnce(const char* what, const std::filesystem::path& path, const std::error_code& ec)
		{
			if (file_failure_reported || !MMAPI::Internal::module_interface)
				return;
			file_failure_reported = true;
			MMAPI::Internal::module_interface->Print(
				YYTK::CM_LIGHTYELLOW,
				"[%s %s] [WARN] MMAPI log file sink: %s failed for '%s' (error %d: %s) — file logging disabled",
				MMAPI::Internal::mod_name.c_str(),
				MMAPI::Internal::mod_version.c_str(),
				what,
				path.string().c_str(),
				ec.value(),
				ec.message().c_str()
			);
		}

		// One-shot console warning when TRACE is enabled (level passes the filter) but the File
		// sink is off — TRACE is file-only by policy, so without File the message is dropped.
		// Without this warning, a user setting SetLevel(Trace) without SetSinks(... | File) sees
		// nothing in either sink and has no signal that messages are being silently discarded.
		inline bool trace_without_file_reported = false;

		inline void ReportTraceWithoutFileSinkOnce()
		{
			if (trace_without_file_reported || !MMAPI::Internal::module_interface)
				return;
			trace_without_file_reported = true;
			MMAPI::Internal::module_interface->Print(
				YYTK::CM_LIGHTYELLOW,
				"[%s %s] [WARN] MMAPI Log: Trace level is active but File sink is disabled — TRACE messages are being dropped. Call MMAPI::Log::SetSinks(MMAPI::Log::Sinks::Console | MMAPI::Log::Sinks::File) to capture them.",
				MMAPI::Internal::mod_name.c_str(),
				MMAPI::Internal::mod_version.c_str()
			);
		}

		inline void WriteTimestampedLine(Level level, const char* message)
		{
			EnsureFilePathInitialized();

			if (!file_stream.is_open() && !file_path.empty() && !file_failure_reported)
			{
				std::error_code ec;
				std::filesystem::create_directories(file_path.parent_path(), ec);
				if (ec)
				{
					ReportFileSinkFailureOnce("create_directories", file_path.parent_path(), ec);
					return;
				}

				// Truncate on first write each session, matching YYTK's log behavior.
				// Subsequent writes append to the same open stream.
				file_stream.open(file_path, std::ios::out | std::ios::trunc);
				if (!file_stream.is_open())
				{
					// ofstream::open doesn't surface a std::error_code; we use errno (set by the
					// underlying libc fopen) and route through std::generic_category for a readable
					// message. On Windows MSVC, errno is set to EACCES / ENOENT / EBUSY etc.
					std::error_code open_ec(errno, std::generic_category());
					ReportFileSinkFailureOnce("ofstream::open", file_path, open_ec);
					return;
				}
			}

			if (!file_stream.is_open())
				return;

			using namespace std::chrono;
			auto now  = system_clock::now();
			auto ms   = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
			std::time_t t = system_clock::to_time_t(now);
			std::tm tm{};
			localtime_s(&tm, &t);

			char timestamp[32];
			std::snprintf(timestamp, sizeof(timestamp),
				"%04d-%02d-%02d %02d:%02d:%02d.%03lld",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec,
				static_cast<long long>(ms.count()));

			file_stream << timestamp << " [" << LevelTagPadded(level) << "] " << message << '\n';
			file_stream.flush();
		}

		inline void Dispatch(Level level, const char* message)
		{
			// TRACE is file-only by policy — regardless of Sinks bitmask, TRACE messages never
			// reach the console. Reserved for high-volume diagnostic plumbing (dependency edges,
			// individual script-hook installations, etc.) that would drown the console.
			const bool console_enabled = level != Level::Trace
				&& HasSink(current_sinks, Sinks::Console)
				&& MMAPI::Internal::module_interface;

			if (console_enabled)
			{
				MMAPI::Internal::module_interface->Print(
					LevelColor(level),
					"[%s %s] [%s] %s",
					MMAPI::Internal::mod_name.c_str(),
					MMAPI::Internal::mod_version.c_str(),
					LevelTag(level),
					message
				);
			}

			if (HasSink(current_sinks, Sinks::File))
				WriteTimestampedLine(level, message);
			else if (level == Level::Trace)
				ReportTraceWithoutFileSinkOnce();
		}

		/// Formats the message into a stack buffer and dispatches to active sinks.
		/// If snprintf would write more than kMessageBufferSize bytes, the message is truncated
		/// and "..." is written into the last three bytes before the null terminator so readers
		/// know the line is incomplete.
		template <typename... Args>
		inline void FormatAndDispatch(Level level, const char* format, Args... args)
		{
			if (static_cast<int>(level) < static_cast<int>(current_level))
				return;

			char buffer[kMessageBufferSize];
			int written = std::snprintf(buffer, sizeof(buffer), format, args...);
			if (written > 0 && static_cast<size_t>(written) >= sizeof(buffer))
			{
				buffer[sizeof(buffer) - 4] = '.';
				buffer[sizeof(buffer) - 3] = '.';
				buffer[sizeof(buffer) - 2] = '.';
				buffer[sizeof(buffer) - 1] = '\0';
			}

			Dispatch(level, buffer);
		}

		/// Like FormatAndDispatch but unconditionally bypasses the console sink — writes only
		/// to the file sink, gated by current_level and the File sink being in current_sinks.
		/// Used for high-volume diagnostic output that would clutter the console (e.g. the
		/// dependency resolution edges emitted by MMAPI_ENABLE_DEPENDENCY).
		template <typename... Args>
		inline void FormatAndDispatchFileOnly(Level level, const char* format, Args... args)
		{
			if (static_cast<int>(level) < static_cast<int>(current_level))
				return;
			if (!HasSink(current_sinks, Sinks::File))
				return;

			char buffer[kMessageBufferSize];
			int written = std::snprintf(buffer, sizeof(buffer), format, args...);
			if (written > 0 && static_cast<size_t>(written) >= sizeof(buffer))
			{
				buffer[sizeof(buffer) - 4] = '.';
				buffer[sizeof(buffer) - 3] = '.';
				buffer[sizeof(buffer) - 2] = '.';
				buffer[sizeof(buffer) - 1] = '\0';
			}

			WriteTimestampedLine(level, buffer);
		}
	}

	/// Sets the minimum log level. Messages below this level are dropped before formatting.
	inline void SetLevel(Level level) noexcept { Internal::current_level = level; }
	inline Level GetLevel() noexcept { return Internal::current_level; }

	/// Sets the active log sinks (bitmask). Defaults to Console.
	inline void SetSinks(Sinks sinks) noexcept { Internal::current_sinks = sinks; }
	inline Sinks GetSinks() noexcept { return Internal::current_sinks; }

	/// Overrides the file sink path. Default path is `<cwd>/mod_data/<mod_name>/logs/MMAPI-<mod_name>.log`.
	inline void SetFilePath(std::filesystem::path path)
	{
		if (Internal::file_stream.is_open())
			Internal::file_stream.close();
		Internal::file_path             = std::move(path);
		Internal::file_path_initialized = true;
	}

	/// Returns the active file sink path. Resolves the default lazily on first access.
	inline const std::filesystem::path& GetFilePath()
	{
		Internal::EnsureFilePathInitialized();
		return Internal::file_path;
	}

	/// Logs a Trace-level message. printf-style format string.
	template <typename... Args>
	inline void Trace(const char* format, Args... args) { Internal::FormatAndDispatch(Level::Trace, format, args...); }

	template <typename... Args>
	inline void Debug(const char* format, Args... args) { Internal::FormatAndDispatch(Level::Debug, format, args...); }

	template <typename... Args>
	inline void Info(const char* format, Args... args) { Internal::FormatAndDispatch(Level::Info, format, args...); }

	template <typename... Args>
	inline void Warn(const char* format, Args... args) { Internal::FormatAndDispatch(Level::Warn, format, args...); }

	template <typename... Args>
	inline void Error(const char* format, Args... args) { Internal::FormatAndDispatch(Level::Error, format, args...); }

	/// Logs a Debug-level message routed only to the File sink, never to Console.
	/// Gated by current_level filtering and File sink being in current_sinks; drops silently
	/// otherwise. Useful for diagnostic output that is valuable in a log file but would
	/// clutter the console — e.g. dependency-graph edges emitted by MMAPI_ENABLE_DEPENDENCY.
	template <typename... Args>
	inline void DebugFile(const char* format, Args... args) { Internal::FormatAndDispatchFileOnly(Level::Debug, format, args...); }

	namespace Internal
	{
		// UTF-8 byte sequences for the tree connectors. Kept as named constants so the bare
		// byte literals don't appear inline at each printf site.
		inline constexpr const char* kTreeConnectorMid   = "\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80";  // ├──
		inline constexpr const char* kTreeConnectorLast  = "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80";  // └──
		inline constexpr const char* kTreeIndentMid      = "\xE2\x94\x82\x20\x20\x20";              // │
		inline constexpr const char* kTreeIndentLast     = "    ";

		// DFS render of a single node and its subtree. `prefix` accumulates the column-leader
		// string for this depth (the vertical bars / spaces that connect this row's connector
		// back to the root). `visited` is shared across the entire dump so shared dependencies
		// are expanded exactly once; subsequent encounters print "(already enabled)" and stop
		// recursing — also breaks any defensive cycle that shouldn't normally exist.
		inline void RenderDependencySubtree(
			const std::string& node,
			const std::string& prefix,
			bool is_last,
			std::unordered_set<std::string>& visited)
		{
			const char* connector = is_last ? kTreeConnectorLast : kTreeConnectorMid;
			auto [_, first_visit] = visited.insert(node);

			char line[kMessageBufferSize];
			if (first_visit)
				std::snprintf(line, sizeof(line), "%s%s %s", prefix.c_str(), connector, node.c_str());
			else
				std::snprintf(line, sizeof(line), "%s%s %s (already enabled)", prefix.c_str(), connector, node.c_str());
			WriteTimestampedLine(Level::Trace, line);

			if (!first_visit) return;

			auto it = dependency_graph.find(node);
			if (it == dependency_graph.end()) return;  // leaf — no recorded deps

			const auto& deps = it->second;
			const std::string child_prefix = prefix + (is_last ? kTreeIndentLast : kTreeIndentMid);
			for (size_t i = 0; i < deps.size(); ++i)
			{
				const bool child_is_last = (i + 1 == deps.size());
				RenderDependencySubtree(deps[i], child_prefix, child_is_last, visited);
			}
		}
	}

	/// Pretty-prints the recorded MMAPI dependency graph to the file sink only — flat form.
	/// Each caller (Hooks::* registrar or module Enable) gets its own block listing only its
	/// direct dependencies. Shared dependencies appear once per caller that uses them. Compact
	/// for shallow audits; the structural cascade is NOT followed.
	///
	/// Use `DumpDependencyGraphTree()` instead if you want to see transitive cascades nested
	/// under each root (depth-first, deduplicated globally).
	///
	/// Intended to be called once after the mod's initialization has settled (typically the
	/// end of ModuleInitialize). No-op if the File sink is not enabled or current_level is
	/// above Trace. Console output is never emitted regardless of sink config.
	///
	/// Sample output:
	///   === MMAPI Dependency Graph (flat) ===
	///   Recorded 11 edges across 5 callers (invocation order)
	///
	///   MMAPI::Item
	///   ├── MMAPI::Instance
	///   └── MMAPI::Text
	///
	///   MMAPI::Text
	///   └── MMAPI::Instance
	inline void DumpDependencyGraphFlat()
	{
		if (static_cast<int>(Level::Trace) < static_cast<int>(Internal::current_level))
			return;
		if (!HasSink(Internal::current_sinks, Sinks::File))
			return;

		size_t edge_count = 0;
		for (const auto& [_, deps] : Internal::dependency_graph)
			edge_count += deps.size();

		Internal::WriteTimestampedLine(Level::Trace, "=== MMAPI Dependency Graph (flat) ===");
		char header[128];
		std::snprintf(header, sizeof(header),
			"Recorded %zu edges across %zu callers (invocation order)",
			edge_count, Internal::dependency_graph.size());
		Internal::WriteTimestampedLine(Level::Trace, header);
		Internal::WriteTimestampedLine(Level::Trace, "");

		// Iterate the parallel order vector instead of the map so callers appear in the
		// sequence they first recorded an edge — matches the chronological log entries
		// above and makes init-cascade reasoning natural to read top-to-bottom.
		for (const auto& caller : Internal::dependency_graph_caller_order)
		{
			const auto& deps = Internal::dependency_graph.at(caller);
			Internal::WriteTimestampedLine(Level::Trace, caller.c_str());
			for (size_t i = 0; i < deps.size(); ++i)
			{
				const bool is_last = (i + 1 == deps.size());
				char line[Internal::kMessageBufferSize];
				std::snprintf(line, sizeof(line),
					"%s %s",
					is_last ? Internal::kTreeConnectorLast : Internal::kTreeConnectorMid,
					deps[i].c_str());
				Internal::WriteTimestampedLine(Level::Trace, line);
			}
			Internal::WriteTimestampedLine(Level::Trace, "");
		}
	}

	/// Pretty-prints the recorded MMAPI dependency graph to the file sink only — nested tree
	/// form. Each top-level root (a caller that no other caller depends on) is expanded
	/// depth-first to show the full transitive cascade. Shared dependencies are expanded once
	/// globally; subsequent encounters are rendered as `<node> (already enabled)` so the dump
	/// doesn't repeat itself.
	///
	/// More useful than the flat form for understanding "what cascade actually happened" — you
	/// can read top-to-bottom and follow the cascade tree. Tradeoff: shared deps appear once
	/// fully expanded plus several `(already enabled)` markers in other roots' subtrees, which
	/// is more lines than the flat form for graphs with lots of shared deps.
	///
	/// Use `DumpDependencyGraphFlat()` instead for a per-caller direct-deps view that's more
	/// compact and easier to grep.
	///
	/// Intended to be called once after the mod's initialization has settled (typically the
	/// end of ModuleInitialize). No-op if the File sink is not enabled or current_level is
	/// above Trace. Console output is never emitted regardless of sink config.
	///
	/// Sample output:
	///   === MMAPI Dependency Graph (tree) ===
	///   Recorded 11 edges across 5 callers (depth-first from each root)
	///
	///   MMAPI::Item
	///   ├── MMAPI::Instance
	///   │   └── MMAPI::Weather
	///   └── MMAPI::Text
	///       └── MMAPI::Instance (already enabled)
	///
	///   MMAPI::Game
	///   ├── MMAPI::Instance (already enabled)
	///   └── MMAPI::Weather (already enabled)
	inline void DumpDependencyGraphTree()
	{
		if (static_cast<int>(Level::Trace) < static_cast<int>(Internal::current_level))
			return;
		if (!HasSink(Internal::current_sinks, Sinks::File))
			return;

		size_t edge_count = 0;
		for (const auto& [_, deps] : Internal::dependency_graph)
			edge_count += deps.size();

		Internal::WriteTimestampedLine(Level::Trace, "=== MMAPI Dependency Graph (tree) ===");
		char header[128];
		std::snprintf(header, sizeof(header),
			"Recorded %zu edges across %zu callers (depth-first from each root)",
			edge_count, Internal::dependency_graph.size());
		Internal::WriteTimestampedLine(Level::Trace, header);
		Internal::WriteTimestampedLine(Level::Trace, "");

		// Iterate the caller order vector and render each entry as a top-level subtree IFF it
		// hasn't already been visited via an earlier expansion. This:
		// - preserves invocation order at the top level (same as the flat dump)
		// - skips non-root callers whose subtree was already shown under their parent
		// - gives every direct-user-call a top-level entry unless it was reached transitively first
		std::unordered_set<std::string> visited;
		for (const auto& caller : Internal::dependency_graph_caller_order)
		{
			if (visited.contains(caller))
				continue;

			visited.insert(caller);
			Internal::WriteTimestampedLine(Level::Trace, caller.c_str());

			const auto& deps = Internal::dependency_graph.at(caller);
			for (size_t i = 0; i < deps.size(); ++i)
			{
				const bool is_last = (i + 1 == deps.size());
				Internal::RenderDependencySubtree(deps[i], "", is_last, visited);
			}
			Internal::WriteTimestampedLine(Level::Trace, "");
		}
	}
}

// Contract enforcement macros (MMAPI_REQUIRE_ENABLED / _VOID) and the dependency cascade macro
// (MMAPI_ENABLE_DEPENDENCY) live in Core.hpp. They are infrastructure for the module Enable()
// contract — not logging primitives. Log.hpp is included transitively by all callers, so the
// macros' references to MMAPI::Log::Warn and MMAPI::Log::DebugFile resolve at expansion.
