#!/usr/bin/env python3
"""Static wiring regression for Runeforge workstation physical Back input."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    args = parser.parse_args()
    source_root = args.source_root.resolve()
    native = (source_root / "src/RuneforgeWorkstation.cpp").read_text(encoding="utf-8")
    policy = (source_root / "include/CalamityAffixes/RuneforgeWorkstationInputPolicy.h").read_text(encoding="utf-8")

    require(
        "ButtonForInputEvent(button->QUserEvent().c_str(), inputDevice, button->GetIDCode())" in native,
        "native input sink must pass user event, device and raw ID to the shared classifier",
    )
    require("kKeyboardEscapeCode" in policy and "kKeyboardTabCode" in policy,
            "policy must classify physical Escape and Tab")
    require("kGamepadBCode" in policy and "kGamepadBackCode" in policy,
            "policy must classify physical B and preserve View/Back identity")
    require('command == "back"' in native and "Command(state == SP::State::kPerforming ? IP::Command::kSkip : IP::Command::kBack)" in native,
            "mouse ESC callback must continue to route through the shared Back command")
    require("RightButton(button->IsDown(), button->IsUp())" in native,
            "mouse right-button routing must remain separate from Back")
    require("MayConsumeWorkstationInput(" in native and "g_uiYield.load(std::memory_order_acquire)" in native,
            "foreign-menu input arbitration must remain ahead of workstation input consumption")
    print("runeforge_back_input_static: OK")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"runeforge_back_input_static: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
