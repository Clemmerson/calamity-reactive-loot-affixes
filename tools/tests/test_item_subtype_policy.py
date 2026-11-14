from __future__ import annotations

import importlib.util
import json
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _load_view_source() -> str:
    """The Prisma view as one buffer, across however many files it is split."""
    path = REPO_ROOT / "tools" / "prisma_view_source.py"
    spec = importlib.util.spec_from_file_location("prisma_view_source", path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.load_view_source()


class ItemSubtypePolicyTests(unittest.TestCase):
    def test_suffix_contract_is_exact_and_armor_remains_unrestricted(self) -> None:
        suffixes = json.loads(
            (REPO_ROOT / "affixes/modules/keywords.affixes.suffixes.json").read_text(encoding="utf-8")
        )
        expected = {
            "swordsman": ["OneHandedMelee"],
            "champion": ["TwoHandedMelee"],
            "marksman": ["Bow", "Crossbow"],
            "eagle_eye": ["Bow", "Crossbow"],
        }

        counts = {family: 0 for family in expected}
        for affix in suffixes:
            family = affix["family"]
            if family in expected:
                counts[family] += 1
                self.assertEqual(affix.get("weaponSubtypes"), expected[family], affix["id"])
            else:
                self.assertNotIn("weaponSubtypes", affix, affix["id"])

        self.assertEqual(counts, {family: 3 for family in expected})

        policy = (REPO_ROOT / "skse/CalamityAffixes/include/CalamityAffixes/ItemSubtypePolicy.h").read_text(
            encoding="utf-8"
        )
        self.assertIn("if (!a_isWeapon)", policy)
        self.assertIn("return true;", policy)

    def test_all_new_suffix_roll_paths_pass_actual_weapon_subtype(self) -> None:
        preview = (REPO_ROOT / "skse/CalamityAffixes/src/EventBridge.Loot.AffixSlotRoll.cpp").read_text(
            encoding="utf-8"
        )
        service = (REPO_ROOT / "skse/CalamityAffixes/src/EventBridge.Loot.Service.cpp").read_text(
            encoding="utf-8"
        )
        tooltip = (REPO_ROOT / "skse/CalamityAffixes/src/EventBridge.Loot.TooltipResolution.cpp").read_text(
            encoding="utf-8"
        )
        reforge = (REPO_ROOT / "skse/CalamityAffixes/src/EventBridge.Loot.Runeword.Reforge.cpp").read_text(
            encoding="utf-8"
        )
        assign = (REPO_ROOT / "skse/CalamityAffixes/src/EventBridge.Loot.Assign.cpp").read_text(
            encoding="utf-8"
        )

        self.assertIn("IsSuffixWeaponSubtypeEligible(", preview)
        self.assertIn("IsSuffixWeaponSubtypeEligible(", service)
        self.assertIn("ResolveWeaponSubtype(a_item->object->As<RE::TESObjectWEAP>())", tooltip)
        self.assertIn(
            "RollSuffixIndex(*lootType, weaponSubtype, &keptFamilies, &excludedIndices)",
            reforge,
        )
        self.assertNotIn("IsSuffixWeaponSubtypeEligible(", assign)

    def test_specialized_runeword_warning_is_non_blocking(self) -> None:
        ui = _load_view_source()
        panel = (REPO_ROOT / "skse/CalamityAffixes/src/EventBridge.Loot.Runeword.PanelState.cpp").read_text(
            encoding="utf-8"
        )
        policy = (REPO_ROOT / "skse/CalamityAffixes/include/CalamityAffixes/ItemSubtypePolicy.h").read_text(
            encoding="utf-8"
        )

        recipe_entries = (
            REPO_ROOT / "skse/CalamityAffixes/src/EventBridge.Loot.Runeword.RecipeEntries.cpp"
        ).read_text(encoding="utf-8")

        self.assertIn("const canTransmute = Boolean(state.canInsert)", ui)
        self.assertIn("baseCompatibilityWarning", ui)
        self.assertIn("transmute remains allowed", panel)
        self.assertIn("각인할 수 있지만", panel)
        self.assertNotIn("engraving is blocked", panel)
        self.assertIn("a_panelState.baseCompatibilityWarning = true;", panel)
        self.assertIn("panelState.canInsert = CanInsertRunewordFromPanel(ready, canApplyResult);", panel)
        self.assertIn('a_recipeId == "rw_strength"', policy)
        self.assertIn('a_recipeId == "rw_edge" || a_recipeId == "rw_zephyr"', policy)
        self.assertIn('a_recipeId == "rw_oath"', policy)
        self.assertIn('a_recipeId == "rw_myth"', policy)
        self.assertIn('a_recipeId == "rw_unbending_will"', policy)
        self.assertNotIn('a_recipeId == "rw_dragon"', policy)
        self.assertIn('{ "rw_strength", "two_handed_melee" }', recipe_entries)
        self.assertIn('{ "rw_edge", "bow" }', recipe_entries)
        self.assertIn('{ "rw_zephyr", "bow" }', recipe_entries)
        self.assertIn('{ "rw_oath", "one_handed_melee" }', recipe_entries)
        self.assertIn('{ "rw_myth", "heavy_armor" }', recipe_entries)
        self.assertIn('{ "rw_unbending_will", "shield" }', recipe_entries)


if __name__ == "__main__":
    unittest.main()
