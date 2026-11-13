#!/usr/bin/env python3
"""RunewordEffectCard v1 wiring (FE-ASR R8.7.4.21 lane I3; B5-panel offlineTests T4, T7, T8 source part).

The card itself is unit-tested in tests/runeword_effect_card_checks.inl (host) and over all 95 recipes in
tests/runeword_effect_card_catalog_check.py. This test proves the three surfaces actually consume it:

T4  RuneforgeMenu.cpp BuildSnapshot emits recipes[].effectCard from RunewordRecipeEntry::effectCard; the workstation
    BuildRender writes r["card"] in SELECT_ITEM, the formula/CONFIRM branch and RESULT_SUCCESS; CONFIRM lines come from
    RunewordEffectCardPolicy::PanelBody(Panel::kConfirm); the focused item card is fetched by instance key, read-only
    and cached; Commit() captures the card before the refresh; the card JSON has no "hold" key (AS2 signature()).
T5  the Details toggle is guarded by PromptPolicy::DetailToggleAllowed (no toggle under a filling hold).
T7  the lang-0 tooltip Runeword branch renders RunewordEffectCardPolicy::TooltipText and no longer the text-parsed
    upstream stats (RunewordResultAffixTooltipEn) or the record-name spell profile (formatAffixDetail(..., false)).
T8  AS2: no htmlText; the info card grows with content (no fixed 276 px frame drawn before measuring).

Usage: python3 tests/runeforge_effect_card_static_test.py --source-root <skse/CalamityAffixes>
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def between(text: str, start: str, end: str) -> str:
    a = text.index(start)
    b = text.index(end, a + len(start))
    return text[a:b]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    root = parser.parse_args().source_root.resolve()
    read = lambda rel: (root / rel).read_text(encoding="utf-8")
    header = read("include/CalamityAffixes/RunewordEffectCardPolicy.h")
    contracts = read("include/CalamityAffixes/RunewordUiContracts.h")
    entries = read("src/EventBridge.Loot.Runeword.RecipeEntries.cpp")
    tooltip = read("src/EventBridge.Loot.TooltipResolution.cpp")
    menu = read("src/RuneforgeMenu.cpp")
    ws = read("src/RuneforgeWorkstation.cpp")
    view = read("ui/runeforge/as2/src/RuneforgeWorkstationView.as")
    menu_view = read("ui/runeforge/as2/src/RuneforgeMenu.as")

    # ---- T4: producer
    require("RunewordEffectCardPolicy::Card effectCard{};" in contracts, "RunewordRecipeEntry carries the card")
    require("entries.back().effectCard = RunewordEffectCardPolicy::Build(BuildRunewordEffectCardInput(affix, recipe));" in entries,
            "GetRunewordRecipeEntries builds the card from the resolved AffixRuntime")
    recipe_block = between(menu, 'snapshot["recipes"].push_back(', "});")
    require('{"effectCard", RunewordEffectCardPolicy::ToJson<json>(recipe.effectCard)}' in recipe_block,
            "BuildSnapshot emits recipes[].effectCard")
    item_op = between(menu, "nlohmann::json ItemEffectCard(", "\n\t\t}\n")
    require("GetInstanceRunewordEffectCard(a_instanceKey, a_worn)" in item_op and "Select" not in item_op and "Transmute" not in item_op,
            "ItemEffectCard is a read-only key-addressed accessor")
    require('j["hold"]' not in header and '"hold"' not in between(header, "Json ToJson(", "return j;"),
            "card JSON keys never include 'hold'")

    # ---- T4: workstation consumer
    render = between(ws, "json BuildRender()", "void PushRender()")
    select_item = between(render, "case SP::State::kSelectItem:", "case SP::State::kSelectFormula:")
    require("FocusedItemCard(item)" in select_item and 'r["card"] = std::move(itemCard);' in select_item and
            "EC::PanelBody(EC::Panel::kSelectItem" in select_item, "SELECT_ITEM shows the focused item's card (G2)")
    formula = between(render, "case SP::State::kSelectFormula:\n\t\t\tcase SP::State::kConfirm:", "case SP::State::kOperations:")
    require("EC::Panel::kConfirm" in formula and "EC::PanelBody(panel, cardLines" in formula and 'r["card"] = std::move(card);' in formula,
            "SELECT_FORMULA/CONFIRM render the formula card; CONFIRM uses Panel::kConfirm")
    require('lines.push_back(Str(f, "effect"));\n\t\t\t\t\t\tfor (auto& line : cardBody) lines.push_back(std::move(line));' in formula,
            "CONFIRM lists the card lines (trigger, cooldown, exact effects) without Details")
    require('Str(f, "effectDetail")' not in render, "the record-name free-text effectDetail blob is no longer shown (G1/G3)")
    result = between(render, "case SP::State::kResultSuccess:", "default:")
    require('r["card"] = g.resultCard;' in result, "RESULT_SUCCESS carries the captured card")
    commit = between(ws, "void Commit()", "void Apply(")
    require(commit.index("g.resultCard = FormulaCard(formula);") < commit.rindex("RefreshSnapshot();") and
            "EC::PanelBody(EC::Panel::kResult" in commit, "Commit() captures the card before the post-commit refresh")
    require(commit.count("Operations::Transmute(") == 1, "the one commit path is unchanged")
    focused = between(ws, "const json& FocusedItemCard(", "json BuildRender()")
    require("g.itemCardRevision" in focused and "Operations::ItemEffectCard(" in focused, "focused card is cached by revision")
    detail = between(ws, "case IP::Command::kDetail:", "case IP::Command::kEmergency:")
    require("PRP::DetailToggleAllowed(g.session.state, g.session.hold >= 0)" in detail, "T5 Details guarded by the hold")

    # ---- T7: tooltip
    branch = between(tooltip, "if (a_uiLanguageMode == 0) {\n\t\t\t\t\tif (const auto* recipe = FindRunewordRecipeByResultAffixToken(affix.token))",
                     "auto resolvedName = resolveDisplayName(affix);")
    require("RunewordEffectCardPolicy::TooltipText(RunewordEffectCardPolicy::Build(cardInput))" in branch,
            "lang-0 Runeword tooltip renders the effect card")
    require("RunewordResultAffixTooltipEn" not in branch and "formatAffixDetail(affix, idxIt->second, a_candidate, false)" not in branch,
            "lang-0 Runeword tooltip no longer parses upstream stats or appends record-name spell profiles (G3/G5)")
    require("ResolveInstanceProcChanceContext(" in branch, "DD-B5-3 effective chance on the tooltip card")
    require(tooltip.count("ResolveInstanceProcChanceContext(") >= 3, "the existing chance line and the card share one resolver")

    # ---- T8: AS2
    require("htmlText" not in view and "htmlText" not in menu_view, "AS2 views keep TextField.text (no markup)")
    info = between(view, "private static function drawInfoCard(", "private static function drawRow(")
    require("var cardH:Number = (result ? 220 : 276) * scale;" not in info and "bottom - y" in info,
            "info card height follows content within the panel")
    require("wsLinesOverflow" in view, "plain-line states have an overflow guard (G8)")
    require("effectCard" in menu_view, "the Calamity 2 Runeforge menu shows the card lines")

    print("runeforge_effect_card_static: OK")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AssertionError, ValueError, OSError) as error:
        print(f"runeforge_effect_card_static: FAIL: {error}", file=sys.stderr)
        sys.exit(1)
