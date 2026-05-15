// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#if !__has_include(<nlohmann/json.hpp>)
#error "MMAPI::Config requires nlohmann/json.hpp. Either vendor it at include/nlohmann/json.hpp in your mod project (the MMAPI sync flow ships a copy alongside the MMAPI headers), or remove #include <MMAPI/Config.hpp> — it's intentionally not pulled in by <MMAPI/MMAPI.hpp> by default."
#endif

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>

#include "nlohmann/json.hpp"

namespace MMAPI::Config
{
	/// Returns the standard config file path for a mod, creating the directory if needed.
	/// Files are stored at: <game_dir>/mod_data/<mod_name>/<mod_name>.json
	/// @param mod_name The mod's name, used for both the subdirectory and the filename.
	/// @return The full path to the mod's config file.
	inline std::filesystem::path GetConfigPath(const std::string& mod_name)
	{
		std::filesystem::path config_dir = std::filesystem::current_path() / "mod_data" / mod_name;
		std::filesystem::create_directories(config_dir);
		return config_dir / (mod_name + ".json");
	}

	/// Loads a JSON config file from disk.
	/// @param path The path to the config file.
	/// @return The parsed JSON object, or an empty object if the file does not exist or cannot be parsed.
	inline nlohmann::json Load(const std::filesystem::path& path)
	{
		if (!std::filesystem::exists(path))
			return {};

		try
		{
			std::ifstream stream(path);
			return nlohmann::json::parse(stream);
		}
		catch (...) {}

		return {};
	}

	/// Saves a JSON object to a config file with 4-space indentation.
	/// @param path The path to write the config file to.
	/// @param data The JSON object to save.
	inline void Save(const std::filesystem::path& path, const nlohmann::json& data)
	{
		std::ofstream stream(path);
		stream << std::setw(4) << data << std::endl;
	}

	/// Gets a typed value from a JSON object by key.
	/// Returns the default if the key is absent or the stored value cannot be converted to T.
	/// @param j The JSON object to read from.
	/// @param key The key to look up.
	/// @param default_value The value to return on any failure.
	/// @return The value at the given key cast to T, or default_value.
	template <typename T>
	inline T GetValue(const nlohmann::json& j, const std::string& key, const T& default_value)
	{
		try
		{
			return j.value(key, default_value);
		}
		catch (...) {}

		return default_value;
	}

	/// Gets a typed value from a JSON object by key, returning the default if the value is outside [min, max].
	/// @param j The JSON object to read from.
	/// @param key The key to look up.
	/// @param default_value The value to return if the key is absent, the type is wrong, or the value is out of range.
	/// @param min The minimum acceptable value (inclusive).
	/// @param max The maximum acceptable value (inclusive).
	/// @return The value at the given key if it exists and is within range, or default_value.
	template <typename T>
	inline T GetValue(const nlohmann::json& j, const std::string& key, const T& default_value, const T& min, const T& max)
	{
		T value = GetValue(j, key, default_value);
		if (value < min || value > max)
			return default_value;
		return value;
	}
}
