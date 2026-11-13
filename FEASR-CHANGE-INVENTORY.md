# FEASR source change inventory

- Upstream baseline: `989f215e81f3595dc4526a1311437946646a998a` (Calamity 2.2.8).
- FEASR PC1.1 source reconstruction: `6ba338b` (unaccepted local RW95 changes excluded).
- Accepted PC1.2 workstation readiness change: `0c75027`.
- Native CMake registration: `207d391`.
- Documentation-only English-primary translation commit: `6055aa2`; no gameplay or build source changed in that commit.
- Native source files under `include/`, `src/`, and `tests/`: 296.
- Changed paths from the pinned baseline, excluding the self-referential manifest file: 131 (73 added, 58 modified, 0 deleted).
- No newer upstream commit was merged. Exact per-file byte lengths and SHA-256 values are in `SOURCE-MANIFEST.json`.
- The complete build comparison is documented in `BUILDING.md`; the clean build is not byte-identical to the accepted PC1.2 DLL.
- Two unaccepted local changes are excluded: duplicate-equipped-Runeword suppression and the recipe/base incompatibility restriction.

```text
M	AGENTS.md
A	BUILDING.md
A	CHANGELOG-FEASR.md
A	COMPATIBILITY.md
A	FEASR-CHANGE-INVENTORY.md
A	README.ko.md
M	README.md
A	REPOSITORY-METADATA.md
A	SUPPORT.md
M	THIRD_PARTY_NOTICES.txt
A	UPSTREAM.md
A	docs/AFFIX_CATALOG.en.md
A	docs/INDEX.ko.md
M	docs/INDEX.md
A	docs/LANGUAGE-AND-TRANSLATION.md
A	docs/QA_SMOKE_CHECKLIST.en.md
A	docs/QA_STABILITY_SCENARIOS.en.md
M	skse/CalamityAffixes/CMakeLists.txt
M	skse/CalamityAffixes/include/CalamityAffixes/AffixCraftingPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/DropPickupInventoryIdentityPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/DropPickupOwnershipPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/DropReferenceIdentityPolicy.h
M	skse/CalamityAffixes/include/CalamityAffixes/EventBridge.h
M	skse/CalamityAffixes/include/CalamityAffixes/EventNames.h
A	skse/CalamityAffixes/include/CalamityAffixes/IdentificationRitualPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/InstanceKeyStateMapTransfer.h
M	skse/CalamityAffixes/include/CalamityAffixes/InstanceKeyTransferPolicy.h
M	skse/CalamityAffixes/include/CalamityAffixes/LootRerollGuard.h
M	skse/CalamityAffixes/include/CalamityAffixes/LootRuntimeState.h
M	skse/CalamityAffixes/include/CalamityAffixes/PrismaTooltip.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeActivationIntentPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeInventorySnapshotPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeMenu.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeMenuCallbackPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeMenuOpenPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeMenuOperations.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeMenuRacePolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgePresentation.generated.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgePresentation.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeReaderInputContract.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstation.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationCompletionPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationContextPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationControlsPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationExitSettlementPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationFramingPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationFreeCameraLayout.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationInputPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationLayoutPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationLeasePolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationPromptPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationProtocolPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationReadinessPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationSeatPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationShotPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationSnapshotContract.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationStagePolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationStatePolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationSweepPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationUiOwnershipPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuneforgeWorkstationViewPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RunewordContractCompatibilityPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RunewordEffectCardPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RunewordEffectDetailPolicy.h
A	skse/CalamityAffixes/include/CalamityAffixes/RunewordRuneWeights.h
M	skse/CalamityAffixes/include/CalamityAffixes/RunewordUiContracts.h
A	skse/CalamityAffixes/include/CalamityAffixes/RunewordUiPresentation.h
A	skse/CalamityAffixes/include/CalamityAffixes/RuntimeContractDocumentPolicy.h
M	skse/CalamityAffixes/include/CalamityAffixes/SerializationCurrentRecordReader.h
M	skse/CalamityAffixes/include/CalamityAffixes/SerializationWireContract.h
M	skse/CalamityAffixes/include/CalamityAffixes/detail/EventBridge.Constants.inl
M	skse/CalamityAffixes/include/CalamityAffixes/detail/EventBridge.PrivateApi.inl
M	skse/CalamityAffixes/include/CalamityAffixes/detail/EventBridge.PublicApi.inl
M	skse/CalamityAffixes/include/CalamityAffixes/detail/EventBridge.StateGroups.inl
M	skse/CalamityAffixes/include/CalamityAffixes/detail/EventBridge.Types.inl
M	skse/CalamityAffixes/src/EventBridge.Config.AffixParsing.cpp
M	skse/CalamityAffixes/src/EventBridge.Config.AffixRegistry.cpp
M	skse/CalamityAffixes/src/EventBridge.Config.RunewordSynthesis.cpp
M	skse/CalamityAffixes/src/EventBridge.Events.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Assign.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.BaseSelection.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.Catalog.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.Crafting.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.Detail.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.Expand.cpp
A	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.InventoryPreparation.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.PanelState.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.Policy.cpp
A	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.Presentation.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.RecipeEntries.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.RecipeUi.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.Reforge.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.Selection.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Runeword.Transmute.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.Service.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.TooltipResolution.cpp
M	skse/CalamityAffixes/src/EventBridge.Loot.cpp
M	skse/CalamityAffixes/src/EventBridge.Serialization.Lifecycle.cpp
M	skse/CalamityAffixes/src/EventBridge.Serialization.Load.Records.inl
M	skse/CalamityAffixes/src/EventBridge.Serialization.Load.cpp
M	skse/CalamityAffixes/src/EventBridge.Serialization.Save.cpp
M	skse/CalamityAffixes/src/EventBridge.Triggers.ActivateEvent.cpp
M	skse/CalamityAffixes/src/EventBridge.Triggers.ModCallback.Handlers.cpp
M	skse/CalamityAffixes/src/EventBridge.Triggers.ModCallback.cpp
M	skse/CalamityAffixes/src/Papyrus.cpp
M	skse/CalamityAffixes/src/PrismaTooltip.HandleUiCommand.inl
M	skse/CalamityAffixes/src/PrismaTooltip.MenuInput.inl
M	skse/CalamityAffixes/src/PrismaTooltip.PanelData.inl
M	skse/CalamityAffixes/src/PrismaTooltip.SettingsLayout.inl
M	skse/CalamityAffixes/src/PrismaTooltip.SharedState.inl
M	skse/CalamityAffixes/src/PrismaTooltip.Tick.inl
M	skse/CalamityAffixes/src/PrismaTooltip.ViewLifecycle.inl
M	skse/CalamityAffixes/src/PrismaTooltip.cpp
A	skse/CalamityAffixes/src/RuneforgeMenu.cpp
A	skse/CalamityAffixes/src/RuneforgeWorkstation.cpp
M	skse/CalamityAffixes/src/RunewordContractSnapshot.cpp
M	skse/CalamityAffixes/src/main.cpp
A	skse/CalamityAffixes/tests/data/runeforge-snapshot-shape.v1.json
A	skse/CalamityAffixes/tests/data/workstation-vectors.txt
A	skse/CalamityAffixes/tests/data/workstation-vectors.v1.txt
A	skse/CalamityAffixes/tests/data/workstation-vectors.v2.txt
A	skse/CalamityAffixes/tests/data/workstation-vectors.v3.txt
A	skse/CalamityAffixes/tests/run_runeforge_workstation_checks.sh
A	skse/CalamityAffixes/tests/runeforge_back_input_static_test.py
A	skse/CalamityAffixes/tests/runeforge_effect_card_static_test.py
A	skse/CalamityAffixes/tests/runeforge_presentation_state_static_test.py
A	skse/CalamityAffixes/tests/runeforge_reader_input_static_test.py
A	skse/CalamityAffixes/tests/runeforge_snapshot_contract_static_test.py
A	skse/CalamityAffixes/tests/runeforge_workstation_native_mutants.py
A	skse/CalamityAffixes/tests/runeforge_workstation_policy_tests.cpp
A	skse/CalamityAffixes/tests/runeword_effect_card_checks.inl
```
