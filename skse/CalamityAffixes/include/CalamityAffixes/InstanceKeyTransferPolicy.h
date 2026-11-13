#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

namespace CalamityAffixes
{
	using InstanceKey = std::uint64_t;

	enum class InstanceKeyTransferDecision : std::uint8_t
	{
		kIgnore,
		kRemap,
		kConflict
	};

	struct InstanceKeyTransferContext
	{
		// Normalized TESUniqueIDChangeEvent semantics: ExtraUniqueID is keyed by
		// owner + unique ID. itemFormID identifies the moved item and is retained
		// only for legacy recovery and diagnostics.
		std::uint32_t playerID{ 0 };
		std::uint32_t oldOwnerID{ 0 };
		std::uint32_t newOwnerID{ 0 };
		std::uint32_t itemFormID{ 0 };
		std::uint16_t oldUniqueID{ 0 };
		std::uint16_t newUniqueID{ 0 };
		bool playerHasOldKey{ false };
		bool playerHasNewKey{ false };
		bool oldKeyTracked{ false };
		bool legacyItemKeyTracked{ false };
		bool newKeyTracked{ false };
	};

	struct InstanceKeyTransferPlan
	{
		InstanceKeyTransferDecision decision{ InstanceKeyTransferDecision::kIgnore };
		InstanceKey oldKey{ 0 };
		InstanceKey newKey{ 0 };
	};

	[[nodiscard]] constexpr InstanceKey MakeTrackedInstanceKey(
		std::uint32_t a_ownerFormID,
		std::uint16_t a_uniqueID) noexcept
	{
		return (static_cast<InstanceKey>(a_ownerFormID) << 16u) |
		       static_cast<InstanceKey>(a_uniqueID);
	}

	// A dropped item is no longer owned by the player's UID namespace. Bind its
	// transient state to the concrete world reference instead, using a
	// Calamity-internal sentinel UID so the key remains compatible with existing
	// serialization. Safety comes from matching the exact reference FormID, not
	// from assuming the engine reserves this UID value.
	inline constexpr std::uint16_t kDetachedWorldInstanceUID = 0xFFFFu;

	[[nodiscard]] constexpr InstanceKey MakeDetachedWorldInstanceKey(
		std::uint32_t a_referenceFormID) noexcept
	{
		return a_referenceFormID == 0u ?
			InstanceKey{ 0 } :
			MakeTrackedInstanceKey(a_referenceFormID, kDetachedWorldInstanceUID);
	}

	enum class DropTransferDecision : std::uint8_t
	{
		kIgnore,
		kRetainSource,
		kRemap,
		kConflictRetain
	};

	[[nodiscard]] constexpr DropTransferDecision ResolveDropTransfer(
		InstanceKey a_sourceKey,
		InstanceKey a_worldKey,
		bool a_ambiguousSource,
		bool a_sourceHasTrackedState,
		bool a_destinationHasMaterialState) noexcept
	{
		if (a_ambiguousSource) {
			return DropTransferDecision::kConflictRetain;
		}
		if (a_sourceKey == 0u || !a_sourceHasTrackedState) {
			return DropTransferDecision::kIgnore;
		}
		if (a_worldKey == 0u) {
			return DropTransferDecision::kRetainSource;
		}
		if (a_destinationHasMaterialState) {
			return DropTransferDecision::kConflictRetain;
		}
		return DropTransferDecision::kRemap;
	}

	[[nodiscard]] constexpr bool IsDetachedWorldReferenceMatch(
		InstanceKey a_guardedKey,
		InstanceKey a_referenceKey) noexcept
	{
		// A zero guarded key is a valid "matched drop, no state" marker. Any
		// transferable key requires the concrete reference identity to agree.
		return a_guardedKey == 0u ||
		       (a_referenceKey != 0u && a_guardedKey == a_referenceKey);
	}

	[[nodiscard]] constexpr bool IsOrphanedPlayerInstanceKey(
		InstanceKey a_instanceKey,
		std::uint32_t a_playerID,
		bool a_playerInventoryContainsKey) noexcept
	{
		return a_instanceKey != 0u &&
		       a_playerID != 0u &&
		       static_cast<std::uint32_t>(a_instanceKey >> 16u) == a_playerID &&
		       !a_playerInventoryContainsKey;
	}

	struct InstanceBaseObjectRecoveryCandidate
	{
		InstanceKey key{ 0 };
		std::uint32_t baseObjectFormID{ 0 };
		bool materialState{ false };
		bool playerInventoryContainsKey{ false };
	};

	struct InstanceBaseObjectRecoveryPlan
	{
		InstanceKey key{ 0 };
		bool ambiguous{ false };
	};

	[[nodiscard]] constexpr InstanceBaseObjectRecoveryPlan ResolveDroppedPlayerInstanceByBaseObject(
		std::uint32_t a_playerID,
		std::uint32_t a_baseObjectFormID,
		std::span<const InstanceBaseObjectRecoveryCandidate> a_candidates,
		std::int32_t a_eventItemCount = 1,
		std::int32_t a_remainingPlayerBaseItemCount = 0) noexcept
	{
		InstanceBaseObjectRecoveryPlan plan{};
		if (a_playerID == 0u || a_baseObjectFormID == 0u) {
			return plan;
		}
		if (a_eventItemCount != 1 || a_remainingPlayerBaseItemCount != 0) {
			plan.ambiguous = std::ranges::any_of(a_candidates, [&](const auto& a_candidate) {
				return a_candidate.key != 0u &&
				       a_candidate.baseObjectFormID == a_baseObjectFormID &&
				       a_candidate.materialState &&
				       !a_candidate.playerInventoryContainsKey &&
				       static_cast<std::uint32_t>(a_candidate.key >> 16u) == a_playerID;
			});
			return plan;
		}

		for (const auto& candidate : a_candidates) {
			if (candidate.key == 0u ||
				candidate.baseObjectFormID != a_baseObjectFormID ||
				!candidate.materialState ||
				candidate.playerInventoryContainsKey ||
				static_cast<std::uint32_t>(candidate.key >> 16u) != a_playerID) {
				continue;
			}

			if (plan.key == 0u) {
				plan.key = candidate.key;
				continue;
			}
			if (plan.key != candidate.key) {
				plan.key = 0u;
				plan.ambiguous = true;
				return plan;
			}
		}
		return plan;
	}

	[[nodiscard]] constexpr InstanceKeyTransferPlan ResolveInstanceKeyTransfer(
		const InstanceKeyTransferContext& a_context) noexcept
	{
		const auto oldKey = MakeTrackedInstanceKey(a_context.oldOwnerID, a_context.oldUniqueID);
		const auto newKey = MakeTrackedInstanceKey(a_context.newOwnerID, a_context.newUniqueID);
		const auto legacyItemKey = MakeTrackedInstanceKey(a_context.itemFormID, a_context.oldUniqueID);

		if (a_context.playerID == 0u ||
			a_context.oldOwnerID == 0u ||
			a_context.newOwnerID == 0u ||
			a_context.itemFormID == 0u ||
			a_context.oldUniqueID == 0u ||
			a_context.newUniqueID == 0u) {
			return { InstanceKeyTransferDecision::kIgnore, oldKey, newKey };
		}

		const auto sourceKey = a_context.oldKeyTracked ?
			oldKey :
			(a_context.legacyItemKeyTracked ? legacyItemKey : InstanceKey{ 0 });
		if (sourceKey == 0u || sourceKey == newKey) {
			return { InstanceKeyTransferDecision::kIgnore, sourceKey, newKey };
		}

		const bool ownerTouchesPlayer =
			a_context.oldOwnerID == a_context.playerID ||
			a_context.newOwnerID == a_context.playerID;
		const bool playerInventoryContainsEither =
			a_context.playerHasOldKey || a_context.playerHasNewKey;
		// The exact owner + UID key moving between two other owners is still the
		// same item changing hands: Malborn passing the player's confiscated gear
		// to the embassy chest, or a merchant moving sold stock. Leaving the state
		// behind orphaned it, and the item came back to the player bare. Only the
		// inexact legacy item-form key still needs the player as evidence.
		const bool exactSourceMoves = a_context.oldKeyTracked;
		if (!ownerTouchesPlayer && !playerInventoryContainsEither && !exactSourceMoves) {
			return { InstanceKeyTransferDecision::kIgnore, sourceKey, newKey };
		}

		if (a_context.newKeyTracked) {
			return { InstanceKeyTransferDecision::kConflict, sourceKey, newKey };
		}

		return { InstanceKeyTransferDecision::kRemap, sourceKey, newKey };
	}
}
