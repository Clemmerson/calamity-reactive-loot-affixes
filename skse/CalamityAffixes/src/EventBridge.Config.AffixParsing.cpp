#include "CalamityAffixes/EventBridge.h"

#include "CalamityAffixes/AffixParsingPolicy.h"
#include "CalamityAffixes/PluginEditorIds.h"
#include "EventBridge.Config.Shared.h"

#include <algorithm>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace CalamityAffixes
{
	namespace
	{
		using ConfigShared::Trim;

		std::string DeriveAffixLabel(std::string_view a_displayName)
		{
			// Typical convention: "Label (details): description".
			// We want a short label for list display / renaming.
			auto text = Trim(a_displayName);
			if (text.empty()) {
				return {};
			}

			const auto posColon = text.find(':');
			const auto posParen = text.find('(');

			std::size_t cut = std::string_view::npos;
			if (posColon != std::string_view::npos) {
				cut = posColon;
			}
			if (posParen != std::string_view::npos) {
				cut = (cut == std::string_view::npos) ? posParen : std::min(cut, posParen);
			}

			if (cut != std::string_view::npos) {
				text = text.substr(0, cut);
			}

			text = Trim(text);
			std::string label(text);

			// Some UI/font stacks render arrow glyphs as tofu squares.
			// Normalize into ASCII so item labels remain readable in all menus.
			ConfigShared::ReplaceAll(label, "→", "->");
			ConfigShared::ReplaceAll(label, "⇒", "->");
			ConfigShared::ReplaceAll(label, "➜", "->");
			ConfigShared::ReplaceAll(label, "↔", "<->");
			ConfigShared::ReplaceAll(label, "⟶", "->");
			return label;
		}
	}

	bool EventBridge::InitializeAffixFromJson(const nlohmann::json& a_affix, AffixRuntime& a_out) const
	{
		a_out = AffixRuntime{};
		a_out.id = a_affix.value("id", std::string{});
		a_out.token = a_out.id.empty() ? 0u : MakeAffixToken(a_out.id);
		a_out.action.sourceToken = a_out.token;
		a_out.keywordEditorId = a_affix.value("editorId", std::string{});
		if (a_out.keywordEditorId.empty()) {
			return false;
		}

		a_out.displayName = a_affix.value("name", std::string{});
		if (a_out.displayName.empty()) {
			a_out.displayName = a_out.keywordEditorId;
		}
		a_out.displayNameEn = a_affix.value("nameEn", std::string{});
		a_out.displayNameKo = a_affix.value("nameKo", std::string{});
		if (a_out.displayNameEn.empty()) {
			// Legacy `name` is Korean in existing configs. Use the stable ASCII ID
			// when an explicit English name is unavailable.
			a_out.displayNameEn = a_out.id;
		}
		if (a_out.displayNameKo.empty()) {
			a_out.displayNameKo = a_out.displayName;
		}
		a_out.label = DeriveAffixLabel(a_out.displayName);
		if (a_out.label.empty()) {
			a_out.label = a_out.displayName;
		}

		a_out.keyword = PluginEditorIds::Lookup<RE::BGSKeyword>(a_out.keywordEditorId);
		return a_out.keyword != nullptr;
	}

	void EventBridge::ApplyAffixSlotAndFamilyFromJson(const nlohmann::json& a_affix, AffixRuntime& a_out) const
	{
		auto parseAffixSlot = [](std::string_view a_slot) -> AffixSlot {
			if (a_slot == "Prefix" || a_slot == "prefix") {
				return AffixSlot::kPrefix;
			}
			if (a_slot == "Suffix" || a_slot == "suffix") {
				return AffixSlot::kSuffix;
			}
			return AffixSlot::kNone;
		};

		const auto slotIt = a_affix.find("slot");
		if (slotIt != a_affix.end() && slotIt->is_string()) {
			a_out.slot = parseAffixSlot(slotIt->get<std::string>());
		}

		const auto famIt = a_affix.find("family");
		if (famIt != a_affix.end() && famIt->is_string()) {
			a_out.family = famIt->get<std::string>();
		}

		const auto subtypeIt = a_affix.find("weaponSubtypes");
		if (subtypeIt == a_affix.end()) {
			return;
		}

		if (a_out.slot != AffixSlot::kSuffix || !subtypeIt->is_array() || subtypeIt->empty()) {
			a_out.weaponSubtypeMask = detail::kWeaponSubtypeInvalid;
			SKSE::log::error(
				"CalamityAffixes: weaponSubtypes requires a non-empty array on a suffix affix (affixId={}).",
				a_out.id);
			return;
		}

		std::uint8_t parsedMask = 0u;
		for (const auto& subtype : *subtypeIt) {
			if (!subtype.is_string()) {
				parsedMask |= detail::kWeaponSubtypeInvalid;
				continue;
			}
			const auto key = subtype.get<std::string>();
			const auto bit = detail::WeaponSubtypeMaskForContractKey(key);
			if (bit == 0u || (parsedMask & bit) != 0u) {
				parsedMask |= detail::kWeaponSubtypeInvalid;
				SKSE::log::error(
					"CalamityAffixes: invalid or duplicate weaponSubtypes value '{}' (affixId={}).",
					key,
					a_out.id);
				continue;
			}
			parsedMask |= bit;
		}
		a_out.weaponSubtypeMask = parsedMask == 0u ? detail::kWeaponSubtypeInvalid : parsedMask;
	}

	void EventBridge::ApplyAffixKidLootFromJson(const nlohmann::json& a_affix, AffixRuntime& a_out, float& a_outKidChancePct) const
	{
		const auto& kid = a_affix.value("kid", nlohmann::json::object());
		a_outKidChancePct = kid.is_object() ?
			static_cast<float>(kid.value("chance", 0.0)) :
			0.0f;

		// Loot-time drop weighting is sourced from explicit loot weight or legacy kid.chance.
		// Keep this independent from runtime trigger proc chance.
		a_out.lootWeight = detail::ResolveAffixKidLootWeight(a_out.lootWeight, a_outKidChancePct);

		if (!kid.is_object()) {
			return;
		}

		const auto type = kid.value("type", std::string{});
		a_out.lootType = ParseLootItemType(type);
	}

	bool EventBridge::ApplyAffixRuntimeGateFromJson(
		const nlohmann::json& a_runtime,
		const nlohmann::json& a_action,
		float a_kidChancePct,
		std::string_view a_actionType,
		AffixRuntime& a_out) const
	{
		// Parse passiveSpell for ANY slot (prefix, suffix, runeword).
		const auto passiveSpellId = a_action.value("passiveSpellEditorId", std::string{});
		if (!passiveSpellId.empty()) {
			a_out.passiveSpell = PluginEditorIds::Lookup<RE::SpellItem>(passiveSpellId);
			if (!a_out.passiveSpell) {
				SKSE::log::warn(
					"CalamityAffixes: passive spell not found (affixId={}, spellEditorId={}).",
					a_out.id, passiveSpellId);
			}
		}
		a_out.refreshPassiveSpellOnPostLoad =
			a_action.value("refreshPassiveSpellOnPostLoad", false) && a_out.passiveSpell != nullptr;
		if (a_action.value("refreshPassiveSpellOnPostLoad", false) && !a_out.passiveSpell) {
			SKSE::log::warn(
				"CalamityAffixes: passive refresh ignored without a resolved passive spell (affixId={}, spellEditorId={}).",
				a_out.id, passiveSpellId);
		}

		if (a_out.slot == AffixSlot::kSuffix) {
			a_out.trigger = Trigger::kHit;
			a_out.procChancePct = detail::ResolveSuffixProcChancePct();
			a_out.critDamageBonusPct = static_cast<float>(a_action.value("critDamageBonusPct", 0.0));
			return true;
		}

		return ApplyRuntimeTriggerConfigFromJson(a_runtime, a_actionType, a_out);
	}

	void EventBridge::ParseActionFeedbackFromJson(
		const nlohmann::json& a_action,
		RE::TESDataHandler* a_handler,
		AffixRuntime& a_out) const
	{
		const auto feedbackIt = a_action.find("feedback");
		if (feedbackIt == a_action.end()) {
			return;
		}
		if (!feedbackIt->is_object()) {
			SKSE::log::warn(
				"CalamityAffixes: runtime.action.feedback must be an object; feedback disabled (affixId={}).",
				a_out.id);
			return;
		}

		const auto& feedback = *feedbackIt;
		auto readString = [&feedback](std::string_view a_key) -> std::string {
			const auto it = feedback.find(std::string(a_key));
			return (it != feedback.end() && it->is_string()) ? it->get<std::string>() : std::string{};
		};

		const auto playOn = readString("playOn");
		if (playOn == "Proc") {
			a_out.action.feedback.playOn = ActionFeedbackPlayOn::kProc;
		} else if (playOn == "PassiveAdd") {
			a_out.action.feedback.playOn = ActionFeedbackPlayOn::kPassiveAdd;
		} else {
			SKSE::log::warn(
				"CalamityAffixes: unknown feedback.playOn '{}'; feedback disabled (affixId={}).",
				playOn,
				a_out.id);
			return;
		}

		const auto target = readString("target");
		if (target == "Owner") {
			a_out.action.feedback.target = ActionFeedbackTarget::kOwner;
		} else if (target == "Target") {
			a_out.action.feedback.target = ActionFeedbackTarget::kTarget;
		} else if (target == "Corpse") {
			a_out.action.feedback.target = ActionFeedbackTarget::kCorpse;
		} else {
			SKSE::log::warn(
				"CalamityAffixes: unknown feedback.target '{}'; feedback disabled (affixId={}).",
				target,
				a_out.id);
			a_out.action.feedback.playOn = ActionFeedbackPlayOn::kNone;
			return;
		}

		float durationSeconds = 0.25f;
		if (const auto durationIt = feedback.find("durationSeconds");
			durationIt != feedback.end() && durationIt->is_number()) {
			durationSeconds = durationIt->get<float>();
		}
		a_out.action.feedback.durationSeconds = std::clamp(durationSeconds, 0.05f, 2.0f);
		a_out.action.feedback.spatialSound = feedback.value("spatialSound", false);

		const auto artSpec = readString("artObjectEditorId");
		if (!artSpec.empty()) {
			a_out.action.feedback.art = ConfigShared::LookupFormFromSpec<RE::BGSArtObject>(artSpec, a_handler);
			if (!a_out.action.feedback.art) {
				SKSE::log::warn(
					"CalamityAffixes: feedback ArtObject not found; visual feedback skipped (affixId={}, art={}).",
					a_out.id,
					artSpec);
			}
		}

		const auto soundSpec = readString("soundForm");
		if (!soundSpec.empty()) {
			a_out.action.feedback.sound = ConfigShared::LookupFormFromSpec<RE::BGSSoundDescriptorForm>(soundSpec, a_handler);
			if (!a_out.action.feedback.sound) {
				SKSE::log::warn(
					"CalamityAffixes: feedback sound not found; audio feedback skipped (affixId={}, sound={}).",
					a_out.id,
					soundSpec);
			}
		}
	}

	void EventBridge::ParseTrapFeedbackFromJson(
		const nlohmann::json& a_action,
		RE::TESDataHandler* a_handler,
		AffixRuntime& a_out) const
	{
		const auto feedbackIt = a_action.find("trapFeedback");
		if (feedbackIt == a_action.end() || !feedbackIt->is_object() || a_out.action.type != ActionType::kSpawnTrap) {
			return;
		}

		const auto& feedback = *feedbackIt;
		const auto markerSpec = feedback.value("markerArtObjectEditorId", std::string{});
		const auto markerWorldObjectSpec = feedback.value("markerWorldObjectEditorId", std::string{});
		if (!markerSpec.empty()) {
			a_out.action.trapFeedback.markerArt = ConfigShared::LookupFormFromSpec<RE::BGSArtObject>(markerSpec, a_handler);
		}
		if (!markerWorldObjectSpec.empty()) {
			a_out.action.trapFeedback.markerWorldObject =
				ConfigShared::LookupFormFromSpec<RE::TESObjectSTAT>(markerWorldObjectSpec, a_handler);
		}
		if (!markerSpec.empty() && !a_out.action.trapFeedback.markerArt) {
			SKSE::log::warn(
				"CalamityAffixes: trap marker ArtObject not found; particle marker unavailable (affixId={}, art={}).",
				a_out.id,
				markerSpec);
		}
		if (!markerWorldObjectSpec.empty() && !a_out.action.trapFeedback.markerWorldObject) {
			SKSE::log::warn(
				"CalamityAffixes: trap marker world object not found or not Static-compatible (STAT/MSTT); placed marker unavailable (affixId={}, object={}).",
				a_out.id,
				markerWorldObjectSpec);
		}
		if (!a_out.action.trapFeedback.markerArt && !a_out.action.trapFeedback.markerWorldObject) {
			SKSE::log::warn(
				"CalamityAffixes: trap feedback has no usable marker; trap feedback disabled (affixId={}).",
				a_out.id);
			return;
		}

		if (const auto animationIt = feedback.find("worldMarkerAnimation");
			animationIt != feedback.end() && animationIt->is_object()) {
			const auto readAnimationEvent = [&](std::string_view a_name) {
				const auto eventIt = animationIt->find(std::string(a_name));
				return eventIt != animationIt->end() && eventIt->is_string() ?
					eventIt->get<std::string>() : std::string{};
			};
			TrapWorldMarkerAnimation animation{
				.initialEvent = readAnimationEvent("initialEvent"),
				.triggerEvent = readAnimationEvent("triggerEvent"),
				.rearmEvent = readAnimationEvent("rearmEvent")
			};
			if (const auto gateIt = animationIt->find("openGateMilliseconds");
				gateIt != animationIt->end() && gateIt->is_number_integer()) {
				const auto rawGate = gateIt->get<std::int64_t>();
				animation.openGateMilliseconds = static_cast<std::uint32_t>(
					std::clamp<std::int64_t>(rawGate, 0, 5000));
			}

			const bool complete = !animation.initialEvent.empty() &&
				!animation.triggerEvent.empty() && !animation.rearmEvent.empty();
			if (!a_out.action.trapFeedback.markerWorldObject) {
				SKSE::log::warn(
					"CalamityAffixes: trap world-marker animation ignored without a Static-compatible world marker (affixId={}).",
					a_out.id);
			} else if (!complete) {
				SKSE::log::warn(
					"CalamityAffixes: incomplete trap world-marker animation ignored (affixId={}).",
					a_out.id);
			} else {
				a_out.action.trapFeedback.worldMarkerAnimation = std::move(animation);
			}
		}
		a_out.action.trapFeedback.unarmedScale = std::clamp(feedback.value("unarmedScale", 0.75f), 0.1f, 4.0f);
		a_out.action.trapFeedback.armedScale = std::clamp(feedback.value("armedScale", 1.0f), 0.1f, 4.0f);

		auto parseCue = [&](std::string_view a_name, TrapFeedbackCue& a_cue) {
			const auto cueIt = feedback.find(std::string(a_name));
			if (cueIt == feedback.end() || !cueIt->is_object()) {
				return;
			}
			const auto artSpec = cueIt->value("artObjectEditorId", std::string{});
			const auto soundSpec = cueIt->value("soundForm", std::string{});
			if (!artSpec.empty()) {
				a_cue.art = ConfigShared::LookupFormFromSpec<RE::BGSArtObject>(artSpec, a_handler);
			}
			if (!soundSpec.empty()) {
				a_cue.sound = ConfigShared::LookupFormFromSpec<RE::BGSSoundDescriptorForm>(soundSpec, a_handler);
			}
			a_cue.durationSeconds = std::clamp(cueIt->value("durationSeconds", 0.0f), 0.0f, 2.0f);
			a_cue.scale = std::clamp(cueIt->value("scale", 1.0f), 0.1f, 4.0f);
		};

		parseCue("placed", a_out.action.trapFeedback.placed);
		parseCue("armed", a_out.action.trapFeedback.armed);
		parseCue("triggered", a_out.action.trapFeedback.triggered);
		parseCue("expired", a_out.action.trapFeedback.expired);
		a_out.action.trapFeedback.configured = true;
	}

	void EventBridge::ApplyScrollNoConsumeChanceFromJson(const nlohmann::json& a_action, AffixRuntime& a_out) const
	{
		const float rawScrollNoConsumeChancePct = static_cast<float>(a_action.value("scrollNoConsumeChancePct", 0.0));
		a_out.scrollNoConsumeChancePct = std::clamp(rawScrollNoConsumeChancePct, 0.0f, 100.0f);
	}

	void EventBridge::NormalizeParsedAffixRuntimePolicy(AffixRuntime& a_out, std::string_view a_actionType) const
	{
		const bool isSpecialAction =
			a_out.action.type == ActionType::kCastOnCrit ||
			a_out.action.type == ActionType::kConvertDamage ||
			a_out.action.type == ActionType::kMindOverMatter ||
			a_out.action.type == ActionType::kArchmage ||
			a_out.action.type == ActionType::kCorpseExplosion ||
			a_out.action.type == ActionType::kSummonCorpseExplosion;

		if (a_out.luckyHitChancePct > 0.0f) {
			bool luckyHitSupported = false;
			switch (a_out.action.type) {
			case ActionType::kCastOnCrit:
			case ActionType::kConvertDamage:
			case ActionType::kMindOverMatter:
			case ActionType::kArchmage:
				luckyHitSupported = true;
				break;
			case ActionType::kCorpseExplosion:
			case ActionType::kSummonCorpseExplosion:
				luckyHitSupported = false;
				break;
			default:
				luckyHitSupported = (a_out.trigger == Trigger::kHit || a_out.trigger == Trigger::kIncomingHit);
				break;
			}

			if (!luckyHitSupported) {
				SKSE::log::warn(
					"CalamityAffixes: luckyHitChancePercent is ignored for unsupported affix context (affixId={}, actionType={}, trigger={}).",
					a_out.id,
					a_actionType,
					static_cast<std::uint32_t>(a_out.trigger));
				a_out.luckyHitChancePct = 0.0f;
				a_out.luckyHitProcCoefficient = 1.0f;
			}
		}

		if (isSpecialAction && a_out.perTargetIcd.count() > 0) {
			SKSE::log::warn(
				"CalamityAffixes: perTargetICDSeconds is ignored for special action (affixId={}, actionType={}).",
				a_out.id,
				a_actionType);
		}
	}

	void EventBridge::HarvestRecordDisplayNames(const nlohmann::json& a_affix, RE::TESDataHandler* a_handler)
	{
		// ESP FULL names are ASCII-only by design (ToPluginSafeName strips the Korean
		// half), so player-facing text that wants localized record names has to read
		// them from the config's records specs instead of the loaded forms.
		const auto recordsIt = a_affix.find("records");
		if (recordsIt == a_affix.end() || !recordsIt->is_object()) {
			return;
		}

		auto harvest = [&](const nlohmann::json& a_record, bool a_isSpell) {
			if (!a_record.is_object()) {
				return;
			}
			const auto name = a_record.value("name", std::string{});
			const auto editorId = a_record.value("editorId", std::string{});
			if (name.empty() || editorId.empty()) {
				return;
			}

			std::string english = name;
			std::string korean;
			if (const auto sep = name.find(" / "); sep != std::string::npos) {
				english = name.substr(0, sep);
				korean = name.substr(sep + 3);
			}
			constexpr std::string_view kCalamityPrefix = "Calamity: ";
			if (english.rfind(kCalamityPrefix.data(), 0) == 0) {
				english.erase(0, kCalamityPrefix.size());
			}

			const RE::TESForm* form = a_isSpell ?
				static_cast<const RE::TESForm*>(ConfigShared::LookupFormFromSpec<RE::SpellItem>(editorId, a_handler)) :
				static_cast<const RE::TESForm*>(ConfigShared::LookupFormFromSpec<RE::EffectSetting>(editorId, a_handler));
			if (!form) {
				return;
			}
			_affixRuntimeState.recordDisplayNames[form->GetFormID()] = { std::move(english), std::move(korean) };
		};

		if (const auto it = recordsIt->find("magicEffect"); it != recordsIt->end()) {
			harvest(*it, false);
		}
		if (const auto it = recordsIt->find("spell"); it != recordsIt->end()) {
			harvest(*it, true);
		}
		if (const auto it = recordsIt->find("magicEffects"); it != recordsIt->end() && it->is_array()) {
			for (const auto& record : *it) {
				harvest(record, false);
			}
		}
		if (const auto it = recordsIt->find("spells"); it != recordsIt->end() && it->is_array()) {
			for (const auto& record : *it) {
				harvest(record, true);
			}
		}
	}

	std::string EventBridge::ResolveRecordDisplayName(
		const RE::TESForm* a_form,
		std::string_view a_fallback,
		bool a_korean) const
	{
		if (a_form) {
			if (const auto it = _affixRuntimeState.recordDisplayNames.find(a_form->GetFormID());
				it != _affixRuntimeState.recordDisplayNames.end()) {
				const auto& [english, korean] = it->second;
				if (a_korean && !korean.empty()) {
					return korean;
				}
				if (!english.empty()) {
					return english;
				}
			}
		}
		return std::string(a_fallback);
	}

	void EventBridge::ParseConfiguredAffixesFromJson(const nlohmann::json& a_affixes, RE::TESDataHandler* a_handler)
	{
		// Strip null values from all affix entries before parsing.
		// nlohmann::json::value() throws type_error::302 when a key exists with null value.
		auto affixes = a_affixes;
		ConfigShared::StripNullValues(affixes);

		for (const auto& a : affixes) {
			if (!a.is_object()) {
				continue;
			}

			HarvestRecordDisplayNames(a, a_handler);

			try {
				AffixRuntime out{};
				if (!InitializeAffixFromJson(a, out)) {
					continue;
				}

				ApplyAffixSlotAndFamilyFromJson(a, out);

				float kidChancePct = 0.0f;
				ApplyAffixKidLootFromJson(a, out, kidChancePct);

				const auto& runtime = a.value("runtime", nlohmann::json::object());
				if (!runtime.is_object()) {
					continue;
				}

				const auto& action = runtime.value("action", nlohmann::json::object());
				if (!action.is_object()) {
					SKSE::log::warn(
						"CalamityAffixes: runtime.action must be an object (affixId={}); skipping affix.",
						out.id);
					continue;
				}
				out.action.debugNotify = action.value("debugNotify", false);
				const auto type = action.value("type", std::string{});

				if (!ApplyAffixRuntimeGateFromJson(runtime, action, kidChancePct, type, out)) {
					continue;
				}

				ApplyScrollNoConsumeChanceFromJson(action, out);

				if (!ParseRuntimeActionFromJson(action, type, a_handler, out)) {
					continue;
				}
				ParseActionFeedbackFromJson(action, a_handler, out);
				ParseTrapFeedbackFromJson(action, a_handler, out);

				NormalizeParsedAffixRuntimePolicy(out, type);

				_affixRuntimeState.affixes.push_back(std::move(out));
				const auto idx = _affixRuntimeState.affixes.size() - 1;

				IndexAffixTriggerBucket(_affixRuntimeState.affixes[idx], idx);
				IndexAffixSpecialActionBucket(_affixRuntimeState.affixes[idx], idx);
			} catch (const nlohmann::json::exception& e) {
				SKSE::log::error(
					"CalamityAffixes: JSON error while parsing affix entry; skipping. ({})",
					e.what());
			}
		}
	}
}
