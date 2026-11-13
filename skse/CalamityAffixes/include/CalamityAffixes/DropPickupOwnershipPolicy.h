#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace CalamityAffixes
{
	using DropOwnershipKey = std::uint64_t;
	using DropOwnershipFormID = std::uint32_t;

	struct PendingDropOwnership
	{
		DropOwnershipKey instanceKey{ 0u };
		DropOwnershipFormID baseObjectFormID{ 0u };
		std::uint32_t expiresAtDay{ 0u };
		// TESContainerChangedEvent.reference can contain the spawned reference's
		// dynamic FormID before that reference is registered in the handle table.
		// Retain it only as a candidate; activation must resolve the same FormID
		// and base before this ID is trusted as a concrete object identity.
		DropOwnershipFormID referenceFormID{ 0u };
		bool referenceIdentityEstablished{ false };

		[[nodiscard]] constexpr bool operator==(const PendingDropOwnership&) const noexcept = default;
	};

	inline constexpr std::size_t kMaxPendingDropOwnership = 64u;
	inline constexpr std::uint32_t kPendingDropOwnershipLifetimeDays = 30u;

	enum class PendingDropTrackResult : std::uint8_t
	{
		kInvalid,
		kAdded,
		kDuplicate,
		kConflict,
		kFull
	};

	enum class PendingDropTransferResult : std::uint8_t
	{
		kNoMatch,
		kUpdated,
		kResolved,
		kConflict
	};

	[[nodiscard]] constexpr bool IsPendingDropOwnershipLive(
		const PendingDropOwnership& a_entry,
		std::uint32_t a_currentDay) noexcept
	{
		return a_entry.instanceKey != 0u &&
		       a_entry.baseObjectFormID != 0u &&
		       a_entry.expiresAtDay != 0u &&
		       a_currentDay < a_entry.expiresAtDay;
	}

	[[nodiscard]] inline PendingDropTrackResult TrackPendingDropOwnership(
		std::vector<PendingDropOwnership>& a_entries,
		DropOwnershipKey a_instanceKey,
		DropOwnershipFormID a_baseObjectFormID,
		std::uint32_t a_expiresAtDay,
		DropOwnershipFormID a_referenceFormID = 0u,
		bool a_referenceIdentityEstablished = false)
	{
		if (a_instanceKey == 0u || a_baseObjectFormID == 0u || a_expiresAtDay == 0u) {
			return PendingDropTrackResult::kInvalid;
		}

		for (auto& entry : a_entries) {
			if (entry.instanceKey != a_instanceKey) {
				continue;
			}
			if (entry.baseObjectFormID != a_baseObjectFormID) {
				return PendingDropTrackResult::kConflict;
			}
			if (a_referenceFormID != 0u) {
				if (entry.referenceFormID != 0u && entry.referenceFormID != a_referenceFormID) {
					return PendingDropTrackResult::kConflict;
				}
				entry.referenceFormID = a_referenceFormID;
				entry.referenceIdentityEstablished = entry.referenceIdentityEstablished || a_referenceIdentityEstablished;
			}
			entry.expiresAtDay = std::max(entry.expiresAtDay, a_expiresAtDay);
			return PendingDropTrackResult::kDuplicate;
		}

		if (a_entries.size() >= kMaxPendingDropOwnership) {
			return PendingDropTrackResult::kFull;
		}
		a_entries.push_back({ a_instanceKey, a_baseObjectFormID, a_expiresAtDay, a_referenceFormID, a_referenceIdentityEstablished });
		return PendingDropTrackResult::kAdded;
	}

	[[nodiscard]] inline bool EstablishPendingDropReferenceIdentity(
		std::vector<PendingDropOwnership>& a_entries,
		DropOwnershipKey a_instanceKey,
		DropOwnershipFormID a_baseObjectFormID,
		DropOwnershipFormID a_referenceFormID,
		std::uint32_t a_currentDay)
	{
		if (a_instanceKey == 0u || a_baseObjectFormID == 0u || a_referenceFormID == 0u) {
			return false;
		}
		PendingDropOwnership* match = nullptr;
		for (auto& entry : a_entries) {
			if (!IsPendingDropOwnershipLive(entry, a_currentDay) ||
				entry.instanceKey != a_instanceKey ||
				entry.baseObjectFormID != a_baseObjectFormID ||
				entry.referenceFormID != a_referenceFormID) {
				continue;
			}
			if (match) {
				return false;
			}
			match = &entry;
		}
		if (!match) {
			return false;
		}
		match->referenceIdentityEstablished = true;
		return true;
	}

	struct PendingDropReferenceResolution
	{
		DropOwnershipKey instanceKey{ 0u };
		bool ambiguous{ false };
	};

	[[nodiscard]] inline PendingDropReferenceResolution ResolvePendingDropByActivatedReference(
		std::span<const PendingDropOwnership> a_entries,
		DropOwnershipFormID a_ownerFormID,
		DropOwnershipFormID a_baseObjectFormID,
		DropOwnershipFormID a_referenceFormID,
		std::uint32_t a_currentDay) noexcept
	{
		if (a_ownerFormID == 0u || a_baseObjectFormID == 0u || a_referenceFormID == 0u) {
			return {};
		}
		PendingDropReferenceResolution result{};
		for (const auto& entry : a_entries) {
			if (!IsPendingDropOwnershipLive(entry, a_currentDay) ||
				entry.referenceFormID != a_referenceFormID ||
				entry.baseObjectFormID != a_baseObjectFormID ||
				static_cast<DropOwnershipFormID>(entry.instanceKey >> 16u) != a_ownerFormID) {
				continue;
			}
			if (result.instanceKey != 0u && result.instanceKey != entry.instanceKey) {
				return { .instanceKey = 0u, .ambiguous = true };
			}
			result.instanceKey = entry.instanceKey;
		}
		return result;
	}

	[[nodiscard]] inline bool HasPendingDropOwnership(
		std::span<const PendingDropOwnership> a_entries,
		DropOwnershipKey a_instanceKey,
		DropOwnershipFormID a_baseObjectFormID = 0u) noexcept
	{
		return std::ranges::any_of(a_entries, [&](const auto& a_entry) {
			return a_entry.instanceKey == a_instanceKey &&
			       (a_baseObjectFormID == 0u || a_entry.baseObjectFormID == a_baseObjectFormID);
		});
	}

	[[nodiscard]] inline PendingDropTransferResult ApplyExactPendingDropIdentityTransfer(
		std::vector<PendingDropOwnership>& a_entries,
		DropOwnershipKey a_oldKey,
		DropOwnershipKey a_newKey,
		DropOwnershipFormID a_eventBaseObjectFormID,
		bool a_destinationHasMaterialState,
		bool a_returnedToPlayer)
	{
		if (a_oldKey == 0u || a_newKey == 0u || a_oldKey == a_newKey || a_eventBaseObjectFormID == 0u) {
			return PendingDropTransferResult::kNoMatch;
		}

		auto match = a_entries.end();
		for (auto it = a_entries.begin(); it != a_entries.end(); ++it) {
			if (it->instanceKey != a_oldKey || it->baseObjectFormID != a_eventBaseObjectFormID) {
				continue;
			}
			if (match != a_entries.end()) {
				return PendingDropTransferResult::kConflict;
			}
			match = it;
		}
		if (match == a_entries.end()) {
			return PendingDropTransferResult::kNoMatch;
		}
		if (a_destinationHasMaterialState) {
			return PendingDropTransferResult::kConflict;
		}
		if (a_returnedToPlayer) {
			a_entries.erase(match);
			return PendingDropTransferResult::kResolved;
		}
		match->instanceKey = a_newKey;
		return PendingDropTransferResult::kUpdated;
	}

	[[nodiscard]] inline bool ResolvePendingDropWithExactInventoryKey(
		std::vector<PendingDropOwnership>& a_entries,
		DropOwnershipKey a_inventoryKey,
		DropOwnershipFormID a_baseObjectFormID)
	{
		if (a_inventoryKey == 0u || a_baseObjectFormID == 0u) {
			return false;
		}
		const auto before = a_entries.size();
		std::erase_if(a_entries, [&](const auto& a_entry) {
			return a_entry.instanceKey == a_inventoryKey &&
			       a_entry.baseObjectFormID == a_baseObjectFormID;
		});
		return a_entries.size() != before;
	}

	[[nodiscard]] inline std::vector<DropOwnershipKey> ExpirePendingDropOwnership(
		std::vector<PendingDropOwnership>& a_entries,
		std::uint32_t a_currentDay)
	{
		std::vector<DropOwnershipKey> expired;
		std::erase_if(a_entries, [&](const auto& a_entry) {
			if (IsPendingDropOwnershipLive(a_entry, a_currentDay)) {
				return false;
			}
			expired.push_back(a_entry.instanceKey);
			return true;
		});
		return expired;
	}

	[[nodiscard]] constexpr bool IsPendingDropOrphanProtected(
		DropOwnershipKey a_key,
		std::span<const PendingDropOwnership> a_entries,
		std::uint32_t a_currentDay) noexcept
	{
		return std::ranges::any_of(a_entries, [&](const auto& a_entry) {
			return a_entry.instanceKey == a_key && IsPendingDropOwnershipLive(a_entry, a_currentDay);
		});
	}
}
