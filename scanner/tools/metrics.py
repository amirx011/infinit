#!/usr/bin/env python3
"""Per-stage metrics for the classical math scanner over a generated dataset.

Reads the `manifest.json` written by `make_dataset.py`, pushes every image
through the scanner and reports what each stage did:

  preprocess      foreground F1 of the binarized page against the reference
                  render (the same page with only the geometric steps applied,
                  so blur/noise/paper/lines are the only difference)
  segmentation    ground-truth symbols vs detected symbols, matched by IoU
  classification  top-1 and top-3 accuracy on the matched symbols, per class,
                  plus a confusion matrix
  end-to-end      exact expression match, parser acceptance, symbols that
                  needed repair
  resources       per-stage latency from the scanner's own `timing_ms`, plus
                  the wall clock this script measured

Two entry points are used:
  * `libodecore.so` through ctypes (the same way `python/tests/*` do) for the
    reading JSON: it carries `timing_ms`, `parse_valid`, the repair overrides
    and the classifier's top-3 alternatives, none of which the debug CLI prints;
  * `scanner/build/scan_debug` for the per-stage PNGs (`05-binary.png`), which
    is the only way to see the binarized page without touching the C++.
  The parser is injected as the validator (`ode_solve` in `parse` mode), so
  `parse_valid` is a real verdict from the core parser, not a guess.

Readings run in a child process: the injected parser can crash the process on a
malformed repaired candidate, and one crash must not lose the whole report, so
the image is recorded as failed and the run continues.

Ground-truth boxes live in the coordinates of the generated image; the scanner
works on a downscaled, deskewed copy. Boxes are mapped with the exact transform
the preprocessor applies (uniform downscale to the long side, then a rotation
about the working centre by `skewDegrees`), not by fitting.

Usage:
  ~/.venvs/scanner/bin/python scanner/tools/metrics.py
  python3 scanner/tools/metrics.py --manifest scanner/dataset/manifest.json
  python3 scanner/tools/metrics.py --limit 5 --iou 0.4 --no-preprocess
  python3 scanner/tools/metrics.py --out-dir /tmp/report

Output: a compact report on stdout, plus `metrics.json` and `metrics.md` in
`--out-dir` (default: the dataset directory).
"""

from __future__ import annotations

import argparse
import ctypes
import json
import math
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from PIL import Image

TOOLS = Path(__file__).resolve().parent
REPO = TOOLS.parent.parent
DEFAULT_MANIFEST = REPO / "scanner" / "dataset" / "manifest.json"
DEFAULT_LIB = REPO / "core" / "build" / "lib" / "libodecore.so"
DEFAULT_BANK = REPO / "scanner" / "data" / "glyph_templates.bin"
DEFAULT_DEBUG = REPO / "scanner" / "build" / "scan_debug"

STAGES = ("preprocess", "segment", "classify", "structure", "repair")


# ---------------------------------------------------------------------------
# Scanner access
# ---------------------------------------------------------------------------


class Scanner:
    """The shipped C ABI, loaded the way the app loads it."""

    def __init__(self, lib_path: Path, bank_path: Path, use_validator: bool = True):
        if not lib_path.exists():
            raise SystemExit(f"native library not found: {lib_path} (build the core first)")
        if not bank_path.exists():
            raise SystemExit(f"template bank not found: {bank_path}")
        self.lib = ctypes.CDLL(str(lib_path))
        self.lib.scanner_read.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        self.lib.scanner_read.restype = ctypes.c_void_p
        self.lib.scanner_free.argtypes = [ctypes.c_void_p]
        self.lib.scanner_load_templates.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        self.lib.scanner_load_templates.restype = ctypes.c_int
        self.lib.ode_solve.argtypes = [ctypes.c_char_p]
        self.lib.ode_solve.restype = ctypes.c_void_p
        self.lib.ode_free.argtypes = [ctypes.c_void_p]

        bank = bank_path.read_bytes()
        if not self.lib.scanner_load_templates(bank, len(bank)):
            raise SystemExit(f"the scanner rejected the template bank: {bank_path}")
        self.bank_labels = _bank_labels(bank)

        self._validator = None
        self._cache: dict[str, bool] = {}
        if use_validator:
            native_validator = ctypes.CFUNCTYPE(
                ctypes.c_int, ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p)
            )
            outer = self

            def validate(expression, _error):
                try:
                    text = expression.decode("utf-8") if expression else ""
                except UnicodeDecodeError:
                    return 0
                if text in outer._cache:
                    return 1 if outer._cache[text] else 0
                try:
                    verdict = outer._parses(text)
                except Exception:  # never let a callback kill the scan
                    verdict = False
                if len(outer._cache) > 4096:
                    outer._cache.clear()
                outer._cache[text] = verdict
                return 1 if verdict else 0

            self._validator = native_validator(validate)
            self.lib.scanner_set_validator(self._validator)

    def _parses(self, text: str) -> bool:
        request = json.dumps({"mode": "parse", "input": text, "x": "x", "lang": "en"})
        pointer = self.lib.ode_solve(request.encode())
        try:
            if not pointer:
                return False
            payload = json.loads(ctypes.string_at(pointer).decode("utf-8"))
        finally:
            if pointer:
                self.lib.ode_free(pointer)
        return bool(payload.get("ok"))

    def read(self, data: bytes) -> dict:
        pointer = self.lib.scanner_read(data, len(data))
        if not pointer:
            return {"ok": False, "error": "null reading"}
        try:
            return json.loads(ctypes.string_at(pointer).decode("utf-8"))
        finally:
            self.lib.scanner_free(pointer)


def _bank_labels(raw: bytes) -> set[str]:
    if raw[:4] != b"IGT1":
        return set()
    count = int.from_bytes(raw[4:8], "little")
    labels: set[str] = set()
    offset = 8
    for _ in range(count):
        length = raw[offset]
        offset += 1
        labels.add(raw[offset : offset + length].decode("utf-8"))
        offset += length
        width = raw[offset + 1]
        height = raw[offset + 2]
        offset += 3 + width * height
    return labels


# ---------------------------------------------------------------------------
# Isolation
# ---------------------------------------------------------------------------


class ScanWorker:
    """Runs readings in a child process so a native crash is contained.

    `ode_solve` segfaults on some repaired candidates (for example
    `sqrt()2+9)=)-1`), and the repair stage asks the injected parser about every
    candidate it invents. Without isolation one bad image would kill the whole
    report; here the worker dies, that image is recorded as a crash and the next
    image gets a fresh worker.
    """

    def __init__(self, options):
        self.options = options
        self.process = None

    def _command(self):
        command = [
            sys.executable,
            str(Path(__file__).resolve()),
            "--worker",
            "--lib",
            str(self.options.lib),
            "--bank",
            str(self.options.bank),
        ]
        if self.options.no_validator:
            command.append("--no-validator")
        return command

    def _spawn(self):
        self.process = subprocess.Popen(
            self._command(),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )

    def read(self, path: Path):
        """Return (reading, milliseconds) or (None, reason) on a crash."""
        if self.process is None or self.process.poll() is not None:
            self._spawn()
        request = json.dumps({"path": str(path)}) + "\n"
        try:
            self.process.stdin.write(request)
            self.process.stdin.flush()
        except (BrokenPipeError, ValueError):
            self._spawn()
            self.process.stdin.write(request)
            self.process.stdin.flush()
        started = time.perf_counter()
        ready, _, _ = _select([self.process.stdout], [], [], self.options.timeout)
        if not ready:
            self._kill()
            return None, f"scan timed out after {self.options.timeout:.0f} s"
        line = self.process.stdout.readline()
        elapsed = (time.perf_counter() - started) * 1000.0
        if not line:
            code = self.process.poll()
            self._kill()
            return None, f"scanner process died (exit {code}); native crash"
        try:
            payload = json.loads(line)
        except json.JSONDecodeError:
            self._kill()
            return None, "scanner process wrote an unreadable reply"
        return payload.get("reading"), payload.get("scan_ms", elapsed)

    def _kill(self):
        if self.process is not None:
            try:
                self.process.kill()
                self.process.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):
                pass
            self.process = None

    def close(self):
        if self.process is not None:
            try:
                self.process.stdin.close()
                self.process.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired, ValueError):
                self._kill()
            self.process = None


def _select(streams, _write, _error, timeout):
    import select

    return select.select(streams, _write, _error, timeout)


def run_worker() -> int:
    """Child mode: one JSON reading per line of stdin."""
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--lib", required=True)
    parser.add_argument("--bank", required=True)
    parser.add_argument("--no-validator", action="store_true")
    options, _unknown = parser.parse_known_args()
    scanner = Scanner(Path(options.lib), Path(options.bank), not options.no_validator)
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        request = json.loads(line)
        started = time.perf_counter()
        reading = scanner.read(Path(request["path"]).read_bytes())
        elapsed = (time.perf_counter() - started) * 1000.0
        sys.stdout.write(json.dumps({"reading": reading, "scan_ms": elapsed}) + "\n")
        sys.stdout.flush()
    return 0


# ---------------------------------------------------------------------------
# Geometry: ground truth -> working coordinates
# ---------------------------------------------------------------------------


def map_box(box, image_size, work_size, skew_degrees):
    """Ground truth in image pixels -> the scanner's working image pixels.

    `preprocess` downscales the long side to its working limit (a uniform scale
    about the origin) and then rotates about the centre of the *working* image
    by `skewDegrees`. Both steps are reproduced exactly here.
    """
    scale = max(work_size) / max(image_size)
    x0, y0, x1, y1 = box
    points = [(x0 * scale, y0 * scale), (x1 * scale, y0 * scale),
              (x1 * scale, y1 * scale), (x0 * scale, y1 * scale)]
    if abs(skew_degrees) >= 0.01:
        radians = math.radians(skew_degrees)
        ca, sa = math.cos(radians), math.sin(radians)
        cx = (work_size[0] - 1) / 2.0
        cy = (work_size[1] - 1) / 2.0
        rotated = []
        for x, y in points:
            dx, dy = x - cx, y - cy
            rotated.append((cx + dx * ca + dy * sa, cy - dx * sa + dy * ca))
        points = rotated
    xs = [point[0] for point in points]
    ys = [point[1] for point in points]
    return [min(xs), min(ys), max(xs), max(ys)]


def iou(a, b) -> float:
    x0 = max(a[0], b[0])
    y0 = max(a[1], b[1])
    x1 = min(a[2], b[2])
    y1 = min(a[3], b[3])
    if x1 <= x0 or y1 <= y0:
        return 0.0
    intersection = (x1 - x0) * (y1 - y0)
    area_a = max(0.0, (a[2] - a[0])) * max(0.0, (a[3] - a[1]))
    area_b = max(0.0, (b[2] - b[0])) * max(0.0, (b[3] - b[1]))
    union = area_a + area_b - intersection
    return intersection / union if union > 0 else 0.0


def match_symbols(ground_truth, predicted, threshold):
    """Greedy one-to-one matching by IoU, best pairs first."""
    pairs = []
    for gt_index, truth in enumerate(ground_truth):
        for pred_index, pred in enumerate(predicted):
            score = iou(truth["box"], pred["box"])
            if score >= threshold:
                pairs.append((score, gt_index, pred_index))
    pairs.sort(reverse=True)
    used_gt, used_pred = set(), set()
    matches = []
    for score, gt_index, pred_index in pairs:
        if gt_index in used_gt or pred_index in used_pred:
            continue
        used_gt.add(gt_index)
        used_pred.add(pred_index)
        matches.append((ground_truth[gt_index], predicted[pred_index], score))
    missed = [truth for index, truth in enumerate(ground_truth) if index not in used_gt]
    extra = [pred for index, pred in enumerate(predicted) if index not in used_pred]
    return matches, missed, extra


# ---------------------------------------------------------------------------
# Preprocess: foreground F1
# ---------------------------------------------------------------------------


def foreground_f1(ref_path: Path, stage_dir: Path, work_size, skew_degrees):
    """Ink overlap between the reference render and the scanner's binary page."""
    binary_path = stage_dir / "05-binary.png"
    ref_path = Path(ref_path)
    if not binary_path.exists() or not ref_path.exists():
        return None
    predicted = np.asarray(Image.open(binary_path).convert("L")) > 127
    reference = Image.open(ref_path).convert("L")
    if reference.size != tuple(work_size):
        reference = reference.resize(tuple(work_size), Image.BILINEAR)
    if abs(skew_degrees) >= 0.01:
        # Image.transform() reads the source at the mapped position, so the
        # reference is warped with the inverse of the scanner's rotation.
        radians = math.radians(-skew_degrees)
        ca, sa = math.cos(radians), math.sin(radians)
        cx = (work_size[0] - 1) / 2.0
        cy = (work_size[1] - 1) / 2.0
        coefficients = (
            ca,
            sa,
            cx - ca * cx - sa * cy,
            -sa,
            ca,
            cy + sa * cx - ca * cy,
        )
        reference = reference.transform(
            (work_size[0], work_size[1]),
            Image.AFFINE,
            coefficients,
            resample=Image.BILINEAR,
            fillcolor=255,
        )
    reference_pixels = np.asarray(reference) < 160
    if predicted.shape != reference_pixels.shape:
        return None
    true_positive = int((predicted & reference_pixels).sum())
    false_positive = int((predicted & ~reference_pixels).sum())
    false_negative = int((~predicted & reference_pixels).sum())
    precision = true_positive / (true_positive + false_positive) if true_positive + false_positive else 0.0
    recall = true_positive / (true_positive + false_negative) if true_positive + false_negative else 0.0
    f1 = 2 * precision * recall / (precision + recall) if precision + recall else 0.0
    return {
        "precision": precision,
        "recall": recall,
        "f1": f1,
        "true_positive": true_positive,
        "false_positive": false_positive,
        "false_negative": false_negative,
    }


# ---------------------------------------------------------------------------
# One image
# ---------------------------------------------------------------------------


def evaluate_image(scanner, base: Path, record: dict, options) -> dict:
    row = {
        "id": record["id"],
        "expression": record["expression"],
        "recipe": record["recipe"],
        "fontset": record.get("fontset", ""),
        "augmentations": [step["name"] for step in record.get("augmentations", [])],
        "has_boxes": bool(record.get("boxes")),
    }
    started = time.perf_counter()
    reading, scan_ms = scanner.read(base / record["path"])
    row["wall_ms"] = (time.perf_counter() - started) * 1000.0
    if reading is None:
        row["ok"] = False
        row["error"] = scan_ms
        row["scan_ms"] = row["wall_ms"]
        return row
    row["scan_ms"] = scan_ms
    row["ok"] = bool(reading.get("ok"))
    row["error"] = reading.get("error")
    if not row["ok"]:
        return row

    row["expression_text"] = reading.get("expression", "")
    row["kind"] = reading.get("kind", "")
    row["expect_text"] = record["expect_text"]
    row["expect_kind"] = record["expect_kind"]
    row["parse_valid"] = reading.get("parse_valid")
    row["repaired"] = bool(reading.get("repaired"))
    row["overrides"] = len(reading.get("overrides") or {})
    row["confidence"] = reading.get("confidence", 0.0)
    row["profile"] = reading.get("profile", "")
    row["binarization"] = reading.get("binarization", "")
    row["symbols_reported"] = reading.get("symbols", 0)
    row["timing_ms"] = {stage: float(reading.get("timing_ms", {}).get(stage, 0.0)) for stage in STAGES}

    glyphs = reading.get("glyphs") or []
    unknown = [glyph for glyph in glyphs if glyph.get("label") == "?"]
    row["unknown_glyphs"] = len(unknown)
    row["low_margin_glyphs"] = sum(1 for glyph in glyphs if float(glyph.get("margin", 1.0)) < 0.02)

    # Debug CLI: stage images and the working geometry.
    stage_dir = None
    if options.preprocess and options.scan_debug and Path(options.scan_debug).exists():
        stage_dir = Path(tempfile.mkdtemp(prefix=f"scan_{record['id']}_"))
        debug = subprocess.run(
            [str(options.scan_debug), str(base / record["path"]), str(stage_dir)],
            capture_output=True,
            text=True,
        )
        row["wall_ms"] = (time.perf_counter() - started) * 1000.0
        row["debug_symbols"] = None
        if debug.returncode == 0:
            try:
                debug_json = json.loads(debug.stdout)
                row["debug_symbols"] = debug_json.get("symbols")
                row["debug_ok"] = True
            except json.JSONDecodeError:
                row["debug_ok"] = False
        else:
            row["debug_ok"] = False

    work_size = [int(reading.get("width", 0)), int(reading.get("height", 0))]
    skew = float(reading.get("skewDegrees", 0.0))
    row["work_size"] = work_size
    row["skew_degrees"] = skew

    if options.preprocess and stage_dir is not None and work_size[0] > 0:
        row["preprocess"] = foreground_f1(base / record["ref"], stage_dir, work_size, skew)
        if not options.keep_stages:
            for png in stage_dir.glob("*.png"):
                png.unlink()
            stage_dir.rmdir()

    if not record.get("boxes"):
        row["segmentation"] = None
        return row

    truth = []
    for symbol in record["boxes"]:
        mapped = dict(symbol)
        mapped["box"] = map_box(symbol["box"], record["size"], work_size, skew)
        truth.append(mapped)
    predicted = [
        {"box": [float(v) for v in glyph["box"]], "label": glyph.get("label", "?"),
         "score": float(glyph.get("score", 0.0)),
         "alternatives": [glyph.get("label", "?")] + [
             alternative.get("glyph", "?") for alternative in glyph.get("alternatives") or []
         ]}
        for glyph in glyphs
    ]

    matches, missed, extra = match_symbols(truth, predicted, options.iou)
    row["segmentation"] = {
        "truth": len(truth),
        "predicted": len(predicted),
        "matched": len(matches),
        "missed": len(missed),
        "extra": len(extra),
        "mean_iou": statistics.fmean(score for _, _, score in matches) if matches else 0.0,
        "missed_classes": [symbol["class"] for symbol in missed],
        "extra_labels": [symbol["label"] for symbol in extra],
    }

    judged = []
    for symbol, glyph, _score in matches:
        if symbol["class"] is None:
            continue
        alternatives = glyph["alternatives"][:3]
        judged.append(
            {
                "truth": symbol["class"],
                "predicted": glyph["label"],
                "top1": glyph["label"] == symbol["class"],
                "top3": symbol["class"] in alternatives,
            }
        )
    row["classification"] = judged
    # Matched symbols whose class the template bank cannot express (a radical,
    # a bracket drawn from a font outside the bank): segmentation counts them,
    # classification cannot judge them.
    row["unjudged"] = len(matches) - len(judged)

    # Alignment sanity check: the mapped truth should cover the same span as
    # the detected symbols, give or take the classifier's own misses.
    if predicted:
        truth_box = _union([symbol["box"] for symbol in truth])
        pred_box = _union([glyph["box"] for glyph in predicted])
        row["alignment_error"] = [
            round(truth_box[0] - pred_box[0], 1),
            round(truth_box[1] - pred_box[1], 1),
            round(truth_box[2] - pred_box[2], 1),
            round(truth_box[3] - pred_box[3], 1),
        ]
    row["wall_ms"] = (time.perf_counter() - started) * 1000.0
    return row


def _union(boxes):
    x0 = min(box[0] for box in boxes)
    y0 = min(box[1] for box in boxes)
    x1 = max(box[2] for box in boxes)
    y1 = max(box[3] for box in boxes)
    return [x0, y0, x1, y1]


# ---------------------------------------------------------------------------
# Aggregation
# ---------------------------------------------------------------------------


def normalise(text: str) -> str:
    return "".join(character for character in (text or "") if not character.isspace())


def aggregate(rows, manifest, options) -> dict:
    result = {
        "manifest": str(options.manifest),
        "library": str(options.lib),
        "images": len(rows),
        "evaluated": sum(1 for row in rows if row["ok"]),
        "failed": [{"id": row["id"], "error": row["error"]} for row in rows if not row["ok"]],
        "config": {"iou_threshold": options.iou, "preprocess": bool(options.preprocess)},
        "per_image": rows,
    }

    # segmentation
    matched = sum(row["segmentation"]["matched"] for row in rows if row.get("segmentation"))
    missed = sum(row["segmentation"]["missed"] for row in rows if row.get("segmentation"))
    extra = sum(row["segmentation"]["extra"] for row in rows if row.get("segmentation"))
    truth_total = sum(row["segmentation"]["truth"] for row in rows if row.get("segmentation"))
    predicted_total = sum(row["segmentation"]["predicted"] for row in rows if row.get("segmentation"))
    ious = [row["segmentation"]["mean_iou"] for row in rows if row.get("segmentation")]
    precision = matched / predicted_total if predicted_total else 0.0
    recall = matched / truth_total if truth_total else 0.0
    result["segmentation"] = {
        "images": sum(1 for row in rows if row.get("segmentation")),
        "truth_symbols": truth_total,
        "predicted_symbols": predicted_total,
        "true_positive": matched,
        "false_positive": extra,
        "false_negative": missed,
        "precision": precision,
        "recall": recall,
        "f1": 2 * precision * recall / (precision + recall) if precision + recall else 0.0,
        "mean_iou_matched": statistics.fmean(ious) if ious else 0.0,
        "missed_by_class": _count_all(
            symbol for row in rows for symbol in (row.get("segmentation") or {}).get("missed_classes", [])
        ),
        "extra_by_label": _count_all(
            label for row in rows for label in (row.get("segmentation") or {}).get("extra_labels", [])
        ),
    }

    # classification
    judged = [entry for row in rows for entry in row.get("classification") or []]
    per_class: dict[str, dict[str, int]] = {}
    confusion: dict[str, dict[str, int]] = {}
    for entry in judged:
        stats = per_class.setdefault(entry["truth"], {"total": 0, "top1": 0, "top3": 0})
        stats["total"] += 1
        stats["top1"] += int(entry["top1"])
        stats["top3"] += int(entry["top3"])
        bucket = confusion.setdefault(entry["truth"], {})
        bucket[entry["predicted"]] = bucket.get(entry["predicted"], 0) + 1
    result["classification"] = {
        "judged": len(judged),
        "top1": (sum(1 for entry in judged if entry["top1"]) / len(judged)) if judged else 0.0,
        "top3": (sum(1 for entry in judged if entry["top3"]) / len(judged)) if judged else 0.0,
        "correct_top1": sum(1 for entry in judged if entry["top1"]),
        "correct_top3": sum(1 for entry in judged if entry["top3"]),
        "per_class": {
            name: {
                **stats,
                "top1_accuracy": stats["top1"] / stats["total"] if stats["total"] else 0.0,
                "top3_accuracy": stats["top3"] / stats["total"] if stats["total"] else 0.0,
            }
            for name, stats in sorted(per_class.items())
        },
        "confusion": confusion,
        "unjudged_matches": sum(row.get("unjudged", 0) for row in rows),
        "unknown_glyphs": sum(row.get("unknown_glyphs", 0) for row in rows),
    }

    # end to end
    with_text = [row for row in rows if row["ok"]]
    exact = [row for row in with_text if normalise(row["expression_text"]) == normalise(row["expect_text"])]
    kind_ok = [row for row in with_text if row["kind"] == row["expect_kind"]]
    parsed = [row for row in with_text if row["parse_valid"] is True]
    parsed_known = [row for row in with_text if row["parse_valid"] is not None]
    overrides = sum(row.get("overrides", 0) for row in with_text)
    images_with_overrides = sum(1 for row in with_text if row.get("overrides"))
    unknown = sum(row.get("unknown_glyphs", 0) for row in with_text)
    clean = [row for row in with_text if not row["augmentations"]]
    augmented = [row for row in with_text if row["augmentations"]]
    result["end_to_end"] = {
        "images": len(rows),
        "read": len(with_text),
        "exact": len(exact),
        "exact_rate": len(exact) / len(rows) if rows else 0.0,
        "parse_accepted": len(parsed),
        "parse_acceptance": (len(parsed) / len(parsed_known)) if parsed_known else None,
        "kind_match": len(kind_ok),
        "kind_match_rate": (len(kind_ok) / len(with_text)) if with_text else 0.0,
        "symbols_repaired": overrides,
        "images_repaired": images_with_overrides,
        "unknown_glyphs": unknown,
        "symbols_needing_correction": overrides + unknown,
        "clean": {
            "images": len(clean),
            "exact": sum(
                1 for row in clean if normalise(row["expression_text"]) == normalise(row["expect_text"])
            ),
        },
        "augmented": {
            "images": len(augmented),
            "exact": sum(
                1 for row in augmented if normalise(row["expression_text"]) == normalise(row["expect_text"])
            ),
        },
        "by_expression": _by_expression(with_text),
        "by_recipe": _by_recipe(with_text),
        "by_fontset": _by_fontset(with_text),
        "failures": [
            {
                "id": row["id"],
                "expression": row["expression"],
                "augmentations": row["augmentations"],
                "expected": row["expect_text"],
                "got": row["expression_text"],
            }
            for row in with_text
            if normalise(row["expression_text"]) != normalise(row["expect_text"])
        ],
    }

    # preprocess
    f1_rows = [row["preprocess"] for row in rows if row.get("preprocess")]
    result["preprocess"] = {
        "available": bool(f1_rows),
        "images": len(f1_rows),
        "precision": statistics.fmean(entry["precision"] for entry in f1_rows) if f1_rows else None,
        "recall": statistics.fmean(entry["recall"] for entry in f1_rows) if f1_rows else None,
        "f1": statistics.fmean(entry["f1"] for entry in f1_rows) if f1_rows else None,
        "worst": sorted(
            (
                {"id": row["id"], "f1": row["preprocess"]["f1"]}
                for row in rows
                if row.get("preprocess")
            ),
            key=lambda entry: entry["f1"],
        )[:5],
    }

    # resources
    per_stage = {}
    for stage in STAGES:
        values = [row["timing_ms"][stage] for row in rows if row.get("timing_ms")]
        if not values:
            continue
        per_stage[stage] = {
            "mean": statistics.fmean(values),
            "median": statistics.median(values),
            "p95": sorted(values)[max(0, math.ceil(0.95 * len(values)) - 1)],
            "max": max(values),
            "total": sum(values),
        }
    wall = [row["wall_ms"] for row in rows]
    result["resources"] = {
        "per_stage_ms": per_stage,
        "wall_ms": {
            "total": sum(wall),
            "mean": statistics.fmean(wall) if wall else 0.0,
            "median": statistics.median(wall) if wall else 0.0,
            "max": max(wall) if wall else 0.0,
        },
        "summed_stage_ms": sum(entry["mean"] for entry in per_stage.values()),
    }

    # diagnostics
    disagreements = [
        {"id": row["id"], "api": row.get("symbols_reported"), "debug": row.get("debug_symbols")}
        for row in rows
        if row.get("debug_symbols") is not None
        and row.get("symbols_reported") != row.get("debug_symbols")
    ]
    alignment = [row["alignment_error"] for row in rows if row.get("alignment_error")]
    result["diagnostics"] = {
        "symbol_count_disagreements": disagreements,
        "alignment_error_max_abs": max(
            (max(abs(value) for value in errors) for errors in alignment), default=0.0
        ),
        "recipes": _count_all(
            "+".join(row["augmentations"]) or "clean" for row in rows
        ),
    }
    return result


def _count_all(values) -> dict[str, int]:
    out: dict[str, int] = {}
    for value in values:
        key = "None" if value is None else str(value)
        out[key] = out.get(key, 0) + 1
    return dict(sorted(out.items(), key=lambda entry: -entry[1]))


def _by_recipe(rows) -> list[dict]:
    grouped: dict[str, dict] = {}
    for row in rows:
        stats = grouped.setdefault(
            row["recipe"],
            {
                "recipe": row["recipe"],
                "augmentations": row["augmentations"],
                "images": 0,
                "exact": 0,
                "parsed": 0,
                "symbols": 0,
                "symbols_correct": 0,
            },
        )
        stats["images"] += 1
        stats["exact"] += int(normalise(row["expression_text"]) == normalise(row["expect_text"]))
        stats["parsed"] += int(row["parse_valid"] is True)
        judged = row.get("classification") or []
        stats["symbols"] += len(judged)
        stats["symbols_correct"] += sum(1 for entry in judged if entry["top1"])
    out = sorted(grouped.values(), key=lambda entry: entry["recipe"])
    for entry in out:
        entry["symbol_top1"] = (
            entry["symbols_correct"] / entry["symbols"] if entry["symbols"] else None
        )
    return out


def _by_fontset(rows) -> list[dict]:
    grouped: dict[str, dict] = {}
    for row in rows:
        stats = grouped.setdefault(
            row["fontset"],
            {"fontset": row["fontset"], "images": 0, "exact": 0, "parsed": 0,
             "symbols": 0, "symbols_correct": 0},
        )
        stats["images"] += 1
        stats["exact"] += int(normalise(row["expression_text"]) == normalise(row["expect_text"]))
        stats["parsed"] += int(row["parse_valid"] is True)
        judged = row.get("classification") or []
        stats["symbols"] += len(judged)
        stats["symbols_correct"] += sum(1 for entry in judged if entry["top1"])
    out = sorted(grouped.values(), key=lambda entry: entry["fontset"])
    for entry in out:
        entry["symbol_top1"] = (
            entry["symbols_correct"] / entry["symbols"] if entry["symbols"] else None
        )
    return out


def _by_expression(rows) -> list[dict]:
    grouped: dict[str, dict] = {}
    for row in rows:
        stats = grouped.setdefault(
            row["expression"], {"expression": row["expression"], "images": 0, "exact": 0, "parsed": 0}
        )
        stats["images"] += 1
        stats["exact"] += int(normalise(row["expression_text"]) == normalise(row["expect_text"]))
        stats["parsed"] += int(row["parse_valid"] is True)
    return sorted(grouped.values(), key=lambda entry: (entry["exact"] / entry["images"], entry["expression"]))


# ---------------------------------------------------------------------------
# Reports
# ---------------------------------------------------------------------------


def format_text(result: dict) -> str:
    lines = []
    segmentation = result["segmentation"]
    classification = result["classification"]
    end_to_end = result["end_to_end"]
    preprocess = result["preprocess"]
    resources = result["resources"]

    lines.append("scanner dataset metrics")
    lines.append(f"  manifest    {result['manifest']}")
    lines.append(
        f"  images      {result['images']} (read {result['evaluated']}, failed {len(result['failed'])})"
    )
    lines.append(f"  library     {result['library']}")
    lines.append(f"  iou         {result['config']['iou_threshold']}")
    lines.append("")

    lines.append("== preprocess ==")
    if preprocess["available"]:
        lines.append(
            f"  foreground F1 {preprocess['f1']:.3f}  precision {preprocess['precision']:.3f}"
            f"  recall {preprocess['recall']:.3f}  over {preprocess['images']} images"
        )
        worst = ", ".join(f"{entry['id']}={entry['f1']:.2f}" for entry in preprocess["worst"][:3])
        if worst:
            lines.append(f"  worst        {worst}")
    else:
        lines.append("  not measured (--no-preprocess, or scan_debug is missing)")
    lines.append("")

    lines.append("== segmentation ==")
    lines.append(
        f"  ground truth {segmentation['truth_symbols']} symbols, detected {segmentation['predicted_symbols']}"
    )
    lines.append(
        f"  tp {segmentation['true_positive']}  fp {segmentation['false_positive']}"
        f"  fn {segmentation['false_negative']}"
    )
    lines.append(
        f"  precision {segmentation['precision']:.3f}  recall {segmentation['recall']:.3f}"
        f"  f1 {segmentation['f1']:.3f}  mean iou {segmentation['mean_iou_matched']:.3f}"
    )
    if segmentation["missed_by_class"]:
        missed = ", ".join(f"{name}x{count}" for name, count in list(segmentation["missed_by_class"].items())[:8])
        lines.append(f"  missed       {missed}")
    if segmentation["extra_by_label"]:
        extra = ", ".join(f"{name}x{count}" for name, count in list(segmentation["extra_by_label"].items())[:8])
        lines.append(f"  extra        {extra}")
    lines.append("")

    lines.append("== classification ==")
    lines.append(
        f"  judged {classification['judged']} symbols"
        f"  top-1 {classification['top1']:.3f} ({classification['correct_top1']})"
        f"  top-3 {classification['top3']:.3f} ({classification['correct_top3']})"
    )
    weak = [
        (name, stats)
        for name, stats in classification["per_class"].items()
        if stats["top1"] < 1.0
    ]
    if weak:
        detail = ", ".join(
            f"{name} {stats['top1']}/{stats['total']}" for name, stats in weak[:8]
        )
        lines.append(f"  below 1.0    {detail}")
    lines.append("")

    lines.append("== end-to-end ==")
    lines.append(
        f"  exact match {end_to_end['exact']}/{end_to_end['images']}"
        f" ({100 * end_to_end['exact_rate']:.1f}%)"
    )
    if end_to_end["parse_acceptance"] is not None:
        lines.append(
            f"  parser accepted {end_to_end['parse_accepted']}/{end_to_end['read']}"
            f" ({100 * end_to_end['parse_acceptance']:.1f}%)"
        )
    else:
        lines.append("  parser acceptance not measured (no validator injected)")
    lines.append(
        f"  kind match {end_to_end['kind_match']}/{end_to_end['read']}"
        f"  symbols needing correction {end_to_end['symbols_needing_correction']}"
        f" ({end_to_end['symbols_repaired']} repaired + {end_to_end['unknown_glyphs']} unknown)"
    )
    lines.append(
        f"  clean {end_to_end['clean']['exact']}/{end_to_end['clean']['images']}"
        f"   augmented {end_to_end['augmented']['exact']}/{end_to_end['augmented']['images']}"
    )
    if end_to_end["by_fontset"]:
        lines.append("  by font:")
        for entry in end_to_end["by_fontset"]:
            top1 = entry["symbol_top1"]
            top1_text = f"{top1:.2f}" if top1 is not None else "n/a"
            lines.append(
                f"    {entry['fontset']:<14} exact {entry['exact']}/{entry['images']}"
                f"  parsed {entry['parsed']}/{entry['images']}  symbol top-1 {top1_text}"
            )
    if end_to_end["by_recipe"]:
        lines.append("  by recipe:")
        for entry in end_to_end["by_recipe"]:
            top1 = entry["symbol_top1"]
            top1_text = f"{top1:.2f}" if top1 is not None else "n/a"
            lines.append(
                f"    {entry['recipe']:<14} exact {entry['exact']}/{entry['images']}"
                f"  parsed {entry['parsed']}/{entry['images']}  symbol top-1 {top1_text}"
            )
    for failure in end_to_end["failures"][:5]:
        lines.append(
            f"    {failure['id']} {failure['expression']} [{'+'.join(failure['augmentations']) or 'clean'}]"
            f" expected {failure['expected']!r} got {failure['got']!r}"
        )
    lines.append("")

    lines.append("== resources ==")
    for stage, stats in resources["per_stage_ms"].items():
        lines.append(
            f"  {stage:<11} median {stats['median']:7.2f} ms   mean {stats['mean']:7.2f}"
            f"   p95 {stats['p95']:7.2f}   max {stats['max']:7.2f}"
        )
    lines.append(
        f"  wall clock {resources['wall_ms']['total'] / 1000.0:.2f} s"
        f"  ({resources['wall_ms']['mean']:.1f} ms/image)"
    )
    if result["failed"]:
        lines.append("")
        lines.append("== failures ==")
        for failure in result["failed"][:10]:
            lines.append(f"  {failure['id']}: {failure['error']}")
    return "\n".join(lines)


def format_markdown(result: dict) -> str:
    segmentation = result["segmentation"]
    classification = result["classification"]
    end_to_end = result["end_to_end"]
    preprocess = result["preprocess"]
    resources = result["resources"]
    lines = ["# Scanner dataset metrics", ""]
    lines.append(f"- manifest: `{result['manifest']}`")
    lines.append(f"- images: {result['images']} (read {result['evaluated']}, failed {len(result['failed'])})")
    lines.append(f"- library: `{result['library']}`")
    lines.append(f"- IoU threshold: {result['config']['iou_threshold']}")
    lines.append("")

    lines.append("## Stage summary")
    lines.append("")
    lines.append("| stage | metric | value |")
    lines.append("|---|---|---|")
    if preprocess["available"]:
        lines.append(
            f"| preprocess | foreground F1 | {preprocess['f1']:.3f} "
            f"(p {preprocess['precision']:.3f}, r {preprocess['recall']:.3f}) |"
        )
    else:
        lines.append("| preprocess | foreground F1 | not measured |")
    lines.append(f"| segmentation | recall | {segmentation['recall']:.3f} |")
    lines.append(f"| segmentation | precision | {segmentation['precision']:.3f} |")
    lines.append(f"| segmentation | F1 | {segmentation['f1']:.3f} |")
    lines.append(f"| classification | top-1 | {classification['top1']:.3f} ({classification['judged']} symbols) |")
    lines.append(f"| classification | top-3 | {classification['top3']:.3f} |")
    lines.append(f"| end-to-end | exact match | {end_to_end['exact']}/{end_to_end['images']} |")
    if end_to_end["parse_acceptance"] is not None:
        lines.append(f"| end-to-end | parser acceptance | {100 * end_to_end['parse_acceptance']:.1f}% |")
    lines.append(
        f"| end-to-end | symbols needing correction | {end_to_end['symbols_needing_correction']} |"
    )
    lines.append("")

    lines.append("## Segmentation")
    lines.append("")
    lines.append("| truth | detected | tp | fp | fn | recall | precision | mean IoU |")
    lines.append("|---|---|---|---|---|---|---|---|")
    lines.append(
        f"| {segmentation['truth_symbols']} | {segmentation['predicted_symbols']} "
        f"| {segmentation['true_positive']} | {segmentation['false_positive']} "
        f"| {segmentation['false_negative']} | {segmentation['recall']:.3f} "
        f"| {segmentation['precision']:.3f} | {segmentation['mean_iou_matched']:.3f} |"
    )
    if segmentation["missed_by_class"]:
        lines.append("")
        lines.append(
            "Missed symbols by class: "
            + ", ".join(f"`{name}` x{count}" for name, count in segmentation["missed_by_class"].items())
        )
    if segmentation["extra_by_label"]:
        lines.append("")
        lines.append(
            "Extra detections by label: "
            + ", ".join(f"`{name}` x{count}" for name, count in segmentation["extra_by_label"].items())
        )
    lines.append("")

    lines.append("## Classification")
    lines.append("")
    lines.append("| class | total | top-1 | top-3 |")
    lines.append("|---|---|---|---|")
    for name, stats in classification["per_class"].items():
        lines.append(
            f"| `{name}` | {stats['total']} | {stats['top1_accuracy']:.2f} | {stats['top3_accuracy']:.2f} |"
        )
    lines.append("")
    confusions = []
    for truth, bucket in classification["confusion"].items():
        for predicted, count in bucket.items():
            if truth != predicted:
                confusions.append((count, truth, predicted))
    confusions.sort(reverse=True)
    if confusions:
        lines.append("### Confusion matrix (wrong predictions)")
        lines.append("")
        lines.append("| truth | predicted | count |")
        lines.append("|---|---|---|")
        for count, truth, predicted in confusions[:20]:
            lines.append(f"| `{truth}` | `{predicted}` | {count} |")
        lines.append("")

    lines.append("## End to end")
    lines.append("")
    lines.append("| metric | value |")
    lines.append("|---|---|")
    lines.append(f"| exact match | {end_to_end['exact']}/{end_to_end['images']} ({100 * end_to_end['exact_rate']:.1f}%) |")
    lines.append(f"| clean renders | {end_to_end['clean']['exact']}/{end_to_end['clean']['images']} |")
    lines.append(f"| augmented | {end_to_end['augmented']['exact']}/{end_to_end['augmented']['images']} |")
    lines.append(f"| kind match | {end_to_end['kind_match']}/{end_to_end['read']} |")
    lines.append(
        f"| symbols repaired | {end_to_end['symbols_repaired']} in {end_to_end['images_repaired']} images |"
    )
    lines.append(f"| unknown glyphs | {end_to_end['unknown_glyphs']} |")
    if end_to_end["parse_acceptance"] is not None:
        lines.append(f"| parser acceptance | {end_to_end['parse_accepted']}/{end_to_end['read']} |")
    lines.append("")
    if end_to_end["failures"]:
        lines.append("### Mismatches")
        lines.append("")
        lines.append("| id | expression | augmentation | expected | got |")
        lines.append("|---|---|---|---|---|")
        for failure in end_to_end["failures"]:
            lines.append(
                f"| {failure['id']} | {failure['expression']} | {'+'.join(failure['augmentations']) or 'clean'} "
                f"| `{failure['expected']}` | `{failure['got']}` |"
            )
        lines.append("")

    if end_to_end["by_fontset"]:
        lines.append("  by font:")
        for entry in end_to_end["by_fontset"]:
            top1 = entry["symbol_top1"]
            top1_text = f"{top1:.2f}" if top1 is not None else "n/a"
            lines.append(
                f"    {entry['fontset']:<14} exact {entry['exact']}/{entry['images']}"
                f"  parsed {entry['parsed']}/{entry['images']}  symbol top-1 {top1_text}"
            )
    if end_to_end["by_fontset"]:
        lines.append("### By math font")
        lines.append("")
        lines.append("| fontset | images | exact | parse accepted | symbol top-1 |")
        lines.append("|---|---|---|---|---|")
        for entry in end_to_end["by_fontset"]:
            top1 = entry["symbol_top1"]
            top1_text = f"{top1:.2f}" if top1 is not None else "n/a"
            lines.append(
                f"| {entry['fontset']} | {entry['images']} | {entry['exact']} "
                f"| {entry['parsed']} | {top1_text} |"
            )
        lines.append("")

    if end_to_end["by_recipe"]:
        lines.append("### By augmentation")
        lines.append("")
        lines.append("| recipe | images | exact | parse accepted | symbol top-1 |")
        lines.append("|---|---|---|---|---|")
        for entry in end_to_end["by_recipe"]:
            top1 = entry["symbol_top1"]
            top1_text = f"{top1:.2f}" if top1 is not None else "n/a"
            lines.append(
                f"| {entry['recipe']} | {entry['images']} | {entry['exact']} "
                f"| {entry['parsed']} | {top1_text} |"
            )
        lines.append("")

    lines.append("## Resources")
    lines.append("")
    lines.append("| stage | median ms | mean ms | p95 ms | max ms |")
    lines.append("|---|---|---|---|---|")
    for stage, stats in resources["per_stage_ms"].items():
        lines.append(
            f"| {stage} | {stats['median']:.2f} | {stats['mean']:.2f} | {stats['p95']:.2f} | {stats['max']:.2f} |"
        )
    lines.append(
        f"\nWall clock: {resources['wall_ms']['total'] / 1000.0:.2f} s total, "
        f"{resources['wall_ms']['mean']:.1f} ms/image."
    )
    lines.append("")

    lines.append("## Per expression")
    lines.append("")
    lines.append("| expression | images | exact | parse accepted |")
    lines.append("|---|---|---|---|")
    for entry in sorted(end_to_end["by_expression"], key=lambda entry: entry["expression"]):
        lines.append(f"| {entry['expression']} | {entry['images']} | {entry['exact']} | {entry['parsed']} |")
    lines.append("")

    lines.append("## Notes")
    lines.append("")
    lines.append(
        "- Ground-truth symbols are one box per typeset glyph (matplotlib's own glyph "
        "outline boxes), with a fraction bar as its own symbol and a radical's overbar "
        "folded into the radical glyph. Glyphs that merely touch stay separate, so recall "
        "drops when the scanner merges them and precision drops when it splits one glyph."
    )
    lines.append(
        "- Boxes are mapped from the generated image into the working image with the exact "
        "downscale + deskew transform the preprocessor applies; the maximum residual "
        f"alignment error over the run was {result['diagnostics']['alignment_error_max_abs']:.1f} px."
    )
    lines.append(
        "- Foreground F1 compares the binarized page with the reference render that still has "
        "the geometric augmentations (rotation, perspective, resolution) but no blur, noise, "
        "lighting gradient, paper texture or ruled lines, so those count as foreground error."
    )
    if result["diagnostics"]["symbol_count_disagreements"]:
        lines.append(
            "- `scan_debug` and the C API disagreed about the symbol count on: "
            + ", ".join(
                f"{entry['id']} ({entry['api']} vs {entry['debug']})"
                for entry in result["diagnostics"]["symbol_count_disagreements"][:8]
            )
        )
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST))
    parser.add_argument("--lib", default=str(DEFAULT_LIB))
    parser.add_argument("--bank", default=str(DEFAULT_BANK))
    parser.add_argument("--scan-debug", default=str(DEFAULT_DEBUG))
    parser.add_argument("--limit", type=int, default=0, help="evaluate only the first N images")
    parser.add_argument("--iou", type=float, default=0.5, help="IoU threshold for symbol matching")
    parser.add_argument("--out-dir", default=None, help="where to write metrics.json/md")
    parser.add_argument("--json-name", default="metrics.json")
    parser.add_argument("--md-name", default="metrics.md")
    parser.add_argument("--no-preprocess", action="store_true", help="skip the foreground F1 stage")
    parser.add_argument("--no-validator", action="store_true", help="do not inject the core parser")
    parser.add_argument("--keep-stages", action="store_true", help="keep scan_debug stage images")
    parser.add_argument("--quiet", "-q", action="store_true")
    parser.add_argument(
        "--in-process",
        action="store_true",
        help="scan in this process (a native crash then ends the run)",
    )
    parser.add_argument(
        "--timeout", type=float, default=120.0, help="seconds allowed per image in the worker"
    )
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.worker:
        return run_worker()
    args.preprocess = not args.no_preprocess

    manifest_path = Path(args.manifest)
    if not manifest_path.exists():
        raise SystemExit(
            f"manifest not found: {manifest_path}\n"
            "generate one with: python3 scanner/tools/make_dataset.py --limit 5"
        )
    manifest = json.loads(manifest_path.read_text())
    base = manifest_path.parent
    records = manifest["images"]
    if args.limit:
        records = records[: args.limit]
    if not records:
        raise SystemExit("the manifest has no images")

    if args.in_process:
        backend = Scanner(Path(args.lib), Path(args.bank), use_validator=not args.no_validator)

        class _InProcess:
            def read(self, path):
                started = time.perf_counter()
                reading = backend.read(Path(path).read_bytes())
                return reading, (time.perf_counter() - started) * 1000.0

            def close(self):
                return None

        scanner = _InProcess()
    else:
        scanner = ScanWorker(args)
    started = time.perf_counter()
    rows = []
    for index, record in enumerate(records):
        row = evaluate_image(scanner, base, record, args)
        rows.append(row)
        if not args.quiet:
            status = "ok" if row["ok"] else f"FAILED ({row['error']})"
            print(f"[{index + 1}/{len(records)}] {record['id']} {record['expression']:<18} {status}",
                  file=sys.stderr)
    scanner.close()
    elapsed = time.perf_counter() - started

    result = aggregate(rows, manifest, args)
    result["tool_seconds"] = elapsed
    # `options` is a Namespace; keep the JSON serialisable.
    result["config"]["iou_threshold"] = args.iou
    result["config"]["preprocess"] = args.preprocess

    out_dir = Path(args.out_dir) if args.out_dir else base
    out_dir.mkdir(parents=True, exist_ok=True)
    json_path = out_dir / args.json_name
    md_path = out_dir / args.md_name
    json_path.write_text(json.dumps(result, indent=1, sort_keys=True) + "\n")
    md_path.write_text(format_markdown(result) + "\n")

    if not args.quiet:
        print(format_text(result))
        print()
        print(f"wrote {json_path} and {md_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
