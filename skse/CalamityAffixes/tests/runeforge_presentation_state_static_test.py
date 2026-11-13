#!/usr/bin/env python3
"""Static contract for presentation-only state routing from native WS to the Papyrus staging host."""
from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True, type=Path, help="CalamityAffixes project root")
    parser.add_argument("--core-root", required=True, type=Path, help="feasr-core source root")
    args = parser.parse_args()
    native = (args.source_root / "src/RuneforgeWorkstation.cpp").read_text(encoding="utf-8")
    protocol = (args.source_root / "include/CalamityAffixes/RuneforgeWorkstationProtocolPolicy.h").read_text(encoding="utf-8")
    stage = (args.core_root / "alpha/workstation/Scripts/Source/FEASR_RF_WorkstationStage.psc").read_text(encoding="utf-8")
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    require('kState = "FEASR_Runeforge_WS_State"' in protocol, "protocol lacks the presentation-state event")
    require('kOperationCue = "FEASR_Runeforge_WS_OperationCue"' in protocol, "protocol lacks operation-specific presentation cue")
    require('Message VisualState(std::uint32_t a_token, std::string_view a_state)' in protocol, "protocol lacks the tokenized state payload helper")
    require('"|" + std::string(a_state)' in protocol, "state payload does not carry its state name")
    dispatch = native[native.find("void Dispatch(const SP::Event& a_event)"):native.find("void Interrupt(SP::Cause a_cause)")]
    require("if (before != after)" in dispatch and "PP::VisualState(visualToken, SP::Name(after))" in dispatch,
            "native only routes state after a state-machine transition")
    require("session.token" in dispatch and "g.session.token" in dispatch, "state event does not preserve session token across exit")
    require("RegisterForModEvent(\"FEASR_Runeforge_WS_State\", \"OnWSState\")" in stage, "Papyrus host is not subscribed")
    state_handler = stage[stage.find("Event OnWSState"):stage.find("EndEvent", stage.find("Event OnWSState"))]
    require("tok != _token" in state_handler and "ApplyVisualState(phase)" in state_handler, "state observer is not session-token guarded")
    require("Function ApplyVisualState(String phase)" in stage, "missing state-to-light presentation mapping")
    for state in ("ENTERING", "SELECT_ITEM", "SELECT_FORMULA", "OPERATIONS", "CONFIRM", "COMMITTING", "PERFORMING", "RESULT_SUCCESS", "RESULT_FAILURE", "EXITING", "IDLE"):
        require(f'phase == "{state}"' in stage, f"state mapping omits {state}")
    require('ObjectReference Property WorkpointBeam Auto' in stage and 'WorkpointBeam.Enable(False)' in stage and 'WorkpointBeam.Disable(False)' in stage,
            "beam proxy is not state-gated and cleanup-disabled")
    beat_handler = stage[stage.find("Event OnWSBeat"):stage.find("EndEvent", stage.find("Event OnWSBeat"))]
    require('verb == "STRIKE"' in beat_handler and 'RegisterForSingleUpdate(StrikeTickSeconds)' in beat_handler and 'CleanSlot(index)' not in beat_handler,
            "a strike does not transfer the active rune through a timed presentation beat")
    require("PlaceLocal(_live[_strikeSlot], moveX, moveY, moveZ, 0.0)" in stage and "progress >= 1.0" in stage,
            "ordered rune transfer has no bounded movement/cleanup endpoint")
    require("Function ClearVisualState()" in stage and "ClearVisualState()" in stage,
            "state cleanup helper/call sites are missing")
    require("ClearVisualState()\n\tSweepIfIdle()" in stage, "load transition does not clear presentation state first")
    require('RegisterForModEvent("FEASR_Runeforge_WS_OperationCue", "OnWSOperationCue")' in stage, "Papyrus host is not subscribed to operation cues")
    for operation in ("IDENTIFY", "REFORGE", "EXPAND", "SCOUR"):
        require(f'operation == "{operation}"' in stage, f"operation presentation omits {operation}")
    require("_operationCueTicks = 14" in stage and "_operationCueTicks -= 1" in stage and "_operationCueTicks <= 0" in stage,
            "operation cue has no bounded timed reset")
    require("_operationCuePending = False" in stage and "ApplyVisualState(_visualState)" in stage,
            "operation cue does not restore the session's normal presentation state")
    require("OnWSEnd" in stage and "OnWSReset" in stage, "exit/reset cleanup handlers are absent")
    if failures:
        for failure in failures:
            print(f"FAIL {failure}")
        return 1
    print("PASS Runeforge presentation state static contract (token routing, state coverage, load/exit cleanup)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
