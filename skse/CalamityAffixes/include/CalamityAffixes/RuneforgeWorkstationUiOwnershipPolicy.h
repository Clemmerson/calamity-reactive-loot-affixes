#pragma once

#include <cstdint>
#include <string_view>

// Pure policy for Runeforge UI ownership/arbitration.  It deliberately has no RE/SKSE
// dependency so the rules can be unit-tested off-line before the runtime sink uses them.
//
// UI ownership rule: a workstation session never competes with a newly opened foreign
// menu before commit. Cursor Menu is the engine companion automatically opened for
// cursor-using menus and is therefore not foreign ownership. Pre-commit foreign UI yields
// input and ends the workstation presentation. After a successful commit, foreign UI only
// yields workstation input temporarily so RESULT_SUCCESS can survive notification/overlay
// menus and resume when the foreign UI closes; the committed transaction is never replayed
// or rolled back.
namespace CalamityAffixes::RuneforgeWorkstation::UiOwnershipPolicy
{
	enum class CommitState : std::uint8_t
	{
		kPreCommit,
		kPostCommit
	};

	enum class MenuClass : std::uint8_t
	{
		kOwn,
		kCompanion,
		kForeign
	};

	enum class Action : std::uint8_t
	{
		kIgnore,
		kYieldAbortPreCommit,
		kYieldAfterCommit
	};

	struct Decision
	{
		MenuClass menuClass{ MenuClass::kForeign };
		Action action{ Action::kIgnore };
		bool yieldInput{ false };
		bool endWorkstation{ false };
		bool preserveCommittedResult{ false };
	};

	inline constexpr std::string_view kCursorMenu = "Cursor Menu";

	inline constexpr MenuClass Classify(std::string_view a_menuName, std::string_view a_runeforgeMenuName) noexcept
	{
		if (a_menuName == a_runeforgeMenuName) return MenuClass::kOwn;
		if (a_menuName == kCursorMenu) return MenuClass::kCompanion;
		return MenuClass::kForeign;
	}

	inline constexpr Decision ResolveOpen(
		std::string_view a_menuName,
		std::string_view a_runeforgeMenuName,
		bool a_opening,
		bool a_sessionActive,
		CommitState a_commitState) noexcept
	{
		const auto menuClass = Classify(a_menuName, a_runeforgeMenuName);
		if (!a_opening || !a_sessionActive || menuClass != MenuClass::kForeign) {
			return { menuClass, Action::kIgnore, false, false, false };
		}
		if (a_commitState == CommitState::kPostCommit) {
			return { menuClass, Action::kYieldAfterCommit, true, false, true };
		}
		return { menuClass, Action::kYieldAbortPreCommit, true, true, false };
	}

	inline constexpr bool MayConsumeWorkstationInput(
		bool a_sessionActive,
		bool a_exiting,
		bool a_foreignMenuYield) noexcept
	{
		return a_sessionActive && !a_exiting && !a_foreignMenuYield;
	}

	// r3.2's settlement timer is wall-clock driven by the review worker.  A foreign
	// menu may pause actor/furniture animation, so those ticks must not consume the
	// bounded exit budget while that UI owns the interaction.
	inline constexpr bool MayAdvanceExitSettlement(bool a_foreignMenuOpen) noexcept
	{
		return !a_foreignMenuOpen;
	}

	// PC1.1 (2026-11-09, P0 control softlock on `coc` during a session). Live: the console aborted the session pre-commit, the
	// exit waited for the furniture, `coc` loaded another cell and an always-open HUD widget ('BestiaryWidget') opened while the
	// session was still EXITING. The r3.2 rule above held the exit budget for as long as ANY tracked foreign menu existed, so the
	// widget held it forever: camera, HUD and the Runeforge control lease were never restored (movement/look/menus disabled).
	// The hold exists only because a PAUSED game freezes the furniture exit animation, so it now applies only while a tracked
	// foreign menu is still open AND the game is actually paused; it never applies once the player has left the workstation
	// cell (the furniture cannot be occupied from another cell); and it is capped so no menu can hold it indefinitely.
	inline constexpr std::int64_t kExitHoldCapMs = 30000;

	struct ExitHoldInput
	{
		bool trackedForeignMenuOpen{ false };   // a foreign menu seen opening during the session that the UI still reports open
		bool gamePaused{ false };               // RE::UI::GameIsPaused()
		bool leftWorkstationCell{ false };      // the player is in a loaded cell other than the forge cell
		std::int64_t heldMs{ 0 };               // exit-budget time already held this exit
	};

	inline constexpr bool HoldsExitSettlement(const ExitHoldInput& a_in) noexcept
	{
		if (a_in.leftWorkstationCell) return false;
		if (a_in.heldMs >= kExitHoldCapMs) return false;
		return a_in.trackedForeignMenuOpen && a_in.gamePaused;
	}

	// PC1.1: a committed session yields its input to foreign UI and resumes when that UI closes (the event sink). Polled every
	// tick as the bounded backstop: if every tracked menu is gone (a close never observed) the input resumes; if the yield lasts
	// kYieldUnpausedCapMs of UNPAUSED game time (an overlay that never closes) the session ends - the committed result is kept,
	// never replayed or rolled back. Paused time (journal, inventory, console over the result) never counts.
	inline constexpr std::int64_t kYieldUnpausedCapMs = 15000;

	enum class YieldAction : std::uint8_t
	{
		kHold,
		kResume,
		kEndSession
	};

	inline constexpr YieldAction ResolveCommittedYield(bool a_trackedForeignMenuOpen, std::int64_t a_unpausedYieldMs) noexcept
	{
		if (!a_trackedForeignMenuOpen) return YieldAction::kResume;
		if (a_unpausedYieldMs >= kYieldUnpausedCapMs) return YieldAction::kEndSession;
		return YieldAction::kHold;
	}

	inline constexpr const char* Name(YieldAction a_value) noexcept
	{
		switch (a_value) {
		case YieldAction::kHold: return "HOLD";
		case YieldAction::kResume: return "RESUME";
		case YieldAction::kEndSession: return "END_SESSION";
		}
		return "?";
	}

	inline constexpr const char* Name(MenuClass a_value) noexcept
	{
		switch (a_value) {
		case MenuClass::kOwn: return "OWN";
		case MenuClass::kCompanion: return "COMPANION";
		case MenuClass::kForeign: return "FOREIGN";
		}
		return "?";
	}

	inline constexpr const char* Name(Action a_value) noexcept
	{
		switch (a_value) {
		case Action::kIgnore: return "IGNORE";
		case Action::kYieldAbortPreCommit: return "YIELD_ABORT_PRECOMMIT";
		case Action::kYieldAfterCommit: return "YIELD_AFTER_COMMIT";
		}
		return "?";
	}
}
