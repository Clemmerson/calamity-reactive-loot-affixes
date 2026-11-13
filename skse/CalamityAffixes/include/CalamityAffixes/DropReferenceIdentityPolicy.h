#pragma once

#include <cstdint>

namespace CalamityAffixes
{
	enum class DropReferenceIdentityAction : std::uint8_t
	{
	kReject,
	kAddIdentity,
	kAlreadyExact,
	kConflict
	};

	struct DropReferenceIdentityContext
	{
		std::uint32_t playerFormID{ 0u };
		std::uint32_t sourceOwnerFormID{ 0u };
		std::uint32_t baseObjectFormID{ 0u };
		std::uint32_t referenceExtraOwnerFormID{ 0u };
		std::uint64_t sourceKey{ 0u };
		std::uint64_t referenceHandle{ 0u };
		std::uint16_t sourceUniqueID{ 0u };
		std::uint16_t referenceExtraUniqueID{ 0u };
		std::int32_t itemCount{ 0 };
	};

	// Stamp only the concrete singleton dropped by this player when an exact
	// tracked player owner+UID key was recovered. Conflicting reference identity
	// is never overwritten, and a base-only drop cannot receive a marker.
	[[nodiscard]] constexpr DropReferenceIdentityAction PlanDropReferenceIdentity(
		const DropReferenceIdentityContext& a_context) noexcept
	{
		if (a_context.playerFormID == 0u || a_context.sourceOwnerFormID != a_context.playerFormID ||
			a_context.baseObjectFormID == 0u || a_context.sourceKey == 0u ||
			a_context.referenceHandle == 0u || a_context.itemCount != 1 ||
			a_context.sourceUniqueID == 0u) {
			return DropReferenceIdentityAction::kReject;
		}
		const auto expectedKey = (static_cast<std::uint64_t>(a_context.sourceOwnerFormID) << 16u) |
			static_cast<std::uint64_t>(a_context.sourceUniqueID);
		if (a_context.sourceKey != expectedKey) {
			return DropReferenceIdentityAction::kReject;
		}
		if (a_context.referenceExtraOwnerFormID == 0u && a_context.referenceExtraUniqueID == 0u) {
			return DropReferenceIdentityAction::kAddIdentity;
		}
		if (a_context.referenceExtraOwnerFormID == a_context.sourceOwnerFormID &&
			a_context.referenceExtraUniqueID == a_context.sourceUniqueID) {
			return DropReferenceIdentityAction::kAlreadyExact;
		}
		return DropReferenceIdentityAction::kConflict;
	}
}
