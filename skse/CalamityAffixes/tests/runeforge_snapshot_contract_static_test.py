#!/usr/bin/env python3
"""Producer/consumer field-path regression for the Runeforge snapshot (R8.7.4.6 P0).

R8.7.4.2 refused every legitimate formula selection because the workstation read `selectedInstanceKey` from the
nested `panel` object while RuneforgeMenu::BuildSnapshot writes it at the top level. This test ties three things
together so the same mismatch cannot recur silently:

1. The producer's top-level and `panel` key sets (parsed from BuildSnapshot) equal the key sets of
   tests/data/runeforge-snapshot-shape.v1.json, the fixture the C++ decision tests run against.
2. `selectedInstanceKey` is written only at the top level, and never inside the panel initializer.
3. The consumer reads selection identity only through SnapshotContract::ReadSelection on the whole snapshot, never
   through Panel(), and the mutating Operations calls are reachable only from the explicit Accept/Commit/operation
   handlers (hover/preview/navigation consume nothing).
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def contract_constants(header: str) -> dict[str, str]:
    return dict(re.findall(r'inline constexpr const char\* (k\w+) = "([^"]+)";', header))


def balanced_block(text: str, start: int) -> str:
    """Return text from the first '{' at/after start through its matching '}'."""
    open_at = text.index("{", start)
    depth = 0
    i = open_at
    in_str = False
    while i < len(text):
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[open_at:i + 1]
        i += 1
    raise AssertionError("unbalanced block")


def first_level_keys(block: str, constants: dict[str, str]) -> list[str]:
    """Keys of a nlohmann initializer-list object `{ {"k", v}, {SC::kX, v}, ... }` at depth 1."""
    keys: list[str] = []
    depth = 0
    i = 0
    in_str = False
    while i < len(block):
        c = block[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_str = False
            i += 1
            continue
        if c == '"':
            in_str = True
        elif c == "{":
            depth += 1
            if depth == 2:
                m = re.match(r'\{\s*(?:"([^"]+)"|SC::(k\w+))\s*,', block[i:])
                if m:
                    if m.group(1):
                        keys.append(m.group(1))
                    else:
                        require(m.group(2) in constants, f"unknown SnapshotContract constant SC::{m.group(2)}")
                        keys.append(constants[m.group(2)])
        elif c == "}":
            depth -= 1
        i += 1
    return keys


def enclosing_function(lines: list[str], index: int) -> str:
    for j in range(index, -1, -1):
        m = re.match(r"^\t\t(?:[\w:<>,]+[\s*&]+)+(\w+)\([^;]*$", lines[j])
        if m and not lines[j].lstrip().startswith(("if", "for", "while", "switch", "return")):
            return m.group(1)
    return "?"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    args = parser.parse_args()
    root = args.source_root.resolve()
    header = (root / "include/CalamityAffixes/RuneforgeWorkstationSnapshotContract.h").read_text(encoding="utf-8")
    producer = (root / "src/RuneforgeMenu.cpp").read_text(encoding="utf-8")
    consumer = (root / "src/RuneforgeWorkstation.cpp").read_text(encoding="utf-8")
    fixture = json.loads((root / "tests/data/runeforge-snapshot-shape.v1.json").read_text(encoding="utf-8"))
    constants = contract_constants(header)
    require(constants.get("kSelectedInstanceKey") == "selectedInstanceKey" and constants.get("kPanel") == "panel",
            "contract field names")

    # --- producer shape ------------------------------------------------------------------------------------------
    build = producer.index("json BuildSnapshot()")
    top_block = balanced_block(producer, producer.index("json snapshot =", build))
    top_keys = first_level_keys(top_block, constants)
    panel_start = producer.index("snapshot[SC::kPanel] = {", build)
    panel_block = balanced_block(producer, panel_start)
    panel_keys = first_level_keys(panel_block, constants)
    require("selectedInstanceKey" in top_keys, "producer writes selectedInstanceKey in the top-level initializer")
    require("selectedInstanceKey" not in panel_keys and "kSelectedInstanceKey" not in panel_block,
            "producer must never write selectedInstanceKey inside panel")
    require("snapshot[SC::kSelectedInstanceKey] = key;" in producer, "selected key assignment goes through the contract")
    require(not re.search(r'snapshot\[\s*"panel"\s*\]\s*\[\s*"selectedInstanceKey"', producer),
            "no nested panel selectedInstanceKey write")
    require(len(top_keys) == len(set(top_keys)) and len(panel_keys) == len(set(panel_keys)), "no duplicate producer keys")
    require(set(top_keys) == set(fixture.keys()),
            f"fixture top-level keys differ from BuildSnapshot: producer-only={sorted(set(top_keys) - set(fixture))} "
            f"fixture-only={sorted(set(fixture) - set(top_keys))}")
    require(set(panel_keys) == set(fixture["panel"].keys()),
            f"fixture panel keys differ from BuildSnapshot: producer-only={sorted(set(panel_keys) - set(fixture['panel']))} "
            f"fixture-only={sorted(set(fixture['panel']) - set(panel_keys))}")
    for name in ("kPanelRecipeId", "kPanelHasBase", "kPanelHasRecipe", "kPanelCanInsert", "kPanelComplete", "kPanelMissing",
                 "kPanelBaseCompatibilityWarning", "kPanelBaseCompatibilityMessage"):
        require(f"SC::{name}" in panel_block, f"panel field {name} is produced through the contract constant")
    base_block = balanced_block(producer, producer.index('snapshot["bases"].push_back(', build))
    recipe_block = balanced_block(producer, producer.index('snapshot["recipes"].push_back(', build))
    base_keys = first_level_keys(base_block, constants)
    recipe_keys = first_level_keys(recipe_block, constants)
    require(set(base_keys) == set(fixture["bases"][0].keys()), f"fixture base row keys differ from producer: {sorted(base_keys)}")
    require(set(recipe_keys) == set(fixture["recipes"][0].keys()), f"fixture recipe row keys differ from producer: {sorted(recipe_keys)}")
    require(constants["kBaseInstanceKey"] in base_keys and constants["kRecipeId"] in recipe_keys,
            "row identity fields the contract reads are produced")

    # --- consumer paths --------------------------------------------------------------------------------------------
    require(not re.search(r'Str\(\s*p\s*,\s*"selectedInstanceKey"', consumer), "consumer must not read selectedInstanceKey from panel")
    require(not re.search(r'Panel\(\)[^;\n]*selectedInstanceKey', consumer), "consumer must not read selectedInstanceKey via Panel()")
    require("g.selectedKey = Str(g.snapshot, SC::kSelectedInstanceKey);" in consumer, "workstation selected key reads the top level")
    select_formula = consumer[consumer.index("case SP::State::kSelectFormula: {"):]
    select_formula = select_formula[:select_formula.index("case SP::State::kOperations:")]
    require("SC::ReadSelection(g.snapshot)" in select_formula and "SC::DecideFormulaSelection(" in select_formula,
            "SELECT_FORMULA decides through the shared contract on the whole refreshed snapshot")
    require(select_formula.index("RefreshSnapshot();") < select_formula.index("SC::ReadSelection(g.snapshot)"),
            "the decision reads the refreshed snapshot")
    require(select_formula.index("RecomputeReadiness();") < select_formula.index("Dispatch({ SP::EventKind::kAccept, ready"),
            "readiness is recomputed before CONFIRM is dispatched")
    require("const auto& f = g.formulas[g.formulaFocus];" not in select_formula,
            "no reference into g.formulas may survive RefreshSnapshot()")

    # --- mutating operations only from explicit actions -----------------------------------------------------------
    lines = consumer.splitlines()
    allowed = {"Accept", "Commit", "RunOperation"}
    mutators = ("Operations::SelectBase(", "Operations::SelectRecipe(", "Operations::Transmute(", "Operations::CraftAffixes(",
                "Operations::ExpandAffix(", "Operations::ExchangeCurrency(")
    seen = 0
    for index, line in enumerate(lines):
        if any(m in line for m in mutators):
            seen += 1
            fn = enclosing_function(lines, index)
            require(fn in allowed, f"mutating Runeforge operation reachable from {fn} (line {index + 1}); preview must not mutate")
    require(seen >= 6, "expected mutating operation call sites were found")

    print("runeforge_snapshot_contract_static: OK")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as error:
        print(f"runeforge_snapshot_contract_static: FAIL: {error}", file=sys.stderr)
        sys.exit(1)
