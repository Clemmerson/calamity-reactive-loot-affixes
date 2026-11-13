#pragma once

// Runeforge workstation input mapping (plan section 6; RC1 decisions WS-D02, WS-D15).
// Pure policy: resolved user events plus fixed Back-key identities in, workstation commands out. ControlMap
// remaps remain honored for normal controls; Esc/Tab/B are fixed Back controls and View/Back retains its
// emergency-hold behavior.

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

#include "CalamityAffixes/RuneforgeWorkstationStatePolicy.h"

namespace CalamityAffixes::RuneforgeWorkstation::InputPolicy
{
	using StatePolicy::State;

	enum class Button : std::uint8_t
	{
		kUp,
		kDown,
		kLeft,
		kRight,
		kPageUp,
		kPageDown,
		kAccept,
		kCancel,
		kOps,
		kDetail,
		kViewBack,
		kF10,
		kOther
	};

	enum class Phase : std::uint8_t
	{
		kDown,
		kHeld,
		kUp
	};

	enum class Device : std::uint8_t
	{
		kKeyboard,
		kMouse,
		kGamepad
	};

	// Default physical key/button IDs from CommonLib's BSKeyboardDevice and BSWin32GamepadDevice.
	// The native caller static-asserts these against the vendored engine declarations.
	inline constexpr std::uint32_t kKeyboardEscapeCode = 0x0001u;
	inline constexpr std::uint32_t kKeyboardTabCode = 0x000Fu;
	inline constexpr std::uint32_t kGamepadBackCode = 0x0020u;
	inline constexpr std::uint32_t kGamepadBCode = 0x2000u;
	inline constexpr std::uint32_t kMouseButtonOffset = 256u;

	// ControlMap stores mouse controls in the same offset namespace used by Skyrim's input events.
	// Decode known buttons and wheel directions; keep unusual extra buttons explicit and device-scoped
	// instead of letting their numeric codes collide with gamepad labels.
	[[nodiscard]] inline std::string MousePromptName(std::uint32_t a_keyCode)
	{
		if (a_keyCode == 0u || a_keyCode == 0xFFu || a_keyCode == 0xFFFFFFFFu) {
			return "MOUSE UNBOUND";
		}
		const auto mouse = a_keyCode >= kMouseButtonOffset ? a_keyCode - kMouseButtonOffset : a_keyCode;
		switch (mouse) {
		case 0u: return "MOUSE LEFT";
		case 1u: return "MOUSE RIGHT";
		case 2u: return "MOUSE MIDDLE";
		case 3u: return "MOUSE 4";
		case 4u: return "MOUSE 5";
		case 8u: return "WHEEL UP";
		case 9u: return "WHEEL DOWN";
		default: return "MOUSE BUTTON " + std::to_string(mouse);
		}
	}

	// Workstation mouse prompts describe the clickable Scaleform actions, not ControlMap bindings.
	[[nodiscard]] constexpr std::string_view MouseClickPromptName() noexcept
	{
		return "LMB";
	}

	enum class Command : std::uint8_t
	{
		kNone,
		kNavUp,
		kNavDown,
		kPageUp,
		kPageDown,
		kFilterPrev,
		kFilterNext,
		kAccept,
		kHoldBegin,
		kHoldEnd,
		kBack,
		kOps,
		kDetail,
		kSkip,
		kEmergency
	};

	struct Timing
	{
		float repeatDelaySeconds{ 0.35f };
		float repeatIntervalSeconds{ 0.08f };
		float emergencyHoldSeconds{ 2.0f };
		float stickDeadZone{ 0.35f };
	};

	// Per-button repeat bookkeeping for one held navigation button.
	struct Tracker
	{
		Button heldNav{ Button::kOther };
		std::uint32_t repeatsEmitted{ 0 };
		bool emergencyFired{ false };
	};

	struct Decision
	{
		Command command{ Command::kNone };
		// Every workstation button is consumed: the menu context owns input for the whole session, and the
		// release/repeat of a state-changing press must never leak to the next screen (the RawCancel lesson).
		bool consume{ true };
	};

	[[nodiscard]] constexpr Button ButtonForUserEvent(std::string_view a_userEvent) noexcept
	{
		if (a_userEvent == "Up") return Button::kUp;
		if (a_userEvent == "Down") return Button::kDown;
		if (a_userEvent == "Left") return Button::kLeft;
		if (a_userEvent == "Right") return Button::kRight;
		if (a_userEvent == "PrevPage" || a_userEvent == "LeftShoulder") return Button::kPageUp;
		if (a_userEvent == "NextPage" || a_userEvent == "RightShoulder") return Button::kPageDown;
		if (a_userEvent == "Accept" || a_userEvent == "Activate") return Button::kAccept;
		if (a_userEvent == "Cancel" || a_userEvent == "Tween Menu" || a_userEvent == "Journal") return Button::kCancel;
		if (a_userEvent == "XButton") return Button::kOps;
		if (a_userEvent == "YButton") return Button::kDetail;
		return Button::kOther;
	}

	// QUserEvent is not reliably populated for these physical menu Back keys in the workstation's
	// custom input context. Prefer the fixed physical controls, then retain ControlMap remaps for
	// every other input. The device check prevents a mouse ID from becoming keyboard Back.
	[[nodiscard]] constexpr Button ButtonForInputEvent(
		std::string_view a_userEvent,
		Device a_device,
		std::uint32_t a_idCode) noexcept
	{
		if (a_device == Device::kKeyboard && (a_idCode == kKeyboardEscapeCode || a_idCode == kKeyboardTabCode)) {
			return Button::kCancel;
		}
		if (a_device == Device::kGamepad && a_idCode == kGamepadBackCode) {
			return Button::kViewBack;
		}
		if (a_device == Device::kGamepad && a_idCode == kGamepadBCode) {
			return Button::kCancel;
		}
		const auto mapped = ButtonForUserEvent(a_userEvent);
		if (mapped != Button::kOther) {
			return mapped;
		}
		return Button::kOther;
	}

	[[nodiscard]] constexpr bool IsNavigation(Button a_button) noexcept
	{
		return a_button == Button::kUp || a_button == Button::kDown || a_button == Button::kPageUp || a_button == Button::kPageDown ||
			a_button == Button::kLeft || a_button == Button::kRight;
	}

	// How many repeats are due for a navigation button held for a_held seconds (0 before the delay).
	[[nodiscard]] inline std::uint32_t RepeatsDue(float a_held, const Timing& a_timing = {}) noexcept
	{
		if (a_held < a_timing.repeatDelaySeconds) {
			return 0u;
		}
		return 1u + static_cast<std::uint32_t>(std::floor((a_held - a_timing.repeatDelaySeconds) / a_timing.repeatIntervalSeconds + 1e-4f));
	}

	[[nodiscard]] constexpr bool SelectionState(State a_state) noexcept
	{
		return a_state == State::kSelectItem || a_state == State::kSelectFormula || a_state == State::kOperations;
	}

	// Scaleform wheel deltas vary by device (commonly +/-1, +/-3, or +/-120). The workstation treats each
	// callback as one detent so high-resolution mice cannot skip rows or enqueue large navigation bursts.
	[[nodiscard]] inline int NormalizeWheelDelta(float a_delta) noexcept
	{
		if (!std::isfinite(a_delta) || a_delta == 0.0f) return 0;
		return a_delta > 0.0f ? 1 : -1;
	}

	[[nodiscard]] constexpr Command NavigationCommand(State a_state, Button a_button) noexcept
	{
		if (!SelectionState(a_state)) {
			return Command::kNone;
		}
		switch (a_button) {
		case Button::kUp: return Command::kNavUp;
		case Button::kDown: return Command::kNavDown;
		case Button::kPageUp: return Command::kPageUp;
		case Button::kPageDown: return Command::kPageDown;
		case Button::kLeft: return a_state == State::kOperations ? Command::kNone : Command::kFilterPrev;
		case Button::kRight: return a_state == State::kOperations ? Command::kNone : Command::kFilterNext;
		default: return Command::kNone;
		}
	}

	// Maps one button phase to a workstation command for the current state.
	inline Decision Map(State a_state, Button a_button, Phase a_phase, float a_held, Tracker& a_tracker, const Timing& a_timing = {})
	{
		Decision decision{};
		if (a_state == State::kIdle) {
			decision.consume = false;
			return decision;
		}
		if (a_button == Button::kF10) {
			if (a_phase == Phase::kDown) decision.command = Command::kEmergency;
			return decision;
		}
		if (a_button == Button::kViewBack) {
			if (a_phase == Phase::kDown) {
				a_tracker.emergencyFired = false;
			} else if (a_phase == Phase::kHeld && !a_tracker.emergencyFired && a_held >= a_timing.emergencyHoldSeconds) {
				a_tracker.emergencyFired = true;
				decision.command = Command::kEmergency;
			} else if (a_phase == Phase::kUp) {
				if (!a_tracker.emergencyFired) decision.command = Command::kBack;
				a_tracker.emergencyFired = false;
			}
			return decision;
		}
		if (IsNavigation(a_button)) {
			if (a_phase == Phase::kDown) {
				a_tracker.heldNav = a_button;
				a_tracker.repeatsEmitted = 0;
				decision.command = NavigationCommand(a_state, a_button);
			} else if (a_phase == Phase::kHeld && a_tracker.heldNav == a_button) {
				// Held directions repeat after the delay, at most one step per input event (never queued).
				if (RepeatsDue(a_held, a_timing) > a_tracker.repeatsEmitted) {
					a_tracker.repeatsEmitted += 1;
					if (a_button != Button::kLeft && a_button != Button::kRight) {
						decision.command = NavigationCommand(a_state, a_button);
					}
				}
			} else if (a_phase == Phase::kUp && a_tracker.heldNav == a_button) {
				a_tracker.heldNav = Button::kOther;
				a_tracker.repeatsEmitted = 0;
			}
			return decision;
		}
		switch (a_state) {
		case State::kEntering:
			if (a_button == Button::kCancel && a_phase == Phase::kDown) decision.command = Command::kBack;
			break;
		case State::kSelectItem:
		case State::kSelectFormula:
		case State::kOperations:
			if (a_phase != Phase::kDown) break;
			if (a_button == Button::kAccept) decision.command = Command::kAccept;
			else if (a_button == Button::kCancel) decision.command = Command::kBack;
			else if (a_button == Button::kOps) decision.command = a_state == State::kSelectFormula ? Command::kOps :
				(a_state == State::kOperations ? Command::kBack : Command::kNone);
			// R8.7.4.21 I3: Details only where it has a visible effect (PromptPolicy::DetailVisible; OPERATIONS has none)
			else if (a_button == Button::kDetail && a_state != State::kOperations) decision.command = Command::kDetail;
			break;
		case State::kConfirm:
			if (a_button == Button::kAccept) {
				if (a_phase == Phase::kDown) decision.command = Command::kHoldBegin;
				else if (a_phase == Phase::kUp) decision.command = Command::kHoldEnd;
			} else if (a_button == Button::kCancel && a_phase == Phase::kDown) {
				decision.command = Command::kBack;
			} else if (a_button == Button::kDetail && a_phase == Phase::kDown) {
				// R8.7.4.21 I3: expands the effect card; never a hold command
				decision.command = Command::kDetail;
			}
			break;
		case State::kPerforming:
			if ((a_button == Button::kAccept || a_button == Button::kCancel) && a_phase == Phase::kDown) decision.command = Command::kSkip;
			break;
		case State::kResultSuccess:
		case State::kResultFailure:
			if (a_phase != Phase::kDown) break;
			if (a_button == Button::kAccept) decision.command = Command::kAccept;
			else if (a_button == Button::kCancel) decision.command = Command::kBack;
			break;
		case State::kCommitting:
		case State::kExiting:
		case State::kIdle:
			break;
		}
		return decision;
	}

	// Left thumbstick to a direction button (dead zone 0.35), dominant axis wins.
	[[nodiscard]] inline Button StickDirection(float a_x, float a_y, const Timing& a_timing = {}) noexcept
	{
		const float ax = std::fabs(a_x);
		const float ay = std::fabs(a_y);
		if (ax < a_timing.stickDeadZone && ay < a_timing.stickDeadZone) return Button::kOther;
		if (ay >= ax) return a_y > 0.0f ? Button::kUp : Button::kDown;
		return a_x > 0.0f ? Button::kRight : Button::kLeft;
	}

	// Commands that drive the state machine directly (the rest are list navigation handled by the session).
	[[nodiscard]] constexpr bool ToStateEvent(Command a_command, StatePolicy::Event& a_event) noexcept
	{
		using StatePolicy::EventKind;
		switch (a_command) {
		case Command::kAccept: a_event = { EventKind::kAccept, 1, 0, {} }; return true;
		case Command::kHoldBegin: a_event = { EventKind::kHoldBegin, 0, 0, {} }; return true;
		case Command::kHoldEnd: a_event = { EventKind::kHoldEnd, 0, 0, {} }; return true;
		case Command::kBack: a_event = { EventKind::kBack, 0, 0, {} }; return true;
		case Command::kSkip: a_event = { EventKind::kBack, 0, 0, {} }; return true;
		case Command::kOps: a_event = { EventKind::kOps, 0, 0, {} }; return true;
		case Command::kEmergency: a_event = { EventKind::kInterrupt, 0, 0, StatePolicy::Cause::kEmergency }; return true;
		default: return false;
		}
	}

	[[nodiscard]] constexpr std::string_view Name(Command a_command) noexcept
	{
		switch (a_command) {
		case Command::kNone: return "none";
		case Command::kNavUp: return "navUp";
		case Command::kNavDown: return "navDown";
		case Command::kPageUp: return "pageUp";
		case Command::kPageDown: return "pageDown";
		case Command::kFilterPrev: return "filterPrev";
		case Command::kFilterNext: return "filterNext";
		case Command::kAccept: return "accept";
		case Command::kHoldBegin: return "holdBegin";
		case Command::kHoldEnd: return "holdEnd";
		case Command::kBack: return "back";
		case Command::kOps: return "ops";
		case Command::kDetail: return "detail";
		case Command::kSkip: return "skip";
		case Command::kEmergency: return "emergency";
		}
		return "none";
	}
}
