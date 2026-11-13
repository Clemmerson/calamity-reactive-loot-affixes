#pragma once

#include <cstdint>

// Runeforge workstation session (FE-ASR RC1, 2026-10-19; feasr-core docs/RUNEFORGE_WORKSTATION_BUILD_20261019.md).
// The native owner of the physical workstation: state machine, input, camera, timeline, staging protocol v2 and
// cleanup. Policies: RuneforgeWorkstation*Policy.h. The commit is RuneforgeMenuOperations.h Transmute, unchanged.

namespace RE
{
	class TESObjectREFR;
}

namespace CalamityAffixes::RuneforgeWorkstation
{
	// kDataLoaded: menu factory, input/event sinks, Ack listener, watchdog worker, settings.
	void Install();
	// Called once the Runeforge open policy has passed. True when the workstation session started; false means
	// the caller opens the flat panel (context failed, flat mode preferred, or Presentation missing).
	[[nodiscard]] bool TryOpen();
	// A session is not IDLE (INV-1: a second Open is refused).
	[[nodiscard]] bool IsActive();
	// Atomically claims one Pending exact-target Activate edge before Papyrus side effects.
	[[nodiscard]] std::uint64_t ClaimActivationIntent();
	// Consumes the exact claim after RequestOpen; only player/session/cell generation bind Open.
	[[nodiscard]] bool ConsumeClaimedActivationIntent(std::uint64_t a_sequence);
	// kPreLoadGame: abort, close the menu, restore the camera at once. kPostLoadGame: IDLE, WS_Reset(0), sweep.
	void OnPreLoadGame();
	void OnPostLoadGame();
	// A workstation display clone (excluded from Calamity's loot path; never takeable).
	[[nodiscard]] bool IsDisplayClone(const RE::TESObjectREFR* a_ref);
	// FEASR Runeforge is installed and its contract is active (legacy-UI routing, RC1 section 13).
	[[nodiscard]] bool IsFeasrRuneforgeActive();
}
