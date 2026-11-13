#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/Hooks.h"
#include "CalamityAffixes/InstanceKeyTransferPolicy.h"
#include "CalamityAffixes/LootCurrencySourceHelpers.h"
#include "CalamityAffixes/PluginEditorIds.h"
#include "CalamityAffixes/PrismaTooltip.h"
#include "EventBridge.Loot.Runeword.Detail.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace CalamityAffixes
{
	void EventBridge::Revert(SKSE::SerializationInterface*)
	{
		const std::scoped_lock lock(_stateMutex);
		_eventDispatcher.Invalidate();

		// Remove any active passive suffix spells before clearing
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (player) {
			for (auto* spell : _instanceTrackingState.appliedPassiveSpells) {
				player->RemoveSpell(spell);
			}
		}
		_instanceTrackingState.appliedPassiveSpells.clear();
		_instanceTrackingState.instanceAffixes.clear();
		_instanceTrackingState.instanceBaseObjects.clear();
		_instanceTrackingState.selectedReforgeCounts.clear();
		_instanceTrackingState.equippedInstanceKeysByToken.clear();
		_instanceTrackingState.equippedTokenCacheReady = false;
		_lootState.ResetForLoadOrRevert();
		_affixRuntimeState.activeCounts.clear();
		_affixRuntimeState.activeCritDamageBonusPct = 0.0f;
		_affixRuntimeState.activeHitTriggerAffixIndices.clear();
		_affixRuntimeState.activeIncomingHitTriggerAffixIndices.clear();
		_affixRuntimeState.activeDotApplyTriggerAffixIndices.clear();
		_affixRuntimeState.activeKillTriggerAffixIndices.clear();
		_affixRuntimeState.activeLowHealthTriggerAffixIndices.clear();
		_instanceTrackingState.instanceStates.clear();
		_runewordState.ResetSelectionAndProgress();
		_feasrKnownRunewordRecipeIds.clear();
		_feasrDiscoveryRevision.clear();
		_feasrDiscoveryExpectedCount = 0u;
		_feasrDiscoveryReceivedCount = 0u;
		_feasrDiscoverySyncActive = false;
		_feasrDiscoverySyncValid = false;
		_feasrDiscoveryReady = false;
		ClearTrapRuntimeState("revert", true);
		_combatState.ResetCorpseExplosionState();
		_combatState.ResetTransientState();
		_miscCurrencyMigrated = false;
		_miscCurrencyRecovered = false;

		for (auto& affix : _affixRuntimeState.affixes) {
			affix.nextAllowed = {};
		}

		Hooks::InvalidateDeferredTasks();
		Hooks::ClearRuntimeState();
	}

	void EventBridge::RebuildInstanceBaseObjectIndexFromPlayerInventory()
	{
		_instanceTrackingState.instanceBaseObjects.clear();

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		auto* changes = player->GetInventoryChanges();
		if (!changes || !changes->entryList) {
			return;
		}

		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->object || !entry->extraLists) {
				continue;
			}
			const auto baseObjectFormID = entry->object->GetFormID();
			if (baseObjectFormID == 0u) {
				continue;
			}
			for (auto* xList : *entry->extraLists) {
				const auto* uid = xList ? xList->GetByType<RE::ExtraUniqueID>() : nullptr;
				if (!uid || uid->baseID == 0u || uid->uniqueID == 0u) {
					continue;
				}
				const auto key = MakeInstanceKey(uid->baseID, uid->uniqueID);
				if (HasMaterialInstanceState(key)) {
					_instanceTrackingState.instanceBaseObjects[key] = baseObjectFormID;
				}
			}
		}
	}

	void EventBridge::OnPostLoadGame()
	{
		const std::scoped_lock lock(_stateMutex);
		// Also discard events captured during world restoration, not just tasks
		// already pending when PreLoad/serialization started.
		_eventDispatcher.Invalidate();
		(void)NormalizeLegacyPlayerInstanceKeys();
		RebuildInstanceBaseObjectIndexFromPlayerInventory();
		ReconcileLoadedPendingDropOwnership();
		(void)PruneOrphanedPlayerInstanceKeys();
		// Serialization Load runs before inventory restoration. Always rebuild at
		// PostLoadGame so normal saves cannot retain an empty/stale equipped cache.
		RebuildActiveCounts(true);
		MaybeMigrateMiscCurrency();
	}

	void EventBridge::OnPreLoadGame()
	{
		const std::scoped_lock lock(_stateMutex);
		_eventDispatcher.Invalidate();
		_lootState.pendingDropOwnership.clear();
		_lootState.pendingDropOwnershipOverflow = false;
		_lootState.pendingDropOwnershipOverflowExpiresAtDay = 0u;
		_feasrKnownRunewordRecipeIds.clear();
		_feasrDiscoveryRevision.clear();
		_feasrDiscoveryExpectedCount = 0u;
		_feasrDiscoveryReceivedCount = 0u;
		_feasrDiscoverySyncActive = false;
		_feasrDiscoverySyncValid = false;
		_feasrDiscoveryReady = false;
		ClearTrapRuntimeState("pre-load", true);
		_combatState.echoStrike.Reset();
		Hooks::InvalidateDeferredTasks();
		Hooks::ClearRuntimeState();
	}

	void EventBridge::ExpirePendingDropOwnership(std::uint32_t a_currentDay)
	{
		const auto expired = CalamityAffixes::ExpirePendingDropOwnership(
			_lootState.pendingDropOwnership,
			a_currentDay);
		auto* player = RE::PlayerCharacter::GetSingleton();
		const auto playerID = player ? player->GetFormID() : 0u;
		for (const auto key : expired) {
			if (key == 0u || (playerID != 0u && PlayerHasInstanceKey(key))) {
				continue;
			}
			DiscardInstanceKeyState(key);
			SKSE::log::warn(
				"CalamityAffixes: expired unresolved drop ownership; discarded detached state rather than risk stale reattachment (key={:016X}, day={}).",
				key,
				a_currentDay);
		}
		if (_lootState.pendingDropOwnershipOverflow &&
			a_currentDay >= _lootState.pendingDropOwnershipOverflowExpiresAtDay) {
			_lootState.pendingDropOwnershipOverflow = false;
			_lootState.pendingDropOwnershipOverflowExpiresAtDay = 0u;
		}
	}

	void EventBridge::ReconcileLoadedPendingDropOwnership()
	{
		const auto currentDay = detail::GetInGameDayStamp();
		ExpirePendingDropOwnership(currentDay);
		auto* player = RE::PlayerCharacter::GetSingleton();
		const auto playerID = player ? player->GetFormID() : 0u;
		std::erase_if(_lootState.pendingDropOwnership, [&](const auto& a_entry) {
			if (a_entry.instanceKey == 0u || a_entry.baseObjectFormID == 0u ||
				!HasMaterialInstanceState(a_entry.instanceKey)) {
				return true;
			}
			if (playerID != 0u &&
				static_cast<std::uint32_t>(a_entry.instanceKey >> 16u) == playerID &&
				PlayerHasExactDropPickupIdentity(a_entry.instanceKey, a_entry.baseObjectFormID)) {
				return true;
			}
			return false;
		});
	}

	void EventBridge::OnPreSaveGame()
	{
		const std::scoped_lock lock(_stateMutex);
		// Ground traps are transient and are not serialized: saving deliberately
		// cancels every active trap together with its visual reference. SKSE
		// dispatches kSaveGame before the engine SaveGame hook target. This
		// is the only save lifecycle point where mutating dynamic world references
		// is allowed; the SerializationInterface Save callback is already inside
		// SkyrimVM::SaveGlobalData and must remain read/write-co-save only.
		ClearTrapRuntimeState("pre-save");
		if (_loot.debugLog) {
			SKSE::log::debug(
				"CalamityAffixes: pre-save trap cleanup complete (activeTraps={}, unresolvedDeferred={}).",
				_trapState.activeTraps.size(),
				_trapState.PendingMarkerCleanupCount());
		}
	}

	bool EventBridge::NormalizeLegacyPlayerInstanceKeys()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return false;
		}

		auto* changes = player->GetInventoryChanges();
		if (!changes || !changes->entryList) {
			return false;
		}

		const auto playerID = player->GetFormID();
		std::unordered_map<std::uint16_t, std::uint32_t> uidUseCounts;
		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->extraLists) {
				continue;
			}
			for (auto* xList : *entry->extraLists) {
				const auto* uid = xList ? xList->GetByType<RE::ExtraUniqueID>() : nullptr;
				if (uid && uid->uniqueID != 0u) {
					++uidUseCounts[uid->uniqueID];
				}
			}
		}

		std::uint32_t normalized = 0u;
		std::uint32_t remapped = 0u;
		std::uint32_t conflicts = 0u;
		std::uint32_t discarded = 0u;
		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->object || !entry->extraLists) {
				continue;
			}

			const auto itemFormID = entry->object->GetFormID();
			if (itemFormID == 0u || itemFormID == playerID) {
				continue;
			}

			for (auto* xList : *entry->extraLists) {
				if (!xList) {
					continue;
				}

				auto* uid = xList->GetByType<RE::ExtraUniqueID>();
				if (!uid || uid->uniqueID == 0u || uid->baseID != itemFormID) {
					continue;
				}

				const auto legacyKey = MakeInstanceKey(itemFormID, uid->uniqueID);
				const auto canonicalKey = MakeInstanceKey(playerID, uid->uniqueID);
				const bool legacyTracked = IsInstanceKeyTracked(legacyKey);
				const bool canonicalTracked = IsInstanceKeyTracked(canonicalKey);
				const auto uidCountIt = uidUseCounts.find(uid->uniqueID);
				const bool duplicateUID = uidCountIt != uidUseCounts.end() && uidCountIt->second > 1u;
				if (duplicateUID) {
					++conflicts;
					if (legacyTracked) {
						DiscardInstanceKeyState(legacyKey);
						++discarded;
					}
					if (canonicalTracked) {
						DiscardInstanceKeyState(canonicalKey);
						++discarded;
					}
					SKSE::log::warn(
						"CalamityAffixes: legacy player instance normalization discarded duplicate-UID state (item={:08X}, uid={}, legacyTracked={}, ownerTracked={}, legacy={:016X}, owner={:016X}).",
						itemFormID,
						uid->uniqueID,
						legacyTracked,
						canonicalTracked,
						legacyKey,
						canonicalKey);
					continue;
				}
				if (canonicalTracked) {
					// The xList itself proves this is a v1.2.21 item-form identity.
					// A pre-existing owner key is therefore ambiguous and must not be
					// allowed to attach to this item during normalization.
					DiscardInstanceKeyState(canonicalKey);
					++discarded;
				}

				if (legacyTracked) {
					RemapInstanceKey(legacyKey, canonicalKey);
					++remapped;
				}
				uid->baseID = playerID;
				++normalized;
			}
		}

		if (normalized > 0u || conflicts > 0u || discarded > 0u) {
			SKSE::log::info(
				"CalamityAffixes: legacy player instance normalization complete (normalized={}, remapped={}, conflicts={}, discarded={}).",
				normalized,
				remapped,
				conflicts,
				discarded);
		}
		return remapped > 0u || discarded > 0u;
	}

	bool EventBridge::PruneOrphanedPlayerInstanceKeys()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return false;
		}

		auto* changes = player->GetInventoryChanges();
		if (!changes || !changes->entryList) {
			return false;
		}

		const auto playerID = player->GetFormID();
		std::unordered_set<std::uint64_t> inventoryKeys;
		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->extraLists) {
				continue;
			}
			for (auto* xList : *entry->extraLists) {
				const auto* uid = xList ? xList->GetByType<RE::ExtraUniqueID>() : nullptr;
				if (uid && uid->baseID != 0u && uid->uniqueID != 0u) {
					inventoryKeys.insert(MakeInstanceKey(uid->baseID, uid->uniqueID));
				}
			}
		}

		std::unordered_set<std::uint64_t> materialKeys;
		for (const auto& [key, _] : _instanceTrackingState.instanceAffixes) {
			materialKeys.insert(key);
		}
		for (const auto& [stateKey, _] : _instanceTrackingState.instanceStates) {
			materialKeys.insert(stateKey.instanceKey);
		}
		for (const auto& [key, _] : _runewordState.instanceStates) {
			materialKeys.insert(key);
		}
		if (_runewordState.selectedBaseKey) {
			materialKeys.insert(*_runewordState.selectedBaseKey);
		}

		std::uint32_t pruned = 0u;
		const auto currentDay = detail::GetInGameDayStamp();
		for (const auto key : materialKeys) {
			if (_lootState.pendingDropOwnershipOverflow &&
				currentDay < _lootState.pendingDropOwnershipOverflowExpiresAtDay &&
				static_cast<std::uint32_t>(key >> 16u) == playerID) {
				continue;
			}
			if (IsPendingDropOrphanProtected(key, _lootState.pendingDropOwnership, currentDay)) {
				continue;
			}
			if (!IsOrphanedPlayerInstanceKey(key, playerID, inventoryKeys.contains(key))) {
				continue;
			}
			DiscardInstanceKeyState(key);
			++pruned;
		}

		if (pruned > 0u) {
			SKSE::log::warn(
				"CalamityAffixes: discarded {} orphaned player-owned instance state(s) during ownership reconciliation; this prevents stale weapon-rack UIDs from attaching to unrelated items.",
				pruned);
		}
		return pruned > 0u;
	}

	void EventBridge::OnFormDelete(RE::VMHandle a_handle)
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			return;
		}

		auto* policy = vm->GetObjectHandlePolicy();
		if (!policy) {
			return;
		}

		if (!policy->HandleIsType(RE::FormType::Reference, a_handle)) {
			return;
		}

		auto* form = policy->GetObjectForHandle(RE::FormType::Reference, a_handle);
		auto* ref = form ? form->As<RE::TESObjectREFR>() : nullptr;
		if (!ref) {
			return;
		}

		const auto refHandle = static_cast<LootRerollGuard::RefHandle>(ref->GetHandle().native_handle());
		if (refHandle == 0) {
			return;
		}
		const auto detachedWorldKey = MakeDetachedWorldInstanceKey(ref->GetFormID());

		{
			const std::scoped_lock lock(_stateMutex);
			_lootState.pendingDroppedRefDeletes.push_back(LootRuntimeState::PendingDroppedRefDelete{
				.refHandle = refHandle,
				.detachedWorldKey = detachedWorldKey
			});
		}
		ScheduleDroppedRefDeleteDrain();
	}

	void EventBridge::ScheduleDroppedRefDeleteDrain()
	{
		{
			const std::scoped_lock lock(_stateMutex);
			if (_lootState.pendingDroppedRefDeletes.empty()) {
				return;
			}
		}

		bool expected = false;
		if (!_lootState.dropDeleteDrainScheduled.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
			return;
		}

		if (auto* task = SKSE::GetTaskInterface()) {
			task->AddTask([]() {
				if (auto* bridge = EventBridge::GetSingleton(); bridge) {
					bridge->DrainPendingDroppedRefDeletes();
				}
			});
			return;
		}

		// Task interface unavailable: keep pending queue for later safe drain.
		_lootState.dropDeleteDrainScheduled.store(false, std::memory_order_release);
	}

	void EventBridge::DrainPendingDroppedRefDeletes()
	{
		std::vector<LootRuntimeState::PendingDroppedRefDelete> pending;
		{
			const std::scoped_lock lock(_stateMutex);
			pending.swap(_lootState.pendingDroppedRefDeletes);
		}

		for (const auto& pendingDelete : pending) {
			ProcessDroppedRefDeleted(pendingDelete.refHandle, pendingDelete.detachedWorldKey);
		}

		_lootState.dropDeleteDrainScheduled.store(false, std::memory_order_release);

		bool hasMorePending = false;
		{
			const std::scoped_lock lock(_stateMutex);
			hasMorePending = !_lootState.pendingDroppedRefDeletes.empty();
		}
		if (hasMorePending) {
			ScheduleDroppedRefDeleteDrain();
		}
	}

	void EventBridge::EraseInstanceRuntimeStates(std::uint64_t a_instanceKey)
	{
		std::erase_if(_instanceTrackingState.instanceStates, [a_instanceKey](const auto& entry) {
			return entry.first.instanceKey == a_instanceKey;
		});
	}

	void EventBridge::ProcessDroppedRefDeleted(
		LootRerollGuard::RefHandle a_refHandle,
		std::uint64_t a_detachedWorldKey)
	{
		const std::scoped_lock lock(_stateMutex);

		const auto keyOpt = _lootState.rerollGuard.ConsumeIfPlayerDropDeleted(a_refHandle);
		if (keyOpt && !IsDetachedWorldReferenceMatch(*keyOpt, a_detachedWorldKey)) {
			SKSE::log::warn(
				"CalamityAffixes: rejected recycled drop-reference handle during delete cleanup (guardedKey={:016X}, referenceKey={:016X}).",
				*keyOpt,
				a_detachedWorldKey);
		}
		const auto key = keyOpt && IsDetachedWorldReferenceMatch(*keyOpt, a_detachedWorldKey) ?
			*keyOpt :
			a_detachedWorldKey;
		if (key == 0u || !IsInstanceKeyTracked(key)) {
			return;
		}
		std::erase_if(_lootState.pendingDropOwnership, [key](const auto& a_entry) {
			return a_entry.instanceKey == key;
		});

		// Safety net: if the instance is already back in the player inventory, don't prune it.
		if (PlayerHasInstanceKey(key)) {
			return;
		}

		DiscardInstanceKeyState(key);
		if (_loot.debugLog) {
			SKSE::log::debug("CalamityAffixes: pruned instance affix (world ref deleted) (key={:016X}).", key);
		}
	}

	bool EventBridge::PlayerHasInstanceKey(std::uint64_t a_instanceKey) const
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return false;
		}

		auto* changes = player->GetInventoryChanges();
		if (!changes || !changes->entryList) {
			return false;
		}

		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->extraLists) {
				continue;
			}

			for (auto* xList : *entry->extraLists) {
				if (!xList) {
					continue;
				}

				const auto* uid = xList->GetByType<RE::ExtraUniqueID>();
				if (!uid) {
					continue;
				}

				if (MakeInstanceKey(uid->baseID, uid->uniqueID) == a_instanceKey) {
					return true;
				}
			}
		}

		return false;
	}

	bool EventBridge::PlayerHasExactDropPickupIdentity(
		std::uint64_t a_instanceKey,
		RE::FormID a_baseObjectFormID) const
	{
		if (a_instanceKey == 0u || a_baseObjectFormID == 0u) {
			return false;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* changes = player ? player->GetInventoryChanges() : nullptr;
		if (!changes || !changes->entryList) {
			return false;
		}

		std::vector<DropPickupInventoryIdentity> observations;
		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->object || entry->object->GetFormID() != a_baseObjectFormID || !entry->extraLists) {
				continue;
			}
			for (auto* xList : *entry->extraLists) {
				if (!xList) {
					continue;
				}
				const auto* uid = xList->GetByType<RE::ExtraUniqueID>();
				if (!uid) {
					continue;
				}
				observations.push_back({
					MakeInstanceKey(uid->baseID, uid->uniqueID),
					entry->object->GetFormID(),
					xList->GetCount()
				});
			}
		}
		return HasUniqueExactDropPickupInventoryIdentity(observations, a_instanceKey, a_baseObjectFormID);
	}

	void EventBridge::MaybeMigrateMiscCurrency()
	{
		// Automatic migration disabled — use MCM "Recover Currency" button instead.
		_miscCurrencyMigrated = true;
	}

	std::string EventBridge::RecoverMiscCurrency()
	{
		const std::scoped_lock lock(_stateMutex);

		if (_miscCurrencyRecovered) {
			// Follows the UI language setting like the tooltip text (0 = English, 1 = Korean, 2 = both).
			switch (PrismaTooltip::GetUiLanguageMode()) {
			case 0:
				return "Already recovered this save.";
			case 1:
				return "이미 복구 완료된 세이브입니다.";
			default:
				return "이미 복구 완료된 세이브입니다. / Already recovered this save.";
			}
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return "Player not available.";
		}

		auto* orb = PluginEditorIds::Lookup<RE::TESObjectMISC>("CAFF_Misc_ReforgeOrb");
		if (!orb) {
			SKSE::log::warn("CalamityAffixes: RecoverMiscCurrency — reforge orb EditorID lookup failed.");
			return "Reforge orb lookup failed.";
		}

		// Grant reforge orbs.
		static constexpr std::uint32_t kRecoveryOrbGrant = 5u;
		player->AddObjectToContainer(orb, nullptr,
			static_cast<std::int32_t>(kRecoveryOrbGrant), nullptr);

		// Grant rune fragments: give 1 of each known rune type the player doesn't own.
		if (_runewordState.runeNameByToken.empty()) {
			InitializeRunewordCatalog();
		}
		std::uint32_t fragmentsGranted = 0u;
		for (const auto& [runeToken, runeName] : _runewordState.runeNameByToken) {
			if (runeToken == 0u || runeName.empty()) {
				continue;
			}
			const auto owned = RunewordDetail::GetOwnedRunewordFragmentCount(
				player, _runewordState.runeNameByToken, runeToken);
			if (owned == 0u) {
				const auto given = RunewordDetail::GrantRunewordFragments(
					player, _runewordState.runeNameByToken, runeToken, 1u);
				fragmentsGranted += given;
			}
		}

		_miscCurrencyRecovered = true;

		SKSE::log::info(
			"CalamityAffixes: RecoverMiscCurrency — granted {} orbs and {} rune fragment types.",
			kRecoveryOrbGrant, fragmentsGranted);
		EmitHudNotification("Calamity: recovered missing currency items.");

		std::string result = "Granted ";
		result.append(std::to_string(kRecoveryOrbGrant));
		result.append(" orbs, ");
		result.append(std::to_string(fragmentsGranted));
		result.append(" fragment types.");
		return result;
	}
}
