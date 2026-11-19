"""The release version carousel in scripts/bump_version.py.

Each part after the major counts 0 to 9 and carries, so the sequence is
1.0.9 -> 1.1.0 and 1.9.9 -> 2.0.0, and the build number never goes backwards.
"""

from __future__ import annotations

import importlib.util
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "bump_version.py"

spec = importlib.util.spec_from_file_location("bump_version", SCRIPT)
bump_version = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(bump_version)


def walk(start, steps):
    major, minor, patch = start
    seen = []
    for _ in range(steps):
        major, minor, patch = bump_version.bump(major, minor, patch)
        seen.append((major, minor, patch))
    return seen


def test_patch_counts_up_to_nine():
    assert walk((1, 0, 0), 3) == [(1, 0, 1), (1, 0, 2), (1, 0, 3)]


def test_patch_carries_into_minor():
    assert walk((1, 0, 8), 2) == [(1, 0, 9), (1, 1, 0)]


def test_minor_carries_into_major():
    assert walk((1, 9, 8), 2) == [(1, 9, 9), (2, 0, 0)]


def test_sequence_matches_the_agreed_order():
    seen = walk((1, 0, 0), 11)
    assert [".".join(map(str, v)) for v in seen] == [
        "1.0.1",
        "1.0.2",
        "1.0.3",
        "1.0.4",
        "1.0.5",
        "1.0.6",
        "1.0.7",
        "1.0.8",
        "1.0.9",
        "1.1.0",
        "1.1.1",
    ]


def test_build_number_grows_with_every_release():
    numbers = [bump_version.build_number(*version) for version in walk((1, 0, 0), 100)]
    assert numbers == sorted(numbers)
    assert len(set(numbers)) == len(numbers)


def test_pubspec_is_readable_and_in_the_expected_shape():
    text = (ROOT / "app" / "pubspec.yaml").read_text(encoding="utf-8")
    major, minor, patch, build = bump_version.read_version(text)
    assert 0 <= minor <= 9
    assert 0 <= patch <= 9
    assert build > 0
