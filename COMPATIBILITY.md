# Compatibility and dependencies

## Source baseline

- FEASR's accepted source baseline is Calamity 2.2.8 at upstream commit `989f215e81f3595dc4526a1311437946646a998a`.
- Later upstream versions are not merged into the PC1.2 source candidate. Compatibility with Calamity 2.3.x is not claimed.
- The PC1.2 native build used CommonLibSSE-NG and was exercised against Skyrim runtime 1.6.1170 in the surrounding FEASR validation environment. Do not infer support for other runtimes from that record.

## Runtime dependencies

Follow the exact versions and installation guidance on [the original Calamity Nexus page](https://www.nexusmods.com/skyrimspecialedition/mods/193193). The upstream dependency set includes:

- Skyrim Special Edition / Anniversary Edition with a matching SKSE64 and Address Library build.
- Prisma UI for Calamity's interface.
- SkyUI, MCM Helper, KID, and powerofthree's Tweaks are optional or recommended according to upstream Calamity's documentation; their status depends on the feature being used.

This repository contains source code, not a player-ready FEASR archive. It does not bundle Calamity's full runtime package, Runeforge plugins, voice files, or game assets. A future distribution must publish its own exact dependency list and independently audit every included file.

## Compatibility status

The PC1.2 source compiles and links in an independent clean build. Its output is not byte-identical to the previously accepted DLL; see [BUILDING.md](BUILDING.md) and [SOURCE-MANIFEST.json](SOURCE-MANIFEST.json). No gameplay certification was performed by this audit. RC1 gameplay certification remains separate and incomplete.
