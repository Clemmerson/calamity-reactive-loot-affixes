#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/PluginEditorIds.h"
#include "EventBridge.Loot.Runeword.Detail.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <mutex>
#include <random>
#include <string>

namespace CalamityAffixes
{
	using namespace RunewordDetail;

		void EventBridge::GrantNextRequiredRuneFragment(std::uint32_t a_amount)
		{
			if (!_configLoaded || a_amount == 0u) {
				return;
			}

			SanitizeRunewordState();
			const RunewordRecipe* recipe = nullptr;
			std::uint32_t nextIndex = 0u;
			if (_runewordState.selectedBaseKey) {
				const auto stateIt = _runewordState.instanceStates.find(*_runewordState.selectedBaseKey);
				if (stateIt != _runewordState.instanceStates.end()) {
					recipe = FindRunewordRecipeByToken(stateIt->second.recipeToken);
					nextIndex = stateIt->second.insertedRunes;
				}
			}

			if (!recipe) {
				recipe = GetCurrentRunewordRecipe();
			}
			if (!recipe || recipe->runeTokens.empty()) {
				return;
			}

				if (nextIndex >= recipe->runeTokens.size()) {
					nextIndex = 0u;
				}

				const auto runeToken = recipe->runeTokens[nextIndex];
				std::string runeName = "Rune";
				if (const auto nameIt = _runewordState.runeNameByToken.find(runeToken); nameIt != _runewordState.runeNameByToken.end()) {
					runeName = nameIt->second;
				}

				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player) {
					return;
				}

				// runeName stays the internal key for logs; HUD text uses the FE-ASR name.
				const auto runeDisplayName = ResolveRuneDisplayName(runeToken);
				const auto given = GrantRunewordFragments(player, _runewordState.runeNameByToken, runeToken, a_amount);
				if (given == 0u) {
					std::string note = "Runeword fragment item missing: ";
					note.append(runeDisplayName);
					EmitHudNotification(note.c_str());
					SKSE::log::error(
						"CalamityAffixes: runeword fragment item missing (runeToken={:016X}, runeName={}).",
						runeToken,
						runeName);
					return;
				}

				const auto owned = GetOwnedRunewordFragmentCount(player, _runewordState.runeNameByToken, runeToken);
				const std::string note = "Runeword Fragment + " + runeDisplayName + " (" + std::to_string(owned) + ")";
				EmitHudNotification(note.c_str());
			}

		void EventBridge::GrantCurrentRecipeRuneSet(std::uint32_t a_amount)
		{
			if (!_configLoaded || a_amount == 0u) {
				return;
			}

			const auto* recipe = GetCurrentRunewordRecipe();
				if (!recipe) {
					return;
				}

				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player) {
					return;
				}

				std::uint32_t totalGiven = 0u;
				for (const auto runeToken : recipe->runeTokens) {
					const auto given = GrantRunewordFragments(player, _runewordState.runeNameByToken, runeToken, a_amount);
					const auto maxVal = std::numeric_limits<std::uint32_t>::max();
					totalGiven = (totalGiven > maxVal - given) ? maxVal : (totalGiven + given);
				}

				if (totalGiven == 0u) {
					EmitHudNotification("Runeword fragments: missing rune fragment item records.");
					SKSE::log::error("CalamityAffixes: runeword fragment items missing (grant recipe set).");
					return;
				}

				const std::string note = "Runeword Fragments + Set: " + recipe->HudName();
				EmitHudNotification(note.c_str());
			}

		std::uint32_t EventBridge::GrantReforgeOrbs(std::uint32_t a_amount)
		{
			return GrantCraftingCurrency("CAFF_Misc_ReforgeOrb", "Reforge Orbs", a_amount);
		}

		std::uint32_t EventBridge::GrantCraftingCurrency(
			std::string_view a_editorId, std::string_view a_label, std::uint32_t a_amount)
		{
			if (!_configLoaded || a_amount == 0u) {
				return 0u;
			}

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return 0u;
			}

			const std::string editorId(a_editorId);
			auto* item = PluginEditorIds::Lookup<RE::TESObjectMISC>(editorId);
			if (!item) {
				SKSE::log::error("CalamityAffixes: crafting currency item missing (editorId={}).", editorId);
				return 0u;
			}

			const auto maxGive = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
			const auto give = (a_amount > maxGive) ? maxGive : a_amount;
			player->AddObjectToContainer(item, nullptr, static_cast<std::int32_t>(give), nullptr);

			const auto owned = std::max(0, player->GetItemCount(item));
			std::string note(a_label);
			note.append(" +");
			note.append(std::to_string(give));
			note.append(" (");
			note.append(std::to_string(owned));
			note.push_back(')');
			EmitHudNotification(note.c_str());
			return give;
		}

		std::uint64_t EventBridge::PickRunewordFragmentToken()
		{
			if (_runewordState.runeTokenPool.empty()) {
				return 0u;
			}

			std::uint64_t runeToken = 0u;
			if (!_runewordState.runeTokenWeights.empty() &&
				_runewordState.runeTokenWeights.size() == _runewordState.runeTokenPool.size()) {
				std::discrete_distribution<std::size_t> pick(
					_runewordState.runeTokenWeights.begin(),
					_runewordState.runeTokenWeights.end());
				std::lock_guard<std::mutex> rngLock(_rngMutex);
				runeToken = _runewordState.runeTokenPool[pick(_rng)];
			} else {
				std::uniform_int_distribution<std::size_t> pick(0, _runewordState.runeTokenPool.size() - 1u);
				std::lock_guard<std::mutex> rngLock(_rngMutex);
				runeToken = _runewordState.runeTokenPool[pick(_rng)];
			}

			return runeToken;
		}

		bool EventBridge::TryRollRunewordFragmentToken(
			float a_sourceChanceMultiplier,
			std::uint64_t& a_outRuneToken,
			bool& a_outPityTriggered)
		{
			a_outRuneToken = 0u;
			a_outPityTriggered = false;
			if (_runewordState.runeTokenPool.empty()) {
				return false;
			}

			const float chance = std::clamp(_loot.runewordFragmentChancePercent, 0.0f, 100.0f);
			if (chance <= 0.0f) {
				return false;
			}

			const float sourceChanceMultiplier = std::max(0.0f, a_sourceChanceMultiplier);
			const float effectiveChance = std::clamp(chance * sourceChanceMultiplier, 0.0f, 100.0f);
			if (effectiveChance <= 0.0f) {
				// Source has zero multiplier — skip without touching fail streak.
				// Intentional: the player wasn't "unlucky", this source simply doesn't drop fragments.
				return false;
			}

			a_outPityTriggered = (_lootState.runewordFragmentFailStreak >= kRunewordFragmentPityFailThreshold);
			bool grant = a_outPityTriggered;
			if (!grant) {
				std::uniform_real_distribution<float> chanceDist(0.0f, 100.0f);
				{
					std::lock_guard<std::mutex> rngLock(_rngMutex);
					grant = (chanceDist(_rng) < effectiveChance);
				}
			}

			if (!grant) {
				if (_lootState.runewordFragmentFailStreak < std::numeric_limits<std::uint32_t>::max()) {
					++_lootState.runewordFragmentFailStreak;
				}
				return false;
			}

			a_outRuneToken = PickRunewordFragmentToken();
			return a_outRuneToken != 0u;
		}

		void EventBridge::CommitRunewordFragmentGrant(bool a_success)
		{
			if (a_success) {
				_lootState.runewordFragmentFailStreak = 0;
			}
		}

		bool EventBridge::TryRollReforgeOrbGrant(
			float a_sourceChanceMultiplier,
			bool& a_outPityTriggered)
		{
			a_outPityTriggered = false;
			const float chance = std::clamp(_loot.reforgeOrbChancePercent, 0.0f, 100.0f);
			if (chance <= 0.0f) {
				return false;
			}

			const float sourceChanceMultiplier = std::max(0.0f, a_sourceChanceMultiplier);
			const float effectiveChance = std::clamp(chance * sourceChanceMultiplier, 0.0f, 100.0f);
			if (effectiveChance <= 0.0f) {
				// Source has zero multiplier — skip without touching fail streak.
				return false;
			}

			a_outPityTriggered = (_lootState.reforgeOrbFailStreak >= kReforgeOrbPityFailThreshold);
			bool grant = a_outPityTriggered;
			if (!grant) {
				std::uniform_real_distribution<float> chanceDist(0.0f, 100.0f);
				{
					std::lock_guard<std::mutex> rngLock(_rngMutex);
					grant = (chanceDist(_rng) < effectiveChance);
				}
			}

			if (!grant) {
				if (_lootState.reforgeOrbFailStreak < std::numeric_limits<std::uint32_t>::max()) {
					++_lootState.reforgeOrbFailStreak;
				}
				return false;
			}

			return true;
		}

		void EventBridge::CommitReforgeOrbGrant(bool a_success)
		{
			if (a_success) {
				_lootState.reforgeOrbFailStreak = 0;
			}
		}

		bool EventBridge::TryAddLootCurrencyToCorpseInventory(
			RE::TESBoundObject* a_dropItem,
			RE::Actor* a_corpse) const
		{
			static_assert(!RuntimePolicy::kAllowWorldCurrencyPlacement);

			if (!a_dropItem || !a_corpse || !a_corpse->IsDead()) {
				return false;
			}

			const auto countInCorpse = [a_corpse, a_dropItem]() {
				auto inventory = a_corpse->GetInventory();
				const auto it = inventory.find(a_dropItem);
				return it != inventory.end() ? std::max(0, it->second.first) : 0;
			};

			const auto before = countInCorpse();
			a_corpse->AddObjectToContainer(a_dropItem, nullptr, 1, nullptr);
			const auto after = countInCorpse();
			return after > before;
		}

		void EventBridge::LogRunewordStatus() const
		{
			std::string note = "Runeword Status: ";
			if (const auto* recipe = GetCurrentRunewordRecipe()) {
				note.append(recipe->HudName());
			} else {
				note.append("No recipe");
			}

			if (_runewordState.selectedBaseKey) {
				note.append(" | Base selected");
				const auto stateIt = _runewordState.instanceStates.find(*_runewordState.selectedBaseKey);
				if (stateIt != _runewordState.instanceStates.end()) {
					const auto* recipe = FindRunewordRecipeByToken(stateIt->second.recipeToken);
					if (recipe) {
						note.append(" | ");
						note.append(std::to_string(stateIt->second.insertedRunes));
						note.push_back('/');
						note.append(std::to_string(recipe->runeTokens.size()));
					}
				}
			}

				if (const auto* recipe = GetCurrentRunewordRecipe()) {
					note.append(" | Runes:");
					auto* player = RE::PlayerCharacter::GetSingleton();
					for (const auto token : recipe->runeTokens) {
						const auto runeName = ResolveRuneDisplayName(token);
						const auto owned = GetOwnedRunewordFragmentCount(player, _runewordState.runeNameByToken, token);
						note.push_back(' ');
						note.append(runeName);
						note.push_back('=');
						note.append(std::to_string(owned));
					}
				}

			EmitHudNotification(note.c_str());
			SKSE::log::info("CalamityAffixes: {}", note);
		}

}
