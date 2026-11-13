#pragma once

// Runeforge workstation staging gate and render de-duplication (RC1-WS, 2026-10-19; run 1/2 fixes ported from the
// FEASR_RF_REVIEW_ONLY r2 build into production, now acknowledgement-driven by protocol v3). Pure policy.
//
// Staging gate: one staging request (item or rune row) is in flight at a time; a request made meanwhile waits and a newer
// one of the same kind replaces it (latest wins); the flight ends on the request's final acknowledgement or after
// kFlightTimeoutMs. A request whose physical content is already on the slab and was positively acknowledged is not resent
// (a failed one is always retried):
// the caller answers it from the recorded acknowledgement (REUSE), so every StatePolicy seq still gets exactly one answer
// and nothing is assumed (PRES-D29). Clears are never gated. A strike or a skip changes the row on the host, so the next
// request is always sent (Forget).
//
// Render: native pushes the AS2 model only when it changed (run 1: every frame; the panel rebuilt each frame, cost FPS and
// dropped clicks between press and release).

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace CalamityAffixes::RuneforgeWorkstation::StagePolicy
{
	inline constexpr std::int64_t kFlightTimeoutMs = 3000;  // = runeAckTimeoutMs: past it the row is BAD anyway

	enum class Kind : std::uint8_t
	{
		kItem,
		kRunes
	};

	enum class Result : std::uint8_t
	{
		kNone,
		kPending,
		kOk,
		kBad
	};

	struct Request
	{
		bool valid{ false };
		std::int64_t seq{ 0 };
		std::int64_t index{ -1 };
		std::string identity{};  // what lands on the slab: item instance key, or formula id + slot glyphs
	};

	struct Slot
	{
		Request waiting{};
		bool known{ false };      // identity/result describe what is on the slab now
		std::string identity{};   // the last request sent
		std::int64_t seq{ 0 };    // its seq
		Result result{ Result::kNone };
	};

	struct Gate
	{
		bool inFlight{ false };
		Kind flightKind{ Kind::kItem };
		std::int64_t flightSeq{ 0 };
		std::int64_t flightUntilMs{ 0 };
		std::array<Slot, 2> slots{};
		std::uint32_t sent{ 0 };
		std::uint32_t reused{ 0 };
		std::uint32_t replaced{ 0 };
		std::uint32_t timedOut{ 0 };
	};

	enum class Decision : std::uint8_t
	{
		kSend,   // send now (the flight starts)
		kQueue,  // waits behind the flight
		kReuse   // already on the slab and acknowledged: answer from Slot::result
	};

	[[nodiscard]] constexpr std::size_t Index(Kind a_kind) noexcept { return a_kind == Kind::kItem ? 0u : 1u; }

	inline void BeginFlight(Gate& a_gate, Kind a_kind, const Request& a_request, std::int64_t a_nowMs)
	{
		auto& slot = a_gate.slots[Index(a_kind)];
		a_gate.inFlight = true;
		a_gate.flightKind = a_kind;
		a_gate.flightSeq = a_request.seq;
		a_gate.flightUntilMs = a_nowMs + kFlightTimeoutMs;
		slot.known = true;
		slot.identity = a_request.identity;
		slot.seq = a_request.seq;
		slot.result = Result::kPending;
		a_gate.sent += 1;
	}

	[[nodiscard]] inline Decision RequestStage(Gate& a_gate, Kind a_kind, Request a_request, std::int64_t a_nowMs)
	{
		auto& slot = a_gate.slots[Index(a_kind)];
		a_request.valid = true;
		if (slot.known && slot.identity == a_request.identity && slot.result == Result::kOk) {
			if (slot.waiting.valid) a_gate.replaced += 1;
			slot.waiting = {};
			a_gate.reused += 1;
			return Decision::kReuse;
		}
		if (a_gate.inFlight) {
			if (slot.waiting.valid) a_gate.replaced += 1;
			slot.waiting = std::move(a_request);
			return Decision::kQueue;
		}
		BeginFlight(a_gate, a_kind, a_request, a_nowMs);
		return Decision::kSend;
	}

	// The final acknowledgement of a sent request (ITEM LOADED / FAIL, RUNES): records the result, ends its flight.
	inline void Acked(Gate& a_gate, Kind a_kind, std::int64_t a_seq, bool a_ok) noexcept
	{
		auto& slot = a_gate.slots[Index(a_kind)];
		if (slot.known && slot.seq == a_seq && slot.result == Result::kPending) slot.result = a_ok ? Result::kOk : Result::kBad;
		if (a_gate.inFlight && a_gate.flightKind == a_kind && a_gate.flightSeq == a_seq) a_gate.inFlight = false;
	}

	struct Release
	{
		Kind kind{ Kind::kItem };
		Request request{};
	};

	// Per frame: ends an expired flight (its result stays pending: StatePolicy times the row out), then hands back the
	// waiting item before the waiting rune row; the caller runs RequestStage on it again (send or reuse).
	[[nodiscard]] inline std::optional<Release> Poll(Gate& a_gate, std::int64_t a_nowMs)
	{
		if (a_gate.inFlight && a_nowMs >= a_gate.flightUntilMs) {
			a_gate.inFlight = false;
			a_gate.timedOut += 1;
		}
		if (a_gate.inFlight) return std::nullopt;
		for (const auto kind : { Kind::kItem, Kind::kRunes }) {
			auto& waiting = a_gate.slots[Index(kind)].waiting;
			if (!waiting.valid) continue;
			Release release{ kind, std::move(waiting) };
			waiting = {};
			return release;
		}
		return std::nullopt;
	}

	// ClearItem / ClearRunes (Back, Operations, End): the slot is known empty, nothing waits for it, its flight is over.
	inline void Cleared(Gate& a_gate, Kind a_kind)
	{
		auto& slot = a_gate.slots[Index(a_kind)];
		slot = {};
		slot.known = true;
		if (a_gate.inFlight && a_gate.flightKind == a_kind) a_gate.inFlight = false;
	}

	// The host changed the slot on its own (a struck rune is spent, SKIP cleans the row): the next request is always sent.
	inline void Forget(Gate& a_gate, Kind a_kind) noexcept { a_gate.slots[Index(a_kind)].known = false; }

	struct RenderDedup
	{
		std::string last{};
		bool valid{ false };
		std::uint32_t pushed{ 0 };
		std::uint32_t suppressed{ 0 };
	};

	[[nodiscard]] inline bool ShouldPush(RenderDedup& a_dedup, const std::string& a_payload)
	{
		if (a_dedup.valid && a_dedup.last == a_payload) {
			a_dedup.suppressed += 1;
			return false;
		}
		a_dedup.last = a_payload;
		a_dedup.valid = true;
		a_dedup.pushed += 1;
		return true;
	}

	inline void ForcePush(RenderDedup& a_dedup) noexcept { a_dedup.valid = false; }

	[[nodiscard]] constexpr const char* Name(Decision a_decision) noexcept
	{
		switch (a_decision) {
		case Decision::kSend: return "send";
		case Decision::kQueue: return "queued (one staging in flight)";
		case Decision::kReuse: return "reuse (already staged and acknowledged)";
		}
		return "?";
	}
}
