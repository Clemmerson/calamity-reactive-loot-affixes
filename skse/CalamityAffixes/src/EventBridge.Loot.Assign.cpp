#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/LootSlotSanitizer.h"
#include "CalamityAffixes/LootUiGuards.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace CalamityAffixes
{
	namespace
	{
		[[nodiscard]] std::string_view TrimLeadingAsciiWhitespaceView(std::string_view a_text) noexcept
		{
			std::size_t pos = 0;
			while (pos < a_text.size()) {
				const unsigned char ch = static_cast<unsigned char>(a_text[pos]);
				if (!std::isspace(ch)) {
					break;
				}
				++pos;
			}
			return a_text.substr(pos);
		}

		void TrimTrailingAsciiWhitespaceInPlace(std::string& a_text)
		{
			while (!a_text.empty()) {
				const unsigned char ch = static_cast<unsigned char>(a_text.back());
				if (!std::isspace(ch)) {
					break;
				}
				a_text.pop_back();
			}
		}

		[[nodiscard]] bool IsLikelyDynamicTemperSuffix(std::string_view a_suffix) noexcept
		{
			const auto trimmed = TrimLeadingAsciiWhitespaceView(a_suffix);
			if (trimmed.size() < 3 || trimmed.front() != '(' || trimmed.back() != ')') {
				return false;
			}
			if (trimmed.find('(', 1) != std::string_view::npos) {
				return false;
			}
			if (trimmed.rfind(')') != trimmed.size() - 1) {
				return false;
			}
			return true;
		}

		void StripTrailingRepeatedSuffix(std::string& a_text, std::string_view a_suffix)
		{
			if (a_text.empty() || a_suffix.empty()) {
				return;
			}

			auto eraseIfEndsWith = [&](std::string_view a_candidate) {
				if (a_candidate.empty() || a_text.size() < a_candidate.size()) {
					return false;
				}
				const std::string_view tail(a_text.data() + (a_text.size() - a_candidate.size()), a_candidate.size());
				if (tail != a_candidate) {
					return false;
				}
				a_text.erase(a_text.size() - a_candidate.size());
				return true;
			};

			const auto trimmedSuffix = TrimLeadingAsciiWhitespaceView(a_suffix);
			bool changed = false;
			while (eraseIfEndsWith(a_suffix) || eraseIfEndsWith(trimmedSuffix)) {
				changed = true;
			}

			if (changed) {
				TrimTrailingAsciiWhitespaceInPlace(a_text);
			}
		}

	}

	bool EventBridge::TryClearStaleLootDisplayName(
		RE::InventoryEntryData* a_entry,
		RE::ExtraDataList* a_xList,
		bool a_allowTrailingMarkerClear)
	{
		if (!a_entry || !a_entry->object || !a_xList) {
			return false;
		}

		auto* text = a_xList->GetExtraTextDisplayData();
		if (!text) {
			return false;
		}

		const auto* name = text->GetDisplayName(a_entry->object, 0);
		if (!name || name[0] == '\0') {
			return false;
		}

		const bool hasLeadingMarker = HasLeadingLootStarPrefix(name);
		const bool hasTrailingMarker = HasTrailingLootStarMarker(name);
		if (!hasLeadingMarker && !(a_allowTrailingMarkerClear && hasTrailingMarker)) {
			return false;
		}

		std::string cleaned;
		if (a_allowTrailingMarkerClear && hasTrailingMarker) {
			cleaned = std::string(StripLootStarMarkers(name));
		} else {
			cleaned = std::string(StripLeadingLootStarPrefix(name));
		}
		if (cleaned.empty()) {
			const char* objectNameRaw = a_entry->object->GetName();
			cleaned = objectNameRaw ? objectNameRaw : "";
		}
		if (cleaned.empty() || cleaned == name) {
			return false;
		}

		text->SetName(cleaned.c_str());
		return true;
	}

	void EventBridge::CleanupInvalidLootInstance(
		RE::InventoryEntryData* a_entry,
		RE::ExtraDataList* a_xList,
		std::uint64_t a_key,
		std::string_view a_reason)
	{
		const auto erasedAffixCount = _instanceTrackingState.instanceAffixes.erase(a_key);
		_instanceTrackingState.instanceBaseObjects.erase(a_key);
		_instanceTrackingState.selectedReforgeCounts.erase(a_key);
		EraseInstanceRuntimeStates(a_key);
		ForgetLootEvaluatedInstance(a_key);
		ForgetLootPreviewSlots(a_key);
		_runewordState.instanceStates.erase(a_key);
		if (_runewordState.selectedBaseKey && *_runewordState.selectedBaseKey == a_key) {
			_runewordState.selectedBaseKey.reset();
		}

		const bool renamed = TryClearStaleLootDisplayName(a_entry, a_xList, true);
		if (_loot.debugLog && (erasedAffixCount > 0 || renamed)) {
			SKSE::log::debug(
				"CalamityAffixes: cleaned invalid loot instance (key={:016X}, reason={}, removedAffixes={}, renamed={}).",
				a_key,
				a_reason,
				erasedAffixCount,
				renamed);
		}
	}

	bool EventBridge::ShouldKeepTrackedLootAffixToken(std::uint64_t a_token) const
	{
		const auto idxIt = _affixRuntimeState.affixRegistry.affixIndexByToken.find(a_token);
		if (idxIt == _affixRuntimeState.affixRegistry.affixIndexByToken.end() || idxIt->second >= _affixRuntimeState.affixes.size()) {
			if (_loot.debugLog) {
				SKSE::log::warn("CalamityAffixes: cleanup skipping unknown affix token {:016X}.", a_token);
			}
			return false;
		}

		if (_loot.stripTrackedSuffixSlots && _affixRuntimeState.affixes[idxIt->second].slot == AffixSlot::kSuffix) {
			return false;
		}

		return true;
	}

	bool EventBridge::InstanceAffixSlotsEqual(
		const InstanceAffixSlots& a_lhs,
		const InstanceAffixSlots& a_rhs) noexcept
	{
		if (a_lhs.count != a_rhs.count) {
			return false;
		}

		for (std::uint8_t i = 0; i < a_lhs.count; ++i) {
			if (a_lhs.tokens[i] != a_rhs.tokens[i]) {
				return false;
			}
		}

		return true;
	}

	void EventBridge::ApplySanitizedInstanceAffixSlots(
		std::uint64_t a_instanceKey,
		InstanceAffixSlots& a_slots,
		const InstanceAffixSlots& a_sanitizedSlots,
		const std::array<std::uint64_t, kMaxAffixesPerItem>& a_removedTokens,
		std::uint8_t a_removedCount)
	{
		a_slots = a_sanitizedSlots;
		for (std::uint8_t i = 0; i < a_removedCount; ++i) {
			const auto removedToken = a_removedTokens[i];
			if (removedToken == 0u || a_slots.HasToken(removedToken)) {
				continue;
			}
			_instanceTrackingState.instanceStates.erase(MakeInstanceStateKey(a_instanceKey, removedToken));
		}

		if (a_slots.count == 0) {
			EraseInstanceRuntimeStates(a_instanceKey);
		}
	}

	bool EventBridge::SanitizeInstanceAffixSlotsForCurrentLootRules(
		std::uint64_t a_instanceKey,
		InstanceAffixSlots& a_slots,
		std::string_view a_context)
	{
		if (a_slots.count == 0) {
			a_slots.Clear();
			return false;
		}

		std::array<std::uint64_t, kMaxAffixesPerItem> removedTokens{};
		std::uint8_t removedCount = 0;
		const auto sanitized = detail::BuildSanitizedInstanceAffixSlots(
			a_slots,
			[&](std::uint64_t a_token) {
				return ShouldKeepTrackedLootAffixToken(a_token);
			},
			&removedTokens,
			&removedCount);

		if (InstanceAffixSlotsEqual(a_slots, sanitized)) {
			return false;
		}

		ApplySanitizedInstanceAffixSlots(a_instanceKey, a_slots, sanitized, removedTokens, removedCount);

		if (_loot.debugLog) {
			SKSE::log::debug(
				"CalamityAffixes: sanitized mapped instance slots (context={}, key={:016X}, removed={}, remaining={}).",
				a_context,
				a_instanceKey,
				removedCount,
				a_slots.count);
		}
		return true;
	}

	bool EventBridge::SanitizeTrackedLootInstanceForCurrentLootRules(
		std::remove_reference_t<decltype(_instanceTrackingState.instanceAffixes)>::iterator& a_it,
		std::string_view a_context,
		std::uint32_t& a_sanitizedInstances,
		std::uint32_t& a_erasedInstances)
	{
		auto& slots = a_it->second;
		const bool changed = SanitizeInstanceAffixSlotsForCurrentLootRules(a_it->first, slots, a_context);
		if (changed) {
			++a_sanitizedInstances;
		}

		if (slots.count == 0) {
			ForgetLootEvaluatedInstance(a_it->first);
			_instanceTrackingState.instanceBaseObjects.erase(a_it->first);
			_instanceTrackingState.selectedReforgeCounts.erase(a_it->first);
			a_it = _instanceTrackingState.instanceAffixes.erase(a_it);
			++a_erasedInstances;
			return true;
		}

		++a_it;
		return false;
	}

	void EventBridge::LogTrackedLootSanitizationSummary(
		std::string_view a_context,
		std::uint32_t a_sanitizedInstances,
		std::uint32_t a_erasedInstances) const
	{
		if (!_loot.debugLog || (a_sanitizedInstances == 0u && a_erasedInstances == 0u)) {
			return;
		}

		SKSE::log::debug(
			"CalamityAffixes: sanitized tracked loot instances (context={}, changed={}, erased={}).",
			a_context,
			a_sanitizedInstances,
			a_erasedInstances);
	}

	void EventBridge::SanitizeAllTrackedLootInstancesForCurrentLootRules(std::string_view a_context)
	{
		if (_affixRuntimeState.affixes.empty() || _affixRuntimeState.affixRegistry.affixIndexByToken.empty()) {
			return;
		}

		std::uint32_t sanitizedInstances = 0u;
		std::uint32_t erasedInstances = 0u;
		for (auto it = _instanceTrackingState.instanceAffixes.begin(); it != _instanceTrackingState.instanceAffixes.end();) {
			(void)SanitizeTrackedLootInstanceForCurrentLootRules(it, a_context, sanitizedInstances, erasedInstances);
		}

		LogTrackedLootSanitizationSummary(a_context, sanitizedInstances, erasedInstances);
	}

	[[nodiscard]] std::string EventBridge::ResolveStoredLootDisplayBaseName(
		RE::InventoryEntryData* a_entry,
		RE::ExtraDataList* a_xList,
		std::string* a_outStoredCustomName) const
	{
		if (!a_entry || !a_entry->object) {
			return {};
		}

		const char* currentRaw = a_xList ? a_xList->GetDisplayName(a_entry->object) : nullptr;
		const std::string currentName = currentRaw ? currentRaw : "";
		auto* text = a_xList ? a_xList->GetExtraTextDisplayData() : nullptr;

		std::string storedCustomName = currentName;
		std::size_t storedCustomNameLength = 0;
		if (text && text->IsPlayerSet()) {
			const char* storedRaw = text->displayName.c_str();
			storedCustomName = storedRaw ? storedRaw : "";
			storedCustomNameLength = static_cast<std::size_t>(text->customNameLength);
			if (storedCustomNameLength > 0 && storedCustomNameLength <= storedCustomName.size()) {
				storedCustomName.resize(storedCustomNameLength);
			}

			if (storedCustomNameLength > 0 && storedCustomNameLength <= currentName.size()) {
				const std::string_view temperSuffix(
					currentName.data() + storedCustomNameLength,
					currentName.size() - storedCustomNameLength);
				if (IsLikelyDynamicTemperSuffix(temperSuffix)) {
					StripTrailingRepeatedSuffix(storedCustomName, temperSuffix);
				}
			}
		} else {
			const char* objectNameRaw = a_entry->object->GetName();
			storedCustomName = objectNameRaw ? objectNameRaw : "";
			if (storedCustomName.empty()) {
				storedCustomName = currentName;
			}
		}

		std::string baseName = StripKnownLootAffixTags(StripLootStarMarkers(storedCustomName));
		if (baseName.empty()) {
			const char* objectNameRaw = a_entry->object->GetName();
			baseName = objectNameRaw ? objectNameRaw : "";
		}

		if (a_outStoredCustomName) {
			*a_outStoredCustomName = storedCustomName;
		}
		TrimTrailingAsciiWhitespaceInPlace(baseName);
		return baseName;
	}

	[[nodiscard]] std::string EventBridge::StripKnownLootAffixTags(std::string_view a_name) const
	{
		auto isKnownAffixLabel = [&](std::string_view a_label) {
			if (a_label.empty()) {
				return false;
			}
			return _affixRuntimeState.affixRegistry.affixLabelSet.contains(std::string(a_label));
		};

		std::string cleaned;
		cleaned.reserve(a_name.size());

		std::size_t pos = 0;
		while (pos < a_name.size()) {
			if (a_name[pos] == '[') {
				const auto right = a_name.find(']', pos + 1);
				if (right != std::string::npos && right > pos + 1) {
					const std::string_view label = a_name.substr(pos + 1, right - pos - 1);
					if (isKnownAffixLabel(label)) {
						while (!cleaned.empty() && cleaned.back() == ' ') {
							cleaned.pop_back();
						}
						pos = right + 1;
						while (pos < a_name.size() && a_name[pos] == ' ') {
							++pos;
						}
						continue;
					}
				}
			}

			cleaned.push_back(a_name[pos]);
			++pos;
		}

		const auto first = cleaned.find_first_not_of(" \n\r\t");
		if (first == std::string::npos) {
			return {};
		}
		const auto last = cleaned.find_last_not_of(" \n\r\t");
		return cleaned.substr(first, last - first + 1);
	}

	[[nodiscard]] std::string EventBridge::BuildLootNameMarker(std::uint8_t a_affixCount)
	{
		if (a_affixCount >= 3) {
			return "***";
		}
		if (a_affixCount == 2) {
			return "**";
		}
		return "*";
	}

	void EventBridge::GrantTrapAffixToSelectedBase()
	{
		// QA helper behind the debug toggles: assigns trap affixes to the runeword
		// panel's selected base so the stability scenarios (S1/S3) do not require
		// farming trap prefixes. One affix per press, cycling bear -> rune ->
		// plague -> tar -> siphon -> chaos and skipping ones already on the item.
		if (!(_loot.debugHudNotifications || _loot.debugLog)) {
			return;
		}
		if (!_configLoaded) {
			EmitHudNotification("Calamity: config not loaded.");
			return;
		}

		std::uint64_t instanceKey = 0u;
		RE::InventoryEntryData* entry = nullptr;
		RE::ExtraDataList* xList = nullptr;
		std::string failure;
		if (!ResolveSelectedRunewordBaseInstance(instanceKey, entry, xList, &failure, false)) {
			std::string note = "Calamity: select a base in the panel first";
			if (!failure.empty()) {
				note.append(" (").append(failure).append(")");
			}
			EmitHudNotification(note.c_str());
			return;
		}

		static constexpr std::array<std::string_view, 6> kTrapAffixIds{
			"bear_trap", "rune_trap", "plague_spore", "tar_blight", "siphon_spore", "chaos_rune"
		};

		auto& slots = _instanceTrackingState.instanceAffixes[instanceKey];
		for (const auto trapId : kTrapAffixIds) {
			const auto idxIt = _affixRuntimeState.affixRegistry.affixIndexById.find(std::string(trapId));
			if (idxIt == _affixRuntimeState.affixRegistry.affixIndexById.end() ||
				idxIt->second >= _affixRuntimeState.affixes.size()) {
				continue;
			}
			const auto& affix = _affixRuntimeState.affixes[idxIt->second];
			if (slots.HasToken(affix.token)) {
				continue;
			}
			if (!slots.AddToken(affix.token)) {
				EmitHudNotification("Calamity: affix slots full on selected base.");
				return;
			}

			// The instance now carries hand-assigned affixes; mark it evaluated so
			// the loot pipeline never rolls over them on a later pickup.
			MarkLootEvaluatedInstance(instanceKey);
			EnsureMultiAffixDisplayName(entry, xList, slots);
			RebuildActiveCounts();

			std::string note = "Calamity: granted ";
			note.append(trapId);
			note.append(" (");
			note.append(std::to_string(slots.count));
			note.append("/4 slots)");
			EmitHudNotification(note.c_str());
			return;
		}

		EmitHudNotification("Calamity: all trap affixes already on this base.");
	}

	void EventBridge::EnsureMultiAffixDisplayName(
		RE::InventoryEntryData* a_entry,
		RE::ExtraDataList* a_xList,
		const InstanceAffixSlots& a_slots)
	{
		if (!_loot.renameItem || !a_entry || !a_entry->object || !a_xList) {
			return;
		}

		if (a_slots.count == 0) {
			return;
		}

		auto* text = a_xList->GetExtraTextDisplayData();
		std::string storedCustomName;
		const std::string baseName = ResolveStoredLootDisplayBaseName(a_entry, a_xList, &storedCustomName);

		if (baseName.empty()) {
			return;
		}

		// Build name marker based on affix count.
		const std::string marker = BuildLootNameMarker(a_slots.count);

		std::string newName;
		if (_loot.nameMarkerPosition == LootNameMarkerPosition::kTrailing) {
			// Requested UX default: "철검*" (no delimiter space) for stable name sorting.
			newName = baseName + marker;
		} else {
			newName = marker + " " + baseName;
		}
		if (newName.empty()) {
			return;
		}
		if (newName == storedCustomName) {
			return;
		}

		if (text) {
			text->SetName(newName.c_str());
		} else {
			auto* created = RE::BSExtraData::Create<RE::ExtraTextDisplayData>();
			created->SetName(newName.c_str());
			a_xList->Add(created);
		}
	}

}
