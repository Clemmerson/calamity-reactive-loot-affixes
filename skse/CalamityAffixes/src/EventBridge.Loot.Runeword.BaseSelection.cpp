#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/LootUiGuards.h"
#include "CalamityAffixes/RuneforgeInventorySnapshotPolicy.h"
#include "EventBridge.Loot.Runeword.Detail.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace CalamityAffixes
{
	using namespace RunewordDetail;
	constexpr std::uint32_t kSkyrimUnarmedWeaponFormID = 0x000001F4u;

	std::vector<std::uint64_t> EventBridge::CollectEquippedRunewordBaseCandidates()
	{
		// Skyrim exposes its virtual unarmed combat weapon as an equipped inventory
		// entry in some saves. It is not a physical item instance the Runeforge can
		// modify, so never present Skyrim.esm's Unarmed form as a base candidate.
		std::vector<std::uint64_t> keys;

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return keys;
		}

		auto* changes = player->GetInventoryChanges();
		if (!changes || !changes->entryList) {
			return keys;
		}

		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->object || !entry->extraLists) {
				continue;
			}
			if (entry->object->GetFormID() == kSkyrimUnarmedWeaponFormID) {
				continue;
			}

			if (!IsWeaponOrArmor(entry->object)) {
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
				if (!uid || uid->baseID == 0u || uid->uniqueID == 0u) {
					continue;
				}

				const auto key = MakeInstanceKey(uid->baseID, uid->uniqueID);
				if (!IsLootObjectEligibleForAffixes(entry->object) && !ResolveCompletedRunewordRecipe(key)) {
					continue;
				}
				keys.push_back(key);
			}
		}

		std::sort(keys.begin(), keys.end());
		keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
		return keys;
	}

	const EventBridge::RunewordRecipe* EventBridge::ResolveCompletedRunewordRecipe(const InstanceAffixSlots& a_slots) const
	{
		const auto cappedCount = std::min<std::uint8_t>(a_slots.count, static_cast<std::uint8_t>(kMaxAffixesPerItem));
		for (std::uint8_t i = 0; i < cappedCount; ++i) {
			const auto token = a_slots.tokens[i];
			if (token == 0u) {
				continue;
			}

			const auto rwIt = _runewordState.recipeIndexByResultAffixToken.find(token);
			if (rwIt == _runewordState.recipeIndexByResultAffixToken.end() || rwIt->second >= _runewordState.recipes.size()) {
				continue;
			}

			const auto& recipe = _runewordState.recipes[rwIt->second];
			if (!HasRunewordRuntimeEffect(recipe)) {
				continue;
			}

			return std::addressof(recipe);
		}

		return nullptr;
	}

	const EventBridge::RunewordRecipe* EventBridge::ResolveCompletedRunewordRecipe(std::uint64_t a_instanceKey) const
	{
		const auto it = _instanceTrackingState.instanceAffixes.find(a_instanceKey);
		if (it == _instanceTrackingState.instanceAffixes.end()) {
			return nullptr;
		}

		return ResolveCompletedRunewordRecipe(it->second);
	}

	bool EventBridge::SelectRunewordBase(std::uint64_t a_instanceKey, bool a_allowUndiscoveredRecipe)
	{
		const std::scoped_lock lock(_stateMutex);
		if (!_configLoaded || a_instanceKey == 0u ||
			_runewordState.transmuteInProgress || _runewordState.affixExpansionInProgress) {
			return false;
		}

		SanitizeRunewordState();

		RE::InventoryEntryData* entry = nullptr;
		RE::ExtraDataList* xList = nullptr;
		if (!ResolvePlayerInventoryInstance(a_instanceKey, entry, xList) || !entry || !entry->object) {
			return false;
		}

		const RunewordRecipe* completedRecipe = ResolveCompletedRunewordRecipe(a_instanceKey);
		if (!IsLootObjectEligibleForAffixes(entry->object) && !completedRecipe) {
			return false;
		}

		_runewordState.selectedBaseKey = a_instanceKey;

		const auto candidates = CollectEquippedRunewordBaseCandidates();
		if (const auto cursor = ResolveRunewordBaseCycleCursor(candidates, a_instanceKey); cursor) {
			_runewordState.baseCycleCursor = *cursor;
		}

		if (completedRecipe) {
			_runewordState.instanceStates.erase(a_instanceKey);
		} else {
			auto& state = _runewordState.instanceStates[a_instanceKey];
			if (!a_allowUndiscoveredRecipe && state.recipeToken != 0u) {
				const auto* assignedRecipe = FindRunewordRecipeByToken(state.recipeToken);
				if (!assignedRecipe || !_feasrKnownRunewordRecipeIds.contains(assignedRecipe->id)) {
					state.recipeToken = 0u;
					state.insertedRunes = 0u;
				}
			}
			if (a_allowUndiscoveredRecipe && state.recipeToken == 0u) {
				if (const auto* recipe = GetCurrentRunewordRecipe()) {
					state.recipeToken = recipe->token;
				}
			}
		}

		const std::string selectedName = ResolveInventoryDisplayName(entry, xList);
		const auto* recipe = ResolveSelectedRunewordRecipe(a_instanceKey, completedRecipe, a_allowUndiscoveredRecipe);
		if (!a_allowUndiscoveredRecipe && recipe && !completedRecipe &&
			!_feasrKnownRunewordRecipeIds.contains(recipe->id)) {
			recipe = nullptr;
		}
		const auto baseType = ResolveInstanceLootType(a_instanceKey);

		std::string note = "Runeword Base: " + selectedName;
		if (recipe) {
			note.append(" | ");
			note.append(recipe->HudName());
			AppendRunewordSelectionRecommendation(note, *recipe, baseType);
		}

		EmitHudNotification(note.c_str());
		return true;
	}

	std::vector<EventBridge::RunewordBaseInventoryEntry> EventBridge::GetRunewordBaseInventoryEntries()
	{
		const std::scoped_lock lock(_stateMutex);
		std::vector<RunewordBaseInventoryEntry> entries;
		if (!_configLoaded) {
			return entries;
		}

		SanitizeRunewordState();
		const auto candidates = CollectEquippedRunewordBaseCandidates();
		entries.reserve(candidates.size());

		for (const auto key : candidates) {
			RE::InventoryEntryData* entry = nullptr;
			RE::ExtraDataList* xList = nullptr;
			if (!ResolvePlayerInventoryInstance(key, entry, xList) || !entry || !entry->object) {
				continue;
			}

			std::string baseName = ResolveInventoryDisplayName(entry, xList);
			std::string displayName = baseName;
			std::string displayNameEn = baseName;
			std::string runewordNameEn;
			std::string runewordNameKo;
			std::string runewordRecipeId;
			if (const auto* completed = ResolveCompletedRunewordRecipe(key)) {
				displayName.append(" [Runeword: ");
				displayName.append(completed->displayName);
				displayName.push_back(']');
				runewordNameEn = completed->PresentationNameEn();
				runewordNameKo = completed->displayNameKo;
				runewordRecipeId = completed->id;

				// FE-ASR presentation: "<item> [Runeword: <public name>]".
				const auto englishName = !runewordNameEn.empty() ?
					                         runewordNameEn :
					                         ("Runeword " + completed->id);
				displayNameEn = RuneforgePresentation::CompletedRunewordItemLabel(baseName, englishName);
			}

			entries.push_back(RunewordBaseInventoryEntry{
				.instanceKey = key,
				.displayName = std::move(displayName),
				.displayNameEn = std::move(displayNameEn),
				.baseName = std::move(baseName),
				.itemKind = entry->object->As<RE::TESObjectWEAP>() ? "weapon" : "armor",
				.runewordNameEn = std::move(runewordNameEn),
				.runewordNameKo = std::move(runewordNameKo),
				.runewordRecipeId = std::move(runewordRecipeId),
				.selected = (_runewordState.selectedBaseKey && *_runewordState.selectedBaseKey == key)
			});
		}

		return entries;
	}

	EventBridge::RuneforgeInventorySnapshot EventBridge::GetRuneforgeInventorySnapshot()
	{
		const std::scoped_lock lock(_stateMutex);
		RuneforgeInventorySnapshot snapshot{};
		if (_runeforgeInventorySnapshotRevision == std::numeric_limits<std::uint64_t>::max()) {
			_runeforgeInventorySnapshotRevision = 0u;
		}
		snapshot.revision = ++_runeforgeInventorySnapshotRevision;
		if (!_configLoaded) {
			return snapshot;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* changes = player ? player->GetInventoryChanges() : nullptr;
		if (!player || !changes || !changes->entryList) {
			return snapshot;
		}
		snapshot.known = true;

		struct Candidate
		{
			std::uint64_t instanceKey{ 0u };
			RE::InventoryEntryData* entry{ nullptr };
			RE::ExtraDataList* xList{ nullptr };
			bool equipped{ false };
			bool favorited{ false };
		};
		std::vector<Candidate> observedCandidates;
		std::vector<RuneforgeInventoryIdentityObservation> identityObservations;

		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->object || !entry->extraLists) {
				continue;
			}
			// The player inventory can expose Skyrim's virtual Unarmed weapon as
			// an equipped ExtraUniqueID instance. It is not a physical item the
			// Runeforge can modify, so exclude it before identity/count analysis.
			if (entry->object->GetFormID() == kSkyrimUnarmedWeaponFormID) {
				continue;
			}
			if (!IsWeaponOrArmor(entry->object)) {
				continue;
			}
			const bool eligibleBase = IsLootObjectEligibleForAffixes(entry->object);

			for (auto* xList : *entry->extraLists) {
				if (!xList) {
					continue;
				}

				auto* uid = xList->GetByType<RE::ExtraUniqueID>();
				if (!uid || uid->baseID == 0u || uid->uniqueID == 0u) {
					if (eligibleBase) {
						++snapshot.skippedMissingUniqueId;
					}
					continue;
				}

				const auto key = MakeInstanceKey(uid->baseID, uid->uniqueID);
				if (key == 0u) {
					if (eligibleBase) {
						++snapshot.skippedMissingUniqueId;
					}
					continue;
				}
				if (!eligibleBase && !ResolveCompletedRunewordRecipe(key)) {
					continue;
				}

				identityObservations.push_back(RuneforgeInventoryIdentityObservation{
					.instanceKey = key,
					.count = xList->GetCount()
				});
				observedCandidates.push_back(Candidate{
					.instanceKey = key,
					.entry = entry,
					.xList = xList,
					.equipped = xList->HasType<RE::ExtraWorn>() || xList->HasType<RE::ExtraWornLeft>(),
					.favorited = xList->HasType<RE::ExtraHotkey>()
				});
			}
		}

		const auto identityScan = AnalyzeRuneforgeInventoryIdentity(identityObservations);
		snapshot.skippedNonSingleton = static_cast<std::uint32_t>(std::min<std::size_t>(
			identityScan.skippedNonSingleton,
			std::numeric_limits<std::uint32_t>::max()));
		snapshot.skippedAmbiguousKeys = static_cast<std::uint32_t>(std::min<std::size_t>(
			identityScan.skippedAmbiguousKeys,
			std::numeric_limits<std::uint32_t>::max()));
		snapshot.entries.reserve(identityScan.eligibleIndices.size());
		for (const auto candidateIndex : identityScan.eligibleIndices) {
			if (candidateIndex >= observedCandidates.size()) {
				continue;
			}
			const auto& candidate = observedCandidates[candidateIndex];
			const auto key = candidate.instanceKey;
			if (!candidate.entry || !candidate.xList || !candidate.entry->object) {
				continue;
			}

			std::string baseName = ResolveInventoryDisplayName(candidate.entry, candidate.xList);
			std::string displayName = baseName;
			std::string displayNameEn = baseName;
			std::string runewordNameEn;
			std::string runewordNameKo;
			std::string runewordRecipeId;
			if (const auto* completed = ResolveCompletedRunewordRecipe(key)) {
				displayName.append(" [Runeword: ");
				displayName.append(completed->displayName);
				displayName.push_back(']');
				runewordNameEn = completed->PresentationNameEn();
				runewordNameKo = completed->displayNameKo;
				runewordRecipeId = completed->id;

				// FE-ASR presentation: "<item> [Runeword: <public name>]".
				const auto englishName = !runewordNameEn.empty() ?
					                         runewordNameEn :
					                         ("Runeword " + completed->id);
				displayNameEn = RuneforgePresentation::CompletedRunewordItemLabel(baseName, englishName);
			}

			snapshot.entries.push_back(RuneforgeInventoryEntry{
				.instanceKey = key,
				.displayName = std::move(displayName),
				.displayNameEn = std::move(displayNameEn),
				.baseName = std::move(baseName),
				.itemKind = candidate.entry->object->As<RE::TESObjectWEAP>() ? "weapon" : "armor",
				.runewordNameEn = std::move(runewordNameEn),
				.runewordNameKo = std::move(runewordNameKo),
				.runewordRecipeId = std::move(runewordRecipeId),
				.selected = (_runewordState.selectedBaseKey && *_runewordState.selectedBaseKey == key),
				.equipped = candidate.equipped,
				.favorited = candidate.favorited
			});
		}
		std::sort(snapshot.entries.begin(), snapshot.entries.end(), [](const auto& a_lhs, const auto& a_rhs) {
			return a_lhs.instanceKey < a_rhs.instanceKey;
		});
		return snapshot;
	}
}
