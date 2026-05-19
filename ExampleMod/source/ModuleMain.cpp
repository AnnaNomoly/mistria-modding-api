// ExampleMod: a reference MMAPI consumer mod for Fields of Mistria.
//
// This is a working template for new mods. It demonstrates the canonical
// YYTK + MMAPI shape: Aurie module entry point -> YYTKInterface resolution
// -> MMAPI::Initialize -> enable the modules you need -> register hooks.
//
// Behavior:
//   - At save load, `Game::Hooks::AfterGameActive` flips a flag and logs a
//     short "session start" notice. Game-state queries from here are kept
//     minimal because not every singleton has latched yet at that boundary
//     (Calendar's unified_time hasn't ticked, for instance).
//   - On every subsequent room transition,
//     `Location::Hooks::AfterGoToRoom` prints a small status table covering
//     Location, Calendar (season/day/weekday/year/time), Weather, Player
//     (HP, stamina, mana, position). At that point all relevant contexts are
//     latched and every read succeeds.
//   - The save-load goto_gm_room (when the game places the player into their
//     saved room) is skipped via the flag, but in current FoM versions the
//     save-load path doesn't even fire AfterGoToRoom, so the flag check is
//     defensive rather than load-bearing.
//
// Build configs:
//   Debug disables optimization for easier stepping; Release applies MaxSpeed.
//   Unlike the official YYTK Example, either is safe to deploy.

#include <YYToolkit/YYTK_Shared.hpp>
#include <MMAPI/MMAPI.hpp>

using namespace Aurie;
using namespace YYTK;

static const char* const MOD_NAME = "ExampleMod";
static const char* const VERSION  = "1.0.0";

// True once `AfterGameActive` has fired (i.e. the game has finished its
// save-load transition and game-state queries are safe to make).
static bool g_game_active = false;

static const char* SeasonName(MMAPI::Calendar::Seasons season)
{
	switch (season)
	{
		case MMAPI::Calendar::Seasons::Spring: return "Spring";
		case MMAPI::Calendar::Seasons::Summer: return "Summer";
		case MMAPI::Calendar::Seasons::Fall:   return "Fall";
		case MMAPI::Calendar::Seasons::Winter: return "Winter";
	}
	return "?";
}

static const char* WeekdayName(MMAPI::Calendar::Weekdays weekday)
{
	switch (weekday)
	{
		case MMAPI::Calendar::Weekdays::Monday:    return "Monday";
		case MMAPI::Calendar::Weekdays::Tuesday:   return "Tuesday";
		case MMAPI::Calendar::Weekdays::Wednesday: return "Wednesday";
		case MMAPI::Calendar::Weekdays::Thursday:  return "Thursday";
		case MMAPI::Calendar::Weekdays::Friday:    return "Friday";
		case MMAPI::Calendar::Weekdays::Saturday:  return "Saturday";
		case MMAPI::Calendar::Weekdays::Sunday:    return "Sunday";
	}
	return "?";
}

static const char* WeatherName(MMAPI::Weather::Ids weather)
{
	switch (weather)
	{
		case MMAPI::Weather::Ids::Calm:           return "Calm";
		case MMAPI::Weather::Ids::Inclement:      return "Inclement";
		case MMAPI::Weather::Ids::HeavyInclement: return "Heavy Inclement";
		case MMAPI::Weather::Ids::Special:        return "Special";
	}
	return "?";
}

static void PrintStatus(std::string_view room_name)
{
	MMAPI::Log::Info("=== Room change -> %s ===", std::string(room_name).c_str());

	MMAPI::Location::Ids location;
	if (MMAPI::Location::TryGetCurrentLocation(location))
	{
		MMAPI::Log::Info("  Location : %s (%s)",
			MMAPI::Location::LocationIdToString(location).c_str(),
			MMAPI::Location::IsCurrentLocationIndoors() ? "indoor" : "outdoor");
	}

	MMAPI::Calendar::Seasons  season;
	MMAPI::Calendar::Weekdays weekday;
	YYTK::RValue              day  = MMAPI::Calendar::GetDay();
	YYTK::RValue              year = MMAPI::Calendar::GetYear();
	if (MMAPI::Calendar::TryGetSeason(season)
	    && MMAPI::Calendar::TryGetWeekday(weekday)
	    && day.m_Kind != YYTK::VALUE_UNDEFINED
	    && year.m_Kind != YYTK::VALUE_UNDEFINED)
	{
		int seconds = MMAPI::Calendar::GetCurrentTimeInSeconds();
		int hours   = seconds / 3600;
		int minutes = (seconds % 3600) / 60;
		MMAPI::Log::Info("  Calendar : %s %lld (%s), Year %lld, %02d:%02d",
			SeasonName(season),
			day.ToInt64(),
			WeekdayName(weekday),
			year.ToInt64(),
			hours,
			minutes);
	}

	MMAPI::Weather::Ids weather;
	if (MMAPI::Weather::TryGetWeather(weather))
		MMAPI::Log::Info("  Weather  : %s", WeatherName(weather));

	YYTK::RValue hp      = MMAPI::Player::GetHealth();
	YYTK::RValue max_hp  = MMAPI::Player::GetMaxHealth();
	YYTK::RValue stamina = MMAPI::Player::GetStamina();
	YYTK::RValue mana    = MMAPI::Player::GetMana();
	if (hp.m_Kind != YYTK::VALUE_UNDEFINED && max_hp.m_Kind != YYTK::VALUE_UNDEFINED)
	{
		MMAPI::Log::Info("  Player   : HP %lld/%lld  Stamina %lld  Mana %lld",
			hp.ToInt64(),
			max_hp.ToInt64(),
			stamina.ToInt64(),
			mana.ToInt64());
	}

	// Position reflects Ari's location in the room they're LEAVING, not the new
	// room they're entering. AfterGoToRoom fires when the goto_gm_room script
	// returns, which is before the game's Taxi system actually teleports Ari to
	// the destination room. If you need post-teleport coordinates, defer the
	// read by a frame or use a later signal (e.g. on_room_start).
	auto position = MMAPI::Player::GetPosition();
	if (position)
		MMAPI::Log::Info("  Position : (%.1f, %.1f)", position->x, position->y);
}

// Fires once per session at the load-transition boundary. We do not read
// game state here. Calendar's unified_time hasn't ticked yet, so its
// getters return undefined, and some MMAPI helpers have similar latching
// preconditions. Just flip the flag and let the real work happen later.
static void OnGameActive()
{
	g_game_active = true;
	MMAPI::Log::Info("Game is active. Next room change will dump the status table.");
}

// Fires on every `goto_gm_room` call. The flag check skips any fires that
// happen before the game becomes interactive (defensive: in current FoM
// versions save-load doesn't route through goto_gm_room anyway). All later
// fires happen during fully-active gameplay where every read succeeds.
static void OnAfterGoToRoom(MMAPI::Location::AfterGoToRoomContext& ctx)
{
	if (!g_game_active)
		return;
	PrintStatus(ctx.GetRoomName());
}

EXPORTED AurieStatus ModuleInitialize(IN AurieModule* Module, IN const fs::path& ModulePath)
{
	UNREFERENCED_PARAMETER(Module);
	UNREFERENCED_PARAMETER(ModulePath);

	YYTKInterface* module_interface = nullptr;
	AurieStatus status = ObGetInterface("YYTK_Main", (AurieInterfaceBase*&)module_interface);
	if (!AurieSuccess(status))
		return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;

	module_interface->Print(CM_LIGHTAQUA, "[%s %s] - Plugin starting...", MOD_NAME, VERSION);

	// Route logs to both console and a per-mod file (mod_data/<MOD_NAME>/logs/MMAPI-<MOD_NAME>.log).
	// Must be set BEFORE MMAPI::Initialize to also capture the startup banner in the file.
	// TRACE is the noisiest level and is FILE-ONLY by policy; consoles never see TRACE entries.
	MMAPI::Log::SetSinks(MMAPI::Log::Sinks::Console | MMAPI::Log::Sinks::File);
	MMAPI::Log::SetLevel(MMAPI::Log::Level::Trace);

	CInstance* global_instance = nullptr;
	module_interface->GetGlobalInstance(&global_instance);
	MMAPI::Initialize(module_interface, global_instance, g_ArSelfModule, MOD_NAME, VERSION);

	MMAPI::Game::Enable();
	MMAPI::Player::Enable();
	MMAPI::Calendar::Enable();
	MMAPI::Weather::Enable();
	MMAPI::Location::Enable();

	MMAPI::Game::Hooks::AfterGameActive(OnGameActive);
	MMAPI::Location::Hooks::AfterGoToRoom(OnAfterGoToRoom);

	// Write the resolved dependency tree to the log file. Lands at TRACE level
	// (file-only), so it's preserved for inspection without spamming the console.
	MMAPI::Log::DumpDependencyGraphTree();

	MMAPI::Log::Info("Plugin started! Status will print on every room change after the game becomes active.");
	return AURIE_SUCCESS;
}
