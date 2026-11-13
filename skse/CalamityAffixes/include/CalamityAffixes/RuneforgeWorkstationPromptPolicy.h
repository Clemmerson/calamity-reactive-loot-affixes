#pragma once

// Runeforge workstation prompt truth (R8.7.4.18). Pure policy, unit-tested in tests/runeforge_workstation_policy_tests.cpp.
//
// Live R8.7.4.17 finding: ordinary mouse motion (and the movie's own pointer/hover reports on every redraw) set the input
// device to mouse, and every footer prompt then read "LMB". The footer buttons are real click targets, but four identical key
// caps told keyboard and controller players nothing.
//
// Rules here:
//  * the device only changes on deliberate input: a key, a gamepad button, a stick past the dead zone, a mouse click, the
//    wheel or a right-drag. Mouse motion, pointer enter/leave and hover previews never change it;
//  * prompt key caps always name the last keyboard or controller binding (never the mouse); a separate pointer flag lets the
//    view add a small "click" cue for mouse users;
//  * names come from the live ControlMap binding (remaps honoured); an unbound control reads "UNSET", except the hardwired
//    fallbacks the input sink really accepts (keyboard E / gamepad A = Accept; Esc, Tab, gamepad B = Back);
//  * Xbox names by default; PlayStation names only when the player chose that family (runeforge_workstation.json
//    "promptFamily"; SteamInput reports PlayStation pads as XInput, so the family cannot be detected reliably).

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "CalamityAffixes/RuneforgeWorkstationInputPolicy.h"
#include "CalamityAffixes/RuneforgeWorkstationStatePolicy.h"

namespace CalamityAffixes::RuneforgeWorkstation::PromptPolicy
{
	using InputPolicy::Button;
	using InputPolicy::Device;
	using StatePolicy::State;

	enum class InputKind : std::uint8_t
	{
		kMouseMove,       // raw mouse motion
		kMouseButton,     // a raw mouse button (the right-button pan)
		kMouseWheel,      // the wheel (raw or from the movie)
		kKeyboard,        // a keyboard key
		kGamepadButton,   // a gamepad button or trigger
		kGamepadStick,    // a thumbstick event (magnitude decides)
		kUiPointer,       // the movie: cursor entered or left the panel
		kUiHover,         // the movie: hover preview of a row
		kUiClick          // the movie: a click on a row or footer action
	};

	inline constexpr float kStickDeadZone = 0.35f;

	[[nodiscard]] constexpr Device NextDevice(Device a_current, InputKind a_kind, float a_stickMagnitude = 0.0f) noexcept
	{
		switch (a_kind) {
		case InputKind::kMouseMove:
		case InputKind::kUiPointer:
		case InputKind::kUiHover:
			return a_current;
		case InputKind::kMouseButton:
		case InputKind::kMouseWheel:
		case InputKind::kUiClick:
			return Device::kMouse;
		case InputKind::kKeyboard:
			return Device::kKeyboard;
		case InputKind::kGamepadButton:
			return Device::kGamepad;
		case InputKind::kGamepadStick:
			return a_stickMagnitude >= kStickDeadZone ? Device::kGamepad : a_current;
		}
		return a_current;
	}

	// The device whose bindings the key caps show: the last keyboard/controller device (the mouse has no key caps).
	[[nodiscard]] constexpr Device NextPromptDevice(Device a_promptDevice, Device a_device) noexcept
	{
		return a_device == Device::kMouse ? a_promptDevice : a_device;
	}

	// Movie callbacks that are a deliberate mouse action. "pointer" (enter/leave) and "preview" (hover) are not.
	[[nodiscard]] constexpr bool CallbackIsDeliberateClick(std::string_view a_name) noexcept
	{
		return a_name == "accept" || a_name == "back" || a_name == "holdBegin" || a_name == "holdEnd" || a_name == "ops" ||
		       a_name == "filterNext" || a_name == "detail" || a_name == "pageUp" || a_name == "pageDown" || a_name == "focus";
	}

	[[nodiscard]] constexpr bool CallbackIsWheel(std::string_view a_name) noexcept
	{
		return a_name == "wheelUp" || a_name == "wheelDown";
	}

	// ---------------------------------------------------------------- names

	inline constexpr std::string_view kUnset = "UNSET";

	// DirectInput scan codes (BSKeyboardDevice::Keys); the native caller static-asserts the anchors against CommonLib.
	[[nodiscard]] inline std::string KeyboardKeyName(std::uint32_t a_code)
	{
		switch (a_code) {
		case 0x01: return "ESC";
		case 0x0C: return "-";
		case 0x0D: return "=";
		case 0x0E: return "BKSP";
		case 0x0F: return "TAB";
		case 0x1A: return "[";
		case 0x1B: return "]";
		case 0x1C: return "ENTER";
		case 0x1D: return "LCTRL";
		case 0x27: return ";";
		case 0x28: return "'";
		case 0x29: return "`";
		case 0x2A: return "LSHIFT";
		case 0x2B: return "\\";
		case 0x33: return ",";
		case 0x34: return ".";
		case 0x35: return "/";
		case 0x36: return "RSHIFT";
		case 0x37: return "NUM*";
		case 0x38: return "LALT";
		case 0x39: return "SPACE";
		case 0x3A: return "CAPS";
		case 0x45: return "NUMLK";
		case 0x46: return "SCRLK";
		case 0x4A: return "NUM-";
		case 0x4E: return "NUM+";
		case 0x53: return "NUM.";
		case 0x57: return "F11";
		case 0x58: return "F12";
		case 0x9C: return "NUMENT";
		case 0x9D: return "RCTRL";
		case 0xB5: return "NUM/";
		case 0xB8: return "RALT";
		case 0xC7: return "HOME";
		case 0xC8: return "UP";
		case 0xC9: return "PGUP";
		case 0xCB: return "LEFT";
		case 0xCD: return "RIGHT";
		case 0xCF: return "END";
		case 0xD0: return "DOWN";
		case 0xD1: return "PGDN";
		case 0xD2: return "INS";
		case 0xD3: return "DEL";
		default: break;
		}
		if (a_code >= 0x02 && a_code <= 0x0B) return std::string(1, "1234567890"[a_code - 0x02]);
		if (a_code >= 0x10 && a_code <= 0x19) return std::string(1, "QWERTYUIOP"[a_code - 0x10]);
		if (a_code >= 0x1E && a_code <= 0x26) return std::string(1, "ASDFGHJKL"[a_code - 0x1E]);
		if (a_code >= 0x2C && a_code <= 0x32) return std::string(1, "ZXCVBNM"[a_code - 0x2C]);
		if (a_code >= 0x3B && a_code <= 0x44) return "F" + std::to_string(a_code - 0x3B + 1);
		switch (a_code) {
		case 0x47: return "NUM7";
		case 0x48: return "NUM8";
		case 0x49: return "NUM9";
		case 0x4B: return "NUM4";
		case 0x4C: return "NUM5";
		case 0x4D: return "NUM6";
		case 0x4F: return "NUM1";
		case 0x50: return "NUM2";
		case 0x51: return "NUM3";
		case 0x52: return "NUM0";
		default: return std::string(kUnset);
		}
	}

	// BSWin32GamepadDevice::Keys (XInput button bits; triggers 0x09/0x0A).
	inline constexpr std::uint32_t kPadUp = 0x0001u, kPadDown = 0x0002u, kPadLeft = 0x0004u, kPadRight = 0x0008u, kPadStart = 0x0010u,
	                               kPadBack = 0x0020u, kPadLeftThumb = 0x0040u, kPadRightThumb = 0x0080u, kPadLeftShoulder = 0x0100u,
	                               kPadRightShoulder = 0x0200u, kPadA = 0x1000u, kPadB = 0x2000u, kPadX = 0x4000u, kPadY = 0x8000u,
	                               kPadLeftTrigger = 0x0009u, kPadRightTrigger = 0x000Au;

	// Face buttons keep their canonical letters (the view draws Xbox letters or PlayStation shapes from them by family).
	[[nodiscard]] inline std::string GamepadKeyName(std::uint32_t a_code, std::string_view a_family)
	{
		const bool ps = a_family == "playstation";
		switch (a_code) {
		case kPadA: return "A";
		case kPadB: return "B";
		case kPadX: return "X";
		case kPadY: return "Y";
		case kPadLeftShoulder: return ps ? "L1" : "LB";
		case kPadRightShoulder: return ps ? "R1" : "RB";
		case kPadLeftTrigger: return ps ? "L2" : "LT";
		case kPadRightTrigger: return ps ? "R2" : "RT";
		case kPadLeftThumb: return ps ? "L3" : "LS";
		case kPadRightThumb: return ps ? "R3" : "RS";
		case kPadUp: return "UP";
		case kPadDown: return "DOWN";
		case kPadLeft: return "LEFT";
		case kPadRight: return "RIGHT";
		case kPadStart: return ps ? "OPTIONS" : "MENU";
		case kPadBack: return ps ? "SHARE" : "VIEW";
		default: return std::string(kUnset);
		}
	}

	// MenuMode user event the workstation resolves each button through (the same names the input sink accepts).
	[[nodiscard]] constexpr std::string_view UserEventFor(Button a_button) noexcept
	{
		switch (a_button) {
		case Button::kAccept: return "Accept";
		case Button::kCancel: return "Cancel";
		case Button::kOps: return "XButton";
		case Button::kDetail: return "YButton";
		case Button::kPageUp: return "PrevPage";
		case Button::kPageDown: return "NextPage";
		case Button::kLeft: return "Left";
		case Button::kRight: return "Right";
		case Button::kUp: return "Up";
		case Button::kDown: return "Down";
		default: return "";
		}
	}

	// Name when the ControlMap has no binding: only the controls the sink accepts regardless of the map (truthful).
	[[nodiscard]] inline std::string UnboundName(Button a_button, Device a_device)
	{
		if (a_device == Device::kGamepad) {
			if (a_button == Button::kAccept) return "A";   // ResolveWorkstationFallbackButton: gamepad A = Accept
			if (a_button == Button::kCancel) return "B";   // ButtonForInputEvent: gamepad B is fixed Back
			return std::string(kUnset);
		}
		if (a_button == Button::kAccept) return "E";       // ResolveWorkstationFallbackButton: keyboard E = Accept
		if (a_button == Button::kCancel) return "ESC";     // ButtonForInputEvent: Esc and Tab are fixed Back
		return std::string(kUnset);
	}

	// Full resolution: a_mappedCode is ControlMap::GetMappedKey(UserEventFor(button), device, MenuMode) (0xFF = unbound).
	[[nodiscard]] inline std::string PromptKeyName(Button a_button, Device a_promptDevice, std::uint32_t a_mappedCode, std::string_view a_family)
	{
		if (a_mappedCode == 0xFFu || a_mappedCode == 0xFFFFFFFFu) return UnboundName(a_button, a_promptDevice);
		std::string name = a_promptDevice == Device::kGamepad ? GamepadKeyName(a_mappedCode, a_family) : KeyboardKeyName(a_mappedCode);
		return name == kUnset ? UnboundName(a_button, a_promptDevice) : name;
	}

	[[nodiscard]] constexpr std::string_view DeviceName(Device a_device) noexcept
	{
		return a_device == Device::kGamepad ? "gamepad" : (a_device == Device::kMouse ? "mouse" : "keyboard");
	}

	// ---------------------------------------------------------------- footer plan per state

	struct PromptSpec
	{
		std::string_view action;  // the movie's command name for a click (and the parity key for tests)
		Button button;            // the input whose binding the key cap names
		std::string_view label;
	};

	struct FooterPlan
	{
		std::array<PromptSpec, 4> prompts{};
		std::size_t count{ 0 };
	};

	// R8.7.4.21 I3 (B5 G9, DD-B5-4): the states where the Details toggle changes what the panel shows (the effect card
	// expands). The footer advertises Details exactly there and InputPolicy maps the Details button exactly there.
	[[nodiscard]] constexpr bool DetailVisible(State a_state) noexcept
	{
		return a_state == State::kSelectItem || a_state == State::kSelectFormula || a_state == State::kConfirm;
	}

	// Details never touches the engrave hold: while the hold ring is filling in CONFIRM the toggle is ignored (no panel
	// rebuild under a held prompt), and it never issues HoldBegin/HoldEnd itself.
	[[nodiscard]] constexpr bool DetailToggleAllowed(State a_state, bool a_holdActive) noexcept
	{
		return DetailVisible(a_state) && !(a_state == State::kConfirm && a_holdActive);
	}

	[[nodiscard]] constexpr FooterPlan PlanFooter(State a_state, bool a_detailOpen) noexcept
	{
		FooterPlan f{};
		auto add = [&f](std::string_view a, Button b, std::string_view l) { f.prompts[f.count++] = { a, b, l }; };
		switch (a_state) {
		case State::kSelectItem:
			add("accept", Button::kAccept, "Select");
			add("filterNext", Button::kRight, "Filter");
			add("detail", Button::kDetail, a_detailOpen ? "Hide Details" : "Details");
			add("back", Button::kCancel, "Exit");
			break;
		case State::kSelectFormula:
			add("accept", Button::kAccept, "Select");
			add("ops", Button::kOps, "Operations");
			add("detail", Button::kDetail, a_detailOpen ? "Hide Details" : "Details");
			add("back", Button::kCancel, "Back");
			break;
		case State::kConfirm:
			add("hold", Button::kAccept, "Hold to engrave");
			add("detail", Button::kDetail, a_detailOpen ? "Hide Details" : "Details");
			add("back", Button::kCancel, "Back");
			break;
		case State::kResultFailure:
			add("back", Button::kCancel, "Back");
			break;
		case State::kOperations:
			add("accept", Button::kAccept, "Select");
			add("back", Button::kCancel, "Back");
			break;
		case State::kCommitting:
			// R8.7.4.18: the authoritative write is in flight and no input acts on it (InputPolicy::Map gives no command):
			// the pre-R18 footer offered "Skip" here, which was not true. No prompt.
			break;
		case State::kPerforming:
			add("skip", Button::kAccept, "Skip");
			break;
		case State::kResultSuccess:
			add("accept", Button::kAccept, "Continue");
			break;
		default:
			break;
		}
		return f;
	}
}
