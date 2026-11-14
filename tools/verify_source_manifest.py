#!/usr/bin/env python3
"""Validate the FEASR source manifest against this checkout and its ancestry."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "SOURCE-MANIFEST.json"
EXPECTED_BASELINE = "989f215e81f3595dc4526a1311437946646a998a"


def fail(message: str) -> None:
    print(f"source manifest: ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def git(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args], cwd=ROOT, text=True, capture_output=True, check=False
    )


def main() -> None:
    try:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"cannot read {MANIFEST.name}: {exc}")

    if manifest.get("schema") != "feasr.calamity-source-manifest.v2":
        fail("unsupported schema")
    upstream = manifest.get("upstream", {})
    if upstream.get("baseline_commit") != EXPECTED_BASELINE:
        fail("upstream baseline is not the pinned Calamity 2.2.8 commit")
    if upstream.get("baseline_version") != "2.2.8":
        fail("upstream baseline version is not 2.2.8")
    if upstream.get("newer_upstream_commits_merged") is not False:
        fail("manifest must affirm that newer upstream commits were not merged")

    for commit in (EXPECTED_BASELINE, manifest.get("feasr_source", {}).get("source_revision_commit")):
        if not commit or git("cat-file", "-e", f"{commit}^{{commit}}").returncode:
            fail(f"required source commit is unavailable: {commit}")
        if git("merge-base", "--is-ancestor", commit, "HEAD").returncode:
            fail(f"required source commit is not an ancestor of HEAD: {commit}")

    entries = manifest.get("changed_files")
    if not isinstance(entries, list) or not entries:
        fail("changed_files must be a non-empty array")
    seen: set[str] = set()
    errors: list[str] = []
    for entry in entries:
        relative = entry.get("path")
        if not isinstance(relative, str) or relative in seen:
            errors.append(f"invalid or duplicate path: {relative!r}")
            continue
        seen.add(relative)
        path = (ROOT / relative).resolve()
        if ROOT not in path.parents:
            errors.append(f"path escapes checkout: {relative}")
            continue
        try:
            # The manifest records canonical LF bytes. Normalize Windows
            # checkouts so the result is independent of core.autocrlf.
            data = (ROOT / relative).read_bytes().replace(b"\r\n", b"\n")
        except OSError as exc:
            errors.append(f"{relative}: {exc}")
            continue
        expected_size = entry.get("bytes")
        expected_hash = entry.get("sha256")
        if len(data) != expected_size:
            errors.append(f"{relative}: byte count {len(data)} != {expected_size}")
        digest = hashlib.sha256(data).hexdigest()
        if digest != expected_hash:
            errors.append(f"{relative}: SHA-256 mismatch")

    if errors:
        for error in errors:
            print(f"source manifest: ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)

    print(
        f"source manifest: OK; {len(entries)} files match, "
        f"baseline {EXPECTED_BASELINE[:12]} is preserved"
    )


if __name__ == "__main__":
    main()
