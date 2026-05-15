// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#if !__has_include(<nlohmann/json.hpp>)
#error "MMAPI::ModSave requires nlohmann/json.hpp. Either vendor it at include/nlohmann/json.hpp in your mod project (the MMAPI sync flow ships a copy alongside the MMAPI headers), or remove #include <MMAPI/ModSave.hpp> — it's intentionally not pulled in by <MMAPI/MMAPI.hpp> by default."
#endif

#include "Core.hpp"
#include "Log.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>

#include "nlohmann/json.hpp"

namespace MMAPI::ModSave
{
	/// Returns the path to the mod's per-save data file for the given save-slot prefix.
	/// Files are stored at `<cwd>/mod_data/<mod_name>/<save_prefix>.json`. The parent directory
	/// is created if it does not exist.
	///
	/// This is for the mod's per-save state (one file per save slot). For the mod's global
	/// config (one file per mod, shared across all saves), use `MMAPI::Config::GetConfigPath`.
	/// For the game's actual save file, hook `Game::Hooks::BeforeSaveGame` / `AfterLoadGame`.
	///
	/// @attention Requires `MMAPI::Initialize` to have been called so `mod_name` is populated.
	/// @param save_prefix The stable per-save identifier (e.g. from `SaveGameContext::GetSavePrefix()`).
	/// @return The path to the per-save data file, or empty if `save_prefix` is empty.
	inline std::filesystem::path GetPath(const std::string& save_prefix)
	{
		if (save_prefix.empty() || MMAPI::Internal::mod_name.empty())
			return {};

		std::filesystem::path dir =
			std::filesystem::current_path() / "mod_data" / MMAPI::Internal::mod_name;
		std::filesystem::create_directories(dir);
		return dir / (save_prefix + ".json");
	}

	/// Reads the mod's per-save data for the given save-slot prefix.
	/// @param save_prefix The stable per-save identifier.
	/// @return The parsed JSON object, or an empty object if the file does not exist or cannot be parsed.
	inline nlohmann::json Read(const std::string& save_prefix)
	{
		std::filesystem::path path = GetPath(save_prefix);
		if (path.empty() || !std::filesystem::exists(path))
			return {};

		try
		{
			std::ifstream stream(path);
			return nlohmann::json::parse(stream);
		}
		catch (const std::exception& e)
		{
			MMAPI::Log::Error("Failed to read mod save data (%s): %s", path.string().c_str(), e.what());
			return {};
		}
	}

	/// Writes the mod's per-save data for the given save-slot prefix. No-op (with an error log)
	/// when `save_prefix` is empty — typically that means the game hasn't given us a real save
	/// path yet, in which case the data isn't safely persistable.
	/// @param save_prefix The stable per-save identifier.
	/// @param data The JSON object to write.
	inline void Write(const std::string& save_prefix, const nlohmann::json& data)
	{
		std::filesystem::path path = GetPath(save_prefix);
		if (path.empty())
		{
			MMAPI::Log::Error("Cannot write mod save data: missing save_prefix or mod_name");
			return;
		}

		try
		{
			std::ofstream stream(path);
			stream << std::setw(4) << data << std::endl;
		}
		catch (const std::exception& e)
		{
			MMAPI::Log::Error("Failed to write mod save data (%s): %s", path.string().c_str(), e.what());
		}
	}

	/// Deletes the mod's per-save data file for the given save-slot prefix. No-op if the file
	/// doesn't exist or `save_prefix` is empty. Useful for "consume on end-of-day" style state
	/// that shouldn't carry over to the next play session.
	/// @param save_prefix The stable per-save identifier.
	inline void Delete(const std::string& save_prefix)
	{
		std::filesystem::path path = GetPath(save_prefix);
		if (path.empty()) return;

		std::error_code ec;
		std::filesystem::remove(path, ec);
	}
}
