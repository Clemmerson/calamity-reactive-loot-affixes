#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/InstanceKeyTransferPolicy.h"
#include "CalamityAffixes/LootUiGuards.h"
#include "CalamityAffixes/TriggerGuards.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <unordered_set>
#include <vector>

namespace CalamityAffixes
{
	namespace
	{
		[[nodiscard]] bool IsPreviewRemapMenuContextOpen() noexcept
		{
			auto* ui = RE::UI::GetSingleton();
			if (!ui) {
				return false;
			}

			constexpr std::array<std::string_view, 3> kPreviewMenus{
				RE::BarterMenu::MENU_NAME,
				RE::ContainerMenu::MENU_NAME,
				RE::GiftMenu::MENU_NAME
			};

			for (const auto menuName : kPreviewMenus) {
				if (!IsPreviewItemSourceMenu(menuName)) {
					continue;
				}

				if (ui->IsMenuOpen(menuName.data())) {
					return true;
				}
			}

			return false;
		}

				[[nodiscard]] bool IsLootSourceCorpseChestOrWorld(RE::FormID a_oldContainer, RE::FormID a_playerId)
				{
					if (a_oldContainer == LootRerollGuard::kWorldContainer) {
						return true;
					}
					if (a_oldContainer == a_playerId) {
						return false;
					}

					auto* sourceForm = RE::TESForm::LookupByID<RE::TESForm>(a_oldContainer);
					if (!sourceForm) {
						return false;
					}

					if (auto* sourceActor = sourceForm->As<RE::Actor>()) {
						return sourceActor->IsDead();
					}

					if (auto* sourceRef = sourceForm->As<RE::TESObjectREFR>()) {
						if (auto* sourceBase = sourceRef->GetBaseObject(); sourceBase && sourceBase->As<RE::TESObjectCONT>()) {
							return true;
						}
					}

					return sourceForm->As<RE::TESObjectCONT>() != nullptr;
				}
	}

	const std::vector<std::uint64_t>* EventBridge::FindEquippedInstanceKeysForAffixTokenCached(std::uint64_t a_affixToken) const
	{
		if (a_affixToken == 0u || !_instanceTrackingState.equippedTokenCacheReady) {
			return nullptr;
		}

		const auto it = _instanceTrackingState.equippedInstanceKeysByToken.find(a_affixToken);
		if (it == _instanceTrackingState.equippedInstanceKeysByToken.end()) {
			return nullptr;
		}

		return std::addressof(it->second);
	}

	std::vector<std::uint64_t> EventBridge::CollectEquippedInstanceKeysForAffixToken(std::uint64_t a_affixToken) const
	{
		std::vector<std::uint64_t> keys;
		if (a_affixToken == 0u) {
			return keys;
		}

		if (const auto* cached = FindEquippedInstanceKeysForAffixTokenCached(a_affixToken); cached) {
			return *cached;
		}
		if (_instanceTrackingState.equippedTokenCacheReady) {
			return keys;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return keys;
		}

		auto* changes = player->GetInventoryChanges();
		if (!changes || !changes->entryList) {
			return keys;
		}

		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->extraLists) {
				continue;
			}

			for (auto* xList : *entry->extraLists) {
				if (!xList) {
					continue;
				}

				const bool worn = xList->HasType<RE::ExtraWorn>() || xList->HasType<RE::ExtraWornLeft>();
				if (!worn) {
					continue;
				}

				auto* uid = xList->GetByType<RE::ExtraUniqueID>();
				if (!uid) {
					continue;
				}

				const auto key = MakeInstanceKey(uid->baseID, uid->uniqueID);
				const auto it = _instanceTrackingState.instanceAffixes.find(key);
				if (it == _instanceTrackingState.instanceAffixes.end() || !it->second.HasToken(a_affixToken)) {
					continue;
				}

				keys.push_back(key);
			}
		}

		std::sort(keys.begin(), keys.end());
		keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
		return keys;
	}

	std::optional<std::uint64_t> EventBridge::ResolvePrimaryEquippedInstanceKey(std::uint64_t a_affixToken) const
	{
		if (const auto* cached = FindEquippedInstanceKeysForAffixTokenCached(a_affixToken); cached && !cached->empty()) {
			return cached->front();
		}

		auto keys = CollectEquippedInstanceKeysForAffixToken(a_affixToken);
		if (keys.empty()) {
			return std::nullopt;
		}
		return keys.front();
	}

	void EventBridge::MarkLootEvaluatedInstance(std::uint64_t a_instanceKey)
	{
		if (a_instanceKey == 0) {
			return;
		}

		_lootState.evaluatedRecent.push_back(a_instanceKey);
		if (_lootState.evaluatedInstances.insert(a_instanceKey).second) {
			++_lootState.evaluatedInsertionsSincePrune;
		}

		if (_lootState.evaluatedRecent.size() > (kLootEvaluatedRecentKeep * 4) ||
			_lootState.evaluatedInsertionsSincePrune >= kLootEvaluatedPruneEveryInserts) {
			PruneLootEvaluatedInstances();
		}
	}

	void EventBridge::ForgetLootEvaluatedInstance(std::uint64_t a_instanceKey)
	{
		if (a_instanceKey == 0) {
			return;
		}

		_lootState.evaluatedInstances.erase(a_instanceKey);
	}

	bool EventBridge::IsLootEvaluatedInstance(std::uint64_t a_instanceKey) const
	{
		if (a_instanceKey == 0) {
			return false;
		}

		return _lootState.evaluatedInstances.contains(a_instanceKey);
	}

	void EventBridge::PruneLootEvaluatedInstances()
	{
		if (_lootState.evaluatedInstances.empty()) {
			_lootState.evaluatedRecent.clear();
			_lootState.evaluatedInsertionsSincePrune = 0;
			return;
		}

		std::unordered_set<std::uint64_t> keep;
		keep.reserve(
			_instanceTrackingState.instanceAffixes.size() +
			_runewordState.instanceStates.size() +
			kLootEvaluatedRecentKeep + 64);

		// Always keep explicit runtime states.
		for (const auto& [key, _] : _instanceTrackingState.instanceAffixes) {
			keep.insert(key);
		}
		for (const auto& [key, _] : _runewordState.instanceStates) {
			keep.insert(key);
		}
		if (_runewordState.selectedBaseKey) {
			keep.insert(*_runewordState.selectedBaseKey);
		}

		// Keep keys that still exist in player inventory.
		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			if (auto* changes = player->GetInventoryChanges(); changes && changes->entryList) {
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
						keep.insert(MakeInstanceKey(uid->baseID, uid->uniqueID));
					}
				}
			}
		}

		// Keep a recent window to survive short out-of-inventory transitions.
		std::size_t recentKept = 0;
		for (auto it = _lootState.evaluatedRecent.rbegin();
		     it != _lootState.evaluatedRecent.rend() && recentKept < kLootEvaluatedRecentKeep;
		     ++it) {
			if (*it == 0) {
				continue;
			}
			keep.insert(*it);
			++recentKept;
		}

		for (auto it = _lootState.evaluatedInstances.begin(); it != _lootState.evaluatedInstances.end();) {
			if (!keep.contains(*it)) {
				it = _lootState.evaluatedInstances.erase(it);
			} else {
				++it;
			}
		}

		std::deque<std::uint64_t> compactedRecent;
		for (const auto key : _lootState.evaluatedRecent) {
			if (key != 0 && _lootState.evaluatedInstances.contains(key)) {
				compactedRecent.push_back(key);
			}
		}

		const std::size_t maxRecentEntries = kLootEvaluatedRecentKeep * 2;
		while (compactedRecent.size() > maxRecentEntries) {
			compactedRecent.pop_front();
		}

		_lootState.evaluatedRecent.swap(compactedRecent);
		_lootState.evaluatedInsertionsSincePrune = 0;
	}

	float EventBridge::ComputeActiveScrollNoConsumeChancePct() const
	{
		if (_affixRuntimeState.affixes.empty() || _affixRuntimeState.activeCounts.empty()) {
			return 0.0f;
		}

		const auto limit = std::min(_affixRuntimeState.affixes.size(), _affixRuntimeState.activeCounts.size());
		double totalChancePct = 0.0;
		for (std::size_t i = 0; i < limit; ++i) {
			const auto stacks = _affixRuntimeState.activeCounts[i];
			if (stacks == 0) {
				continue;
			}

			const float perAffixChancePct = _affixRuntimeState.affixes[i].scrollNoConsumeChancePct;
			if (perAffixChancePct <= 0.0f) {
				continue;
			}

			totalChancePct += static_cast<double>(perAffixChancePct) * static_cast<double>(stacks);
			if (totalChancePct >= 100.0) {
				return 100.0f;
			}
		}

		return static_cast<float>(std::clamp(totalChancePct, 0.0, 100.0));
	}

	std::int32_t EventBridge::RollScrollNoConsumeRestoreCount(
		std::int32_t a_consumedCount,
		float a_preserveChancePct)
	{
		if (a_consumedCount <= 0 || a_preserveChancePct <= 0.0f) {
			return 0;
		}
		if (a_preserveChancePct >= 100.0f) {
			return a_consumedCount;
		}

		std::int32_t restoreCount = 0;
		std::uniform_real_distribution<float> dist(0.0f, 100.0f);
		for (std::int32_t i = 0; i < a_consumedCount; ++i) {
			if (dist(_rng) < a_preserveChancePct) {
				restoreCount += 1;
			}
		}
		return restoreCount;
	}


	RE::BSEventNotifyControl EventBridge::HandleContainerChangedEvent(
		const RE::TESContainerChangedEvent* a_event,
		EventStateLock&,
		const EngineEventContext& a_context)
	{
		const auto now = a_context.observedAt;
		MaybeFlushRuntimeUserSettings(now, false);

		// Ownership bookkeeping (drop, stash, pickup) runs even while the MCM
		// has the runtime off: an affix item dropped or picked up then would
		// otherwise keep its state on the old key, and the next load or the
		// re-enable prune would erase it. Only the paths that grant items or roll
		// loot check the enabled flag.
		if (!_configLoaded) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return RE::BSEventNotifyControl::kContinue;
		}

		const bool runtimeEnabled = _runtimeSettings.enabled.load(std::memory_order_relaxed);
		const auto playerId = player->GetFormID();
		const auto refHandle = static_cast<LootRerollGuard::RefHandle>(RE::ObjectRefHandle(a_event->reference).native_handle());
		RE::FormID referenceFormID = 0;
		std::uint64_t referenceUniqueKey = 0;
		if (a_context.containerReferenceCaptured) {
			referenceFormID = a_context.containerReferenceFormID;
			if (a_context.containerReferenceUidBaseID != 0u && a_context.containerReferenceUniqueID != 0u) {
				referenceUniqueKey = MakeInstanceKey(a_context.containerReferenceUidBaseID, a_context.containerReferenceUniqueID);
			}
		} else if (a_event->reference) {
			if (auto ref = a_event->reference.get(); ref) {
				referenceFormID = ref->GetFormID();
				if (const auto* uid = ref->extraList.GetByType<RE::ExtraUniqueID>();
					uid && uid->baseID != 0u && uid->uniqueID != 0u) {
					referenceUniqueKey = MakeInstanceKey(uid->baseID, uid->uniqueID);
				}
			}
		}

		// Scroll preserve path:
		// - event shape: player inventory -> null container
		// - additive chance from equipped affixes (capped at 100)
		if (ShouldHandleScrollConsumePreservation(
				a_event->oldContainer,
				a_event->newContainer,
				a_event->itemCount,
				a_event->baseObj,
				refHandle,
				playerId)) {
			auto* consumed = RE::TESForm::LookupByID<RE::TESForm>(a_event->baseObj);
			auto* consumedScroll = consumed ? consumed->As<RE::ScrollItem>() : nullptr;
			if (consumedScroll && runtimeEnabled) {
				const float preserveChancePct = ComputeActiveScrollNoConsumeChancePct();
				const auto restoreCount = RollScrollNoConsumeRestoreCount(a_event->itemCount, preserveChancePct);
				if (restoreCount > 0) {
					player->AddObjectToContainer(consumedScroll, nullptr, restoreCount, nullptr);
					if (_loot.debugLog) {
						SKSE::log::debug(
							"CalamityAffixes: restored consumed scroll (baseObj={:08X}, consumed={}, restored={}, chancePct={:.2f}).",
							a_event->baseObj,
							a_event->itemCount,
							restoreCount,
							preserveChancePct);
					}
				} else if (_loot.debugLog && preserveChancePct > 0.0f) {
					SKSE::log::debug(
						"CalamityAffixes: scroll preserve roll failed (baseObj={:08X}, consumed={}, chancePct={:.2f}).",
						a_event->baseObj,
						a_event->itemCount,
						preserveChancePct);
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		// 1a) Record player-dropped world references so re-picking them up can't "reroll" loot affixes.
		if (a_event->oldContainer == playerId &&
			a_event->newContainer == LootRerollGuard::kWorldContainer) {
			const auto detachedWorldKey = MakeDetachedWorldInstanceKey(referenceFormID);
			const auto playerEventKey = a_event->uniqueID != 0u ?
				MakeInstanceKey(playerId, a_event->uniqueID) :
				std::uint64_t{ 0 };
			const auto legacyEventKey = a_event->baseObj != 0u && a_event->uniqueID != 0u ?
				MakeInstanceKey(a_event->baseObj, a_event->uniqueID) :
				std::uint64_t{ 0 };
			const std::array dropSourceCandidates{
				playerEventKey,
				referenceUniqueKey,
				legacyEventKey
			};

			std::uint64_t dropSourceKey = 0;
			bool ambiguousMaterialSources = false;
			for (const auto candidateKey : dropSourceCandidates) {
				if (candidateKey == 0u ||
					candidateKey == detachedWorldKey ||
					!HasMaterialInstanceState(candidateKey)) {
					continue;
				}
				if (dropSourceKey == 0u) {
					dropSourceKey = candidateKey;
				} else if (dropSourceKey != candidateKey) {
					ambiguousMaterialSources = true;
				}
			}

			if (!ambiguousMaterialSources && dropSourceKey == 0u) {
				for (const auto candidateKey : dropSourceCandidates) {
					if (candidateKey != 0u &&
						candidateKey != detachedWorldKey &&
						IsInstanceKeyTracked(candidateKey)) {
						dropSourceKey = candidateKey;
						break;
					}
				}
			}

			if (ambiguousMaterialSources) {
				SKSE::log::warn(
					"CalamityAffixes: blocked ambiguous player-to-world state transfer; candidate states were discarded (baseObj={:08X}, ref={:08X}, uniqueID={}, detachedKey={:016X}).",
					a_event->baseObj,
					referenceFormID,
					a_event->uniqueID,
					detachedWorldKey);
				for (const auto candidateKey : dropSourceCandidates) {
					DiscardInstanceKeyState(candidateKey);
				}
				DiscardInstanceKeyState(detachedWorldKey);
				dropSourceKey = 0u;
			} else if (dropSourceKey != 0u && detachedWorldKey == 0u) {
				SKSE::log::warn(
					"CalamityAffixes: discarded player-to-world state because the dropped reference had no stable FormID (baseObj={:08X}, uniqueID={}, sourceKey={:016X}).",
					a_event->baseObj,
					a_event->uniqueID,
					dropSourceKey);
				DiscardInstanceKeyState(dropSourceKey);
				dropSourceKey = 0u;
			} else if (dropSourceKey != 0u && HasMaterialInstanceState(detachedWorldKey)) {
				if (HasMaterialInstanceState(dropSourceKey)) {
					SKSE::log::warn(
						"CalamityAffixes: blocked occupied player-to-world state transfer; both ambiguous states were discarded (baseObj={:08X}, ref={:08X}, sourceKey={:016X}, detachedKey={:016X}).",
						a_event->baseObj,
						referenceFormID,
						dropSourceKey,
						detachedWorldKey);
					DiscardInstanceKeyState(dropSourceKey);
					DiscardInstanceKeyState(detachedWorldKey);
				}
			} else if (dropSourceKey != 0u) {
				RemapInstanceKey(dropSourceKey, detachedWorldKey);
			}
			if (dropSourceKey == 0u) {
				if (auto* task = SKSE::GetTaskInterface()) {
					task->AddTask([]() {
						if (auto* bridge = EventBridge::GetSingleton(); bridge) {
							const std::scoped_lock lock(bridge->_stateMutex);
							const bool prunedOrphanedDropKeys = bridge->PruneOrphanedPlayerInstanceKeys();
							if (prunedOrphanedDropKeys) {
								bridge->RebuildActiveCounts();
							}
						}
					});
				} else {
					const bool prunedOrphanedDropKeys = PruneOrphanedPlayerInstanceKeys();
					if (prunedOrphanedDropKeys) {
						RebuildActiveCounts();
					}
				}
			}
			if (_loot.debugLog) {
				SKSE::log::debug(
					"CalamityAffixes: reconciled player-to-world instance ownership (baseObj={:08X}, ref={:08X}, eventUid={}, referenceKey={:016X}, sourceKey={:016X}, detachedKey={:016X}).",
					a_event->baseObj,
					referenceFormID,
					a_event->uniqueID,
					referenceUniqueKey,
					dropSourceKey,
					detachedWorldKey);
			}

			if (refHandle != 0) {
				_lootState.rerollGuard.NotePlayerDrop(
					playerId,
					a_event->oldContainer,
					a_event->newContainer,
					a_event->itemCount,
					refHandle,
					detachedWorldKey);
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		// 1b) Record items the player puts into any container (chest, barrel, etc.)
		//     so re-taking them can't reroll. Tracked by {containerID, baseObj} + count
		//     because UniqueID is unreliable across container transfers.
		if (a_event->oldContainer == playerId &&
			a_event->newContainer != 0 &&
			a_event->newContainer != playerId) {
			if (a_event->baseObj != 0) {
				const auto stashKey = std::make_pair(a_event->newContainer, a_event->baseObj);
				_lootState.playerContainerStash[stashKey] += a_event->itemCount;
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		// 2) Only roll when items move INTO the player.
		if (a_event->itemCount <= 0) {
			return RE::BSEventNotifyControl::kContinue;
		}

		// 2a) A companion or another NPC picked up a reference the player dropped
		//     (a "pick that up" order, a disarm): move the detached world state to
		//     the new owner's key so it is still there when the item comes back.
		//     No loot roll on this path.
		if (a_event->oldContainer == LootRerollGuard::kWorldContainer &&
			a_event->newContainer != 0u &&
			a_event->newContainer != playerId) {
			std::optional<std::uint64_t> guardedDropKey;
			if (refHandle != 0u) {
				guardedDropKey = _lootState.rerollGuard.ConsumeDroppedReferenceInstanceKey(a_event->itemCount, refHandle);
			}
			const auto referenceDetachedWorldKey = MakeDetachedWorldInstanceKey(referenceFormID);
			if (guardedDropKey && !IsDetachedWorldReferenceMatch(*guardedDropKey, referenceDetachedWorldKey)) {
				guardedDropKey.reset();
			}
			std::uint64_t detachedWorldKey = guardedDropKey.value_or(0u);
			if (!IsInstanceKeyTracked(detachedWorldKey) && IsInstanceKeyTracked(referenceDetachedWorldKey)) {
				detachedWorldKey = referenceDetachedWorldKey;
			}
			if (IsInstanceKeyTracked(detachedWorldKey)) {
				const auto newOwnerId = a_event->newContainer;
				const auto targetKey = a_event->uniqueID != 0u ?
					MakeInstanceKey(newOwnerId, a_event->uniqueID) :
					((referenceUniqueKey >> 16u) == newOwnerId ? referenceUniqueKey : std::uint64_t{ 0 });
				if (targetKey == 0u) {
					// Keep it: a later UniqueIDChange or NPC-to-player transfer picks
					// it up with the exact-key rules.
					SKSE::log::info(
						"CalamityAffixes: retained detached world state; the new owner has no unique ID for it yet (baseObj={:08X}, ref={:08X}, newOwner={:08X}, detachedKey={:016X}).",
						a_event->baseObj,
						referenceFormID,
						newOwnerId,
						detachedWorldKey);
				} else if (HasMaterialInstanceState(targetKey)) {
					// The detached key is this exact reference (checked above), so
					// whatever the new owner's key still holds is left over from an
					// item that unique ID named before: replace it, as a reused
					// unique ID does on an NPC-to-NPC transfer.
					SKSE::log::info(
						"CalamityAffixes: replaced stale state on the new owner's reused unique ID (baseObj={:08X}, ref={:08X}, newOwner={:08X}, detachedKey={:016X}, targetKey={:016X}).",
						a_event->baseObj,
						referenceFormID,
						newOwnerId,
						detachedWorldKey,
						targetKey);
					DiscardInstanceKeyState(targetKey);
					RemapInstanceKey(detachedWorldKey, targetKey);
				} else {
					RemapInstanceKey(detachedWorldKey, targetKey);
					if (_loot.debugLog) {
						SKSE::log::debug(
							"CalamityAffixes: moved detached world state to its new owner (baseObj={:08X}, ref={:08X}, newOwner={:08X}, detachedKey={:016X}, targetKey={:016X}).",
							a_event->baseObj,
							referenceFormID,
							newOwnerId,
							detachedWorldKey,
							targetKey);
					}
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		if (a_event->newContainer != playerId) {
			return RE::BSEventNotifyControl::kContinue;
		}

		// 3) Reattach state only when this is the exact world reference the player dropped.
		std::optional<std::uint64_t> guardedDropKey;
		if (refHandle != 0u) {
			guardedDropKey = _lootState.rerollGuard.ConsumePlayerDropPickupInstanceKey(
				playerId,
				a_event->oldContainer,
				a_event->newContainer,
				a_event->itemCount,
				refHandle);
		}

		const auto referenceDetachedWorldKey = MakeDetachedWorldInstanceKey(referenceFormID);
		if (guardedDropKey &&
			!IsDetachedWorldReferenceMatch(*guardedDropKey, referenceDetachedWorldKey)) {
			SKSE::log::warn(
				"CalamityAffixes: rejected recycled drop-reference handle during pickup (baseObj={:08X}, ref={:08X}, guardedKey={:016X}, referenceKey={:016X}).",
				a_event->baseObj,
				referenceFormID,
				*guardedDropKey,
				referenceDetachedWorldKey);
			guardedDropKey.reset();
		}
		std::uint64_t detachedWorldKey = guardedDropKey.value_or(0u);
		if ((!IsInstanceKeyTracked(detachedWorldKey)) &&
			IsInstanceKeyTracked(referenceDetachedWorldKey)) {
			detachedWorldKey = referenceDetachedWorldKey;
		}
		if (!IsInstanceKeyTracked(detachedWorldKey) && a_event->oldContainer == LootRerollGuard::kWorldContainer) {
			// A script drop names no reference on either container event; the
			// unique-ID change recorded where the state went.
			SweepAbandonedWorldItemTransits();
			if (const auto transit = _lootState.worldItemTransits.Find(
					a_event->baseObj, referenceFormID, StatusClock::RealNowMs());
				transit && IsInstanceKeyTracked(transit->detachedKey)) {
				detachedWorldKey = transit->detachedKey;
			}
		}
		const bool trackedDetachedWorldState = IsInstanceKeyTracked(detachedWorldKey);
		if (guardedDropKey.has_value() || trackedDetachedWorldState) {
			const auto pickupKey = a_event->uniqueID != 0u ?
				MakeInstanceKey(playerId, a_event->uniqueID) :
				((referenceUniqueKey >> 16u) == playerId ? referenceUniqueKey : std::uint64_t{ 0 });
			if (trackedDetachedWorldState && pickupKey != 0u) {
				if (HasMaterialInstanceState(pickupKey)) {
					SKSE::log::warn(
						"CalamityAffixes: blocked occupied world-to-player state transfer; both ambiguous states were discarded (baseObj={:08X}, ref={:08X}, detachedKey={:016X}, pickupKey={:016X}).",
						a_event->baseObj,
						referenceFormID,
						detachedWorldKey,
						pickupKey);
					DiscardInstanceKeyState(detachedWorldKey);
					DiscardInstanceKeyState(pickupKey);
				} else {
					RemapInstanceKey(detachedWorldKey, pickupKey);
					_lootState.worldItemTransits.Forget(detachedWorldKey);
				}
			} else if (trackedDetachedWorldState && !BindReturnedWorldItemState(a_event->baseObj, detachedWorldKey, false)) {
				// The item may land in the inventory after this event: try once more
				// on the next task, which runs before the world reference's delete
				// cleanup queued after it.
				const auto baseObj = a_event->baseObj;
				const auto retryKey = detachedWorldKey;
				const auto warnRetained = [baseObj, retryKey]() {
					SKSE::log::warn(
						"CalamityAffixes: retained detached world state because pickup had no stable player UID (baseObj={:08X}, detachedKey={:016X}).",
						baseObj,
						retryKey);
				};
				if (auto* task = SKSE::GetTaskInterface(); task) {
					task->AddTask([baseObj, retryKey, warnRetained]() {
						if (auto* bridge = EventBridge::GetSingleton(); bridge) {
							const std::scoped_lock lock(bridge->_stateMutex);
							if (bridge->IsInstanceKeyTracked(retryKey) &&
								!bridge->BindReturnedWorldItemState(baseObj, retryKey, true)) {
								warnRetained();
							}
						}
					});
				} else {
					warnRetained();
				}
			}
			if (_loot.debugLog) {
				SKSE::log::debug(
					"CalamityAffixes: skipping loot roll (player dropped + re-picked) (baseObj={:08X}, uniqueID={}, detachedKey={:016X}, pickupKey={:016X}).",
					a_event->baseObj,
					a_event->uniqueID,
					detachedWorldKey,
					pickupKey);
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		// 3b) Skip if this item was stashed in a container by the player.
		//     Tracked by {containerID, baseObj} count — UniqueID-independent.
		if (a_event->baseObj != 0 && a_event->oldContainer != 0) {
			const auto stashKey = std::make_pair(a_event->oldContainer, a_event->baseObj);
			if (auto stashIt = _lootState.playerContainerStash.find(stashKey);
				stashIt != _lootState.playerContainerStash.end()) {
				stashIt->second -= a_event->itemCount;
				if (stashIt->second <= 0) {
					_lootState.playerContainerStash.erase(stashIt);
				}
				if (_loot.debugLog) {
					SKSE::log::debug(
						"CalamityAffixes: skipping loot roll (player stashed + retrieved) (baseObj={:08X}, container={:08X}).",
						a_event->baseObj,
						a_event->oldContainer);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		}

		// Everything above only moved state between keys; rolling loot needs the
		// runtime on.
		if (!runtimeEnabled) {
			return RE::BSEventNotifyControl::kContinue;
		}

		const auto baseObj = a_event->baseObj;
		const auto count = a_event->itemCount;
		const auto uid = a_event->uniqueID;
		const auto allowRunewordFragmentRoll = IsLootSourceCorpseChestOrWorld(a_event->oldContainer, playerId);

		if (!allowRunewordFragmentRoll) {
			if (_loot.debugLog) {
				SKSE::log::debug(
					"CalamityAffixes: runeword fragment roll skipped (source filter) (oldContainer={:08X}, baseObj={:08X}, uniqueID={}).",
					a_event->oldContainer,
					baseObj,
					uid);
			}
		}

		if (_loot.debugLog) {
			SKSE::log::debug(
				"CalamityAffixes: loot event queued (baseObj={:08X}, count={}, uniqueID={}).",
				baseObj,
				count,
				uid);
		}

		ProcessLootAcquired(baseObj, count, uid, a_event->oldContainer, allowRunewordFragmentRoll);
		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl EventBridge::HandleUniqueIDChangeEvent(
		const RE::TESUniqueIDChangeEvent* a_event,
		EventStateLock&,
		const EngineEventContext& a_context)
	{
		const auto now = a_context.observedAt;
		MaybeFlushRuntimeUserSettings(now, false);

		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}

		const auto oldOwnerID = a_event->oldBaseID;
		const auto newOwnerID = a_event->newBaseID;
		const auto itemFormID = a_event->objectID;
		if (oldOwnerID == 0 || newOwnerID == 0 || itemFormID == 0) {
			if (_loot.debugLog) {
				SKSE::log::debug(
					"CalamityAffixes: ignored incomplete TESUniqueIDChangeEvent owner boundary (oldOwner={:08X}, newOwner={:08X}, item={:08X}, oldUid={}, newUid={}).",
					oldOwnerID,
					newOwnerID,
					itemFormID,
					a_event->oldUniqueID,
					a_event->newUniqueID);
			}
			return RE::BSEventNotifyControl::kContinue;
		}
		// A player item that became a loose world reference (a script drop with no
		// reference on its container event): move its state onto the reference
		// before the drop's orphan prune can erase it.
		if (const auto* uidPlayer = RE::PlayerCharacter::GetSingleton();
			uidPlayer &&
			IsPlayerItemBecomingWorldReference(
				uidPlayer->GetFormID(),
				oldOwnerID,
				newOwnerID,
				a_event->oldUniqueID,
				a_event->newUniqueID) &&
			MovePlayerItemStateToWorldReference(itemFormID, a_event->oldUniqueID, newOwnerID)) {
			return RE::BSEventNotifyControl::kContinue;
		}
		if (a_event->oldUniqueID == 0 || a_event->newUniqueID == 0) {
			if (_loot.debugLog) {
				SKSE::log::debug(
					"CalamityAffixes: ignored incomplete TESUniqueIDChangeEvent UID boundary (oldOwner={:08X}, newOwner={:08X}, item={:08X}, oldUid={}, newUid={}).",
					oldOwnerID,
					newOwnerID,
					itemFormID,
					a_event->oldUniqueID,
					a_event->newUniqueID);
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		const auto oldKey = MakeInstanceKey(oldOwnerID, a_event->oldUniqueID);
		const auto newKey = MakeInstanceKey(newOwnerID, a_event->newUniqueID);
		const auto legacyItemKey = MakeInstanceKey(itemFormID, a_event->oldUniqueID);
		const bool trackedOld = IsInstanceKeyTracked(oldKey);
		const bool trackedLegacyItemKey = legacyItemKey != oldKey && IsInstanceKeyTracked(legacyItemKey);
		const bool materialOld = HasMaterialInstanceState(oldKey);
		const bool materialLegacyItemKey =
			legacyItemKey != oldKey && HasMaterialInstanceState(legacyItemKey);
		const bool trackedNew = IsInstanceKeyTracked(newKey);
		const bool materialNew = HasMaterialInstanceState(newKey);
		const bool trackedPreview =
			FindLootPreviewSlots(oldKey) != nullptr ||
			(trackedLegacyItemKey && FindLootPreviewSlots(legacyItemKey) != nullptr);

		auto* player = RE::PlayerCharacter::GetSingleton();
		const auto playerId = player ? player->GetFormID() : 0u;
		const bool playerHasOldKey =
			PlayerHasInstanceKey(oldKey) ||
			(trackedLegacyItemKey && PlayerHasInstanceKey(legacyItemKey));
		const bool playerHasNewKey = PlayerHasInstanceKey(newKey);
		const bool previewMenuContextOpen = IsPreviewRemapMenuContextOpen();
		const bool previewRemapEvidence = ShouldAllowPreviewUniqueIdRemap(
			false,
			trackedPreview,
			previewMenuContextOpen);
		if (materialOld && materialLegacyItemKey) {
			SKSE::log::warn(
				"CalamityAffixes: blocked ambiguous canonical+legacy source collision; all involved states were discarded (oldOwner={:08X}, newOwner={:08X}, item={:08X}, oldUid={}, newUid={}, oldKey={:016X}, legacyKey={:016X}, newKey={:016X}).",
				oldOwnerID,
				newOwnerID,
				itemFormID,
				a_event->oldUniqueID,
				a_event->newUniqueID,
				oldKey,
				legacyItemKey,
				newKey);
			DiscardInstanceKeyState(oldKey);
			DiscardInstanceKeyState(legacyItemKey);
			if (materialNew) {
				DiscardInstanceKeyState(newKey);
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		const bool policyOldTracked = materialOld || (!materialLegacyItemKey && trackedOld);
		const bool policyLegacyTracked =
			materialLegacyItemKey || (!policyOldTracked && trackedLegacyItemKey);
		const auto transferPlan = ResolveInstanceKeyTransfer(InstanceKeyTransferContext{
			.playerID = playerId,
			.oldOwnerID = oldOwnerID,
			.newOwnerID = newOwnerID,
			.itemFormID = itemFormID,
			.oldUniqueID = a_event->oldUniqueID,
			.newUniqueID = a_event->newUniqueID,
			.playerHasOldKey = playerHasOldKey || previewRemapEvidence,
			.playerHasNewKey = playerHasNewKey,
			.oldKeyTracked = policyOldTracked,
			.legacyItemKeyTracked = policyLegacyTracked,
			.newKeyTracked = materialNew
		});
		if (transferPlan.decision == InstanceKeyTransferDecision::kIgnore) {
			if (_loot.debugLog) {
				SKSE::log::debug(
					"CalamityAffixes: ignored TESUniqueIDChangeEvent (oldOwner={:08X}, newOwner={:08X}, item={:08X}, oldUid={}, newUid={}, oldKey={:016X}, legacyKey={:016X}, newKey={:016X}, trackedOld={}, trackedLegacy={}, trackedNew={}, playerHasOld={}, playerHasNew={}, trackedPreview={}, previewMenuOpen={}).",
					oldOwnerID,
					newOwnerID,
					itemFormID,
					a_event->oldUniqueID,
					a_event->newUniqueID,
					oldKey,
					legacyItemKey,
					newKey,
					trackedOld,
					trackedLegacyItemKey,
					trackedNew,
					playerHasOldKey,
					playerHasNewKey,
					trackedPreview,
					previewMenuContextOpen);
			}
			return RE::BSEventNotifyControl::kContinue;
		}
		if (transferPlan.decision == InstanceKeyTransferDecision::kConflict) {
			SKSE::log::warn(
				"CalamityAffixes: blocked instance-key remap collision; both ambiguous states were discarded instead of merged (oldOwner={:08X}, newOwner={:08X}, item={:08X}, oldUid={}, newUid={}, oldKey={:016X}, newKey={:016X}).",
				oldOwnerID,
				newOwnerID,
				itemFormID,
				a_event->oldUniqueID,
				a_event->newUniqueID,
				transferPlan.oldKey,
				transferPlan.newKey);
			DiscardInstanceKeyState(transferPlan.oldKey);
			DiscardInstanceKeyState(transferPlan.newKey);
			return RE::BSEventNotifyControl::kContinue;
		}
		if (transferPlan.decision == InstanceKeyTransferDecision::kReplaceStaleDestination) {
			SKSE::log::info(
				"CalamityAffixes: replaced stale state on a reused unique ID (oldOwner={:08X}, newOwner={:08X}, item={:08X}, oldUid={}, newUid={}, oldKey={:016X}, newKey={:016X}).",
				oldOwnerID,
				newOwnerID,
				itemFormID,
				a_event->oldUniqueID,
				a_event->newUniqueID,
				transferPlan.oldKey,
				transferPlan.newKey);
			DiscardInstanceKeyState(transferPlan.newKey);
		}

		RemapInstanceKey(transferPlan.oldKey, transferPlan.newKey);
		return RE::BSEventNotifyControl::kContinue;
	}

	bool EventBridge::HasMaterialInstanceState(std::uint64_t a_instanceKey) const
	{
		if (a_instanceKey == 0u) {
			return false;
		}

		const bool hasRuntimeState = std::ranges::any_of(
			_instanceTrackingState.instanceStates,
			[a_instanceKey](const auto& entry) {
				return entry.first.instanceKey == a_instanceKey;
			});
		return _instanceTrackingState.instanceAffixes.contains(a_instanceKey) ||
		       hasRuntimeState ||
		       _runewordState.instanceStates.contains(a_instanceKey) ||
		       (_runewordState.selectedBaseKey && *_runewordState.selectedBaseKey == a_instanceKey);
	}

	bool EventBridge::IsInstanceKeyTracked(std::uint64_t a_instanceKey) const
	{
		return HasMaterialInstanceState(a_instanceKey) ||
		       IsLootEvaluatedInstance(a_instanceKey) ||
		       FindLootPreviewSlots(a_instanceKey) != nullptr;
	}

	void EventBridge::DiscardInstanceKeyState(std::uint64_t a_instanceKey)
	{
		if (a_instanceKey == 0u) {
			return;
		}

		_instanceTrackingState.instanceAffixes.erase(a_instanceKey);
		_instanceTrackingState.selectedReforgeCounts.erase(a_instanceKey);
		ForgetLootEvaluatedInstance(a_instanceKey);
		ForgetLootPreviewSlots(a_instanceKey);
		EraseInstanceRuntimeStates(a_instanceKey);
		_runewordState.instanceStates.erase(a_instanceKey);
		if (_runewordState.selectedBaseKey && *_runewordState.selectedBaseKey == a_instanceKey) {
			_runewordState.selectedBaseKey.reset();
		}
		_instanceTrackingState.equippedTokenCacheReady = false;
	}

	void EventBridge::RemapInstanceKey(std::uint64_t a_oldKey, std::uint64_t a_newKey)
	{
		if (a_oldKey == 0 || a_newKey == 0 || a_oldKey == a_newKey) {
			return;
		}
		if (HasMaterialInstanceState(a_newKey)) {
			SKSE::log::warn(
				"CalamityAffixes: refused occupied instance-key remap; destination state was preserved (old={:016X}, new={:016X}).",
				a_oldKey,
				a_newKey);
			return;
		}
		// Evaluation/preview markers do not own an item. Clear them so a material
		// source can move into the canonical destination without data loss.
		ForgetLootEvaluatedInstance(a_newKey);
		ForgetLootPreviewSlots(a_newKey);

		if (IsLootEvaluatedInstance(a_oldKey)) {
			ForgetLootEvaluatedInstance(a_oldKey);
			MarkLootEvaluatedInstance(a_newKey);
		}

		if (const auto* previewSlots = FindLootPreviewSlots(a_oldKey)) {
			const auto preview = *previewSlots;
			ForgetLootPreviewSlots(a_oldKey);
			RememberLootPreviewSlots(a_newKey, preview);
		}

		if (auto affixNode = _instanceTrackingState.instanceAffixes.extract(a_oldKey); !affixNode.empty()) {
			affixNode.key() = a_newKey;
			_instanceTrackingState.instanceAffixes.insert(std::move(affixNode));
		}

		// The destination owns no affixes (checked above), so any count there is stale.
		auto countNode = _instanceTrackingState.selectedReforgeCounts.extract(a_oldKey);
		_instanceTrackingState.selectedReforgeCounts.erase(a_newKey);
		if (!countNode.empty()) {
			countNode.key() = a_newKey;
			_instanceTrackingState.selectedReforgeCounts.insert(std::move(countNode));
		}

		std::vector<std::pair<InstanceStateKey, InstanceRuntimeState>> remappedStates;
		for (auto it = _instanceTrackingState.instanceStates.begin(); it != _instanceTrackingState.instanceStates.end();) {
			if (it->first.instanceKey != a_oldKey) {
				++it;
				continue;
			}

			remappedStates.emplace_back(it->first, it->second);
			it = _instanceTrackingState.instanceStates.erase(it);
		}

		for (const auto& [oldStateKey, state] : remappedStates) {
			const auto newStateKey = MakeInstanceStateKey(a_newKey, oldStateKey.affixToken);
			_instanceTrackingState.instanceStates.emplace(newStateKey, state);
		}

		if (auto rwNode = _runewordState.instanceStates.extract(a_oldKey); !rwNode.empty()) {
			if (!_runewordState.instanceStates.contains(a_newKey)) {
				rwNode.key() = a_newKey;
				_runewordState.instanceStates.insert(std::move(rwNode));
			}
		}

		if (_runewordState.selectedBaseKey && *_runewordState.selectedBaseKey == a_oldKey) {
			_runewordState.selectedBaseKey = a_newKey;
		}
		_instanceTrackingState.equippedTokenCacheReady = false;
	}

	bool EventBridge::MovePlayerItemStateToWorldReference(
		RE::FormID a_itemFormID,
		std::uint16_t a_playerUniqueID,
		RE::FormID a_referenceFormID)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || a_itemFormID == 0u || a_playerUniqueID == 0u || a_referenceFormID == 0u) {
			return false;
		}
		const auto sourceKey = MakeInstanceKey(player->GetFormID(), a_playerUniqueID);
		if (!HasMaterialInstanceState(sourceKey)) {
			return false;
		}
		// Only the item itself as a loose reference: not an actor, and the same base.
		auto* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(a_referenceFormID);
		auto* base = ref ? ref->GetBaseObject() : nullptr;
		if (!ref || ref->As<RE::Actor>() || !base || base->GetFormID() != a_itemFormID) {
			if (_loot.debugLog) {
				SKSE::log::debug(
					"CalamityAffixes: kept player item state; the new owner is not the item's world reference (item={:08X}, uid={}, newOwner={:08X}).",
					a_itemFormID,
					a_playerUniqueID,
					a_referenceFormID);
			}
			return false;
		}
		const auto detachedKey = MakeDetachedWorldInstanceKey(a_referenceFormID);
		if (HasMaterialInstanceState(detachedKey)) {
			SKSE::log::warn(
				"CalamityAffixes: kept player item state; its world reference already holds state (item={:08X}, uid={}, ref={:08X}).",
				a_itemFormID,
				a_playerUniqueID,
				a_referenceFormID);
			return false;
		}
		SweepAbandonedWorldItemTransits();
		RemapInstanceKey(sourceKey, detachedKey);
		_lootState.worldItemTransits.Record(WorldItemTransit{
			.baseFormID = a_itemFormID,
			.referenceFormID = a_referenceFormID,
			.detachedKey = detachedKey,
			.recordedAtMs = StatusClock::RealNowMs(),
		});
		SKSE::log::info(
			"CalamityAffixes: moved player item state onto its world reference (item={:08X}, uid={}, ref={:08X}, key={:016X}).",
			a_itemFormID,
			a_playerUniqueID,
			a_referenceFormID,
			detachedKey);
		return true;
	}

	bool EventBridge::BindReturnedWorldItemState(
		RE::FormID a_baseFormID,
		std::uint64_t a_detachedKey,
		bool a_logIfNotFound)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* changes = player ? player->GetInventoryChanges() : nullptr;
		if (!changes || !changes->entryList || a_baseFormID == 0u || !IsInstanceKeyTracked(a_detachedKey)) {
			return false;
		}
		// The detached key names the world reference the item was.
		const auto referenceFormID = static_cast<RE::FormID>(a_detachedKey >> 16u);
		// The engine rewrote the dropped item's unique ID to (reference, 0); back
		// in the inventory it may still carry that, or none at all (see
		// ChooseReturnedItem for which copy may be taken).
		RE::ExtraDataList* exact = nullptr;
		std::uint32_t exactCount = 0u;
		RE::ExtraDataList* loose = nullptr;
		std::uint32_t looseCount = 0u;
		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->object || entry->object->GetFormID() != a_baseFormID || !entry->extraLists) {
				continue;
			}
			for (auto* xList : *entry->extraLists) {
				if (!xList) {
					continue;
				}
				const auto* uid = xList->GetByType<RE::ExtraUniqueID>();
				if (uid && uid->uniqueID != 0u && uid->baseID != referenceFormID) {
					continue;  // a stable unique ID: some other copy
				}
				if (uid && uid->baseID == referenceFormID) {
					exact = xList;
					++exactCount;
					continue;
				}
				loose = xList;
				++looseCount;
			}
		}
		const auto choice = ChooseReturnedItem(exactCount, looseCount);
		auto* target = choice == ReturnedItemChoice::kExact ? exact :
		               choice == ReturnedItemChoice::kLoose ? loose :
		                                                      nullptr;
		if (!target) {
			if (a_logIfNotFound || exactCount + looseCount > 1u) {
				SKSE::log::warn(
					"CalamityAffixes: could not tell which returned item to rebind (base={:08X}, ref={:08X}, exact={}, loose={}, key={:016X}).",
					a_baseFormID,
					referenceFormID,
					exactCount,
					looseCount,
					a_detachedKey);
			}
			return false;
		}
		auto* uid = target->GetByType<RE::ExtraUniqueID>();
		if (!uid) {
			uid = RE::BSExtraData::Create<RE::ExtraUniqueID>();
			if (!uid) {
				return false;
			}
			target->Add(uid);
		}
		uid->baseID = player->GetFormID();
		uid->uniqueID = changes->GetNextUniqueID();
		const auto pickupKey = MakeInstanceKey(uid->baseID, uid->uniqueID);
		RemapInstanceKey(a_detachedKey, pickupKey);
		_lootState.worldItemTransits.Forget(a_detachedKey);
		SKSE::log::info(
			"CalamityAffixes: rebound a returned world item to the player (base={:08X}, ref={:08X}, key={:016X} -> {:016X}).",
			a_baseFormID,
			referenceFormID,
			a_detachedKey,
			pickupKey);
		return true;
	}

	void EventBridge::SweepAbandonedWorldItemTransits()
	{
		std::array<std::uint64_t, WorldItemTransitLedger::kCapacity> abandoned{};
		const auto count = _lootState.worldItemTransits.TakeAbandoned(StatusClock::RealNowMs(), abandoned);
		for (std::size_t i = 0; i < count; ++i) {
			const auto key = abandoned[i];
			if (!IsInstanceKeyTracked(key) || PlayerHasInstanceKey(key)) {
				continue;
			}
			DiscardInstanceKeyState(key);
			SKSE::log::info(
				"CalamityAffixes: pruned a world item that was deleted and never came back (key={:016X}).",
				key);
		}
	}


}
