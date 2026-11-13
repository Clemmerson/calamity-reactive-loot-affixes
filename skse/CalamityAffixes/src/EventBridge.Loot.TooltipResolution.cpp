#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/LootUiGuards.h"
#include "CalamityAffixes/ProcChancePolicy.h"
#include "CalamityAffixes/RunewordEffectDetailPolicy.h"
#include "EventBridge.Loot.Runeword.SummaryText.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace CalamityAffixes
{
	namespace
	{
		[[nodiscard]] bool IsPreviewItemSource(std::string_view a_source) noexcept
		{
			return a_source == "barter" || a_source == "container" || a_source == "gift";
		}

		[[nodiscard]] bool IsSynthesizedRunewordAffixId(std::string_view a_affixId) noexcept
		{
			if (a_affixId.size() < 5 || a_affixId.substr(a_affixId.size() - 5) != "_auto") {
				return false;
			}

			// Synthesized runtime runewords are currently emitted as rw_*_auto.
			// Keep runeword_*_auto for backward compatibility with legacy/generated ids.
			return a_affixId.rfind("rw_", 0) == 0 || a_affixId.rfind("runeword_", 0) == 0;
		}
	}

	std::optional<std::string> EventBridge::GetInstanceAffixTooltip(
		const RE::InventoryEntryData* a_item,
		std::string_view a_selectedDisplayName,
		int a_uiLanguageMode,
		std::string_view a_itemSource,
		RE::FormID a_sourceContainerFormID,
		std::uint64_t a_preferredInstanceKey)
	{
		const std::scoped_lock lock(_stateMutex);
		if (!_configLoaded) {
			return std::nullopt;
		}
		if (!a_item) {
			return std::nullopt;
		}

		if (!a_item->extraLists) {
			return std::nullopt;
		}

		std::optional<LootItemType> itemType{};
		if (a_item->object && a_item->object->As<RE::TESObjectWEAP>()) {
			itemType = LootItemType::kWeapon;
		} else if (a_item->object && a_item->object->As<RE::TESObjectARMO>()) {
			itemType = LootItemType::kArmor;
		}

		// Preview-time synthetic affix rolls are disabled by policy.
		// Tooltip now surfaces only already-materialized instance data.
		const bool allowPreview = ShouldEnableSyntheticLootPreviewTooltip() &&
			itemType.has_value() &&
			IsPreviewItemSource(a_itemSource) &&
			IsLootObjectEligibleForAffixes(a_item->object);
		(void)a_sourceContainerFormID;

		struct TooltipCandidate
		{
			std::uint64_t instanceKey{ 0u };
			std::uint64_t primaryToken{ 0u };
			std::string rowName{};
			InstanceAffixSlots slots{};
			bool preview{ false };
			bool worn{ false };
		};

		std::vector<TooltipCandidate> candidates;
		for (auto* xList : *a_item->extraLists) {
			if (!xList) {
				continue;
			}

			auto* uid = xList->GetByType<RE::ExtraUniqueID>();
			if (!uid && allowPreview) {
				auto* player = RE::PlayerCharacter::GetSingleton();
				auto* changes = player ? player->GetInventoryChanges() : nullptr;
				if (changes) {
					auto* created = RE::BSExtraData::Create<RE::ExtraUniqueID>();
					if (created) {
						created->baseID = a_sourceContainerFormID != 0u ?
							a_sourceContainerFormID : player->GetFormID();
						created->uniqueID = changes->GetNextUniqueID();
						xList->Add(created);
						uid = created;
					}
				}
			}

			if (!uid) {
				continue;
			}

			const auto key = MakeInstanceKey(uid->baseID, uid->uniqueID);

			TooltipCandidate candidate{};
			candidate.instanceKey = key;
			candidate.preview = false;
			candidate.worn = xList->HasType<RE::ExtraWorn>() || xList->HasType<RE::ExtraWornLeft>();
			candidate.slots.Clear();

			if (const auto mappedIt = _instanceTrackingState.instanceAffixes.find(key); mappedIt != _instanceTrackingState.instanceAffixes.end()) {
				candidate.slots = mappedIt->second;
			}

			if (candidate.slots.count == 0 && allowPreview) {
				if (const auto* previewSlots = FindLootPreviewSlots(key)) {
					candidate.slots = *previewSlots;
					candidate.preview = true;
				} else if (const auto generatedPreview = BuildLootPreviewAffixSlots(
					key,
					*itemType,
					detail::ResolveWeaponSubtype(a_item->object->As<RE::TESObjectWEAP>()));
					generatedPreview.has_value()) {
					RememberLootPreviewSlots(key, *generatedPreview);
					if (const auto* storedPreview = FindLootPreviewSlots(key)) {
						candidate.slots = *storedPreview;
						candidate.preview = true;
					}
				}
			}

			// For normal runtime candidates, skip items without mapped slots.
			// For preview mode, keep zero-slot candidates so users can see "no affix" before pickup.
			if (candidate.slots.count == 0 && !candidate.preview) {
				continue;
			}

			candidate.primaryToken = candidate.slots.GetPrimary();
			const char* rowNameRaw = xList->GetDisplayName(a_item->object);
			candidate.rowName = rowNameRaw ? rowNameRaw : "";
			candidates.push_back(std::move(candidate));
		}

		if (candidates.empty()) {
			return std::nullopt;
		}

		auto formatAffixDetail = [
			this,
			a_uiLanguageMode,
			itemType
		](const AffixRuntime& a_affix, std::size_t a_affixIndex, const TooltipCandidate& a_candidate,
			bool a_includeRunewordSummary = true) -> std::string {
			std::string detail;
			const auto* state = FindInstanceRuntimeState(a_candidate.instanceKey, a_affix.token);
			const auto& action = a_affix.action;

			if (action.modeCycleEnabled && !action.modeCycleSpells.empty()) {
				const bool adaptiveManualMode =
					(action.type == ActionType::kCastSpellAdaptiveElement) && action.modeCycleManualOnly;
				const auto modeCount = adaptiveManualMode ?
					static_cast<std::uint32_t>(action.modeCycleSpells.size() + 1u) :
					static_cast<std::uint32_t>(action.modeCycleSpells.size());
				const std::uint32_t idx = modeCount > 0u ? ((state ? state->modeIndex : 0u) % modeCount) : 0u;

				detail.append("Mode ");
				if (adaptiveManualMode && idx == 0u) {
					detail.append("Auto");
				} else {
					const std::size_t labelIdx = adaptiveManualMode ?
						static_cast<std::size_t>(idx - 1u) :
						static_cast<std::size_t>(idx);
					if (labelIdx < action.modeCycleLabels.size() && !action.modeCycleLabels[labelIdx].empty()) {
						detail.append(action.modeCycleLabels[labelIdx]);
					} else {
						const auto displayIndex = adaptiveManualMode ? idx : (idx + 1u);
						const auto displayCount = adaptiveManualMode ?
							static_cast<std::uint32_t>(action.modeCycleSpells.size()) :
							modeCount;
						detail.append(std::to_string(displayIndex));
						detail.push_back('/');
						detail.append(std::to_string(displayCount));
					}
				}
				if (action.modeCycleManualOnly) {
					detail.append(" (Manual)");
				}
			}

			auto formatFloat1 = [](float a_value) {
				char buf[32]{};
				std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(a_value));
				return std::string(buf);
			};

			auto triggerTextEn = [](Trigger a_trigger) -> std::string_view {
				switch (a_trigger) {
				case Trigger::kHit:
					return "On hit";
				case Trigger::kIncomingHit:
					return "On taking a hit";
				case Trigger::kDotApply:
					return "On DoT apply";
				case Trigger::kKill:
					return "On kill";
				case Trigger::kLowHealth:
					return "On low health";
				}
				return "On trigger";
			};

			auto triggerTextKo = [](Trigger a_trigger) -> std::string_view {
				switch (a_trigger) {
				case Trigger::kHit:
					return "적중 시";
				case Trigger::kIncomingHit:
					return "피격 시";
				case Trigger::kDotApply:
					return "지속 피해 적용 시";
				case Trigger::kKill:
					return "처치 시";
				case Trigger::kLowHealth:
					return "저체력 시";
				}
				return "트리거 시";
			};

			auto formatNumberCompact = [&](float a_value) {
				std::string text = formatFloat1(a_value);
				if (text.size() >= 2 && text.compare(text.size() - 2, 2, ".0") == 0) {
					text.erase(text.size() - 2);
				}
				return text;
			};

			auto maxSpellDurationSeconds = [](RE::SpellItem* a_spell) {
				if (!a_spell) {
					return 0.0f;
				}
				float maxDuration = 0.0f;
				for (const auto* effect : a_spell->effects) {
					if (!effect || effect->effectItem.duration == 0u) {
						continue;
					}
					maxDuration = std::max(maxDuration, static_cast<float>(effect->effectItem.duration));
				}
				return maxDuration;
			};

			auto actionDurationSeconds = [&](const Action& a_action) {
				switch (a_action.type) {
				case ActionType::kCastSpell:
					return maxSpellDurationSeconds(a_action.spell);
				case ActionType::kCastOnCrit:
					return maxSpellDurationSeconds(a_action.spell);
				case ActionType::kCastSpellAdaptiveElement: {
					float maxDuration = 0.0f;
					if (a_action.adaptiveFire) {
						maxDuration = std::max(maxDuration, maxSpellDurationSeconds(a_action.adaptiveFire));
					}
					if (a_action.adaptiveFrost) {
						maxDuration = std::max(maxDuration, maxSpellDurationSeconds(a_action.adaptiveFrost));
					}
					if (a_action.adaptiveShock) {
						maxDuration = std::max(maxDuration, maxSpellDurationSeconds(a_action.adaptiveShock));
					}
					return maxDuration;
				}
				case ActionType::kSpawnTrap:
					if (a_action.trapTtl > std::chrono::milliseconds::zero()) {
						return static_cast<float>(a_action.trapTtl.count()) / 1000.0f;
					}
					return maxSpellDurationSeconds(a_action.spell);
				default:
					return 0.0f;
				}
			};

			auto spellNameOr = [](RE::SpellItem* a_spell, std::string_view a_fallback) {
				if (!a_spell) {
					return std::string(a_fallback);
				}
				const char* rawName = a_spell->GetName();
				if (!rawName || rawName[0] == '\0') {
					return std::string(a_fallback);
				}
				return std::string(rawName);
			};

			auto buildRunewordAutoSummary = [&](const AffixRuntime& a_autoAffix) -> std::string {
				if (!IsSynthesizedRunewordAffixId(a_autoAffix.id)) {
					return {};
				}

				RunewordSummary::BaseType baseType = RunewordSummary::BaseType::kUnknown;
				if (itemType.has_value()) {
					baseType = (*itemType == LootItemType::kWeapon) ?
					               RunewordSummary::BaseType::kWeapon :
					               RunewordSummary::BaseType::kArmor;
				}

				std::string_view recipeId = a_autoAffix.id;
				if (recipeId.size() > 5 && recipeId.substr(recipeId.size() - 5) == "_auto") {
					recipeId = recipeId.substr(0, recipeId.size() - 5);
				}
				const auto summaryKey = RunewordSummary::ResolveEffectSummaryKey(recipeId, baseType);

				const auto chancePct = std::clamp(a_autoAffix.procChancePct, 0.0f, 100.0f);
				const auto icdSec = static_cast<float>(a_autoAffix.icd.count()) / 1000.0f;
				const auto perTargetIcdSec = static_cast<float>(a_autoAffix.perTargetIcd.count()) / 1000.0f;
				const auto durationSec = actionDurationSeconds(a_autoAffix.action);

				std::string effectEn;
				std::string effectKo;
				switch (a_autoAffix.action.type) {
				case ActionType::kCastSpell: {
					const auto spellName = spellNameOr(a_autoAffix.action.spell, "Spell");
					if (a_autoAffix.action.applyToSelf) {
						effectEn = "Self-cast " + spellName;
						effectKo = "자신에게 " + spellName + " 시전";
					} else {
						effectEn = "Cast " + spellName;
						effectKo = "대상에게 " + spellName + " 시전";
					}
					break;
				}
				case ActionType::kCastSpellAdaptiveElement: {
					std::vector<std::string_view> elemsEn;
					std::vector<std::string_view> elemsKo;
					if (a_autoAffix.action.adaptiveFire) {
						elemsEn.push_back("Fire");
						elemsKo.push_back("화염");
					}
					if (a_autoAffix.action.adaptiveFrost) {
						elemsEn.push_back("Frost");
						elemsKo.push_back("냉기");
					}
					if (a_autoAffix.action.adaptiveShock) {
						elemsEn.push_back("Shock");
						elemsKo.push_back("번개");
					}

					auto joinElems = [](const std::vector<std::string_view>& a_values) {
						std::string out;
						for (std::size_t i = 0; i < a_values.size(); ++i) {
							if (i > 0) {
								out.append("/");
							}
							out.append(a_values[i]);
						}
						return out;
					};

					const auto elemEn = joinElems(elemsEn);
					const auto elemKo = joinElems(elemsKo);
					effectEn = elemEn.empty() ? "Adaptive elemental cast" : ("Adaptive elemental cast (" + elemEn + ")");
					effectKo = elemKo.empty() ? "적응형 원소 시전" : ("적응형 원소 시전 (" + elemKo + ")");
					break;
				}
				case ActionType::kSpawnTrap: {
					const auto spellName = spellNameOr(a_autoAffix.action.spell, "Trap");
					effectEn = "Deploy trap: " + spellName;
					effectKo = "함정 설치: " + spellName;
					break;
				}
				default:
					effectEn = "Auto-generated runeword effect";
					effectKo = "자동 생성된 룬워드 효과";
					break;
				}

				std::string headerEn;
				headerEn.append(triggerTextEn(a_autoAffix.trigger));
				headerEn.push_back(' ');
				if (chancePct > 0.0f) {
					headerEn.append(formatNumberCompact(chancePct));
					headerEn.append("% chance to ");
				} else {
					headerEn.append("Always ");
				}
				if (!effectEn.empty()) {
					headerEn.append(effectEn);
				} else {
					headerEn.append("trigger an effect");
				}
				if (icdSec > 0.0f) {
					headerEn.append(" (Cooldown ");
					headerEn.append(formatNumberCompact(icdSec));
					headerEn.append("s)");
				}
				if (perTargetIcdSec > 0.0f) {
					headerEn.append(" (Per-target cooldown ");
					headerEn.append(formatNumberCompact(perTargetIcdSec));
					headerEn.append("s)");
				}
				if (durationSec > 0.0f) {
					headerEn.append(" (Duration ");
					headerEn.append(formatNumberCompact(durationSec));
					headerEn.append("s)");
				}
				// Quoted spell record names carry the upstream recipe name.
				if (const auto* autoRecipe = FindRunewordRecipeByResultAffixToken(a_autoAffix.token)) {
					headerEn = RuneforgePresentation::PresentRunewordRecordText(
						headerEn,
						autoRecipe->id,
						autoRecipe->displayNameEn);
				}

				std::string headerKo;
				headerKo.append(triggerTextKo(a_autoAffix.trigger));
				headerKo.push_back(' ');
				if (chancePct > 0.0f) {
					headerKo.append(formatNumberCompact(chancePct));
					headerKo.append("% 확률로 ");
				} else {
					headerKo.append("항상 ");
				}
				const auto mappedKo = RunewordSummary::ActionSummaryTextByKey(summaryKey);
				if (!mappedKo.empty()) {
					headerKo.append(mappedKo);
				} else if (!effectKo.empty()) {
					headerKo.append(effectKo);
				} else {
					headerKo.append("효과 발동");
				}
				if (icdSec > 0.0f) {
					headerKo.append(" (쿨타임 ");
					headerKo.append(formatNumberCompact(icdSec));
					headerKo.append("초)");
				}
				if (perTargetIcdSec > 0.0f) {
					headerKo.append(" (대상별 쿨타임 ");
					headerKo.append(formatNumberCompact(perTargetIcdSec));
					headerKo.append("초)");
				}
				if (durationSec > 0.0f) {
					headerKo.append(" (지속 ");
					headerKo.append(formatNumberCompact(durationSec));
					headerKo.append("초)");
				}

				switch (a_uiLanguageMode) {
				case 0:
					return headerEn;
				case 1:
					return headerKo;
				case 2:
				default:
					return headerKo + " / " + headerEn;
				}
			};

			if (a_includeRunewordSummary) {
				const auto autoSummary = buildRunewordAutoSummary(a_affix);
				if (!autoSummary.empty()) {
					if (!detail.empty()) {
						detail.push_back('\n');
					}
					detail.append(autoSummary);
				}
			}

			auto appendSpellEffectProfile = [&](std::string_view a_prefix, RE::SpellItem* a_spell) {
				if (!a_spell) {
					return;
				}
				using RunewordEffectDetailPolicy::SpellEffect;
				std::vector<SpellEffect> effects;
				effects.reserve(3u);
				std::size_t totalEffectCount = 0u;
				for (const auto* effect : a_spell->effects) {
					if (!effect || !effect->baseEffect) {
						continue;
					}
					++totalEffectCount;
					if (effects.size() >= 3u) {
						continue;
					}
					const char* rawName = effect->baseEffect->GetName();
					const auto fallback = rawName && rawName[0] != '\0' ? std::string(rawName) : std::string("Effect");
					effects.push_back(SpellEffect{
						.name = ResolveRecordDisplayName(effect->baseEffect, fallback, a_uiLanguageMode == 1),
						.magnitude = effect->effectItem.magnitude,
						.durationSeconds = effect->effectItem.duration,
						.area = effect->effectItem.area
					});
				}
				auto profile = RunewordEffectDetailPolicy::FormatSpellEffects(
					effects, totalEffectCount, a_uiLanguageMode == 1);
				if (profile.empty()) {
					return;
				}
				if (!detail.empty()) {
					detail.push_back('\n');
				}
				if (!a_prefix.empty()) {
					detail.append(a_prefix);
					detail.append(":\n");
				}
				detail.append(profile);
			};

			if (FindRunewordRecipeByResultAffixToken(a_affix.token)) {
				switch (action.type) {
				case ActionType::kCastSpell:
				case ActionType::kCastOnCrit:
				case ActionType::kConvertDamage:
				case ActionType::kArchmage:
				case ActionType::kSpawnTrap:
				case ActionType::kCorpseExplosion:
				case ActionType::kSummonCorpseExplosion:
					appendSpellEffectProfile(a_uiLanguageMode == 1 ? "주문 효과" : "Spell effects", action.spell);
					break;
				case ActionType::kCastSpellAdaptiveElement:
					if (action.adaptiveFire) appendSpellEffectProfile(a_uiLanguageMode == 1 ? "화염" : "Fire", action.adaptiveFire);
					if (action.adaptiveFrost) appendSpellEffectProfile(a_uiLanguageMode == 1 ? "냉기" : "Frost", action.adaptiveFrost);
					if (action.adaptiveShock) appendSpellEffectProfile(a_uiLanguageMode == 1 ? "전격" : "Shock", action.adaptiveShock);
					break;
				case ActionType::kEchoStrike:
					detail.append(a_uiLanguageMode == 1 ?
						("그림자 타격 재시전: " + std::to_string(action.echoDelay.count() / 1000) + "초 뒤") :
						("Shadow echo repeats after " + formatNumberCompact(static_cast<float>(action.echoDelay.count()) / 1000.0f) + "s"));
					break;
				default:
					break;
				}
				if (a_affix.passiveSpell && a_affix.passiveSpell != action.spell) {
					appendSpellEffectProfile(a_uiLanguageMode == 1 ? "상시 효과" : "Passive effects", a_affix.passiveSpell);
				}
			}

			// R8.7.4.21 I3: the chance numbers come from ResolveInstanceProcChanceContext (shared with the Runeword effect
			// card); the wording below is unchanged.
			if (const auto chance = ResolveInstanceProcChanceContext(
					a_affix, a_affixIndex, a_candidate.instanceKey, a_candidate.slots, a_candidate.worn);
				chance.mode != RunewordEffectCardPolicy::ChanceMode::kNone) {
				const bool useCurrentChance = chance.mode == RunewordEffectCardPolicy::ChanceMode::kCurrent;
				const bool inactive = chance.mode == RunewordEffectCardPolicy::ChanceMode::kInactive;
				{
					const auto baseText = formatNumberCompact(std::clamp(chance.basePct, 0.0f, 100.0f));
					const auto effectiveText = formatNumberCompact(chance.effectivePct);
					const std::string chanceEn = inactive ?
						("Currently inactive (base proc chance " + baseText + "%)") :
						(useCurrentChance ?
							("Current effective proc chance " + effectiveText + "% (base " + baseText + "%)") :
							("If equipped alone: effective proc chance " + effectiveText + "% (base " + baseText + "%)"));
					const std::string chanceKo = inactive ?
						("현재 비활성 (기본 발동률 " + baseText + "%)") :
						(useCurrentChance ?
							("현재 유효 발동률 " + effectiveText + "% (기본 " + baseText + "%)") :
							("단독 장착 시 예상 발동률 " + effectiveText + "% (기본 " + baseText + "%)"));

					if (!detail.empty()) {
						detail.push_back('\n');
					}
					switch (a_uiLanguageMode) {
					case 0:
						detail.append(chanceEn);
						break;
					case 1:
						detail.append(chanceKo);
						break;
					case 2:
					default:
						detail.append(chanceKo + " / " + chanceEn);
						break;
					}
				}
			}

			return detail;
		};

		auto resolveDisplayName = [&](const AffixRuntime& a_affix) -> std::string {
			// English halves go through the FE-ASR presentation boundary (Runeword
			// result affixes); every other affix resolves to its upstream name.
			switch (a_uiLanguageMode) {
			case 0:
				return ResolveAffixDisplayNameEn(a_affix);
			case 1:
				return a_affix.displayNameKo;
			case 2:
			default:
				{
					const auto nameEn = ResolveAffixDisplayNameEn(a_affix);
					if (a_affix.displayNameKo == nameEn || nameEn.empty()) {
						return a_affix.displayNameKo;
					}
					if (a_affix.displayNameKo.empty()) {
						return nameEn;
					}
					return a_affix.displayNameKo + " / " + nameEn;
				}
			}
		};

		auto formatTooltip = [&](const TooltipCandidate& a_candidate) -> std::string {
			if (a_candidate.slots.count == 0) {
				if (a_candidate.preview) {
					return "Preview: No affix";
				}
				return {};
			}

			std::string tooltip;
			if (a_candidate.preview) {
				tooltip.append("Preview\n");
			}

			for (std::uint8_t s = 0; s < a_candidate.slots.count; ++s) {
				const auto token = a_candidate.slots.tokens[s];
				const auto idxIt = _affixRuntimeState.affixRegistry.affixIndexByToken.find(token);
				if (idxIt == _affixRuntimeState.affixRegistry.affixIndexByToken.end() || idxIt->second >= _affixRuntimeState.affixes.size()) {
					continue;
				}

				const auto& affix = _affixRuntimeState.affixes[idxIt->second];
				if (a_uiLanguageMode == 0) {
					if (const auto* recipe = FindRunewordRecipeByResultAffixToken(affix.token)) {
						(void)recipe;
						// R8.7.4.21 I3: RunewordEffectCard v1 from the resolved runtime data (no record names, no
						// text-parsed upstream stats), plus this instance's effective proc chance (DD-B5-3).
						auto cardInput = BuildRunewordEffectCardInput(affix, *recipe);
						cardInput.chance = ResolveInstanceProcChanceContext(
							affix, idxIt->second, a_candidate.instanceKey, a_candidate.slots, a_candidate.worn);
						auto compactRuneword = RunewordEffectCardPolicy::TooltipText(RunewordEffectCardPolicy::Build(cardInput));
						if (!compactRuneword.empty()) {
							if (!tooltip.empty() && tooltip.back() != '\n') {
								tooltip.push_back('\n');
							}
							tooltip.append(compactRuneword);
							continue;
						}
					}
				}

				auto resolvedName = resolveDisplayName(affix);
				if (resolvedName.empty()) {
					continue;
				}

				if (!tooltip.empty() && tooltip.back() != '\n') {
					tooltip.push_back('\n');
				}

				if (a_candidate.slots.count > 1) {
					tooltip.push_back('[');
					if (affix.slot == AffixSlot::kPrefix) {
						tooltip.push_back('P');
					} else if (affix.slot == AffixSlot::kSuffix) {
						tooltip.push_back('S');
					}
					tooltip.append(std::to_string(s + 1));
					tooltip.push_back('/');
					tooltip.append(std::to_string(a_candidate.slots.count));
					tooltip.append("] ");
				}

				tooltip.append(resolvedName);

				const auto detail = formatAffixDetail(affix, idxIt->second, a_candidate);
				if (!detail.empty()) {
					tooltip.push_back('\n');
					tooltip.append(detail);
				}
			}

			return tooltip;
		};

		std::vector<TooltipResolutionCandidate> resolutionCandidates;
		resolutionCandidates.reserve(candidates.size());
		for (const auto& candidate : candidates) {
			resolutionCandidates.push_back(TooltipResolutionCandidate{
				.rowName = candidate.rowName,
				.affixToken = candidate.primaryToken,
				.instanceKey = candidate.instanceKey
			});
		}

		if (a_preferredInstanceKey != 0u) {
			const auto preferredIt = std::find_if(
				candidates.begin(),
				candidates.end(),
				[a_preferredInstanceKey](const TooltipCandidate& a_candidate) {
					return a_candidate.instanceKey == a_preferredInstanceKey;
				});

			if (preferredIt == candidates.end()) {
				return std::nullopt;
			}

			const auto preferredTooltip = formatTooltip(*preferredIt);
			if (preferredTooltip.empty()) {
				return std::nullopt;
			}
			return preferredTooltip;
		}

		const auto resolution = ResolveTooltipCandidateSelection(resolutionCandidates, a_selectedDisplayName);
		if (resolution.matchedIndex &&
			*resolution.matchedIndex < candidates.size() &&
			!resolution.ambiguous) {
			const auto& matched = candidates[*resolution.matchedIndex];
			const auto formatted = formatTooltip(matched);
			if (formatted.empty()) {
				return std::nullopt;
			}
			return formatted;
		}

		if (!a_selectedDisplayName.empty()) {
			return std::nullopt;
		}

		const auto fallback = formatTooltip(candidates.front());
		if (fallback.empty()) {
			return std::nullopt;
		}
		return fallback;
	}

	RunewordEffectCardPolicy::ChanceContext EventBridge::ResolveInstanceProcChanceContext(
		const AffixRuntime& a_affix,
		std::size_t a_affixIndex,
		std::uint64_t a_instanceKey,
		const InstanceAffixSlots& a_slots,
		bool a_worn) const
	{
		RunewordEffectCardPolicy::ChanceContext out{};
		const bool usesStandardTriggerProcLane =
			a_affix.action.type == ActionType::kCastSpell ||
			a_affix.action.type == ActionType::kCastSpellAdaptiveElement ||
			a_affix.action.type == ActionType::kSpawnTrap;
		const bool usesSpecialProcLane =
			a_affix.action.type == ActionType::kCastOnCrit ||
			a_affix.action.type == ActionType::kConvertDamage ||
			a_affix.action.type == ActionType::kMindOverMatter ||
			a_affix.action.type == ActionType::kArchmage ||
			a_affix.action.type == ActionType::kCorpseExplosion ||
			a_affix.action.type == ActionType::kSummonCorpseExplosion;
		if (!(usesStandardTriggerProcLane || usesSpecialProcLane) ||
			a_affix.procChancePct <= 0.0f ||
			a_affix.luckyHitChancePct > 0.0f ||
			a_affix.slot == AffixSlot::kSuffix) {
			return out;
		}
		bool exactInstanceCached = false;
		if (const auto equippedIt = _instanceTrackingState.equippedInstanceKeysByToken.find(a_affix.token);
			equippedIt != _instanceTrackingState.equippedInstanceKeysByToken.end()) {
			exactInstanceCached = std::find(
				equippedIt->second.begin(),
				equippedIt->second.end(),
				a_instanceKey) != equippedIt->second.end();
		}

		const bool affixActive =
			a_affixIndex < _affixRuntimeState.activeCounts.size() &&
			_affixRuntimeState.activeCounts[a_affixIndex] > 0u;
		const auto displayMode = ResolveProcChanceDisplayMode(
			_runtimeSettings.enabled.load(std::memory_order_relaxed),
			a_worn,
			_instanceTrackingState.equippedTokenCacheReady,
			exactInstanceCached,
			affixActive);

		const bool useCurrentChance = displayMode == ProcChanceDisplayMode::kCurrent;
		const float currentTriggerChance = useCurrentChance && usesStandardTriggerProcLane ?
			ResolveTriggerProcChancePct(a_affix, a_affixIndex) :
			0.0f;
		const float effectiveChance = ResolveDisplayedProcChancePct(
			displayMode,
			usesStandardTriggerProcLane,
			a_affix.procChancePct,
			_runtimeSettings.procChanceMult,
			ResolveMultiAffixProcPenalty(CountProcPenaltySlots(a_slots)),
			currentTriggerChance);

		const bool inactive = displayMode == ProcChanceDisplayMode::kInactive;
		if (!inactive && !ShouldShowAdjustedProcChance(a_affix.procChancePct, effectiveChance)) {
			return out;
		}
		out.mode = inactive ? RunewordEffectCardPolicy::ChanceMode::kInactive :
		           (useCurrentChance ? RunewordEffectCardPolicy::ChanceMode::kCurrent :
		                               RunewordEffectCardPolicy::ChanceMode::kIfEquippedAlone);
		out.basePct = a_affix.procChancePct;
		out.effectivePct = effectiveChance;
		return out;
	}

	std::optional<RunewordEffectCardPolicy::Card> EventBridge::GetInstanceRunewordEffectCard(
		std::uint64_t a_instanceKey,
		bool a_worn)
	{
		const std::scoped_lock lock(_stateMutex);
		if (!_configLoaded || a_instanceKey == 0u) {
			return std::nullopt;
		}
		// The only per-instance input is the IAXF slot list (co-save record, moved by the drop/pickup key transfer);
		// recipe/affix runtime and SpellItem records are config, rebuilt at load. Nothing of the card is persisted.
		const auto slotsIt = _instanceTrackingState.instanceAffixes.find(a_instanceKey);
		if (slotsIt == _instanceTrackingState.instanceAffixes.end()) {
			return std::nullopt;
		}
		const auto& slots = slotsIt->second;
		for (std::uint8_t s = 0; s < slots.count; ++s) {
			const auto token = slots.tokens[s];
			const auto* recipe = FindRunewordRecipeByResultAffixToken(token);
			if (!recipe) {
				continue;
			}
			const auto idxIt = _affixRuntimeState.affixRegistry.affixIndexByToken.find(token);
			if (idxIt == _affixRuntimeState.affixRegistry.affixIndexByToken.end() || idxIt->second >= _affixRuntimeState.affixes.size()) {
				continue;
			}
			const auto& affix = _affixRuntimeState.affixes[idxIt->second];
			bool worn = a_worn;
			if (const auto equippedIt = _instanceTrackingState.equippedInstanceKeysByToken.find(token);
				equippedIt != _instanceTrackingState.equippedInstanceKeysByToken.end()) {
				worn = worn || std::find(equippedIt->second.begin(), equippedIt->second.end(), a_instanceKey) != equippedIt->second.end();
			}
			auto input = BuildRunewordEffectCardInput(affix, *recipe);
			input.chance = ResolveInstanceProcChanceContext(affix, idxIt->second, a_instanceKey, slots, worn);
			auto card = RunewordEffectCardPolicy::Build(input);
			if (card.known) {
				return card;
			}
		}
		return std::nullopt;
	}
}
