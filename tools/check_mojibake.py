#!/usr/bin/env python3
"""Detect common mojibake markers in player-visible source text."""

from __future__ import annotations

import re
import sys
from pathlib import Path


SCAN_EXTENSIONS = {".c", ".h", ".txt", ".csv", ".tsv"}

MARKERS = [
    "\u00c3",  # Ã
    "\u00c2",  # Â
    "\ufffd",  # replacement character
    "\u00e6",  # æ
    "\u00e5",  # å
    "\u00e4",  # ä
    "\u00e8",  # è
    "\u00e9",  # é
    "\u00e7",  # ç
    "\u00e3",  # ã
    "\u00ef",  # ï
]

PATTERN = re.compile("|".join(re.escape(marker) for marker in MARKERS))


def iter_files(root: Path):
    for path in root.rglob("*"):
        if path.is_file() and path.suffix.lower() in SCAN_EXTENSIONS:
            yield path


def main() -> int:
    root = Path("src")
    failures = []
    for path in iter_files(root):
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            failures.append((path, 0, f"invalid UTF-8: {exc}"))
            continue
        for line_no, line in enumerate(text.splitlines(), 1):
            if PATTERN.search(line):
                failures.append((path, line_no, line.strip()))

    for path, line_no, snippet in failures:
        if line_no:
            print(f"{path}:{line_no}: suspicious mojibake marker: {snippet}")
        else:
            print(f"{path}: {snippet}")

    if failures:
        print(f"\nFound {len(failures)} suspicious text line(s).")
        return 1
    print("No mojibake markers found in src text files.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
