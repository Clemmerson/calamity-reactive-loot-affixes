#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/RunewordContractSnapshot.h"
#include "CalamityAffixes/RunewordContractCompatibilityPolicy.h"
#include "CalamityAffixes/RunewordRuneWeights.h"
#include "CalamityAffixes/RuntimeContract.h"
#include "EventBridge.Loot.Runeword.Detail.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace CalamityAffixes
{
	using namespace RunewordDetail;

	namespace
	{
		struct RunewordNameKoRow
		{
			std::string_view id{};
			std::string_view ko{};
		};

		constexpr std::array<RunewordNameKoRow, 95> kRunewordNameKoRows{ {
			{ "rw_nadir", "나디르" },
			{ "rw_steel", "스틸" },
			{ "rw_malice", "맬리스" },
			{ "rw_stealth", "스텔스" },
			{ "rw_leaf", "리프" },
			{ "rw_ancients_pledge", "고대인의 서약" },
			{ "rw_holy_thunder", "홀리 썬더" },
			{ "rw_zephyr", "제피르" },
			{ "rw_pattern", "패턴" },
			{ "rw_kings_grace", "왕의 은총" },
			{ "rw_strength", "스트렝스" },
			{ "rw_edge", "엣지" },
			{ "rw_myth", "미스" },
			{ "rw_spirit", "스피릿" },
			{ "rw_honor", "아너" },
			{ "rw_lore", "로어" },
			{ "rw_radiance", "래디언스" },
			{ "rw_insight", "통찰" },
			{ "rw_rhyme", "라임" },
			{ "rw_peace", "피스" },
			{ "rw_black", "블랙" },
			{ "rw_bulwark", "벌워크" },
			{ "rw_cure", "큐어" },
			{ "rw_ground", "그라운드" },
			{ "rw_hearth", "하스" },
			{ "rw_temper", "템퍼" },
			{ "rw_white", "화이트" },
			{ "rw_memory", "메모리" },
			{ "rw_smoke", "스모크" },
			{ "rw_splendor", "스플렌더" },
			{ "rw_melody", "멜로디" },
			{ "rw_harmony", "하모니" },
			{ "rw_hustle_w", "허슬(무기)" },
			{ "rw_hustle_a", "허슬(방어구)" },
			{ "rw_lionheart", "라이언하트" },
			{ "rw_obedience", "오베디언스" },
			{ "rw_unbending_will", "언벤딩 윌" },
			{ "rw_wealth", "웰스" },
			{ "rw_passion", "패션" },
			{ "rw_lawbringer", "로브링어" },
			{ "rw_voice_of_reason", "보이스 오브 리즌" },
			{ "rw_treachery", "트래셔리" },
			{ "rw_enlightenment", "인라이트먼트" },
			{ "rw_wisdom", "위즈덤" },
			{ "rw_crescent_moon", "크레센트 문" },
			{ "rw_duress", "듀레스" },
			{ "rw_gloom", "글룸" },
			{ "rw_stone", "스톤" },
			{ "rw_bone", "본" },
			{ "rw_venom", "베넘" },
			{ "rw_prudence", "프루던스" },
			{ "rw_sanctuary", "생츄어리" },
			{ "rw_oath", "오쓰" },
			{ "rw_rain", "레인" },
			{ "rw_call_to_arms", "콜 투 암스" },
			{ "rw_delirium", "딜리리엄" },
			{ "rw_kingslayer", "킹슬레이어" },
			{ "rw_rift", "리프트" },
			{ "rw_principle", "프린서플" },
			{ "rw_mosaic", "모자이크" },
			{ "rw_silence", "사일런스" },
			{ "rw_heart_of_the_oak", "참나무의 심장" },
			{ "rw_death", "데스" },
			{ "rw_flickering_flame", "플리커링 플레임" },
			{ "rw_chaos", "카오스" },
			{ "rw_exile", "엑자일" },
			{ "rw_fortitude", "인내" },
			{ "rw_grief", "슬픔" },
			{ "rw_bramble", "브램블" },
			{ "rw_wind", "윈드" },
			{ "rw_dragon", "드래곤" },
			{ "rw_beast", "비스트" },
			{ "rw_chains_of_honor", "명예의 사슬" },
			{ "rw_eternity", "이터니티" },
			{ "rw_infinity", "무한" },
			{ "rw_wrath", "래쓰" },
			{ "rw_fury", "퓨리" },
			{ "rw_enigma", "수수께끼" },
			{ "rw_famine", "패민" },
			{ "rw_brand", "브랜드" },
			{ "rw_destruction", "파괴" },
			{ "rw_dream", "드림" },
			{ "rw_faith", "신념" },
			{ "rw_ice", "아이스" },
			{ "rw_last_wish", "라스트 위시" },
			{ "rw_phoenix", "피닉스" },
			{ "rw_doom", "둠" },
			{ "rw_hand_of_justice", "정의의 손길" },
			{ "rw_pride", "프라이드" },
			{ "rw_plague", "플레이그" },
			{ "rw_metamorphosis", "메타모포시스" },
			{ "rw_obsession", "집착" },
			{ "rw_mist", "미스트" },
			{ "rw_breath_of_the_dying", "죽음의 숨결" },
			{ "rw_shadow_boxer", "쉐도우 복서" },
		} };

		[[nodiscard]] std::string_view ResolveRunewordDisplayNameKo(
			std::string_view a_recipeId,
			std::string_view a_fallbackEn) noexcept
		{
			for (const auto& row : kRunewordNameKoRows) {
				if (row.id == a_recipeId) {
					return row.ko;
				}
			}
			return a_fallbackEn;
		}
	}

		bool EventBridge::ResolvePlayerInventoryInstance(
			std::uint64_t a_instanceKey,
			RE::InventoryEntryData*& a_outEntry,
			RE::ExtraDataList*& a_outXList) const
		{
			a_outEntry = nullptr;
			a_outXList = nullptr;
			if (a_instanceKey == 0u) {
				return false;
			}

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return false;
			}

			auto* changes = player->GetInventoryChanges();
			if (!changes || !changes->entryList) {
				return false;
			}

			RE::InventoryEntryData* foundEntry = nullptr;
			RE::ExtraDataList* foundXList = nullptr;
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

					if (MakeInstanceKey(uid->baseID, uid->uniqueID) != a_instanceKey) {
						continue;
					}
					// An instance key is safe only when it identifies one individual copy.
					// Reject stacks and duplicate keys instead of silently choosing one.
					if (xList->GetCount() != 1 || foundXList) {
						return false;
					}
					foundEntry = entry;
					foundXList = xList;
				}
			}

			if (!foundEntry || !foundXList) {
				return false;
			}
			a_outEntry = foundEntry;
			a_outXList = foundXList;
			return true;
		}

		std::optional<EventBridge::LootItemType> EventBridge::ResolveInstanceLootType(std::uint64_t a_instanceKey) const
		{
			RE::InventoryEntryData* entry = nullptr;
			RE::ExtraDataList* xList = nullptr;
			if (!ResolvePlayerInventoryInstance(a_instanceKey, entry, xList) || !entry || !entry->object) {
				return std::nullopt;
			}

			if (entry->object->As<RE::TESObjectWEAP>()) {
				return LootItemType::kWeapon;
			}
			if (entry->object->As<RE::TESObjectARMO>()) {
				return LootItemType::kArmor;
			}

			return std::nullopt;
		}

		const char* EventBridge::DescribeLootItemType(LootItemType a_type)
		{
			switch (a_type) {
			case LootItemType::kWeapon:
				return "Weapon";
			case LootItemType::kArmor:
				return "Armor";
			}

			return "Unknown";
		}

		const EventBridge::RunewordRecipe* EventBridge::FindRunewordRecipeByToken(std::uint64_t a_recipeToken) const
		{
			const auto it = _runewordState.recipeIndexByToken.find(a_recipeToken);
			if (it == _runewordState.recipeIndexByToken.end() || it->second >= _runewordState.recipes.size()) {
				return nullptr;
			}

			return std::addressof(_runewordState.recipes[it->second]);
		}

		const EventBridge::RunewordRecipe* EventBridge::GetCurrentRunewordRecipe() const
		{
			if (_runewordState.recipes.empty()) {
				return nullptr;
			}

		const std::size_t count = _runewordState.recipes.size();
		const auto start = static_cast<std::size_t>(_runewordState.recipeCycleCursor % count);
		for (std::size_t offset = 0; offset < count; ++offset) {
			const auto idx = (start + offset) % count;
			const auto& recipe = _runewordState.recipes[idx];
			if (!HasRunewordRuntimeEffect(recipe)) {
				continue;
			}
			return std::addressof(recipe);
			}
			return nullptr;
		}

		void EventBridge::InitializeRunewordCatalog()
		{
			_runewordState.ResetCatalog();
			_feasrRuneforgeContractCompatible = false;

			auto addRecipe = [&](std::string_view a_recipeId,
				std::string_view a_displayName,
				const std::vector<std::string_view>& a_runes,
				std::string_view a_resultAffixId,
				std::optional<LootItemType> a_recommendedBaseType) {
				RunewordRecipe recipe{};
				recipe.id = std::string(a_recipeId);
				recipe.token = MakeAffixToken(recipe.id);
				recipe.displayNameEn = std::string(a_displayName);
				recipe.displayNameKo = std::string(ResolveRunewordDisplayNameKo(a_recipeId, a_displayName));
				recipe.displayName = recipe.displayNameKo;
				recipe.resultAffixToken = MakeAffixToken(a_resultAffixId);
				recipe.recommendedBaseType = a_recommendedBaseType;

				for (const auto rune : a_runes) {
					if (rune.empty()) {
						continue;
					}

					recipe.runeIds.emplace_back(rune);
					const auto runeToken = MakeAffixToken(ToLowerAscii(rune));
					recipe.runeTokens.push_back(runeToken);

					_runewordState.runeNameByToken.try_emplace(runeToken, std::string(rune));
					if (std::find(_runewordState.runeTokenPool.begin(), _runewordState.runeTokenPool.end(), runeToken) == _runewordState.runeTokenPool.end()) {
						_runewordState.runeTokenPool.push_back(runeToken);
					}
				}

				if (recipe.token == 0u || recipe.resultAffixToken == 0u || recipe.runeTokens.empty()) {
					return;
				}

				const auto idx = _runewordState.recipes.size();
				_runewordState.recipeIndexByToken.emplace(recipe.token, idx);
				if (const auto [it, inserted] = _runewordState.recipeIndexByResultAffixToken.emplace(recipe.resultAffixToken, idx); !inserted) {
					SKSE::log::warn(
						"CalamityAffixes: duplicate resultAffixToken {:016X} — recipe #{} collides with recipe #{}.",
						recipe.resultAffixToken, idx, it->second);
				}
				_runewordState.recipes.push_back(std::move(recipe));
			};

			const auto parseRecommendedBaseType = [](const std::optional<bool>& a_recommendedBaseIsWeapon) {
				if (!a_recommendedBaseIsWeapon.has_value()) {
					return std::optional<LootItemType>{};
				}
				return *a_recommendedBaseIsWeapon ?
					std::optional<LootItemType>{ LootItemType::kWeapon } :
					std::optional<LootItemType>{ LootItemType::kArmor };
			};

			struct RunewordCatalogRow
			{
				std::string_view recipeId{};
				std::string_view displayName{};
				std::string_view runeCsv{};
				std::string_view resultAffixId{};
				std::optional<LootItemType> recommendedBaseType{};
			};
			constexpr std::array<RunewordCatalogRow, 95> kRunewordCatalogRows{ {
#include "RunewordCatalogRows.inl"
			} };

			RunewordContractSnapshot expectedContract{};
			expectedContract.recipes.reserve(kRunewordCatalogRows.size());
			for (const auto& [runeName, weight] : kPinnedRunewordRuneWeights) {
				expectedContract.runeWeightsByName.emplace(std::string(runeName), weight);
			}
			for (const auto& row : kRunewordCatalogRows) {
				RunewordContractRecipeRow expected{};
				expected.id = row.recipeId;
				expected.displayName = row.displayName;
				expected.resultAffixId = row.resultAffixId;
				if (row.recommendedBaseType) {
					expected.recommendedBaseIsWeapon = *row.recommendedBaseType == LootItemType::kWeapon;
				}
				auto runeCsv = row.runeCsv;
				while (!runeCsv.empty()) {
					const auto separator = runeCsv.find(',');
					const auto rune = separator == std::string_view::npos ? runeCsv : runeCsv.substr(0, separator);
					if (rune.empty()) break;
					expected.runeNames.emplace_back(rune);
					if (separator == std::string_view::npos) break;
					runeCsv.remove_prefix(separator + 1u);
				}
				expectedContract.recipes.push_back(std::move(expected));
			}

			auto loadCompiledFallbackCatalog = [&]() {
				auto splitRunes = [](std::string_view a_csv) {
					std::vector<std::string_view> runes;
					while (!a_csv.empty()) {
						const auto pos = a_csv.find(',');
						const auto token = (pos == std::string_view::npos) ? a_csv : a_csv.substr(0, pos);
						if (!token.empty()) {
							runes.push_back(token);
						}
						if (pos == std::string_view::npos) {
							break;
						}
						a_csv.remove_prefix(pos + 1);
					}
					return runes;
				};

				// Extended D2/D2R runeword catalog (94 recipes) plus Calamity originals (Shadow Boxer).
				for (const auto& row : kRunewordCatalogRows) {
					const auto runes = splitRunes(row.runeCsv);
					if (runes.empty()) {
						continue;
					}
					addRecipe(
						row.recipeId,
						row.displayName,
						runes,
						row.resultAffixId,
						row.recommendedBaseType);
				}
			};

			RunewordContractSnapshot contractSnapshot{};
			bool loadedFromContract = LoadRunewordContractSnapshotFromRuntime(
				RuntimeContract::kRuntimeContractRelativePath,
				contractSnapshot);
			_feasrRuneforgeContractCompatible = loadedFromContract &&
				RunewordContractCompatibilityPolicy::MatchesPinnedContract(contractSnapshot, expectedContract);
			if (!_feasrRuneforgeContractCompatible) {
				_feasrDiscoveryReady = false;
				_feasrKnownRunewordRecipeIds.clear();
				SKSE::log::warn("Runeforge: dependency runtime contract does not match the pinned 2.2.8 contract; FEASR Runeforge is disabled.");
			}

			if (loadedFromContract) {
				for (const auto& row : contractSnapshot.recipes) {
					std::vector<std::string_view> runeViews;
					runeViews.reserve(row.runeNames.size());
					for (const auto& runeName : row.runeNames) {
						runeViews.push_back(runeName);
					}

					addRecipe(
						row.id,
						row.displayName,
						runeViews,
						row.resultAffixId,
						parseRecommendedBaseType(row.recommendedBaseIsWeapon));
				}

				if (_runewordState.recipes.empty()) {
					SKSE::log::warn(
						"CalamityAffixes: runtime contract snapshot produced zero usable runeword recipes; falling back to compiled catalog.");
					loadCompiledFallbackCatalog();
					loadedFromContract = false;
				} else {
					SKSE::log::info(
						"CalamityAffixes: loaded {} runeword recipes from runtime contract snapshot.",
						_runewordState.recipes.size());
				}
			} else {
				loadCompiledFallbackCatalog();
			}

			if (!_runewordState.recipes.empty()) {
				if (!loadedFromContract) {
					SKSE::log::info(
						"CalamityAffixes: loaded {} runeword recipes from compiled fallback catalog.",
						_runewordState.recipes.size());
				}
			} else {
				SKSE::log::info(
					"CalamityAffixes: runeword catalog is empty after contract/fallback load.");
			}

				if (_runewordState.recipeCycleCursor >= _runewordState.recipes.size()) {
					_runewordState.recipeCycleCursor = 0;
				}

				_runewordState.runeTokenWeights.reserve(_runewordState.runeTokenPool.size());
				for (const auto token : _runewordState.runeTokenPool) {
					double weight = 1.0;
					if (const auto nameIt = _runewordState.runeNameByToken.find(token); nameIt != _runewordState.runeNameByToken.end()) {
						if (loadedFromContract) {
							if (const auto weightIt = contractSnapshot.runeWeightsByName.find(nameIt->second);
								weightIt != contractSnapshot.runeWeightsByName.end() && weightIt->second > 0.0) {
								weight = weightIt->second;
							} else {
								weight = ResolveRunewordFragmentWeight(nameIt->second);
							}
						} else {
							weight = ResolveRunewordFragmentWeight(nameIt->second);
						}
					}
					_runewordState.runeTokenWeights.push_back(weight);
				}
			}

			void EventBridge::SanitizeRunewordState()
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				const auto canValidateInventory = (player && player->GetInventoryChanges() && player->GetInventoryChanges()->entryList);
				if (canValidateInventory) {
					// Legacy -> physical item migration: older saves stored fragments in SKSE serialization.
					// Convert those counters into real inventory items once, then clear the legacy map.
					if (!_runewordState.runeFragments.empty()) {
						if (_runewordState.runeNameByToken.empty()) {
							InitializeRunewordCatalog();
						}

						std::uint32_t migrated = 0u;
						for (auto it = _runewordState.runeFragments.begin(); it != _runewordState.runeFragments.end();) {
							const auto runeToken = it->first;
							const auto amount = it->second;
							if (runeToken == 0u || amount == 0u) {
								it = _runewordState.runeFragments.erase(it);
								continue;
							}

							const auto given = GrantRunewordFragments(player, _runewordState.runeNameByToken, runeToken, amount);
							if (given > 0u) {
								const auto maxVal = std::numeric_limits<std::uint32_t>::max();
								migrated = (migrated > maxVal - given) ? maxVal : (migrated + given);
								it = _runewordState.runeFragments.erase(it);
							} else {
								++it;
							}
						}

						if (migrated > 0u) {
							EmitHudNotification("Runeword: migrated legacy rune fragments to inventory.");
							SKSE::log::info(
								"CalamityAffixes: migrated {} legacy runeword fragments to inventory items.",
								migrated);
						}
					}

					for (auto it = _runewordState.instanceStates.begin(); it != _runewordState.instanceStates.end();) {
						if (!PlayerHasInstanceKey(it->first)) {
							it = _runewordState.instanceStates.erase(it);
						} else {
							++it;
					}
				}

					if (_runewordState.selectedBaseKey) {
						RE::InventoryEntryData* selectedEntry = nullptr;
						RE::ExtraDataList* selectedXList = nullptr;
						const auto selectedKey = *_runewordState.selectedBaseKey;
						const bool selectedAvailable = ResolvePlayerInventoryInstance(selectedKey, selectedEntry, selectedXList) &&
							selectedEntry && selectedEntry->object;
						const bool selectedCompleted = ResolveCompletedRunewordRecipe(selectedKey) != nullptr;
						if (!selectedAvailable ||
							(!selectedCompleted && !IsLootObjectEligibleForAffixes(selectedEntry->object))) {
							_runewordState.selectedBaseKey.reset();
						}
					}
				}

			if (_runewordState.recipes.empty()) {
				_runewordState.recipeCycleCursor = 0;
				return;
			}

			if (_runewordState.recipeCycleCursor >= _runewordState.recipes.size()) {
				_runewordState.recipeCycleCursor = 0;
			}

			const auto* currentRecipe = GetCurrentRunewordRecipe();
			for (auto& [instanceKey, state] : _runewordState.instanceStates) {
				auto* recipe = FindRunewordRecipeByToken(state.recipeToken);
				if (!recipe) {
					if (currentRecipe) {
						state.recipeToken = currentRecipe->token;
						recipe = currentRecipe;
					}
				}

				if (recipe && state.insertedRunes > recipe->runeTokens.size()) {
					state.insertedRunes = static_cast<std::uint32_t>(recipe->runeTokens.size());
				}
			}
		}

		void EventBridge::CycleRunewordBase(std::int32_t a_direction)
		{
			if (!_configLoaded || a_direction == 0) {
				return;
			}

			SanitizeRunewordState();
			auto candidates = CollectEquippedRunewordBaseCandidates();
			if (candidates.empty()) {
				EmitHudNotification("Runeword: no equipped weapon/armor base found.");
				return;
			}

			std::size_t index = static_cast<std::size_t>(_runewordState.baseCycleCursor % candidates.size());
			if (_runewordState.selectedBaseKey) {
				const auto selectedIt = std::find(candidates.begin(), candidates.end(), *_runewordState.selectedBaseKey);
				if (selectedIt != candidates.end()) {
					index = static_cast<std::size_t>(std::distance(candidates.begin(), selectedIt));
				}
			}

			if (a_direction > 0) {
				index = (index + 1u) % candidates.size();
			} else {
				index = (index + candidates.size() - 1u) % candidates.size();
			}

			const auto selectedKey = candidates[index];
			_runewordState.baseCycleCursor = static_cast<std::uint32_t>(index);
			(void)SelectRunewordBase(selectedKey);
		}

		void EventBridge::CycleRunewordRecipe(std::int32_t a_direction)
		{
			if (!_configLoaded || !_runtimeSettings.enabled.load(std::memory_order_relaxed) ||
				!_feasrDiscoveryReady || a_direction == 0 || _runewordState.recipes.empty()) {
				return;
			}

			SanitizeRunewordState();
			const std::size_t count = _runewordState.recipes.size();
			auto cursor = static_cast<std::size_t>(_runewordState.recipeCycleCursor % count);
			const auto isEligible = [&](std::size_t a_idx) {
				const auto& recipe = _runewordState.recipes[a_idx];
				return HasRunewordRuntimeEffect(recipe) && _feasrKnownRunewordRecipeIds.contains(recipe.id);
			};

			for (std::size_t step = 0; step < count; ++step) {
				if (a_direction > 0) {
					cursor = (cursor + 1u) % count;
				} else {
					cursor = (cursor + count - 1u) % count;
				}
				if (!isEligible(cursor)) {
					continue;
				}
				_runewordState.recipeCycleCursor = static_cast<std::uint32_t>(cursor);
				break;
			}

			const auto* recipe = GetCurrentRunewordRecipe();
			if (!recipe) {
				return;
			}
			(void)SelectRunewordRecipe(recipe->token);
		}
}
