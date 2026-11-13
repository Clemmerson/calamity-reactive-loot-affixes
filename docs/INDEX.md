# Documentation

This fork is pinned to **Calamity 2.2.8** at upstream commit `989f215e81f3595dc4526a1311437946646a998a`. It does not include later Calamity 2.3.x changes. The documents here describe source behavior and historical test procedures; they do not certify the FEASR RC1 gameplay release.

## Start here

- [FEASR fork overview](../README.md)
- [Build instructions and clean-build comparison](../BUILDING.md)
- [Compatibility and dependencies](../COMPATIBILITY.md)
- [Upstream attribution and permission scope](../UPSTREAM.md)
- [FEASR-specific support policy](../SUPPORT.md)
- [FEASR source change log](../CHANGELOG-FEASR.md)
- [Source and build checksum manifest](../SOURCE-MANIFEST.json)
- [Language and preservation policy](LANGUAGE-AND-TRANSLATION.md)

## Affixes and runewords

- [English affix catalog](AFFIX_CATALOG.en.md): counts, categories, and the translated overview of the upstream catalog.
- [Prefix effects](PREFIX_EFFECTS.md): generated public descriptions and values.
- [Suffix effects](SUFFIX_EFFECTS.md): generated family tiers and values.
- [Runeword effects](RUNEWORD_EFFECTS.md): generated rune combinations and descriptions.
- [VFX/SFX matrix](VFX_MATRIX.md): bilingual notes on visual and sound behavior.
- Korean originals remain available as [the generated Korean catalog](AFFIX_CATALOG.md) and [the original Korean index](INDEX.ko.md).

The pinned source data defines 73 public prefixes, 66 suffixes across 22 families, 95 runewords, and 33 rune weights. These are source/catalog counts, not a claim that every gameplay path passed RC1 certification.

## Testing references

- [In-game smoke checklist — English translation](QA_SMOKE_CHECKLIST.en.md), paired with [the Korean source](QA_SMOKE_CHECKLIST.md).
- [Stability scenarios — English translation](QA_STABILITY_SCENARIOS.en.md), paired with [the Korean source](QA_STABILITY_SCENARIOS.md).
- [Corpse currency compatibility policy](references/2026-07-14-corpse-runtime-currency-policy.md): includes an English summary followed by the original Korean detail.

These are procedures and recorded observations from earlier project work. Read their status notes before treating an item as verified. This fork preparation did not run them or certify RC1 gameplay.

## Korean planning archive

The upstream Korean design, planning, architecture, review, and release notes remain in their original paths and in inherited upstream history. Examples include `design/`, `plans/`, `reviews/`, `releases/`, the root `CHANGELOG.md`, and `스카이림 바닐라 효과 ARPG 변환 아이디어.md`. They are preserved as historical source material; they are not current build instructions or blanket compatibility guarantees.

The original Calamity README is also preserved at the repository root as [README.ko.md](../README.ko.md). For the source history and permission boundary, see [UPSTREAM.md](../UPSTREAM.md).
