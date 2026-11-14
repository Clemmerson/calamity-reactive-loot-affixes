#!/usr/bin/env python3
from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def load(relative: str):
    return json.loads((REPO_ROOT / relative).read_text(encoding="utf-8"))


def collect_record_names(node, out: list[str]) -> None:
    if isinstance(node, dict):
        for key, value in node.items():
            if key == "name" and isinstance(value, str) and " / 룬워드 " in value:
                out.append(value)
            collect_record_names(value, out)
    elif isinstance(node, list):
        for value in node:
            collect_record_names(value, out)


class RunewordDisplayNameTests(unittest.TestCase):
    """Tooltips show nameEn/nameKo. Seven multi-word runewords once kept only
    their first word there ("Runeword Holy" for Holy Thunder), and 37 Korean
    tooltips used a translated name ("혼돈") while the panel recipe list, fed by
    the DLL's kRunewordNameKoRows, showed the transliteration ("카오스")."""

    def setUp(self) -> None:
        self.recipes = load("affixes/runeword.contract.json")["runewordCatalog"]
        self.affixes = {e["id"]: e for e in load("affixes/modules/keywords.affixes.runewords.json")}
        catalog_cpp = (REPO_ROOT / "skse/CalamityAffixes/src/EventBridge.Loot.Runeword.Catalog.cpp").read_text(
            encoding="utf-8"
        )
        self.panel_names_ko = dict(re.findall(r'\{ "(rw_[a-z0-9_]+)", "([^"]+)" \}', catalog_cpp))

    def test_english_tooltip_names_carry_the_full_recipe_name_and_runes(self) -> None:
        for recipe in self.recipes:
            affix = self.affixes[recipe["resultAffixId"]]
            expected = f"Runeword {recipe['name']} ({'-'.join(recipe['runes'])}): "
            self.assertTrue(affix["nameEn"].startswith(expected), f"{recipe['id']}: {affix['nameEn']}")

    def test_korean_tooltip_names_match_the_panel_recipe_list(self) -> None:
        self.assertEqual(len(self.recipes), len(self.panel_names_ko))
        for recipe in self.recipes:
            affix = self.affixes[recipe["resultAffixId"]]
            expected = f"룬워드 {self.panel_names_ko[recipe['id']]} [{'-'.join(recipe['runes'])}]: "
            self.assertTrue(affix["nameKo"].startswith(expected), f"{recipe['id']}: {affix['nameKo']}")
            self.assertEqual(affix["name"], affix["nameKo"], recipe["id"])

    def test_record_names_use_the_recipe_and_panel_names(self) -> None:
        # The ESP keeps only the English half of "EN / KO" record names (the
        # active effect name in game); the Korean half stays a source label.
        for recipe in self.recipes:
            names: list[str] = []
            collect_record_names(self.affixes[recipe["resultAffixId"]].get("records", {}), names)
            for name in names:
                english, korean = name.split(" / 룬워드 ", 1)
                self.assertTrue(english.startswith(f"Calamity: Runeword {recipe['name']}"), name)
                self.assertTrue(korean.startswith(self.panel_names_ko[recipe["id"]]), name)

if __name__ == "__main__":
    unittest.main()


class RunewordRecipeNameLanguageTests(unittest.TestCase):
    """RunewordRecipe::displayName is the Korean name. 2.2.1 still let it into
    the English base label "[Runeword: 카오스]" and three HUD notes, because
    each site copied the name on its own. Text built in the DLL uses HudName()
    (English); the panel gets En/Ko pairs. Only the catalog assignment, Korean-name fallbacks, and the legacy
    Korean payload fields may read displayName directly. English synthesis
    falls back to recipe ID, not a Korean name."""

    ALLOWED = {
        ("EventBridge.Loot.Runeword.Catalog.cpp", "recipe.displayName = recipe.displayNameKo;"),
        (
            "EventBridge.Config.RunewordSynthesis.cpp",
            "const std::string recipeNameKo = recipe.displayNameKo.empty() ? recipe.displayName : recipe.displayNameKo;",
        ),
        ("EventBridge.Loot.Runeword.RecipeEntries.cpp", ".displayName = recipe.displayName,"),
        ("EventBridge.Loot.Runeword.BaseSelection.cpp", "displayName.append(completed->displayName);"),
        ("EventBridge.Loot.Runeword.PanelState.cpp", "panelState.recipeName = currentRecipe->displayName;"),
        ("EventBridge.Loot.Runeword.PanelState.cpp", "panelState.recipeName = recipe->displayName;"),
    }

    def test_only_legacy_payload_fields_read_the_korean_recipe_name(self) -> None:
        pattern = re.compile(r"\b(?:a_)?(?:recipe|completed\w*|current\w*|\w+Recipe)(?:->|\.)displayName\b(?!En|Ko)")
        found = set()
        for path in sorted((REPO_ROOT / "skse/CalamityAffixes/src").glob("*")):
            if path.suffix not in (".cpp", ".inl", ".h"):
                continue
            for line in path.read_text(encoding="utf-8").splitlines():
                if pattern.search(line):
                    found.add((path.name, line.strip()))
        self.assertEqual(found, self.ALLOWED)
        synthesis = (REPO_ROOT / "skse/CalamityAffixes/src/EventBridge.Config.RunewordSynthesis.cpp").read_text(encoding="utf-8")
        self.assertIn("recipe.displayNameEn.empty() ? recipe.id : recipe.displayNameEn", synthesis)
        self.assertNotIn("recipe.displayNameEn.empty() ? recipe.displayName :", synthesis)


if __name__ == "__main__":
    unittest.main()
