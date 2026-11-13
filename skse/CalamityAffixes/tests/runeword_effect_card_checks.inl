// R8.7.4.21 lane I3: RunewordEffectCard v1 checks (B5-panel offlineTests T1, T2, T4 (pure part), T5, T7 (pure part) and the
// persistence derivation). Included by tests/runeforge_workstation_policy_tests.cpp so the native mutant runner
// (tests/runeforge_workstation_native_mutants.py) kills header mutants of RunewordEffectCardPolicy.h with these checks.
//
// The vectors copy the deployed Data/SKSE/Plugins/CalamityAffixes/affixes.json records (sha256 701ceff1...) by hand; the
// all-recipe generator is tests/runeword_effect_card_catalog_check.py (T3).

#include "CalamityAffixes/InstanceKeyStateMapTransfer.h"
#include "CalamityAffixes/RuneforgeWorkstationCompletionPolicy.h"
#include "CalamityAffixes/RunewordEffectCardPolicy.h"
#include "CalamityAffixes/RunewordEffectDetailPolicy.h"
#include "CalamityAffixes/SerializationWireContract.h"

#include <unordered_map>

namespace
{
	namespace EC = CalamityAffixes::RunewordEffectCardPolicy;

	std::string JoinLines(const std::vector<std::string>& a_lines)
	{
		std::string out;
		for (const auto& l : a_lines) out += (out.empty() ? "" : " | ") + l;
		return out;
	}

	void CheckLines(const EC::Card& a_card, const std::vector<std::string>& a_expected, const std::string& a_name)
	{
		Check(a_card.lines == a_expected, a_name + " card lines\n  got:  " + JoinLines(a_card.lines) + "\n  want: " + JoinLines(a_expected));
	}

	bool CardIsSafe(const EC::Card& a_card, const EC::CardInput& a_in)
	{
		if (a_card.lines.size() > EC::kMaxLines) return false;
		std::vector<std::string> seen;
		for (const auto& l : a_card.lines) {
			if (l.size() > EC::kMaxLineChars || !EC::IsAsciiLine(l) || EC::ContainsForbiddenToken(l, a_in)) return false;
			if (std::find(seen.begin(), seen.end(), l) != seen.end()) return false;
			seen.push_back(l);
		}
		return true;
	}

	EC::EffectInput Fx(std::string a_av, float a_mag, float a_dur, bool a_hostile, bool a_recover, float a_area = 0.0f,
		std::string a_resist = {}, EC::Archetype a_arch = EC::Archetype::kValueModifier, std::string a_record = {})
	{
		EC::EffectInput e{};
		e.actorValue = std::move(a_av);
		e.magnitude = a_mag;
		e.durationS = a_dur;
		e.hostile = a_hostile;
		e.recover = a_recover;
		e.area = a_area;
		e.resistValue = std::move(a_resist);
		e.archetype = a_arch;
		e.recordName = std::move(a_record);
		return e;
	}

	// rw_leaf / Embercraft: runeword_leaf_final (Hit 22%, ICD 7s, CastSpell Self CAFF_SPEL_RW_LEAF_FLAME DestructionModifier 20, 8s).
	EC::CardInput LeafInput()
	{
		EC::CardInput in{};
		in.recipeId = "rw_leaf";
		in.internalRunes = { "Tir", "Ral" };
		in.upstreamName = "Leaf";
		in.trigger = EC::Trigger::kHit;
		in.procChancePct = 22.0f;
		in.icdS = 7.0f;
		in.action = EC::ActionKind::kCastSpell;
		in.applyToSelf = true;
		in.effects = { Fx("DestructionModifier", 20.0f, 8.0f, false, true, 0.0f, {}, EC::Archetype::kValueModifier, "Calamity: Runeword Leaf") };
		return in;
	}

	// T1: Embercraft golden + negative control (the pre-I3 tooltip composition leaks the record name).
	void EffectCardGoldenChecks()
	{
		const auto in = LeafInput();
		const auto card = EC::Build(in);
		Check(card.known && card.name == "Embercraft" && card.runes == "Cael-Hyr" && card.glyphs == "cael,hyr" &&
				card.summary == "Destruction skill boost",
			"T1 Embercraft identity comes from the manifest");
		CheckLines(card, { "Embercraft (Cael-Hyr)", "22% on hit, 7s cooldown", "Destruction skill: +20 for 8s" }, "T1 Embercraft");
		Check(card.chancePct == 22.0f && card.cooldownS == 7.0f && card.exact && !card.guarded && card.compactCount == 3u,
			"T1 Embercraft numbers equal the input and the card is exact");
		Check(CardIsSafe(card, in), "T1 Embercraft card is ASCII, within budget, without forbidden tokens or duplicates");

		// T7 (pure part): the lang-0 tooltip for rw_leaf.
		const auto tooltip = EC::TooltipText(card);
		Check(tooltip == "Runeword: Embercraft\nRunes: Cael-Hyr\nEffect: Destruction skill boost\n22% on hit, 7s cooldown\nDestruction skill: +20 for 8s",
			"T7 rw_leaf lang-0 tooltip golden: " + tooltip);
		Check(tooltip.find("Runeword Leaf") == std::string::npos && tooltip.find("Calamity:") == std::string::npos &&
				tooltip.rfind("Runeword: ", 0) == 0,
			"T7 rw_leaf tooltip has no record name and keeps the Prisma 'Runeword: ' title key");

		// Negative control: the pre-I3 lang-0 composition (RunewordResultAffixTooltipEn + appendSpellEffectProfile with the
		// harvested record display name) carries "Runeword Leaf"; the same safety predicate must reject it.
		using CalamityAffixes::RunewordEffectDetailPolicy::SpellEffect;
		const std::vector<SpellEffect> legacyEffects{ SpellEffect{ .name = "Runeword Leaf", .magnitude = 20.0f, .durationSeconds = 8u, .area = 0u } };
		const std::string legacy = CalamityAffixes::RuneforgePresentation::RunewordResultAffixTooltipEn("rw_leaf", in.internalRunes,
			"Runeword Leaf (Tir-Ral): 22% on hit / ICD 7s - Leaf Flame (Destruction +20, 8s)") +
			"\nSpell effects:\n" + CalamityAffixes::RunewordEffectDetailPolicy::FormatSpellEffects(legacyEffects, 1u, false);
		bool legacyLeaks = false;
		for (const auto& line : Split(legacy, '\n')) legacyLeaks = legacyLeaks || EC::ContainsForbiddenToken(line, in);
		Check(legacyLeaks && legacy.find("Runeword Leaf (Magnitude 20; Duration 8s)") != std::string::npos,
			"T7 negative control: the pre-I3 tooltip leaks 'Runeword Leaf' and the safety predicate catches it");

		// Unknown actor value: generic 'Effect n' with the raw magnitude, card inexact (DD-B5-1/2).
		EC::CardInput unknown = in;
		unknown.effects[0].actorValue = "UnknownAV";
		const auto unknownCard = EC::Build(unknown);
		Check(!unknownCard.exact && unknownCard.lines.size() == 3u && unknownCard.lines[2] == "Effect 1: magnitude 20 for 8s",
			"unknown actor value falls back to 'Effect n' and marks the card inexact (DD-B5-1/2)");
		// Guard: a line that would carry a record name fails closed to a generic label (G3).
		EC::CardInput leak = in;
		leak.effects[0].recordName = "Destruction skill";
		const auto leakCard = EC::Build(leak);
		Check(leakCard.guarded && !leakCard.exact && leakCard.lines.size() == 3u && leakCard.lines[2] == "Effect 1",
			"a line containing a record name is replaced (fail closed)");
	}

	// T2: family vectors (one per mechanic family).
	void EffectCardFamilyChecks()
	{
		using EC::Archetype;
		using EC::ActionKind;
		using EC::Trigger;
		{  // adaptive + override (rw_chaos / Elemental Rupture)
			EC::CardInput in{};
			in.recipeId = "rw_chaos";
			in.internalRunes = { "Fal", "Ohm", "Um" };
			in.upstreamName = "Chaos";
			in.procChancePct = 16.0f;
			in.icdS = 5.5f;
			in.action = ActionKind::kAdaptive;
			in.magnitudeOverride = 45.0f;
			in.adaptive = {
				{ EC::Element::kFire, { Fx("Health", 1.0f, 1.0f, true, false, 0.0f, "ResistFire", Archetype::kValueModifier, "Calamity: Physical->Fire Conversion") } },
				{ EC::Element::kFrost, { Fx("Health", 1.0f, 1.0f, true, false, 0.0f, "ResistFrost") } },
				{ EC::Element::kShock, { Fx("Health", 1.0f, 1.0f, true, false, 0.0f, "ResistShock") } },
			};
			const auto card = EC::Build(in);
			CheckLines(card, { "Elemental Rupture (Syth-Aethyr-Vael)", "16% on hit, 5.5s cooldown", "Fire/frost/shock damage: 45",
								 "Uses the element the target resists least" },
				"T2 adaptive (chaos)");
			Check(CardIsSafe(card, in), "T2 chaos safe (no 'Physical->Fire Conversion')");
		}
		{  // corpse explosion (rw_plague / Plaguebloom-family) -- the carrier spell is not shown
			EC::CardInput in{};
			in.recipeId = "rw_plague";
			in.internalRunes = {};
			in.upstreamName = "Plague";
			in.trigger = Trigger::kKill;
			in.procChancePct = 40.0f;
			in.icdS = 4.0f;
			in.action = ActionKind::kCorpseExplosion;
			in.effects = { Fx("Health", 1.0f, 1.0f, true, false, 0.0f, "PoisonResist") };
			in.corpseFlatDamage = 12.0f;
			in.corpsePctOfMaxHealth = 3.0f;
			in.corpseRadius = 450.0f;
			in.corpseMaxTargets = 12u;
			in.corpseMaxChainDepth = 2u;
			in.corpseChainFalloff = 0.7f;
			const auto card = EC::Build(in);
			const std::vector<std::string> body(card.lines.begin() + 1, card.lines.end());
			Check(body == std::vector<std::string>{ "40% on kill, 4s cooldown", "Explosion damage: 12 + 3% of corpse max Health",
							   "Explosion radius: 450 units, up to 12 targets", "Chains up to 2 times, 70% damage per step" },
				"T2 corpse explosion (plague): " + JoinLines(body));
		}
		{  // echo strike (rw_shadow_boxer)
			EC::CardInput in{};
			in.recipeId = "rw_shadow_boxer";
			in.upstreamName = "Shadow Boxer";
			in.procChancePct = 100.0f;
			in.icdS = 12.0f;
			in.action = ActionKind::kEchoStrike;
			in.effects = { Fx("Health", 1.0f, 0.0f, true, false) };
			in.scaling = { EC::ScaleSource::kHitPhysical, 0.4f, 0.0f, 0.0f, 0.0f, false };
			in.echoDelayS = 0.4f;
			in.echoWindowS = 8.0f;
			const auto card = EC::Build(in);
			const std::vector<std::string> body(card.lines.begin() + 1, card.lines.end());
			Check(body == std::vector<std::string>{ "Every hit, 12s cooldown", "Echo damage: 40% of physical hit damage",
							   "Shadow echo: repeats each melee hit after 0.4s", "Echo window: 8s" },
				"T2 echo (shadow boxer): " + JoinLines(body));
		}
		{  // passive sentinel (rw_wealth)
			EC::CardInput in{};
			in.recipeId = "rw_wealth";
			in.upstreamName = "Wealth";
			in.passiveOnly = true;
			in.action = ActionKind::kOther;
			in.passiveEffects = { Fx("CarryWeight", 75.0f, 0.0f, false, true), Fx("SpeechcraftModifier", 15.0f, 0.0f, false, true) };
			const auto card = EC::Build(in);
			const std::vector<std::string> body(card.lines.begin() + 1, card.lines.end());
			Check(body == std::vector<std::string>{ "Always active while equipped", "Carry weight: +75", "Speech skill: +15" },
				"T2 passive sentinel (wealth): " + JoinLines(body));
		}
		{  // low health (rw_exile threshold 40, rearm default threshold + 10)
			EC::CardInput in{};
			in.recipeId = "rw_exile";
			in.upstreamName = "Exile";
			in.trigger = Trigger::kLowHealth;
			in.procChancePct = 100.0f;
			in.icdS = 50.0f;
			in.lowHealthThresholdPct = 40.0f;
			in.lowHealthRearmPct = 50.0f;
			in.action = ActionKind::kCastSpell;
			in.applyToSelf = true;
			in.effects = { Fx("DamageResist", 250.0f, 10.0f, false, true) };
			const auto card = EC::Build(in);
			const std::vector<std::string> body(card.lines.begin() + 1, card.lines.end());
			Check(body == std::vector<std::string>{ "When your Health drops below 40%, 50s cooldown", "Armor rating: +250 for 10s",
							   "Ready again once Health is above 50%" },
				"T2 low health (exile): " + JoinLines(body));
		}
		{  // per-target cooldown + adaptive debuff (rw_infinity)
			EC::CardInput in{};
			in.recipeId = "rw_infinity";
			in.upstreamName = "Infinity";
			in.procChancePct = 100.0f;
			in.icdS = 0.5f;
			in.perTargetIcdS = 6.0f;
			in.action = ActionKind::kAdaptive;
			in.adaptive = {
				{ EC::Element::kFire, { Fx("ResistFire", 20.0f, 5.0f, true, true) } },
				{ EC::Element::kFrost, { Fx("ResistFrost", 20.0f, 5.0f, true, true) } },
				{ EC::Element::kShock, { Fx("ResistShock", 20.0f, 5.0f, true, true) } },
			};
			const auto card = EC::Build(in);
			Check(card.perTargetCooldownS == 6.0f && card.lines.size() >= 3u && card.lines[1] == "Every hit, 0.5s cooldown, 6s per target" &&
					card.lines[2] == "Target fire/frost/shock resistance: -20% for 5s",
				"T2 per-target (infinity): " + JoinLines(card.lines));
		}
		{  // magnitude scaling (rw_destruction / Stormfield): never the spell base as an exact magnitude; 0.04 -> 4%
			EC::CardInput in{};
			in.recipeId = "rw_destruction";
			in.upstreamName = "Destruction";
			in.procChancePct = 30.0f;
			in.icdS = 5.0f;
			in.action = ActionKind::kCastSpell;
			in.effects = { Fx("Health", 10.0f, 5.0f, true, false, 350.0f, "ResistShock", Archetype::kValueModifier, "Calamity: Runeword Destruction Bleed") };
			in.scaling = { EC::ScaleSource::kHitPhysical, 0.04f, 0.0f, 0.0f, 40.0f, true };
			const auto card = EC::Build(in);
			const std::vector<std::string> body(card.lines.begin() + 1, card.lines.end());
			Check(body == std::vector<std::string>{ "30% on hit, 5s cooldown", "Shock damage: 4% of physical hit damage per second for 5s",
							   "Area: 350 ft", "Scaled value: 10 to 40" },
				"T2 scaling (destruction): " + JoinLines(body));
			bool presentsBaseAsExact = false;
			for (const auto& l : card.lines) presentsBaseAsExact = presentsBaseAsExact || l.find("Magnitude 10") != std::string::npos || l.find(": 10 per") != std::string::npos;
			Check(!presentsBaseAsExact && CardIsSafe(card, in), "T2 scaling: spell base 10 is not presented as the applied magnitude; no 'Runeword Destruction Bleed'");
		}
		{  // override + scaling (rw_obsession): scaling wins over the fallback override; passive ability listed
			EC::CardInput in{};
			in.recipeId = "rw_obsession";
			in.upstreamName = "Obsession";
			in.procChancePct = 34.0f;
			in.icdS = 5.0f;
			in.action = ActionKind::kCastSpell;
			in.effects = { Fx("Health", 1.0f, 0.0f, true, false) };
			in.magnitudeOverride = 75.0f;
			in.scaling = { EC::ScaleSource::kHitTotal, 0.18f, 0.0f, 75.0f, 300.0f, false };
			in.passiveEffects = { Fx("MagickaRateMult", 25.0f, 0.0f, false, true) };
			const auto card = EC::Build(in);
			const std::vector<std::string> body(card.lines.begin() + 1, card.lines.end());
			Check(body == std::vector<std::string>{ "34% on hit, 5s cooldown", "Damage: 18% of hit damage", "Scaled value: 75 to 300",
							   "Magicka regen: +25% (always)" },
				"T2 override (obsession): " + JoinLines(body));
		}
		{  // Mult actor values as percent (rw_beast / Feral Ascendance): AttackDamageMult 0.3 -> +30%
			EC::CardInput in{};
			in.recipeId = "rw_beast";
			in.upstreamName = "Beast";
			in.procChancePct = 30.0f;
			in.icdS = 18.0f;
			in.action = ActionKind::kCastSpell;
			in.applyToSelf = true;
			in.effects = { Fx("AttackDamageMult", 0.3f, 10.0f, false, true), Fx("DamageResist", 150.0f, 10.0f, false, true),
				Fx("WeaponSpeedMult", 0.15f, 10.0f, false, true) };
			const auto card = EC::Build(in);
			const std::vector<std::string> body(card.lines.begin() + 1, card.lines.end());
			Check(body == std::vector<std::string>{ "30% on hit, 18s cooldown", "Attack damage: +30% for 10s", "Armor rating: +150 for 10s",
							   "Attack speed: +15% for 10s" },
				"T2 Mult AV (beast): " + JoinLines(body));
			Check(card.compactCount == 4u, "T2 beast compact view = header, trigger, two effects");
		}
		{  // duplicates merge (three identical effects) and target debuff wording
			EC::CardInput in = LeafInput();
			in.applyToSelf = false;
			in.effects = { Fx("SpeedMult", 30.0f, 5.0f, true, true), Fx("SpeedMult", 30.0f, 5.0f, true, true) };
			const auto card = EC::Build(in);
			Check(card.lines.size() == 3u && card.lines[2] == "Target move speed: -30% for 5s (x2)", "duplicate effects merge into one line");
		}
		{  // budget: overflow keeps <= kMaxLines and says how much was cut
			EC::CardInput in = LeafInput();
			in.effects.clear();
			for (int i = 0; i < 9; ++i) in.effects.push_back(Fx("DamageResist", static_cast<float>(10 + i), 5.0f, false, true));
			const auto card = EC::Build(in);
			Check(card.lines.size() == EC::kMaxLines && card.overflow == 4u && card.lines.back() == "+4 more not shown",
				"line budget is enforced with an explicit overflow line: " + JoinLines(card.lines));
		}
		{  // number formatting never rounds a nonzero share to 0 (G7)
			Check(EC::Num(0.04 * 100.0) == "4" && EC::Num(0.004) == "0.004" && EC::Num(5.5) == "5.5" && EC::Num(7.0) == "7" && EC::Num(0.12f * 100.0) == "12",
				"compact numbers");
		}
		{  // unknown recipe keeps the caller's legacy path
			EC::CardInput in = LeafInput();
			in.recipeId = "rw_not_in_manifest";
			Check(!EC::Build(in).known && EC::TooltipText(EC::Build(in)).empty(), "unknown recipes produce no card");
		}
		{  // DD-B5-3 chance line uses the existing tooltip wording
			EC::CardInput in = LeafInput();
			in.chance = { EC::ChanceMode::kIfEquippedAlone, 22.0f, 18.5f };
			const auto card = EC::Build(in);
			Check(card.lines.back() == "If equipped alone: effective proc chance 18.5% (base 22%)" && card.lines.size() == 4u,
				"instance card appends the effective proc chance line");
		}
	}

	// T4 (pure part) + T5: panel bodies and Details parity.
	void EffectCardPanelChecks()
	{
		namespace PR = WS::PromptPolicy;
		using WS::InputPolicy::Button;
		using WS::InputPolicy::Command;
		using WS::InputPolicy::Phase;
		EC::CardInput in = LeafInput();
		in.effects.push_back(Fx("DamageResist", 50.0f, 8.0f, false, true));
		in.effects.push_back(Fx("SpeedMult", 20.0f, 8.0f, false, true));
		const auto card = EC::Build(in);
		const auto confirm = EC::PanelBody(EC::Panel::kConfirm, card.lines, card.compactCount, false);
		Check(confirm.size() == 4u && confirm[0] == "22% on hit, 7s cooldown" && confirm[1] == "Destruction skill: +20 for 8s" &&
				confirm[3] == "+1 more (Details)",
			"T4 CONFIRM shows trigger, cooldown and exact effect numbers without Details: " + JoinLines(confirm));
		const auto confirmFull = EC::PanelBody(EC::Panel::kConfirm, card.lines, card.compactCount, true);
		Check(confirmFull.size() == card.lines.size() - 1u && confirmFull.back() == "Move speed: +20% for 8s", "T4 CONFIRM Details shows the full card");
		const auto item = EC::PanelBody(EC::Panel::kSelectItem, card.lines, card.compactCount, false);
		Check(!item.empty() && item[0] == "22% on hit, 7s cooldown", "T4 SELECT_ITEM card body starts with the trigger line");
		const auto result = EC::PanelBody(EC::Panel::kResult, card.lines, card.compactCount, false);
		Check(result.size() == card.lines.size() - 1u, "T4 RESULT shows every card line");
		CalamityAffixes::RuneforgeWorkstation::CompletionPolicy::Summary summary{ "Embercraft", "Iron Sword", "Destruction skill boost", "Cael-Hyr", result };
		const auto resultLines = CalamityAffixes::RuneforgeWorkstation::CompletionPolicy::BuildLines(summary);
		Check(resultLines.size() == 4u + result.size() && resultLines[4] == "22% on hit, 7s cooldown", "T4 completion lines carry the effect card");
		const auto json = EC::ToJson<nlohmann::json>(card).dump();
		Check(json.find("\"hold\":") == std::string::npos && json.find("\"lines\":") != std::string::npos, "T4 card JSON has no 'hold' key (AS2 signature())");

		// T5: Details parity in every state (footer <-> input map <-> visible effect) and hold isolation in CONFIRM.
		const State states[] = { State::kEntering, State::kSelectItem, State::kSelectFormula, State::kOperations, State::kConfirm,
			State::kCommitting, State::kPerforming, State::kResultSuccess, State::kResultFailure };
		for (const auto st : states) {
			bool prompted = false;
			for (const bool detail : { false, true }) {
				const auto plan = PR::PlanFooter(st, detail);
				for (std::size_t i = 0; i < plan.count; ++i) prompted = prompted || plan.prompts[i].action == "detail";
			}
			WS::InputPolicy::Tracker tr{};
			const bool mapped = WS::InputPolicy::Map(st, Button::kDetail, Phase::kDown, 0.0f, tr).command == Command::kDetail;
			Check(prompted == PR::DetailVisible(st) && mapped == PR::DetailVisible(st),
				std::string("T5 Details parity in ") + std::string(WS::StatePolicy::Name(st)));
		}
		Check(PR::DetailVisible(State::kSelectItem) && PR::DetailVisible(State::kConfirm) && !PR::DetailVisible(State::kOperations),
			"T5 Details is offered in SELECT_ITEM and CONFIRM (G9), not in OPERATIONS");
		{
			WS::InputPolicy::Tracker tr{};
			const auto down = WS::InputPolicy::Map(State::kConfirm, Button::kDetail, Phase::kDown, 0.0f, tr).command;
			const auto held = WS::InputPolicy::Map(State::kConfirm, Button::kDetail, Phase::kHeld, 0.9f, tr).command;
			const auto up = WS::InputPolicy::Map(State::kConfirm, Button::kDetail, Phase::kUp, 1.0f, tr).command;
			WS::StatePolicy::Event ev{};
			Check(down == Command::kDetail && held == Command::kNone && up == Command::kNone && !WS::InputPolicy::ToStateEvent(down, ev),
				"T5 CONFIRM Details never issues HoldBegin/HoldEnd or any state event");
			Check(PR::DetailToggleAllowed(State::kConfirm, false) && !PR::DetailToggleAllowed(State::kConfirm, true) &&
					PR::DetailToggleAllowed(State::kSelectItem, false) && !PR::DetailToggleAllowed(State::kOperations, false),
				"T5 Details is ignored while the engrave hold is filling");
			// The hold itself completes regardless of a Details press in between (StatePolicy sees no Details event).
			WS::StatePolicy::Session s{};
			s.state = State::kConfirm;
			s.row = WS::StatePolicy::Ack::kOk;
			WS::StatePolicy::Effects fx;
			WS::StatePolicy::Timing timing{};
			WS::StatePolicy::Step(s, { EventKind::kHoldBegin, 0, 0, {} }, fx, timing);
			const auto holdAfterBegin = s.hold;
			WS::StatePolicy::Step(s, { EventKind::kTick, timing.holdConfirmMs + 1, 0, {} }, fx, timing);
			Check(holdAfterBegin == 0 && s.state == State::kCommitting, "T5 an uninterrupted hold still commits (Details adds no event)");
		}
		// Negative control: the pre-I3 footer (no Details in SELECT_ITEM) violated parity: InputPolicy toggled g.detail there.
		{
			WS::InputPolicy::Tracker tr{};
			const bool preI3MappedInSelectItem = WS::InputPolicy::Map(State::kSelectItem, Button::kDetail, Phase::kDown, 0.0f, tr).command == Command::kDetail;
			Check(preI3MappedInSelectItem, "T5 control: SELECT_ITEM maps Details (the pre-I3 footer omitted it -> parity failure)");
		}
	}

	// Persistence: the card is derived from data that survives drop/pickup and co-save reload. The only per-instance
	// input is the IAXF slot list (instanceKey -> affix tokens); everything else is config (recipe/affix runtime + plugin
	// spells) rebuilt at load. Round-trip IAXF through the production wire writer and the v7 layout the loader reads,
	// move it with the production key-transfer helper, then rebuild the card from the token.
	void EffectCardPersistenceChecks()
	{
		using namespace CalamityAffixes;
		constexpr std::uint64_t kLeafToken = 0x00C0FFEE1EAFull;
		const std::unordered_map<std::uint64_t, EC::CardInput> configByToken{ { kLeafToken, LeafInput() } };
		auto cardForKey = [&](const std::unordered_map<std::uint64_t, std::vector<std::uint64_t>>& a_iaxf, std::uint64_t a_key) {
			const auto it = a_iaxf.find(a_key);
			if (it == a_iaxf.end()) return EC::Card{};
			for (const auto token : it->second) {
				if (const auto cfg = configByToken.find(token); cfg != configByToken.end()) return EC::Build(cfg->second);
			}
			return EC::Card{};
		};
		const std::uint64_t playerKey = (static_cast<std::uint64_t>(0x00012E49u) << 16) | 41u;
		const std::uint64_t worldKey = 0xFFFF000000000000ull | 0xFF001234ull;
		const std::uint64_t pickedKey = (static_cast<std::uint64_t>(0x00012E49u) << 16) | 52u;
		std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> iaxf{ { playerKey, { 0x1111ull, kLeafToken } } };
		const auto before = cardForKey(iaxf, playerKey);
		const auto identity = [](std::uint64_t a_key) { return a_key; };
		const auto rekey = [](std::uint64_t, std::uint64_t a_newKey) { return a_newKey; };
		const bool dropped = RemapInstanceKeyedMapEntries(iaxf, playerKey, worldKey, identity, rekey);
		const bool picked = RemapInstanceKeyedMapEntries(iaxf, worldKey, pickedKey, identity, rekey);
		const auto afterPickup = cardForKey(iaxf, pickedKey);
		Check(dropped && picked && before.known && afterPickup.lines == before.lines && cardForKey(iaxf, playerKey).lines.empty(),
			"persistence: drop/pickup moves the IAXF slots and the card rebuilt from the new key is identical");

		// co-save: write IAXF v7 with the production writer, read it back with the loader's layout.
		SerializationWire::CurrentSaveSnapshot snap{};
		SerializationWire::InstanceAffixEntry entry{};
		entry.baseFormId = 0x00012E49u;
		entry.uniqueId = 52u;
		entry.affixCount = 2u;
		entry.tokens[0] = 0x1111ull;
		entry.tokens[1] = kLeafToken;
		snap.instanceAffixes.push_back(entry);
		std::vector<std::uint8_t> bytes;
		auto write = [&bytes](auto a_value) {
			for (std::size_t i = 0; i < sizeof(a_value); ++i) bytes.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(a_value) >> (8u * i)) & 0xFFu));
			return true;
		};
		const bool wrote = SerializationWire::WriteInstanceAffixesPayload(write, snap);
		std::size_t cursor = 0;
		auto read = [&](auto& a_value) {
			using T = std::remove_reference_t<decltype(a_value)>;
			if (cursor + sizeof(T) > bytes.size()) return false;
			std::uint64_t v = 0;
			for (std::size_t i = 0; i < sizeof(T); ++i) v |= static_cast<std::uint64_t>(bytes[cursor++]) << (8u * i);
			a_value = static_cast<T>(v);
			return true;
		};
		std::uint32_t count = 0;
		std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> loaded;
		bool readOk = read(count) && count == 1u;
		for (std::uint32_t i = 0; readOk && i < count; ++i) {
			std::uint32_t base = 0;
			std::uint16_t uid = 0;
			std::uint8_t n = 0;
			readOk = read(base) && read(uid) && read(n);
			std::vector<std::uint64_t> tokens;
			for (std::size_t s = 0; readOk && s < CalamityAffixes::kMaxAffixesPerItem; ++s) {
				std::uint64_t t = 0;
				readOk = read(t);
				if (s < n) tokens.push_back(t);
			}
			loaded[(static_cast<std::uint64_t>(base) << 16) | uid] = tokens;
		}
		const auto afterReload = cardForKey(loaded, pickedKey);
		Check(wrote && readOk && cursor == bytes.size() && afterReload.lines == before.lines,
			"persistence: co-save IAXF round trip rebuilds the identical card (no card text is persisted)");
	}

	void EffectCardChecks()
	{
		EffectCardGoldenChecks();
		EffectCardFamilyChecks();
		EffectCardPanelChecks();
		EffectCardPersistenceChecks();
	}
}
