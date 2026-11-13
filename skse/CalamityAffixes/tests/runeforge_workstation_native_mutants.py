#!/usr/bin/env python3
"""Native mutation check of the Runeforge workstation policies (RC1-WS r2, R3.1; PRES-D27/D28/D29, PRES-D33/D34; R8.7.4.18 prompt
and framing policies N41-N52; R8.7.4.21 sweep / cradle N53-N60 and timing v3 gates N61-N69).

Each mutant is one textual change to a temporary copy of a production policy header (StatePolicy v4, LeasePolicy, SeatPolicy v2,
ControlsPolicy). The host test (tests/runeforge_workstation_policy_tests.cpp + tests/data/workstation-vectors.v3.txt) must fail
for every mutant and pass for the unmutated control. Nothing in the source tree is written.
R8.7.4.21 I3 adds N70-N83 (RunewordEffectCardPolicy, Details parity, and three source mutants in src/ that the wiring test
tests/runeforge_effect_card_static_test.py must kill; every build also runs that static test on its temporary tree).
R8.7.4.21 I5 adds N84-N86 (decision 3: the held result) and --only <comma ids> (a subset; the control always runs).
Usage: python3 tests/runeforge_workstation_native_mutants.py [--cxx /usr/lib/llvm-19/bin/clang++] [--json out.json] [--only N84,N85]
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]  # skse/CalamityAffixes
INC = ROOT / "include" / "CalamityAffixes"
TEST = ROOT / "tests" / "runeforge_workstation_policy_tests.cpp"
VECTORS = ROOT / "tests" / "data" / "workstation-vectors.v3.txt"
args = sys.argv[1:]
CXX = args[args.index("--cxx") + 1] if "--cxx" in args else os.environ.get("CXX", "c++")
OUT = args[args.index("--json") + 1] if "--json" in args else None
ONLY = set(args[args.index("--only") + 1].split(",")) if "--only" in args else None

S = "RuneforgeWorkstationStatePolicy.h"
L = "RuneforgeWorkstationLeasePolicy.h"
E = "RuneforgeWorkstationSeatPolicy.h"
C = "RuneforgeWorkstationControlsPolicy.h"
PR = "RuneforgeWorkstationPromptPolicy.h"
FR = "RuneforgeWorkstationFramingPolicy.h"
SW = "RuneforgeWorkstationSweepPolicy.h"
LY = "RuneforgeWorkstationLayoutPolicy.h"
EC = "RunewordEffectCardPolicy.h"
IN = "RuneforgeWorkstationInputPolicy.h"
STATIC = ROOT / "tests" / "runeforge_effect_card_static_test.py"
STATIC_INPUTS = ("src/EventBridge.Loot.Runeword.RecipeEntries.cpp", "src/EventBridge.Loot.TooltipResolution.cpp", "src/RuneforgeMenu.cpp",
                 "src/RuneforgeWorkstation.cpp", "ui/runeforge/as2/src/RuneforgeWorkstationView.as", "ui/runeforge/as2/src/RuneforgeMenu.as")
RELEASE_IN_EXIT = "\t\t\tReleaseLease(a_s, a_fx);\n\t\t\ta_s.shot = Shot::kNone;"
MUTANTS = [
    # ---- StatePolicy v4
    ("N01-timeout-opens-menu", S,
     "if (a_s.t >= a_timing.acquisitionTimeoutMs) BeginExit(a_s, ExitReason::kAbort, Cause::kStanceFailed, a_fx);",
     "if (a_s.t >= a_timing.acquisitionTimeoutMs) { Enter(a_s, State::kSelectItem); SetShot(a_s, Shot::kOverview, a_fx); }"),
    ("N02-no-release-on-F10", S, RELEASE_IN_EXIT,
     "\t\t\tif (!(a_reason == ExitReason::kAbort && a_cause == Cause::kF10)) ReleaseLease(a_s, a_fx);\n\t\t\ta_s.shot = Shot::kNone;"),
    ("N03-no-release-on-preload", S, RELEASE_IN_EXIT,
     "\t\t\tif (!(a_reason == ExitReason::kAbort && a_cause == Cause::kPreload)) ReleaseLease(a_s, a_fx);\n\t\t\ta_s.shot = Shot::kNone;"),
    ("N04-no-release-on-lease-failure", S, RELEASE_IN_EXIT,
     "\t\t\tif (!(a_reason == ExitReason::kAbort && a_cause == Cause::kLeaseFailed)) ReleaseLease(a_s, a_fx);\n\t\t\ta_s.shot = Shot::kNone;"),
    ("N05-double-release", S, RELEASE_IN_EXIT,
     "\t\t\tReleaseLease(a_s, a_fx);\n\t\t\tPush(a_fx, EffectKind::kLeaseRelease, a_s.token);\n\t\t\ta_s.shot = Shot::kNone;"),
    ("N06-commit-with-pending-row", S,
     "} else if (a_s.row == Ack::kPending) {\n\t\t\t\t\t\t\ta_s.hold = a_timing.holdConfirmMs;",
     "} else if (a_s.row == Ack::kPending) {\n\t\t\t\t\t\t\tPush(a_fx, EffectKind::kCommit);\n\t\t\t\t\t\t\tEnter(a_s, State::kCommitting);"),
    ("N07-commit-with-bad-row", S,
     "\t\t\t\t\t\t\tRefuse(a_fx, Refusal::kRowIncomplete);\n\t\t\t\t\t\t\ta_s.hold = -1;",
     "\t\t\t\t\t\t\tPush(a_fx, EffectKind::kCommit);\n\t\t\t\t\t\t\tEnter(a_s, State::kCommitting);"),
    ("N08-stale-seq-accepted", S,
     "if (a_event.a == a_s.token && a_event.b == a_s.runeSeq && a_s.row == Ack::kPending)",
     "if (a_event.a == a_s.token && a_s.row == Ack::kPending)"),
    ("N09-stale-token-accepted", S,
     "if (a_event.a == a_s.token && a_event.b == a_s.runeSeq && a_s.row == Ack::kPending)",
     "if (a_event.b == a_s.runeSeq && a_s.row == Ack::kPending)"),
    ("N10-lease-refusal-ignored", S,
     "a_s.acq = AcqPhase::kVerifyLease;\n\t\t\t\t\t} else {\n\t\t\t\t\t\tBeginExit(a_s, ExitReason::kAbort, Cause::kLeaseFailed, a_fx);",
     "a_s.acq = AcqPhase::kVerifyLease;\n\t\t\t\t\t} else {\n\t\t\t\t\t\t(void)0;"),
    ("N11-seat-loss-ignored", S,
     "if (a_event.a != 1) BeginExit(a_s, ExitReason::kAbort, Cause::kStanceLost, a_fx);",
     "(void)0;"),
    ("N12-lease-not-required", S,
     "if (a_s.ready && a_s.seated && a_s.leased) {", "if (a_s.ready && a_s.seated) {"),
    ("N13-no-release-on-postload", S,
     "\t\t\tPush(a_fx, EffectKind::kReset);\n\t\t\tReleaseLease(a_s, a_fx);", "\t\t\tPush(a_fx, EffectKind::kReset);"),
    ("N20-lease-at-open", S,
     "\t\t\tPush(a_fx, EffectKind::kSnapshot);\n\t\t\tPush(a_fx, EffectKind::kMenuOpen);",
     "\t\t\tPush(a_fx, EffectKind::kSnapshot);\n\t\t\tPush(a_fx, EffectKind::kLeaseAcquire, a_s.token);\n\t\t\ta_s.leaseOpen = true;\n\t\t\tPush(a_fx, EffectKind::kMenuOpen);"),
    ("N21-lease-before-seat", S,
     "if (IsSeatPhase(a_s.acq) && IsSeatPhase(to) && AcqEdge(a_s.acq, to)) a_s.acq = to;",
     "if (IsSeatPhase(a_s.acq) && IsSeatPhase(to) && AcqEdge(a_s.acq, to)) a_s.acq = to;\n\t\t\t\tif (to == AcqPhase::kActivate && !a_s.leaseOpen) { Push(a_fx, EffectKind::kLeaseAcquire, a_s.token); a_s.leaseOpen = true; }"),
    ("N22-restore-not-verified", S,
     "if (a_s.state == State::kExiting && a_s.cleared && a_s.restore != RestoreState::kPending) ToIdle(a_s);",
     "if (a_s.state == State::kExiting && a_s.cleared) ToIdle(a_s);"),
    ("N23-lease-verify-skipped", S,
     "\t\t\t\t\t\ta_s.acq = AcqPhase::kVerifyLease;",
     "\t\t\t\t\t\ta_s.acq = AcqPhase::kDone;\n\t\t\t\t\t\ta_s.leased = true;\n\t\t\t\t\t\tMaybeLeaveEntering(a_s, a_fx);"),
    ("N24-activation-without-placement-edge", S,
     "return (a_from == A::kPlace && a_to == A::kSettle) ||",
     "return (a_from == A::kPlace && a_to == A::kActivate) || (a_from == A::kPlace && a_to == A::kSettle) ||"),
    ("N25-open-over-pending-restore", S,
     "\t\t\tif (a_s.restore == RestoreState::kPending) {\n\t\t\t\tRefuse(a_fx, Refusal::kRestorePending);\n\t\t\t\treturn;\n\t\t\t}\n", ""),
    ("N26-release-not-pending", S,
     "\t\t\t\ta_s.leaseOpen = false;\n\t\t\t\ta_s.restore = RestoreState::kPending;", "\t\t\t\ta_s.leaseOpen = false;"),
    ("N27-seat-lost-while-leasing-ignored", S,
     "} else if (a_s.seated) {\n\t\t\t\t\tBeginExit(a_s, ExitReason::kAbort, Cause::kStanceLost, a_fx);\n\t\t\t\t}",
     "} else if (a_s.seated) {\n\t\t\t\t}"),
    ("N28-watchdog-ends-unverified", S,
     "\t\t\t\t\ta_s.cleared = true;\n\t\t\t\t}\n\t\t\t\tMaybeIdle(a_s);",
     "\t\t\t\t\ta_s.cleared = true;\n\t\t\t\t\tToIdle(a_s);\n\t\t\t\t}\n\t\t\t\tMaybeIdle(a_s);"),
    # ---- LeasePolicy
    ("N14-lease-double-release-effective", L,
     "if (!a_lease.held || (a_token != 0u && a_token != a_lease.token)) return {};",
     "if (a_token != 0u && a_token != a_lease.token) return {};"),
    ("N15-lease-leaks-fighting", L,
     "kMovement | kLooking | kActivate | kMenu | kPOVSwitch | kFighting | kSneaking",
     "kMovement | kLooking | kActivate | kMenu | kPOVSwitch | kSneaking"),
    ("N16-lease-leaks-movement", L,
     "kMovement | kLooking | kActivate | kMenu | kPOVSwitch | kFighting | kSneaking",
     "kLooking | kActivate | kMenu | kPOVSwitch | kFighting | kSneaking"),
    # ---- SeatPolicy v2 (PRES-D33)
    ("N17-settle-in-place-frame", E,
     "if ((a_t.placeAttempts > 0 && a_obs.nowMs <= a_t.lastPlaceMs) || !a_obs.player3DLoaded || a_obs.movedSinceLastPoll > kStillUnits) {",
     "if (!a_obs.player3DLoaded) {"),
    ("N18-seated-without-stability", E,
     "if (!a_t.reported && a_obs.nowMs - a_t.seatedSinceMs >= kStableMs) {", "if (!a_t.reported) {"),
    ("N19-seat-reported-in-other-furniture", E,
     "return a_obs.occupiesStance && a_obs.sitState == kSitIsSitting;", "return a_obs.sitState == kSitIsSitting;"),
    ("N29-threshold-only-placement", E,
     "return a_obs.planarToDatum <= kPlanarTolerance &&", "return a_obs.planarToDatum <= 160.0f &&"),
    ("N30-activation-without-placement", E,
     "\t\tcase Phase::kActivate:\n\t\t\tif (!on) {", "\t\tcase Phase::kActivate:\n\t\t\tif (false) {"),
    ("N31-settle-skipped", E,
     "\t\t\tif (on) {\n\t\t\t\ta_t.phase = Phase::kSettle;", "\t\t\tif (on) {\n\t\t\t\ta_t.phase = Phase::kActivate;"),
    ("N32-seat-loss-not-reported", E,
     "\t\t\ta_t.reported = false;\n\t\t\ta_t.unseatedSinceMs = -1;\n\t\t\treturn Action::kReportUnseated;",
     "\t\t\treturn Action::kNone;"),
    # ---- ControlsPolicy (PRES-D34, R3-E11)
    ("N33-wrong-layout-accepted", C,
     'inline constexpr Layout kLayoutAE{ "AE", 0x108, 0x120, 0x124, 18, 0x130 };',
     'inline constexpr Layout kLayoutAE{ "AE", 0x100, 0x118, 0x11C, 17, 0x128 };'),
    ("N34-selfcheck-ignores-restricted-flags", C,
     "if (a_atAcquire && (a_raw.flags & kRequiredAtAcquire) != kRequiredAtAcquire) f |= kFaultRestricted;", "(void)a_atAcquire;"),
    ("N35-selfcheck-ignores-stack-pointer", C,
     "if (!PlausiblePointer(a_raw.stackData)) f |= kFaultStackData;", "(void)0;"),
    ("N36-restore-verified-in-release-tick", C,
     "if (a_frame <= a_r.frame) return Step::kWait;", "(void)a_frame;"),
    ("N37-no-repair", C,
     "\t\tif (a_r.stage == 0) {\n\t\t\ta_r.stage = 1;\n\t\t\treturn Step::kRepair;\n\t\t}\n", ""),
    ("N38-companion-ignored", C,
     "if (a_words.companion != kInvalid) out.companion = a_enable ? (a_words.companion | a_mask) : (a_words.companion & ~a_mask);", "(void)0;"),
    ("N39-empty-stack-accepted", C,
     "if (a_raw.stackSize == 0 || a_raw.stackSize > a_layout.contexts) f |= kFaultStackSize;",
     "if (a_raw.stackSize > a_layout.contexts) f |= kFaultStackSize;"),
    ("N40-layout-by-ae-flag", C,
     "inline constexpr std::uint32_t kShiftPatch = 1130;", "inline constexpr std::uint32_t kShiftPatch = 317;"),
    # ---- R8.7.4.18 PromptPolicy (truthful device prompts, footer parity)
    ("N41-mouse-move-flips-device", PR,
     "\t\tcase InputKind::kMouseMove:\n\t\tcase InputKind::kUiPointer:\n\t\tcase InputKind::kUiHover:\n\t\t\treturn a_current;",
     "\t\tcase InputKind::kMouseMove:\n\t\tcase InputKind::kUiPointer:\n\t\tcase InputKind::kUiHover:\n\t\t\treturn Device::kMouse;"),
    ("N42-stick-drift-flips-device", PR,
     "return a_stickMagnitude >= kStickDeadZone ? Device::kGamepad : a_current;", "return Device::kGamepad;"),
    ("N43-mouse-erases-key-caps", PR,
     "return a_device == Device::kMouse ? a_promptDevice : a_device;", "return a_device;"),
    ("N44-hover-is-a-click", PR,
     "a_name == \"pageDown\" || a_name == \"focus\";", "a_name == \"pageDown\" || a_name == \"focus\" || a_name == \"preview\";"),
    ("N45-committing-skip-restored", PR,
     "\t\tcase State::kCommitting:\n\t\t\t// R8.7.4.18", "\t\tcase State::kCommitting:\n\t\t\tadd(\"skip\", Button::kAccept, \"Skip\");\n\t\t\t// R8.7.4.18"),
    ("N46-unbound-page-invents-key", PR,
     "\t\tif (a_button == Button::kCancel) return \"ESC\";     // ButtonForInputEvent: Esc and Tab are fixed Back\n\t\treturn std::string(kUnset);",
     "\t\tif (a_button == Button::kCancel) return \"ESC\";     // ButtonForInputEvent: Esc and Tab are fixed Back\n\t\treturn \"F\";"),
    ("N47-playstation-shoulder-as-xbox", PR,
     "case kPadLeftShoulder: return ps ? \"L1\" : \"LB\";", "case kPadLeftShoulder: return \"LB\";"),
    # ---- R8.7.4.18 FramingPolicy (guarded furniture-camera framing)
    ("N48-no-effect-not-failed-closed", FR,
     "if (std::fabs(a_measuredDeltaDegrees) < kNoEffectShare * std::fabs(a_commandedDegrees)) return SignVerdict::kNoEffect;", "(void)0;"),
    ("N49-zoom-unbounded", FR,
     "const float hi = a_baseZoom + kMaxZoomDelta;", "const float hi = a_baseZoom + 10.0f;"),
    ("N50-pitch-override-unclamped", FR,
     "return { std::clamp(a_target.pitchDegrees, 0.0f, kMaxPitchDegrees), std::clamp(a_target.pullBack, 0.0f, kMaxPullBack) };",
     "return a_target;"),
    ("N51-framing-while-exiting", FR,
     "\t\tcase State::kResultFailure:\n\t\t\treturn true;\n\t\tdefault:\n\t\t\treturn false;",
     "\t\tcase State::kResultFailure:\n\t\t\treturn true;\n\t\tdefault:\n\t\t\treturn true;"),
    ("N52-sign-judged-before-settle", FR,
     "inline constexpr float kJudgeAfterSeconds = kRampSeconds + 0.35f;", "inline constexpr float kJudgeAfterSeconds = 0.15f;"),
    # ---- R8.7.4.21 SweepPolicy / LayoutPolicy cradle (A3 G1/G2/G3)
    ("N53-plugin-refs-deleted", SW,
     "return a_dynamic ? Action::kDisableAndDelete : Action::kDisable;", "return Action::kDisableAndDelete;"),
    ("N54-orphan-without-activation-block", SW,
     "return a_ref.dynamic && !a_ref.deleted && a_ref.inventoryBase && a_ref.activationBlocked && a_ref.inCradle;",
     "return a_ref.dynamic && !a_ref.deleted && a_ref.inventoryBase && a_ref.inCradle;"),
    ("N55-orphan-includes-plugin-refs", SW,
     "return a_ref.dynamic && !a_ref.deleted && a_ref.inventoryBase && a_ref.activationBlocked && a_ref.inCradle;",
     "return !a_ref.deleted && a_ref.inventoryBase && a_ref.activationBlocked && a_ref.inCradle;"),
    ("N56-sweep-during-session", SW, "if (!a_idle || a_ref.deleted) return Action::kKeep;", "if (a_ref.deleted) return Action::kKeep;"),
    ("N57-retired-plugin-ref-reprocessed", SW, "if (a_ref.disabled && !a_ref.dynamic) return Action::kKeep;", "(void)0;"),
    ("N58-cradle-widened", LY, "inline constexpr double kCradleMaxX = 62.0;", "inline constexpr double kCradleMaxX = 80.0;"),
    ("N59-intruder-reports-allow-list", SW,
     "return !a_ref.dynamic && !a_ref.deleted && !a_ref.disabled && a_ref.inCradle && !a_ref.allowListed && !a_ref.displayBase;",
     "return !a_ref.dynamic && !a_ref.deleted && !a_ref.disabled && a_ref.inCradle && !a_ref.displayBase;"),
    ("N60-slab-inverse-wrong-sign", LY, "return { dx * c - dy * s, dx * s + dy * c, a_world.z - a_slab.z };", "return { dx * c + dy * s, -dx * s + dy * c, a_world.z - a_slab.z };"),
    # ---- R8.7.4.21 StatePolicy timing v3 / presentation-ack gates
    ("N61-gate-never-times-out", S, "return a_now >= a_due && (a_acked || a_now >= a_due + a_timing.beatAckTimeoutMs);", "return a_now >= a_due && a_acked;"),
    ("N62-gate-ignores-landed", S, "return a_now >= a_due && (a_acked || a_now >= a_due + a_timing.beatAckTimeoutMs);", "return a_now >= a_due;"),
    ("N63-stale-token-landed-accepted", S,
     "if (a_s.state == State::kPerforming && a_event.a == a_s.token && a_event.b >= 1 && a_event.b <= a_s.beat) a_s.landed",
     "if (a_s.state == State::kPerforming && a_event.b >= 1 && a_event.b <= a_s.beat) a_s.landed"),
    ("N64-future-landed-accepted", S,
     "if (a_s.state == State::kPerforming && a_event.a == a_s.token && a_event.b >= 1 && a_event.b <= a_s.beat) a_s.landed",
     "if (a_s.state == State::kPerforming && a_event.a == a_s.token && a_event.b >= 1) a_s.landed"),
    ("N65-reveal-ignores-seal", S, "if (GateOpen(a_s.t, a_s.boundAt + a_timing.revealAfterBindMs, a_s.sealed, a_timing)) {",
     "if (GateOpen(a_s.t, a_s.boundAt + a_timing.revealAfterBindMs, false, a_timing)) {"),
    ("N66-seal-before-bind-accepted", S, "if (a_s.state == State::kPerforming && a_event.a == a_s.token && a_s.bound) a_s.sealed = true;",
     "if (a_s.state == State::kPerforming && a_event.a == a_s.token) a_s.sealed = true;"),
    ("N67-compression-ignored", S, "return a_timing.strikeIntervalLongMs > 0 && a_timing.compressFromRunes > 0 && a_runes >= a_timing.compressFromRunes ? a_timing.strikeIntervalLongMs :",
     "return false ? a_timing.strikeIntervalLongMs :"),
    ("N68-bind-not-gated", S, "if (GateOpen(a_s.t, a_s.lastBeatAt + a_timing.bindAfterLastStrikeMs, a_s.landed >= n, a_timing)) {",
     "if (a_s.t >= a_s.lastBeatAt + a_timing.bindAfterLastStrikeMs) {"),
    ("N69-absolute-schedule-under-gating", S, "const auto due = a_s.beat == 0 ? a_timing.firstStrikeMs : a_s.lastBeatAt + IntervalFor(n, a_timing);",
     "const auto due = a_timing.firstStrikeMs + IntervalFor(n, a_timing) * a_s.beat;"),
    # ---- R8.7.4.21 I3 RunewordEffectCardPolicy (B5 T6) and Details parity (T5)
    ("N70-card-drops-cooldown", EC, 'if (a_in.icdS > 0.0f) text += ", " + Num(a_in.icdS) + "s cooldown";', "(void)0;"),
    ("N71-card-swaps-magnitude-duration", EC,
     "const double magnitude = a_applied.overridden ? a_applied.fixed : a_effect.magnitude;\n\t\tconst bool passive = a_role == Role::kPassive;\n\t\tconst bool target = a_role == Role::kTarget || a_role == Role::kEcho;\n\t\tconst float duration = passive ? 0.0f : a_effect.durationS;",
     "const double magnitude = a_applied.overridden ? a_applied.fixed : a_effect.durationS;\n\t\tconst bool passive = a_role == Role::kPassive;\n\t\tconst bool target = a_role == Role::kTarget || a_role == Role::kEcho;\n\t\tconst float duration = passive ? 0.0f : a_effect.magnitude;"),
    ("N72-card-reverts-to-record-names", EC, "std::string line = Capitalize(std::move(a_label));",
     "std::string line = a_effect.recordName.empty() ? Capitalize(std::move(a_label)) : a_effect.recordName;"),
    ("N73-card-removed-from-confirm", EC, "if (a_lines.size() <= 1u) return out;", "if (a_lines.size() <= 1u || a_panel == Panel::kConfirm) return out;"),
    ("N74-mult-av-not-percent", EC, '{ "AttackDamageMult", "attack damage", UnitKind::kFractionPct },',
     '{ "AttackDamageMult", "attack damage", UnitKind::kPercentPoints },'),
    ("N75-scaling-share-rounded-to-1-decimal", EC, "Num(static_cast<double>(a_in.scaling.mult) * 100.0)",
     "Num(std::round(a_in.scaling.mult * 10.0) / 10.0 * 100.0)"),
    ("N76-confirm-details-starts-hold", IN, "\t\t\t\t// R8.7.4.21 I3: expands the effect card; never a hold command\n\t\t\t\tdecision.command = Command::kDetail;",
     "\t\t\t\t// R8.7.4.21 I3: expands the effect card; never a hold command\n\t\t\t\tdecision.command = Command::kHoldBegin;"),
    ("N77-select-item-footer-without-details", PR,
     'add("filterNext", Button::kRight, "Filter");\n\t\t\tadd("detail", Button::kDetail, a_detailOpen ? "Hide Details" : "Details");',
     'add("filterNext", Button::kRight, "Filter");'),
    ("N78-details-toggles-under-hold", PR, "return DetailVisible(a_state) && !(a_state == State::kConfirm && a_holdActive);",
     "return DetailVisible(a_state);"),
    ("N79-card-line-budget-exceeded", EC, "const std::size_t bodyBudget = kMaxLines - fixed;", "const std::size_t bodyBudget = kMaxLines + 4u - fixed;"),
    ("N80-forbidden-token-guard-disabled", EC, "if (!IsAsciiLine(part) || ContainsForbiddenToken(part, a_in)) {", "if (false) {"),
    # source mutants (src/), killed by the wiring test
    ("N81-tooltip-back-to-upstream-stats", "src/EventBridge.Loot.TooltipResolution.cpp",
     "auto compactRuneword = RunewordEffectCardPolicy::TooltipText(RunewordEffectCardPolicy::Build(cardInput));",
     "auto compactRuneword = RuneforgePresentation::RunewordResultAffixTooltipEn(recipe->id, recipe->runeIds, affix.displayNameEn);"),
    ("N82-workstation-confirm-without-card", "src/RuneforgeWorkstation.cpp",
     'lines.push_back(Str(f, "effect"));\n\t\t\t\t\t\tfor (auto& line : cardBody) lines.push_back(std::move(line));',
     'lines.push_back(Str(f, "effect"));'),
    ("N83-snapshot-without-card", "src/RuneforgeMenu.cpp",
     '{"effectCard", RunewordEffectCardPolicy::ToJson<json>(recipe.effectCard)},', ""),
    # R8.7.4.21 I5 decision 3: the held result
    ("N84-timed-result-clear-restored", S, "\t\tt.resultHoldMs = 0;\n", "\t\tt.resultHoldMs = 1200;\n"),
    ("N85-held-result-ignores-accept", S,
     "\t\t\t} else if (kind == EventKind::kAccept || kind == EventKind::kBack) {\n\t\t\t\tBeginExit(a_s, ExitReason::kDone, Cause::kForced, a_fx);\n\t\t\t}\n\t\t\tbreak;\n\t\tcase State::kResultFailure:",
     "\t\t\t} else if (kind == EventKind::kBack) {\n\t\t\t\tBeginExit(a_s, ExitReason::kDone, Cause::kForced, a_fx);\n\t\t\t}\n\t\t\tbreak;\n\t\tcase State::kResultFailure:"),
    ("N86-held-result-ignores-interrupt", S,
     "\t\tif (a_event.kind == EventKind::kInterrupt) {\n\t\t\tif (a_s.state != State::kExiting) {",
     "\t\tif (a_event.kind == EventKind::kInterrupt) {\n\t\t\tif (a_s.state != State::kExiting && a_s.state != State::kResultSuccess) {"),
]


def run(include_dir: Path, work: Path):
    exe = work / "t"
    source_root = include_dir.parent
    # R8.7.4.18: the policy test includes nlohmann/json (snapshot contract checks); without the vendor include every build, the
    # control included, failed to compile and the runner reported 0 kills
    c = subprocess.run([CXX, "-std=c++23", "-O1", "-w", f"-I{include_dir}", f"-I{ROOT / 'extern' / 'vendor' / 'include'}", str(TEST), "-o", str(exe)],
                       capture_output=True, text=True)
    if c.returncode != 0:
        return "compile-error", c.stderr.strip().splitlines()[-1] if c.stderr.strip() else ""
    r = subprocess.run([str(exe), str(VECTORS), str(ROOT / "tests" / "data" / "runeforge-snapshot-shape.v1.json")], capture_output=True, text=True, cwd=str(ROOT))
    tail = (r.stdout + r.stderr).strip().splitlines()
    if r.returncode != 0:
        return "fail", (tail[-1] if tail else "")
    # R8.7.4.21 I3: the effect-card wiring test on the same (possibly mutated) tree
    s = subprocess.run([sys.executable, str(STATIC), "--source-root", str(source_root)], capture_output=True, text=True)
    stail = (s.stdout + s.stderr).strip().splitlines()
    if s.returncode != 0:
        return "fail", (stail[-1] if stail else "static wiring test failed")
    return "pass", (tail[-1] if tail else "")


def main() -> int:
    results = []
    project_tmp = ROOT.parents[1] / ".tmp"
    project_tmp.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ws-native-mutants-", dir=project_tmp) as tmp:
        base = Path(tmp)
        for name, header, old, new in [("control", S, "", "")] + [m for m in MUTANTS if ONLY is None or m[0] in ONLY or m[0].split("-")[0] in ONLY]:
            inc = base / name / "include"
            shutil.copytree(ROOT / "include", inc)
            for rel in STATIC_INPUTS:  # only the files the wiring test reads (copying all of src/ per mutant is slow)
                (base / name / rel).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / rel, base / name / rel)
            if old:
                path = (base / name / header) if header.startswith(("src/", "ui/")) else (inc / "CalamityAffixes" / header)
                text = path.read_text(encoding="utf-8")
                if text.count(old) != 1:
                    results.append({"id": name, "killed": False, "detail": f"mutation site found {text.count(old)} times"})
                    continue
                path.write_text(text.replace(old, new), encoding="utf-8", newline="\n")
            verdict, detail = run(inc, base / name)
            killed = verdict != "pass"
            results.append({"id": name, "killed": killed, "verdict": verdict, "detail": detail})
    ok = True
    for r in results:
        good = (not r["killed"]) if r["id"] == "control" else (r["killed"] and r.get("verdict") == "fail")
        ok = ok and good
        print(f"{'PASS' if good else 'FAIL'} {r['id']}: {r.get('verdict', '?')} {r['detail'][:140]}")
    killed = sum(1 for r in results if r["id"] != "control" and r["killed"] and r.get("verdict") == "fail")
    print(f"native workstation mutation: {killed}/{len(results) - 1} killed by the test (not by compile errors), control {'PASS' if not results[0]['killed'] else 'FAIL'}")
    if OUT:
        Path(OUT).write_text(json.dumps({"ok": ok, "results": results}, indent=1), encoding="utf-8")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
