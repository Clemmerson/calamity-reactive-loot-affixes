# Affix Catalog — Calamity 2.2.8 Source Baseline

> English translation of the upstream Korean catalog at `docs/AFFIX_CATALOG.md`.
> The Korean generated source remains unchanged. Counts below describe the pinned
> source catalog; they do not certify that every gameplay path passed RC1 QA.

Updated: 2026-10-09  
Baseline: `v2.2.8`  
Source: `affixes/affixes.json`  
Internal entries are omitted from the public catalog.

This overview summarizes the affix structure visible to players. See the detailed
effect documents for numeric values and individual descriptions.

## Catalog size

- Prefixes: **73**
- Suffixes: **66**
- Runewords: **95**

## Detailed catalogs

- [Prefixes](PREFIX_EFFECTS.md): effect chances, ICDs, normal-attack procs, and duplicate-equip rules.
- [Suffixes](SUFFIX_EFFECTS.md): T1–T3 family values and equipped-tier aggregation.
- [Runewords](RUNEWORD_EFFECTS.md): rune combinations, recommended bases, and individual effects.

## Proc and stacking rules in the source catalog (RC6)

- Bear Trap and Critical Cast have a normal-attack proc path. See the prefix catalog for each effect's chance and ICD.
- Identical standard-proc affixes combine the chances of at most three equipped copies with diminishing returns, then execute the effect once. This is not a damage multiplier shared by every prefix.
- Critical Cast can trigger up to two different spells on melee critical hits and power attacks, and up to one spell on ordinary melee attacks, bows, and crossbows. A proc batch shares a global 0.15-second ICD.
- Tiers in the same suffix family aggregate: T1 + T1 becomes T2; T1 + T2 becomes T3; the maximum is T3. Only the resulting tier's effect applies.

## Prefix categories

| Category | Summary |
|---|---|
| Elemental and spell hits | Added elemental damage, resistance weakening, and adaptive elemental responses |
| Summons and utility | Summons, resource recovery, movement, stealth, and speed support |
| Defense and survival | Defensive buffs on incoming hits and Magicka-conversion effects |
| Special mechanics | Build mechanics such as Archmage and ConvertDamage |
| Critical Cast | Extra-spell procs on critical hits, power attacks, and ordinary attacks |
| Passive and scrolls | Always-on support and consumable-saving effects |
| Traps, explosions, and control | Bloom, Trap, Corpse Explosion, and summon-explosion effects |
| Shouts and school expansion | Thu'um themes and expanded magic-school effects |
| Other | Remaining public prefixes outside the groups above |

## Suffix categories

Suffixes contain 22 families and 66 entries, mostly arranged in T1/T2/T3 tiers.

| Category | Examples (IDs preserved) |
|---|---|
| Resources and survival | `vitality`, `endurance`, `brilliance`, `guardian`, `spell_ward` |
| Regeneration and utility | `regeneration`, `tenacity`, `meditation`, `steed`, `swiftness` |
| Combat specialization | `swordsman`, `champion`, `marksman`, `gladiator`, `assassin` |
| Defense, stealth, and support | `evasion`, `fortitude`, `bulwark`, `shadow` |
| Crafting and magic | `enchanter`, `conjurer` |

See the [suffix catalog](SUFFIX_EFFECTS.md) for the full tier tables and aggregation examples.

## Runewords

The pinned source catalog contains **95** individually defined runewords. The detailed
catalog uses the in-game display strings. See [Runeword Effects](RUNEWORD_EFFECTS.md).

## Publication scope

This catalog documents entries players can obtain, reforge, or apply. Internal
companion definitions and generator-only support entries are omitted. It favors
consistency with in-game behavior and public descriptions over implementation detail.
