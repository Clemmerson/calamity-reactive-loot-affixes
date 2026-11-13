#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/InstanceKeyStateMapTransfer.h"
#include "CalamityAffixes/LootCurrencySourceHelpers.h"
#include "CalamityAffixes/DropReferenceIdentityPolicy.h"
#include "CalamityAffixes/InstanceKeyTransferPolicy.h"
#include "CalamityAffixes/LootUiGuards.h"
#include "CalamityAffixes/RuneforgeWorkstation.h"
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

				void LogPlayerInventoryIdentitySnapshot(RE::PlayerCharacter* a_player, RE::FormID a_baseObjectFormID, std::string_view a_phase)
				{
					if (!a_player || a_baseObjectFormID == 0u) {
						return;
					}
					auto* changes = a_player->GetInventoryChanges();
					std::uint32_t matched = 0u;
					std::uint32_t emitted = 0u;
					if (changes && changes->entryList) {
						for (auto* entry : *changes->entryList) {
							if (!entry || !entry->object || entry->object->GetFormID() != a_baseObjectFormID || !entry->extraLists) {
								continue;
							}
							for (auto* xList : *entry->extraLists) {
								if (!xList) {
									continue;
								}
								++matched;
								if (emitted >= 8u) {
									continue;
								}
								const auto* uid = xList->GetByType<RE::ExtraUniqueID>();
								SKSE::log::info(
									"CalamityAffixes: ownership inventory identity (phase={}, baseObj={:08X}, extraBase={:08X}, uid={}, worn={}, wornLeft={}).",
									a_phase,
									a_baseObjectFormID,
									uid ? uid->baseID : 0u,
									uid ? uid->uniqueID : 0u,
									xList->HasType<RE::ExtraWorn>(),
									xList->HasType<RE::ExtraWornLeft>());
								++emitted;
							}
						}
					}
					SKSE::log::info(
						"CalamityAffixes: ownership inventory identity snapshot (phase={}, baseObj={:08X}, inventoryAvailable={}, matchingExtraLists={}, emitted={}).",
						a_phase,
						a_baseObjectFormID,
						changes && changes->entryList,
						matched,
						emitted);
				}

				[[nodiscard]] std::vector<DropPickupInventoryIdentity> CollectPlayerDropPickupInventoryIdentities(
					RE::PlayerCharacter* a_player,
					RE::FormID a_baseObjectFormID)
				{
					std::vector<DropPickupInventoryIdentity> observations;
					if (!a_player || a_baseObjectFormID == 0u) {
						return observations;
					}
					auto* changes = a_player->GetInventoryChanges();
					if (!changes || !changes->entryList) {
						return observations;
					}
					for (auto* entry : *changes->entryList) {
						if (!entry || !entry->object || entry->object->GetFormID() != a_baseObjectFormID || !entry->extraLists) {
							continue;
						}
						for (auto* xList : *entry->extraLists) {
							if (!xList) {
								continue;
							}
							const auto* uid = xList->GetByType<RE::ExtraUniqueID>();
							if (!uid || uid->uniqueID == 0u || uid->baseID == 0u) {
								continue;
							}
							observations.push_back({
								MakeTrackedInstanceKey(uid->baseID, uid->uniqueID),
								entry->object->GetFormID(),
								xList->GetCount()
							});
						}
					}
					return observations;
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
		if (a_event) {
			auto* eventPlayer = RE::PlayerCharacter::GetSingleton();
			const auto eventPlayerId = eventPlayer ? eventPlayer->GetFormID() : 0u;
			if (eventPlayerId != 0u && a_event->oldContainer == 0u && a_event->newContainer == eventPlayerId) {
				SKSE::log::info(
					"[R84-XFER] handled world-to-player seq={} base={:08X} old={:08X} new={:08X} count={} eventUid={} refHandle={:016X}.",
					a_context.ingressSequence,
					a_event->baseObj,
					a_event->oldContainer,
					a_event->newContainer,
					a_event->itemCount,
					a_event->uniqueID,
					static_cast<std::uint64_t>(RE::ObjectRefHandle(a_event->reference).native_handle()));
			}
		}
		const auto now = a_context.observedAt;
		MaybeFlushRuntimeUserSettings(now, false);

		if (!_configLoaded || !_runtimeSettings.enabled.load(std::memory_order_relaxed)) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return RE::BSEventNotifyControl::kContinue;
		}

		const auto playerId = player->GetFormID();
		ExpirePendingDropOwnership(detail::GetInGameDayStamp());
		const auto refHandle = static_cast<LootRerollGuard::RefHandle>(RE::ObjectRefHandle(a_event->reference).native_handle());
		RE::FormID referenceFormID = 0;
		RE::FormID referenceFormCandidate = 0;
		std::uint16_t referenceUniqueID = 0;
		std::uint64_t referenceUniqueKey = 0;
		RE::TESObjectREFR* eventReference = nullptr;
		if (a_event->reference) {
			if (auto ref = a_event->reference.get(); ref) {
				eventReference = ref.get();
				// A Runeforge workstation display clone is never loot: no affix roll, no reroll record (SP-7).
				if (RuneforgeWorkstation::IsDisplayClone(ref.get())) {
					SKSE::log::warn("CalamityAffixes: container change of Runeforge display clone {:08X} ignored by the loot path", ref->GetFormID());
					return RE::BSEventNotifyControl::kContinue;
				}
				referenceFormID = ref->GetFormID();
				if ((referenceFormID & 0xFF000000u) == 0xFF000000u) {
					referenceFormCandidate = referenceFormID;
				}
				if (const auto* uid = ref->extraList.GetByType<RE::ExtraUniqueID>(); uid && uid->uniqueID != 0u) {
					referenceUniqueID = uid->uniqueID;
					if (uid->baseID != 0u) {
						referenceUniqueKey = MakeInstanceKey(uid->baseID, uid->uniqueID);
					}
				}
			}
		}
		// Skyrim's container-change payload can place a dynamic reference FormID
		// in the handle storage. Such a value is not a resolvable ObjectRefHandle
		// (get() above returns null), but it can still be accepted as a reference
		// only after the FormID lookup returns that exact reference and its base
		// matches this singleton transfer event.
		if (!eventReference && refHandle != 0u) {
			const auto candidateFormID = static_cast<RE::FormID>(refHandle);
			if ((candidateFormID & 0xFF000000u) == 0xFF000000u) {
				// Keep this only as a provisional dynamic FormID. The engine has not
				// materialized it yet on the observed drop path; player activation
				// must later resolve the same FormID and base before it can be stamped.
				referenceFormCandidate = candidateFormID;
			}
			if (auto* candidate = RE::TESForm::LookupByID<RE::TESObjectREFR>(candidateFormID); candidate) {
				auto* candidateBase = candidate->GetBaseObject();
				if (candidate->GetFormID() == candidateFormID && candidateBase &&
					candidateBase->GetFormID() == a_event->baseObj) {
					eventReference = candidate;
					referenceFormID = candidateFormID;
					referenceFormCandidate = candidateFormID;
					if (const auto* uid = candidate->extraList.GetByType<RE::ExtraUniqueID>(); uid && uid->uniqueID != 0u) {
						referenceUniqueID = uid->uniqueID;
						if (uid->baseID != 0u) {
							referenceUniqueKey = MakeInstanceKey(uid->baseID, uid->uniqueID);
						}
					}
					SKSE::log::info(
						"CalamityAffixes: resolved container-event reference field as an exact dynamic FormID (baseObj={:08X}, refForm={:08X}, rawHandle={:08X}).",
						a_event->baseObj,
						candidateFormID,
						static_cast<RE::FormID>(refHandle));
				} else {
					SKSE::log::warn(
						"CalamityAffixes: rejected raw container-event reference candidate because the resolved reference/base did not match (raw={:08X}, resolvedForm={:08X}, resolvedBase={:08X}, eventBase={:08X}).",
						candidateFormID,
						candidate->GetFormID(),
						candidateBase ? candidateBase->GetFormID() : 0u,
						a_event->baseObj);
				}
			} else {
				SKSE::log::debug(
					"CalamityAffixes: raw container-event reference was neither a live handle nor a resolvable reference FormID (raw={:08X}, baseObj={:08X}).",
					candidateFormID,
					a_event->baseObj);
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
			if (consumedScroll) {
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
			const auto referencePlayerKey = referenceUniqueID != 0u ?
				MakeInstanceKey(playerId, referenceUniqueID) :
				std::uint64_t{ 0 };
			const auto legacyEventKey = a_event->baseObj != 0u && a_event->uniqueID != 0u ?
				MakeInstanceKey(a_event->baseObj, a_event->uniqueID) :
				std::uint64_t{ 0 };
			const std::array dropSourceCandidates{
				playerEventKey,
				referencePlayerKey,
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

			bool ambiguousBaseObjectRecovery = false;
			std::vector<std::uint64_t> ambiguousBaseObjectKeys;
			if (!ambiguousMaterialSources && dropSourceKey == 0u && a_event->baseObj != 0u) {
				std::int32_t remainingBaseItemCount = -1;
				if (auto* baseObject = RE::TESForm::LookupByID<RE::TESBoundObject>(a_event->baseObj); baseObject) {
					const auto currentInventory = player->GetInventory([baseFormID = a_event->baseObj](RE::TESBoundObject& a_object) {
						return a_object.GetFormID() == baseFormID;
					});
					if (const auto inventoryIt = currentInventory.find(baseObject); inventoryIt != currentInventory.end()) {
						remainingBaseItemCount = inventoryIt->second.first;
					} else {
						remainingBaseItemCount = 0;
					}
				}
				std::vector<InstanceBaseObjectRecoveryCandidate> recoveryCandidates;
				recoveryCandidates.reserve(_instanceTrackingState.instanceBaseObjects.size());
				for (const auto& [candidateKey, candidateBaseObject] : _instanceTrackingState.instanceBaseObjects) {
					recoveryCandidates.push_back(InstanceBaseObjectRecoveryCandidate{
						.key = candidateKey,
						.baseObjectFormID = candidateBaseObject,
						.materialState = HasMaterialInstanceState(candidateKey),
						.playerInventoryContainsKey = PlayerHasInstanceKey(candidateKey) ||
						HasPendingDropOwnership(_lootState.pendingDropOwnership, candidateKey),
					});
					if (candidateBaseObject == a_event->baseObj &&
						HasMaterialInstanceState(candidateKey) &&
						!PlayerHasInstanceKey(candidateKey) &&
						!HasPendingDropOwnership(_lootState.pendingDropOwnership, candidateKey)) {
						ambiguousBaseObjectKeys.push_back(candidateKey);
					}
				}
				const auto recovery = ResolveDroppedPlayerInstanceByBaseObject(
					playerId,
					a_event->baseObj,
					recoveryCandidates,
					a_event->itemCount,
					remainingBaseItemCount);
				if (recovery.key != 0u) {
					dropSourceKey = recovery.key;
					SKSE::log::info(
						"CalamityAffixes: recovered player-to-world state source by base object (baseObj={:08X}, sourceKey={:016X}, ref={:08X}).",
						a_event->baseObj,
						dropSourceKey,
						referenceFormID);
				} else if (recovery.ambiguous) {
					ambiguousBaseObjectRecovery = true;
					SKSE::log::warn(
						"CalamityAffixes: ambiguous base-object drop recovery; preserving candidate states instead of pruning (baseObj={:08X}, ref={:08X}).",
						a_event->baseObj,
						referenceFormID);
				}
			}

			const auto dropTransferDecision = ResolveDropTransfer(
				dropSourceKey,
				detachedWorldKey,
				ambiguousMaterialSources,
				dropSourceKey != 0u && IsInstanceKeyTracked(dropSourceKey),
				HasMaterialInstanceState(detachedWorldKey));
			const auto diagnosticSourceKey = dropSourceKey;
			SKSE::log::info(
				"CalamityAffixes: ownership drop identity event (baseObj={:08X}, oldContainer={:08X}, newContainer={:08X}, count={}, eventUid={}, refHandle={:016X}, refForm={:08X}, refExtraBase={:08X}, refExtraUid={}, sourceKey={:016X}, detachedKey={:016X}, decision={}).",
				a_event->baseObj,
				a_event->oldContainer,
				a_event->newContainer,
				a_event->itemCount,
				a_event->uniqueID,
				static_cast<std::uint64_t>(refHandle),
				referenceFormID,
				referenceUniqueID != 0u ? static_cast<RE::FormID>(referenceUniqueKey >> 16u) : 0u,
				referenceUniqueID,
				diagnosticSourceKey,
				detachedWorldKey,
				static_cast<unsigned>(dropTransferDecision));
			LogPlayerInventoryIdentitySnapshot(player, a_event->baseObj, "drop-callback-current");
			const auto pendingExpiryDay = detail::GetInGameDayStamp() + kPendingDropOwnershipLifetimeDays;
			auto retainPending = [&](std::uint64_t a_key, bool a_bindReferenceCandidate = false) {
				if (a_key == 0u || a_event->baseObj == 0u) {
					return;
				}
				const auto trackResult = TrackPendingDropOwnership(
					_lootState.pendingDropOwnership,
					a_key,
					a_event->baseObj,
					pendingExpiryDay,
					a_bindReferenceCandidate && a_event->itemCount == 1 ? referenceFormCandidate : 0u);
				if (trackResult == PendingDropTrackResult::kFull ||
					trackResult == PendingDropTrackResult::kConflict) {
					_lootState.pendingDropOwnershipOverflow = true;
					_lootState.pendingDropOwnershipOverflowExpiresAtDay =
						std::max(_lootState.pendingDropOwnershipOverflowExpiresAtDay, pendingExpiryDay);
					SKSE::log::error(
						"CalamityAffixes: pending drop identity ledger saturated or conflicting; preserving player-owned state under bounded overflow protection (key={:016X}, baseObj={:08X}).",
						a_key,
						a_event->baseObj);
				}
			};
			if (dropTransferDecision == DropTransferDecision::kConflictRetain) {
				SKSE::log::warn(
					"CalamityAffixes: blocked ambiguous or occupied player-to-world state transfer; preserving source and destination candidates (baseObj={:08X}, ref={:08X}, uniqueID={}, sourceKey={:016X}, detachedKey={:016X}).",
					a_event->baseObj,
					referenceFormID,
					a_event->uniqueID,
					dropSourceKey,
					detachedWorldKey);
				retainPending(dropSourceKey);
				if (ambiguousMaterialSources) {
					for (const auto candidateKey : dropSourceCandidates) {
						if (candidateKey != 0u && HasMaterialInstanceState(candidateKey)) {
							retainPending(candidateKey);
						}
					}
				}
				if (ambiguousBaseObjectRecovery) {
					for (const auto candidateKey : ambiguousBaseObjectKeys) {
						retainPending(candidateKey);
					}
				}
				dropSourceKey = 0u;
			} else if (dropTransferDecision == DropTransferDecision::kRetainSource) {
				retainPending(dropSourceKey, true);
				if (eventReference && diagnosticSourceKey != 0u && a_event->itemCount == 1) {
					const auto sourceOwner = static_cast<RE::FormID>(diagnosticSourceKey >> 16u);
					const auto sourceUniqueID = static_cast<std::uint16_t>(diagnosticSourceKey & 0xFFFFu);
					auto* referenceUID = eventReference->extraList.GetByType<RE::ExtraUniqueID>();
					const auto stampPlan = PlanDropReferenceIdentity(DropReferenceIdentityContext{
						.playerFormID = playerId,
						.sourceOwnerFormID = sourceOwner,
						.baseObjectFormID = a_event->baseObj,
						.referenceExtraOwnerFormID = referenceUID ? referenceUID->baseID : 0u,
						.sourceKey = diagnosticSourceKey,
						.referenceHandle = static_cast<std::uint64_t>(refHandle),
						.sourceUniqueID = sourceUniqueID,
						.referenceExtraUniqueID = referenceUID ? referenceUID->uniqueID : std::uint16_t{ 0u },
						.itemCount = a_event->itemCount
					});
					if (stampPlan == DropReferenceIdentityAction::kAddIdentity) {
						if (!referenceUID) {
							referenceUID = RE::BSExtraData::Create<RE::ExtraUniqueID>();
							if (referenceUID) {
								eventReference->extraList.Add(referenceUID);
							}
						}
						if (referenceUID) {
							referenceUID->baseID = playerId;
							referenceUID->uniqueID = sourceUniqueID;
						}
						const auto* verifiedUID = eventReference->extraList.GetByType<RE::ExtraUniqueID>();
						if (verifiedUID && verifiedUID->baseID == playerId && verifiedUID->uniqueID == sourceUniqueID) {
							const bool referenceBound = EstablishPendingDropReferenceIdentity(
								_lootState.pendingDropOwnership,
								diagnosticSourceKey,
								a_event->baseObj,
								eventReference->GetFormID(),
								detail::GetInGameDayStamp());
							SKSE::log::info(
								"CalamityAffixes: stamped dropped reference with exact owner+UID before pickup (baseObj={:08X}, refForm={:08X}, sourceKey={:016X}, pendingBound={}).",
								a_event->baseObj,
								eventReference->GetFormID(),
								diagnosticSourceKey,
								referenceBound);
						} else {
							SKSE::log::error(
								"CalamityAffixes: failed to verify exact owner+UID stamp on dropped reference (baseObj={:08X}, refHandle={:016X}, sourceKey={:016X}).",
								a_event->baseObj,
								static_cast<std::uint64_t>(refHandle),
								diagnosticSourceKey);
						}
					} else if (stampPlan == DropReferenceIdentityAction::kAlreadyExact) {
						const bool referenceBound = EstablishPendingDropReferenceIdentity(
							_lootState.pendingDropOwnership,
							diagnosticSourceKey,
							a_event->baseObj,
							eventReference->GetFormID(),
							detail::GetInGameDayStamp());
						SKSE::log::info(
							"CalamityAffixes: dropped reference already carries exact owner+UID (baseObj={:08X}, refForm={:08X}, sourceKey={:016X}, pendingBound={}).",
							a_event->baseObj,
							eventReference->GetFormID(),
							diagnosticSourceKey,
							referenceBound);
					} else {
						SKSE::log::warn(
							"CalamityAffixes: refused dropped-reference identity stamp (baseObj={:08X}, refHandle={:016X}, sourceKey={:016X}, plan={}).",
							a_event->baseObj,
							static_cast<std::uint64_t>(refHandle),
							diagnosticSourceKey,
							static_cast<unsigned>(stampPlan));
					}
				} else {
					SKSE::log::warn(
						"CalamityAffixes: could not stamp a dropped reference because exact singleton identity or reference was unavailable (baseObj={:08X}, sourceKey={:016X}, refHandle={:016X}, count={}).",
						a_event->baseObj,
						diagnosticSourceKey,
						static_cast<std::uint64_t>(refHandle),
						a_event->itemCount);
				}
				SKSE::log::warn(
					"CalamityAffixes: retained player-to-world state because the dropped reference had no stable FormID (baseObj={:08X}, uniqueID={}, sourceKey={:016X}); handoff remains unresolved.",
					a_event->baseObj,
					a_event->uniqueID,
					dropSourceKey);
				dropSourceKey = 0u;
			} else if (dropTransferDecision == DropTransferDecision::kRemap) {
				// Do not move material state to a synthetic FF/FFFF key here. Skyrim's
				// pickup event can arrive with both UID and reference handle zero, so
				// the exact pending source key must remain available until the concrete
				// reference has been stamped and its returned inventory identity proven.
				retainPending(dropSourceKey, true);
				RememberInstanceBaseObject(dropSourceKey, a_event->baseObj);
				if (eventReference && eventReference->GetFormID() == referenceFormCandidate &&
					eventReference->GetBaseObject() &&
					eventReference->GetBaseObject()->GetFormID() == a_event->baseObj &&
					a_event->itemCount == 1) {
					const auto sourceOwner = static_cast<RE::FormID>(dropSourceKey >> 16u);
					const auto sourceUniqueID = static_cast<std::uint16_t>(dropSourceKey & 0xFFFFu);
					auto* referenceUID = eventReference->extraList.GetByType<RE::ExtraUniqueID>();
					const auto stampPlan = PlanDropReferenceIdentity(DropReferenceIdentityContext{
						.playerFormID = playerId,
						.sourceOwnerFormID = sourceOwner,
						.baseObjectFormID = a_event->baseObj,
						.referenceExtraOwnerFormID = referenceUID ? referenceUID->baseID : 0u,
						.sourceKey = dropSourceKey,
						.referenceHandle = static_cast<std::uint64_t>(eventReference->GetHandle().native_handle()),
						.sourceUniqueID = sourceUniqueID,
						.referenceExtraUniqueID = referenceUID ? referenceUID->uniqueID : std::uint16_t{ 0u },
						.itemCount = a_event->itemCount
					});
					if (stampPlan == DropReferenceIdentityAction::kAddIdentity) {
						if (!referenceUID) {
							referenceUID = RE::BSExtraData::Create<RE::ExtraUniqueID>();
							if (referenceUID) {
								eventReference->extraList.Add(referenceUID);
							}
						}
						if (referenceUID) {
							referenceUID->baseID = playerId;
							referenceUID->uniqueID = sourceUniqueID;
						}
					}
					const auto* verifiedUID = eventReference->extraList.GetByType<RE::ExtraUniqueID>();
					const bool exactMarker = verifiedUID && verifiedUID->baseID == playerId &&
						verifiedUID->uniqueID == sourceUniqueID &&
						(stampPlan == DropReferenceIdentityAction::kAddIdentity ||
						 stampPlan == DropReferenceIdentityAction::kAlreadyExact);
					const bool referenceBound = exactMarker && EstablishPendingDropReferenceIdentity(
						_lootState.pendingDropOwnership,
						dropSourceKey,
						a_event->baseObj,
						eventReference->GetFormID(),
						detail::GetInGameDayStamp());
					SKSE::log::info(
						"CalamityAffixes: kRemap handoff retained exact source state and entered pending lifecycle (baseObj={:08X}, candidate={:08X}, sourceKey={:016X}, markerVerified={}, referenceBound={}).",
						a_event->baseObj,
						referenceFormCandidate,
						dropSourceKey,
						exactMarker,
						referenceBound);
				} else {
					SKSE::log::info(
						"CalamityAffixes: kRemap handoff retained exact source state pending concrete reference activation (baseObj={:08X}, candidate={:08X}, sourceKey={:016X}).",
						a_event->baseObj,
						referenceFormCandidate,
						dropSourceKey);
				}
			}
		if (ambiguousBaseObjectRecovery) {
			for (const auto candidateKey : ambiguousBaseObjectKeys) {
				retainPending(candidateKey);
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

			const bool pendingSourceHasExactBase = diagnosticSourceKey != 0u &&
				std::ranges::any_of(_lootState.pendingDropOwnership, [&](const auto& a_pending) {
					return a_pending.instanceKey == diagnosticSourceKey &&
						a_pending.baseObjectFormID == a_event->baseObj;
				});
			const auto guardedDropStateKey = IsInstanceKeyTracked(detachedWorldKey) ? detachedWorldKey :
				(pendingSourceHasExactBase ? diagnosticSourceKey : std::uint64_t{ 0u });
			if (refHandle != 0) {
				_lootState.rerollGuard.NotePlayerDrop(
					playerId,
					a_event->oldContainer,
					a_event->newContainer,
					a_event->itemCount,
					refHandle,
					guardedDropStateKey);
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

		if (a_event->newContainer != playerId) {
			return RE::BSEventNotifyControl::kContinue;
		}

		// Only an exact owner+UID key or exact detached reference can resolve a
		// pending drop. A matching base FormID alone never transfers state.
		const auto referenceDetachedWorldKey = MakeDetachedWorldInstanceKey(referenceFormID);
		const auto eventPlayerKey = a_event->uniqueID != 0u ?
			MakeInstanceKey(playerId, a_event->uniqueID) :
			(referenceUniqueID != 0u ? MakeInstanceKey(playerId, referenceUniqueID) : std::uint64_t{ 0u });
		if (std::ranges::any_of(_lootState.pendingDropOwnership, [&](const auto& a_pending) {
				return a_pending.baseObjectFormID == a_event->baseObj;
			})) {
			SKSE::log::info(
				"CalamityAffixes: ownership pickup identity event (baseObj={:08X}, oldContainer={:08X}, newContainer={:08X}, count={}, eventUid={}, eventPlayerKey={:016X}, refHandle={:016X}, refForm={:08X}, refExtraBase={:08X}, refExtraUid={}, detachedKey={:016X}).",
				a_event->baseObj,
				a_event->oldContainer,
				a_event->newContainer,
				a_event->itemCount,
				a_event->uniqueID,
				eventPlayerKey,
				static_cast<std::uint64_t>(refHandle),
				referenceFormID,
				referenceUniqueID != 0u ? static_cast<RE::FormID>(referenceUniqueKey >> 16u) : 0u,
				referenceUniqueID,
				referenceDetachedWorldKey);
			LogPlayerInventoryIdentitySnapshot(player, a_event->baseObj, "pickup-callback-current");
		}
		if (eventPlayerKey != 0u && PlayerHasExactDropPickupIdentity(eventPlayerKey, a_event->baseObj) &&
			ResolvePendingDropWithExactInventoryKey(
				_lootState.pendingDropOwnership,
				eventPlayerKey,
				a_event->baseObj)) {
			const auto retiredDropReferences = _lootState.rerollGuard.ForgetPlayerDropReferencesByInstanceKey(eventPlayerKey);
			RememberInstanceBaseObject(eventPlayerKey, a_event->baseObj);
			RebuildActiveCounts();
			SKSE::log::info(
				"CalamityAffixes: resolved dropped-item handoff using exact pickup owner+UID+base evidence (baseObj={:08X}, key={:016X}, retiredDropHandles={}).",
				a_event->baseObj,
				eventPlayerKey,
				retiredDropReferences);
			return RE::BSEventNotifyControl::kContinue;
		}

		// Consume the exact dropped-reference association before the generic
		// unresolved-base guard below. A UID can change between the world and
		// player containers even when TESContainerChangedEvent.uniqueID is zero.
		// The original drop source is accepted only because this exact reference
		// handle was recorded against that owner+UID key; the destination must be
		// a singleton inventory ExtraUniqueID on this same base.
		std::optional<std::uint64_t> guardedDropKey;
		if (a_event->oldContainer == LootRerollGuard::kWorldContainer && refHandle != 0u) {
			guardedDropKey = _lootState.rerollGuard.ConsumePlayerDropPickupInstanceKey(
				playerId,
				a_event->oldContainer,
				a_event->newContainer,
				a_event->itemCount,
				refHandle);
		}
		if (guardedDropKey && *guardedDropKey != 0u && a_event->itemCount == 1 &&
			HasPendingDropOwnership(_lootState.pendingDropOwnership, *guardedDropKey, a_event->baseObj) &&
			!HasMaterialInstanceState(*guardedDropKey)) {
			const auto targetUniqueID = a_event->uniqueID != 0u ? a_event->uniqueID :
				((referenceUniqueKey >> 16u) == playerId ? referenceUniqueID : std::uint16_t{ 0u });
			const auto targetKey = targetUniqueID != 0u ? MakeInstanceKey(playerId, targetUniqueID) : std::uint64_t{ 0u };
			if (targetKey != 0u && targetKey != *guardedDropKey &&
				!IsInstanceKeyTracked(targetKey) &&
				PlayerHasExactDropPickupIdentity(targetKey, a_event->baseObj) &&
				RemapInstanceKey(*guardedDropKey, targetKey)) {
				const auto pendingResult = ApplyExactPendingDropIdentityTransfer(
					_lootState.pendingDropOwnership,
					*guardedDropKey,
					targetKey,
					a_event->baseObj,
					false,
					true);
				if (pendingResult == PendingDropTransferResult::kResolved) {
					RememberInstanceBaseObject(targetKey, a_event->baseObj);
					RebuildActiveCounts();
					SKSE::log::info(
						"CalamityAffixes: resolved changed-UID pickup through exact dropped-reference handle and singleton inventory ExtraUniqueID (baseObj={:08X}, sourceKey={:016X}, targetKey={:016X}).",
						a_event->baseObj,
						*guardedDropKey,
						targetKey);
					return RE::BSEventNotifyControl::kContinue;
				}
				SKSE::log::error(
					"CalamityAffixes: destination identity remap succeeded but the exact pending transfer could not be resolved (sourceKey={:016X}, targetKey={:016X}, baseObj={:08X}, result={}).",
					*guardedDropKey,
					targetKey,
					a_event->baseObj,
					static_cast<unsigned>(pendingResult));
				return RE::BSEventNotifyControl::kContinue;
			}
		}
		// The event UID can be zero even when Skyrim's inventory ExtraList has
		// already restored the original owner+UID. Match that exact singleton
		// identity and base, never infer ownership from the base alone.
		if (a_event->oldContainer == LootRerollGuard::kWorldContainer) {
			std::vector<PendingDropInventoryCandidate> pendingCandidates;
			for (const auto& pending : _lootState.pendingDropOwnership) {
				pendingCandidates.push_back({ pending.instanceKey, pending.baseObjectFormID });
			}
			const auto inventoryIdentities = CollectPlayerDropPickupInventoryIdentities(player, a_event->baseObj);
			const auto exactResolution = ResolveUniquePendingDropFromInventory(
				pendingCandidates,
				inventoryIdentities,
				a_event->baseObj);
			if (!exactResolution.ambiguous && exactResolution.instanceKey != 0u &&
				ResolvePendingDropWithExactInventoryKey(
					_lootState.pendingDropOwnership,
					exactResolution.instanceKey,
					a_event->baseObj)) {
				const auto retiredDropReferences = _lootState.rerollGuard.ForgetPlayerDropReferencesByInstanceKey(exactResolution.instanceKey);
				RememberInstanceBaseObject(exactResolution.instanceKey, a_event->baseObj);
				RebuildActiveCounts();
				SKSE::log::info(
					"CalamityAffixes: resolved dropped-item handoff from the exact singleton inventory ExtraUniqueID (baseObj={:08X}, key={:016X}, eventUid={}, retiredDropHandles={}).",
					a_event->baseObj,
					exactResolution.instanceKey,
					a_event->uniqueID,
					retiredDropReferences);
				return RE::BSEventNotifyControl::kContinue;
			}
			if (exactResolution.ambiguous) {
				SKSE::log::warn(
					"CalamityAffixes: refused zero-UID pickup because multiple pending exact ExtraUniqueID identities for the same base are present (baseObj={:08X}).",
					a_event->baseObj);
			}
		}
		for (const auto& pending : _lootState.pendingDropOwnership) {
			if (pending.baseObjectFormID != a_event->baseObj) {
				continue;
			}
			if (referenceDetachedWorldKey != 0u && pending.instanceKey == referenceDetachedWorldKey) {
				continue;
			}
			SKSE::log::warn(
				"CalamityAffixes: pickup base matches an unresolved drop but lacks the recorded owner+UID identity; retained state and suppressed loot roll (baseObj={:08X}, pendingKey={:016X}).",
				a_event->baseObj,
				pending.instanceKey);
			return RE::BSEventNotifyControl::kContinue;
		}
		if (_lootState.pendingDropOwnershipOverflow &&
			detail::GetInGameDayStamp() < _lootState.pendingDropOwnershipOverflowExpiresAtDay) {
			SKSE::log::warn("CalamityAffixes: skipped unproven loot roll while bounded drop ownership ledger is saturated (baseObj={:08X}).", a_event->baseObj);
			return RE::BSEventNotifyControl::kContinue;
		}

		// 3) Reattach state only when this is the exact world reference the player dropped.
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
		const bool trackedDetachedWorldState = IsInstanceKeyTracked(detachedWorldKey);
		if (guardedDropKey.has_value() || trackedDetachedWorldState) {
			auto pickupKey = a_event->uniqueID != 0u ?
				MakeInstanceKey(playerId, a_event->uniqueID) :
				(referenceUniqueID != 0u ?
					MakeInstanceKey(playerId, referenceUniqueID) :
					((referenceUniqueKey >> 16u) == playerId ? referenceUniqueKey : std::uint64_t{ 0 }));
			if (trackedDetachedWorldState && pickupKey != 0u) {
				if (HasMaterialInstanceState(pickupKey)) {
					SKSE::log::warn(
						"CalamityAffixes: blocked occupied world-to-player state transfer; preserving both states (baseObj={:08X}, ref={:08X}, detachedKey={:016X}, pickupKey={:016X}).",
						a_event->baseObj,
						referenceFormID,
						detachedWorldKey,
						pickupKey);
				} else {
					RememberInstanceBaseObject(detachedWorldKey, a_event->baseObj);
					if (RemapInstanceKey(detachedWorldKey, pickupKey)) {
						(void)ApplyExactPendingDropIdentityTransfer(
							_lootState.pendingDropOwnership,
							detachedWorldKey,
							pickupKey,
							a_event->baseObj,
							false,
							true);
						RebuildActiveCounts();
					} else {
						SKSE::log::warn(
							"CalamityAffixes: retained world instance state because exact pickup-key remap was refused (world={:016X}, pickup={:016X}).",
							detachedWorldKey,
							pickupKey);
					}
				}
			} else if (trackedDetachedWorldState) {
				SKSE::log::warn(
					"CalamityAffixes: retained detached world state because pickup had no stable player UID (baseObj={:08X}, ref={:08X}, detachedKey={:016X}).",
					a_event->baseObj,
					referenceFormID,
					detachedWorldKey);
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
		ExpirePendingDropOwnership(detail::GetInGameDayStamp());

		const auto oldOwnerID = a_event->oldBaseID;
		const auto newOwnerID = a_event->newBaseID;
		const auto itemFormID = a_event->objectID;
		if (itemFormID != 0u && std::ranges::any_of(
				_lootState.pendingDropOwnership,
				[&](const auto& a_pending) { return a_pending.baseObjectFormID == itemFormID; })) {
			SKSE::log::info(
				"CalamityAffixes: ownership unique-ID boundary (oldOwner={:08X}, newOwner={:08X}, item={:08X}, oldUid={}, newUid={}, oldKey={:016X}, newKey={:016X}).",
				oldOwnerID,
				newOwnerID,
				itemFormID,
				a_event->oldUniqueID,
				a_event->newUniqueID,
				MakeInstanceKey(oldOwnerID, a_event->oldUniqueID),
				MakeInstanceKey(newOwnerID, a_event->newUniqueID));
			LogPlayerInventoryIdentitySnapshot(RE::PlayerCharacter::GetSingleton(), itemFormID, "unique-id-change-callback-current");
		}
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
				"CalamityAffixes: blocked ambiguous canonical+legacy source collision; all involved states were retained (oldOwner={:08X}, newOwner={:08X}, item={:08X}, oldUid={}, newUid={}, oldKey={:016X}, legacyKey={:016X}, newKey={:016X}).",
				oldOwnerID,
				newOwnerID,
				itemFormID,
				a_event->oldUniqueID,
				a_event->newUniqueID,
				oldKey,
				legacyItemKey,
				newKey);
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
				"CalamityAffixes: blocked instance-key remap collision; both states were retained instead of merged or discarded (oldOwner={:08X}, newOwner={:08X}, item={:08X}, oldUid={}, newUid={}, oldKey={:016X}, newKey={:016X}).",
				oldOwnerID,
				newOwnerID,
				itemFormID,
				a_event->oldUniqueID,
				a_event->newUniqueID,
				transferPlan.oldKey,
				transferPlan.newKey);
			return RE::BSEventNotifyControl::kContinue;
		}

		RememberInstanceBaseObject(transferPlan.oldKey, itemFormID);
		if (!RemapInstanceKey(transferPlan.oldKey, transferPlan.newKey)) {
			return RE::BSEventNotifyControl::kContinue;
		}
		const auto pendingTransfer = ApplyExactPendingDropIdentityTransfer(
			_lootState.pendingDropOwnership,
			transferPlan.oldKey,
			transferPlan.newKey,
			itemFormID,
			materialNew,
			newOwnerID == playerId);
		if (pendingTransfer == PendingDropTransferResult::kConflict) {
			SKSE::log::warn(
				"CalamityAffixes: exact unique-ID event remapped material state but pending handoff ledger was ambiguous (item={:08X}, old={:016X}, new={:016X}); retained ledger for expiry review.",
				itemFormID,
				transferPlan.oldKey,
				transferPlan.newKey);
		}
		RebuildActiveCounts();
		return RE::BSEventNotifyControl::kContinue;
	}

	void EventBridge::RememberInstanceBaseObject(
		std::uint64_t a_instanceKey,
		RE::FormID a_baseObjectFormID)
	{
		if (a_instanceKey == 0u || a_baseObjectFormID == 0u) {
			return;
		}
		_instanceTrackingState.instanceBaseObjects[a_instanceKey] = a_baseObjectFormID;
	}

	std::optional<std::uint64_t> EventBridge::ResolveUniqueUnboundMarkedPlayerInstanceKey(
		RE::FormID a_baseObjectFormID) const
	{
		if (a_baseObjectFormID == 0u) {
			return std::nullopt;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return std::nullopt;
		}
		auto* changes = player->GetInventoryChanges();
		if (!changes || !changes->entryList) {
			return std::nullopt;
		}

		std::optional<std::uint64_t> match;
		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->object || entry->object->GetFormID() != a_baseObjectFormID || !entry->extraLists) {
				continue;
			}
			for (auto* xList : *entry->extraLists) {
				if (!xList) {
					continue;
				}
				const auto* uid = xList->GetByType<RE::ExtraUniqueID>();
				if (!uid || uid->baseID == 0u || uid->uniqueID == 0u) {
					continue;
				}
				const char* displayNameRaw = xList->GetDisplayName(entry->object);
				const std::string_view displayName = displayNameRaw ? displayNameRaw : "";
				if (!HasLeadingLootStarPrefix(displayName) && !HasTrailingLootStarMarker(displayName)) {
					continue;
				}
				const auto key = MakeInstanceKey(uid->baseID, uid->uniqueID);
				if (HasMaterialInstanceState(key)) {
					continue;
				}
				if (match && *match != key) {
					return std::nullopt;
				}
				match = key;
			}
		}
		return match;
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
		_instanceTrackingState.instanceBaseObjects.erase(a_instanceKey);
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

	bool EventBridge::RemapInstanceKey(std::uint64_t a_oldKey, std::uint64_t a_newKey)
	{
		if (a_oldKey == 0 || a_newKey == 0 || a_oldKey == a_newKey) {
			return false;
		}
		if (HasMaterialInstanceState(a_newKey)) {
			SKSE::log::warn(
				"CalamityAffixes: refused occupied instance-key remap; destination state was preserved (old={:016X}, new={:016X}).",
				a_oldKey,
				a_newKey);
			return false;
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

		if (!RemapInstanceKeyedMapEntries(
				_instanceTrackingState.instanceAffixes,
				a_oldKey,
				a_newKey,
				[](std::uint64_t a_key) { return a_key; },
				[](std::uint64_t, std::uint64_t a_newKeyValue) { return a_newKeyValue; })) {
			return false;
		}
		if (auto baseNode = _instanceTrackingState.instanceBaseObjects.extract(a_oldKey); !baseNode.empty()) {
			_instanceTrackingState.instanceBaseObjects.erase(a_newKey);
			baseNode.key() = a_newKey;
			_instanceTrackingState.instanceBaseObjects.insert(std::move(baseNode));
		}

		// The destination owns no affixes (checked above), so any count there is stale.
		auto countNode = _instanceTrackingState.selectedReforgeCounts.extract(a_oldKey);
		_instanceTrackingState.selectedReforgeCounts.erase(a_newKey);
		if (!countNode.empty()) {
			countNode.key() = a_newKey;
			_instanceTrackingState.selectedReforgeCounts.insert(std::move(countNode));
		}

		if (!RemapInstanceKeyedMapEntries(
				_instanceTrackingState.instanceStates,
				a_oldKey,
				a_newKey,
				[](const InstanceStateKey& a_key) { return a_key.instanceKey; },
				[](const InstanceStateKey& a_key, std::uint64_t a_newKeyValue) {
					return MakeInstanceStateKey(a_newKeyValue, a_key.affixToken);
				})) {
			return false;
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
		return true;
	}


}
