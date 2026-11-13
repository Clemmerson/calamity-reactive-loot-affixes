#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/LootRollSelection.h"
#include "CalamityAffixes/PluginEditorIds.h"
#include "EventBridge.Loot.Runeword.Detail.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace CalamityAffixes
{
	using namespace RunewordDetail;

	namespace
	{
		struct ScopedAffixExpansionInFlight
		{
			bool& flag;

			explicit ScopedAffixExpansionInFlight(bool& a_flag) :
				flag(a_flag)
			{
				flag = true;
			}

			~ScopedAffixExpansionInFlight()
			{
				flag = false;
			}
		};
	}

	EventBridge::OperationResult EventBridge::ExpandSelectedRunewordBaseAffixes(
		std::uint64_t a_expectedInstanceKey,
		std::uint8_t a_expectedRegularAffixCount)
	{
		return ExpandSelectedRunewordBaseAffixes(a_expectedInstanceKey, a_expectedRegularAffixCount, {});
	}

	EventBridge::OperationResult EventBridge::ExpandSelectedRunewordBaseAffixes(
		std::uint64_t a_expectedInstanceKey,
		std::uint8_t a_expectedRegularAffixCount,
		std::string_view a_expectedAffixSignature)
	{
		const std::scoped_lock lock(_stateMutex);
		OperationResult result{};
		if (!_configLoaded || !_runtimeSettings.enabled.load(std::memory_order_relaxed)) {
			result.message = "Affix expansion unavailable: Calamity runtime is disabled or not ready.";
			return result;
		}
		if (_runewordState.transmuteInProgress || _runewordState.affixExpansionInProgress) {
			result.message = "Affix expansion unavailable: another item mutation is in progress.";
			return result;
		}
		if (!detail::ResolveRegularAffixExpansionPolicy(a_expectedRegularAffixCount) ||
			a_expectedInstanceKey == 0u) {
			result.message = "Affix expansion failed: invalid expected base or affix count.";
			return result;
		}
		if (_loot.stripTrackedSuffixSlots) {
			result.message = "Affix expansion failed: suffix slots are disabled by runtime policy.";
			return result;
		}

		ScopedAffixExpansionInFlight expansionGuard(_runewordState.affixExpansionInProgress);
		SanitizeRunewordState();

		std::uint64_t instanceKey = 0u;
		RE::InventoryEntryData* entry = nullptr;
		RE::ExtraDataList* xList = nullptr;
		std::string baseResolveFailure;
		if (!ResolveSelectedRunewordBaseInstance(instanceKey, entry, xList, &baseResolveFailure, true)) {
			result.message = "Affix expansion failed: " + baseResolveFailure;
			return result;
		}
		if (instanceKey != a_expectedInstanceKey) {
			result.message = "Affix expansion failed: selected base changed; try again.";
			return result;
		}

		const auto slotsIt = _instanceTrackingState.instanceAffixes.find(instanceKey);
		if (slotsIt == _instanceTrackingState.instanceAffixes.end()) {
			result.message = "Affix expansion failed: identify this base with a Scroll of Identification first.";
			return result;
		}
		const InstanceAffixSlots previousSlots = slotsIt->second;
		if (!detail::HasUniqueAffixTokens(previousSlots)) {
			result.message = "Affix expansion failed: invalid affix layout.";
			return result;
		}

		const auto* completedRunewordRecipe = ResolveCompletedRunewordRecipe(instanceKey);
		const std::uint64_t preservedRunewordToken =
			completedRunewordRecipe ? completedRunewordRecipe->resultAffixToken : 0u;
		const std::uint8_t expectedRunewordCount = preservedRunewordToken != 0u ? 1u : 0u;

		InstanceAffixSlots regularSlots{};
		std::uint8_t runewordCount = 0u;
		std::uint8_t prefixCount = 0u;
		std::uint8_t suffixCount = 0u;
		std::vector<std::size_t> chosenIndices;
		std::vector<std::string> chosenFamilies;
		chosenIndices.reserve(previousSlots.count);
		chosenFamilies.reserve(previousSlots.count);

		for (std::uint8_t i = 0u; i < previousSlots.count; ++i) {
			const auto token = previousSlots.tokens[i];
			if (token == 0u) {
				result.message = "Affix expansion failed: invalid affix layout.";
				return result;
			}
			if (_runewordState.recipeIndexByResultAffixToken.contains(token)) {
				++runewordCount;
				if (token != preservedRunewordToken) {
					result.message = "Affix expansion failed: conflicting runeword state.";
					return result;
				}
				continue;
			}

			const auto affixIt = _affixRuntimeState.affixRegistry.affixIndexByToken.find(token);
			if (affixIt == _affixRuntimeState.affixRegistry.affixIndexByToken.end() ||
				affixIt->second >= _affixRuntimeState.affixes.size()) {
				result.message = "Affix expansion failed: unresolved regular affix; standard reforge it first.";
				return result;
			}

			const auto& affix = _affixRuntimeState.affixes[affixIt->second];
			if (affix.slot == AffixSlot::kPrefix) {
				++prefixCount;
			} else if (affix.slot == AffixSlot::kSuffix) {
				++suffixCount;
				if (affix.family.empty() ||
					std::find(chosenFamilies.begin(), chosenFamilies.end(), affix.family) != chosenFamilies.end()) {
					result.message = "Affix expansion failed: invalid suffix families; standard reforge it first.";
					return result;
				}
				chosenFamilies.push_back(affix.family);
			} else {
				result.message = "Affix expansion failed: non-regular affix in regular slots.";
				return result;
			}

			if (!regularSlots.AddToken(token)) {
				result.message = "Affix expansion failed: invalid regular affix slots.";
				return result;
			}
			chosenIndices.push_back(affixIt->second);
		}

		if (runewordCount != expectedRunewordCount ||
			(preservedRunewordToken != 0u && previousSlots.GetPrimary() != preservedRunewordToken) ||
			previousSlots.count != static_cast<std::uint8_t>(regularSlots.count + runewordCount)) {
			result.message = "Affix expansion failed: corrupted runeword or affix slot state.";
			return result;
		}
		if (!detail::IsExpectedAffixExpansionState(
				a_expectedInstanceKey,
				instanceKey,
				a_expectedRegularAffixCount,
				regularSlots.count)) {
			result.message = "Affix expansion failed: affix count changed; refresh and try again.";
			return result;
		}
		if (!a_expectedAffixSignature.empty()) {
			std::string currentSignature;
			for (const auto token : regularSlots) {
				if (!currentSignature.empty()) currentSignature.push_back(',');
				currentSignature += std::to_string(token);
			}
			if (currentSignature.empty()) currentSignature = "-";
			if (currentSignature != a_expectedAffixSignature) {
				result.message = "Affixes changed; review the current state and try again.";
				return result;
			}
		}
		if (!detail::IsCanonicalRegularAffixExpansionLayout(
				regularSlots.count,
				prefixCount,
				suffixCount)) {
			result.message = "Affix expansion failed: invalid affix layout.";
			return result;
		}

		const auto expansionPolicy = detail::ResolveRegularAffixExpansionPolicy(regularSlots.count);
		if (!expansionPolicy ||
			expansionPolicy->targetRegularAffixCount > kMaxRegularAffixesPerItem ||
			previousSlots.count >= kMaxAffixesPerItem) {
			result.message = "Affix expansion failed: regular affix slots are already at maximum.";
			return result;
		}

		const auto lootType = ResolveInstanceLootType(instanceKey);
		if (!lootType) {
			result.message = "Affix expansion failed: unable to resolve item type.";
			return result;
		}
		const auto weaponSubtype =
			detail::ResolveWeaponSubtype(entry->object->As<RE::TESObjectWEAP>());

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			result.message = "Affix expansion failed: player not available.";
			return result;
		}
		auto* orb = PluginEditorIds::Lookup<RE::TESObjectMISC>("CAFF_Misc_ReforgeOrb");
		if (!orb) {
			result.message = "Affix expansion failed: orb item missing.";
			SKSE::log::error("CalamityAffixes: affix expansion orb item missing (editorId=CAFF_Misc_ReforgeOrb).");
			return result;
		}

		const auto ownedBefore = std::max(0, player->GetItemCount(orb));
		const auto orbCost = expansionPolicy->orbCost;
		if (static_cast<std::uint32_t>(ownedBefore) < orbCost) {
			result.message = "Affix expansion failed: requires " + std::to_string(orbCost) + " Reforge Orbs.";
			return result;
		}

		const auto suffixSharedBagBefore = _lootState.suffixSharedBag;
		const auto suffixWeaponBagBefore = _lootState.suffixWeaponBag;
		const auto suffixArmorBagBefore = _lootState.suffixArmorBag;
		auto restoreSuffixShuffleBags = [&]() {
			_lootState.suffixSharedBag = suffixSharedBagBefore;
			_lootState.suffixWeaponBag = suffixWeaponBagBefore;
			_lootState.suffixArmorBag = suffixArmorBagBefore;
		};

		std::mt19937 rngBefore{};
		std::mt19937 rngAfter{};
		std::size_t newAffixIndex = 0u;
		InstanceAffixSlots newSlots = previousSlots;
		bool candidateAccepted = false;
		{
			std::lock_guard<std::mutex> rngLock(_rngMutex);
			rngBefore = _rng;
			const auto candidate = RollSuffixIndex(*lootType, weaponSubtype, &chosenFamilies, &chosenIndices);
			if (candidate && *candidate < _affixRuntimeState.affixes.size()) {
				const auto& affix = _affixRuntimeState.affixes[*candidate];
				const bool familyAvailable = !affix.family.empty() &&
					std::find(chosenFamilies.begin(), chosenFamilies.end(), affix.family) == chosenFamilies.end();
				if (affix.slot == AffixSlot::kSuffix && familyAvailable &&
					!_runewordState.recipeIndexByResultAffixToken.contains(affix.token) &&
					newSlots.AddToken(affix.token)) {
					newAffixIndex = *candidate;
					candidateAccepted = true;
				}
			}

			const auto expectedTotalCount = static_cast<std::uint8_t>(
				expansionPolicy->targetRegularAffixCount + runewordCount);
			if (!candidateAccepted || newSlots.count != expectedTotalCount ||
				!detail::HasUniqueAffixTokens(newSlots) ||
				(preservedRunewordToken != 0u &&
					(newSlots.GetPrimary() != preservedRunewordToken || !newSlots.HasToken(preservedRunewordToken)))) {
				candidateAccepted = false;
				_rng = rngBefore;
			}
			rngAfter = _rng;
		}

		if (!candidateAccepted) {
			restoreSuffixShuffleBags();
			result.message = "Affix expansion failed: no eligible different-family suffix is available.";
			return result;
		}

		auto restoreRollStateAfterEngineFailure = [&](std::string_view a_reason) {
			restoreSuffixShuffleBags();
			std::lock_guard<std::mutex> rngLock(_rngMutex);
			if (_rng == rngAfter) {
				_rng = rngBefore;
				return;
			}
			SKSE::log::warn(
				"CalamityAffixes: affix expansion {} left RNG advanced because another thread consumed randomness after candidate generation.",
				a_reason);
		};

		player->RemoveItem(
			orb,
			static_cast<std::int32_t>(orbCost),
			RE::ITEM_REMOVE_REASON::kRemove,
			nullptr,
			nullptr);
		const auto ownedAfter = std::max(0, player->GetItemCount(orb));
		if (!detail::DidConsumeExactInventoryCount(
				static_cast<std::uint32_t>(ownedBefore),
				static_cast<std::uint32_t>(ownedAfter),
				orbCost)) {
			const auto refundBefore = ownedAfter;
			auto refundAfter = refundBefore;
			const auto observedConsumed = detail::ResolveObservedInventoryConsumption(
				static_cast<std::uint32_t>(ownedBefore),
				static_cast<std::uint32_t>(ownedAfter));
			if (observedConsumed > 0u) {
				player->AddObjectToContainer(
					orb,
					nullptr,
					static_cast<std::int32_t>(observedConsumed),
					nullptr);
				refundAfter = std::max(0, player->GetItemCount(orb));
			}
			const bool refundConfirmed =
				refundAfter == ownedBefore &&
				(observedConsumed == 0u ||
					detail::DidRestoreExactInventoryCount(
						static_cast<std::uint32_t>(refundBefore),
						static_cast<std::uint32_t>(refundAfter),
						observedConsumed));
			restoreRollStateAfterEngineFailure("currency rollback");
			result.message = refundConfirmed
				? "Affix expansion failed: Reforge Orb consumption was not confirmed; inventory was restored."
				: "Affix expansion failed: Reforge Orb consumption was not confirmed; check inventory.";
			SKSE::log::error(
				"CalamityAffixes: affix expansion aborted because orb inventory delta did not match the exact cost "
				"(cost={}, before={}, after={}, observedConsumed={}, refundAfter={}, refundConfirmed={}, instance={:016X}).",
				orbCost,
				ownedBefore,
				ownedAfter,
				observedConsumed,
				refundAfter,
				refundConfirmed,
				instanceKey);
			return result;
		}

		std::uint64_t revalidatedInstanceKey = 0u;
		RE::InventoryEntryData* revalidatedEntry = nullptr;
		RE::ExtraDataList* revalidatedXList = nullptr;
		std::string revalidateFailure;
		const bool baseStillValid = ResolveSelectedRunewordBaseInstance(
			revalidatedInstanceKey,
			revalidatedEntry,
			revalidatedXList,
			&revalidateFailure,
			true);
		const auto currentSlotsIt = _instanceTrackingState.instanceAffixes.find(instanceKey);
		const bool slotsStillMatch = currentSlotsIt != _instanceTrackingState.instanceAffixes.end() &&
			detail::AreInstanceAffixSlotsEqual(previousSlots, currentSlotsIt->second);
		if (!baseStillValid || revalidatedInstanceKey != a_expectedInstanceKey || !slotsStillMatch ||
			!_configLoaded || _loot.stripTrackedSuffixSlots) {
			const auto refundBefore = ownedAfter;
			player->AddObjectToContainer(orb, nullptr, static_cast<std::int32_t>(orbCost), nullptr);
			const auto refundAfter = std::max(0, player->GetItemCount(orb));
			const bool refundConfirmed =
				refundAfter == ownedBefore &&
				detail::DidRestoreExactInventoryCount(
					static_cast<std::uint32_t>(refundBefore),
					static_cast<std::uint32_t>(refundAfter),
					orbCost);
			restoreRollStateAfterEngineFailure("stale-state rollback");
			result.message = refundConfirmed
				? "Affix expansion failed: selected base or affix state changed; inventory was restored."
				: "Affix expansion failed: selected base or affix state changed; check inventory.";
			SKSE::log::error(
				"CalamityAffixes: affix expansion aborted after exact charge because state changed "
				"(expectedInstance={:016X}, resolvedInstance={:016X}, slotsStillMatch={}, refundAfter={}, refundConfirmed={}).",
				a_expectedInstanceKey,
				revalidatedInstanceKey,
				slotsStillMatch,
				refundAfter,
				refundConfirmed);
			return result;
		}

		_instanceTrackingState.instanceAffixes[instanceKey] = newSlots;
		MarkLootEvaluatedInstance(instanceKey);
		const auto newAffixToken = _affixRuntimeState.affixes[newAffixIndex].token;
		EnsureInstanceRuntimeState(instanceKey, newAffixToken);
		EnsureMultiAffixDisplayName(revalidatedEntry, revalidatedXList, newSlots);
		RebuildActiveCounts();

		std::string itemName = ResolveInventoryDisplayName(revalidatedEntry, revalidatedXList);
		if (itemName.empty()) {
			itemName = "Selected base";
		}
		std::string affixName = _affixRuntimeState.affixes[newAffixIndex].displayNameEn;
		if (affixName.empty()) {
			affixName = _affixRuntimeState.affixes[newAffixIndex].displayName.empty() ?
				_affixRuntimeState.affixes[newAffixIndex].id :
				_affixRuntimeState.affixes[newAffixIndex].displayName;
		}

		result.success = true;
		result.message = "Affix slots expanded: " + itemName + " [Added: " + affixName + "] (" +
			std::to_string(expansionPolicy->targetRegularAffixCount) + "/" +
			std::to_string(kMaxRegularAffixesPerItem) + ", Orbs: " + std::to_string(ownedAfter) + ")";
		return result;
	}
}
