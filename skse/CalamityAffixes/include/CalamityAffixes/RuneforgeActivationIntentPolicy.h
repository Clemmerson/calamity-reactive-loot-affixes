#pragma once

#include <chrono>
#include <cstdint>

namespace CalamityAffixes::RuneforgeWorkstation::ActivationIntentPolicy
{
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;
	inline constexpr auto kCleanupWindow = std::chrono::seconds(30);

	struct Identity
	{
		std::uint32_t player{ 0 };
		std::uint32_t cell{ 0 };
		std::uint32_t target{ 0 };
		std::uint64_t session{ 0 };
		std::uint64_t cellEpoch{ 0 };
		std::uint64_t menuEpoch{ 0 };
		bool gameplayAvailable{ false };
	};

	struct OpenIdentity
	{
		std::uint32_t player{ 0 };
		std::uint32_t cell{ 0 };
		std::uint64_t session{ 0 };
		std::uint64_t cellEpoch{ 0 };
	};

	struct Authorization
	{
		std::uint64_t sequence{ 0 };
		TimePoint createdAt{};
		Identity capture{};
		OpenIdentity open{};
		bool present{ false };
	};

	struct State
	{
		std::uint64_t sequence{ 0 };
		Authorization pending{};
		Authorization claimed{};
	};

	[[nodiscard]] inline bool SameCapture(const Identity& a, const Identity& b) noexcept
	{
		return a.player == b.player && a.cell == b.cell && a.target == b.target && a.session == b.session &&
			a.cellEpoch == b.cellEpoch && a.menuEpoch == b.menuEpoch && a.gameplayAvailable && b.gameplayAvailable;
	}

	[[nodiscard]] inline bool SameOpen(const OpenIdentity& a, const OpenIdentity& b) noexcept
	{
		return a.player == b.player && a.cell == b.cell && a.session == b.session && a.cellEpoch == b.cellEpoch;
	}

	inline void ClearPending(State& s) noexcept { s.pending = {}; }
	inline void ClearClaimed(State& s) noexcept { s.claimed = {}; }
	inline void Clear(State& s) noexcept { ClearPending(s); ClearClaimed(s); }

	[[nodiscard]] inline bool ObserveActivateDown(State& s, const Identity& id, bool exactTarget, bool idle, TimePoint now) noexcept
	{
		if (!exactTarget || !idle || !id.gameplayAvailable) return false;
		if (s.claimed.present) {
			if (now < s.claimed.createdAt || now - s.claimed.createdAt < kCleanupWindow) return false;
			ClearClaimed(s);  // only a later physical edge can recover an abandoned claim
		}
		++s.sequence;
		if (s.sequence == 0) ++s.sequence;
		s.pending = { .sequence = s.sequence, .createdAt = now, .capture = id, .present = true };
		return true;
	}

	// Atomically changes Pending to Claimed. Claim is one-use before Papyrus side effects.
	[[nodiscard]] inline std::uint64_t Claim(State& s, const Identity& current, TimePoint now) noexcept
	{
		if (!s.pending.present) return 0;
		const auto candidate = s.pending;
		// Elapsed time is not authorization. A captured physical edge remains valid
		// until claimed or replaced; exact context is the authorization check.
		if (now < candidate.createdAt || !SameCapture(candidate.capture, current)) {
			ClearPending(s);
			return 0;
		}
		ClearPending(s);
		s.claimed = { .sequence = candidate.sequence, .createdAt = candidate.createdAt, .capture = candidate.capture,
			.open = { current.player, current.cell, current.session, current.cellEpoch }, .present = true };
		return s.claimed.sequence;
	}

	// Final consumption intentionally ignores target/menu/gameplay state. A first-help
	// Message.Show transition is expected after Claim; only session and cell identity bind Open.
	[[nodiscard]] inline bool ConsumeClaim(State& s, std::uint64_t sequence, const OpenIdentity& current) noexcept
	{
		if (sequence == 0 || !s.claimed.present || s.claimed.sequence != sequence) return false;
		if (!SameOpen(s.claimed.open, current)) {
			ClearClaimed(s);
			return false;
		}
		ClearClaimed(s);
		return true;
	}
}
