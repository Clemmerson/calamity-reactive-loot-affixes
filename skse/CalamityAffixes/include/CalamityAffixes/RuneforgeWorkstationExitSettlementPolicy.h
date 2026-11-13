#pragma once

// FE-ASR RC1-WS r3.4 (2026-10-20).
// Pure, host-testable policy for the asynchronous Skyrim furniture-exit lifecycle.
// The Runeforge may not declare a clean IDLE merely because StopInteractingQuick(false) was issued.
// A 2.5 s soft timeout records/reissues a slow real-world exit while observation continues; clean
// settlement still requires the exact Runeforge furniture to be unoccupied, kNormal sit state, and
// naturally restored furniture-suppressed controls. A separate 10 s hard bound exists only so the
// runtime can recover Runeforge-owned presentation/control state without falsely claiming settlement.

#include <cstdint>

namespace CalamityAffixes::RuneforgeWorkstation::ExitSettlementPolicy
{
	inline constexpr std::uint32_t kSitNormal = 0u;
	inline constexpr std::int64_t kStableMs = 100;
	inline constexpr std::int64_t kReissueMs = 1000;
	inline constexpr std::int64_t kSoftTimeoutMs = 2500;
	inline constexpr std::int64_t kHardTimeoutMs = 10000;

	enum class Action : std::uint8_t
	{
		kNone,
		kReissueStop,
		kSoftTimeout,
		kSettled,
		kHardTimeout
	};

	struct Tracker
	{
		bool active{ false };
		bool stopIssued{ false };
		bool reissued{ false };
		bool softTimedOut{ false };
		std::int64_t elapsedMs{ 0 };
		std::int64_t stableMs{ 0 };
	};

	struct Observation
	{
		bool stanceOccupied{ false };
		std::uint32_t sitState{ kSitNormal };
		std::uint32_t controls{ 0 };
		std::uint32_t furnitureSuppressed{ 0 };
	};

	[[nodiscard]] constexpr bool FurnitureControlsRestored(const Observation& a_obs) noexcept
	{
		return (a_obs.controls & a_obs.furnitureSuppressed) == a_obs.furnitureSuppressed;
	}

	[[nodiscard]] constexpr bool Clean(const Observation& a_obs) noexcept
	{
		return !a_obs.stanceOccupied && a_obs.sitState == kSitNormal && FurnitureControlsRestored(a_obs);
	}

	inline constexpr void Start(Tracker& a_tracker) noexcept
	{
		a_tracker = {};
		a_tracker.active = true;
		a_tracker.stopIssued = true;
	}

	[[nodiscard]] constexpr Action Next(Tracker& a_tracker, const Observation& a_obs, std::int64_t a_deltaMs) noexcept
	{
		if (!a_tracker.active) return Action::kNone;
		if (a_deltaMs < 0) a_deltaMs = 0;
		a_tracker.elapsedMs += a_deltaMs;

		if (Clean(a_obs)) {
			a_tracker.stableMs += a_deltaMs;
			if (a_tracker.stableMs >= kStableMs) {
				a_tracker.active = false;
				return Action::kSettled;
			}
		} else {
			a_tracker.stableMs = 0;
		}

		if (!a_tracker.reissued && a_tracker.elapsedMs >= kReissueMs && a_obs.stanceOccupied) {
			a_tracker.reissued = true;
			return Action::kReissueStop;
		}

		if (a_tracker.elapsedMs >= kHardTimeoutMs) {
			a_tracker.active = false;
			return Action::kHardTimeout;
		}

		if (!a_tracker.softTimedOut && a_tracker.elapsedMs >= kSoftTimeoutMs) {
			a_tracker.softTimedOut = true;
			return Action::kSoftTimeout;
		}
		return Action::kNone;
	}

	// PC1.1 (2026-11-09): the player is in a loaded cell other than the workstation's (`coc`, a scripted teleport or a load while
	// exiting). The exact furniture cannot be occupied from another cell, so observation ends at once: SETTLED when it is not
	// occupied (the engine ended the interaction with the cell change), HARD_TIMEOUT recovery otherwise. Either way the
	// Runeforge-owned camera, HUD and control lease are released now instead of waiting on a cell the player has left.
	[[nodiscard]] constexpr Action LeaveCell(Tracker& a_tracker, const Observation& a_obs) noexcept
	{
		if (!a_tracker.active) return Action::kNone;
		a_tracker.active = false;
		return a_obs.stanceOccupied ? Action::kHardTimeout : Action::kSettled;
	}

	[[nodiscard]] constexpr const char* Name(Action a_action) noexcept
	{
		switch (a_action) {
		case Action::kNone: return "NONE";
		case Action::kReissueStop: return "REISSUE_STOP";
		case Action::kSoftTimeout: return "SOFT_TIMEOUT";
		case Action::kSettled: return "SETTLED";
		case Action::kHardTimeout: return "HARD_TIMEOUT";
		}
		return "UNKNOWN";
	}
}
