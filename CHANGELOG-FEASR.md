# FEASR change log

This log records changes in the FEASR fork. It does not replace the upstream [Calamity changelog](https://github.com/servaltullius/calamity-reactive-loot-affixes/blob/main/CHANGELOG.md).

## PC1.2 source candidate — 2026-11-10

- Added a bounded readiness gate before the workstation places the player after a forced camera switch or weapon sheathe. It observes camera state, weapon state, and equip/unequip animation state before continuing, with a timeout that preserves the existing fail-closed acquisition behavior.
- Added first-person/weapon readiness and workstation exit/follower diagnostic observations.
- Source delta from PC1.1: `RuneforgeWorkstationSeatPolicy.h`, `RuneforgeWorkstation.cpp`, and `runeforge_workstation_policy_tests.cpp`.
- Recorded PC1.2 DLL SHA-256: `b987160bc406bef8dd773db6ac6b59684d311bcea9e5b9f06f847fc4d7a32dd4`.
- The package evidence marks live acceptance as not observed. This entry is source/build provenance and does not claim gameplay certification.

## PC1.1 source baseline — 2026-11-09

- Added the FEASR native Runeforge menu/workstation integration and its policy, inventory, runeword, and presentation support.
- Corrected workstation exit settlement for cell transitions and changed the initial camera framing direction to match the engine response.
- Added native policy fixtures and source guards for the FEASR integration.
- Recorded PC1.1 DLL SHA-256: `9e487ae7aaef03761c29ac793df6c5f0910c7fb0183f80353a70008124535f7a`.

## Public source preparation

- Registered the four accepted FEASR translation units in the native CMake target.
- Added attribution, support, compatibility, source provenance, and build documentation.
- No binary release or Nexus archive is part of this source preparation.
