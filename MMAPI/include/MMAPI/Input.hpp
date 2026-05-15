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

	// Push/pop guard for Windows.h macro `DELETE` (access right 0x00010000L in WinNT.h) — without
	// this the enumerator name would expand to a numeric literal mid-declaration and break parsing.
	#pragma push_macro("DELETE")
	#undef DELETE

	/// Strongly-typed keyboard key codes for direct Keybind construction. Values are the underlying
	/// Win32 VK_* codes, so static_cast<int>(KeyboardKeys::F1) == VK_F1. Use this when the binding is
	/// known at compile time; for config-string-driven bindings (e.g. JSON), use TryParseKeybind.
	enum class KeyboardKeys : int
	{
		// Function keys
		F1 = VK_F1, F2 = VK_F2, F3 = VK_F3, F4 = VK_F4,
		F5 = VK_F5, F6 = VK_F6, F7 = VK_F7, F8 = VK_F8,
		F9 = VK_F9, F10 = VK_F10, F11 = VK_F11, F12 = VK_F12,

		// Numpad
		NUMPAD_0 = VK_NUMPAD0, NUMPAD_1 = VK_NUMPAD1, NUMPAD_2 = VK_NUMPAD2,
		NUMPAD_3 = VK_NUMPAD3, NUMPAD_4 = VK_NUMPAD4, NUMPAD_5 = VK_NUMPAD5,
		NUMPAD_6 = VK_NUMPAD6, NUMPAD_7 = VK_NUMPAD7, NUMPAD_8 = VK_NUMPAD8,
		NUMPAD_9 = VK_NUMPAD9,

		// Top-row digits
		DIGIT_0 = '0', DIGIT_1 = '1', DIGIT_2 = '2', DIGIT_3 = '3', DIGIT_4 = '4',
		DIGIT_5 = '5', DIGIT_6 = '6', DIGIT_7 = '7', DIGIT_8 = '8', DIGIT_9 = '9',

		// Letters
		A = 'A', B = 'B', C = 'C', D = 'D', E = 'E', F = 'F', G = 'G', H = 'H',
		I = 'I', J = 'J', K = 'K', L = 'L', M = 'M', N = 'N', O = 'O', P = 'P',
		Q = 'Q', R = 'R', S = 'S', T = 'T', U = 'U', V = 'V', W = 'W', X = 'X',
		Y = 'Y', Z = 'Z',

		// Modifiers (generic — fire for left or right variants)
		SHIFT = VK_SHIFT, CTRL = VK_CONTROL, ALT = VK_MENU,

		// Arrow keys
		ARROW_UP    = VK_UP,
		ARROW_DOWN  = VK_DOWN,
		ARROW_LEFT  = VK_LEFT,
		ARROW_RIGHT = VK_RIGHT,

		// Common navigation / editing
		SPACE     = VK_SPACE,
		ENTER     = VK_RETURN,
		ESCAPE    = VK_ESCAPE,
		TAB       = VK_TAB,
		BACKSPACE = VK_BACK,
		INSERT    = VK_INSERT,
		DELETE    = VK_DELETE,
		HOME      = VK_HOME,
		PAGE_UP   = VK_PRIOR,
		PAGE_DOWN = VK_NEXT,

		// Lock keys & misc
		NUM_LOCK    = VK_NUMLOCK,
		SCROLL_LOCK = VK_SCROLL,
		CAPS_LOCK   = VK_CAPITAL,
		PAUSE_BREAK = VK_PAUSE,
	};

	#pragma pop_macro("DELETE")

	/// Strongly-typed gamepad button codes for direct Keybind construction. Values are GameMaker's
	/// 0x80xx gamepad button constants. Use this when the binding is known at compile time; for
	/// config-string-driven bindings, use TryParseKeybind.
	enum class GamepadButtons : int
	{
		GAMEPAD_A = 0x8001, GAMEPAD_B = 0x8002,
		GAMEPAD_X = 0x8003, GAMEPAD_Y = 0x8004,
		GAMEPAD_LEFT_SHOULDER  = 0x8005, GAMEPAD_RIGHT_SHOULDER = 0x8006,
		GAMEPAD_LEFT_TRIGGER   = 0x8007, GAMEPAD_RIGHT_TRIGGER  = 0x8008,
		GAMEPAD_SELECT = 0x8009, GAMEPAD_START = 0x800A,
		GAMEPAD_LEFT_STICK  = 0x800B, GAMEPAD_RIGHT_STICK = 0x800C,
		GAMEPAD_DPAD_UP    = 0x800D, GAMEPAD_DPAD_DOWN  = 0x800E,
		GAMEPAD_DPAD_LEFT  = 0x800F, GAMEPAD_DPAD_RIGHT = 0x8010,
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

	/// A runtime-ready binding for a keyboard key or gamepad button.
	///
	/// Construction paths:
	///   - From a config-string: `auto kb = MMAPI::Input::TryParseKeybind("F10");` returns std::optional<Keybind>.
	///   - Direct from enum:     `Keybind kb(KeyboardKeys::F10);` or `Keybind kb(GamepadButtons::GAMEPAD_A);`
	///
	/// Per-frame check: `if (MMAPI::Input::IsKeybindPressed(kb)) { ... }`
	///
	/// The struct is opaque-ish: `code` and `is_gamepad` are exposed for inspection but
	/// most callers only need IsValid() + IsKeybindPressed() / IsKeybindDown().
	struct Keybind
	{
		int  code       = -1;     ///< Either a KeyboardKeys/VK_* code or a GamepadButtons/0x80xx code.
		bool is_gamepad = false;  ///< When true, `code` is a gamepad button; otherwise a keyboard code.

		constexpr Keybind() = default;
		constexpr Keybind(KeyboardKeys   key) : code(static_cast<int>(key)), is_gamepad(false) {}
		constexpr Keybind(GamepadButtons btn) : code(static_cast<int>(btn)), is_gamepad(true)  {}

		/// Returns true if this keybind was resolved to a known key/button.
		bool IsValid() const { return code >= 0; }

		/// Returns the canonical name of this keybind (e.g. "F1", "GAMEPAD_A"), or nullopt if
		/// the code doesn't match any known KeyboardKeys / GamepadButtons value. Useful for
		/// echoing user-configured bindings in notifications, config dumps, or debug overlays.
		std::optional<std::string> TryToString() const;
	};

	namespace Internal
	{
		inline bool enabled = false;

		// Re-guard for the `DELETE` Windows.h macro — the map literal below references
		// KeyboardKeys::DELETE, which the preprocessor would otherwise expand to a numeric literal.
		#pragma push_macro("DELETE")
		#undef DELETE

		/// Canonical mapping of keyboard-keybind names (as used in user config files) to typed
		/// KeyboardKeys enum values. Names are uppercase, use underscores for compound keys
		/// ("PAGE_UP", "NUMPAD_0"), and "ARROW_*" prefix for arrows. Lazily-initialized singleton.
		inline const std::map<std::string, KeyboardKeys>& NameToKeyboardKey()
		{
			static const std::map<std::string, KeyboardKeys> m = {
				{ "F1", KeyboardKeys::F1 }, { "F2", KeyboardKeys::F2 }, { "F3", KeyboardKeys::F3 }, { "F4", KeyboardKeys::F4 },
				{ "F5", KeyboardKeys::F5 }, { "F6", KeyboardKeys::F6 }, { "F7", KeyboardKeys::F7 }, { "F8", KeyboardKeys::F8 },
				{ "F9", KeyboardKeys::F9 }, { "F10", KeyboardKeys::F10 }, { "F11", KeyboardKeys::F11 }, { "F12", KeyboardKeys::F12 },
				{ "NUMPAD_0", KeyboardKeys::NUMPAD_0 }, { "NUMPAD_1", KeyboardKeys::NUMPAD_1 }, { "NUMPAD_2", KeyboardKeys::NUMPAD_2 },
				{ "NUMPAD_3", KeyboardKeys::NUMPAD_3 }, { "NUMPAD_4", KeyboardKeys::NUMPAD_4 }, { "NUMPAD_5", KeyboardKeys::NUMPAD_5 },
				{ "NUMPAD_6", KeyboardKeys::NUMPAD_6 }, { "NUMPAD_7", KeyboardKeys::NUMPAD_7 }, { "NUMPAD_8", KeyboardKeys::NUMPAD_8 },
				{ "NUMPAD_9", KeyboardKeys::NUMPAD_9 },
				{ "0", KeyboardKeys::DIGIT_0 }, { "1", KeyboardKeys::DIGIT_1 }, { "2", KeyboardKeys::DIGIT_2 },
				{ "3", KeyboardKeys::DIGIT_3 }, { "4", KeyboardKeys::DIGIT_4 }, { "5", KeyboardKeys::DIGIT_5 },
				{ "6", KeyboardKeys::DIGIT_6 }, { "7", KeyboardKeys::DIGIT_7 }, { "8", KeyboardKeys::DIGIT_8 },
				{ "9", KeyboardKeys::DIGIT_9 },
				{ "A", KeyboardKeys::A }, { "B", KeyboardKeys::B }, { "C", KeyboardKeys::C }, { "D", KeyboardKeys::D },
				{ "E", KeyboardKeys::E }, { "F", KeyboardKeys::F }, { "G", KeyboardKeys::G }, { "H", KeyboardKeys::H },
				{ "I", KeyboardKeys::I }, { "J", KeyboardKeys::J }, { "K", KeyboardKeys::K }, { "L", KeyboardKeys::L },
				{ "M", KeyboardKeys::M }, { "N", KeyboardKeys::N }, { "O", KeyboardKeys::O }, { "P", KeyboardKeys::P },
				{ "Q", KeyboardKeys::Q }, { "R", KeyboardKeys::R }, { "S", KeyboardKeys::S }, { "T", KeyboardKeys::T },
				{ "U", KeyboardKeys::U }, { "V", KeyboardKeys::V }, { "W", KeyboardKeys::W }, { "X", KeyboardKeys::X },
				{ "Y", KeyboardKeys::Y }, { "Z", KeyboardKeys::Z },
				{ "SHIFT", KeyboardKeys::SHIFT }, { "CTRL", KeyboardKeys::CTRL }, { "ALT", KeyboardKeys::ALT },
				{ "ARROW_UP", KeyboardKeys::ARROW_UP }, { "ARROW_DOWN", KeyboardKeys::ARROW_DOWN },
				{ "ARROW_LEFT", KeyboardKeys::ARROW_LEFT }, { "ARROW_RIGHT", KeyboardKeys::ARROW_RIGHT },
				{ "SPACE", KeyboardKeys::SPACE }, { "ENTER", KeyboardKeys::ENTER }, { "ESCAPE", KeyboardKeys::ESCAPE },
				{ "TAB", KeyboardKeys::TAB }, { "BACKSPACE", KeyboardKeys::BACKSPACE },
				{ "INSERT", KeyboardKeys::INSERT }, { "DELETE", KeyboardKeys::DELETE }, { "HOME", KeyboardKeys::HOME },
				{ "PAGE_UP", KeyboardKeys::PAGE_UP }, { "PAGE_DOWN", KeyboardKeys::PAGE_DOWN },
				{ "NUM_LOCK", KeyboardKeys::NUM_LOCK }, { "SCROLL_LOCK", KeyboardKeys::SCROLL_LOCK },
				{ "CAPS_LOCK", KeyboardKeys::CAPS_LOCK }, { "PAUSE_BREAK", KeyboardKeys::PAUSE_BREAK },
			};
			return m;
		}

		#pragma pop_macro("DELETE")

		/// Canonical mapping of gamepad-button names to typed GamepadButtons enum values.
		/// Names are uppercase and use the "GAMEPAD_*" prefix. Lazily-initialized singleton.
		inline const std::map<std::string, GamepadButtons>& NameToGamepadButton()
		{
			static const std::map<std::string, GamepadButtons> m = {
				{ "GAMEPAD_A", GamepadButtons::GAMEPAD_A }, { "GAMEPAD_B", GamepadButtons::GAMEPAD_B },
				{ "GAMEPAD_X", GamepadButtons::GAMEPAD_X }, { "GAMEPAD_Y", GamepadButtons::GAMEPAD_Y },
				{ "GAMEPAD_LEFT_SHOULDER",  GamepadButtons::GAMEPAD_LEFT_SHOULDER },
				{ "GAMEPAD_RIGHT_SHOULDER", GamepadButtons::GAMEPAD_RIGHT_SHOULDER },
				{ "GAMEPAD_LEFT_TRIGGER",   GamepadButtons::GAMEPAD_LEFT_TRIGGER },
				{ "GAMEPAD_RIGHT_TRIGGER",  GamepadButtons::GAMEPAD_RIGHT_TRIGGER },
				{ "GAMEPAD_SELECT", GamepadButtons::GAMEPAD_SELECT }, { "GAMEPAD_START", GamepadButtons::GAMEPAD_START },
				{ "GAMEPAD_LEFT_STICK",  GamepadButtons::GAMEPAD_LEFT_STICK },
				{ "GAMEPAD_RIGHT_STICK", GamepadButtons::GAMEPAD_RIGHT_STICK },
				{ "GAMEPAD_DPAD_UP",    GamepadButtons::GAMEPAD_DPAD_UP },
				{ "GAMEPAD_DPAD_DOWN",  GamepadButtons::GAMEPAD_DPAD_DOWN },
				{ "GAMEPAD_DPAD_LEFT",  GamepadButtons::GAMEPAD_DPAD_LEFT },
				{ "GAMEPAD_DPAD_RIGHT", GamepadButtons::GAMEPAD_DPAD_RIGHT },
			};
			return m;
		}

		/// Reverse lookup for Keybind::TryToString() — built once from NameToKeyboardKey by inversion.
		/// If the forward map ever gets duplicate enum values mapped to different strings, the last
		/// one wins (std::map insertion semantics); current entries are 1:1 so this is deterministic.
		inline const std::map<KeyboardKeys, std::string>& KeyboardKeyToName()
		{
			static const std::map<KeyboardKeys, std::string> m = []() {
				std::map<KeyboardKeys, std::string> result;
				for (const auto& [name, key] : NameToKeyboardKey())
					result[key] = name;
				return result;
			}();
			return m;
		}

		/// Reverse lookup for Keybind::TryToString() — built once from NameToGamepadButton.
		inline const std::map<GamepadButtons, std::string>& GamepadButtonToName()
		{
			static const std::map<GamepadButtons, std::string> m = []() {
				std::map<GamepadButtons, std::string> result;
				for (const auto& [name, button] : NameToGamepadButton())
					result[button] = name;
				return result;
			}();
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

	// Out-of-line because the reverse maps live in Internal — Keybind itself is declared before
	// Internal, so we provide only the declaration on the struct and define the body here.
	inline std::optional<std::string> Keybind::TryToString() const
	{
		if (!IsValid())
			return std::nullopt;

		if (is_gamepad)
		{
			const auto& m = Internal::GamepadButtonToName();
			if (auto it = m.find(static_cast<GamepadButtons>(code)); it != m.end())
				return it->second;
			return std::nullopt;
		}

		const auto& m = Internal::KeyboardKeyToName();
		if (auto it = m.find(static_cast<KeyboardKeys>(code)); it != m.end())
			return it->second;
		return std::nullopt;
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
	/// representation. Accepts:
	///   - Keyboard: "F1"-"F12", "A"-"Z", "0"-"9", "NUMPAD_0"-"NUMPAD_9",
	///     "SHIFT", "CTRL", "ALT",
	///     "ARROW_UP", "ARROW_DOWN", "ARROW_LEFT", "ARROW_RIGHT",
	///     "SPACE", "ENTER", "ESCAPE", "TAB", "BACKSPACE",
	///     "INSERT", "DELETE", "HOME", "PAGE_UP", "PAGE_DOWN",
	///     "NUM_LOCK", "SCROLL_LOCK", "CAPS_LOCK", "PAUSE_BREAK"
	///   - Gamepad: "GAMEPAD_A", "GAMEPAD_B", "GAMEPAD_X", "GAMEPAD_Y",
	///     "GAMEPAD_LEFT_SHOULDER", "GAMEPAD_RIGHT_SHOULDER",
	///     "GAMEPAD_LEFT_TRIGGER", "GAMEPAD_RIGHT_TRIGGER",
	///     "GAMEPAD_SELECT", "GAMEPAD_START",
	///     "GAMEPAD_LEFT_STICK", "GAMEPAD_RIGHT_STICK",
	///     "GAMEPAD_DPAD_UP", "GAMEPAD_DPAD_DOWN", "GAMEPAD_DPAD_LEFT", "GAMEPAD_DPAD_RIGHT"
	///
	/// Names are case-sensitive (uppercase). For compile-time-known bindings, construct a Keybind
	/// directly from the typed enums instead: `Keybind(KeyboardKeys::F1)` / `Keybind(GamepadButtons::GAMEPAD_A)`.
	/// @param name The keybind name string to parse.
	/// @return A `Keybind` on success, or `std::nullopt` if `name` doesn't match any known key/button.
	inline std::optional<Keybind> TryParseKeybind(std::string_view name)
	{
		std::string key(name);

		if (auto it = Internal::NameToGamepadButton().find(key); it != Internal::NameToGamepadButton().end())
			return Keybind(it->second);

		if (auto it = Internal::NameToKeyboardKey().find(key); it != Internal::NameToKeyboardKey().end())
			return Keybind(it->second);

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
