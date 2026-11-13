#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace CalamityAffixes
{
	struct DropPickupInventoryIdentity
	{
		std::uint64_t instanceKey{ 0u };
		std::uint32_t baseObjectFormID{ 0u };
		std::int32_t count{ 0 };
	};

	struct PendingDropInventoryCandidate
	{
		std::uint64_t instanceKey{ 0u };
		std::uint32_t baseObjectFormID{ 0u };
	};

	struct PendingDropInventoryResolution
	{
		std::uint64_t instanceKey{ 0u };
		bool ambiguous{ false };
	};

	// Resolve only an exact, unique singleton identity; never infer ownership
	// from a base-object match alone.
	[[nodiscard]] inline bool HasUniqueExactDropPickupInventoryIdentity(
		std::span<const DropPickupInventoryIdentity> a_observations,
		std::uint64_t a_instanceKey,
		std::uint32_t a_baseObjectFormID) noexcept
	{
		if (a_instanceKey == 0u || a_baseObjectFormID == 0u) {
			return false;
		}
		std::size_t matches = 0u;
		for (const auto& observation : a_observations) {
			if (observation.instanceKey != a_instanceKey || observation.baseObjectFormID != a_baseObjectFormID) {
				continue;
			}
			if (observation.count != 1) {
				return false;
			}
			++matches;
		}
		return matches == 1u;
	}

	// A zero-UID pickup event can still be resolved when Skyrim's current
	// inventory ExtraList carries the exact owner+UID identity that is pending.
	// Multiple matching pending identities are ambiguous, even if each one is
	// individually present in inventory; a base-object pickup event cannot say
	// which one it describes.
	[[nodiscard]] inline PendingDropInventoryResolution ResolveUniquePendingDropFromInventory(
		std::span<const PendingDropInventoryCandidate> a_pending,
		std::span<const DropPickupInventoryIdentity> a_inventory,
		std::uint32_t a_baseObjectFormID) noexcept
	{
		if (a_baseObjectFormID == 0u) {
			return {};
		}

		PendingDropInventoryResolution result{};
		for (const auto& pending : a_pending) {
			if (pending.instanceKey == 0u || pending.baseObjectFormID != a_baseObjectFormID ||
				!HasUniqueExactDropPickupInventoryIdentity(a_inventory, pending.instanceKey, a_baseObjectFormID)) {
				continue;
			}
			if (result.instanceKey != 0u && result.instanceKey != pending.instanceKey) {
				return { .instanceKey = 0u, .ambiguous = true };
			}
			result.instanceKey = pending.instanceKey;
		}
		return result;
	}
}
