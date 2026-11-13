#pragma once
// Proposed FEASR portable identification presentation contract.
// Source-only: calls to native CraftSelectedAffixes and animation graph are
// deliberately NOT made from this pure state machine.
#include <cstdint>

namespace CalamityAffixes::IdentificationRitualPolicy {
enum class Phase : std::uint8_t {
    Idle, Requested, Kneeling, ScrollOpened, ProjectionStable, AwaitingCommit,
    Committing, Recovering, Completed, Cancelled
};
enum class Event : std::uint8_t {
    Begin, KneelReached, ScrollOpened, RunesProjected, CommitMarker,
    NativeSucceeded, NativeFailed, StoodUp, Abort
};
struct Context {
    Phase phase{Phase::Idle};
    std::uint64_t instanceKey{0};
    std::uint64_t attemptId{0};
    bool consumed{false};
    bool commitIssued{false};
    bool resultReady{false};
};
struct Decision {
    bool changed{false};
    bool requestNativeIdentify{false};
    bool requestAnimationExit{false};
    bool displayResult{false};
};
[[nodiscard]] inline Decision Step(Context& ctx, Event event,
    std::uint64_t expectedAttemptId=0, std::uint64_t selectedInstanceKey=0) noexcept
{
    Decision d{};
    // Attempt correlation prevents stale animation events from a previous ritual
    // reaching a different item. Begin uses a fresh, nonzero attempt ID.
    if (event==Event::Begin) {
        if (ctx.phase!=Phase::Idle && ctx.phase!=Phase::Cancelled && ctx.phase!=Phase::Completed)
            return d;
        if (!expectedAttemptId || !selectedInstanceKey) return d;
        ctx={Phase::Requested,selectedInstanceKey,expectedAttemptId,false,false,false};
        d.changed=true;
        return d;
    }
    if (!ctx.attemptId || expectedAttemptId!=ctx.attemptId) return d;
    if (event==Event::Abort) {
        if (ctx.phase==Phase::Committing || ctx.phase==Phase::Recovering ||
            ctx.phase==Phase::Completed || ctx.phase==Phase::Cancelled) return d;
        ctx.phase=Phase::Cancelled;d.changed=true;d.requestAnimationExit=true;
        return d;
    }
    switch (ctx.phase) {
    case Phase::Requested:
        if (event==Event::KneelReached) {ctx.phase=Phase::Kneeling;d.changed=true;}
        break;
    case Phase::Kneeling:
        if (event==Event::ScrollOpened) {ctx.phase=Phase::ScrollOpened;d.changed=true;}
        break;
    case Phase::ScrollOpened:
        if (event==Event::RunesProjected) {ctx.phase=Phase::ProjectionStable;d.changed=true;}
        break;
    case Phase::ProjectionStable:
        // Only native-confirmed commit marker requests the transaction.
        if (event==Event::CommitMarker && !ctx.commitIssued) {
            ctx.phase=Phase::Committing;ctx.commitIssued=true;
            d.changed=true;d.requestNativeIdentify=true;
        }
        break;
    case Phase::Committing:
        if (event==Event::NativeSucceeded) {
            ctx.consumed=true;ctx.resultReady=true;
            ctx.phase=Phase::Recovering;d.changed=true;d.requestAnimationExit=true;
        } else if (event==Event::NativeFailed) {
            ctx.phase=Phase::Cancelled;d.changed=true;d.requestAnimationExit=true;
        }
        break;
    case Phase::Recovering:
        if (event==Event::StoodUp) {
            ctx.phase=Phase::Completed;d.changed=true;d.displayResult=ctx.resultReady;
        }
        break;
    default:
        break;
    }
    return d;
}
}
