// ExampleMod — a minimal MMAPI consumer mod for Fields of Mistria.
//
// This is a reference template. It demonstrates the canonical YYTK + MMAPI mod
// shape: Aurie module entry point → YYTKInterface resolution → MMAPI::Initialize
// → enable the modules you need → register hooks. It logs a small calendar +
// weather status table at two cadences:
//   - Once at session start, via `Game::Hooks::AfterGameActive`.
//   - Once per in-game day rollover, via `Game::Hooks::BeforeNewDay`.

#include <YYToolkit/YYTK_Shared.hpp>
#include <MMAPI/MMAPI.hpp>

using namespace Aurie;
using namespace YYTK;

static const char* const MOD_NAME = "ExampleMod";
static const char* const VERSION  = "1.0.0";

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

// Prints a small table of the current calendar / weather / player state. Each
// row is independently guarded so partial-context situations degrade gracefully
// (a column that can't be read is simply skipped).
static void PrintStatus()
{
	MMAPI::Calendar::Seasons  season;
	MMAPI::Calendar::Weekdays weekday;
	YYTK::RValue              day  = MMAPI::Calendar::GetDay();
	YYTK::RValue              year = MMAPI::Calendar::GetYear();
	if (MMAPI::Calendar::TryGetSeason(season)
	    && MMAPI::Calendar::TryGetWeekday(weekday)
	    && day.m_Kind != YYTK::VALUE_UNDEFINED
	    && year.m_Kind != YYTK::VALUE_UNDEFINED)
	{
		int seconds = MMAPI::Game::GetCurrentTimeInSeconds();
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
}

// Fires once per session, after the game becomes interactive (post-title,
// post-save-load). The example uses this as the "welcome" log.
static void OnGameActive()
{
	MMAPI::Log::Info("=== Session start ===");
	PrintStatus();
}

// Fires once per in-game day, right before the rollover. The example uses this
// to log the outgoing day's snapshot.
static void OnBeforeNewDay()
{
	MMAPI::Log::Info("=== New day rollover ===");
	PrintStatus();
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

	CInstance* global_instance = nullptr;
	module_interface->GetGlobalInstance(&global_instance);
	MMAPI::Initialize(module_interface, global_instance, g_ArSelfModule, MOD_NAME, VERSION);

	MMAPI::Game::Enable();
	MMAPI::Calendar::Enable();
	MMAPI::Weather::Enable();

	MMAPI::Game::Hooks::AfterGameActive(OnGameActive);
	MMAPI::Game::Hooks::BeforeNewDay(OnBeforeNewDay);

	MMAPI::Log::Info("Plugin started! Status will print at session start and on each new-day rollover.");
	return AURIE_SUCCESS;
}
