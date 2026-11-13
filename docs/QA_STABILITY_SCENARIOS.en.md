# Stability Scenario Matrix — English Translation

This translates [QA_STABILITY_SCENARIOS.md](QA_STABILITY_SCENARIOS.md). The source notes that three defects identified in the 2026-07-27 cross-analysis had only static verification (compilation, gates, and sabotage tests); absence of crashes or freezes can be established only in the live game. Ordinary combat does not reliably create the conditions below, so set them up deliberately.

## Common setup

- Set `debugVerboseLogging=true` and `debugHudNotifications=true`.
- **Prepare trap affixes (tens of seconds):** with debug toggles enabled, the Prisma panel's Advanced tab shows the button labeled `선택 베이스에 함정 어픽스`. In the Runeword tab, select a weapon as the base. Each button press adds one trap affix in sequence: bear trap → rune → plague → tar → siphon → chaos. Put two or three types on each of two weapons to satisfy the scenario setup. The debug action labeled `스타트 재련 오브 +3` is also repeatedly granted in a debug session, regardless of current inventory.
- Make a manual save before each scenario so a failure can be reproduced.
- If a crash occurs, keep the last 50 lines of `CalamityAffixes.log` and crash-logger output, if installed.
- If you need to spawn a target, find its BaseID with console command `help "<name>" 4`, then use `player.placeatme <BaseID> <count>`.

Common pass criteria: no CTD or freeze; no `[error] CalamityAffixes:` entry in the log; and procs/traps still work after the scenario (watch for a regression where procs stop permanently).

## S1. Trap cell detach (v1.7.3 P0-2 fix)

**Defect addressed:** A trap retained its owning cell pointer for its TTL. If the cell unloaded first, the trap could later access freed memory and cause a CTD.

**Why ordinary combat is not enough:** A cell rarely unloads while a trap still has time remaining. Force the unload deliberately.

| Step | Procedure |
|---:|---|
| 1 | Equip a trap affix. In combat, place at least three traps and confirm their markers. |
| 2 | While trap TTL remains, **leave the cell immediately**: exit an interior through a door or fast-travel from an exterior. |
| 3 | While the cell is unloaded, wait at least 60 seconds in real time for the TTL to expire. Do not use the console wait command. |
| 4 | Return to the original cell and inspect the trap locations. |
| 5 | Repeat each combination at least once: interior to exterior, exterior to interior, fast travel, and `coc` travel. |

Also confirm no marker or effect remains after returning and that new traps still work.

## S2. Trigger index snapshot (v1.7.3 P1-1 fix)

**Defect addressed:** While iterating a proc list, an effect could re-enter the engine and an equip event could rebuild the same list, invalidating the active iteration and causing a CTD (use-after-free). The fix uses a snapshot from the start of iteration.

**Why ordinary combat is not enough:** Natural play rarely repeats the timing collision between proc handling and an equip rebuild.

| Step | Procedure |
|---:|---|
| 1 | Equip at least four proc affixes (mostly hit-triggered) and at least one incoming-hit trigger. |
| 2 | Fight at least five enemies and keep procs triggering in quick succession. |
| 3 | During combat, rapidly swap weapons/shields with a hotkey; each swap rebuilds equipped state. |
| 4 | For five minutes, repeatedly try to time a swap to a proc HUD notification. |
| 5 | Open Favorites, change equipment, close the menu immediately, and repeat attacks. |

Also confirm procs continue after the session. If they stop, suspect a guard-counter regression. The number of `RebuildActiveCounts` log lines should be consistent with the swaps performed.

## S3. Multiple trap state transitions in one tick (v1.7.3 P1-2 fix)

**Defect addressed:** The nearby-actor scan for trap ticks was consolidated to one scan per tick. The risky path is several traps triggering, being removed, or expiring in the same tick.

**Why ordinary combat is not enough:** To change several traps' states in one tick, place them close together and draw a group of enemies into them.

| Step | Procedure |
|---:|---|
| 1 | Equip two or three trap-affix types at once. |
| 2 | Place at least six traps close together in a narrow space. |
| 3 | Spawn at least eight enemies with `player.placeatme` and draw them into the trap area to encourage simultaneous triggers. |
| 4 | Place another cluster and leave it untriggered so several TTLs expire together. |
| 5 | Repeat steps 2–4 once in a high-actor-count area, such as a city market. |

Also confirm a target killed by an earlier trap remains excluded from later traps (no duplicate-trigger log), and record any noticeable frame hitch.

## Results

| Scenario | Date | Build (commit / first 8 DLL SHA-256 characters) | Result | Notes |
|---|---|---|---|---|
| S1 | | | | |
| S2 | | | | |
| S3 | | | | |

The source checklist says that passing all three scenarios closes the three items marked “not verifiable in-game” in `calamity-p0-risk-backlog`. This is a historical QA procedure, not a statement that these scenarios have passed for FEASR PC1.2 or RC1.
