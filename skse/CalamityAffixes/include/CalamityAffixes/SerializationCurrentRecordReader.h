#pragma once

#include <cstdint>
#include <utility>

#include "CalamityAffixes/SerializationWireContract.h"

namespace CalamityAffixes::SerializationWire
{
	inline constexpr std::uint32_t kMaxCurrentShuffleBagSize = 100'000u;
	inline constexpr std::uint32_t kMaxPendingDropOwnershipEntries =
		static_cast<std::uint32_t>(CalamityAffixes::kMaxPendingDropOwnership);

	enum class CurrentShuffleBagReadStatus : std::uint8_t
	{
		kComplete = 0,
		kTruncatedHeader,
		kTruncatedPayload,
		kSizeLimitExceeded,
	};

	struct CurrentShuffleBagReadResult
	{
		CurrentShuffleBagReadStatus status{ CurrentShuffleBagReadStatus::kComplete };
		std::uint32_t invalidBagSize{ 0u };
	};

	template <class Read, class Apply>
	[[nodiscard]] CurrentShuffleBagReadResult ReadCurrentLootShuffleBagsPayload(
		Read& a_read,
		Apply&& a_apply)
	{
		std::uint8_t bagCount = 0u;
		if (!static_cast<bool>(a_read(bagCount))) {
			return { .status = CurrentShuffleBagReadStatus::kTruncatedHeader };
		}

		for (std::uint8_t i = 0u; i < bagCount; ++i) {
			LootShuffleBagEntry entry{};
			std::uint32_t size = 0u;
			if (!static_cast<bool>(a_read(entry.id)) ||
				!static_cast<bool>(a_read(entry.cursor)) ||
				!static_cast<bool>(a_read(size))) {
				return { .status = CurrentShuffleBagReadStatus::kTruncatedPayload };
			}
			if (size > kMaxCurrentShuffleBagSize) {
				return {
					.status = CurrentShuffleBagReadStatus::kSizeLimitExceeded,
					.invalidBagSize = size,
				};
			}

			entry.order.reserve(size);
			for (std::uint32_t n = 0u; n < size; ++n) {
				std::uint32_t index = 0u;
				if (!static_cast<bool>(a_read(index))) {
					return { .status = CurrentShuffleBagReadStatus::kTruncatedPayload };
				}
				entry.order.push_back(index);
			}

			a_apply(std::move(entry));
		}

		return {};
	}

	// Returns false on truncation; entries read before the cut are already applied.
	template <class Read, class Apply>
	[[nodiscard]] bool ReadCurrentInstanceReforgeCountsPayload(
		Read& a_read,
		Apply&& a_apply)
	{
		std::uint32_t count = 0u;
		if (!static_cast<bool>(a_read(count))) {
			return false;
		}
		for (std::uint32_t i = 0u; i < count; ++i) {
			InstanceReforgeCountEntry entry{};
			if (!static_cast<bool>(a_read(entry.baseFormId)) ||
				!static_cast<bool>(a_read(entry.uniqueId)) ||
				!static_cast<bool>(a_read(entry.count))) {
				return false;
			}
			a_apply(entry);
		}
		return true;
	}

	enum class PendingDropOwnershipReadStatus : std::uint8_t
	{
		kComplete,
		kTruncated,
		kSizeLimitExceeded,
	};

	struct PendingDropOwnershipReadResult
	{
		PendingDropOwnershipReadStatus status{ PendingDropOwnershipReadStatus::kComplete };
		std::uint32_t invalidCount{ 0u };
		bool overflow{ false };
		std::uint32_t overflowExpiresAtDay{ 0u };
	};

	template <class Read, class Apply>
	[[nodiscard]] PendingDropOwnershipReadResult ReadCurrentPendingDropOwnershipPayload(
		Read& a_read,
		Apply&& a_apply,
		std::uint32_t a_version = 3u)
	{
		if (a_version < 1u || a_version > 3u) {
			return { .status = PendingDropOwnershipReadStatus::kTruncated };
		}
		std::uint8_t overflow = 0u;
		std::uint32_t overflowExpiresAtDay = 0u;
		std::uint32_t count = 0u;
		if (!static_cast<bool>(a_read(overflow)) ||
			!static_cast<bool>(a_read(overflowExpiresAtDay)) ||
			!static_cast<bool>(a_read(count))) {
			return { .status = PendingDropOwnershipReadStatus::kTruncated };
		}
		if (count > kMaxPendingDropOwnershipEntries) {
			return { .status = PendingDropOwnershipReadStatus::kSizeLimitExceeded, .invalidCount = count };
		}
		for (std::uint32_t i = 0u; i < count; ++i) {
			PendingDropOwnershipEntry entry{};
			if (!static_cast<bool>(a_read(entry.ownerFormId)) ||
				!static_cast<bool>(a_read(entry.uniqueId)) ||
				!static_cast<bool>(a_read(entry.baseFormId)) ||
				!static_cast<bool>(a_read(entry.expiresAtDay))) {
				return { .status = PendingDropOwnershipReadStatus::kTruncated };
			}
			if (a_version >= 2u && !static_cast<bool>(a_read(entry.referenceFormId))) {
				return { .status = PendingDropOwnershipReadStatus::kTruncated };
			}
			if (a_version >= 3u && !static_cast<bool>(a_read(entry.referenceIdentityEstablished))) {
				return { .status = PendingDropOwnershipReadStatus::kTruncated };
			}
			a_apply(entry);
		}
		return {
			.status = PendingDropOwnershipReadStatus::kComplete,
			.overflow = overflow != 0u,
			.overflowExpiresAtDay = overflowExpiresAtDay,
		};
	}

	template <class Read>
	[[nodiscard]] bool ReadCurrentMigrationFlagsPayload(
		Read& a_read,
		std::uint8_t& a_flags)
	{
		return static_cast<bool>(a_read(a_flags));
	}
}
