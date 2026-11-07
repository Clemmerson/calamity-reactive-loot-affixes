import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SRC = REPO / "skse" / "CalamityAffixes" / "src"


class V232RuntimeGuardTests(unittest.TestCase):
    """An NPC temper service (Temper and Recharge Services) drops the item, tempers
    the world reference and adds it back; the container events name no reference,
    so the affixes were pruned as orphaned (Arca report 2026-11-06). The pure parts
    have static_assert tests in test_world_item_transit_ledger.cpp."""

    def setUp(self) -> None:
        self.loot = (SRC / "EventBridge.Loot.cpp").read_text(encoding="utf-8")

    def test_the_unique_id_change_to_a_world_reference_moves_the_state_first(self) -> None:
        handler = self.loot[self.loot.index("EventBridge::HandleUniqueIDChangeEvent(") :]
        move = handler.index("MovePlayerItemStateToWorldReference(itemFormID, a_event->oldUniqueID, newOwnerID)")
        # Before the generic "incomplete UID boundary" ignore, which used to drop it.
        self.assertLess(move, handler.index("ignored incomplete TESUniqueIDChangeEvent UID boundary"))
        self.assertLess(handler.index("IsPlayerItemBecomingWorldReference("), move)

    def test_the_move_only_takes_the_items_own_world_reference(self) -> None:
        body = self.loot[self.loot.index("bool EventBridge::MovePlayerItemStateToWorldReference(") :]
        body = body[: body.index("bool EventBridge::BindReturnedWorldItemState(")]
        self.assertIn("ref->As<RE::Actor>()", body)
        self.assertIn("base->GetFormID() != a_itemFormID", body)
        self.assertIn("RemapInstanceKey(sourceKey, detachedKey);", body)
        self.assertIn("_lootState.worldItemTransits.Record(", body)

    def test_a_pickup_without_a_reference_finds_the_transit_and_rebinds(self) -> None:
        pickup = self.loot[self.loot.index("// 3) Reattach state only when this is the exact world reference") :]
        pickup = pickup[: pickup.index("// 3b) Skip if this item was stashed")]
        self.assertIn("a_event->oldContainer == LootRerollGuard::kWorldContainer", pickup)
        self.assertIn("a_event->baseObj, referenceFormID, StatusClock::RealNowMs());", pickup)
        self.assertIn("BindReturnedWorldItemState(a_event->baseObj, detachedWorldKey, false)", pickup)
        # A retry runs as a task, ahead of the reference's delete cleanup.
        self.assertIn("task->AddTask([baseObj, retryKey, warnRetained]()", pickup)

    def test_the_rebind_gives_the_returned_item_a_player_unique_id(self) -> None:
        body = self.loot[self.loot.index("bool EventBridge::BindReturnedWorldItemState(") :]
        # The engine leaves (reference, 0) on the dropped item: that copy is this one.
        self.assertIn("uid->baseID == referenceFormID", body)
        self.assertIn("ChooseReturnedItem(exactCount, looseCount)", body)
        self.assertIn("uid->baseID = player->GetFormID();", body)
        self.assertIn("uid->uniqueID = changes->GetNextUniqueID();", body)
        self.assertIn("RemapInstanceKey(a_detachedKey, pickupKey);", body)

    def test_deleting_the_reference_forgets_its_transit(self) -> None:
        lifecycle = (SRC / "EventBridge.Serialization.Lifecycle.cpp").read_text(encoding="utf-8")
        self.assertIn("_lootState.worldItemTransits.Forget(key);", lifecycle)

    def test_the_early_delete_signal_keeps_a_pending_transit(self) -> None:
        # The form-delete callback fired 16 ms after the drop, 2 s before the
        # script added the item back, and pruned the state (log 2026-11-06 22:33).
        lifecycle = (SRC / "EventBridge.Serialization.Lifecycle.cpp").read_text(encoding="utf-8")
        body = lifecycle[lifecycle.index("void EventBridge::ProcessDroppedRefDeleted(") :]
        body = body[: body.index("bool EventBridge::PlayerHasInstanceKey(")]
        self.assertLess(
            body.index("_lootState.worldItemTransits.IsAwaitingReturn(key, StatusClock::RealNowMs())"),
            body.index("DiscardInstanceKeyState(key);"),
        )
        move = self.loot[self.loot.index("bool EventBridge::MovePlayerItemStateToWorldReference(") :]
        self.assertIn(".recordedAtMs = StatusClock::RealNowMs(),", move)
        # A held-back delete that turns out real is swept, not kept forever.
        self.assertIn("_lootState.worldItemTransits.MarkDeleteSeen(key);", body)
        self.assertIn("SweepAbandonedWorldItemTransits();", lifecycle[lifecycle.index("void EventBridge::OnPreSaveGame()") :])


if __name__ == "__main__":
    unittest.main()
