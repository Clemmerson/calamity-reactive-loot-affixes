#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace CalamityAffixes
{
	struct RuneforgeInventoryIdentityObservation
	{
		std::uint64_t instanceKey{ 0u };
		std::int32_t count{ 0 };
	};

	struct RuneforgeInventoryIdentityScan
	{
		std::vector<std::size_t> eligibleIndices;
		std::size_t skippedNonSingleton{ 0u };
		std::size_t skippedMissingUniqueId{ 0u };
		std::size_t skippedAmbiguousKeys{ 0u };
	};

	// Only singleton lists with a valid, unique instance key are eligible for a
	// Runeforge snapshot. A matching key on any other list makes that key
	// ambiguous, regardless of whether the other list is stacked.
	[[nodiscard]] inline RuneforgeInventoryIdentityScan AnalyzeRuneforgeInventoryIdentity(
		std::span<const RuneforgeInventoryIdentityObservation> observations)
	{
		RuneforgeInventoryIdentityScan result;
		std::vector<bool> eligible(observations.size(), false);
		std::unordered_map<std::uint64_t, std::size_t> firstIndexByKey;
		std::unordered_set<std::uint64_t> ambiguousKeys;
		firstIndexByKey.reserve(observations.size());
		ambiguousKeys.reserve(observations.size());

		for (std::size_t index = 0; index < observations.size(); ++index) {
			const auto& observation = observations[index];
			if (observation.instanceKey == 0u) {
				++result.skippedMissingUniqueId;
				continue;
			}
			if (observation.count != 1) {
				++result.skippedNonSingleton;
			}

			const auto [firstIt, inserted] = firstIndexByKey.try_emplace(observation.instanceKey, index);
			if (!inserted) {
				ambiguousKeys.insert(observation.instanceKey);
				eligible[firstIt->second] = false;
				continue;
			}

			if (observation.count != 1) {
				continue;
			}
			eligible[index] = true;
		}

		result.skippedAmbiguousKeys = ambiguousKeys.size();
		result.eligibleIndices.reserve(observations.size());
		for (std::size_t index = 0; index < eligible.size(); ++index) {
			if (eligible[index]) {
				result.eligibleIndices.push_back(index);
			}
		}
		return result;
	}
}
