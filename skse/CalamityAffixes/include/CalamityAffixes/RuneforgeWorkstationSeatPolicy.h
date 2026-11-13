#pragma once

// Runeforge workstation seat acquisition v2 (RC1-WS r2, 2026-10-19; PRES-D27, PRES-D33). Pure policy: the native session
// polls it once per frame while ENTERING and performs the action it returns; StatePolicy owns the bounded window
// (acquisitionTimeoutMs) and the abort, and receives the phase as kAcq. r3 attempt 1 activated the stance three times from
// 135.9 units (v1 moved the player only beyond 160) and never seated.
//
// Order: force third person, sheathe a drawn weapon (and wait for it), then PLACE the player on the authored work datum
// (feasr-core/alpha/workstation/data/workstation-stance-datum.v1.json: the stance ref transform, HammerTableMarker's single
// SIT marker at offset 0 / heading 0, entered from BEHIND) whenever the player is outside the tight datum band, SETTLE
// (on the datum, 3D loaded, not moving, on two polls kSettlePollSpacingMs apart, never in the placement frame), ACTIVATE
// the stance (at most kActivateAttempts, kActivateSpacingMs apart, each only from a settled position on the datum), and
// VERIFY_SEAT: SEATED only after the occupied furniture is the stance and the sit state is kIsSitting on two polls at least
// kStableMs apart. Raising a delay is never the fix: the verdict comes from the observed state.

#include <cstdint>
#include <string_view>

namespace CalamityAffixes::RuneforgeWorkstation::SeatPolicy
{
	// Values mirror workstation-stance-datum.v1.json "tolerance"/"attempts" and workstation-performance.v3.json "seat".
	inline constexpr float kPlanarTolerance = 4.0f;
	inline constexpr float kVerticalTolerance = 24.0f;
	inline constexpr float kHeadingToleranceDeg = 10.0f;
	inline constexpr float kStillUnits = 0.5f;
	inline constexpr std::uint32_t kSettledPolls = 2;
	inline constexpr std::int64_t kSettlePollSpacingMs = 50;
	inline constexpr std::uint32_t kPlaceAttempts = 3;
	inline constexpr std::int64_t kPlaceSpacingMs = 500;
	inline constexpr std::uint32_t kActivateAttempts = 3;
	inline constexpr std::int64_t kActivateSpacingMs = 600;
	inline constexpr std::int64_t kNormalOccupiedGraceMs = 1200;
	inline constexpr std::int64_t kStableMs = 100;
	inline constexpr std::int64_t kLostMs = 250;  // a verified seat counts as lost only after this long without it
	// The datum is the stance ref transform: HammerTableMarker's marker offset and heading are zero, so MoveTo(stance) lands on it.
	inline constexpr float kMarkerOffsetX = 0.0f;
	inline constexpr float kMarkerOffsetY = 0.0f;
	inline constexpr float kMarkerOffsetZ = 0.0f;
	inline constexpr float kMarkerHeadingDeg = 0.0f;
	// RE::SIT_SLEEP_STATE (include/RE/A/ActorState.h); static_asserted against CommonLib in the game build.
	inline constexpr std::uint32_t kSitNormal = 0;
	inline constexpr std::uint32_t kSitIsSitting = 3;
	// RE::WEAPON_STATE kSheathed (include/RE/A/ActorState.h); static_asserted against CommonLib in the game build.
	inline constexpr std::uint32_t kWeaponSheathed = 0;
	// PC1.2 readiness gate (2026-11-10). Evidence: every activation that started with a drawn weapon failed (4/4: two
	// first-person, two third-person) and every one without a weapon seated (23/23, eight first-person). The engine's
	// weapon-state flag read "sheathed" 11-17 ms after DrawWeaponMagicHands(false), the placement followed at once while the
	// sheathe animation still ran, the furniture was taken (occupied) but the sit never started (kNormal; no OnSit), and the
	// session ended ABORT_STANCE_FAILED after the grace window. The forced camera switch alone never failed (8/8).
	// After a requested camera switch or sheathe, PLACE waits until the change is OBSERVED complete: not first person, the
	// camera's own state is third person, the weapon state is kSheathed, the behaviour graph is not equipping/unequipping,
	// and (after a sheathe only) kSheatheFloorMs has passed since the request because the weapon-state flag was proven to run
	// ahead of the animation. Two quiet polls kReadyPollSpacingMs apart. Bounded: past kReadyCapMs the acquisition proceeds as
	// before (the StatePolicy acquisition window and the kNormal fail-closed abort still apply), so a camera mod that keeps a
	// custom camera state can never stall entry.
	inline constexpr std::int64_t kSheatheFloorMs = 900;
	inline constexpr std::int64_t kReadyPollSpacingMs = 50;
	inline constexpr std::uint32_t kReadyPolls = 2;
	inline constexpr std::int64_t kReadyCapMs = 2000;

	struct Observation
	{
		std::int64_t nowMs{ 0 };
		bool occupiesStance{ false };      // player->GetOccupiedFurniture() is the stance reference
		std::uint32_t sitState{ 0 };       // player->AsActorState()->GetSitSleepState()
		bool firstPerson{ false };
		bool weaponDrawn{ false };
		float planarToDatum{ 0.0f };       // horizontal distance from the player root to the datum
		float verticalToDatum{ 0.0f };     // player z minus datum z
		float headingErrorDeg{ 0.0f };     // player heading minus datum heading, in [-180, 180]
		float movedSinceLastPoll{ 0.0f };  // how far the player moved since the previous poll
		bool player3DLoaded{ true };
		bool stanceOccupiedByOther{ false };
		// PC1.2 readiness observations (defaults describe a settled third-person player with nothing in transition)
		std::uint32_t weaponState{ kWeaponSheathed };  // raw RE::WEAPON_STATE
		bool graphEquipBusy{ false };                  // behaviour graph IsEquipping || IsUnequipping
		bool thirdPersonActive{ true };                // PlayerCamera currentState is the third-person state
	};

	// The camera switch / sheathe this acquisition requested has been observed complete (never assumed from the request).
	[[nodiscard]] constexpr bool TransitionQuiet(const Observation& a_obs) noexcept
	{
		return !a_obs.firstPerson && a_obs.thirdPersonActive && !a_obs.weaponDrawn && a_obs.weaponState == kWeaponSheathed && !a_obs.graphEquipBusy;
	}

	// The acquisition phase; the values mirror StatePolicy::AcqPhase kPlace..kVerifySeat (1..4).
	enum class Phase : std::uint8_t
	{
		kPlace = 1,
		kSettle = 2,
		kActivate = 3,
		kVerifySeat = 4
	};

	enum class Action : std::uint8_t
	{
		kNone,              // wait this frame
		kForceThirdPerson,
		kSheathe,
		kPlace,             // move and orient the player onto the datum (MoveTo(stance))
		kActivate,
		kReportSeated,      // dispatch SEATED 1 (once)
		kReportUnseated,    // the seat was verified and is gone again: dispatch SEATED 0 (once)
		kAbortNormalOccupied, // exact stance remains occupied with kNormal beyond the grace window; fail closed
		kReady,             // PC1.2: the requested camera switch / sheathe is observed complete (log only; placement follows)
		kReadyCapped        // PC1.2: readiness not observed within kReadyCapMs; proceed as before (log only)
	};

	struct Tracker
	{
		bool started{ false };
		std::int64_t startMs{ 0 };
		Phase phase{ Phase::kPlace };
		bool thirdPersonForced{ false };
		bool sheatheRequested{ false };
		std::uint32_t placeAttempts{ 0 };
		std::int64_t lastPlaceMs{ 0 };
		std::uint32_t settledPolls{ 0 };
		std::int64_t lastSettledPollMs{ 0 };
		std::uint32_t attempts{ 0 };
		std::int64_t lastActivateMs{ 0 };
		std::int64_t normalOccupiedSinceMs{ -1 };
		std::int64_t seatedSinceMs{ -1 };
		std::int64_t unseatedSinceMs{ -1 };
		bool reported{ false };   // SEATED 1 dispatched
		std::uint32_t polls{ 0 };
		// PC1.2 readiness gate
		std::int64_t transitionRequestedMs{ -1 };  // last camera-switch / sheathe request
		std::int64_t sheatheRequestedMs{ -1 };
		bool ready{ false };
		std::uint32_t readyPolls{ 0 };
		std::int64_t lastReadyPollMs{ 0 };
	};

	[[nodiscard]] constexpr float Abs(float a_value) noexcept
	{
		return a_value < 0.0f ? -a_value : a_value;
	}

	// The tight datum band (PRES-D33): never a broad interaction radius.
	[[nodiscard]] constexpr bool OnDatum(const Observation& a_obs) noexcept
	{
		return a_obs.planarToDatum <= kPlanarTolerance && Abs(a_obs.verticalToDatum) <= kVerticalTolerance && Abs(a_obs.headingErrorDeg) <= kHeadingToleranceDeg;
	}

	[[nodiscard]] constexpr bool Settled(const Observation& a_obs) noexcept
	{
		return a_obs.occupiesStance && a_obs.sitState == kSitIsSitting;
	}

	// One poll. Returns the single action to perform this frame; a_t.phase is the acquisition phase after the poll.
	[[nodiscard]] constexpr Action Next(Tracker& a_t, const Observation& a_obs) noexcept
	{
		if (!a_t.started) {
			a_t = Tracker{};
			a_t.started = true;
			a_t.startMs = a_obs.nowMs;
		}
		a_t.polls += 1;
		if (Settled(a_obs)) {
			a_t.phase = Phase::kVerifySeat;
			a_t.unseatedSinceMs = -1;
			if (a_t.seatedSinceMs < 0) a_t.seatedSinceMs = a_obs.nowMs;
			if (!a_t.reported && a_obs.nowMs - a_t.seatedSinceMs >= kStableMs) {
				a_t.reported = true;
				return Action::kReportSeated;
			}
			return Action::kNone;
		}
		a_t.seatedSinceMs = -1;
		if (a_t.reported) {
			if (a_t.unseatedSinceMs < 0) a_t.unseatedSinceMs = a_obs.nowMs;
			if (a_obs.nowMs - a_t.unseatedSinceMs < kLostMs) return Action::kNone;
			a_t.reported = false;
			a_t.unseatedSinceMs = -1;
			return Action::kReportUnseated;
		}
		// Occupancy and sit/sleep state are independent engine observations. A matching furniture handle
		// with kNormal is not a seat and must never advance to SEATED/lease. Do not reactivate: activation
		// may toggle furniture use, and no idempotent retry contract is established. Give normal graph
		// progression a short grace window, then let the native caller fail closed with diagnostics.
		if (a_obs.occupiesStance && a_obs.sitState == kSitNormal) {
			a_t.phase = Phase::kVerifySeat;
			if (a_t.normalOccupiedSinceMs < 0) a_t.normalOccupiedSinceMs = a_obs.nowMs;
			if (a_obs.nowMs - a_t.normalOccupiedSinceMs >= kNormalOccupiedGraceMs) return Action::kAbortNormalOccupied;
			return Action::kNone;
		}
		a_t.normalOccupiedSinceMs = -1;
		// entering or leaving the stance (its animation is running): verify, never interrupt it with a new action
		if (a_obs.occupiesStance) {
			a_t.phase = Phase::kVerifySeat;
			return Action::kNone;
		}
		// using some other furniture: wait (the bounded window decides)
		if (a_obs.sitState != kSitNormal) return Action::kNone;
		if (a_obs.firstPerson && !a_t.thirdPersonForced) {
			a_t.thirdPersonForced = true;
			a_t.transitionRequestedMs = a_obs.nowMs;
			a_t.ready = false;
			a_t.readyPolls = 0;
			return Action::kForceThirdPerson;
		}
		if (a_obs.weaponDrawn) {
			if (!a_t.sheatheRequested) {
				a_t.sheatheRequested = true;
				a_t.transitionRequestedMs = a_obs.nowMs;
				a_t.sheatheRequestedMs = a_obs.nowMs;
				a_t.ready = false;
				a_t.readyPolls = 0;
				return Action::kSheathe;
			}
			return Action::kNone;  // wait for the sheathe animation
		}
		// PC1.2: a requested camera switch or sheathe must be observed complete before the player is placed
		if (a_t.transitionRequestedMs >= 0 && !a_t.ready) {
			const bool floorMet = a_t.sheatheRequestedMs < 0 || a_obs.nowMs - a_t.sheatheRequestedMs >= kSheatheFloorMs;
			if (TransitionQuiet(a_obs) && floorMet) {
				if (a_t.readyPolls == 0 || a_obs.nowMs - a_t.lastReadyPollMs >= kReadyPollSpacingMs) {
					a_t.readyPolls += 1;
					a_t.lastReadyPollMs = a_obs.nowMs;
				}
				if (a_t.readyPolls >= kReadyPolls) {
					a_t.ready = true;
					return Action::kReady;
				}
			} else {
				a_t.readyPolls = 0;
			}
			if (a_obs.nowMs - a_t.transitionRequestedMs >= kReadyCapMs) {
				a_t.ready = true;
				return Action::kReadyCapped;
			}
			return Action::kNone;
		}
		const bool on = OnDatum(a_obs);
		switch (a_t.phase) {
		case Phase::kPlace:
			if (on) {
				a_t.phase = Phase::kSettle;
				a_t.settledPolls = 0;
				return Action::kNone;
			}
			if (a_t.placeAttempts < kPlaceAttempts && (a_t.placeAttempts == 0 || a_obs.nowMs - a_t.lastPlaceMs >= kPlaceSpacingMs)) {
				a_t.placeAttempts += 1;
				a_t.lastPlaceMs = a_obs.nowMs;
				a_t.phase = Phase::kSettle;
				a_t.settledPolls = 0;
				return Action::kPlace;
			}
			return Action::kNone;
		case Phase::kSettle:
			if (!on) {
				// the move may not have applied yet; past the spacing the player is off the datum: place again
				if (a_t.placeAttempts > 0 && a_obs.nowMs - a_t.lastPlaceMs < kPlaceSpacingMs) return Action::kNone;
				a_t.phase = Phase::kPlace;
				a_t.settledPolls = 0;
				return Action::kNone;
			}
			// never in the placement frame; 3D loaded and still
			if ((a_t.placeAttempts > 0 && a_obs.nowMs <= a_t.lastPlaceMs) || !a_obs.player3DLoaded || a_obs.movedSinceLastPoll > kStillUnits) {
				a_t.settledPolls = 0;
				return Action::kNone;
			}
			if (a_t.settledPolls == 0 || a_obs.nowMs - a_t.lastSettledPollMs >= kSettlePollSpacingMs) {
				a_t.settledPolls += 1;
				a_t.lastSettledPollMs = a_obs.nowMs;
			}
			if (a_t.settledPolls >= kSettledPolls) a_t.phase = Phase::kActivate;
			return Action::kNone;
		case Phase::kActivate:
			if (!on) {
				a_t.phase = Phase::kPlace;
				a_t.settledPolls = 0;
				return Action::kNone;
			}
			if (a_obs.stanceOccupiedByOther) return Action::kNone;
			if (a_t.attempts < kActivateAttempts && (a_t.attempts == 0 || a_obs.nowMs - a_t.lastActivateMs >= kActivateSpacingMs)) {
				a_t.attempts += 1;
				a_t.lastActivateMs = a_obs.nowMs;
				a_t.phase = Phase::kVerifySeat;
				return Action::kActivate;
			}
			return Action::kNone;
		case Phase::kVerifySeat:
			// the activation did not take within the spacing: another settled activation from the datum, or place again
			if (a_obs.nowMs - a_t.lastActivateMs >= kActivateSpacingMs) {
				a_t.phase = on ? Phase::kActivate : Phase::kPlace;
				a_t.settledPolls = 0;
			}
			return Action::kNone;
		}
		return Action::kNone;
	}

	[[nodiscard]] constexpr std::string_view Name(Action a_action) noexcept
	{
		switch (a_action) {
		case Action::kNone: return "wait";
		case Action::kForceThirdPerson: return "force-third-person";
		case Action::kSheathe: return "sheathe";
		case Action::kPlace: return "place-on-datum";
		case Action::kActivate: return "activate";
		case Action::kReportSeated: return "seated";
		case Action::kReportUnseated: return "unseated";
		case Action::kAbortNormalOccupied: return "abort-occupied-normal";
		case Action::kReady: return "ready";
		case Action::kReadyCapped: return "ready-capped";
		}
		return "?";
	}

	[[nodiscard]] constexpr std::string_view Name(Phase a_phase) noexcept
	{
		switch (a_phase) {
		case Phase::kPlace: return "PLACE";
		case Phase::kSettle: return "SETTLE";
		case Phase::kActivate: return "ACTIVATE";
		case Phase::kVerifySeat: return "VERIFY_SEAT";
		}
		return "?";
	}

	[[nodiscard]] constexpr std::string_view SitName(std::uint32_t a_state) noexcept
	{
		switch (a_state) {
		case 0: return "kNormal";
		case 1: return "kWantToSit";
		case 2: return "kWaitingForSitAnim";
		case 3: return "kIsSitting";
		case 4: return "kWantToStand";
		case 5: return "kWantToSleep";
		case 6: return "kWaitingForSleepAnim";
		case 7: return "kIsSleeping";
		case 8: return "kWantToWake";
		}
		return "kUnknown";
	}

	// Heading difference in degrees, normalised to [-180, 180].
	[[nodiscard]] constexpr float HeadingErrorDeg(float a_playerDeg, float a_datumDeg) noexcept
	{
		float d = a_playerDeg - a_datumDeg;
		while (d > 180.0f) d -= 360.0f;
		while (d < -180.0f) d += 360.0f;
		return d;
	}
}
