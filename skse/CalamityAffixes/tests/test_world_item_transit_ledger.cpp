#include "CalamityAffixes/WorldItemTransitLedger.h"

namespace
{
	using CalamityAffixes::ChooseReturnedItem;
	using CalamityAffixes::IsPlayerItemBecomingWorldReference;
	using CalamityAffixes::ReturnedItemChoice;
	using CalamityAffixes::WorldItemTransit;
	using CalamityAffixes::WorldItemTransitLedger;

	constexpr std::uint32_t kPlayer = 0x14u;

	// The log line from the Temper and Recharge Services drop (2026-11-06):
	// oldOwner=00000014, newOwner=FF003736, oldUid=11, newUid=0.
	static_assert(IsPlayerItemBecomingWorldReference(kPlayer, kPlayer, 0xFF003736u, 11u, 0u));
	// A first unique ID (nothing to move), a transfer that keeps a unique ID, and
	// another owner's item are not this shape.
	static_assert(!IsPlayerItemBecomingWorldReference(kPlayer, 0u, kPlayer, 0u, 4u));
	static_assert(!IsPlayerItemBecomingWorldReference(kPlayer, kPlayer, 0x0010FDE7u, 11u, 3u));
	static_assert(!IsPlayerItemBecomingWorldReference(kPlayer, 0x0010FDE7u, 0xFF003736u, 2u, 0u));
	static_assert(!IsPlayerItemBecomingWorldReference(kPlayer, kPlayer, kPlayer, 11u, 0u));

	constexpr bool CheckPickupWithoutReferenceFindsTheOnlyTransit()
	{
		WorldItemTransitLedger ledger{};
		ledger.Record({ .baseFormID = 0x12EB6u, .referenceFormID = 0xFF003736u, .detachedKey = 0xAAu });
		const auto found = ledger.Find(0x12EB6u, 0u, 1000);
		return found && found->detachedKey == 0xAAu && !ledger.Find(0x12EB7u, 0u, 1000);
	}
	static_assert(CheckPickupWithoutReferenceFindsTheOnlyTransit());

	constexpr bool CheckTwoTransitsOfOneBaseNeedTheReference()
	{
		WorldItemTransitLedger ledger{};
		ledger.Record({ .baseFormID = 0x12EB6u, .referenceFormID = 0xFF000001u, .detachedKey = 0xA1u });
		ledger.Record({ .baseFormID = 0x12EB6u, .referenceFormID = 0xFF000002u, .detachedKey = 0xA2u });
		const auto byReference = ledger.Find(0x12EB6u, 0xFF000002u, 1000);
		return !ledger.Find(0x12EB6u, 0u, 1000) && byReference && byReference->detachedKey == 0xA2u &&
		       !ledger.Find(0x12EB6u, 0xFF000003u, 1000);
	}
	static_assert(CheckTwoTransitsOfOneBaseNeedTheReference());

	constexpr bool CheckForgetAndCapacity()
	{
		WorldItemTransitLedger ledger{};
		for (std::uint32_t i = 0; i < WorldItemTransitLedger::kCapacity + 3u; ++i) {
			ledger.Record({ .baseFormID = 0x100u + i, .referenceFormID = 0xFF000000u + i, .detachedKey = 0x1000u + i });
		}
		// The oldest three were dropped to stay within capacity.
		const bool bounded = ledger.Size() == WorldItemTransitLedger::kCapacity && !ledger.Find(0x100u, 0u, 1000) &&
		                     ledger.Find(0x103u, 0u, 1000).has_value();
		ledger.Forget(0x1003u);
		return bounded && !ledger.Find(0x103u, 0u, 1000) && ledger.Size() == WorldItemTransitLedger::kCapacity - 1u;
	}
	static_assert(CheckForgetAndCapacity());

	constexpr bool CheckTheEarlyDeleteSignalOnlyCountsInsideTheWindow()
	{
		WorldItemTransitLedger ledger{};
		ledger.Record({ .baseFormID = 0x12EB6u, .referenceFormID = 0xFF003761u, .detachedKey = 0xAAu, .recordedAtMs = 1000 });
		// The delete signal 16 ms after the drop, and one after the item had long
		// been left on the ground.
		return ledger.IsAwaitingReturn(0xAAu, 1016) &&
		       ledger.IsAwaitingReturn(0xAAu, 1000 + WorldItemTransitLedger::kReturnWindowMs) &&
		       !ledger.IsAwaitingReturn(0xAAu, 1001 + WorldItemTransitLedger::kReturnWindowMs) &&
		       !ledger.IsAwaitingReturn(0xBBu, 1016);
	}
	static_assert(CheckTheEarlyDeleteSignalOnlyCountsInsideTheWindow());

	constexpr bool CheckIncompleteTransitsAreNotRecorded()
	{
		WorldItemTransitLedger ledger{};
		ledger.Record({ .baseFormID = 0x12EB6u, .referenceFormID = 0u, .detachedKey = 0xAAu });
		ledger.Record({ .baseFormID = 0x12EB6u, .referenceFormID = 0xFF003736u, .detachedKey = 0u });
		return ledger.Size() == 0u;
	}
	static_assert(CheckIncompleteTransitsAreNotRecorded());
}

namespace
{
	constexpr bool CheckBaseOnlyMatchesStayInsideTheWindow()
	{
		WorldItemTransitLedger ledger{};
		ledger.Record({ .baseFormID = 0x12EB6u, .referenceFormID = 0xFF003760u, .detachedKey = 0xAAu, .recordedAtMs = 1000 });
		const auto window = WorldItemTransitLedger::kReturnWindowMs;
		// An item left on the ground longer than the window is not claimed by a
		// same-base item arriving with no reference; a named reference still is.
		return ledger.Find(0x12EB6u, 0u, 2500).has_value() &&
		       !ledger.Find(0x12EB6u, 0u, 1001 + window).has_value() &&
		       ledger.Find(0x12EB6u, 0xFF003760u, 1001 + window).has_value();
	}
	static_assert(CheckBaseOnlyMatchesStayInsideTheWindow());

	constexpr bool CheckAHeldBackDeleteIsSweptAfterTheWindow()
	{
		WorldItemTransitLedger ledger{};
		ledger.Record({ .baseFormID = 0x1u, .referenceFormID = 0xFF000001u, .detachedKey = 0xA1u, .recordedAtMs = 1000 });
		ledger.Record({ .baseFormID = 0x2u, .referenceFormID = 0xFF000002u, .detachedKey = 0xA2u, .recordedAtMs = 1000 });
		ledger.MarkDeleteSeen(0xA1u);
		std::array<std::uint64_t, WorldItemTransitLedger::kCapacity> out{};
		const auto window = WorldItemTransitLedger::kReturnWindowMs;
		// Inside the window nothing is abandoned yet.
		const bool early = ledger.TakeAbandoned(1000 + window, out) == 0u;
		// After it, only the one whose delete was held back goes; the other was
		// never deleted (an item still on the ground) and stays.
		const auto taken = ledger.TakeAbandoned(1001 + window, out);
		return early && taken == 1u && out[0] == 0xA1u && ledger.Size() == 1u && !ledger.IsAwaitingReturn(0xA1u, 1016);
	}
	static_assert(CheckAHeldBackDeleteIsSweptAfterTheWindow());

	// Which copy a returned item is: the one still naming its reference, else
	// the single copy with no stable unique ID, else none.
	static_assert(ChooseReturnedItem(1u, 0u) == ReturnedItemChoice::kExact);
	static_assert(ChooseReturnedItem(1u, 3u) == ReturnedItemChoice::kExact);
	static_assert(ChooseReturnedItem(0u, 1u) == ReturnedItemChoice::kLoose);
	static_assert(ChooseReturnedItem(0u, 2u) == ReturnedItemChoice::kNone);
	static_assert(ChooseReturnedItem(2u, 0u) == ReturnedItemChoice::kNone);
	static_assert(ChooseReturnedItem(0u, 0u) == ReturnedItemChoice::kNone);
}
