#!/usr/bin/env python3
"""Smoke test for the synthetic dataset and the per-stage metrics report.

Generates three images with one augmentation chain into a temporary directory,
runs `scanner/tools/metrics.py` over them and checks that every section of the
report is there with non-zero counts. It needs no network and finishes in a few
seconds; it does need the scanner built once:

    cmake -S scanner -B scanner/build -DCMAKE_BUILD_TYPE=Release
    cmake --build scanner/build -j$(nproc)

Run it with pytest (`python3 -m pytest python/tests/test_dataset_metrics.py`) or
directly (`python3 python/tests/test_dataset_metrics.py`).
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAKE_DATASET = ROOT / "scanner" / "tools" / "make_dataset.py"
METRICS = ROOT / "scanner" / "tools" / "metrics.py"
LIBRARY = ROOT / "core" / "build" / "lib" / "libodecore.so"
BANK = ROOT / "scanner" / "data" / "glyph_templates.bin"
SCAN_DEBUG = ROOT / "scanner" / "build" / "scan_debug"

REPORT_SECTIONS = ("preprocess", "segmentation", "classification", "end-to-end", "resources")


def python_with_math_libs() -> str | None:
    """An interpreter that can import numpy, PIL and matplotlib.

    The scanner venv is created with `include-system-site-packages = false`, so
    it usually cannot; the system interpreter can. Try the obvious candidates
    rather than guessing.
    """
    candidates = [sys.executable, str(Path.home() / ".venvs" / "scanner" / "bin" / "python"), "python3"]
    seen = set()
    for candidate in candidates:
        if candidate in seen:
            continue
        seen.add(candidate)
        try:
            probe = subprocess.run(
                [candidate, "-c", "import numpy, PIL, matplotlib"],
                capture_output=True,
                timeout=60,
            )
        except (OSError, subprocess.SubprocessError):
            continue
        if probe.returncode == 0:
            return candidate
    return None


def run(command, timeout):
    finished = subprocess.run(command, capture_output=True, text=True, timeout=timeout)
    if finished.returncode != 0:
        raise AssertionError(
            f"command failed ({finished.returncode}): {' '.join(str(part) for part in command)}\n"
            f"stdout:\n{finished.stdout}\nstderr:\n{finished.stderr}"
        )
    return finished


class DatasetMetricsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.python = python_with_math_libs()
        if cls.python is None:
            raise unittest.SkipTest("no interpreter with numpy, PIL and matplotlib")
        missing = [str(path) for path in (MAKE_DATASET, METRICS, BANK, SCAN_DEBUG) if not path.exists()]
        if missing:
            raise unittest.SkipTest("build the scanner first; missing: " + ", ".join(missing))
        if not LIBRARY.exists():
            raise unittest.SkipTest(f"native library not built: {LIBRARY}")

        cls.tmp = tempfile.TemporaryDirectory(prefix="dataset_metrics_test_")
        cls.outdir = Path(cls.tmp.name) / "dataset"
        run(
            [
                cls.python,
                str(MAKE_DATASET),
                "--outdir",
                str(cls.outdir),
                "--limit",
                "3",
                "--variants",
                "1",
                "--recipe",
                "blur-noise",
                "--seed",
                "4242",
            ],
            timeout=180,
        )
        cls.metrics_run = run(
            [cls.python, str(METRICS), "--manifest", str(cls.outdir / "manifest.json")],
            timeout=180,
        )
        cls.metrics = json.loads((cls.outdir / "metrics.json").read_text())
        cls.manifest = json.loads((cls.outdir / "manifest.json").read_text())

    @classmethod
    def tearDownClass(cls):
        if getattr(cls, "tmp", None) is not None:
            cls.tmp.cleanup()

    def test_manifest_has_ground_truth(self):
        images = self.manifest["images"]
        self.assertEqual(len(images), 3)
        for record in images:
            self.assertIn("expect_text", record)
            self.assertTrue(record["expect_text"])
            self.assertTrue(record["augmentations"], "the smoke chain must augment")
            self.assertIsNotNone(record["boxes"], f"{record['id']} has no ground-truth boxes")
            self.assertGreaterEqual(len(record["boxes"]), 4)
            self.assertTrue((self.outdir / record["path"]).exists())
            self.assertTrue((self.outdir / record["clean"]).exists())
            self.assertTrue((self.outdir / record["ref"]).exists())

        classes = [symbol["class"] for record in images for symbol in record["boxes"]]
        self.assertTrue(any(classes), "no symbol carries a classifier label")

    def test_report_has_every_section(self):
        metrics = self.metrics
        for key in ("preprocess", "segmentation", "classification", "end_to_end", "resources", "per_image"):
            self.assertIn(key, metrics)
        for section in REPORT_SECTIONS:
            self.assertIn(f"== {section} ==", self.metrics_run.stdout)

    def test_counts_are_non_zero(self):
        metrics = self.metrics
        self.assertEqual(metrics["images"], 3)
        self.assertEqual(metrics["evaluated"], 3)
        self.assertEqual(metrics["failed"], [])

        segmentation = metrics["segmentation"]
        self.assertGreater(segmentation["truth_symbols"], 0)
        self.assertGreater(segmentation["predicted_symbols"], 0)
        self.assertGreater(segmentation["true_positive"], 0)
        self.assertGreater(segmentation["recall"], 0.0)
        self.assertGreater(segmentation["precision"], 0.0)
        self.assertLessEqual(segmentation["precision"], 1.0)

        classification = metrics["classification"]
        self.assertGreater(classification["judged"], 0)
        self.assertGreater(classification["top1"], 0.0)
        self.assertGreaterEqual(classification["top3"], classification["top1"])
        self.assertTrue(classification["per_class"])
        self.assertTrue(classification["confusion"])

        end_to_end = metrics["end_to_end"]
        self.assertEqual(end_to_end["read"], 3)
        self.assertGreater(end_to_end["exact"], 0, "no expression read exactly")
        self.assertGreater(end_to_end["parse_accepted"], 0)
        self.assertIsNotNone(end_to_end["parse_acceptance"])

        preprocess = metrics["preprocess"]
        self.assertTrue(preprocess["available"])
        self.assertEqual(preprocess["images"], 3)
        self.assertGreater(preprocess["f1"], 0.5)

        resources = metrics["resources"]
        for stage in ("preprocess", "segment", "classify"):
            self.assertIn(stage, resources["per_stage_ms"])
            self.assertGreater(resources["per_stage_ms"][stage]["median"], 0.0)
        self.assertGreater(resources["wall_ms"]["total"], 0.0)

    def test_markdown_report_exists(self):
        markdown = (self.outdir / "metrics.md").read_text()
        for heading in ("# Scanner dataset metrics", "## Segmentation", "## Classification", "## End to end", "## Resources"):
            self.assertIn(heading, markdown)
        self.assertIn("exact match", markdown)

    def test_truth_boxes_hit_ink(self):
        """The published boxes must actually cover the glyphs they claim."""
        try:
            from PIL import Image
            import numpy as np
        except ImportError:  # pragma: no cover - guarded by setUpClass
            raise unittest.SkipTest("PIL/numpy unavailable")
        for record in self.manifest["images"]:
            image = np.asarray(Image.open(self.outdir / record["path"]).convert("L")) < 128
            covered = np.zeros_like(image)
            for symbol in record["boxes"]:
                x0, y0, x1, y1 = [int(round(value)) for value in symbol["box"]]
                covered[max(0, y0 - 1) : y1 + 2, max(0, x0 - 1) : x1 + 2] = True
            if not image.any():
                self.fail(f"{record['id']} has no ink")
            self.assertGreater(
                float((image & covered).sum()) / float(image.sum()),
                0.9,
                f"{record['id']} ground-truth boxes miss ink",
            )


if __name__ == "__main__":
    os.environ.setdefault("PYTHONDONTWRITEBYTECODE", "1")
    unittest.main(verbosity=2)
