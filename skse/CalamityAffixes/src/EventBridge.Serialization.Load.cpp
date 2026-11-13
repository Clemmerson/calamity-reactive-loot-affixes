#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/LootCurrencySourceHelpers.h"
#include "CalamityAffixes/HostileEffectGuard.h"
#include "CalamityAffixes/SummonProtectionSerialization.h"
#include "CalamityAffixes/LootRollSelection.h"
#include "CalamityAffixes/SerializationCurrentRecordReader.h"
#include "CalamityAffixes/SerializationDrainPolicy.h"
#include "CalamityAffixes/SerializationLoadState.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace CalamityAffixes
{
	namespace
	{
		constexpr std::uint32_t kMaxV1AffixIdLength = 1024u;

		bool DrainRecordBytes(
			SKSE::SerializationInterface* a_intfc,
			std::uint32_t a_length,
			const char* a_context)
		{
			if (!a_intfc || a_length == 0u) {
				return true;
			}

			if (detail::ShouldWarnUnusuallyLargeSerializationDrain(a_length)) {
				SKSE::log::warn(
					"CalamityAffixes: draining unusually large serialization record segment (context={}, bytes={}).",
					a_context,
					a_length);
			}

			std::array<std::uint8_t, detail::kSerializationDrainChunkBytes> sink{};
			std::uint32_t remaining = a_length;
			while (remaining > 0u) {
				const auto chunk = detail::ResolveSerializationDrainChunkSize(remaining);
				const auto read = a_intfc->ReadRecordData(sink.data(), chunk);
				if (read != chunk) {
					SKSE::log::warn(
						"CalamityAffixes: truncated serialization drain (context={}, requested={}, read={}).",
						a_context,
						chunk,
						read);
					return false;
				}
				remaining -= chunk;
			}

			return true;
		}
	}

#include "EventBridge.Serialization.Load.Records.inl"

	void EventBridge::Load(SKSE::SerializationInterface* a_intfc)
	{
		if (!a_intfc) {
			return;
		}

		const std::scoped_lock lock(_stateMutex);
		_eventDispatcher.Invalidate();
		SKSE::log::info("CalamityAffixes: Load() — deserializing co-save records.");

		_instanceTrackingState.instanceAffixes.clear();
		_instanceTrackingState.instanceBaseObjects.clear();
		_instanceTrackingState.selectedReforgeCounts.clear();
		_instanceTrackingState.equippedInstanceKeysByToken.clear();
		_instanceTrackingState.equippedTokenCacheReady = false;
		_lootState.ResetForLoadOrRevert();
		_instanceTrackingState.instanceStates.clear();
		_affixRuntimeState.activeCounts.clear();
		_affixRuntimeState.activeCritDamageBonusPct = 0.0f;
		_affixRuntimeState.activeHitTriggerAffixIndices.clear();
		_affixRuntimeState.activeIncomingHitTriggerAffixIndices.clear();
		_affixRuntimeState.activeDotApplyTriggerAffixIndices.clear();
		_affixRuntimeState.activeKillTriggerAffixIndices.clear();
		_affixRuntimeState.activeLowHealthTriggerAffixIndices.clear();
		_combatState.ResetTransientState();
		ClearTrapRuntimeState("load");
		_runewordState.ResetSelectionAndProgress();
		_feasrKnownRunewordRecipeIds.clear();
		_feasrDiscoveryRevision.clear();
		_feasrDiscoveryExpectedCount = 0u;
		_feasrDiscoveryReceivedCount = 0u;
		_feasrDiscoverySyncActive = false;
		_feasrDiscoverySyncValid = false;
		_feasrDiscoveryReady = false;
		_combatState.ResetCorpseExplosionState();

		std::uint32_t type = 0;
		std::uint32_t version = 0;
		std::uint32_t length = 0;

		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			switch (type) {
			case kCalamitySummonRecord:
				if (SummonProtectionSerialization::IsValidRecord(version, length)) {
					LoadCalamitySummons(a_intfc, version, length);
				} else {
					DrainRecordBytes(a_intfc, length, "unsupported-summon-record");
				}
				break;
			case kSerializationRecordInstanceAffixes:
				LoadInstanceAffixesRecord(a_intfc, version, length);
				break;
			case kSerializationRecordInstanceRuntimeStates:
				LoadInstanceRuntimeStatesRecord(a_intfc, version, length);
				break;
			case kSerializationRecordRunewordState:
				LoadRunewordStateRecord(a_intfc, version, length);
				break;
			case kSerializationRecordLootEvaluated:
				LoadLootEvaluatedRecord(a_intfc, version, length);
				break;
			case kSerializationRecordLootCurrencyLedger:
				LoadLootCurrencyLedgerRecord(a_intfc, version, length);
				break;
			case kSerializationRecordCorpseCurrencyRuntime:
				LoadCorpseCurrencyRuntimeRecord(a_intfc, version, length);
				break;
			case kSerializationRecordLootShuffleBags:
				LoadLootShuffleBagsRecord(a_intfc, version, length);
				break;
			case kSerializationRecordMigrationFlags:
				LoadMigrationFlagsRecord(a_intfc, version, length);
				break;
			case kSerializationRecordInstanceReforgeCounts:
				LoadInstanceReforgeCountsRecord(a_intfc, version, length);
				break;
			case kSerializationRecordPendingDropOwnership:
				LoadPendingDropOwnershipRecord(a_intfc, version, length);
				break;
			default:
				DrainRecordBytes(a_intfc, length, "unknown-record");
				break;
			}
		}

		FinalizeLoadedSerializationState();

		// NOTE: MaybeMigrateMiscCurrency() is intentionally NOT called here.
		// During the SKSE Load callback the game engine has not yet restored the
		// player's inventory, so GetItemCount() returns 0 for all items.
		// The migration check is deferred to OnPostLoadGame() (kPostLoadGame message),
		// where the game state is fully loaded.
	}
}
