#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/SerializationWireContract.h"
#include "CalamityAffixes/LootCurrencySourceHelpers.h"

#include <array>
#include <cstdint>

namespace CalamityAffixes
{
	void EventBridge::Save(SKSE::SerializationInterface* a_intfc)
	{
		static_assert(kSerializationRecordInstanceAffixes == SerializationWire::kInstanceAffixes.type);
		static_assert(kSerializationVersion == SerializationWire::kInstanceAffixes.version);
		static_assert(kSerializationRecordInstanceRuntimeStates == SerializationWire::kInstanceRuntimeStates.type);
		static_assert(kInstanceRuntimeStateSerializationVersion == SerializationWire::kInstanceRuntimeStates.version);
		static_assert(kSerializationRecordRunewordState == SerializationWire::kRunewordState.type);
		static_assert(kRunewordSerializationVersion == SerializationWire::kRunewordState.version);
		static_assert(kSerializationRecordLootEvaluated == SerializationWire::kLootEvaluated.type);
		static_assert(kLootEvaluatedSerializationVersion == SerializationWire::kLootEvaluated.version);
		static_assert(kSerializationRecordLootCurrencyLedger == SerializationWire::kLootCurrencyLedger.type);
		static_assert(kLootCurrencyLedgerSerializationVersion == SerializationWire::kLootCurrencyLedger.version);
		static_assert(kSerializationRecordCorpseCurrencyRuntime == SerializationWire::kCorpseCurrencyRuntime.type);
		static_assert(kCorpseCurrencyRuntimeSerializationVersion == SerializationWire::kCorpseCurrencyRuntime.version);
		static_assert(kSerializationRecordLootShuffleBags == SerializationWire::kLootShuffleBags.type);
		static_assert(kLootShuffleBagSerializationVersion == SerializationWire::kLootShuffleBags.version);
		static_assert(kSerializationRecordMigrationFlags == SerializationWire::kMigrationFlags.type);
		static_assert(kMigrationFlagsVersion == SerializationWire::kMigrationFlags.version);
		static_assert(kSerializationRecordInstanceReforgeCounts == SerializationWire::kInstanceReforgeCounts.type);
		static_assert(kInstanceReforgeCountSerializationVersion == SerializationWire::kInstanceReforgeCounts.version);
		static_assert(kSerializationRecordPendingDropOwnership == SerializationWire::kPendingDropOwnership.type);
		static_assert(kPendingDropOwnershipSerializationVersion == SerializationWire::kPendingDropOwnership.version);

		if (!a_intfc) {
			return;
		}

		const std::scoped_lock lock(_stateMutex);
		// World-reference cleanup is intentionally forbidden here: SKSE invokes
		// this callback from inside SkyrimVM::SaveGlobalData. kSaveGame messaging
		// calls OnPreSaveGame synchronously before the engine save hook instead.
		if (_loot.debugLog) {
			SKSE::log::debug(
				"CalamityAffixes: serialization save trap state (activeTraps={}, unresolvedDeferred={}).",
				_trapState.activeTraps.size(),
				_trapState.PendingMarkerCleanupCount());
		}

		MaybeFlushRuntimeUserSettings(std::chrono::steady_clock::now(), true);
		PruneLootEvaluatedInstances();

		SerializationWire::CurrentSaveSnapshot snapshot{};
		snapshot.instanceAffixes.reserve(_instanceTrackingState.instanceAffixes.size());
		for (const auto& [key, slots] : _instanceTrackingState.instanceAffixes) {
			snapshot.instanceAffixes.push_back(SerializationWire::InstanceAffixEntry{
				.baseFormId = static_cast<std::uint32_t>(key >> 16),
				.uniqueId = static_cast<std::uint16_t>(key & 0xFFFFu),
				.affixCount = slots.count,
				.tokens = slots.tokens,
			});
		}

		snapshot.instanceRuntimeStates.reserve(_instanceTrackingState.instanceStates.size());
		for (const auto& [stateKey, state] : _instanceTrackingState.instanceStates) {
			snapshot.instanceRuntimeStates.push_back(SerializationWire::InstanceRuntimeStateEntry{
				.baseFormId = static_cast<std::uint32_t>(stateKey.instanceKey >> 16),
				.uniqueId = static_cast<std::uint16_t>(stateKey.instanceKey & 0xFFFFu),
				.affixToken = stateKey.affixToken,
				.evolutionXp = state.evolutionXp,
				.modeCycleCounter = state.modeCycleCounter,
				.modeIndex = state.modeIndex,
			});
		}

		if (_runewordState.selectedBaseKey) {
			snapshot.selectedRunewordBaseFormId =
				static_cast<std::uint32_t>(*_runewordState.selectedBaseKey >> 16);
			snapshot.selectedRunewordUniqueId =
				static_cast<std::uint16_t>(*_runewordState.selectedBaseKey & 0xFFFFu);
		}
		snapshot.runewordRecipeCycleCursor = _runewordState.recipeCycleCursor;
		snapshot.runewordBaseCycleCursor = _runewordState.baseCycleCursor;
		snapshot.runewordFragments.reserve(_runewordState.runeFragments.size());
		for (const auto& [runeToken, amount] : _runewordState.runeFragments) {
			snapshot.runewordFragments.push_back({ .runeToken = runeToken, .amount = amount });
		}
		snapshot.runewordInstances.reserve(_runewordState.instanceStates.size());
		for (const auto& [instanceKey, state] : _runewordState.instanceStates) {
			snapshot.runewordInstances.push_back(SerializationWire::RunewordInstanceEntry{
				.baseFormId = static_cast<std::uint32_t>(instanceKey >> 16),
				.uniqueId = static_cast<std::uint16_t>(instanceKey & 0xFFFFu),
				.recipeToken = state.recipeToken,
				.insertedRunes = state.insertedRunes,
			});
		}

		snapshot.lootEvaluated.reserve(_lootState.evaluatedInstances.size());
		for (const auto key : _lootState.evaluatedInstances) {
			snapshot.lootEvaluated.push_back({
				.baseFormId = static_cast<std::uint32_t>(key >> 16),
				.uniqueId = static_cast<std::uint16_t>(key & 0xFFFFu),
			});
		}

		snapshot.lootCurrencyLedger.reserve(_lootState.currencyRollLedger.size());
		for (const auto& [key, dayStamp] : _lootState.currencyRollLedger) {
			snapshot.lootCurrencyLedger.push_back({ .instanceKey = key, .dayStamp = dayStamp });
		}

		snapshot.runewordFragmentFailStreak = _lootState.runewordFragmentFailStreak;
		snapshot.reforgeOrbFailStreak = _lootState.reforgeOrbFailStreak;
		snapshot.corpseCurrencyLedger.reserve(_lootState.corpseCurrencyRollLedger.size());
		for (const auto& [corpseFormId, entry] : _lootState.corpseCurrencyRollLedger) {
			snapshot.corpseCurrencyLedger.push_back({
				.corpseFormId = corpseFormId,
				.dayStamp = entry.dayStamp,
				.processedMask = entry.processedMask,
			});
		}

		const std::array<std::pair<std::uint8_t, const LootShuffleBagState*>, 6> kBags{ {
			{ 0u, &_lootState.prefixSharedBag },
			{ 1u, &_lootState.prefixWeaponBag },
			{ 2u, &_lootState.prefixArmorBag },
			{ 3u, &_lootState.suffixSharedBag },
			{ 4u, &_lootState.suffixWeaponBag },
			{ 5u, &_lootState.suffixArmorBag },
		} };
		snapshot.lootShuffleBags.reserve(kBags.size());
		for (const auto& [id, bag] : kBags) {
			if (!bag) {
				return;
			}
			SerializationWire::LootShuffleBagEntry wireBag{
				.id = id,
				.cursor = static_cast<std::uint32_t>(std::min<std::size_t>(bag->cursor, bag->order.size())),
			};
			wireBag.order.reserve(bag->order.size());
			for (const auto index : bag->order) {
				wireBag.order.push_back(static_cast<std::uint32_t>(index));
			}
			snapshot.lootShuffleBags.push_back(std::move(wireBag));
		}

		snapshot.migrationFlags = (_miscCurrencyMigrated ? 1u : 0u)
			| (_miscCurrencyRecovered ? 2u : 0u);

		// Only items that still carry affixes keep a count; a stale entry would
		// otherwise price a future item that reuses the same instance key.
		snapshot.instanceReforgeCounts.reserve(_instanceTrackingState.selectedReforgeCounts.size());
		for (const auto& [key, count] : _instanceTrackingState.selectedReforgeCounts) {
			if (count == 0u || !_instanceTrackingState.instanceAffixes.contains(key)) {
				continue;
			}
			snapshot.instanceReforgeCounts.push_back({
				.baseFormId = static_cast<std::uint32_t>(key >> 16),
				.uniqueId = static_cast<std::uint16_t>(key & 0xFFFFu),
				.count = count,
			});
		}
		snapshot.pendingDropOwnershipOverflow = _lootState.pendingDropOwnershipOverflow;
		snapshot.pendingDropOwnershipOverflowExpiresAtDay =
			_lootState.pendingDropOwnershipOverflowExpiresAtDay;
		snapshot.pendingDropOwnership.reserve(_lootState.pendingDropOwnership.size());
		for (const auto& pending : _lootState.pendingDropOwnership) {
			if (pending.instanceKey == 0u || pending.baseObjectFormID == 0u ||
				!IsPendingDropOwnershipLive(pending, detail::GetInGameDayStamp())) {
				continue;
			}
			snapshot.pendingDropOwnership.push_back({
				.ownerFormId = static_cast<std::uint32_t>(pending.instanceKey >> 16u),
				.uniqueId = static_cast<std::uint16_t>(pending.instanceKey & 0xFFFFu),
				.baseFormId = pending.baseObjectFormID,
				.expiresAtDay = pending.expiresAtDay,
				.referenceFormId = pending.referenceFormID,
				.referenceIdentityEstablished = static_cast<std::uint8_t>(pending.referenceIdentityEstablished ? 1u : 0u),
			});
		}

		auto openRecord = [a_intfc](std::uint32_t a_type, std::uint32_t a_version) {
			return a_intfc->OpenRecord(a_type, a_version);
		};
		auto writeScalar = [a_intfc]<std::unsigned_integral T>(T a_value) {
			return a_intfc->WriteRecordData(a_value);
		};
		if (!SerializationWire::WriteCurrentRecords(snapshot, openRecord, writeScalar)) {
			return;
		}
	}
}
