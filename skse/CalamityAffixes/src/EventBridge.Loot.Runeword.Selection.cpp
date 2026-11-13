#include "CalamityAffixes/EventBridge.h"

#include <optional>
#include <string>
#include <string_view>

namespace CalamityAffixes
{
	bool EventBridge::ResolveSelectedRunewordBaseInstance(
		std::uint64_t& a_outInstanceKey,
		RE::InventoryEntryData*& a_outEntry,
		RE::ExtraDataList*& a_outXList,
		std::string* a_outFailureMessage,
		bool a_requireEligible)
	{
		a_outInstanceKey = 0u;
		a_outEntry = nullptr;
		a_outXList = nullptr;

		if (!_runewordState.selectedBaseKey) {
			if (a_outFailureMessage) {
				*a_outFailureMessage = "select a base first.";
			}
			return false;
		}

		a_outInstanceKey = *_runewordState.selectedBaseKey;
		if (!ResolvePlayerInventoryInstance(a_outInstanceKey, a_outEntry, a_outXList) ||
			!a_outEntry || !a_outEntry->object || !a_outXList) {
			_runewordState.selectedBaseKey.reset();
			if (a_outFailureMessage) {
				*a_outFailureMessage = "selected base is no longer available.";
			}
			return false;
		}

		if (a_requireEligible && !IsLootObjectEligibleForAffixes(a_outEntry->object)) {
			if (a_outFailureMessage) {
				*a_outFailureMessage = "this item type is not eligible.";
			}
			return false;
		}

		RememberInstanceBaseObject(a_outInstanceKey, a_outEntry->object->GetFormID());
		return true;
	}

	const EventBridge::RunewordRecipe* EventBridge::ResolvePendingRunewordRecipe(std::uint64_t a_instanceKey)
	{
		auto& state = _runewordState.instanceStates[a_instanceKey];
		if (state.recipeToken == 0u) {
			if (const auto* completedRecipe = ResolveCompletedRunewordRecipe(a_instanceKey)) {
				state.recipeToken = completedRecipe->token;
			} else if (const auto* currentRecipe = GetCurrentRunewordRecipe()) {
				state.recipeToken = currentRecipe->token;
			}
		}

		if (const auto* recipe = FindRunewordRecipeByToken(state.recipeToken)) {
			return recipe;
		}

		if (const auto* currentRecipe = GetCurrentRunewordRecipe()) {
			state.recipeToken = currentRecipe->token;
			return currentRecipe;
		}

		return nullptr;
	}

	bool EventBridge::HasRunewordRuntimeEffect(const RunewordRecipe& a_recipe) const noexcept
	{
		const auto affixIt = _affixRuntimeState.affixRegistry.affixIndexByToken.find(a_recipe.resultAffixToken);
		return affixIt != _affixRuntimeState.affixRegistry.affixIndexByToken.end() && affixIt->second < _affixRuntimeState.affixes.size();
	}

	const EventBridge::RunewordRecipe* EventBridge::ResolveSelectedRunewordRecipe(
		std::uint64_t a_selectedBaseKey,
		const RunewordRecipe* a_preferredRecipe,
		bool a_allowCurrentRecipeFallback) const
	{
		if (a_preferredRecipe) {
			return a_preferredRecipe;
		}

		if (const auto stateIt = _runewordState.instanceStates.find(a_selectedBaseKey);
			stateIt != _runewordState.instanceStates.end()) {
			if (const auto* recipe = FindRunewordRecipeByToken(stateIt->second.recipeToken)) {
				return recipe;
			}
		}

		if (const auto* completedRecipe = ResolveCompletedRunewordRecipe(a_selectedBaseKey)) {
			return completedRecipe;
		}

		return a_allowCurrentRecipeFallback ? GetCurrentRunewordRecipe() : nullptr;
	}

	void EventBridge::AppendRunewordSelectionRecommendation(
		std::string& a_note,
		const RunewordRecipe& a_recipe,
		std::optional<LootItemType> a_currentBaseType) const
	{
		// Specialized recipe guidance is evaluated against the actual selected base
		// in the panel. Suppress the generic Weapon/Armor note so it cannot contradict
		// stronger guidance such as Unbending Will -> Shield.
		if (detail::ResolveSpecializedRunewordBase(a_recipe.id) != detail::SpecializedRunewordBase::kNone) {
			return;
		}
		if (!a_recipe.recommendedBaseType || !a_currentBaseType || *a_recipe.recommendedBaseType == *a_currentBaseType) {
			return;
		}

		a_note.append(" (Recommended ");
		a_note.append(DescribeLootItemType(*a_recipe.recommendedBaseType));
		a_note.append(", current ");
		a_note.append(DescribeLootItemType(*a_currentBaseType));
		a_note.push_back(')');
	}
}
