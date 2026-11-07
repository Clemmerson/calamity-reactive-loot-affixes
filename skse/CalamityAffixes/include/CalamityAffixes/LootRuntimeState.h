#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "CalamityAffixes/InstanceAffixSlots.h"
#include "CalamityAffixes/LootRerollGuard.h"
#include "CalamityAffixes/LootShuffleBagState.h"
#include "CalamityAffixes/WorldItemTransitLedger.h"

namespace CalamityAffixes
{
	struct LootRuntimeState
	{
		struct PendingDroppedRefDelete
		{
			LootRerollGuard::RefHandle refHandle{ 0 };
			LootRerollGuard::InstanceKey detachedWorldKey{ 0 };
		};

		struct CorpseCurrencyLedgerEntry
		{
			std::uint32_t dayStamp{ 0u };
			std::uint8_t processedMask{ 0u };
		};

		LootShuffleBagState prefixSharedBag{};
		LootShuffleBagState prefixWeaponBag{};
		LootShuffleBagState prefixArmorBag{};
		LootShuffleBagState suffixSharedBag{};
		LootShuffleBagState suffixWeaponBag{};
		LootShuffleBagState suffixArmorBag{};

		std::unordered_map<std::uint64_t, InstanceAffixSlots> previewAffixes{};
		std::deque<std::uint64_t> previewRecent{};

		std::unordered_set<std::uint64_t> evaluatedInstances{};
		std::deque<std::uint64_t> evaluatedRecent{};
		std::size_t evaluatedInsertionsSincePrune{ 0 };

		std::unordered_map<std::uint64_t, std::uint32_t> currencyRollLedger{};
		std::deque<std::uint64_t> currencyRollLedgerRecent{};
		std::unordered_map<std::uint32_t, CorpseCurrencyLedgerEntry> corpseCurrencyRollLedger{};
		std::deque<std::uint32_t> corpseCurrencyRollLedgerRecent{};

		std::uint32_t lootChanceEligibleFailStreak{ 0 };
		std::uint32_t runewordFragmentFailStreak{ 0 };
		std::uint32_t reforgeOrbFailStreak{ 0 };

		// Per-affix item-local proc penalties for equipped duplicate copies. Each
		// inner vector is kept strongest-first and capped by the proc policy.
		std::vector<std::vector<float>> activeProcSlotPenalties{};
		LootRerollGuard rerollGuard{};
		std::vector<PendingDroppedRefDelete> pendingDroppedRefDeletes{};
		std::atomic_bool dropDeleteDrainScheduled{ false };
		std::map<std::pair<LootRerollGuard::FormID, LootRerollGuard::FormID>, std::int32_t> playerContainerStash{};
		WorldItemTransitLedger worldItemTransits{};

		void ResetForConfigReload() noexcept
		{
			prefixSharedBag = {};
			prefixWeaponBag = {};
			prefixArmorBag = {};
			suffixSharedBag = {};
			suffixWeaponBag = {};
			suffixArmorBag = {};
			previewAffixes.clear();
			previewRecent.clear();
			lootChanceEligibleFailStreak = 0;
			activeProcSlotPenalties.clear();
			rerollGuard.Reset();
			playerContainerStash.clear();
			worldItemTransits.Clear();
		}

		void ResetForLoadOrRevert() noexcept
		{
			ResetForConfigReload();
			evaluatedInstances.clear();
			evaluatedRecent.clear();
			evaluatedInsertionsSincePrune = 0;
			currencyRollLedger.clear();
			currencyRollLedgerRecent.clear();
			corpseCurrencyRollLedger.clear();
			corpseCurrencyRollLedgerRecent.clear();
			runewordFragmentFailStreak = 0;
			reforgeOrbFailStreak = 0;
			pendingDroppedRefDeletes.clear();
			dropDeleteDrainScheduled.store(false, std::memory_order_release);
		}
	};
}
