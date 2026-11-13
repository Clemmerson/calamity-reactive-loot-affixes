		struct SerializationLoadCursor
		{
			SKSE::SerializationInterface* intfc{ nullptr };
			std::uint32_t length{ 0u };
			std::uint32_t bytesRead{ 0u };
			bool recordOk{ true };

			template <class T>
			bool Read(T& a_value)
			{
				if (!recordOk) {
					return false;
				}

				const auto size = intfc->ReadRecordData(a_value);
				bytesRead += size;
				if (size != sizeof(a_value)) {
					recordOk = false;
					return false;
				}
				return true;
			}

			bool ReadBuffer(void* a_buffer, std::uint32_t a_length)
			{
				if (!recordOk) {
					return false;
				}

				const auto size = intfc->ReadRecordData(a_buffer, a_length);
				bytesRead += size;
				if (size != a_length) {
					recordOk = false;
					return false;
				}
				return true;
			}

			void DrainRemaining(const char* a_context)
			{
				if (bytesRead < length) {
					DrainRecordBytes(intfc, length - bytesRead, a_context);
				}
			}
		};

		[[nodiscard]] bool IsSupportedInstanceAffixSerializationVersion(std::uint32_t a_version) noexcept
		{
			return a_version >= 1u && a_version <= 7u;
		}

	void EventBridge::LoadInstanceAffixesRecord(
		SKSE::SerializationInterface* a_intfc,
		std::uint32_t a_version,
		std::uint32_t a_length)
	{
		if (!IsSupportedInstanceAffixSerializationVersion(a_version)) {
			DrainRecordBytes(a_intfc, a_length, "unsupported-iaxf-version");
			return;
		}

		SerializationLoadCursor cursor{ .intfc = a_intfc, .length = a_length };

		if (a_version == kSerializationVersion) {
			std::uint32_t count = 0;
			if (!cursor.Read(count)) {
				SKSE::log::warn("CalamityAffixes: truncated IAXF v7 record header; skipping.");
				cursor.DrainRemaining("partial-record-recovery");
				return;
			}

			SKSE::log::info("CalamityAffixes: IAXF v7 — co-save contains {} instance entries.", count);
			for (std::uint32_t i = 0; i < count; ++i) {
				RE::FormID baseID = 0;
				std::uint16_t uniqueID = 0;
				std::uint8_t affixCount = 0;
				std::array<std::uint64_t, kMaxAffixesPerItem> tokens{};

				if (!cursor.Read(baseID) || !cursor.Read(uniqueID) || !cursor.Read(affixCount)) {
					break;
				}
				bool tokensOk = true;
				for (std::size_t s = 0; s < kMaxAffixesPerItem; ++s) {
					if (!cursor.Read(tokens[s])) {
						tokensOk = false;
						break;
					}
				}
				if (!tokensOk) {
					break;
				}

				RE::FormID resolvedBaseID = 0;
				if (!a_intfc->ResolveFormID(baseID, resolvedBaseID)) {
					SKSE::log::warn("CalamityAffixes: IAXF v7 — ResolveFormID failed for baseID {:08X}, skipping entry.", baseID);
					continue;
				}

				const auto key = MakeInstanceKey(resolvedBaseID, uniqueID);
				InstanceAffixSlots slots;
				slots.count = std::min<std::uint8_t>(affixCount, static_cast<std::uint8_t>(kMaxAffixesPerItem));
				slots.tokens = tokens;
				_instanceTrackingState.instanceAffixes.emplace(key, slots);
			}
			if (!cursor.recordOk) {
				SKSE::log::warn("CalamityAffixes: truncated IAXF v7 record; recovered {} entries.", _instanceTrackingState.instanceAffixes.size());
				cursor.DrainRemaining("partial-record-recovery");
			}

			SKSE::log::info("CalamityAffixes: IAXF v7 — loaded {} instance entries from co-save.", _instanceTrackingState.instanceAffixes.size());
			return;
		}

		if (a_version == kSerializationVersionV6) {
			std::uint32_t count = 0;
			if (!cursor.Read(count)) {
				SKSE::log::warn("CalamityAffixes: truncated IAXF v6 record header; skipping.");
				cursor.DrainRemaining("partial-record-recovery");
				return;
			}

			for (std::uint32_t i = 0; i < count; ++i) {
				RE::FormID baseID = 0;
				std::uint16_t uniqueID = 0;
				std::uint8_t affixCount = 0;
				std::array<std::uint64_t, 3> legacyTokens{};

				if (!cursor.Read(baseID) || !cursor.Read(uniqueID) || !cursor.Read(affixCount)) {
					break;
				}
				bool tokensOk = true;
				for (std::size_t s = 0; s < legacyTokens.size(); ++s) {
					if (!cursor.Read(legacyTokens[s])) {
						tokensOk = false;
						break;
					}
				}
				if (!tokensOk) {
					break;
				}

				RE::FormID resolvedBaseID = 0;
				if (!a_intfc->ResolveFormID(baseID, resolvedBaseID)) {
					SKSE::log::warn("CalamityAffixes: IAXF v6 — ResolveFormID failed for baseID {:08X}, skipping entry.", baseID);
					continue;
				}

				const auto key = MakeInstanceKey(resolvedBaseID, uniqueID);
				InstanceAffixSlots slots;
				slots.count = std::min<std::uint8_t>(affixCount, static_cast<std::uint8_t>(legacyTokens.size()));
				for (std::uint8_t s = 0; s < slots.count; ++s) {
					slots.tokens[s] = legacyTokens[s];
				}
				_instanceTrackingState.instanceAffixes.emplace(key, slots);
			}
			if (!cursor.recordOk) {
				SKSE::log::warn("CalamityAffixes: truncated IAXF v6 record; recovered {} entries.", _instanceTrackingState.instanceAffixes.size());
				cursor.DrainRemaining("partial-record-recovery");
			}
			return;
		}

		std::uint32_t count = 0;
		if (!cursor.Read(count)) {
			SKSE::log::warn("CalamityAffixes: truncated IAXF v1-v5 record header; skipping.");
			cursor.DrainRemaining("partial-record-recovery");
			return;
		}

		for (std::uint32_t i = 0; i < count; i++) {
			RE::FormID baseID = 0;
			std::uint16_t uniqueID = 0;

			if (!cursor.Read(baseID) || !cursor.Read(uniqueID)) {
				break;
			}

			std::uint64_t token = 0;
			std::uint64_t supplementalToken = 0;
			InstanceRuntimeState state{};
			if (a_version == kSerializationVersionV1) {
				std::uint32_t len = 0;
				if (!cursor.Read(len)) {
					break;
				}

				std::string affixId;
				if (len > kMaxV1AffixIdLength) {
					SKSE::log::error("CalamityAffixes: corrupt v1 save — affixId length {} exceeds limit.", len);
					break;
				}
				affixId.resize(len);
				if (len > 0 && !cursor.ReadBuffer(affixId.data(), len)) {
					break;
				}

				token = affixId.empty() ? 0u : MakeAffixToken(affixId);
			} else {
				if (!cursor.Read(token)) {
					break;
				}
				if (a_version == kSerializationVersionV5 || a_version == kSerializationVersionV4) {
					if (!cursor.Read(supplementalToken)) {
						break;
					}
				}

				if (a_version == kSerializationVersionV5 ||
					a_version == kSerializationVersionV4 ||
					a_version == kSerializationVersionV3) {
					if (!cursor.Read(state.evolutionXp) ||
						!cursor.Read(state.modeCycleCounter) ||
						!cursor.Read(state.modeIndex)) {
						break;
					}
				}
			}

			RE::FormID resolvedBaseID = 0;
			if (!a_intfc->ResolveFormID(baseID, resolvedBaseID)) {
				SKSE::log::warn("CalamityAffixes: IAXF v1-v5 — ResolveFormID failed for baseID {:08X}, skipping entry.", baseID);
				continue;
			}

			const auto key = MakeInstanceKey(resolvedBaseID, uniqueID);
			InstanceAffixSlots slots;
			if (token != 0u) {
				slots.AddToken(token);
			}
			if (supplementalToken != 0u) {
				slots.AddToken(supplementalToken);
			}
			_instanceTrackingState.instanceAffixes.emplace(key, slots);
			if (a_version == kSerializationVersionV5 ||
				a_version == kSerializationVersionV4 ||
				a_version == kSerializationVersionV3) {
				const auto stateToken = (token != 0u) ? token : supplementalToken;
				if (stateToken != 0u) {
					_instanceTrackingState.instanceStates[MakeInstanceStateKey(key, stateToken)] = state;
				}
			}
		}

		if (!cursor.recordOk) {
			SKSE::log::warn("CalamityAffixes: truncated IAXF v1-v5 record; recovered {} entries.", _instanceTrackingState.instanceAffixes.size());
			cursor.DrainRemaining("partial-record-recovery");
		}
	}

	void EventBridge::LoadInstanceRuntimeStatesRecord(
		SKSE::SerializationInterface* a_intfc,
		std::uint32_t a_version,
		std::uint32_t a_length)
	{
		if (a_version != kInstanceRuntimeStateSerializationVersion) {
			DrainRecordBytes(a_intfc, a_length, "unsupported-irst-version");
			return;
		}

		SerializationLoadCursor cursor{ .intfc = a_intfc, .length = a_length };
		std::uint32_t count = 0;
		if (!cursor.Read(count)) {
			SKSE::log::warn("CalamityAffixes: truncated IRST record header; skipping.");
			cursor.DrainRemaining("partial-record-recovery");
			return;
		}

		for (std::uint32_t i = 0; i < count; ++i) {
			RE::FormID baseID = 0;
			std::uint16_t uniqueID = 0;
			std::uint64_t affixToken = 0;
			InstanceRuntimeState state{};
			if (!cursor.Read(baseID) || !cursor.Read(uniqueID) || !cursor.Read(affixToken)) {
				break;
			}
			if (!cursor.Read(state.evolutionXp) || !cursor.Read(state.modeCycleCounter) || !cursor.Read(state.modeIndex)) {
				break;
			}

			RE::FormID resolvedBaseID = 0;
			if (!a_intfc->ResolveFormID(baseID, resolvedBaseID)) {
				SKSE::log::warn("CalamityAffixes: IRST — ResolveFormID failed for baseID {:08X}, skipping entry.", baseID);
				continue;
			}
			if (affixToken == 0u) {
				continue;
			}
			const auto key = MakeInstanceKey(resolvedBaseID, uniqueID);
			_instanceTrackingState.instanceStates[MakeInstanceStateKey(key, affixToken)] = state;
		}
		if (!cursor.recordOk) {
			SKSE::log::warn("CalamityAffixes: truncated IRST record; recovered {} entries.", _instanceTrackingState.instanceStates.size());
			cursor.DrainRemaining("partial-record-recovery");
		}
	}

	void EventBridge::LoadRunewordStateRecord(
		SKSE::SerializationInterface* a_intfc,
		std::uint32_t a_version,
		std::uint32_t a_length)
	{
		if (a_version != kRunewordSerializationVersion) {
			DrainRecordBytes(a_intfc, a_length, "unsupported-rwrd-version");
			return;
		}

		SerializationLoadCursor cursor{ .intfc = a_intfc, .length = a_length };
		RE::FormID selectedBaseID = 0;
		std::uint16_t selectedUniqueID = 0;
		if (!cursor.Read(selectedBaseID) || !cursor.Read(selectedUniqueID) ||
			!cursor.Read(_runewordState.recipeCycleCursor) || !cursor.Read(_runewordState.baseCycleCursor)) {
			SKSE::log::warn("CalamityAffixes: truncated RWRD record header; skipping.");
			cursor.DrainRemaining("partial-record-recovery");
			return;
		}

		std::uint32_t fragmentCount = 0;
		if (!cursor.Read(fragmentCount)) {
			SKSE::log::warn("CalamityAffixes: truncated RWRD fragment count; skipping remainder.");
			cursor.DrainRemaining("partial-record-recovery");
			return;
		}
		for (std::uint32_t i = 0; i < fragmentCount; ++i) {
			std::uint64_t runeToken = 0;
			std::uint32_t amount = 0;
			if (!cursor.Read(runeToken) || !cursor.Read(amount)) {
				break;
			}
			if (runeToken != 0u && amount > 0u) {
				_runewordState.runeFragments[runeToken] = amount;
			}
		}
		if (!cursor.recordOk) {
			SKSE::log::warn("CalamityAffixes: truncated RWRD fragments; recovered {} entries.", _runewordState.runeFragments.size());
			cursor.DrainRemaining("partial-record-recovery");
			return;
		}

		std::uint32_t runewordStateCount = 0;
		if (!cursor.Read(runewordStateCount)) {
			SKSE::log::warn("CalamityAffixes: truncated RWRD state count; skipping remainder.");
			cursor.DrainRemaining("partial-record-recovery");
			return;
		}
		for (std::uint32_t i = 0; i < runewordStateCount; ++i) {
			RE::FormID baseID = 0;
			std::uint16_t uniqueID = 0;
			RunewordInstanceState state{};
			if (!cursor.Read(baseID) || !cursor.Read(uniqueID) ||
				!cursor.Read(state.recipeToken) || !cursor.Read(state.insertedRunes)) {
				break;
			}

			RE::FormID resolvedBaseID = 0;
			if (!a_intfc->ResolveFormID(baseID, resolvedBaseID)) {
				SKSE::log::warn("CalamityAffixes: RWRD — ResolveFormID failed for baseID {:08X}, skipping entry.", baseID);
				continue;
			}

			const auto key = MakeInstanceKey(resolvedBaseID, uniqueID);
			_runewordState.instanceStates[key] = state;
		}
		if (!cursor.recordOk) {
			SKSE::log::warn("CalamityAffixes: truncated RWRD states; recovered {} entries.", _runewordState.instanceStates.size());
			cursor.DrainRemaining("partial-record-recovery");
		}

		if (selectedBaseID != 0 && selectedUniqueID != 0) {
			RE::FormID resolvedSelectedBaseID = 0;
			if (a_intfc->ResolveFormID(selectedBaseID, resolvedSelectedBaseID) && resolvedSelectedBaseID != 0) {
				_runewordState.selectedBaseKey = MakeInstanceKey(resolvedSelectedBaseID, selectedUniqueID);
			}
		}
	}

	void EventBridge::LoadLootEvaluatedRecord(
		SKSE::SerializationInterface* a_intfc,
		std::uint32_t a_version,
		std::uint32_t a_length)
	{
		if (a_version != kLootEvaluatedSerializationVersion) {
			DrainRecordBytes(a_intfc, a_length, "unsupported-lrld-version");
			return;
		}

		SerializationLoadCursor cursor{ .intfc = a_intfc, .length = a_length };
		std::uint32_t count = 0;
		if (!cursor.Read(count)) {
			SKSE::log::warn("CalamityAffixes: truncated LRLD record header; skipping.");
			cursor.DrainRemaining("partial-record-recovery");
			return;
		}

		for (std::uint32_t i = 0; i < count; ++i) {
			RE::FormID baseID = 0;
			std::uint16_t uniqueID = 0;
			if (!cursor.Read(baseID) || !cursor.Read(uniqueID)) {
				break;
			}

			RE::FormID resolvedBaseID = 0;
			if (!a_intfc->ResolveFormID(baseID, resolvedBaseID)) {
				SKSE::log::warn("CalamityAffixes: LRLD — ResolveFormID failed for baseID {:08X}, skipping entry.", baseID);
				continue;
			}

			const auto key = MakeInstanceKey(resolvedBaseID, uniqueID);
			if (key != 0) {
				_lootState.evaluatedInstances.insert(key);
			}
		}
		if (!cursor.recordOk) {
			SKSE::log::warn("CalamityAffixes: truncated LRLD record; recovered {} entries.", _lootState.evaluatedInstances.size());
			cursor.DrainRemaining("partial-record-recovery");
		}
	}

	void EventBridge::LoadLootCurrencyLedgerRecord(
		SKSE::SerializationInterface* a_intfc,
		std::uint32_t a_version,
		std::uint32_t a_length)
	{
		if (a_version != kLootCurrencyLedgerSerializationVersion &&
			a_version != kLootCurrencyLedgerSerializationVersionV1) {
			DrainRecordBytes(a_intfc, a_length, "unsupported-lcld-version");
			return;
		}

		SerializationLoadCursor cursor{ .intfc = a_intfc, .length = a_length };
		std::uint32_t count = 0;
		if (!cursor.Read(count)) {
			SKSE::log::warn("CalamityAffixes: truncated LCLD record header; skipping.");
			cursor.DrainRemaining("partial-record-recovery");
			return;
		}

		for (std::uint32_t i = 0; i < count; ++i) {
			std::uint64_t key = 0;
			if (!cursor.Read(key)) {
				break;
			}
			std::uint32_t dayStamp = 0u;
			if (a_version == kLootCurrencyLedgerSerializationVersion) {
				if (!cursor.Read(dayStamp)) {
					break;
				}
			}
			if (key != 0u) {
				_lootState.currencyRollLedger.emplace(key, dayStamp);
			}
		}
		if (!cursor.recordOk) {
			SKSE::log::warn("CalamityAffixes: truncated LCLD record; recovered {} entries.", _lootState.currencyRollLedger.size());
			cursor.DrainRemaining("partial-record-recovery");
		}
	}

	void EventBridge::LoadCorpseCurrencyRuntimeRecord(
		SKSE::SerializationInterface* a_intfc,
		std::uint32_t a_version,
		std::uint32_t a_length)
	{
		if (a_version != kCorpseCurrencyRuntimeSerializationVersion) {
			DrainRecordBytes(a_intfc, a_length, "unsupported-ccrt-version");
			return;
		}

		SerializationLoadCursor cursor{ .intfc = a_intfc, .length = a_length };
		std::uint32_t runewordFailStreak = 0u;
		std::uint32_t reforgeFailStreak = 0u;
		std::uint32_t count = 0u;
		if (!cursor.Read(runewordFailStreak) ||
			!cursor.Read(reforgeFailStreak) ||
			!cursor.Read(count)) {
			SKSE::log::warn("CalamityAffixes: truncated CCRT record header; using empty corpse currency state.");
			cursor.DrainRemaining("partial-record-recovery");
			return;
		}

		_lootState.runewordFragmentFailStreak =
			std::min(runewordFailStreak, kRunewordFragmentPityFailThreshold);
		_lootState.reforgeOrbFailStreak =
			std::min(reforgeFailStreak, kReforgeOrbPityFailThreshold);

		if (count > kLootCurrencyLedgerMaxEntries) {
			SKSE::log::warn(
				"CalamityAffixes: corrupt CCRT ledger count {} exceeds limit {}; skipping ledger entries.",
				count,
				kLootCurrencyLedgerMaxEntries);
			cursor.DrainRemaining("ccrt-count-limit");
			return;
		}

		for (std::uint32_t i = 0u; i < count; ++i) {
			RE::FormID corpseFormId = 0u;
			std::uint32_t dayStamp = 0u;
			std::uint8_t processedMask = 0u;
			if (!cursor.Read(corpseFormId) ||
				!cursor.Read(dayStamp) ||
				!cursor.Read(processedMask)) {
				break;
			}

			RE::FormID resolvedCorpseFormId = 0u;
			if (!a_intfc->ResolveFormID(corpseFormId, resolvedCorpseFormId) ||
				resolvedCorpseFormId == 0u) {
				continue;
			}

			processedMask &= detail::kCorpseCurrencyAllProcessed;
			if (processedMask == 0u) {
				continue;
			}

			const auto [it, inserted] = _lootState.corpseCurrencyRollLedger.emplace(
				resolvedCorpseFormId,
				LootRuntimeState::CorpseCurrencyLedgerEntry{
					.dayStamp = dayStamp,
					.processedMask = processedMask
				});
			if (!inserted) {
				it->second.dayStamp = std::max(it->second.dayStamp, dayStamp);
				it->second.processedMask |= processedMask;
			}
		}

		if (!cursor.recordOk) {
			SKSE::log::warn(
				"CalamityAffixes: truncated CCRT record; recovered {} corpse entries.",
				_lootState.corpseCurrencyRollLedger.size());
			cursor.DrainRemaining("partial-record-recovery");
		}
	}

	void EventBridge::LoadLootShuffleBagsRecord(
		SKSE::SerializationInterface* a_intfc,
		std::uint32_t a_version,
		std::uint32_t a_length)
	{
		if (a_version != kLootShuffleBagSerializationVersion) {
			DrainRecordBytes(a_intfc, a_length, "unsupported-lsbg-version");
			return;
		}

		SerializationLoadCursor cursor{ .intfc = a_intfc, .length = a_length };
		auto readScalar = [&cursor]<std::unsigned_integral T>(T& a_value) {
			return cursor.Read(a_value);
		};
		auto applyBag = [this](SerializationWire::LootShuffleBagEntry a_entry) {
			if (auto* bag = SerializationLoadState::ResolveShuffleBag(_lootState, a_entry.id)) {
				bag->order.clear();
				bag->order.reserve(a_entry.order.size());
				for (const auto index : a_entry.order) {
					bag->order.push_back(static_cast<std::size_t>(index));
				}
				bag->cursor = std::min<std::size_t>(a_entry.cursor, bag->order.size());
			}
		};
		const auto result = SerializationWire::ReadCurrentLootShuffleBagsPayload(
			readScalar,
			applyBag);
		if (result.status == SerializationWire::CurrentShuffleBagReadStatus::kTruncatedHeader) {
			SKSE::log::warn("CalamityAffixes: truncated LSBG record header; skipping.");
			cursor.DrainRemaining("partial-record-recovery");
			return;
		}
		if (result.status == SerializationWire::CurrentShuffleBagReadStatus::kSizeLimitExceeded) {
			SKSE::log::error(
				"CalamityAffixes: corrupt save — shuffle bag size {} exceeds limit.",
				result.invalidBagSize);
			cursor.recordOk = false;
		}
		if (result.status != SerializationWire::CurrentShuffleBagReadStatus::kComplete) {
			SKSE::log::warn("CalamityAffixes: truncated LSBG record; recovered partial shuffle bags.");
			cursor.DrainRemaining("partial-record-recovery");
		}
	}

	void EventBridge::LoadMigrationFlagsRecord(
		SKSE::SerializationInterface* a_intfc,
		std::uint32_t a_version,
		std::uint32_t a_length)
	{
		if (a_version != kMigrationFlagsVersion) {
			DrainRecordBytes(a_intfc, a_length, "unsupported-mflg-version");
			return;
		}

		SerializationLoadCursor cursor{ .intfc = a_intfc, .length = a_length };
		auto readScalar = [&cursor]<std::unsigned_integral T>(T& a_value) {
			return cursor.Read(a_value);
		};
		std::uint8_t flags = 0u;
		if (!SerializationWire::ReadCurrentMigrationFlagsPayload(readScalar, flags)) {
			SKSE::log::warn("CalamityAffixes: truncated MFLG record; skipping.");
			cursor.DrainRemaining("partial-record-recovery");
			return;
		}
		_miscCurrencyMigrated = (flags & 1u) != 0;
		_miscCurrencyRecovered = (flags & 2u) != 0;
	}

	void EventBridge::LoadInstanceReforgeCountsRecord(
		SKSE::SerializationInterface* a_intfc,
		std::uint32_t a_version,
		std::uint32_t a_length)
	{
		if (a_version != kInstanceReforgeCountSerializationVersion) {
			DrainRecordBytes(a_intfc, a_length, "unsupported-irfc-version");
			return;
		}

		SerializationLoadCursor cursor{ .intfc = a_intfc, .length = a_length };
		auto readScalar = [&cursor]<std::unsigned_integral T>(T& a_value) {
			return cursor.Read(a_value);
		};
		auto apply = [&](const SerializationWire::InstanceReforgeCountEntry& a_entry) {
			RE::FormID resolvedBaseID = 0;
			if (a_entry.count == 0u || !a_intfc->ResolveFormID(a_entry.baseFormId, resolvedBaseID)) {
				return;
			}
			if (const auto key = MakeInstanceKey(resolvedBaseID, a_entry.uniqueId); key != 0) {
				_instanceTrackingState.selectedReforgeCounts[key] = a_entry.count;
			}
		};
		if (!SerializationWire::ReadCurrentInstanceReforgeCountsPayload(readScalar, apply)) {
			SKSE::log::warn(
				"CalamityAffixes: truncated IRFC record; recovered {} reforge counts.",
				_instanceTrackingState.selectedReforgeCounts.size());
			cursor.DrainRemaining("partial-record-recovery");
		}
	}

	void EventBridge::LoadPendingDropOwnershipRecord(
		SKSE::SerializationInterface* a_intfc,
		std::uint32_t a_version,
		std::uint32_t a_length)
	{
		if (a_version < 1u || a_version > kPendingDropOwnershipSerializationVersion) {
			DrainRecordBytes(a_intfc, a_length, "unsupported-pdop-version");
			return;
		}

		SerializationLoadCursor cursor{ .intfc = a_intfc, .length = a_length };
		auto readScalar = [&cursor]<std::unsigned_integral T>(T& a_value) {
			return cursor.Read(a_value);
		};
		auto apply = [&](const SerializationWire::PendingDropOwnershipEntry& a_entry) {
			RE::FormID resolvedOwnerID = 0u;
			RE::FormID resolvedBaseID = 0u;
			if (a_entry.ownerFormId == 0u || a_entry.uniqueId == 0u ||
				a_entry.baseFormId == 0u || a_entry.expiresAtDay == 0u ||
				!a_intfc->ResolveFormID(a_entry.ownerFormId, resolvedOwnerID) ||
				!a_intfc->ResolveFormID(a_entry.baseFormId, resolvedBaseID) ||
				resolvedOwnerID == 0u || resolvedBaseID == 0u) {
				return;
			}
			const auto key = MakeInstanceKey(resolvedOwnerID, a_entry.uniqueId);
			const auto result = TrackPendingDropOwnership(
				_lootState.pendingDropOwnership,
				key,
				resolvedBaseID,
				a_entry.expiresAtDay,
				a_entry.referenceFormId,
				a_entry.referenceIdentityEstablished != 0u);
			if (result == PendingDropTrackResult::kConflict ||
				result == PendingDropTrackResult::kFull) {
				_lootState.pendingDropOwnershipOverflow = true;
				_lootState.pendingDropOwnershipOverflowExpiresAtDay =
					std::max(_lootState.pendingDropOwnershipOverflowExpiresAtDay, a_entry.expiresAtDay);
			}
		};
		const auto result = SerializationWire::ReadCurrentPendingDropOwnershipPayload(readScalar, apply, a_version);
		if (result.status == SerializationWire::PendingDropOwnershipReadStatus::kSizeLimitExceeded) {
			_lootState.pendingDropOwnershipOverflow = true;
			_lootState.pendingDropOwnershipOverflowExpiresAtDay =
				std::max(_lootState.pendingDropOwnershipOverflowExpiresAtDay,
						detail::GetInGameDayStamp() + kPendingDropOwnershipLifetimeDays);
			SKSE::log::error(
				"CalamityAffixes: pending drop record count {} exceeds bound {}; preserving player-owned state until overflow expiry.",
				result.invalidCount,
				kMaxPendingDropOwnership);
			cursor.DrainRemaining("pdop-count-limit");
			return;
		}
		if (result.status != SerializationWire::PendingDropOwnershipReadStatus::kComplete) {
			_lootState.pendingDropOwnershipOverflow = true;
			_lootState.pendingDropOwnershipOverflowExpiresAtDay =
				std::max(_lootState.pendingDropOwnershipOverflowExpiresAtDay,
						detail::GetInGameDayStamp() + kPendingDropOwnershipLifetimeDays);
			SKSE::log::warn("CalamityAffixes: truncated PDOP record; retaining recovered exact handoffs and using bounded orphan protection.");
			cursor.DrainRemaining("partial-record-recovery");
			return;
		}
		_lootState.pendingDropOwnershipOverflow = result.overflow;
		_lootState.pendingDropOwnershipOverflowExpiresAtDay = result.overflowExpiresAtDay;
	}

	void EventBridge::FinalizeLoadedSerializationState()
	{
		SKSE::log::info("CalamityAffixes: Load() — deserialized {} instance entries, {} runtime states.", _instanceTrackingState.instanceAffixes.size(), _instanceTrackingState.instanceStates.size());
		if (!_affixRuntimeState.affixRegistry.affixIndexByToken.empty() && !_affixRuntimeState.affixes.empty()) {
			SanitizeAllTrackedLootInstancesForCurrentLootRules("Serialization.Load");
		}
		// IRFC is read after IAXF; a count whose item did not load (or was just
		// sanitized away) must not price a future item that reuses the key.
		std::erase_if(_instanceTrackingState.selectedReforgeCounts, [this](const auto& a_entry) {
			return !_instanceTrackingState.instanceAffixes.contains(a_entry.first);
		});

		for (const auto& [key, _] : _instanceTrackingState.instanceAffixes) {
			_lootState.evaluatedInstances.insert(key);
		}

		SerializationLoadState::RebuildEvaluatedRecent(_lootState, kLootEvaluatedRecentKeep * 2);
		SerializationLoadState::RebuildCurrencyLedgerRecent(_lootState, kLootCurrencyLedgerMaxEntries);
		SerializationLoadState::RebuildCorpseCurrencyLedgerRecent(_lootState, kLootCurrencyLedgerMaxEntries);
		SerializationLoadState::SanitizeShuffleBagOrders(_affixRuntimeState.affixRegistry, _lootState);

		SanitizeRunewordState();
		RebuildActiveCounts();
	}
