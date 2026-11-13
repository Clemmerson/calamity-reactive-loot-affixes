#pragma once

// Runeforge inscription reader input / exit contract (R8.7.4.14, 2026-11-07).
//
// Live failure U1 (R8.7.4.13, 2026-11-06): the stone reader rendered page 1 of 2 with a pointer, but Next, Previous,
// Close and Escape did nothing and the player had to leave the game. Every reader action then depended on the SKSE task
// queue (JS listener -> QueueTask -> HandleUiCommand -> QueueTask -> step/close; gamepad -> QueueTask), and the last
// native activity in the log was the tick that focused the view with Prisma's pauseGame=true -- the one setting in which
// the reader differed from the live-proven control panel (pauseGame=false).
//
// The contract below removes every single point of failure between a player input and a reader exit:
//   * keyboard close keys are handled natively in the input-device sink (no JS, no task queue); close acts on release so
//     the press is absorbed by the focus menu and cannot open the Tween/Journal menu once the reader is gone;
//   * keyboard page keys and gamepad buttons (raw codes) are handled natively on press;
//   * JS button commands are posted to a mailbox that the input sink (every frame) and the tick both drain;
//   * the view is focused without pausing (same as the control panel);
//   * focus is never stranded: if focus cannot be acquired, or is lost after it was acquired, the reader closes.
// Everything here is pure so the host tests can prove it without CommonLib.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace CalamityAffixes::RuneforgeReaderContract
{
	enum class Action : std::uint8_t
	{
		kNone,
		kPrevious,
		kNext,
		kClose,
	};

	enum class Source : std::uint8_t
	{
		kJs,
		kKeyboard,
		kGamepad,
		kFocusLost,
		kFocusUnavailable,
		kFocusMenuClosed,
		kGameplayUnavailable,
		kPanelOpened,
	};

	[[nodiscard]] constexpr std::string_view ToString(Action a_action) noexcept
	{
		switch (a_action) {
		case Action::kPrevious:
			return "previous";
		case Action::kNext:
			return "next";
		case Action::kClose:
			return "close";
		case Action::kNone:
		default:
			return "none";
		}
	}

	[[nodiscard]] constexpr std::string_view ToString(Source a_source) noexcept
	{
		switch (a_source) {
		case Source::kJs:
			return "js";
		case Source::kKeyboard:
			return "keyboard";
		case Source::kGamepad:
			return "gamepad";
		case Source::kFocusLost:
			return "focus-lost";
		case Source::kFocusUnavailable:
			return "focus-unavailable";
		case Source::kFocusMenuClosed:
			return "focus-menu-closed";
		case Source::kGameplayUnavailable:
			return "gameplay-unavailable";
		case Source::kPanelOpened:
			return "panel-opened";
		default:
			return "unknown";
		}
	}

	// The reader must not pause the game through Prisma: pauseGame=true is the configuration that soft-locked live.
	inline constexpr bool kFocusPausesGame = false;
	inline constexpr bool kFocusDisablesFocusMenu = false;

	// Name of Prisma's modal focus menu (observed live in the menu stack while the reader was open).
	inline constexpr std::string_view kPrismaFocusMenuName = "PrismaUI_FocusMenu";

	// DirectInput scan codes (RE::BSKeyboardDevice::Key values).
	inline constexpr std::uint32_t kKeyEscape = 0x01;
	inline constexpr std::uint32_t kKeyBackspace = 0x0E;
	inline constexpr std::uint32_t kKeyTab = 0x0F;
	inline constexpr std::uint32_t kKeyA = 0x1E;
	inline constexpr std::uint32_t kKeyD = 0x20;
	inline constexpr std::uint32_t kKeyPageUp = 0xC9;
	inline constexpr std::uint32_t kKeyLeft = 0xCB;
	inline constexpr std::uint32_t kKeyRight = 0xCD;
	inline constexpr std::uint32_t kKeyPageDown = 0xD1;

	// Close keys are armed by a press seen while the reader is open and fire on the matching release, so a release left
	// over from a key pressed before the reader opened can never close it, and the press itself is absorbed by the focus
	// menu (it cannot reach the Tween/Journal handlers after the reader is gone).
	[[nodiscard]] constexpr std::uint8_t CloseKeyBit(std::uint32_t a_scan) noexcept
	{
		switch (a_scan) {
		case kKeyEscape:
			return 0x01u;
		case kKeyTab:
			return 0x02u;
		case kKeyBackspace:
			return 0x04u;
		default:
			return 0u;
		}
	}

	// Keyboard mapping (pressed = initial down transition, released = up transition). Enter and Space are deliberately
	// not mapped: they activate a focused DOM button, and the button already posts its own command.
	[[nodiscard]] constexpr Action MapKeyboard(std::uint32_t a_scan, bool a_pressed, bool a_released, std::uint8_t& a_armedCloseKeys) noexcept
	{
		if (const auto bit = CloseKeyBit(a_scan); bit != 0u) {
			if (a_pressed) {
				a_armedCloseKeys = static_cast<std::uint8_t>(a_armedCloseKeys | bit);
				return Action::kNone;
			}
			if (a_released && (a_armedCloseKeys & bit) != 0u) {
				a_armedCloseKeys = static_cast<std::uint8_t>(a_armedCloseKeys & ~bit);
				return Action::kClose;
			}
			return Action::kNone;
		}
		if (!a_pressed) {
			return Action::kNone;
		}
		switch (a_scan) {
		case kKeyLeft:
		case kKeyA:
		case kKeyPageUp:
			return Action::kPrevious;
		case kKeyRight:
		case kKeyD:
		case kKeyPageDown:
			return Action::kNext;
		default:
			return Action::kNone;
		}
	}

	// Gamepad raw codes (BSWin32GamepadDevice / XInput masks). Raw codes are used so the mapping does not depend on which
	// input context the focus menu leaves active. Gamepad input is consumed by the native sink while the reader is open.
	inline constexpr std::uint32_t kPadDpadUp = 0x0001u;
	inline constexpr std::uint32_t kPadDpadDown = 0x0002u;
	inline constexpr std::uint32_t kPadDpadLeft = 0x0004u;
	inline constexpr std::uint32_t kPadDpadRight = 0x0008u;
	inline constexpr std::uint32_t kPadStart = 0x0010u;
	inline constexpr std::uint32_t kPadBack = 0x0020u;
	inline constexpr std::uint32_t kPadLeftShoulder = 0x0100u;
	inline constexpr std::uint32_t kPadRightShoulder = 0x0200u;
	inline constexpr std::uint32_t kPadA = 0x1000u;
	inline constexpr std::uint32_t kPadB = 0x2000u;

	[[nodiscard]] constexpr Action MapGamepad(std::uint32_t a_code, bool a_pressed) noexcept
	{
		if (!a_pressed) {
			return Action::kNone;
		}
		switch (a_code) {
		case kPadB:
		case kPadBack:
		case kPadStart:
			return Action::kClose;
		case kPadDpadLeft:
		case kPadDpadUp:
		case kPadLeftShoulder:
			return Action::kPrevious;
		case kPadDpadRight:
		case kPadDpadDown:
		case kPadRightShoulder:
		case kPadA:
			return Action::kNext;
		default:
			return Action::kNone;
		}
	}

	// JS commands accepted from the view. reader.ack is the view's confirmation that the document arrived and that the
	// JS -> native route is alive; it carries no action.
	inline constexpr std::string_view kJsNext = "reader.next";
	inline constexpr std::string_view kJsPrevious = "reader.previous";
	inline constexpr std::string_view kJsClose = "reader.close";
	inline constexpr std::string_view kJsAck = "reader.ack";
	// R8.7.4.15 mouse diagnostic: "reader.pointer:<x>,<y>,<target>" posted by the view on mouse-down. Evidence only; it never
	// pages or closes. Bounded per reader session and reduced to a safe character set before it reaches the log.
	inline constexpr std::string_view kJsPointerPrefix = "reader.pointer:";
	inline constexpr std::uint32_t kMaxPointerReports = 8u;
	inline constexpr std::size_t kMaxPointerReportChars = 48u;

	[[nodiscard]] constexpr bool IsPointerReport(std::string_view a_command) noexcept
	{
		return a_command.starts_with(kJsPointerPrefix);
	}

	// Returns the payload with every character outside [A-Za-z0-9,_-] dropped, truncated to kMaxPointerReportChars.
	[[nodiscard]] inline std::string SanitizePointerReport(std::string_view a_command)
	{
		std::string out;
		if (!IsPointerReport(a_command)) {
			return out;
		}
		for (const char c : a_command.substr(kJsPointerPrefix.size())) {
			const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ',' || c == '_' || c == '-';
			if (ok) {
				out.push_back(c);
				if (out.size() >= kMaxPointerReportChars) {
					break;
				}
			}
		}
		return out;
	}

	[[nodiscard]] constexpr bool IsReaderCommand(std::string_view a_command) noexcept
	{
		return a_command.starts_with("reader.");
	}

	[[nodiscard]] constexpr Action ParseJsCommand(std::string_view a_command) noexcept
	{
		if (a_command == kJsNext) {
			return Action::kNext;
		}
		if (a_command == kJsPrevious) {
			return Action::kPrevious;
		}
		if (a_command == kJsClose) {
			return Action::kClose;
		}
		return Action::kNone;
	}

	struct StepResult
	{
		std::size_t page{};
		bool close{};
		bool changed{};
	};

	// Page reducer. Next on the last page closes (the button reads "Close" there); Previous on the first page is a no-op.
	[[nodiscard]] constexpr StepResult Apply(std::size_t a_page, std::size_t a_count, Action a_action) noexcept
	{
		if (a_count == 0u) {
			return { 0u, a_action != Action::kNone, false };
		}
		const std::size_t page = a_page < a_count ? a_page : a_count - 1u;
		switch (a_action) {
		case Action::kClose:
			return { page, true, false };
		case Action::kPrevious:
			return page > 0u ? StepResult{ page - 1u, false, true } : StepResult{ page, false, false };
		case Action::kNext:
			return page + 1u < a_count ? StepResult{ page + 1u, false, true } : StepResult{ page, true, false };
		case Action::kNone:
		default:
			return { page, false, false };
		}
	}

	// Focus ownership. The reader requests focus until it is confirmed; once confirmed, a loss of focus closes the reader
	// (the player or another UI took input away, so a visible reader would be stranded). If focus is never confirmed
	// within kMaxFocusRequests service passes, the reader closes rather than staying visible without input.
	inline constexpr unsigned kMaxFocusRequests = 12u;

	enum class FocusDecision : std::uint8_t
	{
		kHold,
		kConfirm,
		kRequest,
		kCloseLost,
		kCloseUnavailable,
	};

	[[nodiscard]] constexpr FocusDecision DecideFocus(bool a_focusConfirmed, bool a_hasFocus, unsigned a_requestsMade) noexcept
	{
		if (a_hasFocus) {
			return a_focusConfirmed ? FocusDecision::kHold : FocusDecision::kConfirm;
		}
		if (a_focusConfirmed) {
			return FocusDecision::kCloseLost;
		}
		return a_requestsMade < kMaxFocusRequests ? FocusDecision::kRequest : FocusDecision::kCloseUnavailable;
	}

	// The focus menu closing while the reader holds confirmed focus is an exit, whatever closed it.
	[[nodiscard]] constexpr bool ShouldCloseOnMenuClose(std::string_view a_menuName, bool a_opening, bool a_readerOpen, bool a_focusConfirmed) noexcept
	{
		return !a_opening && a_readerOpen && a_focusConfirmed && a_menuName == kPrismaFocusMenuName;
	}

	// Mailbox bound: JS commands beyond this many unserviced entries are dropped (a stuck route must not grow memory).
	inline constexpr std::size_t kMaxPendingActions = 16u;
}
