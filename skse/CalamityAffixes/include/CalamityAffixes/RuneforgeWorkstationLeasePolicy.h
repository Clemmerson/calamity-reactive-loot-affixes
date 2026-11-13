#pragma once

// Runeforge workstation gameplay-control lease (RC1-WS, 2026-10-19; PRES-D28, R3 contract section 4). Pure policy over the
// RE::ControlMap enabled-controls word (RE::UserEvents::USER_EVENT_FLAG bits; the same mechanism as Game.DisablePlayerControls).
// R3.1: the word is read and written through ControlsPolicy at the running runtime's offsets, never the vendored
// ControlMap::ToggleControls (SE offsets; r3 attempt 1). The lease is taken after the verified seat (PRES-D34). Run 2 showed the menu input context alone does not stop gameplay handlers:
// R drew the weapon and every right-drag raised the shield. The workstation's own input (raw device sink + its menu) is not
// gated by these flags, so Accept/Back, the hold, the right-drag pan, the right-stick orbit, Skip and F10 keep working.
//
// Acquire snapshots the word and disables the leased flags that were enabled; Release re-enables exactly those, so the
// snapshot state of every leased control comes back and a flag a scene had already disabled stays disabled. Release is
// keyed by the session token and idempotent: a second release, or a release with nothing held, changes nothing.

#include <cstdint>

namespace CalamityAffixes::RuneforgeWorkstation::LeasePolicy
{
	// RE::UserEvents::USER_EVENT_FLAG (include/RE/U/UserEvents.h); static_asserted against CommonLib in the game build.
	inline constexpr std::uint32_t kMovement = 1u << 0;   // movement, sprint
	inline constexpr std::uint32_t kLooking = 1u << 1;    // gameplay look (the workstation pans/orbits by writing the camera)
	inline constexpr std::uint32_t kActivate = 1u << 2;   // E activates the world
	inline constexpr std::uint32_t kMenu = 1u << 3;       // Tween / Journal / inventory openers
	inline constexpr std::uint32_t kConsole = 1u << 4;    // never leased (the review harness and support use the console)
	inline constexpr std::uint32_t kPOVSwitch = 1u << 5;  // first/third person toggle
	inline constexpr std::uint32_t kFighting = 1u << 6;   // attack, block, ready/sheath, shout, cast
	inline constexpr std::uint32_t kSneaking = 1u << 7;
	inline constexpr std::uint32_t kMainFour = 1u << 8;   // favourites / the four quick menus
	inline constexpr std::uint32_t kWheelZoom = 1u << 9;
	inline constexpr std::uint32_t kJumping = 1u << 10;
	inline constexpr std::uint32_t kVATS = 1u << 11;

	inline constexpr std::uint32_t kLeasedFlags =
		kMovement | kLooking | kActivate | kMenu | kPOVSwitch | kFighting | kSneaking | kMainFour | kWheelZoom | kJumping | kVATS;
	static_assert((kLeasedFlags & kConsole) == 0, "the console is never leased");

	struct Lease
	{
		bool held{ false };
		std::uint32_t token{ 0 };
		std::uint32_t snapshot{ 0 };  // enabledControls at acquire
		std::uint32_t mask{ 0 };      // the leased flags this lease disabled (enabled in the snapshot)
	};

	struct AcquireResult
	{
		bool acquired{ false };
		std::uint32_t disable{ 0 };  // flags to clear (ControlsPolicy::Toggle(..., false))
	};

	// One lease at a time. Acquire while held refuses (the caller logs it); the held lease is untouched.
	[[nodiscard]] constexpr AcquireResult Acquire(Lease& a_lease, std::uint32_t a_token, std::uint32_t a_enabledNow, std::uint32_t a_leased = kLeasedFlags) noexcept
	{
		if (a_lease.held || a_token == 0u) return {};
		a_lease.held = true;
		a_lease.token = a_token;
		a_lease.snapshot = a_enabledNow;
		a_lease.mask = a_enabledNow & a_leased & ~kConsole;
		return { true, a_lease.mask };
	}

	struct ReleaseResult
	{
		bool effective{ false };           // false: nothing held, or another session's token (a logged no-op)
		std::uint32_t enable{ 0 };         // flags to set again (ControlsPolicy::Toggle(..., true))
		std::uint32_t changedUnderneath{ 0 };  // leased flags someone else changed during the session (logged)
	};

	// a_token 0 releases whatever is held (preload, reset, sweep: the session token may already be gone).
	[[nodiscard]] constexpr ReleaseResult Release(Lease& a_lease, std::uint32_t a_token, std::uint32_t a_enabledNow) noexcept
	{
		if (!a_lease.held || (a_token != 0u && a_token != a_lease.token)) return {};
		ReleaseResult r{};
		r.effective = true;
		r.enable = a_lease.mask;
		// what we disabled should still be disabled; leased flags outside the mask should still match the snapshot
		r.changedUnderneath = (a_enabledNow & a_lease.mask) | ((a_enabledNow ^ a_lease.snapshot) & kLeasedFlags & ~a_lease.mask);
		a_lease = {};
		return r;
	}

	// The controls word after a release, for tests: the snapshot state of every leased flag is back.
	[[nodiscard]] constexpr std::uint32_t Apply(std::uint32_t a_enabled, std::uint32_t a_disable, std::uint32_t a_enable) noexcept
	{
		return (a_enabled & ~a_disable) | a_enable;
	}
}
