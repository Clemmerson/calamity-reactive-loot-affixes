#!/usr/bin/env python3
"""Check local links in the public English FEASR documentation."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[1]
DOCS = (
    "README.md",
    "BUILDING.md",
    "CHANGELOG-FEASR.md",
    "COMPATIBILITY.md",
    "FEASR-CHANGE-INVENTORY.md",
    "SUPPORT.md",
    "UPSTREAM.md",
    "docs/INDEX.md",
    "docs/AFFIX_CATALOG.en.md",
    "docs/LANGUAGE-AND-TRANSLATION.md",
    "docs/QA_SMOKE_CHECKLIST.en.md",
    "docs/QA_STABILITY_SCENARIOS.en.md",
)
LINK = re.compile(r"(?<!!)\[[^\]]*\]\((?:<([^>]+)>|([^\s)]+))(?:\s+[^)]*)?\)")


def main() -> None:
    missing: list[str] = []
    checked = 0
    for relative in DOCS:
        source = ROOT / relative
        if not source.is_file():
            missing.append(f"{relative}: public documentation file is missing")
            continue
        text = source.read_text(encoding="utf-8")
        for match in LINK.finditer(text):
            target = match.group(1) or match.group(2)
            parsed = urlsplit(target)
            if parsed.scheme or target.startswith("//") or not parsed.path:
                continue
            checked += 1
            path = Path(unquote(parsed.path))
            resolved = (ROOT / path) if path.is_absolute() else (source.parent / path)
            if not resolved.exists():
                line = text.count("\n", 0, match.start()) + 1
                missing.append(f"{relative}:{line}: broken local link {target}")

    if missing:
        for item in missing:
            print(f"public docs: ERROR: {item}", file=sys.stderr)
        raise SystemExit(1)
    print(f"public docs: OK; {len(DOCS)} files and {checked} local links checked")


if __name__ == "__main__":
    main()
