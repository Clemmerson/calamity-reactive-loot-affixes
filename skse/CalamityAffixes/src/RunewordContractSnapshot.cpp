#include "CalamityAffixes/RunewordContractSnapshot.h"
#include "CalamityAffixes/RunewordContractCompatibilityPolicy.h"
#include "CalamityAffixes/RuntimeContractDocumentPolicy.h"

#include "CalamityAffixes/RuntimeContract.h"
#include "CalamityAffixes/RuntimePaths.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>

#include <nlohmann/json.hpp>
#include <SKSE/SKSE.h>

namespace CalamityAffixes
{
	namespace
	{
		[[nodiscard]] bool TryReadRequiredString(
			const nlohmann::json& a_object,
			std::string_view a_key,
			std::string& a_outValue)
		{
			const auto it = a_object.find(std::string(a_key));
			if (it == a_object.end() || !it->is_string()) {
				return false;
			}

			a_outValue = it->get<std::string>();
			return !a_outValue.empty();
		}

		[[nodiscard]] bool ParseRunewordRecipeRow(
			const nlohmann::json& a_entry,
			RunewordContractRecipeRow& a_outRow)
		{
			if (!a_entry.is_object()) {
				return false;
			}

			if (!TryReadRequiredString(a_entry, RuntimeContract::kFieldRunewordId, a_outRow.id) ||
			    !TryReadRequiredString(a_entry, RuntimeContract::kFieldRunewordName, a_outRow.displayName) ||
			    !TryReadRequiredString(a_entry, RuntimeContract::kFieldRunewordResultAffixId, a_outRow.resultAffixId)) {
				return false;
			}

			const auto runesIt = a_entry.find(std::string(RuntimeContract::kFieldRunewordRunes));
			if (runesIt == a_entry.end() || !runesIt->is_array()) {
				return false;
			}

			a_outRow.runeNames.clear();
			a_outRow.runeNames.reserve(runesIt->size());
			for (const auto& rune : *runesIt) {
				if (!rune.is_string()) {
					return false;
				}

				const auto runeName = rune.get<std::string>();
				if (runeName.empty()) {
					return false;
				}
				a_outRow.runeNames.push_back(runeName);
			}
			if (a_outRow.runeNames.empty()) {
				return false;
			}

			a_outRow.recommendedBaseIsWeapon.reset();
			const auto baseIt = a_entry.find(std::string(RuntimeContract::kFieldRunewordRecommendedBase));
			if (baseIt == a_entry.end() || baseIt->is_null()) {
				return true;
			}

			if (!baseIt->is_string()) {
				return false;
			}

			const auto base = baseIt->get<std::string>();
			if (base == RuntimeContract::kRecommendedBaseWeapon) {
				a_outRow.recommendedBaseIsWeapon = true;
				return true;
			}
			if (base == RuntimeContract::kRecommendedBaseArmor) {
				a_outRow.recommendedBaseIsWeapon = false;
				return true;
			}
			return false;
		}
	}

	bool LoadRunewordContractSnapshotFromRuntime(
		std::string_view a_relativePath,
		RunewordContractSnapshot& a_outSnapshot)
	{
		a_outSnapshot = {};

		const auto contractPath = RuntimePaths::ResolveRuntimeRelativePath(a_relativePath);
		std::ifstream in(contractPath, std::ios::binary);
		if (!in.is_open()) {
			SKSE::log::warn(
				"CalamityAffixes: runeword contract snapshot missing at {}.",
				contractPath.string());
			return false;
		}

		nlohmann::json contract = nlohmann::json::object();
		std::string parseError;
		if (!RuntimeContractDocumentPolicy::Parse(in, contract, parseError)) {
			SKSE::log::warn(
				"CalamityAffixes: runeword contract snapshot parse failed at {} ({}).",
				contractPath.string(),
				parseError);
			return false;
		}

		RunewordContractSnapshot snapshot{};

		const auto catalogIt = contract.find(std::string(RuntimeContract::kFieldRunewordCatalog));
		if (catalogIt == contract.end() || !catalogIt->is_array()) {
			SKSE::log::warn(
				"CalamityAffixes: runeword contract snapshot missing '{}' array in {}.",
				RuntimeContract::kFieldRunewordCatalog,
				contractPath.string());
			return false;
		}

		snapshot.recipes.reserve(catalogIt->size());
		std::unordered_set<std::string> recipeIds;
		for (const auto& entry : *catalogIt) {
			RunewordContractRecipeRow row{};
			if (!ParseRunewordRecipeRow(entry, row)) {
				SKSE::log::warn(
					"CalamityAffixes: invalid runeword recipe entry in '{}' at {}.",
					RuntimeContract::kFieldRunewordCatalog,
					contractPath.string());
				return false;
			}
			if (!recipeIds.emplace(row.id).second) {
				SKSE::log::warn(
					"CalamityAffixes: duplicate runeword recipe id '{}' in {}.",
					row.id,
					contractPath.string());
				return false;
			}
			snapshot.recipes.push_back(std::move(row));
		}
		if (snapshot.recipes.empty()) {
			SKSE::log::warn(
				"CalamityAffixes: runeword contract snapshot has empty '{}' in {}.",
				RuntimeContract::kFieldRunewordCatalog,
				contractPath.string());
			return false;
		}

		const auto weightsIt = contract.find(std::string(RuntimeContract::kFieldRunewordRuneWeights));
		if (weightsIt == contract.end() || !weightsIt->is_array()) {
			SKSE::log::warn(
				"CalamityAffixes: runeword contract snapshot missing '{}' array in {}.",
				RuntimeContract::kFieldRunewordRuneWeights,
				contractPath.string());
			return false;
		}

		for (const auto& entry : *weightsIt) {
			if (!entry.is_object()) {
				SKSE::log::warn(
					"CalamityAffixes: invalid rune weight entry (not object) in {}.",
					contractPath.string());
				return false;
			}

			std::string runeName;
			if (!TryReadRequiredString(entry, RuntimeContract::kFieldRuneWeightRune, runeName)) {
				SKSE::log::warn(
					"CalamityAffixes: invalid rune weight entry (missing rune name) in {}.",
					contractPath.string());
				return false;
			}

			const auto weightIt = entry.find(std::string(RuntimeContract::kFieldRuneWeightWeight));
			if (weightIt == entry.end() || !weightIt->is_number()) {
				SKSE::log::warn(
					"CalamityAffixes: invalid rune weight entry for '{}' (missing weight) in {}.",
					runeName,
					contractPath.string());
				return false;
			}

			const auto weight = weightIt->get<double>();
			if (!(weight > 0.0)) {
				SKSE::log::warn(
					"CalamityAffixes: invalid rune weight entry for '{}' (non-positive weight={}) in {}.",
					runeName,
					weight,
					contractPath.string());
				return false;
			}

			if (!RunewordContractCompatibilityPolicy::AddUniqueRuneWeight(snapshot, runeName, weight)) {
				SKSE::log::warn(
					"CalamityAffixes: duplicate rune weight entry for '{}' in {}.",
					runeName,
					contractPath.string());
				return false;
			}
		}

		if (snapshot.runeWeightsByName.empty()) {
			SKSE::log::warn(
				"CalamityAffixes: runeword contract snapshot has empty '{}' in {}.",
				RuntimeContract::kFieldRunewordRuneWeights,
				contractPath.string());
			return false;
		}

		std::unordered_set<std::string> runesUsed;
		for (const auto& row : snapshot.recipes) {
			for (const auto& rune : row.runeNames) {
				runesUsed.insert(rune);
			}
		}

		for (const auto& rune : runesUsed) {
			if (snapshot.runeWeightsByName.find(rune) != snapshot.runeWeightsByName.end()) {
				continue;
			}

			SKSE::log::warn(
				"CalamityAffixes: runeword contract snapshot is missing weight for rune '{}' in {}.",
				rune,
				contractPath.string());
			return false;
		}

		a_outSnapshot = std::move(snapshot);
		return true;
	}
}
