#pragma once

#include "CalamityAffixes/RuneforgePresentation.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// RunewordEffectCard v1 (FE-ASR R8.7.4.21 lane I3, design B5-panel/findings.json).
//
// One pure builder (no RE types) turns the resolved runtime data of a Runeword result affix (AffixRuntime + the loaded
// SpellItem effects, copied into CardInput by EventBridge) into a short ASCII card. The inventory tooltip, the
// Calamity 2 Runeforge menu snapshot and the physical workstation panel all show these lines, so the three surfaces
// cannot drift. Display names come only from the presentation manifest (RuneforgePresentation.generated.h); MGEF/SPEL
// record names are never shown.
//
// Interim director decisions are each kept in one place so they can change without touching the rest:
//   DD-B5-1  effect labels   -> AvLabelTable / DescribeEffect: derived plain-English label from the actor value and
//                               archetype when the table knows it, else "Effect <n>". No authored prose.
//   DD-B5-2  units           -> AvLabelTable kinds: Mult AVs that hold a fraction render x100 as "%", percent-point AVs
//                               render "N%", pools/armor/carry weight render points, skill modifiers render "+N";
//                               anything else renders "magnitude N" and marks the card inexact.
//   DD-B5-3  proc chance     -> ChanceLine: the per-instance effective chance (same numbers and wording as the
//                               existing tooltip) is appended only where the caller computed it (instance cards).
//   DD-B5-4  line budget     -> kMaxLines / kMaxLineChars / kCompactLines and PanelBody: no paging; the Details toggle
//                               switches between the compact and the full card.
namespace CalamityAffixes::RunewordEffectCardPolicy
{
	inline constexpr std::uint32_t kVersion = 1u;
	inline constexpr std::size_t kMaxLines = 8u;        // DD-B5-4
	inline constexpr std::size_t kMaxLineChars = 64u;   // DD-B5-4
	inline constexpr std::size_t kCompactLines = 4u;    // header + trigger + up to two effect lines

	enum class Trigger : std::uint8_t
	{
		kHit,
		kIncomingHit,
		kDotApply,
		kKill,
		kLowHealth,
	};

	enum class ActionKind : std::uint8_t
	{
		kOther,
		kCastSpell,
		kAdaptive,
		kCastOnCrit,
		kConvertDamage,
		kMindOverMatter,
		kArchmage,
		kCorpseExplosion,
		kSummonCorpseExplosion,
		kSpawnTrap,
		kEchoStrike,
	};

	enum class Archetype : std::uint8_t
	{
		kValueModifier,
		kPeakValueModifier,
		kDualValueModifier,
		kAbsorb,
		kInvisibility,
		kEtherealize,
		kParalysis,
		kCalm,
		kFrenzy,
		kDemoralize,
		kTurnUndead,
		kDetectLife,
		kScript,
		kOther,
	};

	enum class Role : std::uint8_t
	{
		kSelf,
		kTarget,
		kPassive,
		kEcho,
	};

	enum class ScaleSource : std::uint8_t
	{
		kNone,
		kHitPhysical,
		kHitTotal,
	};

	enum class Element : std::uint8_t
	{
		kNone,
		kFire,
		kFrost,
		kShock,
	};

	enum class ChanceMode : std::uint8_t
	{
		kNone,
		kIfEquippedAlone,
		kCurrent,
		kInactive,
	};

	struct EffectInput
	{
		std::string actorValue{};   // RE::ActorValue enum name ("DestructionModifier", "Health", ...)
		std::string resistValue{};  // resist AV name of the MGEF ("ResistFire", ...) or ""
		Archetype archetype{ Archetype::kValueModifier };
		bool hostile{ false };
		bool recover{ false };
		float magnitude{ 0.0f };
		float durationS{ 0.0f };
		float area{ 0.0f };
		// Diagnostic only: the plugin record name. Never displayed; Build() treats it as a forbidden token (G3).
		std::string recordName{};
	};

	struct Scaling
	{
		ScaleSource source{ ScaleSource::kNone };
		float mult{ 0.0f };
		float add{ 0.0f };
		float min{ 0.0f };
		float max{ 0.0f };
		bool spellBaseAsMin{ false };
	};

	struct AdaptiveSpell
	{
		Element element{ Element::kNone };
		std::vector<EffectInput> effects{};
	};

	struct ChanceContext
	{
		ChanceMode mode{ ChanceMode::kNone };
		float basePct{ 0.0f };
		float effectivePct{ 0.0f };
	};

	struct CardInput
	{
		std::string recipeId{};                  // mechanical rw_* key, never displayed
		std::vector<std::string> internalRunes{};  // mechanical rune order (recipe.runeIds)
		std::string upstreamName{};              // upstream recipe name (forbidden token, never displayed)

		Trigger trigger{ Trigger::kHit };
		float procChancePct{ 0.0f };
		float icdS{ 0.0f };
		float perTargetIcdS{ 0.0f };
		float lowHealthThresholdPct{ 0.0f };
		float lowHealthRearmPct{ 0.0f };
		float requireRecentlyHitS{ 0.0f };
		float requireRecentlyKillS{ 0.0f };
		float requireNotHitRecentlyS{ 0.0f };
		float luckyHitChancePct{ 0.0f };

		ActionKind action{ ActionKind::kCastSpell };
		bool applyToSelf{ false };
		bool passiveOnly{ false };  // proc-less passive sentinel (action DebugNotify, chance 0, passive spell)
		std::vector<EffectInput> effects{};         // the action spell (echo: the echo spell)
		std::vector<AdaptiveSpell> adaptive{};      // CastSpellAdaptiveElement, one entry per element
		bool adaptiveStrongest{ false };            // false = weakest resistance
		std::vector<EffectInput> passiveEffects{};  // passive (ability) spell while equipped

		Scaling scaling{};
		float magnitudeOverride{ 0.0f };
		float effectiveness{ 1.0f };

		float corpseFlatDamage{ 0.0f };
		float corpsePctOfMaxHealth{ 0.0f };
		float corpseRadius{ 0.0f };
		std::uint32_t corpseMaxTargets{ 0u };
		std::uint32_t corpseMaxChainDepth{ 1u };
		float corpseChainFalloff{ 1.0f };

		float trapRadius{ 0.0f };
		float trapTtlS{ 0.0f };

		float echoDelayS{ 0.0f };
		float echoWindowS{ 0.0f };
		bool echoRequirePowerAttack{ false };

		std::vector<std::uint32_t> evolutionThresholds{};
		std::vector<float> evolutionMultipliers{};

		Element convertElement{ Element::kNone };
		float convertPct{ 0.0f };
		float mindOverMatterPct{ 0.0f };
		float mindOverMatterMaxPerHit{ 0.0f };
		float archmageDamagePctOfMaxMagicka{ 0.0f };
		float archmageCostPctOfMaxMagicka{ 0.0f };

		ChanceContext chance{};  // DD-B5-3: set only for instance cards
	};

	struct Card
	{
		bool known{ false };  // false: recipe not in the manifest; callers keep their legacy text
		std::string recipeId{};
		std::string name{};
		std::string runes{};
		std::string glyphs{};
		std::string summary{};
		std::string header{};
		std::string triggerText{};
		float chancePct{ 0.0f };
		float cooldownS{ 0.0f };
		float perTargetCooldownS{ 0.0f };
		std::vector<std::string> effectLines{};   // the proc (action) effects
		std::vector<std::string> extraLines{};    // scaling range, area, explosion/echo/trap/evolution details
		std::vector<std::string> passiveLines{};  // while-equipped (ability) effects
		std::vector<std::string> conditionLines{};
		std::string chanceLine{};
		std::vector<std::string> lines{};  // budgeted: [header, trigger, effects, extras, passives, conditions, chance]
		std::size_t compactCount{ 0u };    // leading lines of the collapsed (Details off) view
		std::size_t overflow{ 0u };        // body lines replaced by the "+N more" line
		bool exact{ true };                // false when any number is not the applied value in player units
		bool guarded{ false };             // a line was replaced because it carried a forbidden token
	};

	// ------------------------------------------------------------------------------------------------ text helpers

	// Compact number: at most 2 decimals, no trailing zeros, never rounds a nonzero value to 0.
	[[nodiscard]] inline std::string Num(double a_value)
	{
		if (!std::isfinite(a_value)) {
			return "?";
		}
		const double magnitude = std::fabs(a_value);
		const int decimals = magnitude == 0.0 ? 0 : (magnitude < 0.01 ? 4 : 2);
		std::array<char, 64> buffer{};
		std::snprintf(buffer.data(), buffer.size(), "%.*f", decimals, a_value);
		std::string out(buffer.data());
		if (out.find('.') != std::string::npos) {
			while (!out.empty() && out.back() == '0') out.pop_back();
			if (!out.empty() && out.back() == '.') out.pop_back();
		}
		if (out == "-0") out = "0";
		return out;
	}

	// One decimal, trailing ".0" dropped: the existing tooltip's formatNumberCompact (proc chance lines).
	[[nodiscard]] inline std::string Num1(double a_value)
	{
		if (!std::isfinite(a_value)) {
			return "?";
		}
		std::array<char, 64> buffer{};
		std::snprintf(buffer.data(), buffer.size(), "%.1f", a_value);
		std::string out(buffer.data());
		if (out.size() >= 2u && out.compare(out.size() - 2u, 2u, ".0") == 0) out.erase(out.size() - 2u);
		return out;
	}

	[[nodiscard]] inline std::string Capitalize(std::string a_text)
	{
		if (!a_text.empty() && a_text[0] >= 'a' && a_text[0] <= 'z') {
			a_text[0] = static_cast<char>(a_text[0] - 'a' + 'A');
		}
		return a_text;
	}

	[[nodiscard]] inline bool IsAsciiLine(std::string_view a_line) noexcept
	{
		for (const char c : a_line) {
			const auto u = static_cast<unsigned char>(c);
			if (u < 0x20u || u > 0x7Eu) return false;
		}
		return true;
	}

	// Tokens that must never reach a player-facing line (B5 textSafety): developer prefixes, mechanical keys, the
	// upstream recipe name and every record name the input carried.
	[[nodiscard]] inline bool ContainsForbiddenToken(std::string_view a_line, const CardInput& a_in)
	{
		static constexpr std::array<std::string_view, 5> kFixed{ "Calamity:", "CAFF_", "rw_", "runeword_", "LoreBox_" };
		for (const auto token : kFixed) {
			if (a_line.find(token) != std::string_view::npos) return true;
		}
		auto hasWord = [&](std::string_view a_word) {
			if (a_word.size() < 3) return false;
			std::size_t at = a_line.find(a_word);
			while (at != std::string_view::npos) {
				const bool startOk = at == 0 || !std::isalnum(static_cast<unsigned char>(a_line[at - 1]));
				const auto end = at + a_word.size();
				const bool endOk = end >= a_line.size() || !std::isalnum(static_cast<unsigned char>(a_line[end]));
				if (startOk && endOk) return true;
				at = a_line.find(a_word, at + 1);
			}
			return false;
		};
		if (!a_in.upstreamName.empty()) {
			std::string phrase = "Runeword ";
			phrase.append(a_in.upstreamName);
			if (a_line.find(phrase) != std::string_view::npos) return true;
		}
		auto recordHit = [&](const std::vector<EffectInput>& a_effects) {
			for (const auto& e : a_effects) {
				if (!e.recordName.empty() && hasWord(e.recordName)) return true;
			}
			return false;
		};
		if (recordHit(a_in.effects) || recordHit(a_in.passiveEffects)) return true;
		for (const auto& spell : a_in.adaptive) {
			if (recordHit(spell.effects)) return true;
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------ DD-B5-1/2 table

	enum class UnitKind : std::uint8_t
	{
		kPool,          // Health / Magicka / Stamina: damage, restore, absorb or max (points)
		kPoints,        // raw value as "+N"
		kPercentPoints, // raw value as "+N%"
		kFractionPct,   // value x100 as "+N%" (AttackDamageMult 0.3 -> +30%)
		kSkill,         // "<Skill> skill: +N"
		kUnknownUnit,   // label known, unit not: "magnitude N" (card inexact)
	};

	struct AvLabel
	{
		std::string_view actorValue;
		std::string_view label;  // lower-case unless a proper game term
		UnitKind kind;
	};

	inline constexpr std::array<AvLabel, 48> kAvLabels{ {
		{ "Health", "Health", UnitKind::kPool },
		{ "Magicka", "Magicka", UnitKind::kPool },
		{ "Stamina", "Stamina", UnitKind::kPool },
		{ "HealRate", "Health regen rate", UnitKind::kPoints },
		{ "MagickaRate", "Magicka regen rate", UnitKind::kPoints },
		{ "StaminaRate", "Stamina regen rate", UnitKind::kPoints },
		{ "HealRateMult", "Health regen", UnitKind::kPercentPoints },
		{ "MagickaRateMult", "Magicka regen", UnitKind::kPercentPoints },
		{ "StaminaRateMult", "Stamina regen", UnitKind::kPercentPoints },
		{ "SpeedMult", "move speed", UnitKind::kPercentPoints },
		{ "AttackDamageMult", "attack damage", UnitKind::kFractionPct },
		{ "WeaponSpeedMult", "attack speed", UnitKind::kFractionPct },
		{ "LeftWeaponSpeedMult", "left-hand attack speed", UnitKind::kFractionPct },
		{ "DamageResist", "armor rating", UnitKind::kPoints },
		{ "ResistFire", "fire resistance", UnitKind::kPercentPoints },
		{ "ResistFrost", "frost resistance", UnitKind::kPercentPoints },
		{ "ResistShock", "shock resistance", UnitKind::kPercentPoints },
		{ "ResistMagic", "magic resistance", UnitKind::kPercentPoints },
		{ "PoisonResist", "poison resistance", UnitKind::kPercentPoints },
		{ "ResistDisease", "disease resistance", UnitKind::kPercentPoints },
		{ "ReflectDamage", "damage reflection", UnitKind::kPercentPoints },
		{ "AbsorbChance", "spell absorption chance", UnitKind::kPercentPoints },
		{ "CriticalChance", "critical chance", UnitKind::kPercentPoints },
		{ "CarryWeight", "carry weight", UnitKind::kPoints },
		{ "OneHandedModifier", "One-handed skill", UnitKind::kSkill },
		{ "TwoHandedModifier", "Two-handed skill", UnitKind::kSkill },
		{ "MarksmanModifier", "Archery skill", UnitKind::kSkill },
		{ "BlockModifier", "Block skill", UnitKind::kSkill },
		{ "SmithingModifier", "Smithing skill", UnitKind::kSkill },
		{ "HeavyArmorModifier", "Heavy Armor skill", UnitKind::kSkill },
		{ "LightArmorModifier", "Light Armor skill", UnitKind::kSkill },
		{ "PickpocketModifier", "Pickpocket skill", UnitKind::kSkill },
		{ "LockpickingModifier", "Lockpicking skill", UnitKind::kSkill },
		{ "SneakingModifier", "Sneak skill", UnitKind::kSkill },
		{ "AlchemyModifier", "Alchemy skill", UnitKind::kSkill },
		{ "SpeechcraftModifier", "Speech skill", UnitKind::kSkill },
		{ "AlterationModifier", "Alteration skill", UnitKind::kSkill },
		{ "ConjurationModifier", "Conjuration skill", UnitKind::kSkill },
		{ "DestructionModifier", "Destruction skill", UnitKind::kSkill },
		{ "IllusionModifier", "Illusion skill", UnitKind::kSkill },
		{ "RestorationModifier", "Restoration skill", UnitKind::kSkill },
		{ "EnchantingModifier", "Enchanting skill", UnitKind::kSkill },
		{ "BowSpeedBonus", "bow draw speed", UnitKind::kUnknownUnit },
		{ "ShoutRecoveryMult", "shout recovery", UnitKind::kUnknownUnit },
		{ "DetectLifeRange", "detect life", UnitKind::kUnknownUnit },
		{ "Aggression", "aggression", UnitKind::kUnknownUnit },
		{ "Confidence", "confidence", UnitKind::kUnknownUnit },
		{ "Paralysis", "paralysis", UnitKind::kUnknownUnit },
	} };

	[[nodiscard]] constexpr const AvLabel* FindAvLabel(std::string_view a_actorValue) noexcept
	{
		for (const auto& row : kAvLabels) {
			if (row.actorValue == a_actorValue) return &row;
		}
		return nullptr;
	}

	// Status archetypes whose magnitude is not an actor-value change.
	[[nodiscard]] constexpr std::string_view StatusLabel(Archetype a_archetype) noexcept
	{
		switch (a_archetype) {
		case Archetype::kInvisibility: return "invisibility";
		case Archetype::kEtherealize: return "ethereal form";
		case Archetype::kParalysis: return "paralysis";
		case Archetype::kCalm: return "calm";
		case Archetype::kFrenzy: return "frenzy";
		case Archetype::kDemoralize: return "fear";
		case Archetype::kTurnUndead: return "turn undead";
		default: return {};
		}
	}

	[[nodiscard]] constexpr bool IsLevelCapStatus(Archetype a_archetype) noexcept
	{
		return a_archetype == Archetype::kCalm || a_archetype == Archetype::kFrenzy ||
		       a_archetype == Archetype::kDemoralize || a_archetype == Archetype::kTurnUndead;
	}

	[[nodiscard]] constexpr std::string_view DamageElement(std::string_view a_resistValue) noexcept
	{
		if (a_resistValue == "ResistFire") return "fire ";
		if (a_resistValue == "ResistFrost") return "frost ";
		if (a_resistValue == "ResistShock") return "shock ";
		if (a_resistValue == "PoisonResist") return "poison ";
		if (a_resistValue == "ResistDisease") return "disease ";
		return {};
	}

	[[nodiscard]] constexpr std::string_view ElementName(Element a_element) noexcept
	{
		switch (a_element) {
		case Element::kFire: return "fire";
		case Element::kFrost: return "frost";
		case Element::kShock: return "shock";
		default: return {};
		}
	}

	// Magnitude actually applied to a spell's effects: the action's scaling (a share of the hit) or override, else the
	// record value. Mirrors EventBridge::ResolveSpellMagnitudeOverride / ResolveMagnitudeOverride.
	struct AppliedMagnitude
	{
		bool scaled{ false };
		std::string scaledText{};
		std::string rangeLine{};
		float fixed{ 0.0f };
		bool overridden{ false };
	};

	[[nodiscard]] inline AppliedMagnitude ResolveApplied(const CardInput& a_in, std::span<const EffectInput> a_spellEffects)
	{
		AppliedMagnitude out{};
		if (a_in.scaling.source != ScaleSource::kNone) {
			float base = 0.0f;
			for (const auto& e : a_spellEffects) base = std::max(base, e.magnitude);
			out.scaled = true;
			out.scaledText = Num(static_cast<double>(a_in.scaling.mult) * 100.0) + "% of " +
				(a_in.scaling.source == ScaleSource::kHitPhysical ? "physical hit damage" : "hit damage");
			if (std::fabs(a_in.scaling.add) >= 0.005f) out.scaledText += " + " + Num(a_in.scaling.add);
			float lo = a_in.scaling.min > 0.0f ? a_in.scaling.min : 0.0f;
			if (a_in.scaling.spellBaseAsMin) lo = std::max(lo, base);
			const float hi = a_in.scaling.max > 0.0f ? a_in.scaling.max : 0.0f;
			if (lo > 0.0f && hi > 0.0f) out.rangeLine = "Scaled value: " + Num(lo) + " to " + Num(hi);
			else if (lo > 0.0f) out.rangeLine = "Scaled value: at least " + Num(lo);
			else if (hi > 0.0f) out.rangeLine = "Scaled value: at most " + Num(hi);
			return out;
		}
		if (std::fabs(a_in.magnitudeOverride) >= 0.01f) {
			out.overridden = true;
			out.fixed = a_in.magnitudeOverride;
		}
		return out;
	}

	struct EffectText
	{
		std::string text{};
		bool exact{ true };
		bool skip{ false };
	};

	// DD-B5-1 + DD-B5-2: one effect -> "<Label>: <value>[ per second][ for Ds][, area A ft]".
	[[nodiscard]] inline EffectText DescribeEffect(
		const EffectInput& a_effect,
		Role a_role,
		std::size_t a_ordinal,
		const AppliedMagnitude& a_applied,
		bool a_suppressPassiveSuffix = false)
	{
		EffectText out{};
		const double magnitude = a_applied.overridden ? a_applied.fixed : a_effect.magnitude;
		const bool passive = a_role == Role::kPassive;
		const bool target = a_role == Role::kTarget || a_role == Role::kEcho;
		const float duration = passive ? 0.0f : a_effect.durationS;
		auto durationText = [&](bool a_perSecond) {
			std::string d;
			if (duration > 0.0f) {
				if (a_perSecond) d += " per second";
				d += " for " + Num(duration) + "s";
			}
			return d;
		};
		auto finish = [&](std::string a_label, std::string a_value, bool a_perSecond, bool a_showDuration = true) {
			std::string line = Capitalize(std::move(a_label));
			if (!a_value.empty()) line += ": " + a_value + (a_showDuration ? durationText(a_perSecond) : std::string{});
			else line += ": " + (duration > 0.0f ? Num(duration) + "s" : std::string("active"));
			if (a_effect.area > 0.0f && !passive) line += ", area " + Num(a_effect.area) + " ft";
			if (passive && !a_suppressPassiveSuffix) line += " (always)";
			return line;
		};
		auto statPrefix = [&](std::string_view a_label) {
			std::string label(a_label);
			if (target) label = "target " + label;
			return label;
		};

		// Status archetypes (no actor-value change).
		if (const auto status = StatusLabel(a_effect.archetype); !status.empty()) {
			std::string value;
			if (IsLevelCapStatus(a_effect.archetype)) value = "up to level " + Num(magnitude);
			out.text = finish(statPrefix(status), value, false);
			return out;
		}
		if (a_effect.archetype == Archetype::kScript || a_effect.actorValue.empty() || a_effect.actorValue == "None") {
			if (a_role == Role::kEcho) {
				out.skip = true;
				return out;
			}
			out.exact = false;
			std::string value = std::fabs(magnitude) >= 0.01 ? "magnitude " + Num(magnitude) : std::string{};
			out.text = finish("Effect " + std::to_string(a_ordinal), value, false);
			return out;
		}

		const auto* row = FindAvLabel(a_effect.actorValue);
		if (!row) {
			out.exact = false;
			out.text = finish("Effect " + std::to_string(a_ordinal), "magnitude " + Num(magnitude), false);
			return out;
		}
		if (a_effect.archetype == Archetype::kDetectLife) {
			out.exact = false;
			out.text = finish("detect life", "magnitude " + Num(magnitude), false);
			return out;
		}

		const char sign = a_effect.hostile ? '-' : '+';
		const std::string scaled = a_applied.scaled ? a_applied.scaledText : std::string{};
		switch (row->kind) {
		case UnitKind::kPool: {
			const std::string pool(row->label);
			const std::string amount = a_applied.scaled ? scaled : Num(magnitude);
			if (a_effect.archetype == Archetype::kAbsorb) {
				out.text = finish(std::string(a_role == Role::kEcho ? "echo " : "") + "absorb " + pool, amount, duration > 1.0f, duration > 1.0f);
			} else if (a_effect.hostile && !a_effect.recover) {
				std::string label = pool == "Health" ? std::string(DamageElement(a_effect.resistValue)) + "damage" : pool + " damage";
				if (a_role == Role::kEcho) label = "echo " + label;
				// A value modifier of at most one second deals its magnitude once; longer ones are per second.
				out.text = finish(label, amount, duration > 1.0f, duration > 1.0f);
			} else if (!a_effect.hostile && !a_effect.recover) {
				out.text = finish("restore " + pool, amount, duration > 1.0f, duration > 1.0f);
			} else {
				const std::string label = a_effect.hostile ? statPrefix(pool) : "max " + pool;
				const std::string value = a_applied.scaled ? scaled : std::string(1, sign) + Num(magnitude);
				out.text = finish(label, value, false);
			}
			if (a_applied.scaled && a_effect.archetype != Archetype::kAbsorb && a_effect.recover) out.exact = false;
			return out;
		}
		case UnitKind::kPoints:
		case UnitKind::kSkill:
			out.text = finish(statPrefix(row->label), a_applied.scaled ? scaled : std::string(1, sign) + Num(magnitude), false);
			out.exact = !a_applied.scaled;
			return out;
		case UnitKind::kPercentPoints:
			out.text = finish(statPrefix(row->label), a_applied.scaled ? scaled : std::string(1, sign) + Num(magnitude) + "%", false);
			out.exact = !a_applied.scaled;
			return out;
		case UnitKind::kFractionPct:
			out.text = finish(statPrefix(row->label),
				a_applied.scaled ? scaled : std::string(1, sign) + Num(magnitude * 100.0) + "%", false);
			out.exact = !a_applied.scaled;
			return out;
		case UnitKind::kUnknownUnit:
			out.exact = false;
			out.text = finish(statPrefix(row->label), "magnitude " + Num(magnitude), false);
			return out;
		}
		return out;
	}

	// Splits a line longer than kMaxLineChars at the last ", " that fits; the tail becomes its own line.
	[[nodiscard]] inline std::vector<std::string> FitLine(std::string a_line)
	{
		std::vector<std::string> out;
		while (a_line.size() > kMaxLineChars) {
			const auto cut = a_line.rfind(", ", kMaxLineChars - 1);
			if (cut == std::string::npos || cut == 0) {
				a_line.resize(kMaxLineChars);
				break;
			}
			out.push_back(a_line.substr(0, cut));
			a_line = Capitalize(a_line.substr(cut + 2));
		}
		out.push_back(std::move(a_line));
		return out;
	}

	// ------------------------------------------------------------------------------------------------ DD-B5-3

	// The per-instance chance line, worded exactly as the existing inventory tooltip (EN half).
	[[nodiscard]] inline std::string ChanceLine(const ChanceContext& a_chance)
	{
		const auto base = Num1(std::clamp(a_chance.basePct, 0.0f, 100.0f));
		const auto effective = Num1(a_chance.effectivePct);
		switch (a_chance.mode) {
		case ChanceMode::kInactive: return "Currently inactive (base proc chance " + base + "%)";
		case ChanceMode::kCurrent: return "Current effective proc chance " + effective + "% (base " + base + "%)";
		case ChanceMode::kIfEquippedAlone: return "If equipped alone: effective proc chance " + effective + "% (base " + base + "%)";
		case ChanceMode::kNone: break;
		}
		return {};
	}

	// ------------------------------------------------------------------------------------------------ trigger

	[[nodiscard]] inline std::string TriggerText(const CardInput& a_in)
	{
		if (a_in.passiveOnly) return "Always active while equipped";
		const float chance = std::clamp(a_in.procChancePct, 0.0f, 100.0f);
		const bool always = chance >= 100.0f;
		std::string text;
		if (a_in.trigger == Trigger::kLowHealth) {
			text = always ? "When your Health drops below " : Num(chance) + "% when your Health drops below ";
			text += Num(a_in.lowHealthThresholdPct) + "%";
		} else {
			std::string_view event;
			std::string_view every;
			switch (a_in.trigger) {
			case Trigger::kHit:
				if (a_in.action == ActionKind::kCastOnCrit) { event = "on critical hit"; every = "Every critical hit"; }
				else if (a_in.action == ActionKind::kEchoStrike && a_in.echoRequirePowerAttack) { event = "on power attack hit"; every = "Every power attack hit"; }
				else { event = "on hit"; every = "Every hit"; }
				break;
			case Trigger::kIncomingHit: event = "when hit"; every = "Every time you are hit"; break;
			case Trigger::kDotApply: event = "when damage over time lands"; every = "Every damage-over-time hit"; break;
			case Trigger::kKill: event = "on kill"; every = "Every kill"; break;
			case Trigger::kLowHealth: break;
			}
			text = always ? std::string(every) : Num(chance) + "% " + std::string(event);
		}
		if (a_in.icdS > 0.0f) text += ", " + Num(a_in.icdS) + "s cooldown";
		if (a_in.perTargetIcdS > 0.0f) text += ", " + Num(a_in.perTargetIcdS) + "s per target";
		return text;
	}

	// ------------------------------------------------------------------------------------------------ build

	[[nodiscard]] inline Card Build(const CardInput& a_in)
	{
		Card card{};
		card.recipeId = a_in.recipeId;
		const auto* recipe = RuneforgePresentation::FindRecipe(a_in.recipeId);
		if (!recipe) return card;
		card.known = true;
		card.name = std::string(recipe->publicName);
		card.runes = RuneforgePresentation::PresentRuneSequence(a_in.internalRunes);
		card.glyphs = RuneforgePresentation::PresentGlyphSequence(a_in.internalRunes);
		card.summary = std::string(recipe->effectSummary);
		card.header = card.runes.empty() ? card.name : card.name + " (" + card.runes + ")";
		card.chancePct = a_in.passiveOnly ? 0.0f : std::clamp(a_in.procChancePct, 0.0f, 100.0f);
		card.cooldownS = a_in.icdS;
		card.perTargetCooldownS = a_in.perTargetIcdS;
		card.triggerText = TriggerText(a_in);

		std::vector<std::string> extras;
		auto addExtra = [&](std::string a_line) {
			if (a_line.empty()) return;
			if (std::find(extras.begin(), extras.end(), a_line) == extras.end()) extras.push_back(std::move(a_line));
		};
		std::vector<std::string> effects;
		std::size_t ordinal = 0;
		auto addEffects = [&](std::span<const EffectInput> a_effects, Role a_role, const AppliedMagnitude& a_applied,
							  std::vector<std::string>& a_out) {
			for (const auto& e : a_effects) {
				++ordinal;
				auto text = DescribeEffect(e, a_role, ordinal, a_applied, a_in.passiveOnly);
				if (text.skip) continue;
				card.exact = card.exact && text.exact;
				std::string line = text.text;
				// keep "area" off the end of a line that would overflow; it becomes its own line
				if (line.size() > kMaxLineChars && e.area > 0.0f) {
					const auto at = line.find(", area ");
					if (at != std::string::npos) {
						addExtra("Area: " + Num(e.area) + " ft");
						line.erase(at, line.find(" ft", at) + 3 - at);
					}
				}
				a_out.push_back(std::move(line));
			}
		};

		const Role actionRole = a_in.action == ActionKind::kEchoStrike ? Role::kEcho :
		                        (a_in.applyToSelf ? Role::kSelf : Role::kTarget);
		switch (a_in.action) {
		case ActionKind::kCorpseExplosion:
		case ActionKind::kSummonCorpseExplosion:
			// The action spell is only the damage carrier; the explosion numbers are the effect.
			if (a_in.corpseFlatDamage > 0.0f || a_in.corpsePctOfMaxHealth > 0.0f) {
				std::string line = "Explosion damage: ";
				if (a_in.corpseFlatDamage > 0.0f) line += Num(a_in.corpseFlatDamage);
				if (a_in.corpseFlatDamage > 0.0f && a_in.corpsePctOfMaxHealth > 0.0f) line += " + ";
				if (a_in.corpsePctOfMaxHealth > 0.0f) line += Num(a_in.corpsePctOfMaxHealth) + "% of corpse max Health";
				effects.push_back(line);
			}
			if (a_in.corpseRadius > 0.0f) {
				std::string line = "Explosion radius: " + Num(a_in.corpseRadius) + " units";
				if (a_in.corpseMaxTargets > 0u) line += ", up to " + std::to_string(a_in.corpseMaxTargets) + " targets";
				effects.push_back(line);
			}
			if (a_in.corpseMaxChainDepth > 1u) {
				addExtra("Chains up to " + std::to_string(a_in.corpseMaxChainDepth) + " times, " +
					Num(static_cast<double>(a_in.corpseChainFalloff) * 100.0) + "% damage per step");
			}
			break;
		case ActionKind::kAdaptive: {
			std::vector<std::string> perElement;
			std::vector<Element> elements;
			for (const auto& spell : a_in.adaptive) {
				std::vector<std::string> lines;
				addEffects(spell.effects, Role::kTarget, ResolveApplied(a_in, spell.effects), lines);
				std::string joined;
				for (const auto& l : lines) joined += (joined.empty() ? "" : "\n") + l;
				perElement.push_back(std::move(joined));
				elements.push_back(spell.element);
				if (const auto applied = ResolveApplied(a_in, spell.effects); !applied.rangeLine.empty()) addExtra(applied.rangeLine);
			}
			// Same effect for every element apart from the element word: one line "fire/frost/shock".
			auto neutral = [](std::string a_text, Element a_element) {
				const auto word = ElementName(a_element);
				if (word.empty()) return a_text;
				std::string lower = a_text;
				for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				const auto at = lower.find(word);
				if (at != std::string::npos) a_text.replace(at, word.size(), "{E}");
				return a_text;
			};
			bool merge = perElement.size() > 1;
			std::string merged = merge ? neutral(perElement[0], elements[0]) : std::string{};
			for (std::size_t i = 1; merge && i < perElement.size(); ++i) {
				merge = merged.find("{E}") != std::string::npos && neutral(perElement[i], elements[i]) == merged;
			}
			if (merge) {
				std::string names;
				for (const auto el : elements) names += (names.empty() ? "" : "/") + std::string(ElementName(el));
				const auto at = merged.find("{E}");
				merged.replace(at, 3, names);
				std::size_t start = 0;
				while (start <= merged.size()) {
					const auto nl = merged.find('\n', start);
					effects.push_back(Capitalize(merged.substr(start, nl == std::string::npos ? std::string::npos : nl - start)));
					if (nl == std::string::npos) break;
					start = nl + 1;
				}
			} else {
				for (std::size_t i = 0; i < perElement.size(); ++i) {
					std::size_t start = 0;
					while (start <= perElement[i].size()) {
						const auto nl = perElement[i].find('\n', start);
						effects.push_back(Capitalize(std::string(ElementName(elements[i]))) + ": " +
							perElement[i].substr(start, nl == std::string::npos ? std::string::npos : nl - start));
						if (nl == std::string::npos) break;
						start = nl + 1;
					}
				}
			}
			addExtra(a_in.adaptiveStrongest ? "Uses the element the target resists most" :
			                                  "Uses the element the target resists least");
			break;
		}
		default: {
			const auto applied = ResolveApplied(a_in, a_in.effects);
			addEffects(a_in.effects, actionRole, applied, effects);
			if (!applied.rangeLine.empty()) addExtra(applied.rangeLine);
			break;
		}
		}

		switch (a_in.action) {
		case ActionKind::kEchoStrike:
			if (a_in.echoDelayS > 0.0f) addExtra("Shadow echo: repeats each melee hit after " + Num(a_in.echoDelayS) + "s");
			if (a_in.echoWindowS > 0.0f) addExtra("Echo window: " + Num(a_in.echoWindowS) + "s");
			break;
		case ActionKind::kSpawnTrap:
			if (a_in.trapRadius > 0.0f || a_in.trapTtlS > 0.0f) {
				std::string line = "Trap";
				if (a_in.trapRadius > 0.0f) line += " radius " + Num(a_in.trapRadius) + " units";
				if (a_in.trapTtlS > 0.0f) line += std::string(a_in.trapRadius > 0.0f ? "," : "") + " lasts " + Num(a_in.trapTtlS) + "s";
				addExtra(line);
			}
			break;
		case ActionKind::kConvertDamage:
			if (a_in.convertPct > 0.0f) {
				const auto element = ElementName(a_in.convertElement);
				addExtra("Converts " + Num(a_in.convertPct) + "% of hit damage to " +
					(element.empty() ? std::string("elemental") : std::string(element)) + " damage");
			}
			break;
		case ActionKind::kMindOverMatter:
			if (a_in.mindOverMatterPct > 0.0f) {
				std::string line = "Redirects " + Num(a_in.mindOverMatterPct) + "% of damage taken to Magicka";
				if (a_in.mindOverMatterMaxPerHit > 0.0f) line += ", max " + Num(a_in.mindOverMatterMaxPerHit) + " per hit";
				addExtra(line);
			}
			break;
		case ActionKind::kArchmage:
			if (a_in.archmageDamagePctOfMaxMagicka > 0.0f) addExtra("Bonus damage: " + Num(a_in.archmageDamagePctOfMaxMagicka) + "% of max Magicka");
			if (a_in.archmageCostPctOfMaxMagicka > 0.0f) addExtra("Extra Magicka cost: " + Num(a_in.archmageCostPctOfMaxMagicka) + "% of max Magicka");
			break;
		default:
			break;
		}

		if (!a_in.evolutionThresholds.empty() && a_in.evolutionThresholds.size() == a_in.evolutionMultipliers.size()) {
			std::string line;
			for (std::size_t i = 0; i < a_in.evolutionThresholds.size(); ++i) {
				if (a_in.evolutionThresholds[i] == 0u || std::fabs(a_in.evolutionMultipliers[i] - 1.0f) < 0.005f) continue;
				line += (line.empty() ? "Grows with use: x" : ", x") + Num(a_in.evolutionMultipliers[i]) + " after " +
					std::to_string(a_in.evolutionThresholds[i]) + " procs";
			}
			addExtra(line);
		}
		if (std::fabs(a_in.effectiveness - 1.0f) >= 0.01f && a_in.action != ActionKind::kCorpseExplosion &&
			a_in.action != ActionKind::kSummonCorpseExplosion) {
			card.exact = false;
			addExtra("Effect strength: x" + Num(a_in.effectiveness));
		}

		// Passive (ability) effects while equipped.
		std::vector<std::string> passives;
		{
			const AppliedMagnitude none{};
			addEffects(a_in.passiveEffects, Role::kPassive, none, passives);
		}

		std::vector<std::string> conditions;
		if (a_in.trigger == Trigger::kLowHealth && !a_in.passiveOnly && a_in.lowHealthRearmPct > a_in.lowHealthThresholdPct) {
			conditions.push_back("Ready again once Health is above " + Num(a_in.lowHealthRearmPct) + "%");
		}
		if (a_in.requireRecentlyHitS > 0.0f) conditions.push_back("Only if you were hit in the last " + Num(a_in.requireRecentlyHitS) + "s");
		if (a_in.requireRecentlyKillS > 0.0f) conditions.push_back("Only within " + Num(a_in.requireRecentlyKillS) + "s of a kill");
		if (a_in.requireNotHitRecentlyS > 0.0f) conditions.push_back("Only if not hit in the last " + Num(a_in.requireNotHitRecentlyS) + "s");
		if (a_in.luckyHitChancePct > 0.0f) conditions.push_back("Lucky hit chance: " + Num(a_in.luckyHitChancePct) + "%");

		// Merge exact duplicates ("x2") so no line repeats.
		auto merge = [](std::vector<std::string>& a_lines) {
			std::vector<std::string> out;
			std::vector<std::size_t> counts;
			for (auto& l : a_lines) {
				const auto it = std::find(out.begin(), out.end(), l);
				if (it == out.end()) { out.push_back(std::move(l)); counts.push_back(1u); }
				else ++counts[static_cast<std::size_t>(it - out.begin())];
			}
			for (std::size_t i = 0; i < out.size(); ++i) {
				if (counts[i] > 1u) out[i] += " (x" + std::to_string(counts[i]) + ")";
			}
			a_lines = std::move(out);
		};
		merge(effects);
		merge(passives);

		// Text safety: fit, ASCII, forbidden tokens (fail closed to a generic label).
		std::size_t guardOrdinal = 0;
		auto safe = [&](std::vector<std::string> a_lines) {
			std::vector<std::string> out;
			for (auto& l : a_lines) {
				++guardOrdinal;
				for (auto& part : FitLine(std::move(l))) {
					if (!IsAsciiLine(part) || ContainsForbiddenToken(part, a_in)) {
						card.guarded = true;
						card.exact = false;
						part = "Effect " + std::to_string(guardOrdinal);
					}
					out.push_back(std::move(part));
				}
			}
			return out;
		};
		card.effectLines = safe(effects);
		card.passiveLines = safe(passives);
		card.extraLines = safe(extras);
		card.conditionLines = safe(conditions);
		card.chanceLine = ChanceLine(a_in.chance);

		// DD-B5-4 budget: header + trigger + body + optional chance line <= kMaxLines.
		auto headerLines = safe({ card.header });
		card.header = headerLines.front();
		auto triggerLines = safe({ card.triggerText });
		card.triggerText = triggerLines.front();
		// The details of the proc effect (scaling range, area) stay next to it; while-equipped effects follow.
		std::vector<std::string> body = card.effectLines;
		body.insert(body.end(), card.extraLines.begin(), card.extraLines.end());
		body.insert(body.end(), card.passiveLines.begin(), card.passiveLines.end());
		body.insert(body.end(), card.conditionLines.begin(), card.conditionLines.end());
		const std::size_t fixed = 2u + (card.chanceLine.empty() ? 0u : 1u);
		const std::size_t bodyBudget = kMaxLines - fixed;
		if (body.size() > bodyBudget) {
			card.overflow = body.size() - (bodyBudget - 1u);
			body.resize(bodyBudget - 1u);
			body.push_back("+" + std::to_string(card.overflow) + " more not shown");
		}
		card.lines.push_back(card.header);
		card.lines.push_back(card.triggerText);
		card.lines.insert(card.lines.end(), body.begin(), body.end());
		if (!card.chanceLine.empty()) card.lines.push_back(card.chanceLine);
		card.compactCount = std::min(card.lines.size(), 2u + std::min<std::size_t>(body.size(), kCompactLines - 2u));
		return card;
	}

	// ------------------------------------------------------------------------------------------------ consumers

	enum class Panel : std::uint8_t
	{
		kSelectItem,
		kSelectFormula,
		kConfirm,
		kResult,
	};

	// The card lines a workstation state shows below its own identity rows (the header line is never repeated):
	// the collapsed view (Details off) is the trigger line plus the first effect lines and a "+N more" hint; the
	// expanded view (Details on) and the completion card show every line. CONFIRM shows exact numbers either way.
	[[nodiscard]] inline std::vector<std::string> PanelBody(
		Panel a_panel,
		std::span<const std::string> a_lines,
		std::size_t a_compactCount,
		bool a_detail)
	{
		std::vector<std::string> out;
		if (a_lines.size() <= 1u) return out;
		const bool full = a_detail || a_panel == Panel::kResult;
		const std::size_t end = full ? a_lines.size() : std::clamp<std::size_t>(a_compactCount, 2u, a_lines.size());
		for (std::size_t i = 1; i < end; ++i) out.push_back(a_lines[i]);
		if (!full && end < a_lines.size()) {
			out.push_back("+" + std::to_string(a_lines.size() - end) + " more (Details)");
		}
		return out;
	}

	// Inventory tooltip (language 0): identity rows the Prisma view keys on ("Runeword: " title), the manifest summary,
	// then the card lines after the header.
	[[nodiscard]] inline std::string TooltipText(const Card& a_card)
	{
		if (!a_card.known) return {};
		std::string out = "Runeword: " + a_card.name;
		if (!a_card.runes.empty()) out += "\nRunes: " + a_card.runes;
		if (!a_card.summary.empty()) out += "\nEffect: " + a_card.summary;
		for (std::size_t i = 1; i < a_card.lines.size(); ++i) out += "\n" + a_card.lines[i];
		return out;
	}

	// JSON for the snapshot/render payloads. No key is named "hold": the AS2 view's signature() strips the first
	// "\"hold\":" it finds to redraw only the hold ring.
	template <class Json>
	[[nodiscard]] Json ToJson(const Card& a_card)
	{
		Json j = Json::object();
		j["v"] = kVersion;
		j["known"] = a_card.known;
		j["recipeId"] = a_card.recipeId;
		j["name"] = a_card.name;
		j["runes"] = a_card.runes;
		j["glyphs"] = a_card.glyphs;
		j["summary"] = a_card.summary;
		j["header"] = a_card.header;
		j["trigger"] = a_card.triggerText;
		j["chancePct"] = a_card.chancePct;
		j["cooldownS"] = a_card.cooldownS;
		j["perTargetCooldownS"] = a_card.perTargetCooldownS;
		j["effects"] = a_card.effectLines;
		j["extras"] = a_card.extraLines;
		j["passives"] = a_card.passiveLines;
		j["conditions"] = a_card.conditionLines;
		j["chance"] = a_card.chanceLine;
		j["lines"] = a_card.lines;
		j["compact"] = a_card.compactCount;
		j["overflow"] = a_card.overflow;
		j["exact"] = a_card.exact;
		return j;
	}
}
