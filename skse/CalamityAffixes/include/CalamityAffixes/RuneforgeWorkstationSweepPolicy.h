#pragma once

// Runeforge workstation native staging sweep (R8.7.4.21, feasr-core/alpha/rc/r87421/A3-preview findings G1/G2/G3 and
// draft/RuneforgeWorkstation.NativeSweep.r87421-A3.diff). Pure policy: which forge-cell references the sweep may remove and how.
// The sweep runs only with no session (post-load, slab cell attach, exit watchdog); RuneforgeWorkstation.cpp gathers the facts.
//   G2: a removed reference is deleted only when this runtime created it (an FF dynamic form); a plugin-placed reference is only
//       disabled (reversible: no deleted-reference change form in the save).
//   G1: an item preview clone that escaped every ledger (an older save) is still found: a dynamic reference of an inventory-item base,
//       activation-blocked (the stage host blocks every clone; a player-dropped item never is) and lying in the item cradle.
//   G3: an enabled plugin-placed reference in the cradle that is not a known workstation reference is reported (never touched).

#include <cstdint>

namespace CalamityAffixes::RuneforgeWorkstation::SweepPolicy
{
	enum class Action : std::uint8_t
	{
		kKeep,
		kDisable,
		kDisableAndDelete
	};

	struct RefFacts
	{
		bool deleted{ false };
		bool disabled{ false };
		bool dynamic{ false };            // FormID >= 0xFF000000: created at runtime (PlaceAtMe)
		bool displayBase{ false };        // base in the Presentation rune display range (0x800-0x820)
		bool inventoryBase{ false };      // WEAP / ARMO / AMMO / MISC base
		bool activationBlocked{ false };
		bool inCradle{ false };           // LayoutPolicy::CradleContains of its slab-local position
		bool allowListed{ false };        // the slab, gear anchor, stage host, slab activator, workpiece glow
	};

	[[nodiscard]] constexpr Action Remove(bool a_dynamic) noexcept
	{
		return a_dynamic ? Action::kDisableAndDelete : Action::kDisable;
	}

	// A reference the sweep already tracks (the native spawned list or the Papyrus ledger FormList).
	[[nodiscard]] constexpr Action ForTracked(const RefFacts& a_ref) noexcept
	{
		return a_ref.deleted ? Action::kKeep : Remove(a_ref.dynamic);
	}

	[[nodiscard]] constexpr bool IsOrphanItemClone(const RefFacts& a_ref) noexcept
	{
		return a_ref.dynamic && !a_ref.deleted && a_ref.inventoryBase && a_ref.activationBlocked && a_ref.inCradle;
	}

	// The forge-cell scan. a_idle: no session owns the slab (the live preview of an open session is never touched).
	[[nodiscard]] constexpr Action ForCellScan(const RefFacts& a_ref, bool a_idle) noexcept
	{
		if (!a_idle || a_ref.deleted) return Action::kKeep;
		if (a_ref.disabled && !a_ref.dynamic) return Action::kKeep;  // a disabled plugin reference is already retired
		if (a_ref.displayBase || IsOrphanItemClone(a_ref)) return Remove(a_ref.dynamic);
		return Action::kKeep;
	}

	// G3 report only: placed dressing re-enabled into the item cradle by a later review plugin (the R7B ItemPreviewRef case).
	[[nodiscard]] constexpr bool IsCradleIntruder(const RefFacts& a_ref) noexcept
	{
		return !a_ref.dynamic && !a_ref.deleted && !a_ref.disabled && a_ref.inCradle && !a_ref.allowListed && !a_ref.displayBase;
	}
}
