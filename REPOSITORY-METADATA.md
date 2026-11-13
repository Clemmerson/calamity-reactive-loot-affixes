# Repository publication metadata draft

## GitHub repository

- Intended repository: `Clemmerson/calamity-reactive-loot-affixes`
- Description: `An English-documented fork of Eutkyu's Calamity 2.2.8 for Fettered Existence — A Skyrim Redux Runeforge project. Includes native workstation integration, source-level compatibility work, and independently maintained source builds. Development / pre-release; no binary or gameplay certification.`
- Proposed topics: `skyrim-se`, `skyrim-ae`, `skse-plugin`, `commonlibsse-ng`, `calamity-2`, `runeforge`, `runeword`
- Pinned review base: `release/feasr-pc1.2` (Calamity v2.2.8 at `989f215e81f3595dc4526a1311437946646a998a`)
- Candidate branch: `feasr/pc1.2-public-prep`
- Stable FEASR branch: `feasr/pc1.2` (created from the reviewed candidate after source/document checks)
- Recommended default after public-content verification: `feasr/pc1.2`; keep fork `main` unchanged as the upstream tracking line.
- Candidate PR base: `release/feasr-pc1.2` (pinned to v2.2.8); do not target a newer upstream-derived `main` and do not merge until source and packaging certification is approved.
- Do not create a tag or GitHub release as part of this preparation.

## Candidate pull request

**Title:** Prepare FEASR PC1.2 source fork

**Body:**

> This candidate preserves upstream Calamity 2.2.8 history at `989f215e81f3595dc4526a1311437946646a998a` and adds the accepted FEASR PC1.1 source baseline plus the PC1.2 workstation readiness change. It includes Eutkyu attribution, the original Nexus link, FEASR-specific DLL/support identification, retained third-party notices, build/compatibility/support documentation, and a source/build checksum manifest. English is the primary landing-page/documentation language; the original Korean README, index, generated catalog, QA procedures, and historical design material are preserved alongside or in upstream history. It deliberately omits unaccepted duplicate-equipped-Runeword suppression and the unverified recipe/base incompatibility restriction found in a later local snapshot.
>
> A fresh clean source build succeeds, but its DLL hash differs from the previously accepted PC1.2 binary because the accepted build reused most PC1.1 objects. The manifest records both hashes; no binary is included. This PR does not create a release, claim RC1 gameplay certification, or clear unrelated external/game-derived assets. Packaging and gameplay certification remain separate release gates.
