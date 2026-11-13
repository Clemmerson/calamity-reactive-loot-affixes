#pragma once

// Runeforge workstation session state machine v4 (FE-ASR RC1-WS r2, 2026-10-19; PRES-D27/D28/D29, PRES-D33/D34).
// Pure policy: no game types. The reference model is feasr-core/alpha/workstation/model/workstation_model_v4.mjs;
// tests/runeforge_workstation_policy_tests.cpp replays its generated vectors (tests/data/workstation-vectors.v3.txt)
// and must reproduce every state, token and effect exactly.
//
// v3 (R3 contract section 2): acquisition is fail-closed (ENTERING leaves only with the stage host READY, a verified seat
// and the gameplay-control lease; the bounded window ends in ABORT_STANCE_FAILED), staging requests carry a per-session seq
// that their acknowledgements answer, and the engraving hold commits only over a positively acknowledged rune row.
// v4 (R3.1, r3 attempt 1): no lease before the seat. The acquisition runs PLACE -> SETTLE -> ACTIVATE -> VERIFY_SEAT (native
// SeatPolicy, reported as kAcq), the verified seat requests the lease, kLease is the acquire result and kLeaseVerified its
// verification on the next tick; every release stays pending until the native verifier confirms it on a later tick
// (kRestored), EXITING reaches IDLE only with the staging cleared and the restore verified, and Open is refused while a
// restore is pending.

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace CalamityAffixes::RuneforgeWorkstation::StatePolicy
{
	enum class State : std::uint8_t
	{
		kIdle,
		kEntering,
		kSelectItem,
		kSelectFormula,
		kOperations,
		kConfirm,
		kCommitting,
		kPerforming,
		kResultSuccess,
		kResultFailure,
		kExiting
	};

	enum class Cause : std::uint8_t
	{
		kHit,
		kCombat,
		kDeath,
		kCell,
		kForced,
		kF10,
		kEmergency,
		kRace,
		kPreload,
		kStanceFailed,
		kStanceLost,
		kLeaseFailed
	};

	enum class EventKind : std::uint8_t
	{
		kOpen,
		kAckReady,
		kAckCleared,
		kLease,          // a = 1 lease acquired (layout self-check passed, flags written), 0 refused
		kSeated,         // a = 1 seat verified (occupied furniture + settled sit state), 0 not / no longer seated
		kAcq,            // a = AcqPhase reached by the native seat acquisition (PLACE, SETTLE, ACTIVATE, VERIFY_SEAT)
		kLeaseVerified,  // a = 1 the lease read back on the next tick, 0 not
		kRestored,       // a = 1 the release verified on a later tick, 2 the fail-safe restore applied
		kAckItem,   // a = token, b = seq, c = 1 LOADED (or a deliberate NEUTRAL), 0 FAIL
		kAckRunes,  // a = token, b = seq, c = 1 the whole requested row is staged, 0 not
		kTick,
		kFocusItem,
		kFocusFormula,
		kAccept,
		kHoldBegin,
		kHoldEnd,
		kBack,
		kOps,
		kCommitResult,
		kInterrupt,
		kPostLoad,
		kSeedToken,
		kAckBeat,   // R8.7.4.21: a = token, b = strike index, c = 1 LANDED / 0 FAIL (paces the next beat; never decides anything)
		kAckSeal    // R8.7.4.21: a = token (the host's SEAL presentation finished)
	};

	struct Event
	{
		EventKind kind{ EventKind::kTick };
		std::int64_t a{ 0 };
		std::int64_t b{ 0 };
		Cause cause{ Cause::kForced };
		std::int64_t c{ 0 };
	};

	enum class Shot : std::uint8_t
	{
		kNone,
		kEnter,
		kOverview,
		kItem,
		kRow,
		kConfirm,
		kStrike,
		kResult
	};

	enum class EffectKind : std::uint8_t
	{
		kSnapshot,
		kLeaseAcquire,
		kLeaseRelease,
		kMenuOpen,
		kMenuClose,
		kHudHide,
		kHudRestore,
		kStanceEnter,
		kStanceExit,
		kShot,
		kRestoreEase,
		kRestoreNow,
		kBegin,
		kStageItem,
		kClearItem,
		kStageRunes,
		kClearRunes,
		kItemTimeout,
		kRowTimeout,
		kBeatStrike,
		kBeatBind,
		kBeatSkip,
		kEnd,
		kReset,
		kCommit,
		kDecline,
		kOp,
		kSweep,
		kFlat,
		kRefuse
	};

	enum class Refusal : std::uint8_t
	{
		kAlreadyOpen,
		kNotEligible,
		kNotReady,
		kOp,
		kRowIncomplete,
		kRestorePending
	};

	enum class ExitReason : std::uint8_t
	{
		kNone,
		kDone,
		kCancel,
		kAbort
	};

	// Acquisition phases (PRES-D33/D34): the native seat policy reports kPlace..kVerifySeat; the lease phases follow the seat.
	enum class AcqPhase : std::uint8_t
	{
		kNone,
		kPlace,
		kSettle,
		kActivate,
		kVerifySeat,
		kLease,
		kVerifyLease,
		kDone
	};

	// The verified-release state of the gameplay controls (PRES-D34); it outlives the session on purpose.
	enum class RestoreState : std::uint8_t
	{
		kNone,
		kPending,
		kVerified
	};

	// Acknowledgement state of the latest staging request of one kind (PRES-D29).
	enum class Ack : std::uint8_t
	{
		kNone,
		kPending,
		kOk,
		kBad
	};

	struct Effect
	{
		EffectKind kind{ EffectKind::kSnapshot };
		std::uint32_t token{ 0 };
		std::int64_t a{ 0 };    // StageItem / StageRunes: list index; BeatStrike: rune index
		std::int64_t b{ 0 };    // StageRunes: slot count
		std::int64_t seq{ 0 };  // StageItem / StageRunes / ItemTimeout / RowTimeout: staging seq
		Shot shot{ Shot::kNone };
		Refusal refusal{ Refusal::kAlreadyOpen };
		ExitReason reason{ ExitReason::kNone };
		Cause cause{ Cause::kForced };
	};

	// Values mirror feasr-core/alpha/workstation/data/workstation-performance.v2.json (checked by the vectors' P line).
	struct Timing
	{
		std::int64_t focusSettleMs{ 300 };
		std::int64_t acquisitionTimeoutMs{ 4000 };
		std::int64_t runeAckTimeoutMs{ 3000 };
		std::int64_t holdConfirmMs{ 800 };
		std::int64_t firstStrikeMs{ 600 };
		std::int64_t strikeIntervalMs{ 550 };
		std::int64_t bindAfterLastStrikeMs{ 300 };
		std::int64_t revealAfterBindMs{ 600 };
		std::int64_t resultHoldMs{ 1200 };   // R8.7.4.21 I5: 0 = the result is held until Accept / Back or an interrupt (no timed exit)
		std::int64_t exitWatchdogMs{ 5000 };
		std::uint32_t tokenModulus{ 8388608u };
		std::int64_t maxRunes{ 6 };
		// R8.7.4.21 timing v3 (feasr-core/alpha/rc/r87421/B6-optics/timing-table.json). 0 keeps the v2 schedule the vectors pin.
		std::int64_t beatAckTimeoutMs{ 0 };      // > 0: a beat after a strike waits for that strike's LANDED (the reveal for SEAL|DONE), at most this long past its schedule
		// R8.7.4.22: > beatAckTimeoutMs: while the host has answered every gated beat of this performance, a gate waits up to this long
		// instead (a slow Papyrus VM: about 1.9 s per rune at 30 fps); after the first gate it lets time out, the rest of the performance
		// falls back to beatAckTimeoutMs. 0 = off (RC2). A silent host costs at most one cap plus beatAckTimeoutMs per further gate.
		std::int64_t beatAckWaitCapMs{ 0 };
		std::int64_t strikeIntervalLongMs{ 0 };  // > 0: the strike interval from compressFromRunes runes on
		std::int64_t compressFromRunes{ 0 };
	};

	// R8.7.4.21: the runtime performance (RuneforgeWorkstation.cpp). About 1.5 s per rune (1.1 s from four runes), BIND 1.8 s after the
	// last strike, the reveal 0.9 s after BIND; every beat after a strike waits for the host's LANDED and the reveal for SEAL|DONE, each
	// at most 700 ms past its schedule, so a silent or slow host can delay the forge by a bounded amount and never block it. The commit
	// is already authoritative before PERFORMING: these are presentation pacing values only.
	// R8.7.4.21 I5 (director decision 3): the result is held (resultHoldMs 0) until the player continues or dismisses it (Accept / Back)
	// or the session is interrupted (kInterrupt, kPostLoad), so the spent rune row stays visible; it is never ended by a timer.
	inline constexpr Timing kPerformanceV3 = [] {
		Timing t{};
		t.firstStrikeMs = 600;
		t.strikeIntervalMs = 1500;
		t.bindAfterLastStrikeMs = 1800;
		t.revealAfterBindMs = 900;
		t.resultHoldMs = 0;
		t.beatAckTimeoutMs = 700;
		t.beatAckWaitCapMs = 2600;   // R8.7.4.22 (alpha/rc/r87422/optics/evidence/fps-fit-*.json): 30 fps needs up to 1907 (rune) / 2399 (6-rune SEAL)
		t.strikeIntervalLongMs = 1100;
		t.compressFromRunes = 4;
		return t;
	}();

	struct Session
	{
		State state{ State::kIdle };
		std::uint32_t token{ 0 };
		std::uint32_t lastToken{ 0 };
		ExitReason reason{ ExitReason::kNone };
		Cause cause{ Cause::kForced };
		bool ready{ false };
		bool seated{ false };
		bool leased{ false };
		bool leaseOpen{ false };  // LeaseAcquire emitted and LeaseRelease not yet (kept across IDLE on purpose: I-LEASE)
		AcqPhase acq{ AcqPhase::kNone };
		RestoreState restore{ RestoreState::kNone };  // kept across IDLE on purpose (I-RESTORE)
		bool cleared{ false };  // EXITING: the staging host answered CLEARED, or the watchdog swept
		std::int64_t t{ 0 };
		std::int64_t hold{ -1 };
		std::int64_t pendingItem{ -1 };
		std::int64_t itemSettle{ 0 };
		std::int64_t pendingFormula{ -1 };
		std::int64_t pendingN{ 0 };
		std::int64_t formulaSettle{ 0 };
		std::int64_t runes{ 0 };
		bool stagedItem{ false };
		bool stagedRunes{ false };
		std::int64_t beat{ 0 };
		bool bound{ false };
		std::uint32_t commits{ 0 };
		Shot shot{ Shot::kNone };
		bool menuOpen{ false };
		std::int64_t seq{ 0 };
		std::int64_t itemSeq{ 0 };
		Ack item{ Ack::kNone };
		std::int64_t itemWait{ 0 };
		std::int64_t runeSeq{ 0 };
		Ack row{ Ack::kNone };
		std::int64_t rowWait{ 0 };
		// R8.7.4.21 gated performance (beatAckTimeoutMs > 0): presentation acks of this performance and the gated schedule
		std::int64_t landed{ 0 };      // highest strike the host reported LANDED (or FAIL) for
		bool sealed{ false };          // the host reported SEAL|DONE after the BIND
		std::int64_t lastBeatAt{ 0 };  // t of the latest strike
		std::int64_t boundAt{ 0 };     // t of the BIND
		bool ackLapsed{ false };       // R8.7.4.22: a gate of this performance opened without its ack (the host fell silent): no more cap
	};

	using Effects = std::vector<Effect>;

	[[nodiscard]] constexpr std::uint32_t NextToken(std::uint32_t a_last, const Timing& a_timing = {}) noexcept
	{
		const auto next = a_last + 1u;
		return next >= a_timing.tokenModulus ? 1u : next;
	}

	[[nodiscard]] constexpr std::int64_t StrikeTime(std::int64_t a_index, const Timing& a_timing = {}) noexcept
	{
		return a_timing.firstStrikeMs + a_timing.strikeIntervalMs * (a_index - 1);
	}

	[[nodiscard]] constexpr std::int64_t BindTime(std::int64_t a_runes, const Timing& a_timing = {}) noexcept
	{
		return StrikeTime(a_runes, a_timing) + a_timing.bindAfterLastStrikeMs;
	}

	[[nodiscard]] constexpr std::int64_t RevealTime(std::int64_t a_runes, const Timing& a_timing = {}) noexcept
	{
		return BindTime(a_runes, a_timing) + a_timing.revealAfterBindMs;
	}

	[[nodiscard]] constexpr bool IsStagingState(State a_state) noexcept
	{
		return a_state == State::kSelectItem || a_state == State::kSelectFormula;
	}

	// The player owns the workstation from SELECT_ITEM until EXITING: seated, leased, stage host ready (I-SEAT).
	[[nodiscard]] constexpr bool IsInteractive(State a_state) noexcept
	{
		return a_state != State::kIdle && a_state != State::kEntering && a_state != State::kExiting;
	}

	// Display references may exist (INV-3) only while the session is past ENTERING and before EXITING.
	[[nodiscard]] constexpr bool MayHoldDisplayRefs(State a_state) noexcept
	{
		return IsInteractive(a_state);
	}

	[[nodiscard]] constexpr std::string_view Name(State a_state) noexcept
	{
		switch (a_state) {
		case State::kIdle: return "IDLE";
		case State::kEntering: return "ENTERING";
		case State::kSelectItem: return "SELECT_ITEM";
		case State::kSelectFormula: return "SELECT_FORMULA";
		case State::kOperations: return "OPERATIONS";
		case State::kConfirm: return "CONFIRM";
		case State::kCommitting: return "COMMITTING";
		case State::kPerforming: return "PERFORMING";
		case State::kResultSuccess: return "RESULT_SUCCESS";
		case State::kResultFailure: return "RESULT_FAILURE";
		case State::kExiting: return "EXITING";
		}
		return "?";
	}

	[[nodiscard]] constexpr std::string_view Name(Cause a_cause) noexcept
	{
		switch (a_cause) {
		case Cause::kHit: return "HIT";
		case Cause::kCombat: return "COMBAT";
		case Cause::kDeath: return "DEATH";
		case Cause::kCell: return "CELL";
		case Cause::kForced: return "FORCED";
		case Cause::kF10: return "F10";
		case Cause::kEmergency: return "EMERGENCY";
		case Cause::kRace: return "RACE";
		case Cause::kPreload: return "PRELOAD";
		case Cause::kStanceFailed: return "STANCE_FAILED";
		case Cause::kStanceLost: return "STANCE_LOST";
		case Cause::kLeaseFailed: return "LEASE_FAILED";
		}
		return "FORCED";
	}

	[[nodiscard]] constexpr std::string_view Name(Shot a_shot) noexcept
	{
		switch (a_shot) {
		case Shot::kNone: return "";
		case Shot::kEnter: return "S_ENTER";
		case Shot::kOverview: return "S_OVERVIEW";
		case Shot::kItem: return "S_ITEM";
		case Shot::kRow: return "S_ROW";
		case Shot::kConfirm: return "S_CONFIRM";
		case Shot::kStrike: return "S_STRIKE";
		case Shot::kResult: return "S_RESULT";
		}
		return "";
	}

	[[nodiscard]] constexpr std::string_view Name(Refusal a_refusal) noexcept
	{
		switch (a_refusal) {
		case Refusal::kAlreadyOpen: return "ALREADY_OPEN";
		case Refusal::kNotEligible: return "NOT_ELIGIBLE";
		case Refusal::kNotReady: return "NOT_READY";
		case Refusal::kOp: return "OP";
		case Refusal::kRowIncomplete: return "ROW_INCOMPLETE";
		case Refusal::kRestorePending: return "RESTORE_PENDING";
		}
		return "?";
	}

	[[nodiscard]] constexpr std::string_view Name(AcqPhase a_phase) noexcept
	{
		switch (a_phase) {
		case AcqPhase::kNone: return "NONE";
		case AcqPhase::kPlace: return "PLACE";
		case AcqPhase::kSettle: return "SETTLE";
		case AcqPhase::kActivate: return "ACTIVATE";
		case AcqPhase::kVerifySeat: return "VERIFY_SEAT";
		case AcqPhase::kLease: return "LEASE";
		case AcqPhase::kVerifyLease: return "VERIFY_LEASE";
		case AcqPhase::kDone: return "DONE";
		}
		return "?";
	}

	[[nodiscard]] constexpr bool IsSeatPhase(AcqPhase a_phase) noexcept
	{
		return a_phase == AcqPhase::kPlace || a_phase == AcqPhase::kSettle || a_phase == AcqPhase::kActivate || a_phase == AcqPhase::kVerifySeat;
	}

	// The acquisition edges the native seat policy may report (PRES-D33): activation only after a settled placement; any seat
	// phase may fall back to PLACE (the player drifted off the datum); PLACE/SETTLE may go straight to VERIFY_SEAT when the
	// player already occupies the stance (nothing to activate). Mirrors ACQ_EDGES in workstation_model_v4.mjs.
	[[nodiscard]] constexpr bool AcqEdge(AcqPhase a_from, AcqPhase a_to) noexcept
	{
		using A = AcqPhase;
		return (a_from == A::kPlace && a_to == A::kSettle) || (a_from == A::kSettle && a_to == A::kActivate) ||
		       (a_from == A::kActivate && a_to == A::kVerifySeat) || (a_from == A::kSettle && a_to == A::kPlace) ||
		       (a_from == A::kActivate && a_to == A::kPlace) || (a_from == A::kVerifySeat && a_to == A::kPlace) ||
		       (a_from == A::kVerifySeat && a_to == A::kActivate) || (a_from == A::kPlace && a_to == A::kVerifySeat) ||
		       (a_from == A::kSettle && a_to == A::kVerifySeat);
	}

	[[nodiscard]] constexpr std::string_view Name(Ack a_ack) noexcept
	{
		switch (a_ack) {
		case Ack::kNone: return "NONE";
		case Ack::kPending: return "PENDING";
		case Ack::kOk: return "OK";
		case Ack::kBad: return "BAD";
		}
		return "?";
	}

	// End reason text as carried by FEASR_Runeforge_WS_End: DONE, CANCEL, ABORT_<cause>.
	[[nodiscard]] inline std::string ReasonText(ExitReason a_reason, Cause a_cause)
	{
		switch (a_reason) {
		case ExitReason::kDone: return "DONE";
		case ExitReason::kCancel: return "CANCEL";
		case ExitReason::kAbort: return "ABORT_" + std::string(Name(a_cause));
		case ExitReason::kNone: break;
		}
		return "CANCEL";
	}

	[[nodiscard]] inline std::string Format(const Effect& a_effect)
	{
		const auto tok = std::to_string(a_effect.token);
		const auto seq = std::to_string(a_effect.seq);
		switch (a_effect.kind) {
		case EffectKind::kSnapshot: return "Snapshot";
		case EffectKind::kLeaseAcquire: return "Lease:ACQUIRE:" + tok;
		case EffectKind::kLeaseRelease: return "Lease:RELEASE:" + tok;
		case EffectKind::kMenuOpen: return "Menu:OPEN";
		case EffectKind::kMenuClose: return "Menu:CLOSE";
		case EffectKind::kHudHide: return "Hud:HIDE";
		case EffectKind::kHudRestore: return "Hud:RESTORE";
		case EffectKind::kStanceEnter: return "Stance:ENTER";
		case EffectKind::kStanceExit: return "Stance:EXIT";
		case EffectKind::kShot: return "Shot:" + std::string(Name(a_effect.shot));
		case EffectKind::kRestoreEase: return "Restore:EASE";
		case EffectKind::kRestoreNow: return "Restore:NOW";
		case EffectKind::kBegin: return "Begin:" + tok;
		case EffectKind::kStageItem: return "StageItem:" + tok + ":" + seq + ":" + std::to_string(a_effect.a);
		case EffectKind::kClearItem: return "ClearItem:" + tok;
		case EffectKind::kStageRunes: return "StageRunes:" + tok + ":" + seq + ":" + std::to_string(a_effect.a) + ":" + std::to_string(a_effect.b);
		case EffectKind::kClearRunes: return "ClearRunes:" + tok;
		case EffectKind::kItemTimeout: return "ItemTimeout:" + tok + ":" + seq;
		case EffectKind::kRowTimeout: return "RowTimeout:" + tok + ":" + seq;
		case EffectKind::kBeatStrike: return "Beat:" + tok + ":STRIKE:" + std::to_string(a_effect.a);
		case EffectKind::kBeatBind: return "Beat:" + tok + ":BIND";
		case EffectKind::kBeatSkip: return "Beat:" + tok + ":SKIP";
		case EffectKind::kEnd: return "End:" + tok + ":" + ReasonText(a_effect.reason, a_effect.cause);
		case EffectKind::kReset: return "Reset:0";
		case EffectKind::kCommit: return "Commit";
		case EffectKind::kDecline: return "Decline";
		case EffectKind::kOp: return "Op";
		case EffectKind::kSweep: return "Sweep";
		case EffectKind::kFlat: return "Flat";
		case EffectKind::kRefuse: return "Refuse:" + std::string(Name(a_effect.refusal));
		}
		return "?";
	}

	namespace detail
	{
		inline void Push(Effects& a_fx, EffectKind a_kind, std::uint32_t a_token = 0, std::int64_t a_a = 0, std::int64_t a_b = 0, std::int64_t a_seq = 0)
		{
			Effect effect{};
			effect.kind = a_kind;
			effect.token = a_token;
			effect.a = a_a;
			effect.b = a_b;
			effect.seq = a_seq;
			a_fx.push_back(effect);
		}

		inline void Refuse(Effects& a_fx, Refusal a_refusal)
		{
			Effect effect{};
			effect.kind = EffectKind::kRefuse;
			effect.refusal = a_refusal;
			a_fx.push_back(effect);
		}

		inline void Enter(Session& a_s, State a_state)
		{
			a_s.state = a_state;
			a_s.t = 0;
			a_s.hold = -1;
			if (a_state == State::kSelectItem) {
				a_s.pendingFormula = -1;
				a_s.formulaSettle = 0;
			}
		}

		inline void SetShot(Session& a_s, Shot a_shot, Effects& a_fx)
		{
			if (a_s.shot != a_shot) {
				a_s.shot = a_shot;
				Effect effect{};
				effect.kind = EffectKind::kShot;
				effect.shot = a_shot;
				a_fx.push_back(effect);
			}
		}

		// A release is never trusted until the native verifier confirms it on a later tick (kRestored).
		inline void ReleaseLease(Session& a_s, Effects& a_fx)
		{
			if (a_s.leaseOpen) {
				Push(a_fx, EffectKind::kLeaseRelease, a_s.token);
				a_s.leaseOpen = false;
				a_s.restore = RestoreState::kPending;
			}
		}

		inline void BeginExit(Session& a_s, ExitReason a_reason, Cause a_cause, Effects& a_fx)
		{
			a_s.reason = a_reason;
			a_s.cause = a_cause;
			Effect end{};
			end.kind = EffectKind::kEnd;
			end.token = a_s.token;
			end.reason = a_reason;
			end.cause = a_cause;
			a_fx.push_back(end);
			if (a_s.menuOpen) {
				Push(a_fx, EffectKind::kMenuClose);
				a_s.menuOpen = false;
			}
			Push(a_fx, a_reason == ExitReason::kAbort ? EffectKind::kRestoreNow : EffectKind::kRestoreEase);
			Push(a_fx, EffectKind::kHudRestore);
			Push(a_fx, EffectKind::kStanceExit);
			ReleaseLease(a_s, a_fx);
			a_s.shot = Shot::kNone;
			a_s.cleared = false;
			a_s.pendingItem = -1;
			a_s.pendingFormula = -1;
			Enter(a_s, State::kExiting);
		}

		// leaseOpen and restore are deliberately kept: they belong to the controls, not to the session (I-LEASE, I-RESTORE)
		inline void ToIdle(Session& a_s)
		{
			const auto lastToken = a_s.lastToken;
			const auto commits = a_s.commits;
			const auto menuOpen = a_s.menuOpen;
			const auto leaseOpen = a_s.leaseOpen;
			const auto restore = a_s.restore;
			a_s = Session{};
			a_s.lastToken = lastToken;
			a_s.commits = commits;
			a_s.menuOpen = menuOpen;
			a_s.leaseOpen = leaseOpen;
			a_s.restore = restore;
		}

		inline void MaybeIdle(Session& a_s)
		{
			if (a_s.state == State::kExiting && a_s.cleared && a_s.restore != RestoreState::kPending) ToIdle(a_s);
		}

		inline std::int64_t NextSeq(Session& a_s, const Timing& a_timing)
		{
			a_s.seq = a_s.seq + 1 >= static_cast<std::int64_t>(a_timing.tokenModulus) ? 1 : a_s.seq + 1;
			return a_s.seq;
		}

		inline void FlushItem(Session& a_s, Effects& a_fx, const Timing& a_timing)
		{
			if (a_s.pendingItem >= 0) {
				a_s.itemSeq = NextSeq(a_s, a_timing);
				a_s.item = Ack::kPending;
				a_s.itemWait = 0;
				Push(a_fx, EffectKind::kStageItem, a_s.token, a_s.pendingItem, 0, a_s.itemSeq);
				a_s.stagedItem = true;
				a_s.pendingItem = -1;
				a_s.itemSettle = 0;
				SetShot(a_s, Shot::kItem, a_fx);
			}
		}

		inline void FlushFormula(Session& a_s, Effects& a_fx, const Timing& a_timing)
		{
			if (a_s.pendingFormula >= 0) {
				a_s.runeSeq = NextSeq(a_s, a_timing);
				a_s.row = Ack::kPending;
				a_s.rowWait = 0;
				Push(a_fx, EffectKind::kStageRunes, a_s.token, a_s.pendingFormula, a_s.pendingN, a_s.runeSeq);
				a_s.stagedRunes = true;
				a_s.runes = a_s.pendingN;
				a_s.pendingFormula = -1;
				a_s.formulaSettle = 0;
			}
		}

		inline void ClearRunes(Session& a_s, Effects& a_fx)
		{
			a_s.pendingFormula = -1;
			a_s.formulaSettle = 0;
			if (a_s.stagedRunes) {
				Push(a_fx, EffectKind::kClearRunes, a_s.token);
				a_s.stagedRunes = false;
			}
			a_s.row = Ack::kNone;
			a_s.rowWait = 0;
		}

		inline void MaybeLeaveEntering(Session& a_s, Effects& a_fx)
		{
			if (a_s.ready && a_s.seated && a_s.leased) {
				Enter(a_s, State::kSelectItem);
				SetShot(a_s, Shot::kOverview, a_fx);
			}
		}

		// Staging acknowledgement timeouts (PRES-D29): a row is never assumed staged.
		inline void TickAcks(Session& a_s, std::int64_t a_ms, Effects& a_fx, const Timing& a_timing)
		{
			if (a_s.item == Ack::kPending) {
				a_s.itemWait += a_ms;
				if (a_s.itemWait >= a_timing.runeAckTimeoutMs) {
					a_s.item = Ack::kBad;
					Push(a_fx, EffectKind::kItemTimeout, a_s.token, 0, 0, a_s.itemSeq);
				}
			}
			if (a_s.row == Ack::kPending) {
				a_s.rowWait += a_ms;
				if (a_s.rowWait >= a_timing.runeAckTimeoutMs) {
					a_s.row = Ack::kBad;
					Push(a_fx, EffectKind::kRowTimeout, a_s.token, 0, 0, a_s.runeSeq);
				}
			}
		}

		[[nodiscard]] constexpr std::int64_t IntervalFor(std::int64_t a_runes, const Timing& a_timing) noexcept
		{
			return a_timing.strikeIntervalLongMs > 0 && a_timing.compressFromRunes > 0 && a_runes >= a_timing.compressFromRunes ? a_timing.strikeIntervalLongMs :
			                                                                                                                       a_timing.strikeIntervalMs;
		}

		// A gated beat is due at its schedule once the host acknowledged what it waits for, and at its schedule + the wait otherwise.
		[[nodiscard]] constexpr bool GateOpen(std::int64_t a_now, std::int64_t a_due, bool a_acked, std::int64_t a_wait) noexcept
		{
			return a_now >= a_due && (a_acked || a_now >= a_due + a_wait);
		}

		// R8.7.4.22: the bounded wait of the next gate: the cap while the host has answered every gate so far, else the base timeout.
		[[nodiscard]] constexpr std::int64_t GateWait(const Session& a_s, const Timing& a_timing) noexcept
		{
			return !a_s.ackLapsed && a_timing.beatAckWaitCapMs > a_timing.beatAckTimeoutMs ? a_timing.beatAckWaitCapMs : a_timing.beatAckTimeoutMs;
		}

		// R8.7.4.21 timing v3: the schedule is relative to the previous beat, so a waited-for LANDED shifts what follows it.
		inline void TickPerformingGated(Session& a_s, Effects& a_fx, const Timing& a_timing)
		{
			const auto n = a_s.runes;
			const auto wait = GateWait(a_s, a_timing);
			if (a_s.beat < n) {
				const auto due = a_s.beat == 0 ? a_timing.firstStrikeMs : a_s.lastBeatAt + IntervalFor(n, a_timing);
				const bool acked = a_s.beat == 0 || a_s.landed >= a_s.beat;
				if (GateOpen(a_s.t, due, acked, wait)) {
					a_s.ackLapsed = a_s.ackLapsed || !acked;
					a_s.beat += 1;
					a_s.lastBeatAt = a_s.t;
					Push(a_fx, EffectKind::kBeatStrike, a_s.token, a_s.beat);
				}
				return;
			}
			if (!a_s.bound) {
				if (GateOpen(a_s.t, a_s.lastBeatAt + a_timing.bindAfterLastStrikeMs, a_s.landed >= n, wait)) {
					a_s.ackLapsed = a_s.ackLapsed || a_s.landed < n;
					a_s.bound = true;
					a_s.boundAt = a_s.t;
					a_s.stagedRunes = false;
					a_s.row = Ack::kNone;
					Push(a_fx, EffectKind::kBeatBind, a_s.token);
				}
				return;
			}
			if (GateOpen(a_s.t, a_s.boundAt + a_timing.revealAfterBindMs, a_s.sealed, wait)) {
				Enter(a_s, State::kResultSuccess);
				SetShot(a_s, Shot::kResult, a_fx);
			}
		}

		inline void TickPerforming(Session& a_s, Effects& a_fx, const Timing& a_timing)
		{
			if (a_timing.beatAckTimeoutMs > 0) {
				TickPerformingGated(a_s, a_fx, a_timing);
				return;
			}
			const auto n = a_s.runes;
			while (a_s.beat < n && a_s.t >= StrikeTime(a_s.beat + 1, a_timing)) {
				a_s.beat += 1;
				Push(a_fx, EffectKind::kBeatStrike, a_s.token, a_s.beat);
			}
			if (a_s.beat == n && !a_s.bound && a_s.t >= BindTime(n, a_timing)) {
				a_s.bound = true;
				a_s.stagedRunes = false;
				a_s.row = Ack::kNone;
				Push(a_fx, EffectKind::kBeatBind, a_s.token);
			}
			if (a_s.bound && a_s.t >= RevealTime(n, a_timing)) {
				Enter(a_s, State::kResultSuccess);
				SetShot(a_s, Shot::kResult, a_fx);
			}
		}
	}

	// Is this acknowledgement addressed to the live session (right token and, for staging acks, the latest seq)?
	[[nodiscard]] constexpr bool AckIsCurrent(const Session& a_s, const Event& a_event) noexcept
	{
		if (a_s.token == 0u || a_event.a != static_cast<std::int64_t>(a_s.token)) return false;
		if (a_event.kind == EventKind::kAckItem) return a_event.b == a_s.itemSeq && a_s.item == Ack::kPending;
		if (a_event.kind == EventKind::kAckRunes) return a_event.b == a_s.runeSeq && a_s.row == Ack::kPending;
		return true;
	}

	// One transition. Effects are appended to a_fx in emission order.
	inline void Step(Session& a_s, const Event& a_event, Effects& a_fx, const Timing& a_timing = {})
	{
		using namespace detail;
		if (a_event.kind == EventKind::kSeedToken) {
			if (a_s.state == State::kIdle) {
				a_s.lastToken = static_cast<std::uint32_t>(a_event.a);
			}
			return;
		}
		if (a_event.kind == EventKind::kRestored) {
			// PRES-D34: the native verifier confirmed the release on a later tick (1) or applied the fail-safe (2)
			if (a_s.restore == RestoreState::kPending) {
				a_s.restore = RestoreState::kVerified;
				MaybeIdle(a_s);
			}
			return;
		}
		if (a_event.kind == EventKind::kPostLoad) {
			Push(a_fx, EffectKind::kReset);
			ReleaseLease(a_s, a_fx);
			ToIdle(a_s);
			a_s.menuOpen = false;
			return;
		}
		if (a_event.kind == EventKind::kOpen) {
			if (a_s.state != State::kIdle) {
				Refuse(a_fx, Refusal::kAlreadyOpen);
				return;
			}
			// PRES-D34: never a new session over controls whose restore is not verified yet
			if (a_s.restore == RestoreState::kPending) {
				Refuse(a_fx, Refusal::kRestorePending);
				return;
			}
			if (a_event.a != 1) {
				Push(a_fx, EffectKind::kFlat);
				return;
			}
			a_s.token = NextToken(a_s.lastToken, a_timing);
			a_s.lastToken = a_s.token;
			a_s.ready = false;
			a_s.seated = false;
			a_s.leased = false;
			a_s.acq = AcqPhase::kPlace;
			a_s.restore = RestoreState::kNone;
			a_s.cleared = false;
			a_s.commits = 0;
			a_s.stagedItem = false;
			a_s.stagedRunes = false;
			a_s.runes = 0;
			a_s.seq = 0;
			a_s.itemSeq = 0;
			a_s.item = Ack::kNone;
			a_s.itemWait = 0;
			a_s.runeSeq = 0;
			a_s.row = Ack::kNone;
			a_s.rowWait = 0;
			// PRES-D34: no lease at Open; the verified seat requests it
			Push(a_fx, EffectKind::kSnapshot);
			Push(a_fx, EffectKind::kMenuOpen);
			Push(a_fx, EffectKind::kHudHide);
			Push(a_fx, EffectKind::kBegin, a_s.token);
			Push(a_fx, EffectKind::kStanceEnter);
			a_s.menuOpen = true;
			Enter(a_s, State::kEntering);
			SetShot(a_s, Shot::kEnter, a_fx);
			return;
		}
		if (a_s.state == State::kIdle) {
			return;
		}
		if (a_event.kind == EventKind::kInterrupt) {
			if (a_s.state != State::kExiting) {
				BeginExit(a_s, ExitReason::kAbort, a_event.cause, a_fx);
			}
			return;
		}
		const auto kind = a_event.kind;
		if (a_s.state == State::kExiting) {
			if (kind == EventKind::kAckCleared && a_event.a == a_s.token) {
				a_s.cleared = true;
				MaybeIdle(a_s);
			} else if (kind == EventKind::kTick) {
				a_s.t += a_event.a;
				if (!a_s.cleared && a_s.t >= a_timing.exitWatchdogMs) {
					Push(a_fx, EffectKind::kSweep);
					a_s.cleared = true;
				}
				MaybeIdle(a_s);
			}
			return;
		}
		// PRES-D29: acknowledgements bound to the live token and the latest seq of their kind; anything else is dropped
		if (kind == EventKind::kAckItem) {
			if (a_event.a == a_s.token && a_event.b == a_s.itemSeq && a_s.item == Ack::kPending) a_s.item = a_event.c == 1 ? Ack::kOk : Ack::kBad;
			return;
		}
		if (kind == EventKind::kAckRunes) {
			if (a_event.a == a_s.token && a_event.b == a_s.runeSeq && a_s.row == Ack::kPending) a_s.row = a_event.c == 1 ? Ack::kOk : Ack::kBad;
			return;
		}
		// R8.7.4.21: presentation acks only pace the live performance; a stale token, a strike not yet sent or a SEAL before the BIND is dropped
		if (kind == EventKind::kAckBeat) {
			if (a_s.state == State::kPerforming && a_event.a == a_s.token && a_event.b >= 1 && a_event.b <= a_s.beat) a_s.landed = std::max(a_s.landed, a_event.b);
			return;
		}
		if (kind == EventKind::kAckSeal) {
			if (a_s.state == State::kPerforming && a_event.a == a_s.token && a_s.bound) a_s.sealed = true;
			return;
		}
		if (IsInteractive(a_s.state)) {
			// PRES-D27: the seat is verified for the whole session; losing it aborts
			if (kind == EventKind::kSeated) {
				if (a_event.a != 1) BeginExit(a_s, ExitReason::kAbort, Cause::kStanceLost, a_fx);
				return;
			}
			if (kind == EventKind::kLease || kind == EventKind::kLeaseVerified || kind == EventKind::kAcq) return;
			if (kind == EventKind::kTick) TickAcks(a_s, a_event.a, a_fx, a_timing);
		}
		switch (a_s.state) {
		case State::kEntering:
			if (kind == EventKind::kAckReady && a_event.a == a_s.token) {
				a_s.ready = true;
				MaybeLeaveEntering(a_s, a_fx);
			} else if (kind == EventKind::kAcq) {
				// PRES-D33: the native acquisition phases, in order; activation only after a settled placement
				const auto to = static_cast<AcqPhase>(a_event.a);
				if (IsSeatPhase(a_s.acq) && IsSeatPhase(to) && AcqEdge(a_s.acq, to)) a_s.acq = to;
			} else if (kind == EventKind::kSeated) {
				if (a_event.a == 1) {
					// PRES-D34: the lease follows the verified seat, never the Open
					if (a_s.acq == AcqPhase::kVerifySeat && !a_s.seated) {
						a_s.seated = true;
						a_s.acq = AcqPhase::kLease;
						Push(a_fx, EffectKind::kLeaseAcquire, a_s.token);
						a_s.leaseOpen = true;
					}
				} else if (a_s.seated) {
					BeginExit(a_s, ExitReason::kAbort, Cause::kStanceLost, a_fx);
				}
			} else if (kind == EventKind::kLease) {
				if (a_s.acq == AcqPhase::kLease) {
					if (a_event.a == 1) {
						a_s.acq = AcqPhase::kVerifyLease;
					} else {
						BeginExit(a_s, ExitReason::kAbort, Cause::kLeaseFailed, a_fx);
					}
				}
			} else if (kind == EventKind::kLeaseVerified) {
				if (a_s.acq == AcqPhase::kVerifyLease) {
					if (a_event.a == 1) {
						a_s.leased = true;
						a_s.acq = AcqPhase::kDone;
						MaybeLeaveEntering(a_s, a_fx);
					} else {
						BeginExit(a_s, ExitReason::kAbort, Cause::kLeaseFailed, a_fx);
					}
				}
			} else if (kind == EventKind::kTick) {
				// PRES-D27: a timer never opens the menu; the bounded window ends in a clean abort
				a_s.t += a_event.a;
				if (a_s.t >= a_timing.acquisitionTimeoutMs) BeginExit(a_s, ExitReason::kAbort, Cause::kStanceFailed, a_fx);
			} else if (kind == EventKind::kBack) {
				BeginExit(a_s, ExitReason::kCancel, Cause::kForced, a_fx);
			}
			break;
		case State::kSelectItem:
			if (kind == EventKind::kFocusItem) {
				a_s.pendingItem = a_event.a;
				a_s.itemSettle = 0;
			} else if (kind == EventKind::kTick) {
				if (a_s.pendingItem >= 0) {
					a_s.itemSettle += a_event.a;
					if (a_s.itemSettle >= a_timing.focusSettleMs) {
						FlushItem(a_s, a_fx, a_timing);
					}
				}
			} else if (kind == EventKind::kAccept) {
				if (a_event.a == 1) {
					FlushItem(a_s, a_fx, a_timing);
					Enter(a_s, State::kSelectFormula);
					SetShot(a_s, Shot::kRow, a_fx);
				} else {
					Refuse(a_fx, Refusal::kNotEligible);
				}
			} else if (kind == EventKind::kBack) {
				BeginExit(a_s, ExitReason::kCancel, Cause::kForced, a_fx);
			}
			break;
		case State::kSelectFormula:
			if (kind == EventKind::kFocusFormula) {
				a_s.pendingFormula = a_event.a;
				a_s.pendingN = std::min<std::int64_t>(a_event.b, a_timing.maxRunes);
				a_s.formulaSettle = 0;
			} else if (kind == EventKind::kTick) {
				if (a_s.pendingFormula >= 0) {
					a_s.formulaSettle += a_event.a;
					if (a_s.formulaSettle >= a_timing.focusSettleMs) {
						FlushFormula(a_s, a_fx, a_timing);
					}
				}
			} else if (kind == EventKind::kAccept) {
				if (a_event.a == 1 && (a_s.stagedRunes || a_s.pendingFormula >= 0)) {
					FlushFormula(a_s, a_fx, a_timing);
					Enter(a_s, State::kConfirm);
					SetShot(a_s, Shot::kConfirm, a_fx);
				} else {
					Refuse(a_fx, Refusal::kNotReady);
				}
			} else if (kind == EventKind::kBack) {
				ClearRunes(a_s, a_fx);
				Enter(a_s, State::kSelectItem);
				SetShot(a_s, Shot::kItem, a_fx);
			} else if (kind == EventKind::kOps) {
				ClearRunes(a_s, a_fx);
				Enter(a_s, State::kOperations);
				SetShot(a_s, Shot::kItem, a_fx);
			}
			break;
		case State::kOperations:
			if (kind == EventKind::kAccept) {
				if (a_event.a == 1) {
					Push(a_fx, EffectKind::kOp);
				} else {
					Refuse(a_fx, Refusal::kOp);
				}
			} else if (kind == EventKind::kBack) {
				Enter(a_s, State::kSelectFormula);
				SetShot(a_s, Shot::kRow, a_fx);
			}
			break;
		case State::kConfirm:
			if (kind == EventKind::kHoldBegin) {
				a_s.hold = 0;
			} else if (kind == EventKind::kHoldEnd) {
				a_s.hold = -1;
			} else if (kind == EventKind::kTick) {
				if (a_s.hold >= 0) {
					a_s.hold += a_event.a;
					if (a_s.hold >= a_timing.holdConfirmMs) {
						// PRES-D29: never an apparently valid engraving over an incomplete row
						if (a_s.row == Ack::kOk) {
							Push(a_fx, EffectKind::kCommit);
							Enter(a_s, State::kCommitting);
						} else if (a_s.row == Ack::kPending) {
							a_s.hold = a_timing.holdConfirmMs;
						} else {
							Refuse(a_fx, Refusal::kRowIncomplete);
							a_s.hold = -1;
						}
					}
				}
			} else if (kind == EventKind::kBack) {
				Enter(a_s, State::kSelectFormula);
				SetShot(a_s, Shot::kRow, a_fx);
			}
			break;
		case State::kCommitting:
			if (kind == EventKind::kCommitResult) {
				if (a_event.a == 1) {
					a_s.commits += 1;
					a_s.beat = 0;
					a_s.bound = false;
					a_s.landed = 0;
					a_s.sealed = false;
					a_s.lastBeatAt = 0;
					a_s.boundAt = 0;
					a_s.ackLapsed = false;
					Enter(a_s, State::kPerforming);
					SetShot(a_s, Shot::kStrike, a_fx);
				} else {
					Push(a_fx, EffectKind::kDecline);
					Enter(a_s, State::kResultFailure);
				}
			}
			break;
		case State::kPerforming:
			if (kind == EventKind::kTick) {
				a_s.t += a_event.a;
				TickPerforming(a_s, a_fx, a_timing);
			} else if (kind == EventKind::kAccept || kind == EventKind::kBack) {
				Push(a_fx, EffectKind::kBeatSkip, a_s.token);
				a_s.beat = a_s.runes;
				a_s.bound = true;
				a_s.stagedRunes = false;
				a_s.row = Ack::kNone;
				Enter(a_s, State::kResultSuccess);
				SetShot(a_s, Shot::kResult, a_fx);
			}
			break;
		case State::kResultSuccess:
			if (kind == EventKind::kTick) {
				a_s.t += a_event.a;
				if (a_timing.resultHoldMs > 0 && a_s.t >= a_timing.resultHoldMs) {
					BeginExit(a_s, ExitReason::kDone, Cause::kForced, a_fx);
				}
			} else if (kind == EventKind::kAccept || kind == EventKind::kBack) {
				BeginExit(a_s, ExitReason::kDone, Cause::kForced, a_fx);
			}
			break;
		case State::kResultFailure:
			if (kind == EventKind::kAccept || kind == EventKind::kBack) {
				Enter(a_s, State::kSelectFormula);
				SetShot(a_s, Shot::kRow, a_fx);
			}
			break;
		case State::kExiting:
		case State::kIdle:
			break;
		}
	}
}
