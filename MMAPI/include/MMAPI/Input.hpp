// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 AnnaNomoly
// Mistria Modding API (MMAPI)
// https://github.com/AnnaNomoly/mistria-modding-api

#pragma once

#include "Core.hpp"
#include "Hook.hpp"
#include "Log.hpp"
#include "Status.hpp"

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include <Windows.h>  // VK_* virtual-key constants used by the keybind name → code maps.

#include "YYToolkit/YYTK_Shared.hpp"

namespace MMAPI::Input
{
	/// Source: globalInstance.__input_id__
	/// Game-specific input action indices passed to `take_press@Input@Input` (press events) and
	/// `check_value@Input@Input` (axis-valued movement actions only — MoveUp/Down/Left/Right return
	/// meaningful values from check_value; other indices return 0). Re-dump after a game patch.
	enum class Actions : int
	{
		LeftMouse           = 0,
		MoveUp              = 1,
		MoveDown            = 2,
		MoveLeft            = 3,
		MoveRight           = 4,
		Jump                = 5,
		Interact            = 6,
		SecondaryInteract   = 7,
		PickUpOne           = 8,
		OpenJournal         = 9,
		MenuBack            = 10,
		UseToolCharged      = 11,
		UseToolRepeated     = 12,
		CastPinnedSpell     = 13,
		Throw               = 14,
		Walk                = 15,
		Ride                = 16,
		OpenMapMenu         = 17,
		MenuTabRight        = 18,
		MenuTabLeft         = 19,
		NextPreset          = 20,
		LastPreset          = 21,
		ToolbarIncUp        = 22,
		ToolbarIncDown      = 23,
		RotateRight         = 24,
		RotateLeft          = 25,
		FurnitureUp         = 26,
		FurnitureDown       = 27,
		FurnitureLeft       = 28,
		FurnitureRight      = 29,
		NextToolbarTab      = 30,
		LastToolbarTab      = 31,
		SelectToolbarOne    = 32,
		SelectToolbarTwo    = 33,
		SelectToolbarThree  = 34,
		SelectToolbarFour   = 35,
		SelectToolbarFive   = 36,
		SelectToolbarSix    = 37,
		SelectToolbarSeven  = 38,
		SelectToolbarEight  = 39,
		SelectToolbarNine   = 40,
		SelectToolbarZero   = 41,
		ConfirmTextInput    = 42,
		ResetControls       = 43,
	};

	struct TakePressContext
	{
		int  m_action_id = 0;
		bool m_result    = false;

		/// Returns the action the game's take_press is evaluating.
		MMAPI::Input::Actions GetAction() const { return static_cast<MMAPI::Input::Actions>(m_action_id); }

		/// Returns whether the input was reported as pressed this frame, as computed by the game.
		bool GetResult() const { return m_result; }

		/// Overrides whether the game considers the input pressed. Set false to swallow the press.
		void SetResult(bool pressed) { m_result = pressed; }
	};

	struct CheckValueContext
	{
		int m_action_id = 0;

		/// Returns the action the game's check_value is about to evaluate.
		MMAPI::Input::Actions GetAction() const { return static_cast<MMAPI::Input::Actions>(m_action_id); }

		/// Overrides the action passed to the game's check_value script. Useful for input remapping
		/// (e.g. swapping direction actions to implement a confusion effect).
		void SetAction(MMAPI::Input::Actions action) { m_action_id = static_cast<int>(action); }
	};

	/// A parsed, runtime-ready binding for a keyboard key or gamepad button — produced by
	/// `TryParseKeybind` and consumed by `IsKeybindPressed`. Mods that take a user-configurable hotkey
	/// (e.g. a JSON config field "activation_button": "F10") should:
	///   1. Parse once at startup: `auto kb = MMAPI::Input::TryParseKeybind(name);`
	///   2. Check per-frame: `if (kb && MMAPI::Input::IsKeybindPressed(*kb)) { ... }`
	/// The struct is opaque-ish: the `code` and `is_gamepad` fields are exposed for inspection but
	/// most callers only need IsValid() + IsKeybindPressed().
	struct Keybind
	{
		int  code       = -1;     ///< Either a Win32 VK_* code (keyboard) or a 0x80xx gamepad-button code.
		bool is_gamepad = false;  ///< When true, `code` is a gamepad button; otherwise a keyboard VK_ code.

		/// Returns true if this keybind was resolved to a known key/button.
		bool IsValid() const { return code >= 0; }
	};

	namespace Internal
	{
		inline bool enabled = false;

		/// Canonical mapping of keyboard-keybind names (as used in user config files) to Win32 VK_*
		/// codes. Names are uppercase and use underscores for compound keys ("PAGE_UP", "NUMPAD_0").
		/// Lazily-initialized singleton.
		inline const std::map<std::string, int>& KeyboardNameToVk()
		{
			static const std::map<std::string, int> m = {
				{ "F1", VK_F1 }, { "F2", VK_F2 }, { "F3", VK_F3 }, { "F4", VK_F4 },
				{ "F5", VK_F5 }, { "F6", VK_F6 }, { "F7", VK_F7 }, { "F8", VK_F8 },
				{ "F9", VK_F9 }, { "F10", VK_F10 }, { "F11", VK_F11 }, { "F12", VK_F12 },
				{ "NUMPAD_0", VK_NUMPAD0 }, { "NUMPAD_1", VK_NUMPAD1 }, { "NUMPAD_2", VK_NUMPAD2 },
				{ "NUMPAD_3", VK_NUMPAD3 }, { "NUMPAD_4", VK_NUMPAD4 }, { "NUMPAD_5", VK_NUMPAD5 },
				{ "NUMPAD_6", VK_NUMPAD6 }, { "NUMPAD_7", VK_NUMPAD7 }, { "NUMPAD_8", VK_NUMPAD8 },
				{ "NUMPAD_9", VK_NUMPAD9 },
				{ "0", '0' }, { "1", '1' }, { "2", '2' }, { "3", '3' }, { "4", '4' },
				{ "5", '5' }, { "6", '6' }, { "7", '7' }, { "8", '8' }, { "9", '9' },
				{ "A", 'A' }, { "B", 'B' }, { "C", 'C' }, { "D", 'D' }, { "E", 'E' },
				{ "F", 'F' }, { "G", 'G' }, { "H", 'H' }, { "I", 'I' }, { "J", 'J' },
				{ "K", 'K' }, { "L", 'L' }, { "M", 'M' }, { "N", 'N' }, { "O", 'O' },
				{ "P", 'P' }, { "Q", 'Q' }, { "R", 'R' }, { "S", 'S' }, { "T", 'T' },
				{ "U", 'U' }, { "V", 'V' }, { "W", 'W' }, { "X", 'X' }, { "Y", 'Y' },
				{ "Z", 'Z' },
				{ "INSERT", VK_INSERT }, { "DELETE", VK_DELETE }, { "HOME", VK_HOME },
				{ "PAGE_UP", VK_PRIOR }, { "PAGE_DOWN", VK_NEXT }, { "NUM_LOCK", VK_NUMLOCK },
				{ "SCROLL_LOCK", VK_SCROLL }, { "CAPS_LOCK", VK_CAPITAL }, { "PAUSE_BREAK", VK_PAUSE },
			};
			return m;
		}

		/// Canonical mapping of gamepad-button names to GameMaker gamepad button constants (0x80xx).
		/// Names are uppercase and use the "GAMEPAD_*" prefix to disambiguate from keyboard names.
		/// Lazily-initialized singleton.
		inline const std::map<std::string, int>& GamepadNameToButton()
		{
			static const std::map<std::string, int> m = {
				{ "GAMEPAD_A", 0x8001 }, { "GAMEPAD_B", 0x8002 },
				{ "GAMEPAD_X", 0x8003 }, { "GAMEPAD_Y", 0x8004 },
				{ "GAMEPAD_LEFT_SHOULDER",  0x8005 }, { "GAMEPAD_RIGHT_SHOULDER", 0x8006 },
				{ "GAMEPAD_LEFT_TRIGGER",   0x8007 }, { "GAMEPAD_RIGHT_TRIGGER",  0x8008 },
				{ "GAMEPAD_SELECT", 0x8009 }, { "GAMEPAD_START", 0x800A },
				{ "GAMEPAD_LEFT_STICK", 0x800B }, { "GAMEPAD_RIGHT_STICK", 0x800C },
				{ "GAMEPAD_DPAD_UP",    0x800D }, { "GAMEPAD_DPAD_DOWN",  0x800E },
				{ "GAMEPAD_DPAD_LEFT",  0x800F }, { "GAMEPAD_DPAD_RIGHT", 0x8010 },
			};
			return m;
		}

		inline constexpr const char* GML_SCRIPT_TAKE_PRESS  = "gml_Script_take_press@Input@Input";
		inline constexpr const char* GML_SCRIPT_CHECK_VALUE = "gml_Script_check_value@Input@Input";

		using AfterTakePressCallback   = void(*)(MMAPI::Input::TakePressContext&);
		using BeforeCheckValueCallback = void(*)(MMAPI::Input::CheckValueContext&);

		inline AfterTakePressCallback   after_take_press_callback    = nullptr;
		inline BeforeCheckValueCallback before_check_value_callback  = nullptr;

		inline YYTK::RValue& GmlScriptAfterTakePressCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_TAKE_PRESS)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			if (after_take_press_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::Input::TakePressContext context{
					static_cast<int>(Arguments[0]->ToInt64()),
					Result.ToBoolean()
				};
				after_take_press_callback(context);
				Result = context.m_result;
			}

			return Result;
		}

		inline YYTK::RValue& GmlScriptBeforeCheckValueCallback(
			IN YYTK::CInstance* Self,
			IN YYTK::CInstance* Other,
			OUT YYTK::RValue& Result,
			IN int ArgumentCount,
			IN YYTK::RValue** Arguments
		)
		{
			if (before_check_value_callback && Arguments && ArgumentCount >= 1 && Arguments[0])
			{
				MMAPI::Input::CheckValueContext context{
					static_cast<int>(Arguments[0]->ToInt64())
				};
				before_check_value_callback(context);
				*Arguments[0] = context.m_action_id;
			}

			const auto original = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(
				Aurie::MmGetHookTrampoline(MMAPI::Internal::self_module, GML_SCRIPT_CHECK_VALUE)
			);
			original(Self, Other, Result, ArgumentCount, Arguments);

			return Result;
		}

		// Raw GameMaker-builtin wrappers used by the public IsKeybindPressed / IsKeybindDown dispatch.
		// Kept Internal because the public Keybind-based flow is the supported API surface; mods that
		// need composition (modifier + main key) build it from public IsKeybind* checks parsed via
		// TryParseKeybind. If a future case truly needs raw-code access, promote selectively.

		/// Returns true if the keyboard key was pressed this frame.
		inline bool KeyboardCheckPressed(int key)
		{
			YYTK::RValue pressed = MMAPI::Internal::module_interface->CallBuiltin("keyboard_check_pressed", { key });
			return pressed.ToBoolean();
		}

		/// Returns true if the keyboard key is currently held down.
		inline bool KeyboardCheck(int key)
		{
			YYTK::RValue down = MMAPI::Internal::module_interface->CallBuiltin("keyboard_check", { key });
			return down.ToBoolean();
		}

		/// Returns true if a gamepad is connected in the given slot.
		inline bool GamepadIsConnected(int gamepad_slot)
		{
			YYTK::RValue connected = MMAPI::Internal::module_interface->CallBuiltin("gamepad_is_connected", { gamepad_slot });
			return connected.ToBoolean();
		}

		/// Returns the first connected gamepad slot, or -1 if no gamepad is connected.
		inline int GetFirstConnectedGamepadSlot()
		{
			for (int slot = 0; slot < 12; slot++)
			{
				if (GamepadIsConnected(slot))
					return slot;
			}

			return -1;
		}

		/// Returns true if the gamepad button was pressed this frame.
		inline bool GamepadButtonCheckPressed(int gamepad_slot, int button)
		{
			YYTK::RValue pressed = MMAPI::Internal::module_interface->CallBuiltin("gamepad_button_check_pressed", { gamepad_slot, button });
			return pressed.ToBoolean();
		}

		/// Returns true if the gamepad button is currently held down.
		inline bool GamepadButtonCheck(int gamepad_slot, int button)
		{
			YYTK::RValue down = MMAPI::Internal::module_interface->CallBuiltin("gamepad_button_check", { gamepad_slot, button });
			return down.ToBoolean();
		}
	}

	/// Activates Input hooks. Installs the take_press and check_value hooks so registered callbacks
	/// can observe input-press results and remap inputs before the game evaluates them. Safe to call
	/// before any Hooks::* registration — each callback no-ops until a user callback is bound.
	/// The pull-style helpers (IsKeybindPressed, IsKeybindDown) do not require Enable().
	/// @return Status::Success if the hooks are installed (or already were); otherwise a failure status.
	inline MMAPI::Status Enable()
	{
		if (Internal::enabled)
			return MMAPI::Status::Success;

		MMAPI::Log::Debug("MMAPI::Input::Enable() called");

		MMAPI::Status status = MMAPI::Internal::InstallScriptHooks({
			{ Internal::GML_SCRIPT_TAKE_PRESS,  reinterpret_cast<PVOID>(Internal::GmlScriptAfterTakePressCallback) },
			{ Internal::GML_SCRIPT_CHECK_VALUE, reinterpret_cast<PVOID>(Internal::GmlScriptBeforeCheckValueCallback) },
		});
		if (!MMAPI::IsSuccess(status))
			return status;

		Internal::enabled = true;
		return MMAPI::Status::Success;
	}

	/// Parses a keybind name string (typically from a mod's config file) into a `Keybind` runtime
	/// representation. Accepts both keyboard names ("F1"-"F12", "A"-"Z", "0"-"9", "NUMPAD_0"-"NUMPAD_9",
	/// "INSERT", "DELETE", "HOME", "PAGE_UP", "PAGE_DOWN", "NUM_LOCK", "SCROLL_LOCK", "CAPS_LOCK",
	/// "PAUSE_BREAK") and gamepad names ("GAMEPAD_A", "GAMEPAD_B", "GAMEPAD_X", "GAMEPAD_Y",
	/// "GAMEPAD_LEFT_SHOULDER", "GAMEPAD_RIGHT_SHOULDER", "GAMEPAD_LEFT_TRIGGER",
	/// "GAMEPAD_RIGHT_TRIGGER", "GAMEPAD_SELECT", "GAMEPAD_START", "GAMEPAD_LEFT_STICK",
	/// "GAMEPAD_RIGHT_STICK", "GAMEPAD_DPAD_UP", "GAMEPAD_DPAD_DOWN", "GAMEPAD_DPAD_LEFT",
	/// "GAMEPAD_DPAD_RIGHT"). Names are case-sensitive (uppercase).
	/// @param name The keybind name string to parse.
	/// @return A `Keybind` on success, or `std::nullopt` if `name` doesn't match any known key/button.
	inline std::optional<Keybind> TryParseKeybind(std::string_view name)
	{
		std::string key(name);

		if (auto it = Internal::GamepadNameToButton().find(key); it != Internal::GamepadNameToButton().end())
			return Keybind{ it->second, true };

		if (auto it = Internal::KeyboardNameToVk().find(key); it != Internal::KeyboardNameToVk().end())
			return Keybind{ it->second, false };

		return std::nullopt;
	}

	/// Returns true if the keybind was pressed this frame. Dispatches to `KeyboardCheckPressed` or
	/// `GamepadButtonCheckPressed` based on `keybind.is_gamepad`. For gamepad keybinds, automatically
	/// resolves the first connected gamepad slot — returns false if no gamepad is connected.
	/// @param keybind The keybind to check, typically produced by `TryParseKeybind`.
	inline bool IsKeybindPressed(const Keybind& keybind)
	{
		if (!keybind.IsValid())
			return false;

		if (keybind.is_gamepad)
		{
			int slot = Internal::GetFirstConnectedGamepadSlot();
			return slot >= 0 && Internal::GamepadButtonCheckPressed(slot, keybind.code);
		}

		return Internal::KeyboardCheckPressed(keybind.code);
	}

	/// Returns true if the keybind is currently held down. Held-state variant of `IsKeybindPressed`.
	/// Dispatches to `KeyboardCheck` or `GamepadButtonCheck` based on `keybind.is_gamepad`. For
	/// gamepad keybinds, automatically resolves the first connected gamepad slot — returns false if
	/// no gamepad is connected.
	/// @param keybind The keybind to check, typically produced by `TryParseKeybind`.
	inline bool IsKeybindDown(const Keybind& keybind)
	{
		if (!keybind.IsValid())
			return false;

		if (keybind.is_gamepad)
		{
			int slot = Internal::GetFirstConnectedGamepadSlot();
			return slot >= 0 && Internal::GamepadButtonCheck(slot, keybind.code);
		}

		return Internal::KeyboardCheck(keybind.code);
	}

	namespace Hooks
	{
		/// Registers a callback that runs after the game's `take_press` script.
		/// Read `ctx.GetAction()` to identify the action being checked, `ctx.GetResult()` to see whether
		/// the game considered it pressed, and `ctx.SetResult(false)` to swallow the press.
		/// @param callback A function called with a mutable `MMAPI::Input::TakePressContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status AfterTakePress(Internal::AfterTakePressCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Input::Hooks::AfterTakePress, MMAPI::Input);

			return MMAPI::Internal::RegisterHook(
				"Input::AfterTakePress",
				Internal::after_take_press_callback,
				callback
			);
		}

		/// Registers a callback that runs before the game's `check_value` script.
		/// Read `ctx.GetAction()` to see which action the game is about to evaluate, and
		/// `ctx.SetAction(Actions::X)` to remap it (e.g. swap direction actions to implement a confusion effect).
		/// @param callback A function called with a mutable `MMAPI::Input::CheckValueContext`.
		/// @return Status::Success if the hook was installed; Status::AlreadyRegistered if a callback is already registered; otherwise a failure status.
		inline MMAPI::Status BeforeCheckValue(Internal::BeforeCheckValueCallback callback)
		{
			MMAPI_ENABLE_DEPENDENCY(MMAPI::Input::Hooks::BeforeCheckValue, MMAPI::Input);

			return MMAPI::Internal::RegisterHook(
				"Input::BeforeCheckValue",
				Internal::before_check_value_callback,
				callback
			);
		}
	}
}
