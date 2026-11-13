#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/RunewordUiPolicy.h"
#include "EventBridge.Loot.Runeword.Detail.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace CalamityAffixes
{
	using namespace RunewordDetail;

	bool EventBridge::SelectRunewordRecipeForBase(
		std::uint64_t a_expectedInstanceKey,
		std::string_view a_recipeId,
		bool a_allowUnknownRecipeDebug)
	{
		const std::scoped_lock lock(_stateMutex);
		if (!_runtimeSettings.enabled.load(std::memory_order_relaxed) ||
			_runewordState.transmuteInProgress || _runewordState.affixExpansionInProgress ||
			(!_feasrDiscoveryReady && !a_allowUnknownRecipeDebug) ||
			a_expectedInstanceKey == 0u || a_recipeId.empty() ||
			(!a_allowUnknownRecipeDebug && !_feasrKnownRunewordRecipeIds.contains(std::string(a_recipeId))) ||
			!_runewordState.selectedBaseKey || *_runewordState.selectedBaseKey != a_expectedInstanceKey) {
			return false;
		}
		const auto recipeIt = std::find_if(_runewordState.recipes.begin(), _runewordState.recipes.end(), [&](const auto& recipe) {
			return recipe.id == a_recipeId && HasRunewordRuntimeEffect(recipe);
		});
		return recipeIt != _runewordState.recipes.end() && SelectRunewordRecipe(recipeIt->token, a_allowUnknownRecipeDebug);
	}

	bool EventBridge::SelectRunewordRecipe(std::uint64_t a_recipeToken, bool a_allowUnknownRecipeDebug)
	{
		const std::scoped_lock lock(_stateMutex);
		if (!_configLoaded || !_runtimeSettings.enabled.load(std::memory_order_relaxed) ||
			_runewordState.transmuteInProgress || _runewordState.affixExpansionInProgress ||
			(!_feasrDiscoveryReady && !a_allowUnknownRecipeDebug) ||
			a_recipeToken == 0u || _runewordState.recipes.empty()) {
			return false;
		}

		SanitizeRunewordState();
		const auto it = _runewordState.recipeIndexByToken.find(a_recipeToken);
		if (it == _runewordState.recipeIndexByToken.end() || it->second >= _runewordState.recipes.size()) {
			return false;
		}

		const auto idx = it->second;
		const auto& recipe = _runewordState.recipes[idx];
		if (!a_allowUnknownRecipeDebug && !_feasrKnownRunewordRecipeIds.contains(recipe.id)) {
			return false;
		}
		if (!HasRunewordRuntimeEffect(recipe)) {
			EmitHudNotification("Runeword Recipe: runtime effect not available.");
			return false;
		}
		_runewordState.recipeCycleCursor = static_cast<std::uint32_t>(idx);

		if (_runewordState.selectedBaseKey) {
			const auto selectedKey = *_runewordState.selectedBaseKey;
			const bool completedBase = ResolveCompletedRunewordRecipe(selectedKey) != nullptr;

			if (ShouldClearRunewordInProgressState(completedBase)) {
				_runewordState.instanceStates.erase(selectedKey);
			}

			// Always record the explicit recipe selection so the UI can
			// highlight the newly chosen recipe (even for re-transmutation).
			auto& state = _runewordState.instanceStates[selectedKey];
			if (state.recipeToken != recipe.token) {
				state.recipeToken = recipe.token;
				state.insertedRunes = 0u;
			}
		}

		// FE-ASR presentation: public recipe name and rune sequence (mechanical order).
		std::string note = "Runeword Recipe: " + recipe.HudName() + " [" + recipe.PresentationRuneSequence() + "]";
		if (_runewordState.selectedBaseKey) {
			AppendRunewordSelectionRecommendation(note, recipe, ResolveInstanceLootType(*_runewordState.selectedBaseKey));
		}

		EmitHudNotification(note.c_str());
		return true;
	}
}
