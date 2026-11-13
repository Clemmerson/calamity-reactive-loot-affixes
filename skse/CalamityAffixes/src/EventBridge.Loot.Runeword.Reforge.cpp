#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/LootRollSelection.h"
#include "CalamityAffixes/PluginEditorIds.h"
#include "EventBridge.Loot.Runeword.Detail.h"

#include <algorithm>
#include <unordered_set>

namespace CalamityAffixes
{
	using namespace RunewordDetail;

	EventBridge::OperationResult EventBridge::CraftSelectedAffixes(
		AffixCraftAction a_action, std::uint64_t a_expectedInstanceKey, std::uint64_t a_selectedToken)
	{
		return CraftSelectedAffixes(a_action, a_expectedInstanceKey, a_selectedToken, {}, 0u);
	}

	EventBridge::OperationResult EventBridge::CraftSelectedAffixes(
		AffixCraftAction a_action,
		std::uint64_t a_expectedInstanceKey,
		std::uint64_t a_selectedToken,
		std::string_view a_expectedAffixSignature,
		std::uint32_t a_expectedRegularAffixCount)
	{
		const std::scoped_lock lock(_stateMutex);
		auto fail = [](std::string a_message) { return OperationResult{ false, std::move(a_message) }; };
		if (!_configLoaded || !_runtimeSettings.enabled.load(std::memory_order_relaxed) || a_expectedInstanceKey == 0u ||
			_runewordState.transmuteInProgress || _runewordState.affixExpansionInProgress) {
			return fail("Crafting unavailable: select a base and wait for other actions to finish.");
		}
		// Reuse the common item-mutation guard so engine inventory callbacks cannot
		// interleave expansion, transmutation, reset, or another currency action.
		struct InFlight {
			bool& flag;
			explicit InFlight(bool& a_flag) : flag(a_flag) { flag = true; }
			~InFlight() { flag = false; }
		} inFlight(_runewordState.affixExpansionInProgress);
		SanitizeRunewordState();

		std::uint64_t instanceKey = 0u;
		RE::InventoryEntryData* entry = nullptr;
		RE::ExtraDataList* xList = nullptr;
		std::string failure;
		if (!ResolveSelectedRunewordBaseInstance(instanceKey, entry, xList, &failure, true) ||
			instanceKey != a_expectedInstanceKey) {
			return fail("Crafting failed: selected base changed; select it again.");
		}
		InstanceAffixSlots previous{};
		if (const auto it = _instanceTrackingState.instanceAffixes.find(instanceKey);
			it != _instanceTrackingState.instanceAffixes.end()) previous = it->second;
		if (previous.count > kMaxAffixesPerItem || !detail::HasUniqueAffixTokens(previous)) {
			return fail("Crafting failed: invalid affix layout.");
		}
		const auto* recipe = ResolveCompletedRunewordRecipe(instanceKey);
		const auto runewordToken = recipe ? recipe->resultAffixToken : 0u;
		InstanceAffixSlots regular{};
		std::vector<std::size_t> excludedIndices;
		std::vector<std::string> keptFamilies;
		std::uint8_t prefixCount = 0u;
		std::uint8_t suffixCount = 0u;
		bool hasLegacyAffix = false;
		std::unordered_set<std::string_view> families;
		std::optional<AffixSlot> selectedSlot;
		for (const auto token : previous) {
			if (_runewordState.recipeIndexByResultAffixToken.contains(token)) {
				if (token != runewordToken) return fail("Crafting failed: conflicting runeword state.");
				continue;
			}
			(void)regular.AddToken(token);
			// Unknown or unslotted tokens still occupy a regular slot: scour can
			// reroll them, identify and selected reforge refuse the base.
			const auto it = _affixRuntimeState.affixRegistry.affixIndexByToken.find(token);
			if (it == _affixRuntimeState.affixRegistry.affixIndexByToken.end() ||
				it->second >= _affixRuntimeState.affixes.size()) {
				hasLegacyAffix = true;
				continue;
			}
			const auto& affix = _affixRuntimeState.affixes[it->second];
			if (affix.slot == AffixSlot::kPrefix) ++prefixCount;
			else if (affix.slot == AffixSlot::kSuffix) {
				++suffixCount;
				if (affix.family.empty() || !families.insert(affix.family).second) hasLegacyAffix = true;
				if (token != a_selectedToken) keptFamilies.push_back(affix.family);
			} else {
				hasLegacyAffix = true;
				continue;
			}
			excludedIndices.push_back(it->second);
			if (token == a_selectedToken) selectedSlot = affix.slot;
		}
		if ((runewordToken != 0u && previous.GetPrimary() != runewordToken) ||
			previous.count != regular.count + (runewordToken != 0u ? 1u : 0u)) {
			return fail("Crafting failed: conflicting runeword state.");
		}
		if (!a_expectedAffixSignature.empty()) {
			std::string currentSignature;
			for (const auto token : regular) {
				if (!currentSignature.empty()) currentSignature.push_back(',');
				currentSignature += std::to_string(token);
			}
			if (currentSignature.empty()) currentSignature = "-";
			if (regular.count != a_expectedRegularAffixCount || currentSignature != a_expectedAffixSignature) {
				return fail("Affixes changed; review the current state and try again.");
			}
		}
		if (!detail::CanCraftAffixLayout(a_action, regular.count, prefixCount, suffixCount, hasLegacyAffix)) {
			return fail("Crafting failed: this base has a legacy affix layout. Use a Scouring Orb to reroll it.");
		}
		if (!detail::CanCraftAffixes(a_action, regular, a_selectedToken)) {
			return fail(a_action == AffixCraftAction::kIdentify ?
				"Identify requires a base without regular affixes. Use a Scouring Orb to reroll existing ones." :
				"Select an existing regular affix to reforge, or a base with regular affixes to scour.");
		}
		if (_loot.stripTrackedSuffixSlots) {
			return fail("Crafting unavailable: suffix slots are disabled by runtime policy.");
		}
		std::uint8_t reforgesDone = 0u;
		if (const auto it = _instanceTrackingState.selectedReforgeCounts.find(instanceKey);
			it != _instanceTrackingState.selectedReforgeCounts.end()) reforgesDone = it->second;
		if (a_action == AffixCraftAction::kReforge && detail::RemainingSelectedReforges(reforgesDone) == 0u) {
			return fail("This item has used all 6 selected reforges. Scour it to reroll every affix and get them back.");
		}
		const auto lootType = ResolveInstanceLootType(instanceKey);
		if (!lootType) return fail("Crafting failed: unsupported item type.");
		const auto weaponSubtype = detail::ResolveWeaponSubtype(entry->object->As<RE::TESObjectWEAP>());
		auto* player = RE::PlayerCharacter::GetSingleton();
		const auto currencyId = detail::CraftCurrency(a_action);
		auto* currency = PluginEditorIds::Lookup<RE::TESObjectMISC>(currencyId.data());
		if (!player || !currency) return fail("Crafting currency unavailable. Install the matching ESP and DLL.");
		const auto cost = a_action == AffixCraftAction::kReforge ? detail::kSelectedReforgeCost :
			(a_action == AffixCraftAction::kIdentify ? detail::kIdentifyScrollCost : detail::kScouringOrbCost);
		const auto ownedBefore = std::max(0, player->GetItemCount(currency));
		if (static_cast<std::uint32_t>(ownedBefore) < cost) return fail("Not enough crafting currency.");

		const auto prefixShared = _lootState.prefixSharedBag;
		const auto prefixWeapon = _lootState.prefixWeaponBag;
		const auto prefixArmor = _lootState.prefixArmorBag;
		const auto suffixShared = _lootState.suffixSharedBag;
		const auto suffixWeapon = _lootState.suffixWeaponBag;
		const auto suffixArmor = _lootState.suffixArmorBag;
		std::mt19937 rngBefore{};
		std::mt19937 rngAfter{};
		InstanceAffixSlots next = previous;
		bool accepted = false;
		// Rolls a fresh canonical layout (Prefix, then distinct-family Suffixes).
		// Identify and scour never exclude the previous tokens: a full reroll may
		// land on the same effect again, like any other identify.
		auto rollRegularLayout = [&](std::uint8_t a_count, InstanceAffixSlots& a_out) {
			std::vector<std::size_t> chosen;
			std::vector<std::string> chosenFamilies;
			a_out.Clear();
			for (std::uint8_t slot = 0u; slot < a_count; ++slot) {
				const auto idx = slot == 0u ? RollLootAffixIndex(*lootType, &chosen, true) :
					RollSuffixIndex(*lootType, weaponSubtype, &chosenFamilies, &chosen);
				if (!idx || *idx >= _affixRuntimeState.affixes.size()) return false;
				const auto& affix = _affixRuntimeState.affixes[*idx];
				if (affix.slot != (slot == 0u ? AffixSlot::kPrefix : AffixSlot::kSuffix) ||
					_runewordState.recipeIndexByResultAffixToken.contains(affix.token) ||
					(slot != 0u && (affix.family.empty() ||
						std::find(chosenFamilies.begin(), chosenFamilies.end(), affix.family) != chosenFamilies.end())) ||
					!a_out.AddToken(affix.token)) return false;
				chosen.push_back(*idx);
				if (slot != 0u) chosenFamilies.push_back(affix.family);
			}
			return a_out.count == a_count;
		};
		{
			std::lock_guard<std::mutex> rngLock(_rngMutex);
			rngBefore = _rng;
			if (a_action == AffixCraftAction::kReforge) {
				const auto idx = selectedSlot == AffixSlot::kPrefix ?
					RollLootAffixIndex(*lootType, &excludedIndices, true) :
					RollSuffixIndex(*lootType, weaponSubtype, &keptFamilies, &excludedIndices);
				if (idx && *idx < _affixRuntimeState.affixes.size()) {
					const auto& affix = _affixRuntimeState.affixes[*idx];
					accepted = affix.slot == selectedSlot &&
						!_runewordState.recipeIndexByResultAffixToken.contains(affix.token) &&
						(affix.slot != AffixSlot::kSuffix || (!affix.family.empty() &&
							std::find(keptFamilies.begin(), keptFamilies.end(), affix.family) == keptFamilies.end())) &&
						detail::ReplaceSelectedAffix(next, a_selectedToken, affix.token);
				}
			} else {
				// Identify draws its count once: candidate retries must never bias
				// the 60/30/10 distribution. Scour keeps the current slot count.
				const auto count = a_action == AffixCraftAction::kIdentify ?
					detail::IdentifyAffixCount(std::uniform_int_distribution<std::uint32_t>(0u, 99u)(_rng)) :
					detail::ScourAffixCount(regular.count);
				for (std::uint8_t attempt = 0u; attempt < detail::kCraftRollMaxAttempts && !accepted; ++attempt) {
					InstanceAffixSlots rolled{};
					if (!rollRegularLayout(count, rolled) ||
						(runewordToken != 0u && !rolled.PromoteTokenToPrimary(runewordToken))) continue;
					accepted = detail::IsValidCraftResult(a_action, previous, rolled, runewordToken, a_selectedToken);
					if (accepted) next = rolled;
				}
			}
			rngAfter = _rng;
		}
		auto restoreRoll = [&]() {
			_lootState.prefixSharedBag = prefixShared;
			_lootState.prefixWeaponBag = prefixWeapon;
			_lootState.prefixArmorBag = prefixArmor;
			_lootState.suffixSharedBag = suffixShared;
			_lootState.suffixWeaponBag = suffixWeapon;
			_lootState.suffixArmorBag = suffixArmor;
			std::lock_guard<std::mutex> rngLock(_rngMutex);
			if (_rng == rngAfter) _rng = rngBefore;
		};
		if (!accepted || !detail::IsValidCraftResult(a_action, previous, next, runewordToken, a_selectedToken)) {
			restoreRoll();
			return fail("No eligible affix result is available. No currency consumed.");
		}

		auto refund = [&](std::uint32_t amount) {
			if (amount > 0u) player->AddObjectToContainer(currency, nullptr, static_cast<std::int32_t>(amount), nullptr);
			restoreRoll();
			return std::max(0, player->GetItemCount(currency)) == ownedBefore;
		};
		player->RemoveItem(currency, static_cast<std::int32_t>(cost), RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
		const auto ownedAfter = std::max(0, player->GetItemCount(currency));
		if (!detail::DidConsumeExactInventoryCount(ownedBefore, ownedAfter, cost)) {
			const bool restored = refund(detail::ResolveObservedInventoryConsumption(ownedBefore, ownedAfter));
			return fail(restored ? "Currency transaction failed; inventory restored." : "Currency transaction failed; check inventory.");
		}
		// RemoveItem can re-enter engine code: never retain the old entry/xList across it.
		std::uint64_t currentKey = 0u;
		RE::InventoryEntryData* currentEntry = nullptr;
		RE::ExtraDataList* currentXList = nullptr;
		InstanceAffixSlots currentSlots{};
		const bool baseValid = ResolveSelectedRunewordBaseInstance(currentKey, currentEntry, currentXList, &failure, true);
		if (const auto it = _instanceTrackingState.instanceAffixes.find(instanceKey);
			it != _instanceTrackingState.instanceAffixes.end()) currentSlots = it->second;
		const auto* currentRecipe = ResolveCompletedRunewordRecipe(instanceKey);
		const auto currentRuneword = currentRecipe ? currentRecipe->resultAffixToken : 0u;
		if (!baseValid || currentKey != instanceKey || !_configLoaded ||
			_loot.stripTrackedSuffixSlots ||
			currentRuneword != runewordToken || !detail::AreInstanceAffixSlotsEqual(previous, currentSlots)) {
			const bool restored = refund(cost);
			return fail(restored ? "Base changed; currency restored." : "Base changed; check inventory.");
		}
		// Selected reforge deletes only the replaced effect, so unselected effects
		// keep their exact runtime state (evolution/charge progress). Identify and
		// scour start every regular effect fresh, even one rolled again. The
		// runeword always keeps its state.
		const bool freshRegular = a_action != AffixCraftAction::kReforge;
		std::erase_if(_instanceTrackingState.instanceStates, [&](const auto& pair) {
			return pair.first.instanceKey == instanceKey && (!next.HasToken(pair.first.affixToken) ||
				(freshRegular && pair.first.affixToken != runewordToken));
		});
		_instanceTrackingState.instanceAffixes[instanceKey] = next;
		// Reforge spends one of the item's six; identify and scour give them back.
		if (a_action == AffixCraftAction::kReforge) {
			_instanceTrackingState.selectedReforgeCounts[instanceKey] = detail::NextSelectedReforgeCount(reforgesDone);
		} else {
			_instanceTrackingState.selectedReforgeCounts.erase(instanceKey);
		}
		MarkLootEvaluatedInstance(instanceKey);
		ForgetLootPreviewSlots(instanceKey);
		for (const auto token : next) EnsureInstanceRuntimeState(instanceKey, token);
		EnsureMultiAffixDisplayName(currentEntry, currentXList, next);
		RebuildActiveCounts();
		const auto name = ResolveInventoryDisplayName(currentEntry, currentXList);
		const std::string operation = a_action == AffixCraftAction::kIdentify ? "Identified: " :
			(a_action == AffixCraftAction::kReforge ? "Selected affix reforged: " : "Regular affixes rerolled: ");
		const std::string runewordNote = runewordToken != 0u ? "Runeword preserved; " : "";
		const std::string reforgeNote = a_action == AffixCraftAction::kReforge ?
			"reforges left: " +
				std::to_string(detail::RemainingSelectedReforges(detail::NextSelectedReforgeCount(reforgesDone))) + "/" +
				std::to_string(detail::kSelectedReforgesPerItem) + "; " :
			"";
		return OperationResult{ true, operation + name + " (" + runewordNote + reforgeNote + "currency remaining: " + std::to_string(ownedAfter) + ")" };
	}

	EventBridge::OperationResult EventBridge::ExchangeCraftingCurrency(CurrencyExchange a_exchange)
	{
		const std::scoped_lock lock(_stateMutex);
		auto fail = [](std::string a_message) { return OperationResult{ false, std::move(a_message) }; };
		if (!_configLoaded || !_runtimeSettings.enabled.load(std::memory_order_relaxed) ||
			_runewordState.transmuteInProgress || _runewordState.affixExpansionInProgress) {
			return fail("Exchange unavailable: wait for other actions to finish.");
		}
		// Same item-mutation guard as crafting, so a trade cannot interleave with a
		// reforge or transmute that is spending the same currency.
		struct InFlight {
			bool& flag;
			explicit InFlight(bool& a_flag) : flag(a_flag) { flag = true; }
			~InFlight() { flag = false; }
		} inFlight(_runewordState.affixExpansionInProgress);

		const auto recipe = detail::ResolveCurrencyExchange(a_exchange);
		const std::string targetName = a_exchange == CurrencyExchange::kScouringOrb ? "Scouring Orb" : "Reforge Orb";
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* source = PluginEditorIds::Lookup<RE::TESObjectMISC>(std::string(recipe.sourceEditorId));
		auto* target = PluginEditorIds::Lookup<RE::TESObjectMISC>(std::string(recipe.targetEditorId));
		if (!player || !source || !target || recipe.sourceCost == 0u) {
			return fail("Exchange currency unavailable. Install the matching ESP and DLL.");
		}
		const auto sourceBefore = static_cast<std::uint32_t>(std::max(0, player->GetItemCount(source)));
		const auto targetBefore = static_cast<std::uint32_t>(std::max(0, player->GetItemCount(target)));
		if (sourceBefore < recipe.sourceCost) {
			return fail("Not enough Identify Scrolls: " + std::to_string(recipe.sourceCost) + " needed.");
		}

		player->RemoveItem(source, static_cast<std::int32_t>(recipe.sourceCost), RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
		const auto sourceAfter = static_cast<std::uint32_t>(std::max(0, player->GetItemCount(source)));
		if (!detail::DidConsumeExactInventoryCount(sourceBefore, sourceAfter, recipe.sourceCost)) {
			const auto observed = detail::ResolveObservedInventoryConsumption(sourceBefore, sourceAfter);
			if (observed > 0u) player->AddObjectToContainer(source, nullptr, static_cast<std::int32_t>(observed), nullptr);
			const bool restored = std::max(0, player->GetItemCount(source)) == static_cast<std::int32_t>(sourceBefore);
			return fail(restored ? "Exchange failed; inventory restored." : "Exchange failed; check inventory.");
		}

		player->AddObjectToContainer(target, nullptr, 1, nullptr);
		const auto targetAfter = static_cast<std::uint32_t>(std::max(0, player->GetItemCount(target)));
		if (!detail::DidRestoreExactInventoryCount(targetBefore, targetAfter, 1u)) {
			// The orb did not arrive exactly once: undo any partial grant and return
			// the scrolls, so a failed trade never costs currency.
			if (targetAfter > targetBefore) {
				player->RemoveItem(target, static_cast<std::int32_t>(targetAfter - targetBefore), RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
			}
			player->AddObjectToContainer(source, nullptr, static_cast<std::int32_t>(recipe.sourceCost), nullptr);
			const bool restored = std::max(0, player->GetItemCount(source)) == static_cast<std::int32_t>(sourceBefore) &&
				std::max(0, player->GetItemCount(target)) == static_cast<std::int32_t>(targetBefore);
			return fail(restored ? "Exchange failed; inventory restored." : "Exchange failed; check inventory.");
		}

		return OperationResult{ true, "Exchanged " + std::to_string(recipe.sourceCost) + " Identify Scrolls for 1 " +
			targetName + " (" + targetName + "s: " + std::to_string(targetAfter) + ", Identify Scrolls: " +
			std::to_string(sourceAfter) + ")" };
	}

	EventBridge::OperationResult EventBridge::ResetSelectedRunewordBaseCalamityState()
	{
		const std::scoped_lock lock(_stateMutex);
		OperationResult result{};
		if (!_configLoaded || !(_loot.debugHudNotifications || _loot.debugLog)) {
			result.message = "Reset unavailable: enable debug tools first.";
			return result;
		}
		if (_runewordState.transmuteInProgress) {
			result.message = "Reset unavailable while runeword transmutation is in progress.";
			return result;
		}
		if (_runewordState.affixExpansionInProgress) {
			result.message = "Reset unavailable while affix expansion is in progress.";
			return result;
		}

		SanitizeRunewordState();
		std::uint64_t instanceKey = 0u;
		RE::InventoryEntryData* entry = nullptr;
		RE::ExtraDataList* xList = nullptr;
		std::string baseResolveFailure;
		if (!ResolveSelectedRunewordBaseInstance(instanceKey, entry, xList, &baseResolveFailure, true)) {
			result.message = "Reset failed: " + baseResolveFailure;
			return result;
		}

		std::string itemName = ResolveStoredLootDisplayBaseName(entry, xList);
		if (itemName.empty()) {
			itemName = ResolveInventoryDisplayName(entry, xList);
		}
		if (itemName.empty()) {
			itemName = "Selected base";
		}

		const bool hadAffixes = _instanceTrackingState.instanceAffixes.erase(instanceKey) > 0u;
		_instanceTrackingState.instanceBaseObjects.erase(instanceKey);
		_instanceTrackingState.selectedReforgeCounts.erase(instanceKey);
		const bool hadRuntimeState = std::ranges::any_of(
			_instanceTrackingState.instanceStates,
			[instanceKey](const auto& stateEntry) {
				return stateEntry.first.instanceKey == instanceKey;
			});
		const bool hadRunewordProgress = _runewordState.instanceStates.erase(instanceKey) > 0u;
		const bool hadPreview = FindLootPreviewSlots(instanceKey) != nullptr;

		EraseInstanceRuntimeStates(instanceKey);
		ForgetLootPreviewSlots(instanceKey);
		// Keep the evaluated marker so reset cannot become a free-reroll path if
		// automatic assignment is enabled again in a future version.
		if (!IsLootEvaluatedInstance(instanceKey)) {
			MarkLootEvaluatedInstance(instanceKey);
		}

		bool renamed = false;
		if (auto* text = xList->GetExtraTextDisplayData(); text && !itemName.empty()) {
			const char* currentRaw = xList->GetDisplayName(entry->object);
			const std::string_view currentName = currentRaw ? std::string_view(currentRaw) : std::string_view{};
			if (currentName != itemName) {
				text->SetName(itemName.c_str());
				renamed = true;
			}
		}

		if (_runewordState.selectedBaseKey && *_runewordState.selectedBaseKey == instanceKey) {
			_runewordState.selectedBaseKey.reset();
		}
		RebuildActiveCounts();

		result.success = true;
		const bool changed = hadAffixes || hadRuntimeState || hadRunewordProgress || hadPreview || renamed;
		result.message = changed ?
			"Reset complete: " + itemName + " (Calamity state removed; no material refund)." :
			"Already clear: " + itemName + ".";
		return result;
	}

}
