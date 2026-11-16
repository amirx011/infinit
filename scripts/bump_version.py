#!/usr/bin/env python3
"""Move app/pubspec.yaml to the next release version.

The version line looks like ``MAJOR.MINOR.PATCH+BUILD``.  Both parts after the
major count 0 to 9 and carry, so a release sequence runs

    1.0.9 -> 1.1.0 -> 1.1.9 -> 1.2.0 ... 1.9.9 -> 2.0.0 -> 2.0.1

The build number mirrors the same three fields (major * 100 + minor * 10 +
patch), so Android always receives a larger versionCode than the release before.

    python3 scripts/bump_version.py            # write the next version
    python3 scripts/bump_version.py --dry-run  # print it, change nothing
    python3 scripts/bump_version.py --show     # print the current one
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
PUBSPEC = ROOT / "app" / "pubspec.yaml"

VERSION_RE = re.compile(r"^version:\s*(\d+)\.(\d+)\.(\d+)\+(\d+)\s*$", re.MULTILINE)


def read_version(text: str) -> tuple[int, int, int, int]:
    match = VERSION_RE.search(text)
    if not match:
        raise SystemExit(
            "app/pubspec.yaml has no 'version: MAJOR.MINOR.PATCH+BUILD' line."
        )
    return tuple(int(part) for part in match.groups())  # type: ignore[return-value]


def bump(major: int, minor: int, patch: int) -> tuple[int, int, int]:
    patch += 1
    if patch > 9:
        patch = 0
        minor += 1
    if minor > 9:
        minor = 0
        major += 1
    return major, minor, patch


def build_number(major: int, minor: int, patch: int) -> int:
    return major * 100 + minor * 10 + patch


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true", help="print, do not write")
    parser.add_argument("--show", action="store_true", help="print the current version")
    args = parser.parse_args(argv[1:])

    text = PUBSPEC.read_text(encoding="utf-8")
    major, minor, patch, _build = read_version(text)
    if args.show:
        print(f"{major}.{minor}.{patch}")
        return 0

    new_major, new_minor, new_patch = bump(major, minor, patch)
    code = build_number(new_major, new_minor, new_patch)
    version = f"{new_major}.{new_minor}.{new_patch}"

    if not args.dry_run:
        updated = VERSION_RE.sub(f"version: {version}+{code}", text, count=1)
        PUBSPEC.write_text(updated, encoding="utf-8")

    print(version)
    if args.dry_run:
        print(f"(dry run, build number would be {code})", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
