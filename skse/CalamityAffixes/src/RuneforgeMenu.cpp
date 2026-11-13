#include "CalamityAffixes/RuneforgeMenu.h"

#include <RE/Skyrim.h>
#include <RE/F/FxResponseArgs.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>

#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/RuneforgeMenuCallbackPolicy.h"
#include "CalamityAffixes/RuneforgeMenuOpenPolicy.h"
#include "CalamityAffixes/RuneforgeMenuOperations.h"
#include "CalamityAffixes/RuneforgeMenuRacePolicy.h"
#include "CalamityAffixes/RuneforgeWorkstation.h"
#include "CalamityAffixes/RuneforgeWorkstationSnapshotContract.h"
#include "CalamityAffixes/RunewordUiPresentation.h"

namespace CalamityAffixes::RuneforgeMenu
{
	namespace
	{
		constexpr std::string_view kMenuName = "CalamityRuneforgeMenu";
		constexpr const char* kMoviePath = "Interface/RuneforgeMenu.swf";
		using json = nlohmann::json;
		namespace SC = RuneforgeWorkstation::SnapshotContract;
		std::uint32_t g_focusIndex = 0;
		std::uint32_t g_controlCount = 0;
		std::uint64_t g_snapshotRevision = 0;
		bool g_debugMode = false;
		std::atomic_bool g_closeTaskPending{ false };
		enum class SuppressedInputKind : std::uint8_t
		{
			kCancel,
			kEmergency
		};
		enum class SuppressionResult : std::uint8_t
		{
			kNoMatch,
			kStarted,
			kSuppressed,
			kContinuation,
			kReleased,
			kCapacityExceeded
		};
		struct SuppressedInput
		{
			std::uint32_t device{};
			std::uint32_t idCode{};
			SuppressedInputKind kind{ SuppressedInputKind::kCancel };
			bool active{};
			bool continuationLogged{};
		};
		std::array<SuppressedInput, 8> g_suppressedInputs{};
		std::mutex g_suppressedInputsMutex;
		using CallbackPolicy::Argument;

		bool IsRuneforgeOpen() noexcept
		{
			auto* ui = RE::UI::GetSingleton();
			return ui && ui->IsMenuOpen(kMenuName);
		}

		bool IsEmergencyCloseButton(const RE::ButtonEvent* a_event) noexcept
		{
			if (!a_event || !a_event->IsDown()) {
				return false;
			}
			if (a_event->GetDevice() == RE::INPUT_DEVICES::kKeyboard) {
				return a_event->GetIDCode() == RE::BSKeyboardDevice::Keys::kF10;
			}
			return false;
		}

		bool IsCloseRequestButton(const RE::ButtonEvent* a_event) noexcept
		{
			if (!a_event || !a_event->IsDown()) {
				return false;
			}
			const auto userEvent = a_event->QUserEvent();
			auto* userEvents = RE::UserEvents::GetSingleton();
			if (userEvents && (userEvent == userEvents->cancel || userEvent == userEvents->journal)) {
				return true;
			}
			switch (a_event->GetDevice()) {
			case RE::INPUT_DEVICES::kKeyboard:
				return a_event->GetIDCode() == RE::BSKeyboardDevice::Keys::kEscape ||
					a_event->GetIDCode() == RE::BSKeyboardDevice::Keys::kTab;
			case RE::INPUT_DEVICES::kGamepad:
				return a_event->GetIDCode() == RE::BSWin32GamepadDevice::Keys::kB ||
					a_event->GetIDCode() == RE::BSWin32GamepadDevice::Keys::kBack;
			default:
				return false;
			}
		}

		const char* SuppressedInputKindName(SuppressedInputKind a_kind) noexcept
		{
			return a_kind == SuppressedInputKind::kEmergency ? "F10 emergency" : "cancel";
		}

		SuppressionResult TrackSuppressedInput(
			const RE::ButtonEvent* a_event,
			bool a_startNewSequence,
			SuppressedInputKind a_newKind,
			SuppressedInputKind& a_observedKind)
		{
			if (!a_event) {
				return SuppressionResult::kNoMatch;
			}

			const auto device = static_cast<std::uint32_t>(a_event->GetDevice());
			const auto idCode = a_event->GetIDCode();
			std::lock_guard lock(g_suppressedInputsMutex);
			for (auto& suppressed : g_suppressedInputs) {
				if (!suppressed.active || suppressed.device != device || suppressed.idCode != idCode) {
					continue;
				}

				a_observedKind = suppressed.kind;
				if (!a_event->IsPressed()) {
					suppressed = {};
					return SuppressionResult::kReleased;
				}
				if (a_event->IsRepeating() && !suppressed.continuationLogged) {
					suppressed.continuationLogged = true;
					return SuppressionResult::kContinuation;
				}
				return SuppressionResult::kSuppressed;
			}

			if (!a_startNewSequence || !a_event->IsDown()) {
				return SuppressionResult::kNoMatch;
			}

			const auto freeSlot = std::find_if(g_suppressedInputs.begin(), g_suppressedInputs.end(),
				[](const SuppressedInput& a_input) { return !a_input.active; });
			if (freeSlot == g_suppressedInputs.end()) {
				a_observedKind = a_newKind;
				return SuppressionResult::kCapacityExceeded;
			}

			*freeSlot = { device, idCode, a_newKind, true, false };
			a_observedKind = a_newKind;
			return SuppressionResult::kStarted;
		}

		void CloseOnGameThread(bool a_emergency)
		{
			auto* ui = RE::UI::GetSingleton();
			if (!ui || !ui->IsMenuOpen(kMenuName)) {
				SKSE::log::info("RuneforgeDiag: close task found menuOpen={} emergency={}",
					ui && ui->IsMenuOpen(kMenuName), a_emergency);
				g_closeTaskPending.store(false, std::memory_order_release);
				return;
			}

			auto* queue = RE::UIMessageQueue::GetSingleton();
			if (!queue) {
				SKSE::log::error("RuneforgeDiag: close failed; UIMessageQueue is null emergency={}", a_emergency);
				RE::DebugNotification("Runeforge close failed: UI message queue unavailable.");
				g_closeTaskPending.store(false, std::memory_order_release);
				return;
			}

			const auto closeType = a_emergency ? RE::UI_MESSAGE_TYPE::kForceHide : RE::UI_MESSAGE_TYPE::kHide;
			SKSE::log::info(
				"RuneforgeDiag: queueing {} for Runeforge only; Skyrim retains cursor/menu teardown emergency={}",
				a_emergency ? "kForceHide" : "kHide",
				a_emergency);
			queue->AddMessage(RE::BSFixedString(kMenuName), closeType, nullptr);
			// Keep the close guard raised until ProcessMessage actually receives hide.
		}

		void QueueCloseInternal(bool a_emergency)
		{
			bool expected = false;
			SKSE::log::info("RuneforgeDiag: close requested emergency={} pending={}",
				a_emergency, g_closeTaskPending.load(std::memory_order_acquire));
			if (!g_closeTaskPending.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
				return;
			}
			if (auto* tasks = SKSE::GetTaskInterface()) {
				tasks->AddTask([a_emergency]() {
					CloseOnGameThread(a_emergency);
				});
			} else {
				SKSE::log::error("RuneforgeDiag: task interface unavailable; performing direct close emergency={}", a_emergency);
				CloseOnGameThread(a_emergency);
			}
		}

		RacePolicy::Context GetPlayerRaceContext(RE::PlayerCharacter* a_player)
		{
			if (!a_player) return {};
			auto* currentRace = a_player->GetRace();
			const auto* actorBase = a_player->GetActorBase();
			auto* baseRace = actorBase ? const_cast<RE::TESNPC*>(actorBase)->GetRace() : nullptr;
			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			auto* forbiddenRaces = dataHandler ? dataHandler->LookupForm<RE::BGSListForm>(
				0x801, "FEASR_Runeforge.esp") : nullptr;
			const bool forbiddenForm = forbiddenRaces &&
				((currentRace && forbiddenRaces->HasForm(currentRace)) ||
					(baseRace && forbiddenRaces->HasForm(baseRace)));
			return {
				.currentRaceResolved = currentRace != nullptr,
				.baseRaceResolved = baseRace != nullptr,
				.racesMatch = currentRace && baseRace && currentRace == baseRace,
				.forbiddenForm = forbiddenForm,
			};
		}

		RacePolicy::Refusal GetPlayerRaceRefusal()
		{
			return RacePolicy::Resolve(GetPlayerRaceContext(RE::PlayerCharacter::GetSingleton()));
		}

		const char* RaceRefusalMessage(RacePolicy::Refusal a_refusal)
		{
			switch (a_refusal) {
			case RacePolicy::Refusal::kRaceUnavailable:
				return "The Runeforge cannot verify your current race.";
			case RacePolicy::Refusal::kForbiddenForm:
				return "The Runeforge cannot be used in your current form.";
			case RacePolicy::Refusal::kRaceOverride:
				return "The Runeforge cannot be used while your race differs from your base form.";
			case RacePolicy::Refusal::kNone:
			default:
				return "";
			}
		}

		template <std::size_t N>
		bool ReadCallbackArguments(const RE::FxDelegateArgs& a_args, std::array<Argument, N>& a_output)
		{
			if (a_args.GetArgCount() != N) {
				return false;
			}
			for (std::size_t i = 0; i < N; ++i) {
				switch (a_args[static_cast<std::uint32_t>(i)].GetType()) {
				case RE::GFxValue::ValueType::kString:
					a_output[i] = Argument::String(a_args[static_cast<std::uint32_t>(i)].GetString());
					break;
				case RE::GFxValue::ValueType::kNumber:
					a_output[i] = Argument::Number(a_args[static_cast<std::uint32_t>(i)].GetNumber());
					break;
				default:
					a_output[i] = Argument::Other();
					break;
				}
			}
			return true;
		}

		bool ContainsHangul(std::string_view a_value)
		{
			for (std::size_t i = 0; i < a_value.size();) {
				const auto first = static_cast<unsigned char>(a_value[i]);
				std::uint32_t codepoint = first;
				std::size_t length = 1;
				if ((first & 0xE0u) == 0xC0u && i + 1 < a_value.size()) {
					codepoint = ((first & 0x1Fu) << 6u) | (static_cast<unsigned char>(a_value[i + 1]) & 0x3Fu);
					length = 2;
				} else if ((first & 0xF0u) == 0xE0u && i + 2 < a_value.size()) {
					codepoint = ((first & 0x0Fu) << 12u) |
						((static_cast<unsigned char>(a_value[i + 1]) & 0x3Fu) << 6u) |
						(static_cast<unsigned char>(a_value[i + 2]) & 0x3Fu);
					length = 3;
				} else if ((first & 0xF8u) == 0xF0u && i + 3 < a_value.size()) {
					codepoint = ((first & 0x07u) << 18u) |
						((static_cast<unsigned char>(a_value[i + 1]) & 0x3Fu) << 12u) |
						((static_cast<unsigned char>(a_value[i + 2]) & 0x3Fu) << 6u) |
						(static_cast<unsigned char>(a_value[i + 3]) & 0x3Fu);
					length = 4;
				}
				if ((codepoint >= 0x1100u && codepoint <= 0x11FFu) ||
					(codepoint >= 0x3130u && codepoint <= 0x318Fu) ||
					(codepoint >= 0xA960u && codepoint <= 0xA97Fu) ||
					(codepoint >= 0xAC00u && codepoint <= 0xD7AFu) ||
					(codepoint >= 0xD7B0u && codepoint <= 0xD7FFu)) return true;
				i += length;
			}
			return false;
		}

		std::string EnglishTextOr(std::string_view a_value, std::string_view a_fallback)
		{
			return a_value.empty() || ContainsHangul(a_value) ? std::string(a_fallback) : std::string(a_value);
		}

		void InvokeMovie(const char* a_method, const RE::GFxValue* a_args = nullptr, std::uint32_t a_argCount = 0)
		{
			if (auto* ui = RE::UI::GetSingleton()) {
				if (auto movie = ui->GetMovieView(kMenuName); movie) {
					movie->InvokeNoReturn(a_method, a_args, a_argCount);
				}
			}
		}

		void SetMovieFocus()
		{
			RE::GFxValue focus(static_cast<double>(g_focusIndex));
			InvokeMovie("focusControl", &focus, 1);
		}

		json BuildSnapshot()
		{
			auto* bridge = EventBridge::GetSingleton();
			json snapshot = {
				{"abiVersion", 1},
				{"snapshotRevision", ++g_snapshotRevision},
				{"inventoryRevision", 0},
				{"inventoryKnown", false},
				{"runtimeEnabled", bridge && bridge->IsRuntimeEnabled()},
				{"skippedMissingUniqueId", 0},
				{"skippedNonSingleton", 0},
				{"skippedAmbiguousKeys", 0},
				{"bases", json::array()},
				{"recipes", json::array()},
				{SC::kPanel, json::object()},
				// R8.7.4.6: the selected item key lives at the TOP LEVEL only (SnapshotContract); never inside `panel`.
				{SC::kSelectedInstanceKey, ""},
			{"debugMode", g_debugMode},
				// Native display fields already carry FE-ASR presentation; mechanical
				// keys (ids, tokens, instance keys) are unchanged.
				{"presentation", {
					{"source", "runeforge-presentation.v1.json"},
					{"manifestSha256", std::string(RuneforgePresentation::Generated::kManifestSha256)},
					{"nativeApplied", true}
				}},
				{"discoveryReady", bridge && bridge->IsFeasrRuneforgeContractCompatible() && bridge->IsFeasrRuneforgeDiscoveryReady()}
			};
			if (!bridge) {
				return snapshot;
			}
			if (!bridge->IsFeasrRuneforgeContractCompatible()) {
				return snapshot;
			}

			const auto inventory = bridge->GetRuneforgeInventorySnapshot();
			snapshot["inventoryRevision"] = inventory.revision;
			snapshot["inventoryKnown"] = inventory.known;
			snapshot["skippedMissingUniqueId"] = inventory.skippedMissingUniqueId;
			snapshot["skippedNonSingleton"] = inventory.skippedNonSingleton;
			snapshot["skippedAmbiguousKeys"] = inventory.skippedAmbiguousKeys;
			std::string selectedAttachedRunewordName;
			std::string selectedAttachedRunewordId;
			for (const auto& base : inventory.entries) {
				const auto key = std::to_string(base.instanceKey);
				if (base.selected) {
					snapshot[SC::kSelectedInstanceKey] = key;
					selectedAttachedRunewordName = EnglishTextOr(base.runewordNameEn, "");
					selectedAttachedRunewordId = base.runewordRecipeId;
				}
				snapshot["bases"].push_back({
					{"instanceKey", key},
					{"name", EnglishTextOr(base.displayNameEn, "Item")},
					{"displayName", EnglishTextOr(base.displayNameEn, "Item")},
					{"baseName", EnglishTextOr(base.baseName, "Item")},
					{"runewordId", base.runewordRecipeId},
					{"itemKind", base.itemKind},
					{"selected", base.selected},
					{"equipped", base.equipped},
					{"favorited", base.favorited}
				});
			}

			const auto panel = bridge->GetRunewordPanelState(g_debugMode, g_debugMode);
			std::string selectedRecipeId;
			// R8.7.4.21 I3: one recipe walk per snapshot (each entry now carries its effect card).
			const auto recipeEntries = bridge->GetRunewordRecipeEntries();
			for (const auto& recipe : recipeEntries) {
				if (!g_debugMode && !bridge->IsFeasrRunewordRecipeKnown(recipe.recipeId)) {
					continue;
				}
				json runes = json::array();
				for (const auto token : recipe.runeTokens) {
					runes.push_back(std::to_string(token));
				}
				// id/token/runeTokens are the mechanical keys the AS2 callbacks send
				// back; name/runeSequence/glyphSequence/effect are FE-ASR display text.
				const auto text = RunewordUiPresentation::BuildRuneforgeRecipeSnapshotText(recipe);
				const std::string englishRecipeName = EnglishTextOr(text.name, "Runeword " + recipe.recipeId);
				snapshot["recipes"].push_back({
					{"id", recipe.recipeId},
					{"token", std::to_string(recipe.recipeToken)},
					{"name", englishRecipeName},
					{"runeSequence", EnglishTextOr(text.runeSequence, "")},
					{"glyphSequence", text.glyphSequence},
					{"effect", EnglishTextOr(text.effect, "")},
					{"effectDetail", EnglishTextOr(recipe.effectDetailTextEn, "")},
					// R8.7.4.21 I3: RunewordEffectCard v1 (ASCII lines from resolved runtime data; base proc chance)
					{"effectCard", RunewordEffectCardPolicy::ToJson<json>(recipe.effectCard)},
					{"feasrPresentation", text.feasrPresentation},
					{"recommendedBase", recipe.recommendedBaseKey},
					{"selected", false},
					{"runeTokens", std::move(runes)}
				});
			}

			if (panel.hasRecipe && panel.recipeToken != 0u) {
				for (auto& recipe : snapshot["recipes"]) {
					if (recipe.value("token", std::string{}) == std::to_string(panel.recipeToken)) {
						selectedRecipeId = recipe.value("id", std::string{});
						recipe["selected"] = true;
						break;
					}
				}
			}
			// Match the attached runeword by its mechanical ID, never by display name.
			std::string attachedRunewordEffect;
			if (!selectedAttachedRunewordId.empty()) {
				for (const auto& recipe : recipeEntries) {
					if (recipe.recipeId == selectedAttachedRunewordId &&
						(g_debugMode || bridge->IsFeasrRunewordRecipeKnown(recipe.recipeId))) {
						attachedRunewordEffect = EnglishTextOr(
							RunewordUiPresentation::BuildRuneforgeRecipeSnapshotText(recipe).effect, "");
						break;
					}
				}
			}
			std::string affixSignature;
			json regularAffixTokens = json::array();
			for (const auto token : panel.regularAffixTokens) {
				if (!affixSignature.empty()) affixSignature += ",";
				affixSignature += std::to_string(token);
				regularAffixTokens.push_back(std::to_string(token));
			}
			if (affixSignature.empty()) affixSignature = "-";
			json runeRequirements = json::array();
			for (const auto& requirement : panel.requiredRunes) {
				runeRequirements.push_back({
					{"name", EnglishTextOr(requirement.runeName, "Rune")},
					{"required", requirement.required},
					{"owned", requirement.owned}
				});
			}
			json runeInventory = json::array();
			for (const auto& rune : panel.runeInventory) {
				runeInventory.push_back({
					{"token", std::to_string(rune.runeToken)},
					{"name", EnglishTextOr(rune.runeName, "Rune")},
					{"owned", rune.owned}
				});
			}
			const bool identifyEligible = panel.hasBase && inventory.known && panel.identifyScrollsKnown &&
				panel.regularAffixCount == 0u && panel.identifyScrollsOwned >= detail::kIdentifyScrollCost;
			const bool scourEligible = panel.hasBase && inventory.known && panel.scouringOrbsKnown &&
				panel.regularAffixCount > 0u && panel.scouringOrbsOwned >= detail::kScouringOrbCost;
			const bool reforgeEligible = panel.hasBase && inventory.known && panel.reforgeOrbsKnown &&
				panel.lockedReforgeCost > 0u && panel.reforgeOrbsOwned >= panel.lockedReforgeCost &&
				panel.selectedReforgesLeft > 0u && !panel.reforgeLockCandidates.empty();
			const bool exchangeReforgeEligible = panel.identifyScrollsKnown &&
				panel.reforgeOrbsKnown && panel.exchangeReforgeScrollCost > 0u &&
				panel.identifyScrollsOwned >= panel.exchangeReforgeScrollCost;
			const bool exchangeScourEligible = panel.identifyScrollsKnown &&
				panel.scouringOrbsKnown && panel.exchangeScourScrollCost > 0u &&
				panel.identifyScrollsOwned >= panel.exchangeScourScrollCost;
			snapshot[SC::kPanel] = {
				{SC::kPanelHasBase, panel.hasBase},
				{SC::kPanelHasRecipe, panel.hasRecipe},
				{SC::kPanelComplete, panel.isComplete},
				{"recipeName", selectedRecipeId.empty() ?
					(panel.isComplete ? "Completed runeword" : "No pattern selected") :
					EnglishTextOr(panel.recipeNameEn, "Runeword " + selectedRecipeId)},
				{SC::kPanelRecipeId, selectedRecipeId},
				{"attachedRunewordId", selectedAttachedRunewordId},
				{"attachedRunewordName", selectedAttachedRunewordName},
				{"attachedRunewordEffect", attachedRunewordEffect},
				{"insertedRunes", panel.insertedRunes},
				{"totalRunes", panel.totalRunes},
				{SC::kPanelCanInsert, panel.canInsert},
				{SC::kPanelMissing, EnglishTextOr(panel.missingSummary, "")},
				{SC::kPanelBaseCompatibilityWarning, panel.baseCompatibilityWarning},
				{SC::kPanelBaseCompatibilityMessage, EnglishTextOr(panel.baseCompatibilityMessageEn, "")},
				{"runeInventoryKnown", panel.runeInventoryKnown},
				{"runeInventoryExpectedCount", panel.runeInventoryExpectedCount},
				{"runeInventory", std::move(runeInventory)},
				{"requiredRunes", std::move(runeRequirements)},
				{"reforgeOrbsKnown", panel.reforgeOrbsKnown},
				{"reforgeOrbsOwned", panel.reforgeOrbsOwned},
				{"identifyScrollsKnown", panel.identifyScrollsKnown},
				{"identifyScrollsOwned", panel.identifyScrollsOwned},
				{"identifyScrollCost", detail::kIdentifyScrollCost},
				{"scouringOrbsKnown", panel.scouringOrbsKnown},
				{"scouringOrbsOwned", panel.scouringOrbsOwned},
				{"scouringOrbCost", detail::kScouringOrbCost},
				{"regularAffixCount", panel.regularAffixCount},
				{"regularAffixTokens", std::move(regularAffixTokens)},
				{"affixSignature", affixSignature},
				{"maxRegularAffixCount", panel.maxRegularAffixCount},
				{"standardReforgeCost", panel.standardReforgeCost},
				{"lockedReforgeCost", panel.lockedReforgeCost},
				{"selectedReforgesLeft", panel.selectedReforgesLeft},
				{"selectedReforgesPerItem", panel.selectedReforgesPerItem},
				{"exchangeReforgeScrollCost", panel.exchangeReforgeScrollCost},
				{"exchangeScourScrollCost", panel.exchangeScourScrollCost},
				{"identifyEligible", identifyEligible},
				{"scourEligible", scourEligible},
				{"reforgeEligible", reforgeEligible},
				{"expandEligible", panel.hasBase && inventory.known && panel.canExpandAffix},
				{"exchangeReforgeEligible", exchangeReforgeEligible},
				{"exchangeScourEligible", exchangeScourEligible},
				{"reforgeLockCandidates", json::array()},
				{"canExpandAffix", panel.canExpandAffix},
				{"expandAffixCost", panel.expandAffixCost},
				{"expandAffixUnavailableReason", panel.expandAffixUnavailableReason}
			};
			for (const auto& candidate : panel.reforgeLockCandidates) {
				snapshot["panel"]["reforgeLockCandidates"].push_back({
					{"affixToken", std::to_string(candidate.affixToken)},
					{"name", EnglishTextOr(candidate.displayNameEn, "Unknown affix")},
					{"slotKind", candidate.slotKind}
				});
			}
			return snapshot;
		}

		void RespondJson(const RE::FxDelegateArgs& a_args, const json& a_result)
		{
			json safeResult = a_result;
			if (safeResult.contains("message") && safeResult["message"].is_string()) {
				const auto message = safeResult["message"].get<std::string>();
				const auto fallback = safeResult.value("ok", false) ?
					"Runeforge operation completed." :
					"Runeforge operation failed. Review the current item and required resources.";
				safeResult["message"] = EnglishTextOr(message, fallback);
			}
			const std::string serialized = safeResult.dump();
			RE::FxResponseArgsEx<1> response;
			response[0].SetString(serialized);
			a_args.Respond(response);
		}

		void GetSnapshotCallback(const RE::FxDelegateArgs& a_args)
		{
			SKSE::log::info("RuneforgeUI: native getRuneforgeSnapshot callback entered");
			auto snapshot = BuildSnapshot();
			SKSE::log::info(
				"RuneforgeUI: snapshot built bases={} recipes={} discoveryReady={} bytes={}",
				snapshot.value("bases", json::array()).size(),
				snapshot.value("recipes", json::array()).size(),
				snapshot.value("discoveryReady", false),
				snapshot.dump().size());
			RespondJson(a_args, snapshot);
		}

		void LogUiDiagnosticCallback(const RE::FxDelegateArgs& a_args)
		{
			std::array<Argument, 1> args{};
			if (ReadCallbackArguments(a_args, args)) {
				if (const auto message = CallbackPolicy::BoundedString(args[0])) {
					SKSE::log::info("RuneforgeUI: AS2 {}", *message);
					return;
				}
			}
			SKSE::log::warn("RuneforgeUI: AS2 diagnostic callback received invalid arguments count={}",
				a_args.GetArgCount());
		}

		void SendRuneforgeStageEvent(const char* a_eventName, std::string_view a_stringArg, RE::TESForm* a_sender)
		{
			auto* source = SKSE::GetModCallbackEventSource();
			if (!source || !a_eventName) {
				return;
			}
			const std::string payload(a_stringArg);
			SKSE::ModCallbackEvent event{
				RE::BSFixedString(a_eventName),
				RE::BSFixedString(payload.c_str()),
				0.0f,
				a_sender
			};
			source->SendEvent(&event);
		}

		RE::TESBoundObject* ResolveRuneforgeStageBase(std::uint64_t a_instanceKey)
		{
			if (a_instanceKey == 0u) {
				return nullptr;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* changes = player ? player->GetInventoryChanges() : nullptr;
			if (!changes || !changes->entryList) {
				return nullptr;
			}
			for (auto* entry : *changes->entryList) {
				if (!entry || !entry->object || !entry->extraLists) {
					continue;
				}
				for (auto* xList : *entry->extraLists) {
					if (!xList) {
						continue;
					}
					const auto* uid = xList->GetByType<RE::ExtraUniqueID>();
					if (!uid || uid->baseID == 0u || uid->uniqueID == 0u) {
						continue;
					}
					const auto key = (static_cast<std::uint64_t>(uid->baseID) << 16) |
						static_cast<std::uint64_t>(uid->uniqueID);
					if (key == a_instanceKey) {
						return entry->object;
					}
				}
			}
			return nullptr;
		}

		std::string ResolveRuneforgeStageGlyphSequence(EventBridge* a_bridge, std::string_view a_recipeId)
		{
			if (!a_bridge || a_recipeId.empty()) {
				return {};
			}
			for (const auto& recipe : a_bridge->GetRunewordRecipeEntries()) {
				if (recipe.recipeId == a_recipeId) {
					return RunewordUiPresentation::BuildRuneforgeRecipeSnapshotText(recipe).glyphSequence;
				}
			}
			return {};
		}

		void SelectBaseCallback(const RE::FxDelegateArgs& a_args)
		{
			std::array<Argument, 1> args{};
			CallbackPolicy::SelectBaseRequest request;
			const bool parsed = ReadCallbackArguments(a_args, args) && CallbackPolicy::ParseSelectBase(args, request);
			auto* bridge = EventBridge::GetSingleton();
			const auto raceRefusal = GetPlayerRaceRefusal();
			const bool raceAllowed = raceRefusal == RacePolicy::Refusal::kNone;
			const bool success = parsed && raceAllowed && bridge && bridge->IsFeasrRuneforgeContractCompatible() &&
				bridge->SelectRunewordBase(request.instanceKey, g_debugMode);
			if (success) {
				auto* baseObject = ResolveRuneforgeStageBase(request.instanceKey);
				SendRuneforgeStageEvent("FEASR_RuneforgeStageBase", "", baseObject);
			}
			RespondJson(a_args, {{"ok", success}, {"message", RaceRefusalMessage(raceRefusal)}, {"snapshot", BuildSnapshot()}});
		}

		void SelectRecipeCallback(const RE::FxDelegateArgs& a_args)
		{
			std::array<Argument, 2> args{};
			CallbackPolicy::SelectRecipeRequest request;
			const bool parsed = ReadCallbackArguments(a_args, args) && CallbackPolicy::ParseSelectRecipe(args, request);
			auto* bridge = EventBridge::GetSingleton();
			const auto raceRefusal = GetPlayerRaceRefusal();
			const bool raceAllowed = raceRefusal == RacePolicy::Refusal::kNone;
			const bool contractReady = bridge && bridge->IsFeasrRuneforgeContractCompatible();
			const bool known = parsed && contractReady && (g_debugMode || bridge->IsFeasrRunewordRecipeKnown(request.recipeId));
			const bool selected = parsed && raceAllowed && known && bridge && bridge->SelectRunewordRecipeForBase(
				request.expectedInstanceKey, request.recipeId, g_debugMode);
			if (selected) {
				const auto glyphSequence = ResolveRuneforgeStageGlyphSequence(bridge, request.recipeId);
				SendRuneforgeStageEvent("FEASR_RuneforgeStageRecipe", glyphSequence, RE::PlayerCharacter::GetSingleton());
			}
			RespondJson(a_args, {{"ok", selected}, {"message", RaceRefusalMessage(raceRefusal)}, {"snapshot", BuildSnapshot()}});
		}

		void TransmuteCallback(const RE::FxDelegateArgs& a_args)
		{
			std::array<Argument, 3> args{};
			CallbackPolicy::TransmuteRequest request;
			const bool parsed = ReadCallbackArguments(a_args, args) && CallbackPolicy::ParseTransmute(args, request);
			if (parsed) {
				SKSE::log::info(
					"RuneforgeDiag: transmute callback parsed key={:016X} recipe='{}' inserted={}",
					request.expectedInstanceKey,
					request.recipeId,
					request.insertedRunes);
			} else {
				SKSE::log::warn("RuneforgeDiag: transmute callback parse failed argCount={}", a_args.GetArgCount());
			}
			// The one commit path, shared with the workstation session (RuneforgeMenuOperations.h).
			const auto result = parsed ?
				Operations::Transmute(request.expectedInstanceKey, request.recipeId, request.insertedRunes) :
				Operations::Outcome{ false, "Invalid Runeforge request." };
			RespondJson(a_args, {{"ok", result.ok}, {"message", result.message}, {"snapshot", BuildSnapshot()}});
		}

		void CraftAffixesCallback(const RE::FxDelegateArgs& a_args)
		{
			std::array<Argument, 5> args{};
			CallbackPolicy::CraftAffixesRequest request;
			const bool parsed = ReadCallbackArguments(a_args, args) && CallbackPolicy::ParseCraftAffixes(args, request);
			std::optional<AffixCraftAction> action;
			if (parsed) {
				switch (request.action) {
				case CallbackPolicy::AffixAction::kIdentify: action = AffixCraftAction::kIdentify; break;
				case CallbackPolicy::AffixAction::kReforge: action = AffixCraftAction::kReforge; break;
				case CallbackPolicy::AffixAction::kScour: action = AffixCraftAction::kScour; break;
				}
			}
			auto* bridge = EventBridge::GetSingleton();
			const auto raceRefusal = GetPlayerRaceRefusal();
			const bool raceAllowed = raceRefusal == RacePolicy::Refusal::kNone;
			EventBridge::OperationResult result{ false, "Invalid Runeforge crafting request." };
			if (!raceAllowed) result.message = RaceRefusalMessage(raceRefusal);
			if (parsed && raceAllowed && action && bridge && bridge->IsFeasrRuneforgeContractCompatible()) {
				result = bridge->CraftSelectedAffixes(
					*action,
					request.expectedInstanceKey,
					request.selectedToken,
					request.expectedAffixSignature,
					request.expectedAffixCount);
			}
			RespondJson(a_args, {{"ok", result.success}, {"message", result.message}, {"snapshot", BuildSnapshot()}});
		}

		void ExpandAffixCallback(const RE::FxDelegateArgs& a_args)
		{
			std::array<Argument, 3> args{};
			CallbackPolicy::ExpandAffixRequest request;
			const bool parsed = ReadCallbackArguments(a_args, args) && CallbackPolicy::ParseExpandAffix(args, request);
			auto* bridge = EventBridge::GetSingleton();
			const auto raceRefusal = GetPlayerRaceRefusal();
			const bool raceAllowed = raceRefusal == RacePolicy::Refusal::kNone;
			EventBridge::OperationResult result{ false, "Invalid Runeforge expansion request." };
			if (!raceAllowed) result.message = RaceRefusalMessage(raceRefusal);
			if (parsed && raceAllowed && bridge && bridge->IsFeasrRuneforgeContractCompatible()) {
				result = bridge->ExpandSelectedRunewordBaseAffixes(
					request.expectedInstanceKey,
					request.expectedAffixCount,
					request.expectedAffixSignature);
			}
			RespondJson(a_args, {{"ok", result.success}, {"message", result.message}, {"snapshot", BuildSnapshot()}});
		}

		void ExchangeCurrencyCallback(const RE::FxDelegateArgs& a_args)
		{
			std::array<Argument, 1> args{};
			CallbackPolicy::ExchangeCurrencyRequest request;
			const bool parsed = ReadCallbackArguments(a_args, args) && CallbackPolicy::ParseExchangeCurrency(args, request);
			std::optional<CurrencyExchange> exchange;
			if (parsed) {
				switch (request.exchange) {
				case CallbackPolicy::CurrencyExchangeAction::kReforgeOrb: exchange = CurrencyExchange::kReforgeOrb; break;
				case CallbackPolicy::CurrencyExchangeAction::kScouringOrb: exchange = CurrencyExchange::kScouringOrb; break;
				}
			}
			auto* bridge = EventBridge::GetSingleton();
			const auto raceRefusal = GetPlayerRaceRefusal();
			const bool raceAllowed = raceRefusal == RacePolicy::Refusal::kNone;
			EventBridge::OperationResult result{ false, "Invalid Runeforge currency exchange." };
			if (!raceAllowed) result.message = RaceRefusalMessage(raceRefusal);
			if (parsed && raceAllowed && exchange && bridge && bridge->IsFeasrRuneforgeContractCompatible()) result = bridge->ExchangeCraftingCurrency(*exchange);
			RespondJson(a_args, {{"ok", result.success}, {"message", result.message}, {"snapshot", BuildSnapshot()}});
		}

		void SetControlCountCallback(const RE::FxDelegateArgs& a_args)
		{
			std::array<Argument, 1> args{};
			std::uint32_t parsedCount = 0;
			if (ReadCallbackArguments(a_args, args) && CallbackPolicy::ParseControlCount(args, parsedCount)) {
				g_controlCount = parsedCount;
			} else {
				g_controlCount = 0u;
			}
			if (g_controlCount == 0u) {
				g_focusIndex = 0u;
			} else if (g_focusIndex >= g_controlCount) {
				g_focusIndex = g_controlCount - 1u;
			}
			SKSE::log::info("RuneforgeUI: setRuneforgeControlCount parsed={} controls={} focus={}",
				parsedCount, g_controlCount, g_focusIndex);
		}

		void SetControlFocusCallback(const RE::FxDelegateArgs& a_args)
		{
			std::array<Argument, 1> args{};
			std::uint32_t requestedIndex = 0;
			if (!ReadCallbackArguments(a_args, args) || !CallbackPolicy::ParseControlCount(args, requestedIndex)) {
				SKSE::log::warn("RuneforgeUI: ignored invalid focused-control index");
				return;
			}
			if (g_controlCount == 0u) {
				g_focusIndex = 0u;
			} else {
				g_focusIndex = std::min(requestedIndex, g_controlCount - 1u);
			}
		}

		class RuneforgeMenuEventHandler final : public RE::MenuEventHandler
		{
		public:
			bool CanProcess(RE::InputEvent* a_event) override
			{
				const auto* button = a_event ? a_event->AsButtonEvent() : nullptr;
				if (!button) return false;
				const auto userEvent = button->QUserEvent();
				return userEvent == "Accept" || userEvent == "Up" || userEvent == "Down";
			}

			bool ProcessButton(RE::ButtonEvent* a_event) override
			{
				if (!a_event || !a_event->IsDown()) {
					return false;
				}
				const auto userEvent = a_event->QUserEvent();
				if (userEvent == "Up" || userEvent == "Down") {
					if (g_controlCount > 0u) {
						g_focusIndex = userEvent == "Down"
							? (g_focusIndex + 1u) % g_controlCount
							: (g_focusIndex + g_controlCount - 1u) % g_controlCount;
						SetMovieFocus();
					}
					SKSE::log::info("RuneforgeUI: navigation userEvent='{}' focus={} controls={}",
						userEvent.c_str(), g_focusIndex, g_controlCount);
					return true;
				}
				if (userEvent == "Accept") {
					SKSE::log::info("RuneforgeUI: accept focus={} controls={}", g_focusIndex, g_controlCount);
					InvokeMovie("activateFocused");
					return true;
				}
				return false;
			}
		};

		RuneforgeMenuEventHandler g_inputHandler;

		class RuneforgeSafetyInputSink final : public RE::BSInputDeviceManager::Sink
		{
		public:
			using Event = RE::InputEvent*;

			RE::BSEventNotifyControl ProcessEvent(
				const Event* a_event,
				RE::BSTEventSource<Event>*) override
			{
				const auto* inputEvent = a_event ? *a_event : nullptr;
				for (auto* current = inputEvent; current; current = current->next) {
					const auto* button = current->AsButtonEvent();
					if (!button) {
						continue;
					}

					SuppressedInputKind trackedKind{ SuppressedInputKind::kCancel };
					const auto tracked = TrackSuppressedInput(
						button, false, SuppressedInputKind::kCancel, trackedKind);
					if (tracked == SuppressionResult::kReleased) {
						SKSE::log::info(
							"RuneforgeDiag: raw {} release suppressed device={} id={}",
							SuppressedInputKindName(trackedKind),
							static_cast<std::uint32_t>(button->GetDevice()),
							button->GetIDCode());
						return RE::BSEventNotifyControl::kStop;
					}
					if (tracked == SuppressionResult::kContinuation) {
						SKSE::log::info(
							"RuneforgeDiag: raw {} repeat suppression continued device={} id={} held={} repeating={}",
							SuppressedInputKindName(trackedKind),
							static_cast<std::uint32_t>(button->GetDevice()),
							button->GetIDCode(),
							button->IsHeld(),
							button->IsRepeating());
						return RE::BSEventNotifyControl::kStop;
					}
					if (tracked == SuppressionResult::kSuppressed) {
						return RE::BSEventNotifyControl::kStop;
					}

					if (!IsRuneforgeOpen()) {
						continue;
					}

					const bool emergency = IsEmergencyCloseButton(button);
					const bool cancel = IsCloseRequestButton(button);
					if (!emergency && !cancel) {
						continue;
					}

					const auto kind = emergency ? SuppressedInputKind::kEmergency : SuppressedInputKind::kCancel;
					trackedKind = kind;
					const auto started = TrackSuppressedInput(button, true, kind, trackedKind);
					if (started == SuppressionResult::kStarted ||
						started == SuppressionResult::kCapacityExceeded) {
						if (started == SuppressionResult::kCapacityExceeded) {
							SKSE::log::error(
								"RuneforgeDiag: raw close suppression table full; consuming device={} id={}",
								static_cast<std::uint32_t>(button->GetDevice()),
								button->GetIDCode());
						}
						if (emergency) {
							SKSE::log::info(
								"RuneforgeDiag: raw F10 DOWN device={} id={} userEvent='{}'; emergency force-hide queued",
								static_cast<std::uint32_t>(button->GetDevice()),
								button->GetIDCode(),
								button->QUserEvent().c_str());
							QueueCloseInternal(true);
						} else {
							SKSE::log::info(
								"RuneforgeDiag: raw cancel DOWN device={} id={} userEvent='{}'; normal hide queued",
								static_cast<std::uint32_t>(button->GetDevice()),
								button->GetIDCode(),
								button->QUserEvent().c_str());
							QueueCloseInternal(false);
						}
						return RE::BSEventNotifyControl::kStop;
					}
				}

				return RE::BSEventNotifyControl::kContinue;
			}
		};

		RuneforgeSafetyInputSink g_safetyInputSink;

		class RuneforgeMenuView final : public RE::IMenu
		{
		public:
			static constexpr std::string_view MENU_NAME = kMenuName;

			RuneforgeMenuView()
			{
				constexpr auto flags =
					static_cast<std::uint32_t>(RE::UI_MENU_FLAGS::kPausesGame) |
					static_cast<std::uint32_t>(RE::UI_MENU_FLAGS::kUsesMenuContext) |
					static_cast<std::uint32_t>(RE::UI_MENU_FLAGS::kModal) |
				static_cast<std::uint32_t>(RE::UI_MENU_FLAGS::kDisablePauseMenu) |
				static_cast<std::uint32_t>(RE::UI_MENU_FLAGS::kUsesCursor);
				menuFlags = static_cast<RE::UI_MENU_FLAGS>(flags);
				inputContext = RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode;
			}

			void PostCreate() override
			{
				SKSE::log::info("RuneforgeDiag: PostCreate entered");
				auto* scaleform = RE::BSScaleformManager::GetSingleton();
				const bool movieLoaded = scaleform && scaleform->LoadMovie(this, uiMovie, "RuneforgeMenu");
				SKSE::log::info(
					"RuneforgeDiag: LoadMovie result={} uiMovieNonNull={}",
					movieLoaded,
					uiMovie != nullptr);
				if (!scaleform || !movieLoaded) {
					constexpr const char* kGfxFallbackPath = "Interface/Exported/RuneforgeMenu.gfx";
					const RE::BSResourceNiBinaryStream requestedMovie(kMoviePath);
					const RE::BSResourceNiBinaryStream gfxFallback(kGfxFallbackPath);
					const bool requestedMovieAvailable = requestedMovie.good();
					const bool gfxFallbackAvailable = gfxFallback.good();
					const char* failureReason = !scaleform
						? "BSScaleformManager singleton unavailable"
						: !scaleform->loader
							? "GFx loader unavailable"
							: !requestedMovieAvailable
								? "requested SWF is absent from the game's resource VFS"
								: "GFx load or movie instantiation failed; LoadMovie exposes only a bool, so the engine loader detail is unavailable";
					SKSE::log::error(
						"Runeforge Scaleform LoadMovie failed: path={} reason={} managerAvailable={} loaderAvailable={} requestedVfsResource={} gfxFallbackPath={} gfxFallbackVfsResource={}",
						kMoviePath,
						failureReason,
						scaleform != nullptr,
						scaleform != nullptr && scaleform->loader != nullptr,
						requestedMovieAvailable,
						kGfxFallbackPath,
						gfxFallbackAvailable);
					QueueCloseInternal(false);
				}
			}

			void Accept(RE::FxDelegateHandler::CallbackProcessor* a_processor) override
			{
				if (!a_processor) {
					SKSE::log::error("RuneforgeUI: FxDelegate Accept received null callback processor");
					return;
				}
				SKSE::log::info("RuneforgeUI: FxDelegate Accept registering native callbacks");
				a_processor->Process("getRuneforgeSnapshot", GetSnapshotCallback);
				a_processor->Process("diagnosticRuneforgeUi", LogUiDiagnosticCallback);
				a_processor->Process("selectRuneforgeBase", SelectBaseCallback);
				a_processor->Process("selectRuneforgeRecipe", SelectRecipeCallback);
				a_processor->Process("transmuteRuneforge", TransmuteCallback);
				a_processor->Process("craftRuneforgeAffixes", CraftAffixesCallback);
				a_processor->Process("expandRuneforgeAffix", ExpandAffixCallback);
				a_processor->Process("exchangeRuneforgeCurrency", ExchangeCurrencyCallback);
				a_processor->Process("setRuneforgeControlCount", SetControlCountCallback);
				a_processor->Process("setRuneforgeFocusIndex", SetControlFocusCallback);
			}

			void PostDisplay() override
			{
				RE::IMenu::PostDisplay();
			}

			RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override
			{
				if (a_message.type == RE::UI_MESSAGE_TYPE::kShow) {
					SKSE::log::info("RuneforgeDiag: ProcessMessage kShow entered");
					if (auto* controls = RE::MenuControls::GetSingleton()) {
						controls->RegisterHandler(&g_inputHandler);
					}
				}
				if (a_message.type == RE::UI_MESSAGE_TYPE::kHide ||
					a_message.type == RE::UI_MESSAGE_TYPE::kForceHide) {
					SKSE::log::info(
						"RuneforgeDiag: ProcessMessage hide entered type={} unregistering menu input handler",
						static_cast<std::uint32_t>(a_message.type.get()));
					if (auto* controls = RE::MenuControls::GetSingleton()) {
						controls->UnregisterHandler(&g_inputHandler);
					}
					SendRuneforgeStageEvent("FEASR_RuneforgeStageClear", "", RE::PlayerCharacter::GetSingleton());
					const auto result = RE::IMenu::ProcessMessage(a_message);
					auto* ui = RE::UI::GetSingleton();
					SKSE::log::info("RuneforgeDiag: base hide handler returned; menuStillOpen={}",
						ui && ui->IsMenuOpen(kMenuName));
					g_closeTaskPending.store(false, std::memory_order_release);
					if (auto* tasks = SKSE::GetTaskInterface()) {
						tasks->AddTask([]() {
							auto* nextFrameUI = RE::UI::GetSingleton();
							SKSE::log::info(
								"RuneforgeDiag: post-hide heartbeat menuOpen={} cursorMenuOpen={}",
								nextFrameUI && nextFrameUI->IsMenuOpen(kMenuName),
								nextFrameUI && nextFrameUI->IsMenuOpen(RE::CursorMenu::MENU_NAME));
						});
					}
					return result;
				}
				return RE::IMenu::ProcessMessage(a_message);
			}
		};

		RE::IMenu* CreateMenu()
		{
			SKSE::log::info("RuneforgeDiag: CreateMenu invoked");
			return new RuneforgeMenuView();
		}

		void OpenOnGameThread(bool a_debugMode)
		{
			SKSE::log::info("RuneforgeDiag: OpenOnGameThread entered debugMode={}", a_debugMode);
			auto* ui = RE::UI::GetSingleton();
			auto* bridge = EventBridge::GetSingleton();
			const bool contractCompatible = bridge && bridge->IsFeasrRuneforgeContractCompatible();
			if (!contractCompatible) {
				SKSE::log::info(
					"RuneforgeDiag: OpenOnGameThread stopped bridgeAvailable={} contractCompatible={}",
					bridge != nullptr,
					contractCompatible);
				RE::DebugNotification("Runeforge unavailable: Calamity 2.2.8 runtime contract mismatch.");
				return;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			bool inCombat = false;
			bool mounted = false;
			RacePolicy::Context raceContext;
			if (player) {
				inCombat = player->IsInCombat();
				raceContext = GetPlayerRaceContext(player);
				RE::NiPointer<RE::Actor> mount;
				mounted = player->GetMount(mount) && static_cast<bool>(mount);
			}

			const OpenPolicy::Context context{
				.uiAvailable = ui != nullptr,
				.runeforgeAlreadyOpen = (ui && ui->IsMenuOpen(kMenuName)) || RuneforgeWorkstation::IsActive(),
				.blockingMenuOpen = ui && (ui->IsModalMenuOpen() || ui->IsApplicationMenuOpen()),
				.playerAvailable = player != nullptr,
				.inCombat = inCombat,
				.mounted = mounted,
				.race = raceContext,
				.runtimeReady = bridge && bridge->IsRuntimeEnabled(),
				.discoveryReady = bridge && bridge->IsFeasrRuneforgeDiscoveryReady(),
				.debugMode = a_debugMode,
			};
			SKSE::log::info(
				"RuneforgeDiag: OpenOnGameThread context uiAvailable={} alreadyOpen={} blockingMenuOpen={} playerAvailable={} inCombat={} mounted={} runtimeReady={} discoveryReady={} debugMode={}",
				context.uiAvailable,
				context.runeforgeAlreadyOpen,
				context.blockingMenuOpen,
				context.playerAvailable,
				context.inCombat,
				context.mounted,
				context.runtimeReady,
				context.discoveryReady,
				context.debugMode);
			const auto refusal = OpenPolicy::Resolve(context);
			SKSE::log::info("RuneforgeDiag: OpenPolicy refusal={}", static_cast<int>(refusal));
			if (refusal != OpenPolicy::Refusal::kNone) {
				const char* message = nullptr;
				switch (refusal) {
				case OpenPolicy::Refusal::kBlockingMenu:
					return;
				case OpenPolicy::Refusal::kCombat:
					message = "The Runeforge cannot be used during combat.";
					break;
				case OpenPolicy::Refusal::kMounted:
					message = "Dismount before using the Runeforge.";
					break;
				case OpenPolicy::Refusal::kRaceUnavailable:
					message = "The Runeforge cannot verify your current race.";
					break;
				case OpenPolicy::Refusal::kForbiddenForm:
					message = "The Runeforge cannot be used in your current form.";
					break;
				case OpenPolicy::Refusal::kRaceOverride:
					message = "The Runeforge cannot be used while your race differs from your base form.";
					break;
				case OpenPolicy::Refusal::kRuntimeUnavailable:
					message = "The Runeforge is unavailable because Calamity is not ready.";
					break;
				case OpenPolicy::Refusal::kDiscoveryUnavailable:
					message = "You have not yet learned how to use the Runeforge.";
					break;
				case OpenPolicy::Refusal::kUiUnavailable:
				case OpenPolicy::Refusal::kAlreadyOpen:
				case OpenPolicy::Refusal::kPlayerUnavailable:
				case OpenPolicy::Refusal::kNone:
				default:
					break;
				}
				if (message) {
					RE::DebugNotification(message);
				}
				return;
			}
			// Workstation mode first (physical workstation, then menu); the flat panel stays the fallback and the
			// accessibility mode (RC1 decision WS-D14). The debug path always uses the flat panel.
			g_debugMode = a_debugMode;
			if (!a_debugMode && RuneforgeWorkstation::TryOpen()) {
				return;
			}
			auto* queue = RE::UIMessageQueue::GetSingleton();
			if (!queue) {
				SKSE::log::error("RuneforgeDiag: UIMessageQueue is null; show message not queued");
			} else {
				g_debugMode = a_debugMode;
				SKSE::log::info("RuneforgeDiag: queueing UI_MESSAGE_TYPE::kShow for {}", kMenuName);
				queue->AddMessage(RE::BSFixedString(kMenuName), RE::UI_MESSAGE_TYPE::kShow, nullptr);
			}
		}
	}

	void Install()
	{
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
			input->AddEventSink(&g_safetyInputSink);
			SKSE::log::info("RuneforgeDiag: raw input safety sink registered");
		} else {
			SKSE::log::error("RuneforgeDiag: input device manager unavailable; raw emergency close is not registered");
		}
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->Register(kMenuName, CreateMenu);
			SKSE::log::info("Runeforge: registered native menu factory.");
		} else {
			SKSE::log::error("RuneforgeDiag: UI singleton unavailable; menu factory registration failed");
		}
	}

	bool IsOpen()
	{
		return IsRuneforgeOpen();
	}

	void QueueClose(bool a_emergency)
	{
		QueueCloseInternal(a_emergency);
	}

	void QueueOpen(bool a_debugMode)
	{
		auto* tasks = SKSE::GetTaskInterface();
		SKSE::log::info("RuneforgeDiag: QueueOpen debug={} taskInterface={}", a_debugMode, tasks != nullptr);
		if (tasks) {
			tasks->AddTask([a_debugMode]() {
				SKSE::log::info("RuneforgeDiag: queued task invoking OpenOnGameThread debug={}", a_debugMode);
				OpenOnGameThread(a_debugMode);
			});
			SKSE::log::info("RuneforgeDiag: QueueOpen accepted task_submitted=true debug={}", a_debugMode);
		} else {
			SKSE::log::error("RuneforgeDiag: QueueOpen rejected reason=task_interface_unavailable; no game-thread task queued");
		}
	}

	namespace Operations
	{
		namespace
		{
			Outcome RaceGate(bool& a_allowed)
			{
				const auto refusal = GetPlayerRaceRefusal();
				a_allowed = refusal == RacePolicy::Refusal::kNone;
				return { false, a_allowed ? std::string{} : std::string(RaceRefusalMessage(refusal)) };
			}

			Outcome English(const EventBridge::OperationResult& a_result, std::string_view a_fallback)
			{
				return { a_result.success, EnglishTextOr(a_result.message, a_fallback) };
			}
		}

		nlohmann::json Snapshot()
		{
			return BuildSnapshot();
		}

		Outcome SelectBase(std::uint64_t a_instanceKey)
		{
			bool raceAllowed = false;
			auto outcome = RaceGate(raceAllowed);
			auto* bridge = EventBridge::GetSingleton();
			outcome.ok = a_instanceKey != 0u && raceAllowed && bridge && bridge->IsFeasrRuneforgeContractCompatible() &&
				bridge->SelectRunewordBase(a_instanceKey, g_debugMode);
			return outcome;
		}

		Outcome SelectRecipe(std::uint64_t a_instanceKey, const std::string& a_recipeId)
		{
			bool raceAllowed = false;
			auto outcome = RaceGate(raceAllowed);
			auto* bridge = EventBridge::GetSingleton();
			const bool known = bridge && bridge->IsFeasrRuneforgeContractCompatible() &&
				(g_debugMode || bridge->IsFeasrRunewordRecipeKnown(a_recipeId));
			outcome.ok = a_instanceKey != 0u && !a_recipeId.empty() && raceAllowed && known &&
				bridge->SelectRunewordRecipeForBase(a_instanceKey, a_recipeId, g_debugMode);
			return outcome;
		}

		Outcome Transmute(std::uint64_t a_instanceKey, const std::string& a_recipeId, std::uint32_t a_insertedRunes)
		{
			SKSE::log::info("RuneforgeDiag: transmute key={:016X} recipe='{}' inserted={}", a_instanceKey, a_recipeId, a_insertedRunes);
			bool raceAllowed = false;
			auto outcome = RaceGate(raceAllowed);
			auto* bridge = EventBridge::GetSingleton();
			EventBridge::OperationResult result{ false, raceAllowed ? "Invalid Runeforge request." : outcome.message };
			if (raceAllowed && bridge && bridge->IsFeasrRuneforgeContractCompatible() && (g_debugMode || bridge->IsFeasrRunewordRecipeKnown(a_recipeId))) {
				result = bridge->TransmuteRunewordBase(a_instanceKey, a_recipeId, a_insertedRunes, g_debugMode);
			}
			SKSE::log::info(
				"RuneforgeDiag: transmute result success={} raceAllowed={} bridge={} message='{}'",
				result.success,
				raceAllowed,
				bridge != nullptr,
				result.message);
			return English(result, result.success ? "Runeforge operation completed." :
				"Runeforge operation failed. Review the current item and required resources.");
		}

		Outcome CraftAffixes(Craft a_action, std::uint64_t a_instanceKey, std::uint64_t a_token, const std::string& a_signature, std::uint32_t a_count)
		{
			bool raceAllowed = false;
			auto outcome = RaceGate(raceAllowed);
			auto* bridge = EventBridge::GetSingleton();
			EventBridge::OperationResult result{ false, raceAllowed ? "Invalid Runeforge crafting request." : outcome.message };
			if (raceAllowed && bridge && bridge->IsFeasrRuneforgeContractCompatible()) {
				const auto action = a_action == Craft::kIdentify ? AffixCraftAction::kIdentify :
					(a_action == Craft::kReforge ? AffixCraftAction::kReforge : AffixCraftAction::kScour);
				result = bridge->CraftSelectedAffixes(action, a_instanceKey, a_token, a_signature, a_count);
			}
			return English(result, result.success ? "Item crafting action completed." : "Affix action failed.");
		}

		Outcome ExpandAffix(std::uint64_t a_instanceKey, std::uint32_t a_count, const std::string& a_signature)
		{
			bool raceAllowed = false;
			auto outcome = RaceGate(raceAllowed);
			auto* bridge = EventBridge::GetSingleton();
			EventBridge::OperationResult result{ false, raceAllowed ? "Invalid Runeforge expansion request." : outcome.message };
			if (raceAllowed && bridge && bridge->IsFeasrRuneforgeContractCompatible() && a_count <= 0xFFu) {
				result = bridge->ExpandSelectedRunewordBaseAffixes(a_instanceKey, static_cast<std::uint8_t>(a_count), a_signature);
			}
			return English(result, result.success ? "An affix slot was added." : "Affix expansion failed.");
		}

		Outcome ExchangeCurrency(bool a_scouringOrb)
		{
			bool raceAllowed = false;
			auto outcome = RaceGate(raceAllowed);
			auto* bridge = EventBridge::GetSingleton();
			EventBridge::OperationResult result{ false, raceAllowed ? "Invalid Runeforge currency exchange." : outcome.message };
			if (raceAllowed && bridge && bridge->IsFeasrRuneforgeContractCompatible()) {
				result = bridge->ExchangeCraftingCurrency(a_scouringOrb ? CurrencyExchange::kScouringOrb : CurrencyExchange::kReforgeOrb);
			}
			return English(result, result.success ? "Currency exchanged." : "Currency exchange failed.");
		}

		RE::TESBoundObject* ResolveBase(std::uint64_t a_instanceKey)
		{
			return ResolveRuneforgeStageBase(a_instanceKey);
		}

		std::string GlyphSequence(const std::string& a_recipeId)
		{
			return ResolveRuneforgeStageGlyphSequence(EventBridge::GetSingleton(), a_recipeId);
		}

		nlohmann::json ItemEffectCard(std::uint64_t a_instanceKey, bool a_worn)
		{
			// Read-only: derives the card of the Runeword engraved on that instance; never selects or stages anything.
			auto* bridge = EventBridge::GetSingleton();
			if (!bridge || a_instanceKey == 0u || !bridge->IsFeasrRuneforgeContractCompatible()) {
				return nlohmann::json::object();
			}
			const auto card = bridge->GetInstanceRunewordEffectCard(a_instanceKey, a_worn);
			return card ? RunewordEffectCardPolicy::ToJson<nlohmann::json>(*card) : nlohmann::json::object();
		}

		void SetDebugMode(bool a_debugMode)
		{
			g_debugMode = a_debugMode;
		}
	}
}
