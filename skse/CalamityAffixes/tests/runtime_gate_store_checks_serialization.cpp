#include "runtime_gate_store_checks_common.h"

#include "CalamityAffixes/SerializationCurrentRecordReader.h"
#include "CalamityAffixes/SerializationWireContract.h"

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace RuntimeGateStoreChecks
{
	namespace
	{
		using namespace CalamityAffixes::SerializationWire;

		struct EncodedRecord
		{
			RecordContract contract{};
			std::vector<std::uint8_t> payload{};

			[[nodiscard]] bool operator==(const EncodedRecord&) const noexcept = default;
		};

		template <std::unsigned_integral T>
		void AppendLittleEndian(std::vector<std::uint8_t>& a_out, T a_value)
		{
			for (std::size_t i = 0u; i < sizeof(T); ++i) {
				a_out.push_back(static_cast<std::uint8_t>(a_value & static_cast<T>(0xFFu)));
				a_value = static_cast<T>(a_value >> 8u);
			}
		}

		[[nodiscard]] std::vector<EncodedRecord> EncodeCurrentRecords(const CurrentSaveSnapshot& a_snapshot)
		{
			std::vector<EncodedRecord> records;
			records.reserve(kCurrentRecordSequence.size());
			EncodedRecord* current = nullptr;
			auto openRecord = [&](std::uint32_t a_type, std::uint32_t a_version) {
				records.push_back(EncodedRecord{ .contract = { a_type, a_version } });
				current = &records.back();
				return true;
			};
			auto write = [&]<std::unsigned_integral T>(T a_value) {
				if (!current) {
					return false;
				}
				AppendLittleEndian(current->payload, a_value);
				return true;
			};
			if (!WriteCurrentRecords(a_snapshot, openRecord, write)) {
				return {};
			}
			return records;
		}

		class WireReader
		{
		public:
			explicit WireReader(std::span<const std::uint8_t> a_bytes) noexcept :
				_bytes(a_bytes)
			{}

			template <std::unsigned_integral T>
			bool Read(T& a_out) noexcept
			{
				if (_bytes.size() - _cursor < sizeof(T)) {
					return false;
				}
				T value = 0u;
				for (std::size_t i = 0u; i < sizeof(T); ++i) {
					value |= static_cast<T>(static_cast<T>(_bytes[_cursor + i]) << (i * 8u));
				}
				_cursor += sizeof(T);
				a_out = value;
				return true;
			}

			[[nodiscard]] bool AtEnd() const noexcept { return _cursor == _bytes.size(); }

		private:
			std::span<const std::uint8_t> _bytes{};
			std::size_t _cursor{ 0u };
		};

		[[nodiscard]] int HexNibble(char a_value) noexcept
		{
			if (a_value >= '0' && a_value <= '9') {
				return a_value - '0';
			}
			if (a_value >= 'a' && a_value <= 'f') {
				return 10 + (a_value - 'a');
			}
			return -1;
		}

		[[nodiscard]] std::vector<std::uint8_t> ParseGoldenHex(std::string_view a_hex)
		{
			if ((a_hex.size() % 2u) != 0u) {
				return {};
			}
			std::vector<std::uint8_t> bytes;
			bytes.reserve(a_hex.size() / 2u);
			for (std::size_t i = 0u; i < a_hex.size(); i += 2u) {
				const auto high = HexNibble(a_hex[i]);
				const auto low = HexNibble(a_hex[i + 1u]);
				if (high < 0 || low < 0) {
					return {};
				}
				bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
			}
			return bytes;
		}

		[[nodiscard]] CurrentSaveSnapshot BuildSerializationFixture()
		{
			CurrentSaveSnapshot fixture{};
			fixture.instanceAffixes.push_back(InstanceAffixEntry{
				.baseFormId = 0x11223344u,
				.uniqueId = 0x5566u,
				.affixCount = 2u,
				.tokens = {
					0x0102030405060708ull,
					0x1112131415161718ull,
					0x2122232425262728ull,
					0x3132333435363738ull,
				},
			});
			fixture.instanceRuntimeStates.push_back(InstanceRuntimeStateEntry{
				.baseFormId = 0xA1B2C3D4u,
				.uniqueId = 0xE5F6u,
				.affixToken = 0x4142434445464748ull,
				.evolutionXp = 0x10203040u,
				.modeCycleCounter = 0x50607080u,
				.modeIndex = 0x90A0B0C0u,
			});

			fixture.selectedRunewordBaseFormId = 0x89ABCDEFu;
			fixture.selectedRunewordUniqueId = 0x1357u;
			fixture.runewordRecipeCycleCursor = 0x2468ACE0u;
			fixture.runewordBaseCycleCursor = 0x0BADF00Du;
			fixture.runewordFragments.push_back({
				.runeToken = 0x0101010102020202ull,
				.amount = 0x03030303u,
			});
			fixture.runewordInstances.push_back({
				.baseFormId = 0xCAFEBABEu,
				.uniqueId = 0xBEEFu,
				.recipeToken = 0x1122334455667788ull,
				.insertedRunes = 3u,
			});

			fixture.lootEvaluated.push_back({ .baseFormId = 0x10293847u, .uniqueId = 0xA1B2u });
			fixture.lootCurrencyLedger.push_back({
				.instanceKey = 0xFFEEDDCCBBAA9988ull,
				.dayStamp = 0x76543210u,
			});

			fixture.runewordFragmentFailStreak = 7u;
			fixture.reforgeOrbFailStreak = 11u;
			fixture.corpseCurrencyLedger.push_back({
				.corpseFormId = 0x0A0B0C0Du,
				.dayStamp = 0x01020304u,
				.processedMask = 3u,
			});

			fixture.lootShuffleBags = {
				{ .id = 0u, .cursor = 1u, .order = { 9u, 4u } },
				{ .id = 1u, .cursor = 0u, .order = {} },
				{ .id = 2u, .cursor = 1u, .order = { 7u } },
				{ .id = 3u, .cursor = 0u, .order = {} },
				{ .id = 4u, .cursor = 0u, .order = {} },
				{ .id = 5u, .cursor = 2u, .order = { 3u, 1u, 8u } },
			};
			fixture.migrationFlags = 3u;
			fixture.instanceReforgeCounts.push_back({
				.baseFormId = 0x0F1E2D3Cu,
				.uniqueId = 0x4B5Au,
				.count = 4u,
			});
			// PDOP v3 is append-only relative to the nine established records.
			// Include an owned object with a known reference identity so the
			// golden bytes verify the full current wire layout.
			fixture.pendingDropOwnershipOverflow = true;
			fixture.pendingDropOwnershipOverflowExpiresAtDay = 0x01020304u;
			fixture.pendingDropOwnership.push_back({
				.ownerFormId = 0x11223344u,
				.uniqueId = 0x5566u,
				.baseFormId = 0xAABBCCDDu,
				.expiresAtDay = 0x05060708u,
				.referenceFormId = 0x10203040u,
				.referenceIdentityEstablished = 1u,
			});
			return fixture;
		}

		[[nodiscard]] bool DecodeCurrentFixture(
			std::span<const EncodedRecord> a_records,
			CurrentSaveSnapshot& a_out)
		{
			if (a_records.size() != kCurrentRecordSequence.size()) {
				return false;
			}

			CurrentSaveSnapshot decoded{};
			{
				WireReader reader(a_records[0].payload);
				std::uint32_t count = 0u;
				if (!reader.Read(count) || count > 64u) {
					return false;
				}
				for (std::uint32_t i = 0u; i < count; ++i) {
					InstanceAffixEntry entry{};
					if (!reader.Read(entry.baseFormId) || !reader.Read(entry.uniqueId) || !reader.Read(entry.affixCount)) {
						return false;
					}
					for (auto& token : entry.tokens) {
						if (!reader.Read(token)) {
							return false;
						}
					}
					decoded.instanceAffixes.push_back(entry);
				}
				if (!reader.AtEnd()) {
					return false;
				}
			}

			{
				WireReader reader(a_records[1].payload);
				std::uint32_t count = 0u;
				if (!reader.Read(count) || count > 64u) {
					return false;
				}
				for (std::uint32_t i = 0u; i < count; ++i) {
					InstanceRuntimeStateEntry entry{};
					if (!reader.Read(entry.baseFormId) || !reader.Read(entry.uniqueId) ||
						!reader.Read(entry.affixToken) || !reader.Read(entry.evolutionXp) ||
						!reader.Read(entry.modeCycleCounter) || !reader.Read(entry.modeIndex)) {
						return false;
					}
					decoded.instanceRuntimeStates.push_back(entry);
				}
				if (!reader.AtEnd()) {
					return false;
				}
			}

			{
				WireReader reader(a_records[2].payload);
				std::uint32_t fragmentCount = 0u;
				if (!reader.Read(decoded.selectedRunewordBaseFormId) ||
					!reader.Read(decoded.selectedRunewordUniqueId) ||
					!reader.Read(decoded.runewordRecipeCycleCursor) ||
					!reader.Read(decoded.runewordBaseCycleCursor) ||
					!reader.Read(fragmentCount) || fragmentCount > 64u) {
					return false;
				}
				for (std::uint32_t i = 0u; i < fragmentCount; ++i) {
					RunewordFragmentEntry entry{};
					if (!reader.Read(entry.runeToken) || !reader.Read(entry.amount)) {
						return false;
					}
					decoded.runewordFragments.push_back(entry);
				}

				std::uint32_t stateCount = 0u;
				if (!reader.Read(stateCount) || stateCount > 64u) {
					return false;
				}
				for (std::uint32_t i = 0u; i < stateCount; ++i) {
					RunewordInstanceEntry entry{};
					if (!reader.Read(entry.baseFormId) || !reader.Read(entry.uniqueId) ||
						!reader.Read(entry.recipeToken) || !reader.Read(entry.insertedRunes)) {
						return false;
					}
					decoded.runewordInstances.push_back(entry);
				}
				if (!reader.AtEnd()) {
					return false;
				}
			}

			{
				WireReader reader(a_records[3].payload);
				std::uint32_t count = 0u;
				if (!reader.Read(count) || count > 64u) {
					return false;
				}
				for (std::uint32_t i = 0u; i < count; ++i) {
					LootEvaluatedEntry entry{};
					if (!reader.Read(entry.baseFormId) || !reader.Read(entry.uniqueId)) {
						return false;
					}
					decoded.lootEvaluated.push_back(entry);
				}
				if (!reader.AtEnd()) {
					return false;
				}
			}

			{
				WireReader reader(a_records[4].payload);
				std::uint32_t count = 0u;
				if (!reader.Read(count) || count > 64u) {
					return false;
				}
				for (std::uint32_t i = 0u; i < count; ++i) {
					LootCurrencyLedgerEntry entry{};
					if (!reader.Read(entry.instanceKey) || !reader.Read(entry.dayStamp)) {
						return false;
					}
					decoded.lootCurrencyLedger.push_back(entry);
				}
				if (!reader.AtEnd()) {
					return false;
				}
			}

			{
				WireReader reader(a_records[5].payload);
				std::uint32_t count = 0u;
				if (!reader.Read(decoded.runewordFragmentFailStreak) ||
					!reader.Read(decoded.reforgeOrbFailStreak) ||
					!reader.Read(count) || count > 64u) {
					return false;
				}
				for (std::uint32_t i = 0u; i < count; ++i) {
					CorpseCurrencyLedgerEntry entry{};
					if (!reader.Read(entry.corpseFormId) || !reader.Read(entry.dayStamp) ||
						!reader.Read(entry.processedMask)) {
						return false;
					}
					decoded.corpseCurrencyLedger.push_back(entry);
				}
				if (!reader.AtEnd()) {
					return false;
				}
			}

			{
				WireReader reader(a_records[6].payload);
				auto readScalar = [&reader]<std::unsigned_integral T>(T& a_value) {
					return reader.Read(a_value);
				};
				auto applyBag = [&decoded](LootShuffleBagEntry a_entry) {
					decoded.lootShuffleBags.push_back(std::move(a_entry));
				};
				const auto result = ReadCurrentLootShuffleBagsPayload(readScalar, applyBag);
				if (result.status != CurrentShuffleBagReadStatus::kComplete || !reader.AtEnd()) {
					return false;
				}
			}

			{
				WireReader reader(a_records[7].payload);
				auto readScalar = [&reader]<std::unsigned_integral T>(T& a_value) {
					return reader.Read(a_value);
				};
				if (!ReadCurrentMigrationFlagsPayload(readScalar, decoded.migrationFlags) || !reader.AtEnd()) {
					return false;
				}
			}

			{
				WireReader reader(a_records[8].payload);
				auto readScalar = [&reader]<std::unsigned_integral T>(T& a_value) {
					return reader.Read(a_value);
				};
				auto applyCount = [&decoded](const InstanceReforgeCountEntry& a_entry) {
					decoded.instanceReforgeCounts.push_back(a_entry);
				};
				if (!ReadCurrentInstanceReforgeCountsPayload(readScalar, applyCount) || !reader.AtEnd()) {
					return false;
				}
			}

			{
				WireReader reader(a_records[9].payload);
				auto readScalar = [&reader]<std::unsigned_integral T>(T& a_value) {
					return reader.Read(a_value);
				};
				auto applyOwnership = [&decoded](PendingDropOwnershipEntry a_entry) {
					decoded.pendingDropOwnership.push_back(a_entry);
				};
				const auto result = ReadCurrentPendingDropOwnershipPayload(
					readScalar, applyOwnership, a_records[9].contract.version);
				if (result.status != PendingDropOwnershipReadStatus::kComplete || !reader.AtEnd()) {
					return false;
				}
				decoded.pendingDropOwnershipOverflow = result.overflow;
				decoded.pendingDropOwnershipOverflowExpiresAtDay = result.overflowExpiresAtDay;
			}

			a_out = std::move(decoded);
			return true;
		}
	}

	bool CheckSerializationWireContract()
	{
		using namespace CalamityAffixes::SerializationWire;

		constexpr std::array<RecordContract, 10> kGoldenContracts{
			RecordContract{ 0x49415846u, 7u },
			RecordContract{ 0x49525354u, 1u },
			RecordContract{ 0x52575244u, 1u },
			RecordContract{ 0x4C524C44u, 1u },
			RecordContract{ 0x4C434C44u, 2u },
			RecordContract{ 0x43435254u, 1u },
			RecordContract{ 0x4C534247u, 2u },
			RecordContract{ 0x4D464C47u, 1u },
			RecordContract{ 0x49524643u, 1u },
			RecordContract{ 0x50444F50u, 3u }, // PDOP: appended pending drop ownership
		};
		constexpr std::array<std::string_view, 10> kGoldenPayloadHex{
			"01000000443322116655020807060504030201181716151413121128272625242322213837363534333231",
			"01000000d4c3b2a1f6e548474645444342414030201080706050c0b0a090",
			"efcdab895713e0ac68240df0ad0b0100000002020202010101010303030301000000bebafecaefbe887766554433221103000000",
			"0100000047382910b2a1",
			"010000008899aabbccddeeff10325476",
			"070000000b000000010000000d0c0b0a0403020103",
			"06000100000002000000090000000400000001000000000000000002010000000100000007000000030000000000000000040000000000000000050200000003000000030000000100000008000000",
			"03",
			"010000003c2d1e0f5a4b04",
			"010403020101000000443322116655ddccbbaa080706054030201001",
		};

		if (kCurrentRecordSequence != kGoldenContracts) {
			std::cerr << "serialization_wire_contract: record type/version sequence changed\n";
			return false;
		}

		const auto fixture = BuildSerializationFixture();
		const auto encoded = EncodeCurrentRecords(fixture);
		if (encoded.size() != kGoldenContracts.size()) {
			std::cerr << "serialization_wire_contract: current writer did not emit all ten records\n";
			return false;
		}
		for (std::size_t i = 0u; i < encoded.size(); ++i) {
			if (encoded[i].contract != kGoldenContracts[i] ||
				encoded[i].payload != ParseGoldenHex(kGoldenPayloadHex[i])) {
				std::cerr << "serialization_wire_contract: golden payload mismatch at record " << i << '\n';
				return false;
			}
		}

		CurrentSaveSnapshot decoded{};
		if (!DecodeCurrentFixture(encoded, decoded) || decoded != fixture) {
			std::cerr << "serialization_wire_contract: current records did not round-trip\n";
			return false;
		}
		if (EncodeCurrentRecords(decoded) != encoded) {
			std::cerr << "serialization_wire_contract: re-encoded bytes changed after round-trip\n";
			return false;
		}

		auto maxSlotFixture = fixture;
		maxSlotFixture.instanceAffixes.front().affixCount = 4u;
		const auto maxSlotEncoded = EncodeCurrentRecords(maxSlotFixture);
		CurrentSaveSnapshot maxSlotDecoded{};
		if (!DecodeCurrentFixture(maxSlotEncoded, maxSlotDecoded) ||
			maxSlotDecoded != maxSlotFixture ||
			maxSlotDecoded.instanceAffixes.front().affixCount != 4u) {
			std::cerr << "serialization_wire_contract: four-token item did not round-trip\n";
			return false;
		}

		auto truncatedShufflePayload = encoded[6].payload;
		truncatedShufflePayload.pop_back();
		WireReader truncatedShuffleReader(truncatedShufflePayload);
		auto readTruncatedShuffle = [&truncatedShuffleReader]<std::unsigned_integral T>(T& a_value) {
			return truncatedShuffleReader.Read(a_value);
		};
		std::vector<LootShuffleBagEntry> recoveredShuffleBags;
		auto recoverShuffleBag = [&recoveredShuffleBags](LootShuffleBagEntry a_entry) {
			recoveredShuffleBags.push_back(std::move(a_entry));
		};
		const auto truncatedShuffleResult = ReadCurrentLootShuffleBagsPayload(
			readTruncatedShuffle,
			recoverShuffleBag);
		if (truncatedShuffleResult.status != CurrentShuffleBagReadStatus::kTruncatedPayload ||
			recoveredShuffleBags.size() != fixture.lootShuffleBags.size() - 1u) {
			std::cerr << "serialization_wire_contract: production LSBG reader lost partial-recovery semantics\n";
			return false;
		}

		std::vector<std::uint8_t> oversizedShufflePayload;
		AppendLittleEndian(oversizedShufflePayload, std::uint8_t{ 1u });
		AppendLittleEndian(oversizedShufflePayload, std::uint8_t{ 0u });
		AppendLittleEndian(oversizedShufflePayload, std::uint32_t{ 0u });
		AppendLittleEndian(oversizedShufflePayload, kMaxCurrentShuffleBagSize + 1u);
		WireReader oversizedShuffleReader(oversizedShufflePayload);
		auto readOversizedShuffle = [&oversizedShuffleReader]<std::unsigned_integral T>(T& a_value) {
			return oversizedShuffleReader.Read(a_value);
		};
		bool oversizedShuffleApplied = false;
		auto rejectOversizedShuffle = [&oversizedShuffleApplied](LootShuffleBagEntry) {
			oversizedShuffleApplied = true;
		};
		const auto oversizedShuffleResult = ReadCurrentLootShuffleBagsPayload(
			readOversizedShuffle,
			rejectOversizedShuffle);
		if (oversizedShuffleResult.status != CurrentShuffleBagReadStatus::kSizeLimitExceeded ||
			oversizedShuffleResult.invalidBagSize != kMaxCurrentShuffleBagSize + 1u ||
			oversizedShuffleApplied) {
			std::cerr << "serialization_wire_contract: production LSBG reader lost its size limit\n";
			return false;
		}

		WireReader truncatedMigrationReader(std::span<const std::uint8_t>{});
		auto readTruncatedMigration = [&truncatedMigrationReader]<std::unsigned_integral T>(T& a_value) {
			return truncatedMigrationReader.Read(a_value);
		};
		std::uint8_t migrationFlags = 0xA5u;
		if (ReadCurrentMigrationFlagsPayload(readTruncatedMigration, migrationFlags) || migrationFlags != 0xA5u) {
			std::cerr << "serialization_wire_contract: production MFLG reader accepted a truncated payload\n";
			return false;
		}

		auto truncatedReforgePayload = encoded[8].payload;
		truncatedReforgePayload.pop_back();
		WireReader truncatedReforgeReader(truncatedReforgePayload);
		auto readTruncatedReforge = [&truncatedReforgeReader]<std::unsigned_integral T>(T& a_value) {
			return truncatedReforgeReader.Read(a_value);
		};
		std::size_t recoveredReforgeCounts = 0u;
		auto countRecovered = [&recoveredReforgeCounts](const InstanceReforgeCountEntry&) { ++recoveredReforgeCounts; };
		if (ReadCurrentInstanceReforgeCountsPayload(readTruncatedReforge, countRecovered) || recoveredReforgeCounts != 0u) {
			std::cerr << "serialization_wire_contract: production IRFC reader accepted a truncated entry\n";
			return false;
		}

		// PDOP v3: do not admit a partial entry, including a truncated
		// reference-identity flag, and do not apply an oversized entry set.
		auto truncatedOwnershipPayload = encoded[9].payload;
		truncatedOwnershipPayload.pop_back();
		WireReader truncatedOwnershipReader(truncatedOwnershipPayload);
		auto readTruncatedOwnership = [&truncatedOwnershipReader]<std::unsigned_integral T>(T& a_value) {
			return truncatedOwnershipReader.Read(a_value);
		};
		std::size_t recoveredOwnershipEntries = 0u;
		auto applyTruncatedOwnership = [&recoveredOwnershipEntries](PendingDropOwnershipEntry) {
			++recoveredOwnershipEntries;
		};
		const auto truncatedOwnershipResult = ReadCurrentPendingDropOwnershipPayload(
			readTruncatedOwnership, applyTruncatedOwnership, 3u);
		if (truncatedOwnershipResult.status != PendingDropOwnershipReadStatus::kTruncated ||
			recoveredOwnershipEntries != 0u) {
			std::cerr << "serialization_wire_contract: production PDOP v3 reader accepted truncated ownership\n";
			return false;
		}

		std::vector<std::uint8_t> oversizedOwnershipPayload;
		AppendLittleEndian(oversizedOwnershipPayload, std::uint8_t{ 0u });
		AppendLittleEndian(oversizedOwnershipPayload, std::uint32_t{ 0u });
		AppendLittleEndian(oversizedOwnershipPayload, kMaxPendingDropOwnershipEntries + 1u);
		WireReader oversizedOwnershipReader(oversizedOwnershipPayload);
		auto readOversizedOwnership = [&oversizedOwnershipReader]<std::unsigned_integral T>(T& a_value) {
			return oversizedOwnershipReader.Read(a_value);
		};
		bool oversizedOwnershipApplied = false;
		auto rejectOversizedOwnership = [&oversizedOwnershipApplied](PendingDropOwnershipEntry) {
			oversizedOwnershipApplied = true;
		};
		const auto oversizedOwnershipResult = ReadCurrentPendingDropOwnershipPayload(
			readOversizedOwnership, rejectOversizedOwnership, 3u);
		if (oversizedOwnershipResult.status != PendingDropOwnershipReadStatus::kSizeLimitExceeded ||
			oversizedOwnershipResult.invalidCount != kMaxPendingDropOwnershipEntries + 1u ||
			oversizedOwnershipApplied) {
			std::cerr << "serialization_wire_contract: production PDOP v3 reader lost its size limit\n";
			return false;
		}

		auto truncated = encoded;
		truncated[0].payload.pop_back();
		if (DecodeCurrentFixture(truncated, decoded)) {
			std::cerr << "serialization_wire_contract: truncated record was accepted\n";
			return false;
		}

		std::size_t openCalls = 0u;
		std::size_t writeCalls = 0u;
		auto openRecord = [&](std::uint32_t, std::uint32_t) {
			++openCalls;
			return true;
		};
		auto failFirstWrite = [&]<std::unsigned_integral T>(T) {
			++writeCalls;
			return false;
		};
		if (WriteCurrentRecords(fixture, openRecord, failFirstWrite) || openCalls != 1u || writeCalls != 1u) {
			std::cerr << "serialization_wire_contract: writer failure did not stop the record stream\n";
			return false;
		}

		return true;
	}
}
