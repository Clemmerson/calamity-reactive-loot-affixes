#include "CalamityAffixes/EventBridge.h"
#include "CalamityAffixes/RunewordUiPresentation.h"
#include "EventBridge.Loot.Runeword.Detail.h"
#include "EventBridge.Loot.Runeword.SummaryText.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace CalamityAffixes
{
	using namespace RunewordDetail;

	namespace
	{
		struct RecipeKeyOverride
		{
			std::string_view id;
			std::string_view key;
		};

		template <std::size_t N>
		[[nodiscard]] constexpr std::string_view FindKeyOverride(
			std::string_view a_id,
			const RecipeKeyOverride (&a_overrides)[N]) noexcept
		{
			for (const auto& entry : a_overrides) {
				if (entry.id == a_id) {
					return entry.key;
				}
			}
			return {};
		}

		template <std::size_t N>
		[[nodiscard]] constexpr bool HasDuplicateOverrideIds(const RecipeKeyOverride (&a_overrides)[N]) noexcept
		{
			for (std::size_t i = 0; i < N; ++i) {
				for (std::size_t j = i + 1; j < N; ++j) {
					if (a_overrides[i].id == a_overrides[j].id) {
						return true;
					}
				}
			}
			return false;
		}

		[[nodiscard]] constexpr std::uint32_t RecipeBucket(std::string_view a_id, std::uint32_t a_modulo) noexcept
		{
			if (a_modulo == 0u) {
				return 0u;
			}
			std::uint32_t hash = 2166136261u;
			for (const auto c : a_id) {
				hash ^= static_cast<std::uint8_t>(c);
				hash *= 16777619u;
			}
			return hash % a_modulo;
		}

		static constexpr RecipeKeyOverride kRecommendedBaseOverrides[] = {
			{ "rw_strength", "two_handed_melee" },
			{ "rw_insight", "polearm" },
			{ "rw_infinity", "polearm" },
			{ "rw_pride", "polearm" },
			{ "rw_obedience", "polearm" },
			{ "rw_faith", "bow" },
			{ "rw_ice", "bow" },
			{ "rw_mist", "bow" },
			{ "rw_harmony", "bow" },
			{ "rw_edge", "bow" },
			{ "rw_zephyr", "bow" },
			{ "rw_melody", "bow" },
			{ "rw_wrath", "bow" },
			{ "rw_brand", "bow" },
			{ "rw_leaf", "staff_wand" },
			{ "rw_memory", "staff_wand" },
			{ "rw_white", "staff_wand" },
			{ "rw_obsession", "staff_wand" },
			{ "rw_chaos", "claw" },
			{ "rw_mosaic", "claw" },
			{ "rw_grief", "sword" },
			{ "rw_lawbringer", "sword" },
			{ "rw_oath", "one_handed_melee" },
			{ "rw_unbending_will", "shield" },
			{ "rw_kingslayer", "sword" },
			{ "rw_exile", "shield" },
			{ "rw_sanctuary", "shield" },
			{ "rw_ancients_pledge", "shield" },
			{ "rw_rhyme", "shield" },
			{ "rw_splendor", "shield" },
			{ "rw_lore", "helm" },
			{ "rw_nadir", "helm" },
			{ "rw_delirium", "helm" },
			{ "rw_bulwark", "helm" },
			{ "rw_cure", "helm" },
			{ "rw_ground", "helm" },
			{ "rw_hearth", "helm" },
			{ "rw_temper", "helm" },
			{ "rw_flickering_flame", "helm" },
			{ "rw_wisdom", "helm" },
			{ "rw_metamorphosis", "helm" },
			{ "rw_enigma", "armor" },
			{ "rw_chains_of_honor", "armor" },
			{ "rw_stealth", "armor" },
			{ "rw_smoke", "armor" },
			{ "rw_treachery", "armor" },
			{ "rw_duress", "armor" },
			{ "rw_gloom", "armor" },
			{ "rw_stone", "armor" },
			{ "rw_bramble", "armor" },
			{ "rw_myth", "heavy_armor" },
			{ "rw_peace", "armor" },
			{ "rw_prudence", "armor" },
			{ "rw_rain", "armor" },
			{ "rw_spirit", "sword_shield" },
			{ "rw_phoenix", "weapon_shield" },
			{ "rw_dream", "helm_shield" },
			{ "rw_dragon", "armor_shield" },
			{ "rw_shadow_boxer", "melee_weapon" },
		};

		static_assert(!HasDuplicateOverrideIds(kRecommendedBaseOverrides),
			"kRecommendedBaseOverrides must not contain duplicate recipe ids.");

		// ---- R8.7.4.21 I3: RunewordEffectCard v1 adapter (RE -> pure CardInput) -----------------------------------
		namespace EC = RunewordEffectCardPolicy;

		// RunewordEffectCardPolicy::kAvLabels keys (Mutagen/affixes.json spelling). Anything else stays unmapped and the
		// card shows a generic "Effect n" with the raw magnitude.
		[[nodiscard]] std::string_view CardActorValueName(RE::ActorValue a_av) noexcept
		{
			using AV = RE::ActorValue;
			switch (a_av) {
			case AV::kNone: return {};
			case AV::kHealth: return "Health";
			case AV::kMagicka: return "Magicka";
			case AV::kStamina: return "Stamina";
			case AV::kHealRate: return "HealRate";
			case AV::kMagickaRate: return "MagickaRate";
			case AV::KStaminaRate: return "StaminaRate";
			case AV::kHealRateMult: return "HealRateMult";
			case AV::kMagickaRateMult: return "MagickaRateMult";
			case AV::kStaminaRateMult: return "StaminaRateMult";
			case AV::kSpeedMult: return "SpeedMult";
			case AV::kAttackDamageMult: return "AttackDamageMult";
			case AV::kWeaponSpeedMult: return "WeaponSpeedMult";
			case AV::kLeftWeaponSpeedMultiply: return "LeftWeaponSpeedMult";
			case AV::kDamageResist: return "DamageResist";
			case AV::kResistFire: return "ResistFire";
			case AV::kResistFrost: return "ResistFrost";
			case AV::kResistShock: return "ResistShock";
			case AV::kResistMagic: return "ResistMagic";
			case AV::kPoisonResist: return "PoisonResist";
			case AV::kResistDisease: return "ResistDisease";
			case AV::kReflectDamage: return "ReflectDamage";
			case AV::kAbsorbChance: return "AbsorbChance";
			case AV::kCriticalChance: return "CriticalChance";
			case AV::kCarryWeight: return "CarryWeight";
			case AV::kOneHandedModifier: return "OneHandedModifier";
			case AV::kTwoHandedModifier: return "TwoHandedModifier";
			case AV::kMarksmanModifier: return "MarksmanModifier";
			case AV::kBlockModifier: return "BlockModifier";
			case AV::kSmithingModifier: return "SmithingModifier";
			case AV::kHeavyArmorModifier: return "HeavyArmorModifier";
			case AV::kLightArmorModifier: return "LightArmorModifier";
			case AV::kPickpocketModifier: return "PickpocketModifier";
			case AV::kLockpickingModifier: return "LockpickingModifier";
			case AV::kSneakingModifier: return "SneakingModifier";
			case AV::kAlchemyModifier: return "AlchemyModifier";
			case AV::kSpeechcraftModifier: return "SpeechcraftModifier";
			case AV::kAlterationModifier: return "AlterationModifier";
			case AV::kConjurationModifier: return "ConjurationModifier";
			case AV::kDestructionModifier: return "DestructionModifier";
			case AV::kIllusionModifier: return "IllusionModifier";
			case AV::kRestorationModifier: return "RestorationModifier";
			case AV::kEnchantingModifier: return "EnchantingModifier";
			case AV::kBowSpeedBonus: return "BowSpeedBonus";
			case AV::kShoutRecoveryMult: return "ShoutRecoveryMult";
			case AV::kDetectLifeRange: return "DetectLifeRange";
			case AV::kAggresion: return "Aggression";
			case AV::kConfidence: return "Confidence";
			case AV::kParalysis: return "Paralysis";
			case AV::kInvisibility: return "Invisibility";
			default: return "Unmapped";
			}
		}

		[[nodiscard]] EC::Archetype CardArchetype(RE::EffectArchetypes::ArchetypeID a_archetype) noexcept
		{
			using A = RE::EffectArchetypes::ArchetypeID;
			switch (a_archetype) {
			case A::kValueModifier: return EC::Archetype::kValueModifier;
			case A::kPeakValueModifier: return EC::Archetype::kPeakValueModifier;
			case A::kDualValueModifier: return EC::Archetype::kDualValueModifier;
			case A::kAbsorb: return EC::Archetype::kAbsorb;
			case A::kInvisibility: return EC::Archetype::kInvisibility;
			case A::kEtherealize: return EC::Archetype::kEtherealize;
			case A::kParalysis: return EC::Archetype::kParalysis;
			case A::kCalm: return EC::Archetype::kCalm;
			case A::kFrenzy: return EC::Archetype::kFrenzy;
			case A::kDemoralize: return EC::Archetype::kDemoralize;
			case A::kTurnUndead: return EC::Archetype::kTurnUndead;
			case A::kDetectLife: return EC::Archetype::kDetectLife;
			case A::kScript: return EC::Archetype::kScript;
			default: return EC::Archetype::kOther;
			}
		}

		[[nodiscard]] std::string_view CardResistName(RE::ActorValue a_av) noexcept
		{
			switch (a_av) {
			case RE::ActorValue::kResistFire: return "ResistFire";
			case RE::ActorValue::kResistFrost: return "ResistFrost";
			case RE::ActorValue::kResistShock: return "ResistShock";
			case RE::ActorValue::kResistMagic: return "ResistMagic";
			case RE::ActorValue::kPoisonResist: return "PoisonResist";
			case RE::ActorValue::kResistDisease: return "ResistDisease";
			default: return {};
			}
		}

		// The loaded record values at call time (the same SpellItem the action casts).
		[[nodiscard]] std::vector<EC::EffectInput> CardEffects(const RE::SpellItem* a_spell)
		{
			std::vector<EC::EffectInput> out;
			if (!a_spell) {
				return out;
			}
			for (const auto* effect : a_spell->effects) {
				if (!effect || !effect->baseEffect) {
					continue;
				}
				const auto* mgef = effect->baseEffect;
				EC::EffectInput e{};
				e.actorValue = std::string(CardActorValueName(mgef->data.primaryAV));
				e.resistValue = std::string(CardResistName(mgef->data.resistVariable));
				e.archetype = CardArchetype(mgef->GetArchetype());
				// The engine subtracts the magnitude of detrimental effects (the generator sets Hostile|Detrimental).
				e.hostile = mgef->IsDetrimental();
				e.recover = mgef->data.flags.all(RE::EffectSetting::EffectSettingData::Flag::kRecover);
				e.magnitude = effect->effectItem.magnitude;
				e.durationS = static_cast<float>(effect->effectItem.duration);
				e.area = static_cast<float>(effect->effectItem.area);
				const char* name = mgef->GetName();
				e.recordName = name ? name : "";
				out.push_back(std::move(e));
			}
			return out;
		}

		[[nodiscard]] float Seconds(std::chrono::milliseconds a_ms) noexcept
		{
			return static_cast<float>(a_ms.count()) / 1000.0f;
		}
	}

	RunewordEffectCardPolicy::CardInput EventBridge::BuildRunewordEffectCardInput(
		const AffixRuntime& a_affix,
		const RunewordRecipe& a_recipe) const
	{
		EC::CardInput in{};
		in.recipeId = a_recipe.id;
		in.internalRunes = a_recipe.runeIds;
		in.upstreamName = a_recipe.displayNameEn;
		switch (a_affix.trigger) {
		case Trigger::kHit: in.trigger = EC::Trigger::kHit; break;
		case Trigger::kIncomingHit: in.trigger = EC::Trigger::kIncomingHit; break;
		case Trigger::kDotApply: in.trigger = EC::Trigger::kDotApply; break;
		case Trigger::kKill: in.trigger = EC::Trigger::kKill; break;
		case Trigger::kLowHealth: in.trigger = EC::Trigger::kLowHealth; break;
		}
		in.procChancePct = a_affix.procChancePct;
		in.icdS = Seconds(a_affix.icd);
		in.perTargetIcdS = Seconds(a_affix.perTargetIcd);
		if (a_affix.trigger == Trigger::kLowHealth) {
			in.lowHealthThresholdPct = a_affix.lowHealthThresholdPct;
			in.lowHealthRearmPct = a_affix.lowHealthRearmPct;
		}
		in.requireRecentlyHitS = Seconds(a_affix.requireRecentlyHit);
		in.requireRecentlyKillS = Seconds(a_affix.requireRecentlyKill);
		in.requireNotHitRecentlyS = Seconds(a_affix.requireNotHitRecently);
		in.luckyHitChancePct = a_affix.luckyHitChancePct;

		const auto& action = a_affix.action;
		in.applyToSelf = action.applyToSelf;
		in.passiveOnly = a_affix.procChancePct <= 0.0f && action.type == ActionType::kDebugNotify && a_affix.passiveSpell != nullptr;
		switch (action.type) {
		case ActionType::kCastSpell: in.action = EC::ActionKind::kCastSpell; break;
		case ActionType::kCastSpellAdaptiveElement: in.action = EC::ActionKind::kAdaptive; break;
		case ActionType::kCastOnCrit: in.action = EC::ActionKind::kCastOnCrit; break;
		case ActionType::kConvertDamage: in.action = EC::ActionKind::kConvertDamage; break;
		case ActionType::kMindOverMatter: in.action = EC::ActionKind::kMindOverMatter; break;
		case ActionType::kArchmage: in.action = EC::ActionKind::kArchmage; break;
		case ActionType::kCorpseExplosion: in.action = EC::ActionKind::kCorpseExplosion; break;
		case ActionType::kSummonCorpseExplosion: in.action = EC::ActionKind::kSummonCorpseExplosion; break;
		case ActionType::kSpawnTrap: in.action = EC::ActionKind::kSpawnTrap; break;
		case ActionType::kEchoStrike: in.action = EC::ActionKind::kEchoStrike; break;
		case ActionType::kDebugNotify: in.action = EC::ActionKind::kOther; break;
		}
		if (action.type == ActionType::kCastSpellAdaptiveElement) {
			std::unordered_set<RE::SpellItem*> seen;
			const std::pair<EC::Element, RE::SpellItem*> spells[] = {
				{ EC::Element::kFire, action.adaptiveFire }, { EC::Element::kFrost, action.adaptiveFrost }, { EC::Element::kShock, action.adaptiveShock } };
			for (const auto& [element, spell] : spells) {
				if (spell && seen.insert(spell).second) {
					in.adaptive.push_back({ element, CardEffects(spell) });
				}
			}
			in.adaptiveStrongest = action.adaptiveMode == AdaptiveElementMode::kStrongestResist;
		} else if (action.type != ActionType::kDebugNotify) {
			in.effects = CardEffects(action.spell);
		}
		if (a_affix.passiveSpell && a_affix.passiveSpell != action.spell) {
			in.passiveEffects = CardEffects(a_affix.passiveSpell);
		}
		switch (action.magnitudeScaling.source) {
		case MagnitudeScaling::Source::kHitPhysicalDealt: in.scaling.source = EC::ScaleSource::kHitPhysical; break;
		case MagnitudeScaling::Source::kHitTotalDealt: in.scaling.source = EC::ScaleSource::kHitTotal; break;
		case MagnitudeScaling::Source::kNone: in.scaling.source = EC::ScaleSource::kNone; break;
		}
		in.scaling.mult = action.magnitudeScaling.mult;
		in.scaling.add = action.magnitudeScaling.add;
		in.scaling.min = action.magnitudeScaling.min;
		in.scaling.max = action.magnitudeScaling.max;
		in.scaling.spellBaseAsMin = action.magnitudeScaling.spellBaseAsMin;
		in.magnitudeOverride = action.magnitudeOverride;
		in.effectiveness = action.effectiveness;
		in.corpseFlatDamage = action.corpseExplosionFlatDamage;
		in.corpsePctOfMaxHealth = action.corpseExplosionPctOfCorpseMaxHealth;
		in.corpseRadius = action.corpseExplosionRadius;
		in.corpseMaxTargets = action.corpseExplosionMaxTargets;
		in.corpseMaxChainDepth = action.corpseExplosionMaxChainDepth;
		in.corpseChainFalloff = action.corpseExplosionChainFalloff;
		if (action.type == ActionType::kSpawnTrap) {
			in.trapRadius = action.trapRadius;
			in.trapTtlS = Seconds(action.trapTtl);
		}
		if (action.type == ActionType::kEchoStrike) {
			in.echoDelayS = Seconds(action.echoDelay);
			in.echoWindowS = Seconds(action.echoWindow);
			in.echoRequirePowerAttack = action.echoRequirePowerAttack;
		}
		if (action.evolutionEnabled) {
			in.evolutionThresholds = action.evolutionThresholds;
			in.evolutionMultipliers = action.evolutionMultipliers;
		}
		switch (action.element) {
		case Element::kFire: in.convertElement = EC::Element::kFire; break;
		case Element::kFrost: in.convertElement = EC::Element::kFrost; break;
		case Element::kShock: in.convertElement = EC::Element::kShock; break;
		default: in.convertElement = EC::Element::kNone; break;
		}
		if (action.type == ActionType::kConvertDamage) in.convertPct = action.convertPct;
		if (action.type == ActionType::kMindOverMatter) {
			in.mindOverMatterPct = action.mindOverMatterDamageToMagickaPct;
			in.mindOverMatterMaxPerHit = action.mindOverMatterMaxRedirectPerHit;
		}
		if (action.type == ActionType::kArchmage) {
			in.archmageDamagePctOfMaxMagicka = action.archmageDamagePctOfMaxMagicka;
			in.archmageCostPctOfMaxMagicka = action.archmageCostPctOfMaxMagicka;
		}
		return in;
	}

	std::vector<EventBridge::RunewordRecipeEntry> EventBridge::GetRunewordRecipeEntries()
	{
		const std::scoped_lock lock(_stateMutex);
		std::vector<RunewordRecipeEntry> entries;
		if (!_configLoaded) {
			return entries;
		}

		SanitizeRunewordState();
		entries.reserve(_runewordState.recipes.size());

		auto resolveRecommendedBaseKey = [&](const RunewordRecipe& a_recipe) -> std::string_view {
			const std::string_view id = a_recipe.id;
			if (const auto overrideKey = FindKeyOverride(id, kRecommendedBaseOverrides); !overrideKey.empty()) {
				return overrideKey;
			}

			if (!a_recipe.recommendedBaseType) {
				return "mixed";
			}
			return *a_recipe.recommendedBaseType == LootItemType::kArmor ? "armor" : "weapon";
		};

		auto resolveRecipeEffectSummaryKey = [&](const RunewordRecipe& a_recipe) -> std::string_view {
			RunewordSummary::BaseType baseType = RunewordSummary::BaseType::kUnknown;
			if (a_recipe.recommendedBaseType.has_value()) {
				baseType = (*a_recipe.recommendedBaseType == LootItemType::kWeapon) ?
				               RunewordSummary::BaseType::kWeapon :
				               RunewordSummary::BaseType::kArmor;
			}
			return RunewordSummary::ResolveEffectSummaryKey(a_recipe.id, baseType);
		};

		auto formatFloat1 = [](float a_value) {
			char buffer[32]{};
			std::snprintf(buffer, sizeof(buffer), "%.1f", static_cast<double>(a_value));
			return std::string(buffer);
		};

		auto formatNumberCompact = [&](float a_value) {
			std::string text = formatFloat1(a_value);
			if (text.size() >= 2 && text.compare(text.size() - 2, 2, ".0") == 0) {
				text.erase(text.size() - 2);
			}
			return text;
		};

		auto appendDelimited = [](std::string& a_out, std::string_view a_part) {
			if (a_part.empty()) {
				return;
			}
			if (!a_out.empty()) {
				a_out.append(" · ");
			}
			a_out.append(a_part);
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

		// Generated records carry developer-facing names ("Calamity: <EN> / <KO>");
		// the recipe preview is player-facing, so strip the prefix and keep only the
		// half matching the requested language.
		auto pickLocalizedRecordName = [](std::string_view a_name, bool a_korean) -> std::string {
			std::string name(a_name);
			constexpr std::string_view kCalamityPrefix = "Calamity: ";
			if (name.rfind(kCalamityPrefix.data(), 0) == 0) {
				name.erase(0, kCalamityPrefix.size());
			}
			if (const auto sep = name.find(" / "); sep != std::string::npos) {
				name = a_korean ? name.substr(sep + 3) : name.substr(0, sep);
			}
			return name;
		};

		auto buildSpellProfileText = [&](RE::SpellItem* a_spell, std::string_view a_prefix, bool a_korean) -> std::string {
			if (!a_spell) {
				return {};
			}

			// Config-sourced names carry the Korean halves that ToPluginSafeName strips
			// from the ESP; the pickLocalizedRecordName fallback covers vanilla forms.
			const auto spellName = ResolveRecordDisplayName(
				a_spell,
				pickLocalizedRecordName(spellNameOr(a_spell, "Spell"), a_korean),
				a_korean);
			std::vector<std::string> effectProfiles;
			effectProfiles.reserve(3);
			std::string firstEffectName;
			std::size_t totalEffects = 0u;

			for (const auto* effect : a_spell->effects) {
				if (!effect || !effect->baseEffect) {
					continue;
				}

				++totalEffects;
				if (effectProfiles.size() >= 3u) {
					continue;
				}

				std::string effectProfile;
				const char* effectNameRaw = effect->baseEffect->GetName();
				if (effectNameRaw && effectNameRaw[0] != '\0') {
					effectProfile.append(ResolveRecordDisplayName(
						effect->baseEffect,
						pickLocalizedRecordName(effectNameRaw, a_korean),
						a_korean));
				} else {
					effectProfile.append("Effect");
				}
				if (effectProfiles.empty()) {
					firstEffectName = effectProfile;
				}

				std::string metrics;
				if (std::abs(effect->effectItem.magnitude) >= 0.01f) {
					appendDelimited(
						metrics,
						std::string(a_korean ? "위력 " : "Magnitude ") + formatFloat1(effect->effectItem.magnitude));
				}
				if (effect->effectItem.duration > 0u) {
					appendDelimited(
						metrics,
						std::string(a_korean ? "지속 " : "Duration ") + std::to_string(effect->effectItem.duration) +
							(a_korean ? "초" : "s"));
				}
				if (effect->effectItem.area > 0u) {
					appendDelimited(
						metrics,
						std::string(a_korean ? "범위 " : "Area ") + std::to_string(effect->effectItem.area));
				}

				if (!metrics.empty()) {
					effectProfile.append(" (");
					effectProfile.append(metrics);
					effectProfile.push_back(')');
				}
				effectProfiles.push_back(std::move(effectProfile));
			}

			std::string profile;
			if (!a_prefix.empty()) {
				profile.append(a_prefix);
				profile.append(": ");
			}
			// "Nadir -> Nadir (…)" reads like a bug; skip the spell name whenever the
			// first effect already carries the same display name.
			const bool spellNameRedundant =
				!effectProfiles.empty() && firstEffectName == spellName;
			if (!spellNameRedundant) {
				profile.append(spellName);
			}
			if (!effectProfiles.empty()) {
				if (!spellNameRedundant) {
					profile.append(" -> ");
				}
				for (std::size_t i = 0; i < effectProfiles.size(); ++i) {
					if (i > 0u) {
						profile.append("; ");
					}
					profile.append(effectProfiles[i]);
				}
				if (totalEffects > effectProfiles.size()) {
					if (a_korean) {
						profile.append(" 외 ");
						profile.append(std::to_string(totalEffects - effectProfiles.size()));
						profile.append("개 효과");
					} else {
						profile.append(" plus ");
						profile.append(std::to_string(totalEffects - effectProfiles.size()));
						profile.append(" more effect(s)");
					}
				}
			}
			return profile;
		};

		auto actionSummaryTextKo = [&](const Action& a_action) -> std::string {
			switch (a_action.type) {
			case ActionType::kCastSpell: {
				const auto spellName = spellNameOr(a_action.spell, "Spell");
				return a_action.applyToSelf ?
					       ("자신에게 " + spellName + " 시전") :
					       ("적에게 " + spellName + " 시전");
			}
			case ActionType::kCastSpellAdaptiveElement: {
				std::string elements;
				if (a_action.adaptiveFire) {
					elements.append("화염");
				}
				if (a_action.adaptiveFrost) {
					appendDelimited(elements, "냉기");
				}
				if (a_action.adaptiveShock) {
					appendDelimited(elements, "번개");
				}
				if (!elements.empty()) {
					return "적에게 적응형 원소 주문 시전 (" + elements + ")";
				}
				return "적에게 적응형 원소 주문 시전";
			}
			case ActionType::kConvertDamage: {
				std::string_view element = "원소";
				switch (a_action.element) {
				case Element::kFire:
					element = "화염";
					break;
				case Element::kFrost:
					element = "냉기";
					break;
				case Element::kShock:
					element = "번개";
					break;
				case Element::kNone:
				default:
					break;
				}
				return formatNumberCompact(std::max(0.0f, a_action.convertPct)) + "%를 " + std::string(element) + " 피해로 전환";
			}
			case ActionType::kMindOverMatter:
				return "피해 일부를 마나로 전환";
			case ActionType::kArchmage:
				return "아크메이지 추가 피해 발동";
			case ActionType::kSpawnTrap: {
				const auto spellName = spellNameOr(a_action.spell, "Trap");
				return spellName + " 함정 설치";
			}
			case ActionType::kCorpseExplosion:
				return "시체 폭발";
			case ActionType::kSummonCorpseExplosion:
				return "소환수 시체 폭발";
			case ActionType::kCastOnCrit: {
				const auto spellName = spellNameOr(a_action.spell, "Spell");
				return "치명타 시 " + spellName + " 시전";
			}
			case ActionType::kEchoStrike:
				return "그림자 권투: 근접 적중마다 그림자가 한 번 더 타격";
			case ActionType::kDebugNotify:
				return "디버그 알림";
			default:
				break;
			}

			if (!a_action.text.empty()) {
				return a_action.text;
			}
			return "효과 발동";
		};

		auto actionSummaryTextEn = [&](const Action& a_action) -> std::string {
			switch (a_action.type) {
			case ActionType::kCastSpell: {
				const auto spellName = spellNameOr(a_action.spell, "Spell");
				return a_action.applyToSelf ?
				       ("self-cast " + spellName) :
				       ("cast " + spellName);
			}
			case ActionType::kCastSpellAdaptiveElement: {
				std::string elements;
				if (a_action.adaptiveFire) {
					elements.append("Fire");
				}
				if (a_action.adaptiveFrost) {
					appendDelimited(elements, "Frost");
				}
				if (a_action.adaptiveShock) {
					appendDelimited(elements, "Shock");
				}
				return elements.empty() ?
				       "cast an adaptive elemental spell" :
				       ("cast an adaptive elemental spell (" + elements + ")");
			}
			case ActionType::kConvertDamage: {
				std::string_view element = "elemental";
				switch (a_action.element) {
				case Element::kFire:
					element = "Fire";
					break;
				case Element::kFrost:
					element = "Frost";
					break;
				case Element::kShock:
					element = "Shock";
					break;
				case Element::kNone:
				default:
					break;
				}
				return "convert " + formatNumberCompact(std::max(0.0f, a_action.convertPct)) + "% to " +
				       std::string(element) + " damage";
			}
			case ActionType::kMindOverMatter:
				return "redirect part of incoming damage to Magicka";
			case ActionType::kArchmage:
				return "trigger Archmage bonus damage";
			case ActionType::kSpawnTrap:
				return "deploy " + spellNameOr(a_action.spell, "Trap");
			case ActionType::kCorpseExplosion:
				return "detonate a corpse";
			case ActionType::kSummonCorpseExplosion:
				return "detonate a summoned corpse";
			case ActionType::kCastOnCrit:
				return "cast " + spellNameOr(a_action.spell, "Spell") + " on critical hit";
			case ActionType::kEchoStrike:
				return "enter Shadow Boxing: your shadow repeats each melee hit";
			case ActionType::kDebugNotify:
				return "show a debug notification";
			default:
				return "trigger an effect";
			}
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
				std::unordered_set<RE::SpellItem*> seenSpells;
				if (a_action.adaptiveFire && seenSpells.insert(a_action.adaptiveFire).second) {
					maxDuration = std::max(maxDuration, maxSpellDurationSeconds(a_action.adaptiveFire));
				}
				if (a_action.adaptiveFrost && seenSpells.insert(a_action.adaptiveFrost).second) {
					maxDuration = std::max(maxDuration, maxSpellDurationSeconds(a_action.adaptiveFrost));
				}
				if (a_action.adaptiveShock && seenSpells.insert(a_action.adaptiveShock).second) {
					maxDuration = std::max(maxDuration, maxSpellDurationSeconds(a_action.adaptiveShock));
				}
				return maxDuration;
			}
			case ActionType::kSpawnTrap:
				if (a_action.trapTtl > std::chrono::milliseconds::zero()) {
					return static_cast<float>(a_action.trapTtl.count()) / 1000.0f;
				}
				return maxSpellDurationSeconds(a_action.spell);
			case ActionType::kEchoStrike:
				return static_cast<float>(a_action.echoWindow.count()) / 1000.0f;
			default:
				return 0.0f;
			}
		};

		auto magnitudeScalingSourceTextEn = [](MagnitudeScaling::Source a_source) -> std::string_view {
			switch (a_source) {
			case MagnitudeScaling::Source::kHitPhysicalDealt:
				return "Physical damage dealt (HitPhysicalDealt)";
			case MagnitudeScaling::Source::kHitTotalDealt:
				return "Total damage dealt (HitTotalDealt)";
			case MagnitudeScaling::Source::kNone:
			default:
				return "";
			}
		};

		auto magnitudeScalingSourceTextKo = [](MagnitudeScaling::Source a_source) -> std::string_view {
			switch (a_source) {
			case MagnitudeScaling::Source::kHitPhysicalDealt:
				return "가한 물리 피해 (HitPhysicalDealt)";
			case MagnitudeScaling::Source::kHitTotalDealt:
				return "가한 총 피해 (HitTotalDealt)";
			case MagnitudeScaling::Source::kNone:
			default:
				return "";
			}
		};

		auto isPassiveOnlySentinel = [](const AffixRuntime& a_affix) noexcept {
			return a_affix.procChancePct <= 0.0f &&
			       a_affix.action.type == ActionType::kDebugNotify &&
			       a_affix.passiveSpell != nullptr;
		};
		auto passiveEffectName = [&](const AffixRuntime& a_affix, bool a_korean) {
			const auto localizedName = a_korean ? a_affix.displayNameKo : ResolveAffixDisplayNameEn(a_affix);
			if (!localizedName.empty()) {
				return localizedName;
			}
			if (!a_affix.displayName.empty()) {
				return a_affix.displayName;
			}
			return spellNameOr(a_affix.passiveSpell, a_korean ? "상시 효과" : "passive effect");
		};

		auto buildEffectSummaryTextKo = [&](const AffixRuntime& a_affix, std::string_view a_summaryKey) {
			if (isPassiveOnlySentinel(a_affix)) {
				const auto mappedActionText = RunewordSummary::ActionSummaryTextByKey(a_summaryKey);
				return mappedActionText.empty() ? passiveEffectName(a_affix, true) : std::string(mappedActionText);
			}

			std::string summary;
			summary.append(a_affix.action.type == ActionType::kEchoStrike && a_affix.action.echoRequirePowerAttack ?
					std::string_view{ "강공격 적중 시" } :
					triggerTextKo(a_affix.trigger));
			summary.push_back(' ');
			const auto chancePct = std::clamp(a_affix.procChancePct, 0.0f, 100.0f);
			if (chancePct > 0.0f) {
				summary.append(formatNumberCompact(chancePct));
				summary.append("% 확률로 ");
			} else {
				summary.append("항상 ");
			}
			const auto mappedActionText = RunewordSummary::ActionSummaryTextByKey(a_summaryKey);
			if (!mappedActionText.empty()) {
				summary.append(mappedActionText);
			} else {
				summary.append(actionSummaryTextKo(a_affix.action));
			}

			const float icdSeconds = static_cast<float>(a_affix.icd.count()) / 1000.0f;
			if (icdSeconds > 0.0f) {
				summary.append(" (쿨타임 ");
				summary.append(formatNumberCompact(icdSeconds));
				summary.append("초)");
			}

			const float perTargetIcdSeconds = static_cast<float>(a_affix.perTargetIcd.count()) / 1000.0f;
			if (perTargetIcdSeconds > 0.0f) {
				summary.append(" (대상별 쿨타임 ");
				summary.append(formatNumberCompact(perTargetIcdSeconds));
				summary.append("초)");
			}

			const float durationSeconds = actionDurationSeconds(a_affix.action);
			if (durationSeconds > 0.0f) {
				summary.append(" (지속 ");
				summary.append(formatNumberCompact(durationSeconds));
				summary.append("초)");
			}
			return summary;
		};

		auto buildEffectSummaryTextEn = [&](const AffixRuntime& a_affix, std::string_view a_summaryKey) {
			if (isPassiveOnlySentinel(a_affix)) {
				const auto mappedActionText = RunewordSummary::ActionSummaryTextEnByKey(a_summaryKey);
				return mappedActionText.empty() ? passiveEffectName(a_affix, false) : std::string(mappedActionText);
			}

			std::string summary;
			summary.append(a_affix.action.type == ActionType::kEchoStrike && a_affix.action.echoRequirePowerAttack ?
					std::string_view{ "On power attack hit" } :
					triggerTextEn(a_affix.trigger));
			summary.push_back(' ');
			const auto chancePct = std::clamp(a_affix.procChancePct, 0.0f, 100.0f);
			if (chancePct > 0.0f) {
				summary.append(formatNumberCompact(chancePct));
				summary.append("% chance to ");
			} else {
				summary.append("always ");
			}
			const auto mappedActionText = RunewordSummary::ActionSummaryTextEnByKey(a_summaryKey);
			if (!mappedActionText.empty()) {
				summary.append(mappedActionText);
			} else {
				summary.append(actionSummaryTextEn(a_affix.action));
			}

			const float icdSeconds = static_cast<float>(a_affix.icd.count()) / 1000.0f;
			if (icdSeconds > 0.0f) {
				summary.append(" (Cooldown ");
				summary.append(formatNumberCompact(icdSeconds));
				summary.append("s)");
			}

			const float perTargetIcdSeconds = static_cast<float>(a_affix.perTargetIcd.count()) / 1000.0f;
			if (perTargetIcdSeconds > 0.0f) {
				summary.append(" (Per-target cooldown ");
				summary.append(formatNumberCompact(perTargetIcdSeconds));
				summary.append("s)");
			}

			const float durationSeconds = actionDurationSeconds(a_affix.action);
			if (durationSeconds > 0.0f) {
				summary.append(" (Duration ");
				summary.append(formatNumberCompact(durationSeconds));
				summary.append("s)");
			}
			return summary;
		};

		// FE-ASR recipes: the same trigger/chance/cooldown sentence, built around the
		// manifest's qualitative effect instead of upstream action or spell names.
		auto buildPublicEffectSummaryTextEn = [&](const AffixRuntime& a_affix, std::string_view a_publicEffect) {
			if (isPassiveOnlySentinel(a_affix)) {
				return "Always active: " + std::string(a_publicEffect);
			}

			std::string summary;
			summary.append(a_affix.action.type == ActionType::kEchoStrike && a_affix.action.echoRequirePowerAttack ?
					std::string_view{ "On power attack hit" } :
					triggerTextEn(a_affix.trigger));
			const auto chancePct = std::clamp(a_affix.procChancePct, 0.0f, 100.0f);
			if (chancePct > 0.0f) {
				summary.append(", ");
				summary.append(formatNumberCompact(chancePct));
				summary.append("% chance: ");
			} else {
				summary.append(", always: ");
			}
			summary.append(a_publicEffect);

			const float icdSeconds = static_cast<float>(a_affix.icd.count()) / 1000.0f;
			if (icdSeconds > 0.0f) {
				summary.append(" (Cooldown ");
				summary.append(formatNumberCompact(icdSeconds));
				summary.append("s)");
			}

			const float perTargetIcdSeconds = static_cast<float>(a_affix.perTargetIcd.count()) / 1000.0f;
			if (perTargetIcdSeconds > 0.0f) {
				summary.append(" (Per-target cooldown ");
				summary.append(formatNumberCompact(perTargetIcdSeconds));
				summary.append("s)");
			}

			const float durationSeconds = actionDurationSeconds(a_affix.action);
			if (durationSeconds > 0.0f) {
				summary.append(" (Duration ");
				summary.append(formatNumberCompact(durationSeconds));
				summary.append("s)");
			}
			return summary;
		};

		auto buildEffectDetailTextKo = [&](const AffixRuntime& a_affix) {
			std::string detail;

			auto appendSpellProfile = [&](std::string_view a_prefix, RE::SpellItem* a_spell) {
				const auto spellProfile = buildSpellProfileText(a_spell, a_prefix, true);
				if (!spellProfile.empty()) {
					appendDelimited(detail, spellProfile);
				}
			};

			switch (a_affix.action.type) {
			case ActionType::kCastSpell:
				appendSpellProfile(
					a_affix.action.applyToSelf ? "버프 효과 (Self Buff)" : "공격/디버프 효과 (Hit Effect)",
					a_affix.action.spell);
				break;
			case ActionType::kCastSpellAdaptiveElement: {
				std::unordered_set<RE::SpellItem*> seenSpells;
				if (a_affix.action.adaptiveFire && seenSpells.insert(a_affix.action.adaptiveFire).second) {
					appendSpellProfile("화염 효과 (Fire)", a_affix.action.adaptiveFire);
				}
				if (a_affix.action.adaptiveFrost && seenSpells.insert(a_affix.action.adaptiveFrost).second) {
					appendSpellProfile("냉기 효과 (Frost)", a_affix.action.adaptiveFrost);
				}
				if (a_affix.action.adaptiveShock && seenSpells.insert(a_affix.action.adaptiveShock).second) {
					appendSpellProfile("번개 효과 (Shock)", a_affix.action.adaptiveShock);
				}
				break;
			}
			case ActionType::kCastOnCrit:
				appendSpellProfile("치명타 발동 효과 (Critical Hit)", a_affix.action.spell);
				break;
			case ActionType::kConvertDamage:
				appendSpellProfile("피해 전환 효과 (Conversion)", a_affix.action.spell);
				break;
			case ActionType::kArchmage:
				appendSpellProfile("아크메이지 효과 (Archmage)", a_affix.action.spell);
				break;
			case ActionType::kCorpseExplosion:
			case ActionType::kSummonCorpseExplosion:
				appendSpellProfile("시체 폭발 효과 (Corpse Explosion)", a_affix.action.spell);
				break;
			case ActionType::kSpawnTrap:
				appendSpellProfile("함정 발동 효과 (Trap Trigger)", a_affix.action.spell);
				break;
			case ActionType::kEchoStrike:
				appendSpellProfile("그림자 태세 (Stance)", a_affix.action.echoStanceSpell);
				appendDelimited(
					detail,
					"그림자 타격 (Echo): " + formatFloat1(static_cast<float>(a_affix.action.echoDelay.count()) / 1000.0f) +
						"초 뒤 재타격");
				break;
			default:
				break;
			}
			if (a_affix.passiveSpell && a_affix.passiveSpell != a_affix.action.spell) {
				appendSpellProfile("상시 효과 (Passive)", a_affix.passiveSpell);
			}

			const float icdSeconds = static_cast<float>(a_affix.icd.count()) / 1000.0f;
			if (icdSeconds > 0.0f) {
				appendDelimited(detail, "내부 쿨다운 (ICD): " + formatFloat1(icdSeconds) + "초");
			}

			const float perTargetIcdSeconds = static_cast<float>(a_affix.perTargetIcd.count()) / 1000.0f;
			if (perTargetIcdSeconds > 0.0f) {
				appendDelimited(detail, "대상별 쿨다운 (Per-target): " + formatFloat1(perTargetIcdSeconds) + "초");
			}

			if (a_affix.luckyHitChancePct > 0.0f) {
				appendDelimited(detail, "행운 적중 (Lucky Hit): " + formatFloat1(a_affix.luckyHitChancePct) + "%");
			}

			if (std::abs(a_affix.action.effectiveness - 1.0f) >= 0.01f) {
				appendDelimited(detail, "효과 배율 (Effectiveness): x" + formatFloat1(a_affix.action.effectiveness));
			}

			if (std::abs(a_affix.action.magnitudeOverride) >= 0.01f) {
				appendDelimited(detail, "고정 위력 (Magnitude): " + formatFloat1(a_affix.action.magnitudeOverride));
			}

			if (a_affix.action.type == ActionType::kMindOverMatter &&
				a_affix.action.mindOverMatterDamageToMagickaPct > 0.0f) {
				std::string moMText = "피해-마나 전환 (Damage->Magicka): " +
				                      formatFloat1(a_affix.action.mindOverMatterDamageToMagickaPct) + "%";
				if (a_affix.action.mindOverMatterMaxRedirectPerHit > 0.0f) {
					moMText.append(", 최대 (max) ");
					moMText.append(formatFloat1(a_affix.action.mindOverMatterMaxRedirectPerHit));
				}
				appendDelimited(detail, moMText);
			}

			if (a_affix.action.type == ActionType::kArchmage) {
				if (a_affix.action.archmageDamagePctOfMaxMagicka > 0.0f) {
					appendDelimited(
						detail,
						"추가 피해 (Damage): + " + formatFloat1(a_affix.action.archmageDamagePctOfMaxMagicka) + "% 최대 마력 (Max Magicka)");
				}
				if (a_affix.action.archmageCostPctOfMaxMagicka > 0.0f) {
					appendDelimited(
						detail,
						"추가 소모 (Cost): + " + formatFloat1(a_affix.action.archmageCostPctOfMaxMagicka) + "% 최대 마력 (Max Magicka)");
				}
			}

			if (a_affix.action.type == ActionType::kCorpseExplosion ||
				a_affix.action.type == ActionType::kSummonCorpseExplosion) {
				if (a_affix.action.corpseExplosionFlatDamage > 0.0f) {
					appendDelimited(detail, "고정 피해 (Flat): " + formatFloat1(a_affix.action.corpseExplosionFlatDamage));
				}
				if (a_affix.action.corpseExplosionPctOfCorpseMaxHealth > 0.0f) {
					appendDelimited(detail, "시체 최대 체력 비례 (CorpseMaxHP): " +
					                        formatFloat1(a_affix.action.corpseExplosionPctOfCorpseMaxHealth) + "%");
				}
				if (a_affix.action.corpseExplosionRadius > 0.0f) {
					appendDelimited(detail, "범위 (Radius): " + formatFloat1(a_affix.action.corpseExplosionRadius));
				}
			}

			if (a_affix.action.type == ActionType::kSpawnTrap) {
				if (a_affix.action.trapRadius > 0.0f) {
					appendDelimited(detail, "함정 범위 (Trap Radius): " + formatFloat1(a_affix.action.trapRadius));
				}
				if (a_affix.action.trapTtl > std::chrono::milliseconds::zero()) {
					const float ttlSeconds = static_cast<float>(a_affix.action.trapTtl.count()) / 1000.0f;
					appendDelimited(detail, "함정 지속시간 (Trap TTL): " + formatFloat1(ttlSeconds) + "초");
				}
			}

			if (a_affix.action.magnitudeScaling.source != MagnitudeScaling::Source::kNone) {
				std::string scalingText = "스케일링 (Scaling): ";
				const auto sourceText = magnitudeScalingSourceTextKo(a_affix.action.magnitudeScaling.source);
				if (!sourceText.empty()) {
					scalingText.append(sourceText);
					scalingText.push_back(' ');
				}
				scalingText.append("x");
				scalingText.append(formatFloat1(a_affix.action.magnitudeScaling.mult));
				if (std::abs(a_affix.action.magnitudeScaling.add) >= 0.01f) {
					scalingText.append(" + ");
					scalingText.append(formatFloat1(a_affix.action.magnitudeScaling.add));
				}
				if (a_affix.action.magnitudeScaling.min > 0.0f) {
					scalingText.append(" (최소 min ");
					scalingText.append(formatFloat1(a_affix.action.magnitudeScaling.min));
					scalingText.push_back(')');
				}
				if (a_affix.action.magnitudeScaling.max > 0.0f) {
					scalingText.append(" (최대 max ");
					scalingText.append(formatFloat1(a_affix.action.magnitudeScaling.max));
					scalingText.push_back(')');
				}
				if (a_affix.action.magnitudeScaling.spellBaseAsMin) {
					scalingText.append(" (스펠 기본값 이상 보장 / spell-base min)");
				}
				appendDelimited(detail, scalingText);
			}

			return detail;
		};

		auto buildEffectDetailTextEn = [&](const AffixRuntime& a_affix) {
			std::string detail;

			auto appendSpellProfile = [&](std::string_view a_prefix, RE::SpellItem* a_spell) {
				const auto spellProfile = buildSpellProfileText(a_spell, a_prefix, false);
				if (!spellProfile.empty()) {
					appendDelimited(detail, spellProfile);
				}
			};

			switch (a_affix.action.type) {
			case ActionType::kCastSpell:
				appendSpellProfile(
					a_affix.action.applyToSelf ? "Self buff" : "Hit effect",
					a_affix.action.spell);
				break;
			case ActionType::kCastSpellAdaptiveElement: {
				std::unordered_set<RE::SpellItem*> seenSpells;
				if (a_affix.action.adaptiveFire && seenSpells.insert(a_affix.action.adaptiveFire).second) {
					appendSpellProfile("Fire effect", a_affix.action.adaptiveFire);
				}
				if (a_affix.action.adaptiveFrost && seenSpells.insert(a_affix.action.adaptiveFrost).second) {
					appendSpellProfile("Frost effect", a_affix.action.adaptiveFrost);
				}
				if (a_affix.action.adaptiveShock && seenSpells.insert(a_affix.action.adaptiveShock).second) {
					appendSpellProfile("Shock effect", a_affix.action.adaptiveShock);
				}
				break;
			}
			case ActionType::kCastOnCrit:
				appendSpellProfile("Critical-hit effect", a_affix.action.spell);
				break;
			case ActionType::kConvertDamage:
				appendSpellProfile("Conversion effect", a_affix.action.spell);
				break;
			case ActionType::kArchmage:
				appendSpellProfile("Archmage effect", a_affix.action.spell);
				break;
			case ActionType::kCorpseExplosion:
			case ActionType::kSummonCorpseExplosion:
				appendSpellProfile("Corpse explosion effect", a_affix.action.spell);
				break;
			case ActionType::kSpawnTrap:
				appendSpellProfile("Trap trigger", a_affix.action.spell);
				break;
			case ActionType::kEchoStrike:
				appendSpellProfile("Shadow stance", a_affix.action.echoStanceSpell);
				appendDelimited(
					detail,
					"Shadow echo: repeats the hit after " +
						formatFloat1(static_cast<float>(a_affix.action.echoDelay.count()) / 1000.0f) + "s");
				break;
			default:
				break;
			}
			if (a_affix.passiveSpell && a_affix.passiveSpell != a_affix.action.spell) {
				appendSpellProfile("Passive effect", a_affix.passiveSpell);
			}

			const float icdSeconds = static_cast<float>(a_affix.icd.count()) / 1000.0f;
			if (icdSeconds > 0.0f) {
				appendDelimited(detail, "Internal cooldown (ICD): " + formatFloat1(icdSeconds) + "s");
			}

			const float perTargetIcdSeconds = static_cast<float>(a_affix.perTargetIcd.count()) / 1000.0f;
			if (perTargetIcdSeconds > 0.0f) {
				appendDelimited(detail, "Per-target cooldown: " + formatFloat1(perTargetIcdSeconds) + "s");
			}

			if (a_affix.luckyHitChancePct > 0.0f) {
				appendDelimited(detail, "Lucky Hit: " + formatFloat1(a_affix.luckyHitChancePct) + "%");
			}

			if (std::abs(a_affix.action.effectiveness - 1.0f) >= 0.01f) {
				appendDelimited(detail, "Effectiveness: x" + formatFloat1(a_affix.action.effectiveness));
			}

			if (std::abs(a_affix.action.magnitudeOverride) >= 0.01f) {
				appendDelimited(detail, "Fixed magnitude: " + formatFloat1(a_affix.action.magnitudeOverride));
			}

			if (a_affix.action.type == ActionType::kMindOverMatter &&
				a_affix.action.mindOverMatterDamageToMagickaPct > 0.0f) {
				std::string moMText = "Damage to Magicka: " +
				                      formatFloat1(a_affix.action.mindOverMatterDamageToMagickaPct) + "%";
				if (a_affix.action.mindOverMatterMaxRedirectPerHit > 0.0f) {
					moMText.append(", max ");
					moMText.append(formatFloat1(a_affix.action.mindOverMatterMaxRedirectPerHit));
				}
				appendDelimited(detail, moMText);
			}

			if (a_affix.action.type == ActionType::kArchmage) {
				if (a_affix.action.archmageDamagePctOfMaxMagicka > 0.0f) {
					appendDelimited(
						detail,
						"Bonus damage: +" + formatFloat1(a_affix.action.archmageDamagePctOfMaxMagicka) + "% Max Magicka");
				}
				if (a_affix.action.archmageCostPctOfMaxMagicka > 0.0f) {
					appendDelimited(
						detail,
						"Additional cost: +" + formatFloat1(a_affix.action.archmageCostPctOfMaxMagicka) + "% Max Magicka");
				}
			}

			if (a_affix.action.type == ActionType::kCorpseExplosion ||
				a_affix.action.type == ActionType::kSummonCorpseExplosion) {
				if (a_affix.action.corpseExplosionFlatDamage > 0.0f) {
					appendDelimited(detail, "Flat damage: " + formatFloat1(a_affix.action.corpseExplosionFlatDamage));
				}
				if (a_affix.action.corpseExplosionPctOfCorpseMaxHealth > 0.0f) {
					appendDelimited(
						detail,
						"Corpse Max Health damage: " +
							formatFloat1(a_affix.action.corpseExplosionPctOfCorpseMaxHealth) + "%");
				}
				if (a_affix.action.corpseExplosionRadius > 0.0f) {
					appendDelimited(detail, "Radius: " + formatFloat1(a_affix.action.corpseExplosionRadius));
				}
			}

			if (a_affix.action.type == ActionType::kSpawnTrap) {
				if (a_affix.action.trapRadius > 0.0f) {
					appendDelimited(detail, "Trap radius: " + formatFloat1(a_affix.action.trapRadius));
				}
				if (a_affix.action.trapTtl > std::chrono::milliseconds::zero()) {
					const float ttlSeconds = static_cast<float>(a_affix.action.trapTtl.count()) / 1000.0f;
					appendDelimited(detail, "Trap lifetime: " + formatFloat1(ttlSeconds) + "s");
				}
			}

			if (a_affix.action.magnitudeScaling.source != MagnitudeScaling::Source::kNone) {
				std::string scalingText = "Scaling: ";
				const auto sourceText = magnitudeScalingSourceTextEn(a_affix.action.magnitudeScaling.source);
				if (!sourceText.empty()) {
					scalingText.append(sourceText);
					scalingText.push_back(' ');
				}
				scalingText.append("x");
				scalingText.append(formatFloat1(a_affix.action.magnitudeScaling.mult));
				if (std::abs(a_affix.action.magnitudeScaling.add) >= 0.01f) {
					scalingText.append(" + ");
					scalingText.append(formatFloat1(a_affix.action.magnitudeScaling.add));
				}
				if (a_affix.action.magnitudeScaling.min > 0.0f) {
					scalingText.append(" (min ");
					scalingText.append(formatFloat1(a_affix.action.magnitudeScaling.min));
					scalingText.push_back(')');
				}
				if (a_affix.action.magnitudeScaling.max > 0.0f) {
					scalingText.append(" (max ");
					scalingText.append(formatFloat1(a_affix.action.magnitudeScaling.max));
					scalingText.push_back(')');
				}
				if (a_affix.action.magnitudeScaling.spellBaseAsMin) {
					scalingText.append(" (at least the spell base value)");
				}
				appendDelimited(detail, scalingText);
			}

			return detail;
		};

		std::uint64_t selectedToken = 0u;
		if (const auto* currentRecipe = GetCurrentRunewordRecipe()) {
			selectedToken = currentRecipe->token;
		}
		if (_runewordState.selectedBaseKey) {
			const auto selectedKey = *_runewordState.selectedBaseKey;
			if (const auto* completed = ResolveCompletedRunewordRecipe(selectedKey)) {
				selectedToken = completed->token;
			}
			// Explicit recipe selection (instance state) wins over completed default,
			// so re-transmutation highlights the newly chosen recipe.
			if (const auto stateIt = _runewordState.instanceStates.find(selectedKey);
				stateIt != _runewordState.instanceStates.end() &&
				stateIt->second.recipeToken != 0u) {
				selectedToken = stateIt->second.recipeToken;
			}
		}

		for (const auto& recipe : _runewordState.recipes) {
			if (!HasRunewordRuntimeEffect(recipe)) {
				continue;
			}
			const auto affixIt = _affixRuntimeState.affixRegistry.affixIndexByToken.find(recipe.resultAffixToken);
			const auto& affix = _affixRuntimeState.affixes[affixIt->second];
			const auto effectSummaryKey = resolveRecipeEffectSummaryKey(recipe);

			std::string runes;
			for (std::size_t i = 0; i < recipe.runeIds.size(); ++i) {
				if (i > 0) {
					runes.push_back('-');
				}
				runes.append(recipe.runeIds[i]);
			}

			entries.push_back(RunewordRecipeEntry{
				.recipeId = recipe.id,
				.recipeToken = recipe.token,
				.displayName = recipe.displayName,
				.displayNameEn = recipe.displayNameEn,
				.displayNameKo = recipe.displayNameKo,
				.runeSequence = std::move(runes),
				.runeTokens = recipe.runeTokens,
				.effectSummaryKey = std::string(effectSummaryKey),
				.effectSummaryTextEn = buildEffectSummaryTextEn(affix, effectSummaryKey),
				.effectSummaryTextKo = buildEffectSummaryTextKo(affix, effectSummaryKey),
				.effectDetailTextEn = buildEffectDetailTextEn(affix),
				.effectDetailTextKo = buildEffectDetailTextKo(affix),
				.recommendedBaseKey = std::string(resolveRecommendedBaseKey(recipe)),
				.selected = (selectedToken != 0u && selectedToken == recipe.token)
			});

			// FE-ASR presentation boundary for the English display fields; recipeId,
			// recipeToken and runeTokens above stay mechanical.
			const auto publicEffect = RuneforgePresentation::EffectSummary(recipe.id);
			RunewordUiPresentation::ApplyRecipeEntryPresentation(
				entries.back(),
				recipe.runeIds,
				recipe.displayNameEn,
				publicEffect.empty() ? std::string{} : buildPublicEffectSummaryTextEn(affix, publicEffect));
			// R8.7.4.21 I3: the formula-level effect card (base proc chance) for the menu snapshot and the workstation.
			entries.back().effectCard = RunewordEffectCardPolicy::Build(BuildRunewordEffectCardInput(affix, recipe));
		}

		return entries;
	}

	bool EventBridge::IsFeasrRuneforgeDiscoveryReady() const
	{
		const std::scoped_lock lock(_stateMutex);
		return _feasrDiscoveryReady;
	}

	bool EventBridge::IsFeasrRunewordRecipeKnown(std::string_view a_recipeId) const
	{
		if (a_recipeId.empty()) {
			return false;
		}
		const std::scoped_lock lock(_stateMutex);
		return _feasrDiscoveryReady && _feasrKnownRunewordRecipeIds.contains(std::string(a_recipeId));
	}

}
