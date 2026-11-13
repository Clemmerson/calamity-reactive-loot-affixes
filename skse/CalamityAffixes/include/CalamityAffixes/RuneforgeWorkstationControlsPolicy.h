#pragma once

// Runeforge workstation ControlMap access (RC1-WS r2, 2026-10-19; PRES-D34, R3-E11). Pure policy: no game types.
//
// r3 attempt 1 soft-locked the game: the vendored CommonLib declares RE::ControlMap with SE direct-member offsets
// (contextPriorityStack 0x100, enabledControls 0x118, unk11C 0x11C) and its ToggleControls() writes those members itself.
// On 1.6.1170 (AE) the class is 8 bytes longer before them (an extra input context): contextPriorityStack 0x108,
// enabledControls 0x120, the companion word 0x124 (alpha/workstation/r3/r31-layout-audit.json; 24 installed AE-built plugin
// PDBs agree). Offset 0x118 on AE is contextPriorityStack.size, so the lease zeroed the input-context stack: no keyboard,
// menu or console. The workstation therefore never touches the vendored members: the native side reads and writes the two
// words at the offsets this policy picks for the running runtime, only after the self-check below passes, mirrors the
// engine toggle semantics the vendored code imitates (flags plus the companion word unless it is kInvalid, then the
// UserEventEnabled event), and verifies every acquire and release on a later tick.

#include <array>
#include <cstdint>
#include <string_view>

namespace CalamityAffixes::RuneforgeWorkstation::ControlsPolicy
{
	// RE::UserEvents::USER_EVENT_FLAG bits (static_asserted against CommonLib in the game build).
	inline constexpr std::uint32_t kMovement = 1u << 0;
	inline constexpr std::uint32_t kLooking = 1u << 1;
	inline constexpr std::uint32_t kActivate = 1u << 2;
	inline constexpr std::uint32_t kMenu = 1u << 3;
	inline constexpr std::uint32_t kConsole = 1u << 4;
	inline constexpr std::uint32_t kInvalid = 1u << 31;
	inline constexpr std::uint32_t kKnownBits = 0x00000FFFu;
	inline constexpr std::uint32_t kUnknownBits = 0x7FFFF000u;
	// At acquire the player must still be able to open menus and the console (no scene restricts them); the lease never takes
	// the console, and a word that lacks these bits is either a scene (do not lease) or the wrong field (r3-run1 read 0x001).
	inline constexpr std::uint32_t kRequiredAtAcquire = kMenu | kConsole;
	// Fail-safe restore: everything a player needs to never be stranded (PRES-D34), on top of the lease mask.
	inline constexpr std::uint32_t kFailsafeBits = kMovement | kLooking | kActivate | kMenu | kConsole;
	inline constexpr std::uint32_t kMaxStackEntries = 32;

	struct Layout
	{
		std::string_view name{};
		std::uint32_t stackOffset{ 0 };      // BSTArray<InputContextID> contextPriorityStack: data +0x0, capacity +0x8, size +0x10
		std::uint32_t flagsOffset{ 0 };      // enabledControls
		std::uint32_t companionOffset{ 0 };  // unk11C (SE) / unk124 (AE): mirrored unless kInvalid
		std::uint32_t contexts{ 0 };         // INPUT_CONTEXT_ID count: every stack entry is below it
		std::uint32_t objectSize{ 0 };       // sizeof(ControlMap)
	};
	inline constexpr std::uint32_t kStackDataAt = 0x0;
	inline constexpr std::uint32_t kStackCapacityAt = 0x8;
	inline constexpr std::uint32_t kStackSizeAt = 0x10;
	inline constexpr Layout kLayoutSE{ "SE", 0x100, 0x118, 0x11C, 17, 0x128 };
	inline constexpr Layout kLayoutAE{ "AE", 0x108, 0x120, 0x124, 18, 0x130 };

	// The shifted layout arrives with the 18th input context in runtime 1.6.1130. The vendored CommonLib (newest known runtime
	// 1.6.629) and its SE members are right up to there, so IsAE() alone would pick the wrong layout on 1.6.317..1.6.1129. Only
	// 1.6.1170 (PDB oracles) and the vendored SE header are proven; on any runtime the self-check still gates every write.
	inline constexpr std::uint32_t kShiftMinor = 6;
	inline constexpr std::uint32_t kShiftPatch = 1130;

	[[nodiscard]] constexpr const Layout& ForRuntime(std::uint32_t a_major, std::uint32_t a_minor, std::uint32_t a_patch) noexcept
	{
		const bool shifted = a_major > 1 || (a_major == 1 && (a_minor > kShiftMinor || (a_minor == kShiftMinor && a_patch >= kShiftPatch)));
		return shifted ? kLayoutAE : kLayoutSE;
	}

	// The raw words read from the live ControlMap at a layout's offsets.
	struct Raw
	{
		bool readable{ false };                // the ControlMap object range was committed and readable
		std::uint64_t stackData{ 0 };
		std::uint32_t stackCapacity{ 0 };
		std::uint32_t stackSize{ 0 };
		bool entriesReadable{ false };         // the stack's entries were readable
		std::array<std::uint32_t, kMaxStackEntries> entries{};
		std::uint32_t flags{ 0 };
		std::uint32_t companion{ 0 };
	};

	enum Fault : std::uint32_t
	{
		kFaultUnreadable = 1u << 0,
		kFaultStackData = 1u << 1,        // null or implausible stack data pointer
		kFaultStackSize = 1u << 2,        // stack size outside 1..contexts
		kFaultStackCapacity = 1u << 3,    // capacity below the size or above kMaxStackEntries
		kFaultStackEntries = 1u << 4,     // entries unreadable, or one is not an input context id
		kFaultUnknownBits = 1u << 5,      // the flag word's unknown bits are mixed (neither all set nor all clear)
		kFaultRestricted = 1u << 6        // at acquire: kRequiredAtAcquire not all enabled
	};

	struct Check
	{
		bool ok{ false };
		std::uint32_t faults{ 0 };
	};

	[[nodiscard]] constexpr bool PlausiblePointer(std::uint64_t a_value) noexcept
	{
		return a_value >= 0x10000ull && a_value < 0x00007FFFFFFFFFFFull && (a_value & 0x3ull) == 0;
	}

	// The layout self-check: structural (the input-context stack at the chosen offset looks like one) plus the flag word.
	[[nodiscard]] constexpr Check SelfCheck(const Raw& a_raw, const Layout& a_layout, bool a_atAcquire) noexcept
	{
		std::uint32_t f = 0;
		if (!a_raw.readable) return { false, kFaultUnreadable };
		if (!PlausiblePointer(a_raw.stackData)) f |= kFaultStackData;
		if (a_raw.stackSize == 0 || a_raw.stackSize > a_layout.contexts) f |= kFaultStackSize;
		if (a_raw.stackCapacity < a_raw.stackSize || a_raw.stackCapacity > kMaxStackEntries) f |= kFaultStackCapacity;
		if (!(f & (kFaultStackData | kFaultStackSize | kFaultStackCapacity))) {
			if (!a_raw.entriesReadable) {
				f |= kFaultStackEntries;
			} else {
				for (std::uint32_t i = 0; i < a_raw.stackSize && i < kMaxStackEntries; ++i) {
					if (a_raw.entries[i] >= a_layout.contexts) f |= kFaultStackEntries;
				}
			}
		}
		const auto unknown = a_raw.flags & kUnknownBits;
		if (unknown != 0u && unknown != kUnknownBits) f |= kFaultUnknownBits;
		if (a_atAcquire && (a_raw.flags & kRequiredAtAcquire) != kRequiredAtAcquire) f |= kFaultRestricted;
		return { f == 0u, f };
	}

	struct Words
	{
		std::uint32_t flags{ 0 };
		std::uint32_t companion{ 0 };
	};

	// The engine toggle semantics (what the vendored ToggleControls imitates, at the right offsets): the flag word gets the bits
	// set or cleared, and the companion word follows unless it holds kInvalid.
	[[nodiscard]] constexpr Words Toggle(Words a_words, std::uint32_t a_mask, bool a_enable) noexcept
	{
		Words out = a_words;
		out.flags = a_enable ? (a_words.flags | a_mask) : (a_words.flags & ~a_mask);
		if (a_words.companion != kInvalid) out.companion = a_enable ? (a_words.companion | a_mask) : (a_words.companion & ~a_mask);
		return out;
	}

	// Acquire verification on the next tick: every leased bit is off, the console is still on, the stack still sane.
	[[nodiscard]] constexpr bool AcquireHolds(const Raw& a_raw, const Layout& a_layout, std::uint32_t a_mask) noexcept
	{
		return SelfCheck(a_raw, a_layout, false).ok && (a_raw.flags & a_mask) == 0u && (a_raw.flags & kConsole) != 0u;
	}

	// What a release has not given back yet: a leased bit still off, or the console/menu bits the snapshot had.
	[[nodiscard]] constexpr std::uint32_t Missing(std::uint32_t a_snapshot, std::uint32_t a_mask, std::uint32_t a_flagsNow) noexcept
	{
		return (a_mask & ~a_flagsNow) | ((a_snapshot & (kConsole | kMenu)) & ~a_flagsNow);
	}

	// The release verifier (PRES-D34): pending until a later tick confirms the restore; one repair, then the fail-safe.
	enum class Step : std::uint8_t
	{
		kWait,         // not a later tick yet
		kVerified,     // the snapshot state of every leased control is back and the stack is sane
		kRepair,       // write the mask again (once)
		kFailsafe,     // force-enable kFailsafeBits (once)
		kFailsafeVerified,
		kFailsafeUnverified  // logged as an error; the pending state ends (never an endless loop)
	};

	struct Restore
	{
		bool pending{ false };
		std::uint32_t token{ 0 };
		std::uint32_t snapshot{ 0 };
		std::uint32_t mask{ 0 };
		std::uint32_t stage{ 0 };    // 0 first verification, 1 after the repair, 2 after the fail-safe
		std::uint64_t frame{ 0 };    // the tick of the last write
	};

	[[nodiscard]] constexpr Step Verify(Restore& a_r, const Raw& a_raw, const Layout& a_layout, std::uint64_t a_frame) noexcept
	{
		if (!a_r.pending) return Step::kVerified;
		if (a_frame <= a_r.frame) return Step::kWait;
		const bool sane = SelfCheck(a_raw, a_layout, false).ok;
		const bool back = sane && Missing(a_r.snapshot, a_r.mask, a_raw.flags) == 0u;
		if (back) {
			const auto stage = a_r.stage;
			a_r.pending = false;
			return stage >= 2 ? Step::kFailsafeVerified : Step::kVerified;
		}
		a_r.frame = a_frame;
		if (a_r.stage == 0) {
			a_r.stage = 1;
			return Step::kRepair;
		}
		if (a_r.stage == 1) {
			a_r.stage = 2;
			return Step::kFailsafe;
		}
		a_r.pending = false;
		return Step::kFailsafeUnverified;
	}

	[[nodiscard]] constexpr std::string_view Name(Step a_step) noexcept
	{
		switch (a_step) {
		case Step::kWait: return "WAIT";
		case Step::kVerified: return "VERIFIED";
		case Step::kRepair: return "REPAIR";
		case Step::kFailsafe: return "FAILSAFE";
		case Step::kFailsafeVerified: return "FAILSAFE_VERIFIED";
		case Step::kFailsafeUnverified: return "FAILSAFE_UNVERIFIED";
		}
		return "?";
	}
}
