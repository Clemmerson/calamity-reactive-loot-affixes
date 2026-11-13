#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "CalamityAffixes/InstanceAffixSlots.h"
#include "CalamityAffixes/DropPickupOwnershipPolicy.h"

namespace CalamityAffixes::SerializationWire
{
	// SKSE record types use the same numeric convention as C/C++ multi-character
	// literals (for example, 'IAXF'), but spelling it out avoids implementation-
	// defined literals in the behavioral test contract.
	[[nodiscard]] consteval std::uint32_t MakeRecordType(char a_a, char a_b, char a_c, char a_d) noexcept
	{
		return (static_cast<std::uint32_t>(static_cast<unsigned char>(a_a)) << 24u) |
		       (static_cast<std::uint32_t>(static_cast<unsigned char>(a_b)) << 16u) |
		       (static_cast<std::uint32_t>(static_cast<unsigned char>(a_c)) << 8u) |
		       static_cast<std::uint32_t>(static_cast<unsigned char>(a_d));
	}

	struct RecordContract
	{
		std::uint32_t type{ 0u };
		std::uint32_t version{ 0u };

		[[nodiscard]] constexpr bool operator==(const RecordContract&) const noexcept = default;
	};

	inline constexpr RecordContract kInstanceAffixes{ MakeRecordType('I', 'A', 'X', 'F'), 7u };
	inline constexpr RecordContract kInstanceRuntimeStates{ MakeRecordType('I', 'R', 'S', 'T'), 1u };
	inline constexpr RecordContract kRunewordState{ MakeRecordType('R', 'W', 'R', 'D'), 1u };
	inline constexpr RecordContract kLootEvaluated{ MakeRecordType('L', 'R', 'L', 'D'), 1u };
	inline constexpr RecordContract kLootCurrencyLedger{ MakeRecordType('L', 'C', 'L', 'D'), 2u };
	inline constexpr RecordContract kCorpseCurrencyRuntime{ MakeRecordType('C', 'C', 'R', 'T'), 1u };
	inline constexpr RecordContract kLootShuffleBags{ MakeRecordType('L', 'S', 'B', 'G'), 2u };
	inline constexpr RecordContract kMigrationFlags{ MakeRecordType('M', 'F', 'L', 'G'), 1u };
	// Appended last so every earlier record keeps its position; older DLLs drain
	// it as an unknown record.
	inline constexpr RecordContract kInstanceReforgeCounts{ MakeRecordType('I', 'R', 'F', 'C'), 1u };
	inline constexpr RecordContract kPendingDropOwnership{ MakeRecordType('P', 'D', 'O', 'P'), 3u };

	inline constexpr std::array<RecordContract, 10> kCurrentRecordSequence{
		kInstanceAffixes,
		kInstanceRuntimeStates,
		kRunewordState,
		kLootEvaluated,
		kLootCurrencyLedger,
		kCorpseCurrencyRuntime,
		kLootShuffleBags,
		kMigrationFlags,
		kInstanceReforgeCounts,
		kPendingDropOwnership,
	};

	struct InstanceAffixEntry
	{
		std::uint32_t baseFormId{ 0u };
		std::uint16_t uniqueId{ 0u };
		std::uint8_t affixCount{ 0u };
		std::array<std::uint64_t, kMaxAffixesPerItem> tokens{};

		[[nodiscard]] bool operator==(const InstanceAffixEntry&) const noexcept = default;
	};

	struct InstanceRuntimeStateEntry
	{
		std::uint32_t baseFormId{ 0u };
		std::uint16_t uniqueId{ 0u };
		std::uint64_t affixToken{ 0u };
		std::uint32_t evolutionXp{ 0u };
		std::uint32_t modeCycleCounter{ 0u };
		std::uint32_t modeIndex{ 0u };

		[[nodiscard]] bool operator==(const InstanceRuntimeStateEntry&) const noexcept = default;
	};

	struct RunewordFragmentEntry
	{
		std::uint64_t runeToken{ 0u };
		std::uint32_t amount{ 0u };

		[[nodiscard]] bool operator==(const RunewordFragmentEntry&) const noexcept = default;
	};

	struct RunewordInstanceEntry
	{
		std::uint32_t baseFormId{ 0u };
		std::uint16_t uniqueId{ 0u };
		std::uint64_t recipeToken{ 0u };
		std::uint32_t insertedRunes{ 0u };

		[[nodiscard]] bool operator==(const RunewordInstanceEntry&) const noexcept = default;
	};

	struct LootEvaluatedEntry
	{
		std::uint32_t baseFormId{ 0u };
		std::uint16_t uniqueId{ 0u };

		[[nodiscard]] bool operator==(const LootEvaluatedEntry&) const noexcept = default;
	};

	struct LootCurrencyLedgerEntry
	{
		std::uint64_t instanceKey{ 0u };
		std::uint32_t dayStamp{ 0u };

		[[nodiscard]] bool operator==(const LootCurrencyLedgerEntry&) const noexcept = default;
	};

	struct CorpseCurrencyLedgerEntry
	{
		std::uint32_t corpseFormId{ 0u };
		std::uint32_t dayStamp{ 0u };
		std::uint8_t processedMask{ 0u };

		[[nodiscard]] bool operator==(const CorpseCurrencyLedgerEntry&) const noexcept = default;
	};

	struct InstanceReforgeCountEntry
	{
		std::uint32_t baseFormId{ 0u };
		std::uint16_t uniqueId{ 0u };
		std::uint8_t count{ 0u };

		[[nodiscard]] bool operator==(const InstanceReforgeCountEntry&) const noexcept = default;
	};

	struct PendingDropOwnershipEntry
	{
		std::uint32_t ownerFormId{ 0u };
		std::uint16_t uniqueId{ 0u };
		std::uint32_t baseFormId{ 0u };
		std::uint32_t expiresAtDay{ 0u };
		std::uint32_t referenceFormId{ 0u };
		std::uint8_t referenceIdentityEstablished{ 0u };

		[[nodiscard]] bool operator==(const PendingDropOwnershipEntry&) const noexcept = default;
	};

	struct LootShuffleBagEntry
	{
		std::uint8_t id{ 0u };
		std::uint32_t cursor{ 0u };
		std::vector<std::uint32_t> order{};

		[[nodiscard]] bool operator==(const LootShuffleBagEntry&) const noexcept = default;
	};

	// This is deliberately a wire DTO, not runtime state ownership. EventBridge
	// still decides what state is saved and when; this snapshot only fixes the
	// current nine-record order and scalar layout in one testable place.
	struct CurrentSaveSnapshot
	{
		std::vector<InstanceAffixEntry> instanceAffixes{};
		std::vector<InstanceRuntimeStateEntry> instanceRuntimeStates{};

		std::uint32_t selectedRunewordBaseFormId{ 0u };
		std::uint16_t selectedRunewordUniqueId{ 0u };
		std::uint32_t runewordRecipeCycleCursor{ 0u };
		std::uint32_t runewordBaseCycleCursor{ 0u };
		std::vector<RunewordFragmentEntry> runewordFragments{};
		std::vector<RunewordInstanceEntry> runewordInstances{};

		std::vector<LootEvaluatedEntry> lootEvaluated{};
		std::vector<LootCurrencyLedgerEntry> lootCurrencyLedger{};

		std::uint32_t runewordFragmentFailStreak{ 0u };
		std::uint32_t reforgeOrbFailStreak{ 0u };
		std::vector<CorpseCurrencyLedgerEntry> corpseCurrencyLedger{};

		std::vector<LootShuffleBagEntry> lootShuffleBags{};
		std::uint8_t migrationFlags{ 0u };

		std::vector<InstanceReforgeCountEntry> instanceReforgeCounts{};
		std::vector<PendingDropOwnershipEntry> pendingDropOwnership{};
		bool pendingDropOwnershipOverflow{ false };
		std::uint32_t pendingDropOwnershipOverflowExpiresAtDay{ 0u };

		[[nodiscard]] bool operator==(const CurrentSaveSnapshot&) const noexcept = default;
	};

	template <class Write, std::unsigned_integral T>
	[[nodiscard]] bool WriteScalar(Write& a_write, T a_value)
	{
		return static_cast<bool>(a_write(a_value));
	}

	template <class Write>
	[[nodiscard]] bool WriteInstanceAffixesPayload(Write& a_write, const CurrentSaveSnapshot& a_snapshot)
	{
		if (!WriteScalar(a_write, static_cast<std::uint32_t>(a_snapshot.instanceAffixes.size()))) {
			return false;
		}
		for (const auto& entry : a_snapshot.instanceAffixes) {
			if (!WriteScalar(a_write, entry.baseFormId) ||
				!WriteScalar(a_write, entry.uniqueId) ||
				!WriteScalar(a_write, entry.affixCount)) {
				return false;
			}
			for (const auto token : entry.tokens) {
				if (!WriteScalar(a_write, token)) {
					return false;
				}
			}
		}
		return true;
	}

	template <class Write>
	[[nodiscard]] bool WriteInstanceRuntimeStatesPayload(Write& a_write, const CurrentSaveSnapshot& a_snapshot)
	{
		if (!WriteScalar(a_write, static_cast<std::uint32_t>(a_snapshot.instanceRuntimeStates.size()))) {
			return false;
		}
		for (const auto& entry : a_snapshot.instanceRuntimeStates) {
			if (!WriteScalar(a_write, entry.baseFormId) ||
				!WriteScalar(a_write, entry.uniqueId) ||
				!WriteScalar(a_write, entry.affixToken) ||
				!WriteScalar(a_write, entry.evolutionXp) ||
				!WriteScalar(a_write, entry.modeCycleCounter) ||
				!WriteScalar(a_write, entry.modeIndex)) {
				return false;
			}
		}
		return true;
	}

	template <class Write>
	[[nodiscard]] bool WriteRunewordStatePayload(Write& a_write, const CurrentSaveSnapshot& a_snapshot)
	{
		if (!WriteScalar(a_write, a_snapshot.selectedRunewordBaseFormId) ||
			!WriteScalar(a_write, a_snapshot.selectedRunewordUniqueId) ||
			!WriteScalar(a_write, a_snapshot.runewordRecipeCycleCursor) ||
			!WriteScalar(a_write, a_snapshot.runewordBaseCycleCursor) ||
			!WriteScalar(a_write, static_cast<std::uint32_t>(a_snapshot.runewordFragments.size()))) {
			return false;
		}
		for (const auto& entry : a_snapshot.runewordFragments) {
			if (!WriteScalar(a_write, entry.runeToken) || !WriteScalar(a_write, entry.amount)) {
				return false;
			}
		}

		if (!WriteScalar(a_write, static_cast<std::uint32_t>(a_snapshot.runewordInstances.size()))) {
			return false;
		}
		for (const auto& entry : a_snapshot.runewordInstances) {
			if (!WriteScalar(a_write, entry.baseFormId) ||
				!WriteScalar(a_write, entry.uniqueId) ||
				!WriteScalar(a_write, entry.recipeToken) ||
				!WriteScalar(a_write, entry.insertedRunes)) {
				return false;
			}
		}
		return true;
	}

	template <class Write>
	[[nodiscard]] bool WriteLootEvaluatedPayload(Write& a_write, const CurrentSaveSnapshot& a_snapshot)
	{
		if (!WriteScalar(a_write, static_cast<std::uint32_t>(a_snapshot.lootEvaluated.size()))) {
			return false;
		}
		for (const auto& entry : a_snapshot.lootEvaluated) {
			if (!WriteScalar(a_write, entry.baseFormId) || !WriteScalar(a_write, entry.uniqueId)) {
				return false;
			}
		}
		return true;
	}

	template <class Write>
	[[nodiscard]] bool WriteLootCurrencyLedgerPayload(Write& a_write, const CurrentSaveSnapshot& a_snapshot)
	{
		if (!WriteScalar(a_write, static_cast<std::uint32_t>(a_snapshot.lootCurrencyLedger.size()))) {
			return false;
		}
		for (const auto& entry : a_snapshot.lootCurrencyLedger) {
			if (!WriteScalar(a_write, entry.instanceKey) || !WriteScalar(a_write, entry.dayStamp)) {
				return false;
			}
		}
		return true;
	}

	template <class Write>
	[[nodiscard]] bool WriteCorpseCurrencyRuntimePayload(Write& a_write, const CurrentSaveSnapshot& a_snapshot)
	{
		if (!WriteScalar(a_write, a_snapshot.runewordFragmentFailStreak) ||
			!WriteScalar(a_write, a_snapshot.reforgeOrbFailStreak) ||
			!WriteScalar(a_write, static_cast<std::uint32_t>(a_snapshot.corpseCurrencyLedger.size()))) {
			return false;
		}
		for (const auto& entry : a_snapshot.corpseCurrencyLedger) {
			if (!WriteScalar(a_write, entry.corpseFormId) ||
				!WriteScalar(a_write, entry.dayStamp) ||
				!WriteScalar(a_write, entry.processedMask)) {
				return false;
			}
		}
		return true;
	}

	template <class Write>
	[[nodiscard]] bool WriteLootShuffleBagsPayload(Write& a_write, const CurrentSaveSnapshot& a_snapshot)
	{
		if (!WriteScalar(a_write, static_cast<std::uint8_t>(a_snapshot.lootShuffleBags.size()))) {
			return false;
		}
		for (const auto& bag : a_snapshot.lootShuffleBags) {
			if (!WriteScalar(a_write, bag.id) ||
				!WriteScalar(a_write, bag.cursor) ||
				!WriteScalar(a_write, static_cast<std::uint32_t>(bag.order.size()))) {
				return false;
			}
			for (const auto index : bag.order) {
				if (!WriteScalar(a_write, index)) {
					return false;
				}
			}
		}
		return true;
	}

	template <class Write>
	[[nodiscard]] bool WriteMigrationFlagsPayload(Write& a_write, const CurrentSaveSnapshot& a_snapshot)
	{
		return WriteScalar(a_write, a_snapshot.migrationFlags);
	}

	template <class Write>
	[[nodiscard]] bool WriteInstanceReforgeCountsPayload(Write& a_write, const CurrentSaveSnapshot& a_snapshot)
	{
		if (!WriteScalar(a_write, static_cast<std::uint32_t>(a_snapshot.instanceReforgeCounts.size()))) {
			return false;
		}
		for (const auto& entry : a_snapshot.instanceReforgeCounts) {
			if (!WriteScalar(a_write, entry.baseFormId) ||
				!WriteScalar(a_write, entry.uniqueId) ||
				!WriteScalar(a_write, entry.count)) {
				return false;
			}
		}
		return true;
	}

	template <class Write>
	[[nodiscard]] bool WritePendingDropOwnershipPayload(Write& a_write, const CurrentSaveSnapshot& a_snapshot)
	{
		if (!WriteScalar(a_write, static_cast<std::uint8_t>(a_snapshot.pendingDropOwnershipOverflow ? 1u : 0u)) ||
			!WriteScalar(a_write, a_snapshot.pendingDropOwnershipOverflowExpiresAtDay) ||
			!WriteScalar(a_write, static_cast<std::uint32_t>(a_snapshot.pendingDropOwnership.size()))) {
			return false;
		}
		for (const auto& entry : a_snapshot.pendingDropOwnership) {
			if (!WriteScalar(a_write, entry.ownerFormId) ||
				!WriteScalar(a_write, entry.uniqueId) ||
				!WriteScalar(a_write, entry.baseFormId) ||
				!WriteScalar(a_write, entry.expiresAtDay) ||
				!WriteScalar(a_write, entry.referenceFormId) ||
				!WriteScalar(a_write, entry.referenceIdentityEstablished)) {
				return false;
			}
		}
		return true;
	}

	template <class OpenRecord, class Write>
	[[nodiscard]] bool WriteCurrentRecords(
		const CurrentSaveSnapshot& a_snapshot,
		OpenRecord&& a_openRecord,
		Write&& a_write)
	{
		const auto writeRecord = [&](RecordContract a_contract, auto a_writePayload) {
			return static_cast<bool>(a_openRecord(a_contract.type, a_contract.version)) &&
			       a_writePayload(a_write, a_snapshot);
		};

		return writeRecord(kInstanceAffixes, WriteInstanceAffixesPayload<Write>) &&
		       writeRecord(kInstanceRuntimeStates, WriteInstanceRuntimeStatesPayload<Write>) &&
		       writeRecord(kRunewordState, WriteRunewordStatePayload<Write>) &&
		       writeRecord(kLootEvaluated, WriteLootEvaluatedPayload<Write>) &&
		       writeRecord(kLootCurrencyLedger, WriteLootCurrencyLedgerPayload<Write>) &&
		       writeRecord(kCorpseCurrencyRuntime, WriteCorpseCurrencyRuntimePayload<Write>) &&
		       writeRecord(kLootShuffleBags, WriteLootShuffleBagsPayload<Write>) &&
		       writeRecord(kMigrationFlags, WriteMigrationFlagsPayload<Write>) &&
		       writeRecord(kInstanceReforgeCounts, WriteInstanceReforgeCountsPayload<Write>) &&
		       writeRecord(kPendingDropOwnership, WritePendingDropOwnershipPayload<Write>);
	}

}
