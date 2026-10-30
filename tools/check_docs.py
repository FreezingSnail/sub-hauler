#!/usr/bin/env python3
"""Check local documentation links and retired build selectors.

The repository keeps current contracts in `docs/` and dated evidence in
`docs/archive/`. This tool verifies that every relative link in tracked
Markdown/HTML points at a file or directory in the checkout. It also rejects
retired profile selectors from active documents; archived records may mention
them because they describe the retired builds.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]

MARKDOWN_LINK = re.compile(r"!?\[[^\]]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
HTML_LINK = re.compile(r"(?:href|src)=\"([^\"]+)\"")
EXTERNAL_PREFIXES = ("http://", "https://", "mailto:", "tel:", "data:", "javascript:")
RETIRED_PATTERNS = (
    re.compile(r"\bPROFILE="),
    re.compile(r"\bmake dev\b"),
    re.compile(r"\bfull/lean\b"),
    re.compile(r"\bSH_FEATURE_[A-Z0-9_]+\b"),
    re.compile(r"\bMUSIC_BACKEND\b"),
    re.compile(r"\bDEV_MODE\b"),
)
ACTIVE_STALE_SCAN = (
    ROOT / "README.md",
    ROOT / "levels" / "README.md",
    ROOT / "docs" / "README.md",
    ROOT / "docs" / "dev-flow.md",
    ROOT / "docs" / "design.md",
    ROOT / "docs" / "story-beats.md",
    ROOT / "docs" / "guide.html",
    ROOT / "docs" / "story-sketchbook.html",
)


def docs_files():
    files = [ROOT / "README.md", ROOT / "AGENTS.md"]
    for pattern in ("*.md", "*.html"):
        files.extend((ROOT / "docs").glob(pattern))
        files.extend((ROOT / "levels").glob(pattern))
        files.extend((ROOT / "music").glob(pattern))
        files.extend((ROOT / "images" / "splash" / "source").glob(pattern))
    files.extend((ROOT / "docs" / "archive").glob("*.md"))
    return sorted(set(path for path in files if path.is_file()))


def extract_links(text: str, is_html: bool):
    pattern = HTML_LINK if is_html else MARKDOWN_LINK
    return [match.group(1) for match in pattern.finditer(text)]


def local_target(source: Path, target: str):
    target = target.strip()
    if (
        not target
        or target.startswith("#")
        or target.startswith(EXTERNAL_PREFIXES)
        or "+" in target
    ):
        return None
    target = unquote(target.split("#", 1)[0].split("?", 1)[0])
    if not target or target.startswith("//"):
        return None
    if target.startswith("/"):
        return ROOT / target.lstrip("/")
    return (source.parent / target).resolve()


def check_links(files):
    failures = []
    for path in files:
        text = path.read_text(encoding="utf-8")
        for target in extract_links(text, path.suffix == ".html"):
            resolved = local_target(path, target)
            if resolved is not None and not resolved.exists():
                failures.append("{}: missing {}".format(path.relative_to(ROOT), target))
    return failures


def check_stale(files):
    failures = []
    for path in files:
        if not path.exists():
            failures.append("{}: active document is missing".format(path.relative_to(ROOT)))
            continue
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            for pattern in RETIRED_PATTERNS:
                if pattern.search(line):
                    failures.append(
                        "{}:{}: retired selector {!r}".format(
                            path.relative_to(ROOT), line_number, pattern.pattern
                        )
                    )
    return failures


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-stale", action="store_true", help="check links only")
    args = parser.parse_args(argv)

    files = docs_files()
    failures = check_links(files)
    if not args.skip_stale:
        failures.extend(check_stale(sorted(set(ACTIVE_STALE_SCAN))))

    if failures:
        print("docs-check: FAIL", file=sys.stderr)
        for failure in failures:
            print("  " + failure, file=sys.stderr)
        return 1
    print("docs-check: {} documents, links and active selectors OK".format(len(files)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
