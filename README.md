# FEASR Runeforge — Calamity 2 source fork

**A FEASR-maintained source fork for the Runeforge expansion.** This is a community fork of **Calamity — Reactive Loot & Affixes** by **Eutkyu**. It is not an official Calamity release.

- Original mod: [Calamity 2 — Reactive Loot and Affixes on Nexus Mods](https://www.nexusmods.com/skyrimspecialedition/mods/193193)
- Original source: [servaltullius/calamity-reactive-loot-affixes](https://github.com/servaltullius/calamity-reactive-loot-affixes)
- FEASR-specific technical support: open an issue in this repository and identify the installed DLL as the **FEASR version of `CalamityAffixes.dll`**.

## What this fork contains

This branch preserves the upstream Calamity history and adds the accepted FEASR native source baseline through PC1.2. The changes cover Runeforge menu/workstation integration, supporting inventory and runeword policies, and the PC1.1/PC1.2 workstation corrections. See [FEASR-CHANGE-INVENTORY.md](FEASR-CHANGE-INVENTORY.md) and [SOURCE-MANIFEST.json](SOURCE-MANIFEST.json) for the exact source commits and checksums.

The repository is being prepared as a **source project**. It does not publish a new game archive or GitHub binary release. The recorded PC1.2 DLL checksum is provenance evidence; it does not claim that RC1 has gameplay certification or that a fresh clone has reproduced that binary byte-for-byte.

## Attribution and permission

The Runeforge expansion and its modified Calamity source/DLL are published with the mod author's permission. Credit **Eutkyu**, link the original Nexus page above, and identify the modified plugin as the **FEASR version** so FEASR-specific support reaches this project. See [UPSTREAM.md](UPSTREAM.md) for the scope and boundaries.

The permission is scope-specific. It does not clear unrelated external, voice, or game-derived assets. This repository does not add a blanket license for upstream code; no `LICENSE` file is intended until the project owner and rights holders approve one.

## Build, dependencies, and support

- [BUILDING.md](BUILDING.md) describes the CMake/vcpkg setup and the limits of reproducing the recorded PC1.2 build.
- [COMPATIBILITY.md](COMPATIBILITY.md) lists the upstream baseline and known runtime/dependency scope.
- [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt) retains the upstream library notices.
- [SUPPORT.md](SUPPORT.md) explains where to report FEASR and upstream issues.

For the original Calamity player documentation, downloads, and non-FEASR support, use Eutkyu's [Nexus page](https://www.nexusmods.com/skyrimspecialedition/mods/193193).
