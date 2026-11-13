#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/RunewordUtil.h"
#include "EventBridge.Loot.Runeword.Detail.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace CalamityAffixes
{
	using namespace RunewordDetail;

	EventBridge::OperationResult EventBridge::TransmuteRunewordBase(
		std::uint64_t a_expectedInstanceKey,
		std::string_view a_expectedRecipeId,
		std::uint32_t a_expectedInsertedRunes,
		bool a_allowUnknownRecipeDebug)
	{
		const std::scoped_lock lock(_stateMutex);
		SKSE::log::info(
			"RuneforgeDiag: TransmuteRunewordBase begin key={:016X} recipe='{}' expectedInserted={} configLoaded={} runtimeEnabled={} discoveryReady={} knownRecipe={}",
			a_expectedInstanceKey,
			a_expectedRecipeId,
			a_expectedInsertedRunes,
			_configLoaded,
			_runtimeSettings.enabled.load(std::memory_order_relaxed),
			_feasrDiscoveryReady,
			_feasrKnownRunewordRecipeIds.contains(std::string(a_expectedRecipeId)));
		if (!_configLoaded || !_runtimeSettings.enabled.load(std::memory_order_relaxed) ||
			(!_feasrDiscoveryReady && !a_allowUnknownRecipeDebug) ||
			a_expectedInstanceKey == 0u || a_expectedRecipeId.empty() ||
			(!a_allowUnknownRecipeDebug && !_feasrKnownRunewordRecipeIds.contains(std::string(a_expectedRecipeId)))) {
			SKSE::log::warn("RuneforgeDiag: TransmuteRunewordBase readiness/argument gate refused request");
			return { false, "Runeforge is not ready." };
		}
		if (!_runewordState.selectedBaseKey || *_runewordState.selectedBaseKey != a_expectedInstanceKey) {
			if (_runewordState.selectedBaseKey) {
				SKSE::log::warn(
					"RuneforgeDiag: transmute selected-base mismatch expected={:016X} actual={:016X}",
					a_expectedInstanceKey,
					*_runewordState.selectedBaseKey);
			} else {
				SKSE::log::warn("RuneforgeDiag: transmute selected-base mismatch expected={:016X} actual=none", a_expectedInstanceKey);
			}
			return { false, "The selected base changed. Review the current selection." };
		}

		const auto before = GetRunewordPanelState();
		const auto selectedRecipeIt = _runewordState.recipeIndexByToken.find(before.recipeToken);
		if (!before.hasRecipe || selectedRecipeIt == _runewordState.recipeIndexByToken.end() ||
			selectedRecipeIt->second >= _runewordState.recipes.size() ||
			_runewordState.recipes[selectedRecipeIt->second].id != a_expectedRecipeId) {
			SKSE::log::warn(
				"RuneforgeDiag: transmute selected-recipe mismatch panelToken={:016X} expectedRecipe='{}' hasRecipe={}",
				before.recipeToken,
				a_expectedRecipeId,
				before.hasRecipe);
			return { false, "The selected recipe changed. Review the current selection." };
		}
		if (before.insertedRunes != a_expectedInsertedRunes) {
			SKSE::log::warn(
				"RuneforgeDiag: transmute progress mismatch panelInserted={} expectedInserted={}",
				before.insertedRunes,
				a_expectedInsertedRunes);
			return { false, "Runeword progress changed. Review the current requirements." };
		}
		SKSE::log::info(
			"RuneforgeDiag: transmute preflight hasRecipe={} canInsert={} inserted={} total={} missing='{}' baseKey={:016X}",
			before.hasRecipe,
			before.canInsert,
			before.insertedRunes,
			before.totalRunes,
			before.missingSummary,
			a_expectedInstanceKey);
		if (!before.canInsert) {
			SKSE::log::warn("RuneforgeDiag: transmute panel rejected canInsert=false");
			return { false, before.missingSummary.empty() ? "Runeword cannot be transmuted yet." : before.missingSummary };
		}

		InsertRunewordRuneIntoSelectedBase(a_allowUnknownRecipeDebug);
		const auto* completedRecipe = ResolveCompletedRunewordRecipe(a_expectedInstanceKey);
		if (completedRecipe && completedRecipe->id == a_expectedRecipeId) {
			SKSE::log::info("RuneforgeDiag: transmute postcondition passed recipe='{}'", completedRecipe->id);
			return { true, "Runeword transmuted." };
		}
		SKSE::log::error(
			"RuneforgeDiag: transmute postcondition failed key={:016X} requestedRecipe='{}' completedRecipe='{}'",
			a_expectedInstanceKey,
			a_expectedRecipeId,
			completedRecipe ? completedRecipe->id : "none");
		return { false, "Transmute did not complete. Check the current requirements and inventory." };
	}

	bool EventBridge::ApplyRunewordResult(
		std::uint64_t a_instanceKey,
		const RunewordRecipe& a_recipe,
		std::string* a_outFailureReason,
		bool a_allowUnknownRecipeDebug)
	{
		if (!_runtimeSettings.enabled.load(std::memory_order_relaxed) ||
			(!_feasrDiscoveryReady && !a_allowUnknownRecipeDebug) ||
			(!a_allowUnknownRecipeDebug && !_feasrKnownRunewordRecipeIds.contains(a_recipe.id))) {
			if (a_outFailureReason) {
				*a_outFailureReason = "recipe-locked-or-runtime-disabled";
			}
			return false;
		}
		const auto* completedBefore = ResolveCompletedRunewordRecipe(a_instanceKey);
		const auto replacedResultToken = completedBefore ? completedBefore->resultAffixToken : 0u;
		const auto blockReason = ResolveRunewordApplyBlockReason(a_instanceKey, a_recipe);
		if (blockReason == RunewordApplyBlockReason::kMissingResultAffix) {
			if (a_outFailureReason) {
				*a_outFailureReason = "missing-result-affix";
			}
			std::string note = "Runeword failed: missing affix ";
			note.append(a_recipe.HudName());
			EmitHudNotification(note.c_str());
			SKSE::log::error(
				"CalamityAffixes: runeword result affix missing (recipe={}, resultToken={:016X}).",
				a_recipe.id,
				a_recipe.resultAffixToken);
			return false;
		}
		if (blockReason == RunewordApplyBlockReason::kAlreadyComplete) {
			if (a_outFailureReason) {
				*a_outFailureReason = "already-complete";
			}
			EmitHudNotification("Runeword blocked: selected base already has this runeword.");
			SKSE::log::info(
				"CalamityAffixes: runeword apply blocked (instance={:016X}, recipe={}, reason=already-complete).",
				a_instanceKey,
				a_recipe.id);
			return false;
		}
		if (blockReason == RunewordApplyBlockReason::kAffixSlotsFull) {
			if (a_outFailureReason) {
				*a_outFailureReason = "affix-slots-full";
			}
			std::string note = "Runeword failed: base has max affixes";
			EmitHudNotification(note.c_str());
			SKSE::log::warn(
				"CalamityAffixes: runeword apply blocked (instance={:016X}, recipe={}, reason=max-affixes-full, max={}).",
				a_instanceKey,
				a_recipe.id,
				kMaxAffixesPerItem);
			return false;
		}

		if (blockReason != RunewordApplyBlockReason::kNone) {
			if (a_outFailureReason) {
				*a_outFailureReason = "incompatible-recipe-base";
			}
			const std::string message = "Runeword blocked: " + BuildRunewordApplyBlockMessage(blockReason);
			EmitHudNotification(message.c_str());
			return false;
		}

		const auto affixIt = _affixRuntimeState.affixRegistry.affixIndexByToken.find(a_recipe.resultAffixToken);
		if (affixIt == _affixRuntimeState.affixRegistry.affixIndexByToken.end() || affixIt->second >= _affixRuntimeState.affixes.size()) {
			if (a_outFailureReason) {
				*a_outFailureReason = "result-affix-invalidated";
			}
			EmitHudNotification("Runeword failed: internal error (affix invalidated).");
			SKSE::log::error(
				"CalamityAffixes: runeword result affix became invalid after policy check (recipe={}, resultToken={:016X}).",
				a_recipe.id,
				a_recipe.resultAffixToken);
			return false;
		}

		auto& slots = _instanceTrackingState.instanceAffixes[a_instanceKey];
		const InstanceAffixSlots previousSlots = slots;
		if (replacedResultToken != 0u && replacedResultToken != a_recipe.resultAffixToken) {
			slots.RemoveToken(replacedResultToken);
		}

		if (!slots.PromoteTokenToPrimary(a_recipe.resultAffixToken)) {
			slots = previousSlots;
			if (a_outFailureReason) {
				*a_outFailureReason = "promote-to-primary-failed";
			}
			EmitHudNotification("Runeword failed: internal error (apply failed).");
			SKSE::log::error(
				"CalamityAffixes: runeword promote-to-primary failed after policy check (instance={:016X}, recipe={}, resultToken={:016X}).",
				a_instanceKey,
				a_recipe.id,
				a_recipe.resultAffixToken);
			return false;
		}

		if (replacedResultToken != 0u && replacedResultToken != a_recipe.resultAffixToken) {
			_instanceTrackingState.instanceStates.erase(MakeInstanceStateKey(a_instanceKey, replacedResultToken));
		}
		EnsureInstanceRuntimeState(a_instanceKey, a_recipe.resultAffixToken);
		_runewordState.instanceStates.erase(a_instanceKey);

		RE::InventoryEntryData* entry = nullptr;
		RE::ExtraDataList* xList = nullptr;
		if (ResolvePlayerInventoryInstance(a_instanceKey, entry, xList) && entry && xList) {
			if (entry->object) {
				RememberInstanceBaseObject(a_instanceKey, entry->object->GetFormID());
			}
			EnsureMultiAffixDisplayName(entry, xList, slots);
		}

		RebuildActiveCounts();

		std::string note = "Runeword Complete: ";
		note.append(a_recipe.HudName());
		EmitHudNotification(note.c_str());
		SKSE::log::info(
			"CalamityAffixes: runeword completed (recipe={}, resultAffix={}).",
			a_recipe.id,
			_affixRuntimeState.affixes[affixIt->second].id);
		if (!a_allowUnknownRecipeDebug) {
			SendModEvent("FEASR_Runeforge_SuccessfulUse", a_recipe.id, 1.0f, RE::PlayerCharacter::GetSingleton());
		}
		return true;
	}

	void EventBridge::InsertRunewordRuneIntoSelectedBase(bool a_allowUnknownRecipeDebug)
	{
		SKSE::log::info("RuneforgeDiag: InsertRunewordRuneIntoSelectedBase entered debug={}", a_allowUnknownRecipeDebug);
		if (!_configLoaded || !_runtimeSettings.enabled.load(std::memory_order_relaxed)) {
			SKSE::log::warn(
				"RuneforgeDiag: insert refused configLoaded={} runtimeEnabled={}",
				_configLoaded,
				_runtimeSettings.enabled.load(std::memory_order_relaxed));
			return;
		}
		if (_runewordState.transmuteInProgress) {
			SKSE::log::warn("RuneforgeDiag: insert refused because transmute is already in progress");
			EmitHudNotification("Runeword: transmute already in progress.");
			return;
		}
		if (_runewordState.affixExpansionInProgress) {
			SKSE::log::warn("RuneforgeDiag: insert refused because affix expansion is in progress");
			EmitHudNotification("Runeword: affix expansion is in progress.");
			return;
		}

		struct ScopedTransmuteInFlight
		{
			bool& flag;

			explicit ScopedTransmuteInFlight(bool& a_flag) :
				flag(a_flag)
			{
				flag = true;
			}

			~ScopedTransmuteInFlight()
			{
				flag = false;
			}
		};
		ScopedTransmuteInFlight transmuteGuard(_runewordState.transmuteInProgress);

		SanitizeRunewordState();
		std::uint64_t instanceKey = 0u;
		RE::InventoryEntryData* entry = nullptr;
		RE::ExtraDataList* xList = nullptr;
		std::string baseResolveFailure;
		if (!ResolveSelectedRunewordBaseInstance(instanceKey, entry, xList, &baseResolveFailure, false)) {
			SKSE::log::warn("RuneforgeDiag: insert could not resolve selected base: {}", baseResolveFailure);
			std::string note = "Runeword: ";
			note.append(baseResolveFailure);
			EmitHudNotification(note.c_str());
			return;
		}

		auto& state = _runewordState.instanceStates[instanceKey];
		const RunewordRecipe* recipe = ResolvePendingRunewordRecipe(instanceKey);
		if (!recipe || recipe->runeTokens.empty()) {
			SKSE::log::warn("RuneforgeDiag: insert has no pending recipe for key={:016X}", instanceKey);
			EmitHudNotification("Runeword: recipe is not set.");
			return;
		}
		if ((!_feasrDiscoveryReady && !a_allowUnknownRecipeDebug) ||
			(!a_allowUnknownRecipeDebug && !_feasrKnownRunewordRecipeIds.contains(recipe->id))) {
			SKSE::log::warn("RuneforgeDiag: insert refused undiscovered recipe='{}'", recipe->id);
			EmitHudNotification("Runeword: recipe has not been discovered.");
			return;
		}

		const auto blockReason = ResolveRunewordApplyBlockReason(instanceKey, *recipe);
		if (blockReason == RunewordApplyBlockReason::kMissingResultAffix) {
			std::string note = "Runeword failed: missing affix ";
			note.append(recipe->HudName());
			EmitHudNotification(note.c_str());
			SKSE::log::error(
				"CalamityAffixes: runeword result affix missing before transmute (recipe={}, resultToken={:016X}).",
				recipe->id,
				recipe->resultAffixToken);
			return;
		}
		if (blockReason == RunewordApplyBlockReason::kAlreadyComplete) {
			EmitHudNotification("Runeword blocked: selected base already has this runeword.");
			SKSE::log::info(
				"CalamityAffixes: runeword transmute blocked before fragment consumption (instance={:016X}, recipe={}, reason=already-complete).",
				instanceKey,
				recipe->id);
			return;
		}
		if (blockReason == RunewordApplyBlockReason::kAffixSlotsFull) {
			SKSE::log::warn("RuneforgeDiag: insert refused because affix slots are full recipe='{}'", recipe->id);
			std::string note = "Runeword blocked: ";
			note.append(BuildRunewordApplyBlockMessage(blockReason));
			EmitHudNotification(note.c_str());
			return;
		}

		const auto totalRunes = recipe->runeTokens.size();
		if (state.insertedRunes >= totalRunes) {
			// Legacy finalize path.
			std::string failureReason;
			const bool applied = ApplyRunewordResult(instanceKey, *recipe, &failureReason, a_allowUnknownRecipeDebug);
			SKSE::log::info(
				"RuneforgeDiag: legacy finalize applied={} reason='{}' inserted={} total={} key={:016X} recipe='{}'",
				applied,
				failureReason,
				state.insertedRunes,
				totalRunes,
				instanceKey,
				recipe->id);
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			SKSE::log::error("RuneforgeDiag: insert has no PlayerCharacter singleton");
			return;
		}

		// Final preflight right before consumption to minimize "consume then fail" cases.
		if (!ResolveSelectedRunewordBaseInstance(instanceKey, entry, xList, &baseResolveFailure, true)) {
			SKSE::log::warn("RuneforgeDiag: insert preflight could not resolve base: {}", baseResolveFailure);
			std::string note = "Runeword: ";
			note.append(baseResolveFailure == "selected base is no longer available." ? "selected base became unavailable." : baseResolveFailure);
			EmitHudNotification(note.c_str());
			return;
		}
		const auto preflightBlockReason = ResolveRunewordApplyBlockReason(instanceKey, *recipe);
		if (preflightBlockReason != RunewordApplyBlockReason::kNone) {
			SKSE::log::warn(
				"RuneforgeDiag: insert preflight blocked enum={} recipe='{}' key={:016X}",
				static_cast<int>(preflightBlockReason),
				recipe->id,
				instanceKey);
			std::string note = "Runeword blocked: ";
			note.append(BuildRunewordApplyBlockMessage(preflightBlockReason));
			EmitHudNotification(note.c_str());
			return;
		}

		const auto requiredCounts = BuildRuneTokenCounts<16>(
			std::span<const std::uint64_t>(recipe->runeTokens.data(), recipe->runeTokens.size()),
			state.insertedRunes);

		bool ready = true;
		std::string missingSummary;
		for (std::size_t i = 0; i < requiredCounts.size; ++i) {
			const auto token = requiredCounts.entries[i].token;
			const auto required = requiredCounts.entries[i].count;
			if (token == 0u || required == 0u) {
				continue;
			}

			const auto runeName = ResolveRuneDisplayName(token);

			const auto owned = GetOwnedRunewordFragmentCount(player, _runewordState.runeNameByToken, token);
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
		}

		if (!ready) {
			SKSE::log::warn("RuneforgeDiag: insert missing required fragments: {}", missingSummary);
			std::string note = "Runeword missing fragments: ";
			note.append(missingSummary.empty() ? "?" : missingSummary);
			EmitHudNotification(note.c_str());
			return;
		}
		SKSE::log::info(
			"RuneforgeDiag: fragment requirements satisfied key={:016X} recipe='{}' inserted={} total={} distinctRequirements={}",
			instanceKey,
			recipe->id,
			state.insertedRunes,
			totalRunes,
			requiredCounts.size);

		std::vector<ConsumedRunewordFragment> consumedRunes;
		consumedRunes.reserve(requiredCounts.size);

		// Consume all required fragments.
		for (std::size_t i = 0; i < requiredCounts.size; ++i) {
			const auto token = requiredCounts.entries[i].token;
			const auto required = requiredCounts.entries[i].count;
			if (token == 0u || required == 0u) {
				continue;
			}

			std::string runeName = "Rune";
			if (const auto nameIt = _runewordState.runeNameByToken.find(token); nameIt != _runewordState.runeNameByToken.end()) {
				runeName = nameIt->second;
			}

			std::string consumeFailureReason;
			if (!ConsumeRunewordFragmentRequirement(
				player,
				token,
				required,
				consumedRunes,
				&runeName,
				&consumeFailureReason)) {
				const bool rollbackOk = RollbackConsumedRunewordFragments(
					player,
					consumedRunes,
					consumeFailureReason.empty() ? std::string_view{"consume-failed"} : std::string_view{ consumeFailureReason });
				// runeName stays the internal key for the logs below.
				std::string note = "Runeword failed to consume fragment: ";
				note.append(ResolveRuneDisplayName(token));
				if (rollbackOk) {
					note.append(" (rollback restored)");
				}
				EmitHudNotification(note.c_str());
				if (consumeFailureReason == "fragment-item-missing") {
					SKSE::log::error(
						"CalamityAffixes: runeword fragment item missing (requiredRuneToken={:016X}, runeName={}, rollbackOk={}).",
						token,
						runeName,
						rollbackOk);
				} else {
					SKSE::log::warn(
						"CalamityAffixes: runeword fragment consume failed (runeToken={:016X}, runeName={}, required={}, reason={}, rollbackOk={}).",
						token,
						runeName,
						required,
						consumeFailureReason.empty() ? "unknown" : consumeFailureReason,
						rollbackOk);
				}
				return;
			}
		}
		SKSE::log::info(
			"RuneforgeDiag: consumed all remaining fragments key={:016X} recipe='{}' consumedKinds={}",
			instanceKey,
			recipe->id,
			consumedRunes.size());

		RE::InventoryEntryData* postConsumeEntry = nullptr;
		RE::ExtraDataList* postConsumeXList = nullptr;
		if (!ResolvePlayerInventoryInstance(instanceKey, postConsumeEntry, postConsumeXList)) {
			const bool rollbackOk = RollbackConsumedRunewordFragments(player, consumedRunes, "base-unavailable-after-consume");
			SKSE::log::error(
				"RuneforgeDiag: base could not be re-resolved after fragment consume key={:016X} rollbackOk={}",
				instanceKey,
				rollbackOk);
			std::string note = "Runeword failed (base unavailable, stacked, or ambiguous)";
			note.append(rollbackOk ? ": fragments restored." : ": fragment rollback partial.");
			EmitHudNotification(note.c_str());
			if (_runewordState.selectedBaseKey && *_runewordState.selectedBaseKey == instanceKey) {
				_runewordState.selectedBaseKey.reset();
			}
			SKSE::log::error(
				"CalamityAffixes: runeword apply aborted after consume (reason=base-unavailable, instance={:016X}, recipe={}, rollbackOk={}).",
				instanceKey,
				recipe->id,
				rollbackOk);
			return;
		}

		std::string applyFailureReason;
		if (!ApplyRunewordResult(instanceKey, *recipe, &applyFailureReason, a_allowUnknownRecipeDebug)) {
			const bool rollbackOk = RollbackConsumedRunewordFragments(player, consumedRunes, "apply-failed");
			SKSE::log::error(
				"RuneforgeDiag: result application failed key={:016X} recipe='{}' reason='{}' rollbackOk={}",
				instanceKey,
				recipe->id,
				applyFailureReason,
				rollbackOk);
			std::string note = "Runeword failed";
			if (!applyFailureReason.empty()) {
				note.append(" (");
				note.append(applyFailureReason);
				note.push_back(')');
			}
			note.append(rollbackOk ? ": fragments restored." : ": fragment rollback partial.");
			EmitHudNotification(note.c_str());
			SKSE::log::error(
				"CalamityAffixes: runeword apply failed after fragment consume (recipe={}, reason={}, rollbackOk={}).",
				recipe->id,
				applyFailureReason.empty() ? "unknown" : applyFailureReason,
				rollbackOk);
			return;
		}
	}

}
