"""Guard rails for the language-free core migration (L0-L3).

L0  no new Persian/Arabic text outside assets/i18n and the shrink-only allowlist
L1  every translation file has the same keys and placeholders
L2  every dotted key used in Dart exists in the default translation file
L3  Python verification scripts contain no Persian/Arabic text
"""

from __future__ import annotations

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
ALLOWLIST = ROOT / "i18n_leak_allowlist.txt"
I18N_DIR = ROOT / "assets" / "i18n"
APP_LIB = ROOT / "app" / "lib"

TEXT_SUFFIXES = {
    ".arb",
    ".c",
    ".cpp",
    ".dart",
    ".h",
    ".hpp",
    ".json",
    ".kt",
    ".kts",
    ".py",
    ".sh",
    ".txt",
    ".xml",
    ".yaml",
    ".yml",
}

SKIP_DIRS = {
    ".dart_tool",
    ".git",
    ".gradle",
    ".idea",
    ".cxx",
    "__pycache__",
    "build",
    "node_modules",
}

# Persian/Arabic letters, Arabic presentation forms, and ZWNJ.  Persian digits
# (U+06F0-U+06F9) are inside the first range.
FORBIDDEN = re.compile(r"[\u0600-\u06FF\uFB50-\uFDFF\uFE70-\uFEFF\u200C]")
PLACEHOLDER = re.compile(r"\{([A-Za-z_][A-Za-z0-9_]*)\}")
DOTTED_KEY_CALL = re.compile(
    r"(?<![A-Za-z0-9_])t\(\s*'([a-z][a-z0-9_]*(?:\.[a-z0-9_]+)+)'"
)


def _relative(path: pathlib.Path) -> str:
    return path.relative_to(ROOT).as_posix()


def _is_allowed(relative: str) -> bool:
    # Documentation is not program logic, and translation data lives here.
    if relative.endswith(".md"):
        return True
    if relative.startswith("assets/i18n/"):
        return True
    return "_i18n_" in pathlib.PurePosixPath(relative).name


def _scan_text_files():
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or path.suffix not in TEXT_SUFFIXES:
            continue
        relative = _relative(path)
        if set(pathlib.PurePosixPath(relative).parts) & SKIP_DIRS:
            continue
        if _is_allowed(relative):
            continue
        yield relative, path


def _offending_lines(path: pathlib.Path):
    text = path.read_text(encoding="utf-8", errors="replace")
    for number, line in enumerate(text.splitlines(), start=1):
        if FORBIDDEN.search(line):
            yield number, line


def _read_allowlist() -> dict[str, int]:
    allowed: dict[str, int] = {}
    if not ALLOWLIST.exists():
        return allowed
    for raw in ALLOWLIST.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        path, _, count = line.rpartition("\t")
        if not path:
            raise AssertionError(f"malformed allowlist entry: {raw!r}")
        allowed[path] = int(count)
    return allowed


def test_l0_no_new_language_leaks():
    actual: dict[str, int] = {}
    samples: dict[str, list[str]] = {}
    for relative, path in _scan_text_files():
        lines = list(_offending_lines(path))
        if not lines:
            continue
        actual[relative] = len(lines)
        samples[relative] = [f"{relative}:{n}: {text.strip()}" for n, text in lines[:3]]

    allowed = _read_allowlist()

    grown = {
        path: (count, allowed.get(path, 0))
        for path, count in actual.items()
        if count > allowed.get(path, 0)
    }
    stale = {
        path: count
        for path, count in allowed.items()
        if actual.get(path, 0) != count
    }

    problems: list[str] = []
    for path in sorted(grown):
        count, cap = grown[path]
        problems.append(
            f"{path}: {count} language line(s), allowlist has {cap}\n  "
            + "\n  ".join(samples.get(path, []))
        )
    for path in sorted(stale):
        problems.append(
            f"{path}: allowlist has {stale[path]}, file has {actual.get(path, 0)}"
            " (shrink the allowlist in the same commit)"
        )
    assert not problems, "i18n language leak:\n" + "\n".join(problems)


def test_l1_translation_keys_match():
    files = sorted(I18N_DIR.glob("*.json")) if I18N_DIR.is_dir() else []
    if not files:
        return  # Phase 2 creates the catalogue.

    catalogues = {
        path.stem: json.loads(path.read_text(encoding="utf-8")) for path in files
    }
    if "fa" not in catalogues:
        raise AssertionError("assets/i18n must contain fa.json as the default")

    default_keys = set(catalogues["fa"])
    problems: list[str] = []
    for language, catalogue in catalogues.items():
        missing = sorted(default_keys - set(catalogue))
        extra = sorted(set(catalogue) - default_keys)
        if missing:
            problems.append(f"{language}: missing keys {missing}")
        if extra:
            problems.append(f"{language}: unexpected keys {extra}")
        for key in sorted(default_keys & set(catalogue)):
            expected = set(PLACEHOLDER.findall(catalogues["fa"][key]))
            actual = set(PLACEHOLDER.findall(catalogue[key]))
            if expected != actual:
                problems.append(
                    f"{language}:{key}: placeholders {sorted(actual)}"
                    f" != fa {sorted(expected)}"
                )
    assert not problems, "translation key mismatch:\n" + "\n".join(problems)


def test_l2_used_keys_exist_and_report_unused():
    if not (I18N_DIR / "fa.json").is_file():
        return  # Phase 2 creates the catalogue.

    default_keys = set(json.loads((I18N_DIR / "fa.json").read_text(encoding="utf-8")))
    used: set[str] = set()
    for path in APP_LIB.rglob("*.dart"):
        used.update(DOTTED_KEY_CALL.findall(path.read_text(encoding="utf-8")))

    missing = sorted(used - default_keys)
    assert not missing, f"keys used in Dart but missing from fa.json: {missing}"

    unused = sorted(default_keys - used)
    if unused:
        print(
            f"warning: {len(unused)} unused translation keys, first 20: {unused[:20]}",
            file=sys.stderr,
        )


def test_l3_python_scripts_are_language_free():
    offenders: list[str] = []
    for relative, path in _scan_text_files():
        if path.suffix != ".py":
            continue
        for number, line in _offending_lines(path):
            offenders.append(f"{relative}:{number}: {line.strip()}")
    assert not offenders, "Persian/Arabic text in Python scripts:\n" + "\n".join(offenders)
