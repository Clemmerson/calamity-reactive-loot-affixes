#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace CalamityAffixes
{
	// Moves every map entry owned by an instance key as one preflighted operation.
	// The callback pair lets composite keys (for example instance+affix token) be
	// re-keyed without copying values or silently overwriting a destination.
	template <class Map, class GetInstanceKey, class MakeDestinationKey>
	[[nodiscard]] bool RemapInstanceKeyedMapEntries(
		Map& a_map,
		std::uint64_t a_oldInstanceKey,
		std::uint64_t a_newInstanceKey,
		GetInstanceKey&& a_getInstanceKey,
		MakeDestinationKey&& a_makeDestinationKey)
	{
		if (a_oldInstanceKey == 0u || a_newInstanceKey == 0u || a_oldInstanceKey == a_newInstanceKey) {
			return false;
		}

		for (const auto& [key, _] : a_map) {
			if (a_getInstanceKey(key) != a_oldInstanceKey) {
				continue;
			}
			if (a_map.contains(a_makeDestinationKey(key, a_newInstanceKey))) {
				return false;
			}
		}

		std::vector<typename Map::node_type> nodes;
		for (auto it = a_map.begin(); it != a_map.end();) {
			if (a_getInstanceKey(it->first) != a_oldInstanceKey) {
				++it;
				continue;
			}
			auto current = it++;
			nodes.push_back(a_map.extract(current));
		}
		for (auto& node : nodes) {
			node.key() = a_makeDestinationKey(node.key(), a_newInstanceKey);
			a_map.insert(std::move(node));
		}
		return true;
	}
}
