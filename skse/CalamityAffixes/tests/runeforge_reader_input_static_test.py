#!/usr/bin/env python3
"""Static wiring regression for the Runeforge inscription reader input/exit contract (R8.7.4.14, after live U1).

The live failure: every reader action went through the SKSE task queue and the view was focused with pauseGame=true.
These checks keep the native paths that no longer depend on either.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path
import sys


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[brace:i + 1]
    raise AssertionError(f"unterminated body: {signature}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    args = parser.parse_args()
    root = args.source_root.resolve()
    src = root / "src"
    lifecycle = (src / "PrismaTooltip.ViewLifecycle.inl").read_text(encoding="utf-8")
    menu_input = (src / "PrismaTooltip.MenuInput.inl").read_text(encoding="utf-8")
    ui_command = (src / "PrismaTooltip.HandleUiCommand.inl").read_text(encoding="utf-8")
    tooltip = (src / "PrismaTooltip.cpp").read_text(encoding="utf-8")
    reader_js = (root / "../../Data/PrismaUI/views/CalamityAffixes/scripts/reader.js").resolve().read_text(encoding="utf-8")

    # 1. Focus never pauses the game, and focus is requested only through the contract helper.
    focus_calls = re.findall(r"g_api->Focus\(g_view,[^)]*\)", lifecycle)
    require(all("true" not in call.split(",", 1)[1] or "kFocus" in call for call in focus_calls),
            f"a reader/panel Focus call passes a literal pauseGame=true: {focus_calls}")
    request = body(lifecycle, "void RequestRuneforgeReaderFocus()")
    require("RuneforgeReaderContract::kFocusPausesGame" in request, "reader focus must use the contract's pauseGame flag")
    tick = body(lifecycle, "void TickRuneforgeReader()")
    require("g_api->Focus(" not in tick, "TickRuneforgeReader must request focus via RequestRuneforgeReaderFocus only")

    # 2. JS reader commands bypass the task queue: the listener posts them to the mailbox before any QueueTask.
    listener = body(lifecycle, "void RegisterPrismaCommandListener()")
    require(listener.index("IsReaderCommand(command)") < listener.index("QueueTask("),
            "reader commands must be diverted to the mailbox before the generic QueueTask")
    post = body(lifecycle, "void PostRuneforgeReaderJsCommand(std::string_view a_command)")
    require("g_runeforgeReaderMutex" not in post and "g_api->" not in post,
            "the JS listener path must not take the reader mutex or call Prisma (deadlock-free mailbox only)")
    require("kMaxPendingActions" in post, "the mailbox must be bounded")

    # 3. The input-device sink services the reader before the panel-hotkey early return, every dispatch.
    sink = body(menu_input, "RE::BSEventNotifyControl ProcessEvent(\n\t\t\tconst Event* a_event,")
    service_at = sink.index("ServiceRuneforgeReader();")
    hotkey_at = sink.index("ResolveConfiguredControlPanelHotkey()")
    require(service_at < hotkey_at, "reader service must run before the configured-hotkey early return")
    require(sink.index("HandleRuneforgeReaderInputBatch(inputEvent)") < hotkey_at,
            "reader keyboard/gamepad handling must run before the configured-hotkey early return")
    batch = body(menu_input, "bool HandleRuneforgeReaderInputBatch(const RE::InputEvent* a_events)")
    require("QueueTask" not in batch and "StepRuneforgeReader" not in batch and "CloseRuneforgeReader()" not in batch,
            "native reader input must apply actions directly, not through the task queue")
    require("INPUT_DEVICES::kKeyboard" in batch and "MapKeyboard(" in batch, "keyboard must be handled natively")
    require("INPUT_DEVICES::kGamepad" in batch and "MapGamepad(" in batch, "gamepad must be handled natively by raw code")

    # 4. Exit contract: focus loss / never-acquired focus / focus menu closing all close the reader.
    service = body(lifecycle, "void ServiceRuneforgeReader()")
    require("std::try_to_lock" in service, "service must never block the input thread")
    for decision in ("kCloseLost", "kCloseUnavailable"):
        require(f"FocusDecision::{decision}" in service, f"service must act on {decision}")
    require("ShouldCloseOnMenuClose(" in menu_input and "Source::kFocusMenuClosed" in menu_input,
            "the menu sink must close the reader when the Prisma focus menu closes")
    sync = body(menu_input, "void SyncViewStateAfterRelevantMenusClosed(int a_openCount)")
    require("g_runeforgeReaderOpen" in sync, "relevant-menu close must not unfocus an open reader")
    close = body(lifecycle, "void CloseRuneforgeReaderOnGameThread(RuneforgeReaderContract::Source a_reason)")
    require("g_api->Unfocus(g_view)" in close and "Runeforge reader closed reason=" in close,
            "close must release focus and log its reason")

    # 5. Every route shares one action path; telemetry exists for each hop.
    require("PostRuneforgeReaderJsCommand(a_command)" in ui_command, "HandleUiCommand must route reader commands to the mailbox")
    public_step = body(tooltip, "void StepRuneforgeReader(int a_delta)")
    public_close = body(tooltip, "void CloseRuneforgeReader()")
    require("PostRuneforgeReaderJsCommand" in public_step and "PostRuneforgeReaderJsCommand" in public_close,
            "public step/close must use the mailbox")
    for needle in ("Runeforge reader input source=", "Runeforge reader focus confirmed", "Runeforge reader JS route confirmed",
                   "no JS ack 3 s after the document push", "focus not acquired after"):
        require(needle in lifecycle, f"missing reader telemetry: {needle}")

    # 6. View: ack on document, local paging, no JS arrow handling (native owns keys), Escape fallback.
    require('sendRuneforgeReaderCommand("reader.ack")' in reader_js, "reader.js must acknowledge the document")
    require("stepRuneforgeReaderLocally" in reader_js, "reader.js must page locally on button clicks")
    require("ArrowRight" not in reader_js and "ArrowLeft" not in reader_js and "PageDown" not in reader_js,
            "reader.js must not handle page keys (native does; double handling skips pages)")
    require('event.key === "Escape"' in reader_js, "reader.js keeps Escape as an idempotent close fallback")

    # R8.7.4.15 mouse: the panel script sets body pointer-events:none whenever the control panel is closed, and the reader is
    # always shown with the panel closed, so the reader root must opt back in (CSS and on every show) or clicks fall through it.
    view = (root / "../../Data/PrismaUI/views/CalamityAffixes").resolve()
    reader_css = (view / "styles/inscription-reader.css").read_text(encoding="utf-8")
    interop_js = (view / "scripts/interop.js").read_text(encoding="utf-8")
    if 'document.body.style.pointerEvents = controlPanelOpen ? "auto" : "none"' in interop_js:
        rule = re.search(r"\.rfReader\s*\{([^}]*)\}", reader_css)
        require(rule is not None and re.search(r"pointer-events\s*:\s*auto", rule.group(1)) is not None,
                "inscription-reader.css .rfReader must set pointer-events: auto (body is pointer-events:none with the panel closed)")
        require("claimRuneforgeReaderPointer();" in body(reader_js, "function setRuneforgeReader(raw)") and
                "claimRuneforgeReaderPointer()" in body(reader_js, "function setRuneforgeReaderVisible(raw)"),
                "reader.js must reclaim pointer events on every document push and show")
    # pointer diagnostic: evidence only (bounded in JS and native; never an action)
    require("reader.pointer:" in reader_js and "RF_READER_MAX_POINTER_REPORTS" in reader_js, "reader.js posts a bounded pointer diagnostic")
    post = body(lifecycle, "void PostRuneforgeReaderJsCommand(std::string_view a_command)")
    require(post.find("IsPointerReport") >= 0 and post.find("IsPointerReport") < post.find("kJsAck"),
            "native handles the pointer diagnostic first, before any action or ack")
    require("kMaxPointerReports" in post and "SanitizePointerReport" in post, "native pointer log is bounded and sanitized")

    print("runeforge_reader_input_static: OK")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, ValueError) as error:
        print(f"runeforge_reader_input_static: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
