#pragma once

#include "CalamityAffixes/RunewordContractSnapshot.h"

#include <unordered_set>
#include <utility>

namespace CalamityAffixes::RunewordContractCompatibilityPolicy
{
	[[nodiscard]] inline bool AddUniqueRuneWeight(
		RunewordContractSnapshot& a_snapshot,
		std::string a_rune,
		double a_weight)
	{
		return a_snapshot.runeWeightsByName.emplace(std::move(a_rune), a_weight).second;
	}

	[[nodiscard]] inline bool MatchesPinnedContract(
		const RunewordContractSnapshot& a_actual,
		const RunewordContractSnapshot& a_expected)
	{
		if (a_actual.recipes.empty() || a_actual.recipes.size() != a_expected.recipes.size() ||
			a_actual.runeWeightsByName.size() != a_expected.runeWeightsByName.size()) {
			return false;
		}

		std::unordered_set<std::string> expectedIds;
		std::unordered_set<std::string> actualIds;
		for (std::size_t i = 0; i < a_expected.recipes.size(); ++i) {
			const auto& expected = a_expected.recipes[i];
			const auto& actual = a_actual.recipes[i];
			if (expected.id.empty() || actual.id.empty() ||
				!expectedIds.emplace(expected.id).second || !actualIds.emplace(actual.id).second ||
				actual.id != expected.id || actual.displayName != expected.displayName ||
				actual.runeNames != expected.runeNames || actual.resultAffixId != expected.resultAffixId ||
				actual.recommendedBaseIsWeapon != expected.recommendedBaseIsWeapon) {
				return false;
			}
		}

		for (const auto& [rune, expectedWeight] : a_expected.runeWeightsByName) {
			const auto actual = a_actual.runeWeightsByName.find(rune);
			if (actual == a_actual.runeWeightsByName.end() || actual->second != expectedWeight) {
				return false;
			}
		}
		return true;
	}
}
