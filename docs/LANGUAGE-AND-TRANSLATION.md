# Language and preservation policy

English is the primary language for the FEASR fork's landing page, build/support guidance, and curated documentation index. Eutkyu's original Korean material is retained in upstream history and, for the main entry pages, alongside English files.

## English/Korean pairs and translations

| English entry | Preserved Korean source | Treatment |
|---|---|---|
| `README.md` | `README.ko.md` | The Korean README is copied unchanged from pinned upstream commit `989f215e81f3595dc4526a1311437946646a998a`. The English landing page adds FEASR scope and status. |
| `docs/INDEX.md` | `docs/INDEX.ko.md` | The Korean index is preserved byte-for-byte from the pinned baseline. The English index corrects catalog counts against the pinned data and directs readers to English material first. |
| `docs/AFFIX_CATALOG.en.md` | `docs/AFFIX_CATALOG.md` | Translates the useful public overview. The Korean catalog remains the generator-owned file and was not edited. |
| `docs/QA_SMOKE_CHECKLIST.en.md` | `docs/QA_SMOKE_CHECKLIST.md` | Translates the install/build identity, proc observation, visual/audio, trap-marker, and save/load checks. Numeric thresholds, IDs, log strings, FormIDs, paths, and UI labels are preserved. |
| `docs/QA_STABILITY_SCENARIOS.en.md` | `docs/QA_STABILITY_SCENARIOS.md` | Translates the three historical stability procedures and keeps their 2026-07-27 context and unverified gameplay status. |
| `CHANGELOG-FEASR.md` | `CHANGELOG.md` | The FEASR-specific history is maintained in English. The upstream changelog remains in Korean in its original path and history. |

`docs/VFX_MATRIX.md` is already bilingual. The corpse-currency policy at `docs/references/2026-07-14-corpse-runtime-currency-policy.md` already contains an English summary followed by the Korean source detail; both are retained.

## Korean archive

Korean planning, design, architecture, review, and historical release documents remain in their original paths. The English index marks them as historical source material, not current build directions or compatibility guarantees. They were not mass-translated because many are dated proposals, superseded reviews, or records of work outside the pinned Calamity 2.2.8 FEASR source baseline.

The original Korean Agent-instructions sentence and machine-specific repository path were not copied into a public sidecar. `AGENTS.md` is now English and uses the repository root as a relative path; its upstream version remains in Git history.

## Translation limits

Translations are documentation-only. They do not change game mechanics, values, compatibility claims, or release status. Functional identifiers are kept verbatim: C++ symbols, file paths, FormIDs, Papyrus names, serialized values, localization keys, JSON identifiers, debug labels, and log strings. Existing Korean and English in-game localization remains in source data and was not changed.

The English smoke and stability documents translate procedures; they do not say those procedures were executed for FEASR PC1.2. RC1 gameplay certification remains on hold.
