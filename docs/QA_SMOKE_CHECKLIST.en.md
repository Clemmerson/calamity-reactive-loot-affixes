# In-Game Smoke Checklist — English Translation

This is a translation of the Korean checklist at [QA_SMOKE_CHECKLIST.md](QA_SMOKE_CHECKLIST.md). It is a procedure, not evidence that the FEASR PC1.2 candidate or RC1 has passed it. RC1 gameplay certification remains on hold.

Repeat this minimum verification when installing a test build. One principle governs the checklist: **logs prove only that an engine path ran.** Use your eyes and ears to decide whether something appeared, sounded, or left a lingering visual. Do not treat `instantiated=true`, `played=true`, or `magic effect apply observed` as proof of visual or audio success.

## 1. Installation

- [ ] In MO2, install with **Replace** on the existing mod. Do not Merge; old files may remain.
- [ ] Launch the game and confirm a new `CalamityAffixes.log` is created in `Documents/My Games/Skyrim Special Edition/SKSE/`.

## 2. Build identity

- [ ] Confirm the installed DLL SHA-256 matches the value stated for the distribution ZIP:

```bash
sha256sum "<MO2 mods folder>/CalamityAffixes/SKSE/Plugins/CalamityAffixes.dll"
```

- Do not rely on the first log line's `build <date> <time>` banner by itself. Incremental builds can leave a stale banner. Use the hash or evidence of a new log line.

## 3. Debug settings

- [ ] In `Data/SKSE/Plugins/CalamityAffixes/user_settings.json`, set `debugVerboseLogging=true` and `debugHudNotifications=true`.
- To visually check proc chances, you may temporarily raise `runtime.procChanceMultiplier` (maximum 3.0). **Restore it to 1.0 afterward.**

## 4. Proc pipeline (log observation)

- [ ] Equip a proc affix or runeword and make at least 20 hits on an enemy that survives several hits.
- [ ] On a proc, confirm the HUD displays `CAFF proc: <affixId>`.
- [ ] After the session, run:

```bash
python3 tools/qa_skse_logcheck.py
```

- [ ] Confirm `RESULT: PASS` for startup/error checks.
- [ ] In `feedback pipeline observation`, confirm the required groups are `OBSERVED`.
  - This section observes paths only. If there was no combat, `NOT OBSERVED` is normal and is not a failure.
  - If `feedback pipeline engine-path rejections` contains an item, the engine rejected the request; recheck that affix in-game.

Expected-chance reference: at 30% proc chance and 0.8-second ICD, 20 effective hits average 6 procs; 2–10 is a normal range. Weak enemies that die in one or two hits are not a useful sample.

## 5. Visual and audio checks (must be observed directly)

- [ ] At the moment of a proc, the effect is **visibly present** on the target or player.
- [ ] The proc sound is **actually audible** and its 3D position sounds natural.
- [ ] After target death or ragdoll transition, the effect ends within its duration and **does not linger**.
- [ ] Simultaneous procs do not feel excessively layered or spammy.
- [ ] For elemental procs, the art matches the effect (for example, fire uses fire art).

## 6. Regression checks (once each)

- [ ] One proc from another group, such as an Owner-buff effect.
- [ ] One corpse explosion.
- [ ] One trap affix: place, trigger, and expire it.
- [ ] Item tooltip: confirm base and effective proc chances for a proc affix.
- [ ] Save, load, and confirm procs work (watch for a regression where procs stop permanently).

### Bear-trap world-marker pilot

- [ ] Use the debug panel to add the first trap affix, `bear_trap`, to a weapon.
- [ ] Immediately after placement, confirm the **vanilla metal bear-trap jaws** are visibly under the enemy instead of a rune.
- [ ] After placement, confirm it opens with `StartOpen` rather than staying in its closed default pose; the first trigger is held for 900 ms after opening.
- [ ] When an enemy steps on it, confirm the logical trap closes it with `Trigger01`. On rearm, confirm `Reset01` opens it again and the next trigger is held for 900 ms.
- [ ] Confirm the behavior graph plays the close and rearm sounds once each, without overlapping explicit sounds.
- [ ] Confirm the marker stays at its placed coordinates when the player moves or turns the camera.
- [ ] Confirm there is no activation prompt or collision with walking, corpses, or projectiles, and no extra damage from vanilla `TrapBear`.
- [ ] Confirm the model disappears after a proc consumes it, its 12-second expiry, cap eviction, or cell exit.
- [ ] While the model is visible, quicksave immediately; confirm the model disappears and the active trap is canceled.
- [ ] Load that save and re-enter the original cell; confirm there is no ghost model, collision, or activation prompt.
- [ ] In the spawn log line, confirm `base=CAFF_MSTT_TRAP_BEAR_VISUAL`, `handleAllocated=true`, `resolved=true`, `strongOwnerRetained=true`, and `spawned=true`.
- [ ] In the quicksave log, confirm `pre-save trap cleanup complete (activeTraps=0, unresolvedDeferred=0)` appears first and the later `serialization save trap state` also reports both values as 0.
- [ ] If `handleAllocated=true, resolved=false` or `unresolvedDeferred` is greater than 0, do not mark this as passing; retain the log and save file for investigation.

Before a formal release, repeat save/load 20 times with an active bear trap and check that save size does not keep growing and generated REFR/change-form records do not accumulate. An automated test or `spawned=true` log cannot replace this check.

### Other five physical markers, ally protection, and regression

Ground visibility for the five models was observed in-game on 2026-08-11. The visibility items below are repeat checks for a future build. Collision, cleanup, ally/neutral protection, and density checks were still unverified in the source checklist.

- [ ] Press `Production Trap World-Ref Probe` once in the debug panel. Confirm the six production markers appear in two rows in front of the player and all disappear after about 3 seconds.
- [ ] Immediately after the probe, run `qa_skse_logcheck.py`. Confirm `trap world marker probe contracts: OBSERVED 6/6` and `OBSERVED` for `trap_world_marker_spawn` and `trap_world_marker_cleanup`. Judge visible appearance separately.
- [ ] Run the probe again and quicksave immediately. Confirm all six markers are cleaned up and `trap_pre_save_state_clean` and `trap_serialization_state_clean` are `OBSERVED`.
- [ ] Use the debug panel to add `rune_trap`, `plague_spore`, `tar_blight`, `siphon_spore`, and `chaos_rune` in sequence; place and trigger each at least once.
- [ ] `rune_trap`: confirm the vanilla stone pressure plate (`TrapStonePressurePlate01.nif`) appears, not a fire-rune glyph.
- [ ] `plague_spore`: confirm the vanilla poison-spider sack (`spidersackdead.nif`) appears, not a poison-rune glyph.
- [ ] `tar_blight`: confirm the vanilla oil puddle (`OilTrapPuddle01.nif`) appears, not an ash-rune glyph.
- [ ] `siphon_spore`: confirm vanilla albino spider eggs (`ExpSpiderEggsAlbino.nif`) appear, not a frenzy-rune glyph.
- [ ] `chaos_rune`: confirm the vanilla metal pressure plate (`TrapPressurePlateMetal01.nif`) appears, not a shock-rune glyph.
- [ ] Confirm all five markers sit naturally on the ground beneath the target (not floating, buried, or tilted) and do not follow player movement or camera rotation.
- [ ] Confirm physical markers have no activation prompts, do not collide with the player, allies, corpses, or projectiles, and do not add vanilla trap-script damage or sounds.
- [ ] Confirm the existing short arming cue and trigger burst/sound each play once, are not obscured by the physical marker, and do not linger after triggering.
- [ ] Confirm each marker is removed after its one proc, natural expiry, per-affix/global cap eviction, cell exit, or settings reset.
- [ ] With an active marker, quicksave. Confirm the trap and marker are canceled; after loading or re-entering the cell, confirm no ghost model, collision, or activation prompt remains.
- [ ] For each spawn log line, confirm the expected `base=CAFF_MSTT_TRAP_*_VISUAL`, `handleAllocated=true`, `resolved=true`, `strongOwnerRetained=true`, and `spawned=true` values.
- [ ] In an A/B with an enemy and an ally/neutral NPC in the same trap radius, confirm at most two hostile actors receive the configured effect and allies/neutrals receive no damage, slow, defense reduction, regeneration reduction, or curse.
- [ ] Confirm the trap MGEF targets in `magic effect apply observed` list only directly selected hostile actors and contain no ally FormID.

Static checks for `spawned=true` and `Effect.Area=0` alone do not pass collision, cleanup, or ally-protection checks. Record the hostile-direct and world-reference lifetime contracts as in-game verified only after the ally-combat A/B and save/load/cell-transition checks are complete.

## 7. Stability scenarios

Before a release candidate, run the separate [stability scenario matrix](QA_STABILITY_SCENARIOS.en.md). An ordinary combat smoke test is not a substitute.
