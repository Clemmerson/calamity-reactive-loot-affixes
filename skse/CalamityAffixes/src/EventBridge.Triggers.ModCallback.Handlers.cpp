#include "CalamityAffixes/EventBridge.h"

#include "CalamityAffixes/PluginEditorIds.h"
#include "CalamityAffixes/PrismaTooltip.h"
#include "CalamityAffixes/RuneforgeMenu.h"
#include "CalamityAffixes/RuneforgeWorkstation.h"
#include "CalamityAffixes/RuneforgeWorkstationContextPolicy.h"
#include "CalamityAffixes/EventNames.h"
#include "EventBridge.Loot.Runeword.Detail.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

namespace CalamityAffixes
{
	bool EventBridge::HandleFeasrRuneforgeModCallback(
		std::string_view a_eventName,
		std::string_view a_stringArg,
		float a_numberArg)
	{
		using namespace EventNames;
		const bool contractCompatible = IsFeasrRuneforgeContractCompatible();
		if (a_eventName == kFeasrRuneforgeDiscoveryBegin) {
			const auto separator = a_stringArg.find(':');
			const auto revision = separator == std::string_view::npos ? a_stringArg : a_stringArg.substr(0u, separator);
			const auto expectedCount = separator == std::string_view::npos ? std::string_view{} : a_stringArg.substr(separator + 1u);
			SKSE::log::info(
				"RuneforgeDiag: Discovery_Begin revision='{}' expectedCount='{}' numArg={} contractCompatible={}",
				revision,
				expectedCount,
				a_numberArg,
				contractCompatible);
		}
		if (a_eventName == kFeasrRuneforgeOpen) {
			SKSE::log::info(
				"RuneforgeDiag: Open received strArg='{}' numArg={} discoveryReady={} runtimeEnabled={} contractCompatible={}",
				a_stringArg,
				a_numberArg,
				_feasrDiscoveryReady,
				_runtimeSettings.enabled.load(std::memory_order_relaxed),
				contractCompatible);
		}
		if (!contractCompatible) {
			_feasrKnownRunewordRecipeIds.clear();
			_feasrDiscoveryReady = false;
			_feasrDiscoverySyncActive = false;
			_feasrDiscoverySyncValid = false;
			if (a_eventName == kFeasrRuneforgeOpen) {
				SKSE::log::info("RuneforgeDiag: QueueOpen(false) not called; contract incompatible");
				EmitHudNotification("Runeforge unavailable: Calamity 2.2.8 runtime contract mismatch.");
			}
			return a_eventName == kFeasrRuneforgeDiscoveryBegin ||
				a_eventName == kFeasrRuneforgeDiscoveryRecipe ||
				a_eventName == kFeasrRuneforgeDiscoveryComplete ||
				a_eventName == kFeasrRuneforgeOpen;
		}
		if (a_eventName == kFeasrRuneforgeDiscoveryBegin) {
			_feasrKnownRunewordRecipeIds.clear();
			_feasrDiscoveryRevision.clear();
			_feasrDiscoveryExpectedCount = 0u;
			_feasrDiscoveryReceivedCount = 0u;
			const auto separator = a_stringArg.find(':');
			std::uint32_t revision = 0u;
			std::uint32_t expectedCount = 0u;
			const auto parseUInt = [](std::string_view text, std::uint32_t& value) {
				const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
				return !text.empty() && error == std::errc{} && end == text.data() + text.size();
			};
			const bool headerValid = separator != std::string_view::npos &&
				parseUInt(a_stringArg.substr(0u, separator), revision) &&
				parseUInt(a_stringArg.substr(separator + 1u), expectedCount) &&
				expectedCount <= 4096u;
			_feasrDiscoverySyncActive = a_numberArg == 1.0f && headerValid;
			_feasrDiscoverySyncValid = _feasrDiscoverySyncActive;
			if (headerValid) {
				_feasrDiscoveryRevision.assign(a_stringArg.substr(0u, separator));
				_feasrDiscoveryExpectedCount = expectedCount;
			}
			_feasrDiscoveryReady = false;
			return true;
		}

		if (a_eventName == kFeasrRuneforgeDiscoveryRecipe) {
			const auto separator = a_stringArg.find(':');
			if (!_feasrDiscoverySyncActive || a_numberArg != 1.0f || separator == std::string_view::npos ||
				a_stringArg.substr(0u, separator) != _feasrDiscoveryRevision ||
				separator + 1u >= a_stringArg.size() || a_stringArg.size() - separator - 1u > 128u) {
				if (_feasrDiscoverySyncActive) {
					_feasrDiscoverySyncValid = false;
				}
				return true;
			}
			const auto recipeId = a_stringArg.substr(separator + 1u);
			const auto it = std::find_if(_runewordState.recipes.begin(), _runewordState.recipes.end(), [&](const auto& recipe) {
				return recipe.id == recipeId;
			});
			if (it != _runewordState.recipes.end()) {
				if (!_feasrKnownRunewordRecipeIds.emplace(recipeId).second) {
					_feasrDiscoverySyncValid = false;
				}
				++_feasrDiscoveryReceivedCount;
			} else {
				_feasrDiscoverySyncValid = false;
				SKSE::log::warn("Runeforge: FE ASR sent unknown recipe id '{}'; snapshot rejected.", recipeId);
			}
			return true;
		}

		if (a_eventName == kFeasrRuneforgeDiscoveryComplete) {
			const auto separator = a_stringArg.find(':');
			std::uint32_t revision = 0u;
			std::uint32_t expectedCount = 0u;
			const auto parseUInt = [](std::string_view text, std::uint32_t& value) {
				const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
				return !text.empty() && error == std::errc{} && end == text.data() + text.size();
			};
			const bool headerValid = separator != std::string_view::npos &&
				parseUInt(a_stringArg.substr(0u, separator), revision) &&
				parseUInt(a_stringArg.substr(separator + 1u), expectedCount);
			const bool matchingSnapshot = headerValid &&
				a_stringArg.substr(0u, separator) == _feasrDiscoveryRevision &&
				expectedCount == _feasrDiscoveryExpectedCount;
			_feasrDiscoveryReady = _feasrDiscoverySyncActive && _feasrDiscoverySyncValid &&
				a_numberArg == 1.0f && matchingSnapshot &&
				_feasrDiscoveryReceivedCount == _feasrDiscoveryExpectedCount &&
				_feasrKnownRunewordRecipeIds.size() == _feasrDiscoveryExpectedCount;
			SKSE::log::info(
				"RuneforgeDiag: Discovery_Complete discoveryReady={} receivedCount={} expectedCount={}",
				_feasrDiscoveryReady,
				_feasrDiscoveryReceivedCount,
				_feasrDiscoveryExpectedCount);
			_feasrDiscoverySyncActive = false;
			if (!_feasrDiscoveryReady) {
				_feasrKnownRunewordRecipeIds.clear();
				SKSE::log::warn("Runeforge: rejected FE ASR discovery snapshot (version, revision, or recipe count mismatch).");
			}
			return true;
		}

		if (a_eventName == kFeasrRuneforgeOpen) {
			std::uint64_t activationSequence = 0;
			const auto [sequenceEnd, sequenceError] = std::from_chars(
				a_stringArg.data(), a_stringArg.data() + a_stringArg.size(), activationSequence);
			if (a_stringArg.empty()) {
				SKSE::log::warn("RuneforgeDiag: Open rejected source=direct_or_scripted reason=activation_sequence_missing; QueueOpen not called");
				return true;
			}
			if (sequenceError != std::errc{} || sequenceEnd != a_stringArg.data() + a_stringArg.size() ||
				std::to_string(activationSequence) != a_stringArg) {
				SKSE::log::warn("RuneforgeDiag: Open rejected source=untrusted_event reason=activation_sequence_malformed value='{}'; QueueOpen not called", a_stringArg);
				return true;
			}
			if (!RuneforgeWorkstation::ConsumeClaimedActivationIntent(activationSequence)) {
				SKSE::log::warn("RuneforgeDiag: Open rejected source=untrusted_event reason=claimed_intent_stale_context_changed_or_already_consumed seq={}; QueueOpen not called", activationSequence);
				return true;
			}
			const bool apiVersionValid = a_numberArg == 1.0f;
			const bool discoveryReady = _feasrDiscoveryReady;
			const bool runtimeEnabled = _runtimeSettings.enabled.load(std::memory_order_relaxed);
			if (!apiVersionValid || !discoveryReady || !runtimeEnabled) {
				SKSE::log::warn(
					"RuneforgeDiag: Open rejected after intent consumed apiVersionValid={} discoveryReady={} runtimeEnabled={}; QueueOpen not called",
					apiVersionValid,
					discoveryReady,
					runtimeEnabled);
				EmitHudNotification("Runeforge: discovery or Calamity runtime is not ready.");
				return true;
			}
			SKSE::log::info("RuneforgeDiag: Open accepted source=physical_activation intent=consumed; calling QueueOpen(false)");
			RuneforgeMenu::QueueOpen(false);
			return true;
		}

		return false;
	}

	namespace
	{
		// MCM passes boolean settings as float 1.0/0.0; threshold for truthy conversion.
		constexpr float kMcmBoolThreshold = 0.5f;
		constexpr auto kMcmChanceNotificationCooldown = std::chrono::milliseconds(1500);

		bool ShouldEmitChanceNotification(
			std::chrono::steady_clock::time_point& a_lastNotificationAt,
			std::chrono::steady_clock::time_point a_now)
		{
			if (a_lastNotificationAt.time_since_epoch().count() == 0) {
				a_lastNotificationAt = a_now;
				return true;
			}
			if ((a_now - a_lastNotificationAt) < kMcmChanceNotificationCooldown) {
				return false;
			}
			a_lastNotificationAt = a_now;
			return true;
		}
	}

	bool EventBridge::HandleUiModCallback(std::string_view a_eventName, float a_numArg)
	{
		if (a_eventName == kUiTogglePanelEvent) {
			PrismaTooltip::ToggleControlPanel();
			return true;
		}

		if (a_eventName == kUiSetPanelEvent) {
			const bool open = a_numArg > kMcmBoolThreshold;
			PrismaTooltip::SetControlPanelOpen(open);
			return true;
		}

		if (a_eventName == kUiStatusEvent) {
			PrismaTooltip::ReportStatus();
			return true;
		}

		return false;
	}

	bool EventBridge::HandleRuntimeSettingsModCallback(
		std::string_view a_eventName,
		float a_numArg,
		std::chrono::steady_clock::time_point a_now)
	{
		static auto s_lastRunewordChanceNotificationAt = std::chrono::steady_clock::time_point{};
		static auto s_lastReforgeChanceNotificationAt = std::chrono::steady_clock::time_point{};
		static auto s_lastCraftingChanceNotificationAt = std::chrono::steady_clock::time_point{};

		if (a_eventName == kMcmSetEnabledEvent) {
			const bool enabled = (a_numArg > kMcmBoolThreshold);
			_runtimeSettings.enabled.store(enabled, std::memory_order_relaxed);
			if (enabled) {
				(void)PruneOrphanedPlayerInstanceKeys();
				RebuildActiveCounts();
			} else {
				DeactivateRuntimeState();
			}
			QueueRuntimeUserSettingsPersist();
			EmitHudNotification(enabled ? "Calamity: enabled" : "Calamity: disabled");
			return true;
		}

		if (a_eventName == kLegacyMcmSetDebugNotificationsEvent) {
			const bool enabled = (a_numArg > kMcmBoolThreshold);
			_loot.debugHudNotifications = enabled;
			_loot.debugLog = enabled;
			ApplyVerboseLoggingLevel();
			spdlog::flush_on(spdlog::level::warn);
			QueueRuntimeUserSettingsPersist();
			EmitHudNotification(enabled ? "Calamity: debug HUD/log ON (legacy)" : "Calamity: debug HUD/log OFF (legacy)");
			return true;
		}

		if (a_eventName == kMcmSetDebugHudNotificationsEvent) {
			_loot.debugHudNotifications = (a_numArg > kMcmBoolThreshold);
			QueueRuntimeUserSettingsPersist();
			EmitHudNotification(_loot.debugHudNotifications ? "Calamity: debug HUD ON" : "Calamity: debug HUD OFF");
			return true;
		}

		if (a_eventName == kMcmSetDebugVerboseLoggingEvent) {
			_loot.debugLog = (a_numArg > kMcmBoolThreshold);
			ApplyVerboseLoggingLevel();
			spdlog::flush_on(spdlog::level::warn);
			QueueRuntimeUserSettingsPersist();
			EmitHudNotification(_loot.debugLog ? "Calamity: debug log ON" : "Calamity: debug log OFF");
			return true;
		}

		if (a_eventName == kMcmSetValidationIntervalEvent) {
			const float seconds = std::clamp(a_numArg, 0.0f, 30.0f);
			_equipResync.intervalMs = (seconds <= 0.0f) ? 0u : static_cast<std::uint64_t>(seconds * 1000.0f);
			_equipResync.nextAtMs = 0;
			QueueRuntimeUserSettingsPersist();
			if (seconds <= 0.0f) {
				EmitHudNotification("Calamity: periodic validation OFF");
			} else {
				std::string note = "Calamity: validation interval ";
				note += std::to_string(static_cast<int>(seconds));
				note += "s";
				EmitHudNotification(note.c_str());
			}
			return true;
		}

		if (a_eventName == kMcmSetProcChanceMultEvent) {
			_runtimeSettings.procChanceMult = std::clamp(a_numArg, 0.0f, 3.0f);
			QueueRuntimeUserSettingsPersist();
			std::string note = "Calamity: proc x";
			note += std::to_string(_runtimeSettings.procChanceMult);
			EmitHudNotification(note.c_str());
			return true;
		}

		if (a_eventName == kMcmSetRunewordFragmentChanceEvent) {
			const float previousRunewordChance = _loot.runewordFragmentChancePercent;
			_loot.runewordFragmentChancePercent = std::clamp(a_numArg, 0.0f, 100.0f);
			const float runewordChanceDelta = _loot.runewordFragmentChancePercent - previousRunewordChance;
			const bool runewordChanceChanged = runewordChanceDelta > 0.001f || runewordChanceDelta < -0.001f;
			if (runewordChanceChanged) {
				SyncCurrencyDropModeState("MCM.SetRunewordFragmentChance");
				QueueRuntimeUserSettingsPersist();
				if (ShouldEmitChanceNotification(s_lastRunewordChanceNotificationAt, a_now)) {
					std::string note = "Calamity: runeword fragment chance ";
					note += std::to_string(_loot.runewordFragmentChancePercent);
					note += "%";
					EmitHudNotification(note.c_str());
				}
			}
			return true;
		}

		if (a_eventName == kMcmSetReforgeOrbChanceEvent) {
			const float previousReforgeChance = _loot.reforgeOrbChancePercent;
			_loot.reforgeOrbChancePercent = std::clamp(a_numArg, 0.0f, 100.0f);
			const float reforgeChanceDelta = _loot.reforgeOrbChancePercent - previousReforgeChance;
			const bool reforgeChanceChanged = reforgeChanceDelta > 0.001f || reforgeChanceDelta < -0.001f;
			if (reforgeChanceChanged) {
				SyncCurrencyDropModeState("MCM.SetReforgeOrbChance");
				QueueRuntimeUserSettingsPersist();
				if (ShouldEmitChanceNotification(s_lastReforgeChanceNotificationAt, a_now)) {
					std::string note = "Calamity: reforge orb chance ";
					note += std::to_string(_loot.reforgeOrbChancePercent);
					note += "%";
					EmitHudNotification(note.c_str());
				}
			}
			return true;
		}

		if (a_eventName == kMcmSetIdentifyScrollChanceEvent || a_eventName == kMcmSetScouringOrbChanceEvent) {
			const bool identify = a_eventName == kMcmSetIdentifyScrollChanceEvent;
			float& chance = identify ? _loot.identifyScrollChancePercent : _loot.scouringOrbChancePercent;
			const float previousChance = chance;
			chance = std::clamp(a_numArg, 0.0f, 100.0f);
			const float chanceDelta = chance - previousChance;
			if (chanceDelta > 0.001f || chanceDelta < -0.001f) {
				QueueRuntimeUserSettingsPersist();
				if (ShouldEmitChanceNotification(s_lastCraftingChanceNotificationAt, a_now)) {
					std::string note = identify ? "Calamity: identify scroll chance " : "Calamity: scouring orb chance ";
					note += std::to_string(chance);
					note += "%";
					EmitHudNotification(note.c_str());
				}
			}
			return true;
		}

		if (a_eventName == kMcmSetDotSafetyAutoDisableEvent) {
			_loot.dotTagSafetyAutoDisable = (a_numArg > kMcmBoolThreshold);
			QueueRuntimeUserSettingsPersist();
			if (!_loot.dotTagSafetyAutoDisable) {
				_combatState.dotTagSafetySuppressed = false;
				EmitHudNotification("Calamity: DotApply auto-disable OFF (warn only)");
			} else {
				if (_loot.dotTagSafetyUniqueEffectThreshold > 0u &&
					_combatState.dotObservedMagicEffects.size() > _loot.dotTagSafetyUniqueEffectThreshold) {
					_combatState.dotTagSafetySuppressed = true;
					EmitHudNotification("Calamity: DotApply auto-disabled (safety)");
				} else {
					EmitHudNotification("Calamity: DotApply auto-disable ON");
				}
			}
			return true;
		}

		if (a_eventName == kMcmSetAllowNonHostileFirstHitProcEvent) {
			const bool enabled = (a_numArg > kMcmBoolThreshold);
			_runtimeSettings.allowNonHostilePlayerOwnedOutgoingProcs.store(enabled, std::memory_order_relaxed);
			_combatState.nonHostileFirstHitGate.Clear();
			QueueRuntimeUserSettingsPersist();
			EmitHudNotification(enabled ?
				"Calamity: non-hostile first-hit proc ON" :
				"Calamity: non-hostile first-hit proc OFF");
			return true;
		}

		return false;
	}

	bool EventBridge::HandleDebugUtilityModCallback(std::string_view a_eventName, float a_numArg)
	{
		if (a_eventName == kMcmSpawnTestItemEvent) {
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* ironSword = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00012EB7);
			if (player && ironSword) {
				player->AddObjectToContainer(ironSword, nullptr, 1, nullptr);
				EmitHudNotification("Calamity: spawned test item (Iron Sword)");
			}
			return true;
		}

		if (a_eventName == kMcmGrantTrapAffixEvent) {
			GrantTrapAffixToSelectedBase();
			return true;
		}

		if (a_eventName == kMcmTrapMarkerProbeEvent) {
			SpawnTrapMarkerProbe();
			return true;
		}

		if (a_eventName == kRunewordGrantStarterOrbsEvent) {
			// One-time starter grant: skip if the player already owns any reforge orbs.
			// Debug sessions bypass the one-time gate so QA can farm reforge rolls
			// (the panel already hides this button unless a debug toggle is on).
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* orb = player
				? PluginEditorIds::Lookup<RE::TESObjectMISC>("CAFF_Misc_ReforgeOrb")
				: nullptr;
			const bool debugSession = _loot.debugHudNotifications || _loot.debugLog;
			if (!debugSession && player && orb && player->GetItemCount(orb) > 0) {
				EmitHudNotification("Reforge Orbs: already owned.");
				return true;
			}

			const auto amount = (a_numArg > 0.0f) ? static_cast<std::uint32_t>(a_numArg) : 3u;
			GrantReforgeOrbs(amount);
			// The Scouring Orb is a rare drop; only debug sessions get the full crafting kit.
			if (debugSession) {
				GrantCraftingCurrency("CAFF_Misc_IdentifyScroll", "Identify Scrolls", amount);
				GrantCraftingCurrency("CAFF_Misc_ScouringOrb", "Scouring Orbs", 1u);
			}
			return true;
		}

		if (a_eventName == kMcmForceRebuildEvent) {
			auto* rebuildPlayer = RE::PlayerCharacter::GetSingleton();
			if (rebuildPlayer) {
				for (auto* spell : _instanceTrackingState.appliedPassiveSpells) {
					rebuildPlayer->RemoveSpell(spell);
				}
			}
			_instanceTrackingState.appliedPassiveSpells.clear();
			RebuildActiveCounts();
			std::uint32_t activeCount = 0;
			for (std::size_t i = 0; i < _affixRuntimeState.activeCounts.size(); ++i) {
				if (_affixRuntimeState.activeCounts[i] > 0) {
					++activeCount;
				}
			}
			std::string note = "Calamity: rebuilt (";
			note += std::to_string(activeCount);
			note += " active, ";
			note += std::to_string(_instanceTrackingState.appliedPassiveSpells.size());
			note += " passive)";
			EmitHudNotification(note.c_str());
			SKSE::log::info("CalamityAffixes: force-rebuild — {} active affixes, {} passive spells applied.",
				activeCount, _instanceTrackingState.appliedPassiveSpells.size());
			return true;
		}

		if (a_eventName == kMcmGrantRecoveryPackEvent) {
			auto* recoveryPlayer = RE::PlayerCharacter::GetSingleton();
			if (recoveryPlayer) {
				GrantReforgeOrbs(5u);
				if (_runewordState.runeNameByToken.empty()) {
					InitializeRunewordCatalog();
				}
				std::uint32_t fragmentsGranted = 0u;
				for (const auto& [runeToken, runeName] : _runewordState.runeNameByToken) {
					if (runeToken == 0u || runeName.empty()) {
						continue;
					}
					const auto given = RunewordDetail::GrantRunewordFragments(
						recoveryPlayer, _runewordState.runeNameByToken, runeToken, 1u);
					fragmentsGranted += given;
				}
				std::string note = "Calamity: recovery pack (5 orbs, ";
				note += std::to_string(fragmentsGranted);
				note += " fragments)";
				EmitHudNotification(note.c_str());
				SKSE::log::info("CalamityAffixes: granted recovery pack — 5 orbs, {} fragment types.", fragmentsGranted);
			}
			return true;
		}

		if (a_eventName == kMcmRecoverMiscCurrencyEvent) {
			const auto result = RecoverMiscCurrency();
			if (!result.starts_with("Granted ")) {
				EmitHudNotification(result.c_str());
			}
			return true;
		}

		return false;
	}

	bool EventBridge::HandleRuntimeGameplayModCallback(
		std::string_view a_eventName,
		float a_numArg,
		std::string_view a_affixIdFilter)
	{
		if (a_eventName == kManualModeCycleNextEvent) {
			CycleManualModeForEquippedAffixes(+1, a_affixIdFilter);
			return true;
		}
		if (a_eventName == kManualModeCyclePrevEvent) {
			CycleManualModeForEquippedAffixes(-1, a_affixIdFilter);
			return true;
		}
		if (a_eventName == kRunewordBaseNextEvent) {
			CycleRunewordBase(+1);
			return true;
		}
		if (a_eventName == kRunewordBasePrevEvent) {
			CycleRunewordBase(-1);
			return true;
		}
		if (a_eventName == kRunewordRecipeNextEvent) {
			CycleRunewordRecipe(+1);
			return true;
		}
		if (a_eventName == kRunewordRecipePrevEvent) {
			CycleRunewordRecipe(-1);
			return true;
		}
		if (a_eventName == kRunewordInsertRuneEvent) {
			// RC1 legacy-UI policy: with FEASR Runeforge active, runeword insertion runs only at the Runeforge.
			using namespace RuneforgeWorkstation::ContextPolicy;
			if (ResolveLegacy(LegacyOperation::kRunewordInsert, RuneforgeWorkstation::IsFeasrRuneforgeActive()) != LegacyVerdict::kAllow) {
				SKSE::log::info("RuneforgeDiag: legacy runeword insert event refused (FEASR Runeforge active)");
				EmitHudNotification(kUseRuneforgeMessage.data());
				return true;
			}
			InsertRunewordRuneIntoSelectedBase();
			return true;
		}
		if (a_eventName == kRunewordStatusEvent) {
			LogRunewordStatus();
			return true;
		}
		if (a_eventName == kRunewordGrantNextRuneEvent) {
			const auto amount = (a_numArg > 0.0f) ? static_cast<std::uint32_t>(a_numArg) : 1u;
			GrantNextRequiredRuneFragment(amount);
			return true;
		}
		if (a_eventName == kRunewordGrantRecipeSetEvent) {
			const auto amount = (a_numArg > 0.0f) ? static_cast<std::uint32_t>(a_numArg) : 1u;
			GrantCurrentRecipeRuneSet(amount);
			return true;
		}

		return false;
	}
}
