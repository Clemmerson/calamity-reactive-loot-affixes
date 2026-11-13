# Calamity — FEASR Runeforge Fork

**Authorized Skyrim SE/AE source modification.** This English-documented fork extends **Calamity 2** by **Eutkyu** for the Runeforge project in **Fettered Existence — A Skyrim Redux**. FEASR integration is maintained by **Clemmerson**.

It includes native Runeforge workstation and menu integration, source-level compatibility work, and independently maintained source builds. The source baseline is **Calamity 2.2.8** at upstream commit [`989f215e81f3595dc4526a1311437946646a998a`](https://github.com/servaltullius/calamity-reactive-loot-affixes/commit/989f215e81f3595dc4526a1311437946646a998a). It does **not** include later Calamity 2.3.x changes. Current status: **development / pre-release**; no binary release is published, and RC1 gameplay certification remains on hold.

- Original author: **Eutkyu**
- FEASR integration: **Clemmerson**
- Original source: [servaltullius/calamity-reactive-loot-affixes](https://github.com/servaltullius/calamity-reactive-loot-affixes)
- Original mod: [Calamity 2 — Reactive Loot and Affixes on Nexus Mods](https://www.nexusmods.com/skyrimspecialedition/mods/193193)
- FEASR-specific technical support: open an issue in this repository and identify the installed DLL as the **FEASR version of `CalamityAffixes.dll`**.
- Archived upstream Korean README: [README.ko.md](README.ko.md)

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

For English documentation and the preserved Korean technical material index, start at [docs/INDEX.md](docs/INDEX.md). For the original Calamity player documentation, downloads, and non-FEASR support, use Eutkyu's [Nexus page](https://www.nexusmods.com/skyrimspecialedition/mods/193193).
