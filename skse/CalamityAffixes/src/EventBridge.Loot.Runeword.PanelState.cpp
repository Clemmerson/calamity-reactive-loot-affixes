#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/EquippedBuildSummaryPolicy.h"
#include "CalamityAffixes/LootRollSelection.h"
#include "CalamityAffixes/PluginEditorIds.h"
#include "CalamityAffixes/ProcChancePolicy.h"
#include "CalamityAffixes/RuneforgePresentation.h"
#include "CalamityAffixes/RunewordUiPolicy.h"
#include "CalamityAffixes/RunewordUtil.h"
#include "CalamityAffixes/SpecialActionSafetyPolicy.h"
#include "CalamityAffixes/SuffixFamilySelection.h"
#include "CalamityAffixes/TriggerGuards.h"
#include "EventBridge.Loot.Runeword.Detail.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace CalamityAffixes
{
	using namespace RunewordDetail;

	namespace
	{
		[[nodiscard]] detail::RunewordBaseKind ResolveRunewordBaseKind(RE::TESBoundObject* a_object) noexcept
		{
			if (!a_object) {
				return detail::RunewordBaseKind::kUnknown;
			}
			if (const auto* weapon = a_object->As<RE::TESObjectWEAP>()) {
				return detail::ResolveRunewordWeaponBaseKind(
					static_cast<std::uint8_t>(weapon->GetWeaponType()));
			}
			if (const auto* armor = a_object->As<RE::TESObjectARMO>()) {
				return detail::ResolveRunewordArmorBaseKind(
					armor->IsShield(),
					armor->IsHeavyArmor(),
					armor->IsLightArmor(),
					armor->IsClothing());
			}
			return detail::RunewordBaseKind::kUnknown;
		}

		void PopulateSpecializedBaseWarning(
			RunewordPanelState& a_panelState,
			std::string_view a_recipeId,
			RE::TESBoundObject* a_object)
		{
			const auto requirement = detail::ResolveSpecializedRunewordBase(a_recipeId);
			if (requirement == detail::SpecializedRunewordBase::kNone ||
				detail::IsSpecializedRunewordBaseCompatible(
					requirement,
					ResolveRunewordBaseKind(a_object))) {
				return;
			}

			a_panelState.baseCompatibilityWarning = true;
			a_panelState.baseCompatibilityMessageEn =
				"Base mismatch: recommended base is ";
			a_panelState.baseCompatibilityMessageEn.append(
				detail::DescribeSpecializedRunewordBaseEn(requirement));
			a_panelState.baseCompatibilityMessageEn.append(
				". The selected base is incompatible; engraving is blocked.");

			a_panelState.baseCompatibilityMessageKo =
				"베이스 불일치: 이 룬워드는 ";
			a_panelState.baseCompatibilityMessageKo.append(
				detail::DescribeSpecializedRunewordBaseKo(requirement));
			a_panelState.baseCompatibilityMessageKo.append(
				"에 맞춰져 있습니다. 현재 베이스와 호환되지 않아 각인할 수 없습니다.");
		}
	}

	void EventBridge::PopulateEquippedBuildSummary(RunewordPanelState& a_panelState) const
	{
		auto& summary = a_panelState.equippedBuild;
		summary.runtimeEnabled = _runtimeSettings.enabled.load(std::memory_order_relaxed);
		summary.ready = detail::ResolveEquippedBuildSummaryReady(
			_configLoaded,
			summary.runtimeEnabled,
			_instanceTrackingState.equippedTokenCacheReady,
			_affixRuntimeState.activeCounts.size() == _affixRuntimeState.affixes.size());
		if (!summary.ready || !summary.runtimeEnabled) {
			return;
		}

		std::unordered_map<std::string_view, detail::SuffixFamilyRankSelection> suffixFamilySelections;
		for (std::size_t i = 0; i < _affixRuntimeState.affixes.size(); ++i) {
			const auto& affix = _affixRuntimeState.affixes[i];
			if (affix.slot == AffixSlot::kSuffix && !affix.family.empty()) {
				auto& selection = suffixFamilySelections[affix.family];
				selection.ConsiderDefinition(affix.id);
				selection.ConsiderEquipped(affix.id, i, _affixRuntimeState.activeCounts[i]);
			}
		}
		for (std::size_t i = 0; i < _affixRuntimeState.affixes.size(); ++i) {
			const auto& affix = _affixRuntimeState.affixes[i];
			if (affix.slot == AffixSlot::kSuffix && !affix.family.empty()) {
				suffixFamilySelections[affix.family].ConsiderEffectiveDefinition(affix.id, i);
			}
		}

		const auto resolveTrigger = [](Trigger a_trigger) noexcept {
			switch (a_trigger) {
			case Trigger::kIncomingHit:
				return detail::EquippedBuildTriggerKind::kIncomingHit;
			case Trigger::kDotApply:
				return detail::EquippedBuildTriggerKind::kDotApply;
			case Trigger::kKill:
				return detail::EquippedBuildTriggerKind::kKill;
			case Trigger::kLowHealth:
				return detail::EquippedBuildTriggerKind::kLowHealth;
			case Trigger::kHit:
			default:
				return detail::EquippedBuildTriggerKind::kHit;
			}
		};

		const auto resolveProcLane = [](ActionType a_actionType) noexcept {
			switch (a_actionType) {
			case ActionType::kCastSpell:
			case ActionType::kCastSpellAdaptiveElement:
			case ActionType::kSpawnTrap:
				return detail::EquippedBuildProcLane::kStandard;
			case ActionType::kCastOnCrit:
			case ActionType::kConvertDamage:
			case ActionType::kMindOverMatter:
			case ActionType::kArchmage:
			case ActionType::kCorpseExplosion:
			case ActionType::kSummonCorpseExplosion:
				return detail::EquippedBuildProcLane::kSpecial;
			case ActionType::kDebugNotify:
			default:
				return detail::EquippedBuildProcLane::kNone;
			}
		};

		summary.entries.reserve(_affixRuntimeState.affixes.size());
		std::uint64_t visibleSlotCount = 0u;
		for (std::size_t i = 0; i < _affixRuntimeState.affixes.size(); ++i) {
			const auto equippedCount = _affixRuntimeState.activeCounts[i];
			if (equippedCount == 0u) {
				continue;
			}

			const auto& affix = _affixRuntimeState.affixes[i];
			const auto slot = affix.slot == AffixSlot::kSuffix ?
				detail::EquippedBuildSlotKind::kSuffix :
				(_runewordState.recipeIndexByResultAffixToken.contains(affix.token) ?
					detail::EquippedBuildSlotKind::kRuneword :
					detail::EquippedBuildSlotKind::kPrefix);
			const auto procLane = resolveProcLane(affix.action.type);

			const detail::SuffixFamilyRankSelection* suffixFamilySelection = nullptr;
			detail::SuffixFamilyBestCandidate effectiveSuffixCandidate{};
			const AffixRuntime* effectiveSuffixAffix = nullptr;
			bool isSuffixFamilyWinner = false;
			if (affix.slot == AffixSlot::kSuffix && !affix.family.empty()) {
				const auto selectionIt = suffixFamilySelections.find(affix.family);
				if (selectionIt != suffixFamilySelections.end()) {
					suffixFamilySelection = &selectionIt->second;
					isSuffixFamilyWinner = suffixFamilySelection->bestEquipped.selected &&
						suffixFamilySelection->bestEquipped.index == i;
					effectiveSuffixCandidate = suffixFamilySelection->ResolveEffectiveCandidate();
					if (effectiveSuffixCandidate.selected &&
						effectiveSuffixCandidate.index < _affixRuntimeState.affixes.size()) {
						effectiveSuffixAffix = &_affixRuntimeState.affixes[effectiveSuffixCandidate.index];
					}
				}
			}
			const bool suffixFamilySuppressed =
				affix.slot == AffixSlot::kSuffix &&
				!affix.family.empty() &&
				!isSuffixFamilyWinner;
			// The family representative describes the contribution that runtime
			// actually selected, which may be an otherwise unequipped promoted tier.
			// Scroll preservation remains copy-additive and therefore stays sourced
			// from the equipped affix rather than the family-selected tier.
			const auto passiveContributionAffixIndex = detail::ResolveSuffixFamilyContributionSourceIndex(
				isSuffixFamilyWinner,
				effectiveSuffixCandidate,
				i);
			const auto& passiveContributionAffix = passiveContributionAffixIndex < _affixRuntimeState.affixes.size() ?
				_affixRuntimeState.affixes[passiveContributionAffixIndex] :
				affix;
			const auto passiveContribution = detail::ResolveEquippedBuildPassiveContributionState({
				.hasPassiveSpell = passiveContributionAffix.passiveSpell != nullptr,
				.passiveSpellsDisabled = _runtimeSettings.disablePassiveSuffixSpells,
				.hasCritContribution = passiveContributionAffix.critDamageBonusPct != 0.0f,
				.hasScrollContribution = affix.scrollNoConsumeChancePct != 0.0f,
				.suffixFamilySuppressed = suffixFamilySuppressed,
			});
			const detail::EquippedBuildPolicyInput policyInput{
				.trigger = resolveTrigger(affix.trigger),
				.slot = slot,
				.procLane = procLane,
				.hasPassiveContribution = passiveContribution.hasPassiveContribution,
				.isDebugNotify = affix.action.type == ActionType::kDebugNotify,
				.configuredProcChancePct = affix.procChancePct,
				.luckyHitChancePct = affix.luckyHitChancePct,
			};
			if (!detail::ShouldShowEquippedBuildEntry(policyInput)) {
				continue;
			}

			EquippedBuildEntry entry{};
			entry.token = affix.token;
			entry.displayNameEn = !affix.displayNameEn.empty() ? ResolveAffixDisplayNameEn(affix) :
				(!affix.displayName.empty() ? affix.displayName : affix.id);
			entry.displayNameKo = !affix.displayNameKo.empty() ? affix.displayNameKo :
				(!affix.displayName.empty() ? affix.displayName : affix.id);
			if (const auto* recipe = FindRunewordRecipeByResultAffixToken(affix.token)) {
				entry.effectSummaryEn = std::string(RuneforgePresentation::EffectSummary(recipe->id));
			}
			entry.cooldownSeconds = static_cast<float>(affix.icd.count()) / 1000.0f;
			entry.perTargetCooldownSeconds = static_cast<float>(affix.perTargetIcd.count()) / 1000.0f;
			if (affix.trigger == Trigger::kLowHealth) {
				entry.lowHealthThresholdPct = affix.lowHealthThresholdPct;
				entry.lowHealthRearmPct = affix.lowHealthRearmPct;
			}
			entry.group = detail::DescribeEquippedBuildGroup(
				detail::ResolveEquippedBuildGroup(policyInput));
			entry.triggerKey = detail::DescribeEquippedBuildTriggerKey(
				detail::ResolveEquippedBuildTriggerKey(policyInput));
			entry.slotKind = detail::DescribeEquippedBuildSlotKind(slot);
			entry.suffixState = detail::DescribeEquippedBuildSuffixState(
				detail::ResolveEquippedBuildSuffixState(
					affix.slot == AffixSlot::kSuffix,
					!affix.family.empty(),
					isSuffixFamilyWinner));
			entry.equippedCount = equippedCount;
			if (suffixFamilySelection) {
				entry.suffixTierRank = detail::ParseSuffixTierRank(affix.id);
				entry.suffixFamilyRankPoints = suffixFamilySelection->rankPoints;
				entry.effectiveSuffixTierRank = suffixFamilySelection->ResolvedEffectiveRank();
				if (effectiveSuffixAffix) {
					entry.effectiveSuffixDisplayNameEn = !effectiveSuffixAffix->displayNameEn.empty() ?
						effectiveSuffixAffix->displayNameEn :
						(!effectiveSuffixAffix->displayName.empty() ? effectiveSuffixAffix->displayName : effectiveSuffixAffix->id);
					entry.effectiveSuffixDisplayNameKo = !effectiveSuffixAffix->displayNameKo.empty() ?
						effectiveSuffixAffix->displayNameKo :
						(!effectiveSuffixAffix->displayName.empty() ? effectiveSuffixAffix->displayName : effectiveSuffixAffix->id);
				}
			}
			entry.hasPassiveContribution = passiveContribution.hasPassiveContribution;
			entry.passiveContributionActive = passiveContribution.passiveContributionActive;
			entry.passiveSpellDisabled = passiveContribution.passiveSpellDisabled;
			entry.castOnCritSelectionLimited = affix.action.type == ActionType::kCastOnCrit;
			entry.hasProcRoll = detail::HasEquippedBuildProcRoll(policyInput);
			if (entry.hasProcRoll) {
				entry.procRollChancePct = procLane == detail::EquippedBuildProcLane::kStandard ?
					ResolveTriggerProcChancePct(affix, i) :
					detail::ResolveSpecialActionProcChancePct(
						affix.procChancePct * _runtimeSettings.procChanceMult);
				entry.procRollStackCount = 1u;
				if (procLane == detail::EquippedBuildProcLane::kStandard &&
					i < _lootState.activeProcSlotPenalties.size()) {
					entry.procRollStackCount = static_cast<std::uint32_t>(std::max<std::size_t>(
						1u,
						std::min(_lootState.activeProcSlotPenalties[i].size(), kMaxEquippedDuplicateProcCopies)));
				}
			}
			entry.hasNormalWeaponHitProcRoll = affix.normalWeaponHitProcChancePct > 0.0f;
			if (entry.hasNormalWeaponHitProcRoll) {
				entry.normalWeaponHitProcChancePct = procLane == detail::EquippedBuildProcLane::kStandard ?
					ResolveTriggerProcChancePctFromBase(affix, i, affix.normalWeaponHitProcChancePct) :
					detail::ResolveSpecialActionProcChancePct(
						affix.normalWeaponHitProcChancePct * _runtimeSettings.procChanceMult);
				entry.normalWeaponHitProcStackCount = 1u;
				if (procLane == detail::EquippedBuildProcLane::kStandard &&
					i < _lootState.activeProcSlotPenalties.size()) {
					entry.normalWeaponHitProcStackCount = static_cast<std::uint32_t>(std::max<std::size_t>(
						1u,
						std::min(_lootState.activeProcSlotPenalties[i].size(), kMaxEquippedDuplicateProcCopies)));
				}
			}
			entry.hasLuckyHitGate = detail::HasEquippedBuildLuckyHitGate(policyInput);
			if (entry.hasLuckyHitGate) {
				entry.luckyHitGateChancePct = ResolveLuckyHitEffectiveChancePct(
					affix.luckyHitChancePct,
					affix.luckyHitProcCoefficient);
			}

			visibleSlotCount += equippedCount;
			summary.entries.push_back(std::move(entry));
		}

		summary.equippedAffixSlots = static_cast<std::uint32_t>(std::min<std::uint64_t>(
			visibleSlotCount,
			std::numeric_limits<std::uint32_t>::max()));
	}

	EventBridge::RunewordPanelState EventBridge::GetRunewordPanelState(
		bool a_allowUndiscoveredRecipe,
		bool a_allowCurrentRecipeFallback)
	{
		const std::scoped_lock lock(_stateMutex);
		RunewordPanelState panelState{};
		panelState.maxRegularAffixCount = static_cast<std::uint32_t>(kMaxRegularAffixesPerItem);
		panelState.standardReforgeCost = detail::kSelectedReforgeCost;
		panelState.lockedReforgeCost = detail::kSelectedReforgeCost;
		panelState.debugTools = _loot.debugHudNotifications || _loot.debugLog;
		panelState.exchangeReforgeScrollCost = detail::kExchangeScrollsPerReforgeOrb;
		panelState.exchangeScourScrollCost = detail::kExchangeScrollsPerScouringOrb;
		panelState.selectedReforgesPerItem = detail::kSelectedReforgesPerItem;
		panelState.selectedReforgesLeft = detail::kSelectedReforgesPerItem;
		PopulateEquippedBuildSummary(panelState);
		if (!_configLoaded) {
			return panelState;
		}
		panelState.pityKnown = true;
		panelState.runewordFragmentFailStreak = _lootState.runewordFragmentFailStreak;
		panelState.runewordFragmentFailStreakThreshold = kRunewordFragmentPityFailThreshold;
		panelState.reforgeOrbFailStreak = _lootState.reforgeOrbFailStreak;
		panelState.reforgeOrbFailStreakThreshold = kReforgeOrbPityFailThreshold;

		SanitizeRunewordState();
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (player) {
			if (auto* orb = PluginEditorIds::Lookup<RE::TESObjectMISC>("CAFF_Misc_ReforgeOrb")) {
				panelState.reforgeOrbsKnown = true;
				panelState.reforgeOrbsOwned = static_cast<std::uint32_t>(
					std::max(0, player->GetItemCount(orb)));
			}

            if (auto* item = PluginEditorIds::Lookup<RE::TESObjectMISC>("CAFF_Misc_IdentifyScroll")) {
                panelState.identifyScrollsKnown = true;
                panelState.identifyScrollsOwned = static_cast<std::uint32_t>(std::max(0, player->GetItemCount(item)));
            }
            if (auto* item = PluginEditorIds::Lookup<RE::TESObjectMISC>("CAFF_Misc_ScouringOrb")) {
                panelState.scouringOrbsKnown = true;
                panelState.scouringOrbsOwned = static_cast<std::uint32_t>(std::max(0, player->GetItemCount(item)));
            }

			std::vector<std::uint64_t> referencedRuneTokens;
			for (const auto& recipe : _runewordState.recipes) {
				if (!HasRunewordRuntimeEffect(recipe)) {
					continue;
				}
				referencedRuneTokens.insert(
					referencedRuneTokens.end(),
					recipe.runeTokens.begin(),
					recipe.runeTokens.end());
			}
			const auto dashboardRuneTokens = BuildRunewordRuneDashboardTokens(
				_runewordState.runeTokenPool,
				referencedRuneTokens);
			panelState.runeInventoryExpectedCount = static_cast<std::uint32_t>(
				std::min<std::size_t>(dashboardRuneTokens.size(), std::numeric_limits<std::uint32_t>::max()));

			auto runeInventory = BuildRunewordRuneInventorySnapshot(
				dashboardRuneTokens,
				[&](std::uint64_t a_runeToken) -> std::optional<std::uint32_t> {
					auto* fragment = LookupRunewordFragmentItem(_runewordState.runeNameByToken, a_runeToken);
					if (!fragment) {
						return std::nullopt;
					}
					return static_cast<std::uint32_t>(std::max(0, player->GetItemCount(fragment)));
				});
			if (runeInventory && PopulateRunewordRuneInventoryNames(
					*runeInventory,
					[&](std::uint64_t a_runeToken) -> std::optional<std::string_view> {
						const auto nameIt = _runewordState.runeNameByToken.find(a_runeToken);
						if (nameIt == _runewordState.runeNameByToken.end()) {
							return std::nullopt;
						}
						// FE-ASR rune name; the internal key stays in runeNameByToken.
						return RuneforgePresentation::RuneNameOr(nameIt->second);
					})) {
				panelState.runeInventoryKnown = true;
				panelState.runeInventory = std::move(*runeInventory);
			}
		}

		if (a_allowCurrentRecipeFallback) {
			if (const auto* currentRecipe = GetCurrentRunewordRecipe()) {
				panelState.recipeName = currentRecipe->displayName;
				panelState.recipeNameEn = currentRecipe->PresentationNameEn();
				panelState.recipeNameKo = currentRecipe->displayNameKo;
				panelState.recipeToken = currentRecipe->token;
			}
		}
		if (!_runewordState.selectedBaseKey) {
			panelState.expandAffixUnavailableReason = "no_base";
			return panelState;
		}
		panelState.hasBase = true;
		if (const auto it = _instanceTrackingState.selectedReforgeCounts.find(*_runewordState.selectedBaseKey);
			it != _instanceTrackingState.selectedReforgeCounts.end()) {
			panelState.selectedReforgesLeft = detail::RemainingSelectedReforges(it->second);
		}
		bool expansionLayoutValid = true;
		std::uint8_t expansionPrefixCount = 0u;
		std::uint8_t expansionSuffixCount = 0u;
		std::uint8_t expansionRunewordCount = 0u;
		std::unordered_set<std::string_view> expansionSuffixFamilies;

		if (const auto slotsIt = _instanceTrackingState.instanceAffixes.find(*_runewordState.selectedBaseKey);
			slotsIt != _instanceTrackingState.instanceAffixes.end()) {
			const auto* completedRecipe = ResolveCompletedRunewordRecipe(*_runewordState.selectedBaseKey);
			const auto completedToken = completedRecipe ? completedRecipe->resultAffixToken : 0u;
			if (!detail::HasUniqueAffixTokens(slotsIt->second)) {
				expansionLayoutValid = false;
			}
			panelState.reforgeLockCandidates.reserve(slotsIt->second.count);
			for (std::uint8_t i = 0; i < slotsIt->second.count; ++i) {
				const auto token = slotsIt->second.tokens[i];
				if (token == 0u) {
					expansionLayoutValid = false;
					continue;
				}
				if (_runewordState.recipeIndexByResultAffixToken.contains(token)) {
					++expansionRunewordCount;
					if (token != completedToken) {
						expansionLayoutValid = false;
					}
					continue;
				}
				panelState.regularAffixTokens.push_back(token);

				// Legacy tokens (unknown or unslotted) still occupy a regular slot,
				// so the panel offers Scour to reroll them. They are never
				// selected-reforge candidates.
				const auto affixIt = _affixRuntimeState.affixRegistry.affixIndexByToken.find(token);
				if (affixIt == _affixRuntimeState.affixRegistry.affixIndexByToken.end() ||
					affixIt->second >= _affixRuntimeState.affixes.size()) {
					expansionLayoutValid = false;
					++panelState.regularAffixCount;
					continue;
				}

				const auto& affix = _affixRuntimeState.affixes[affixIt->second];
				if (affix.slot != AffixSlot::kPrefix && affix.slot != AffixSlot::kSuffix) {
					expansionLayoutValid = false;
					++panelState.regularAffixCount;
					continue;
				}
				if (affix.slot == AffixSlot::kPrefix) {
					++expansionPrefixCount;
				} else {
					++expansionSuffixCount;
					if (affix.family.empty() || !expansionSuffixFamilies.insert(affix.family).second) {
						expansionLayoutValid = false;
					}
				}

				std::string nameEn = affix.displayNameEn;
				if (nameEn.empty()) {
					nameEn = affix.id;
				}
				std::string nameKo = affix.displayNameKo;
				if (nameKo.empty()) {
					nameKo = affix.displayName.empty() ? affix.id : affix.displayName;
				}
				panelState.reforgeLockCandidates.push_back(RunewordReforgeLockCandidate{
					.affixToken = token,
					.displayNameEn = std::move(nameEn),
					.displayNameKo = std::move(nameKo),
					.slotKind = affix.slot == AffixSlot::kPrefix ? "prefix" : "suffix",
				});
				++panelState.regularAffixCount;
			}
			const auto expectedRunewordCount = completedToken != 0u ? 1u : 0u;
			if (expansionRunewordCount != expectedRunewordCount ||
				(completedToken != 0u && slotsIt->second.GetPrimary() != completedToken) ||
				slotsIt->second.count != panelState.regularAffixCount + expansionRunewordCount) {
				expansionLayoutValid = false;
			}
		}

		const auto regularCount = static_cast<std::uint8_t>(std::min<std::uint32_t>(
			panelState.regularAffixCount,
			std::numeric_limits<std::uint8_t>::max()));
		const auto expansionPolicy = detail::ResolveRegularAffixExpansionPolicy(regularCount);
		if (expansionPolicy) {
			panelState.expandAffixCost = expansionPolicy->orbCost;
		}
		if (regularCount == 0u) {
			panelState.expandAffixUnavailableReason = "requires_first_affix";
		} else if (regularCount == kMaxRegularAffixesPerItem) {
			panelState.expandAffixUnavailableReason = "max_slots";
		} else if (_loot.stripTrackedSuffixSlots) {
			panelState.expandAffixUnavailableReason = "suffix_slots_disabled";
		} else if (!expansionPolicy || !expansionLayoutValid ||
			!detail::IsCanonicalRegularAffixExpansionLayout(
				regularCount,
				expansionPrefixCount,
				expansionSuffixCount)) {
			panelState.expandAffixUnavailableReason = "invalid_layout";
		} else if (_runewordState.transmuteInProgress || _runewordState.affixExpansionInProgress ||
			!panelState.reforgeOrbsKnown) {
			panelState.expandAffixUnavailableReason = "unavailable";
		} else if (panelState.reforgeOrbsOwned < panelState.expandAffixCost) {
			panelState.expandAffixUnavailableReason = "insufficient_orbs";
		} else {
			panelState.canExpandAffix = true;
			panelState.expandAffixUnavailableReason.clear();
		}

		// Re-transmutation falls through to the normal recipe selection flow.
		// The old runeword remains active until the replacement commits successfully.

		std::uint32_t inserted = 0u;

		const auto stateIt = _runewordState.instanceStates.find(*_runewordState.selectedBaseKey);
		if (stateIt != _runewordState.instanceStates.end()) {
			inserted = stateIt->second.insertedRunes;
		}

		const auto* completedRecipe = ResolveCompletedRunewordRecipe(*_runewordState.selectedBaseKey);
		const auto* recipe = ResolveSelectedRunewordRecipe(
			*_runewordState.selectedBaseKey,
			completedRecipe,
			a_allowCurrentRecipeFallback);
		if (!a_allowUndiscoveredRecipe && recipe && !completedRecipe &&
			!_feasrKnownRunewordRecipeIds.contains(recipe->id)) {
			recipe = nullptr;
		}
		if (!recipe || recipe->runeTokens.empty()) {
			return panelState;
		}

		panelState.hasRecipe = true;
		panelState.recipeName = recipe->displayName;
		panelState.recipeNameEn = recipe->PresentationNameEn();
		panelState.recipeNameKo = recipe->displayNameKo;
		panelState.recipeToken = recipe->token;
		panelState.totalRunes = static_cast<std::uint32_t>(recipe->runeTokens.size());
		panelState.insertedRunes = std::min(inserted, panelState.totalRunes);

		RE::InventoryEntryData* selectedEntry = nullptr;
		RE::ExtraDataList* selectedXList = nullptr;
		if (ResolvePlayerInventoryInstance(
				*_runewordState.selectedBaseKey,
				selectedEntry,
				selectedXList) &&
			selectedEntry && selectedEntry->object) {
			PopulateSpecializedBaseWarning(panelState, recipe->id, selectedEntry->object);

		}

		const auto applyBlockReason = ResolveRunewordApplyBlockReason(*_runewordState.selectedBaseKey, *recipe);
		panelState.isComplete = applyBlockReason == RunewordApplyBlockReason::kAlreadyComplete;
		if (panelState.isComplete) {
			panelState.insertedRunes = panelState.totalRunes;
			return panelState;
		}
		const bool canApplyResult = applyBlockReason == RunewordApplyBlockReason::kNone;

		if (panelState.insertedRunes >= panelState.totalRunes) {
			// Legacy: allow finalization only when result can actually be applied.
			panelState.canInsert = CanFinalizeRunewordFromPanel(
				panelState.insertedRunes,
				panelState.totalRunes,
				canApplyResult);
			if (!canApplyResult) {
				panelState.missingSummary = BuildRunewordApplyBlockMessage(applyBlockReason);
			}
			return panelState;
		}

		// Transmute-only UI: require all remaining rune fragments at once.
		const auto requiredCounts = BuildRuneTokenCounts<16>(
			std::span<const std::uint64_t>(recipe->runeTokens.data(), recipe->runeTokens.size()),
			panelState.insertedRunes);

		bool ready = true;
		bool firstMissingSet = false;
		std::string missingSummary;

		panelState.requiredRunes.reserve(requiredCounts.size);
		for (std::size_t i = 0; i < requiredCounts.size; ++i) {
			const auto token = requiredCounts.entries[i].token;
			const auto required = requiredCounts.entries[i].count;
			if (token == 0u || required == 0u) {
				continue;
			}

			const auto runeName = ResolveRuneDisplayName(token);

			const auto owned = GetOwnedRunewordFragmentCount(player, _runewordState.runeNameByToken, token);
			panelState.requiredRunes.push_back(RunewordRuneRequirement{
				.runeName = runeName,
				.required = required,
				.owned = owned,
			});
			if (owned >= required) {
				continue;
			}

			ready = false;
			const auto missing = required - owned;
			if (!missingSummary.empty()) {
				missingSummary.append(", ");
			}
			missingSummary.append(runeName);
			missingSummary.append(" x");
			missingSummary.append(std::to_string(missing));

			if (!firstMissingSet) {
				firstMissingSet = true;
				panelState.nextRuneName = runeName;
				panelState.nextRuneOwned = owned;
			}
		}

		if (!missingSummary.empty()) {
			panelState.missingSummary = std::move(missingSummary);
		} else if (!canApplyResult) {
			panelState.missingSummary = BuildRunewordApplyBlockMessage(applyBlockReason);
		}

		panelState.canInsert = CanInsertRunewordFromPanel(ready, canApplyResult);
		return panelState;
	}

	std::optional<std::string> EventBridge::GetSelectedRunewordBaseAffixTooltip(int a_uiLanguageMode)
	{
		const std::scoped_lock lock(_stateMutex);
		if (!_configLoaded) {
			return std::nullopt;
		}

		SanitizeRunewordState();
		if (!_runewordState.selectedBaseKey) {
			return std::nullopt;
		}

		RE::InventoryEntryData* entry = nullptr;
		RE::ExtraDataList* xList = nullptr;
		if (!ResolvePlayerInventoryInstance(*_runewordState.selectedBaseKey, entry, xList) || !entry || !entry->object || !xList) {
			return std::nullopt;
		}

		const std::string selectedName = ResolveInventoryDisplayName(entry, xList);
		return GetInstanceAffixTooltip(
			entry,
			selectedName,
			a_uiLanguageMode,
			"inventory",
			0u,
			*_runewordState.selectedBaseKey);
	}

}
