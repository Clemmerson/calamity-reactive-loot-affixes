#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/RunewordUiPolicy.h"

#include <cstdint>
#include <string>

namespace CalamityAffixes
{
	EventBridge::RunewordApplyBlockReason EventBridge::ResolveRunewordApplyBlockReason(
		std::uint64_t a_instanceKey,
		const RunewordRecipe& a_recipe) const
	{
		const auto affixIt = _affixRuntimeState.affixRegistry.affixIndexByToken.find(a_recipe.resultAffixToken);
		if (affixIt == _affixRuntimeState.affixRegistry.affixIndexByToken.end() || affixIt->second >= _affixRuntimeState.affixes.size()) {
			return RunewordApplyBlockReason::kMissingResultAffix;
		}

		const auto* completed = ResolveCompletedRunewordRecipe(a_instanceKey);
		if (IsSameCompletedRuneword(
				completed ? completed->resultAffixToken : 0u,
				a_recipe.resultAffixToken)) {
			return RunewordApplyBlockReason::kAlreadyComplete;
		}

		RE::InventoryEntryData* baseEntry = nullptr;
		RE::ExtraDataList* baseXList = nullptr;
		if (!ResolvePlayerInventoryInstance(a_instanceKey, baseEntry, baseXList) ||
			!baseEntry || !baseEntry->object || !IsLootObjectEligibleForAffixes(baseEntry->object)) {
			return RunewordApplyBlockReason::kIneligibleBase;
		}



		if (const auto it = _instanceTrackingState.instanceAffixes.find(a_instanceKey); it != _instanceTrackingState.instanceAffixes.end()) {
			const auto& slots = it->second;
			auto effectiveCount = slots.count;
			if (!slots.HasToken(a_recipe.resultAffixToken)) {
				// Keep the completed result active for rollback, but count its slot as
				// replaceable while validating a re-transmutation.
				if (const auto* completed = ResolveCompletedRunewordRecipe(slots);
					completed && completed->resultAffixToken != a_recipe.resultAffixToken &&
					slots.HasToken(completed->resultAffixToken) && effectiveCount > 0u) {
					--effectiveCount;
				}
			}

			if (!slots.HasToken(a_recipe.resultAffixToken) &&
				effectiveCount >= static_cast<std::uint8_t>(kMaxAffixesPerItem)) {
				return RunewordApplyBlockReason::kAffixSlotsFull;
			}
		}

		return RunewordApplyBlockReason::kNone;
	}

	std::string EventBridge::BuildRunewordApplyBlockMessage(RunewordApplyBlockReason a_reason)
	{
		switch (a_reason) {
		case RunewordApplyBlockReason::kMissingResultAffix:
			return "Runeword result affix missing";
		case RunewordApplyBlockReason::kAlreadyComplete:
			return "Selected base already has this runeword";
		case RunewordApplyBlockReason::kAffixSlotsFull: {
			std::string reason = "Affix slots full (max ";
			reason.append(std::to_string(kMaxAffixesPerItem));
			reason.push_back(')');
			return reason;
		}
		case RunewordApplyBlockReason::kIneligibleBase:
			return "This item is not supported by Calamity for Runewords";
		default:
			break;
		}

		return {};
	}
}
