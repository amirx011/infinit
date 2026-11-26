# Infinit Math Scanner — Classical (No-ML) Implementation Plan

> Revision of `infinit_math_scanner_implementation_plan.md`.
> Goal: photo of a math problem → editable, parser-valid expression → existing C++ Core.
> Constraint: **no neural/AI model**. Only OpenCV, classical computer vision, and hand-written C++ logic.

---

## 0. Honest scope and expectations

Removing neural models changes *where* the difficulty lives. Classical pipelines are fast, tiny, deterministic and debuggable, but they are weaker on messy input.

| Input type | Expected result (well-lit, cropped) |
|---|---|
| Printed: algebra, fractions, powers, roots, `sin/cos/ln`, derivatives with `'` and `dy/dx` | Good |
| Printed: integrals with limits, sums, limits (`lim`), simple matrices, systems with `{` | Moderate; needs correction UI |
| Handwriting drawn on an in-app canvas (strokes captured live, see Section 12A) | Good for neat writing; the recommended handwriting path |
| Neat block-letter handwriting in a **photo** | Moderate to weak |
| Cursive or sloppy handwriting, dense multi-line pages | Not realistic without ML |

Decisions that follow from this:

1. **Printed math is the primary target for photos.** Handwriting goes through the live canvas path (Section 12A); handwriting inside photos is best effort.
2. **The correction UI is part of the recognizer, not an afterthought.** Symbol-level correction (Section 12) is the biggest accuracy lever you have.
3. **Keep a plug-in seam** (`ISymbolClassifier`) so a small model could be dropped in later without touching segmentation, structure analysis, or UI. You are not building it now; you are just not blocking it.

---

## 1. What changes vs. the previous plan

| Previous plan section | Status |
|---|---|
| §3–5 Camera, capture, review, crop (Flutter) | **Keep.** Upgrade crop to 4-corner perspective warp (Section 3.1). |
| §6 OpenCV pipeline | **Expand** (Section 3). |
| §7 OCR engine selection (Tesseract, Pix2Text, ONNX) | **Replace.** Pix2Text and ONNX models are neural. Tesseract 4+ is an LSTM network too, and it does not understand math structure anyway. |
| §8 OCR output contract | **Replace** with a richer contract (Section 10). Confidence becomes real, because you compute it. |
| §9 LaTeX → internal syntax normalizer | **Remove.** The recognizer produces an expression **tree**; emit LaTeX (for rendering) and internal syntax (for the Core) from that tree. No regex conversion between strings. |
| §10 Preview/edit | **Keep**, add symbol-level overlay (Section 12). |
| §11–13 FFI, performance, testing | **Keep**, adapted below. |

---

## 2. Architecture

```text
Flutter (Dart)
  Camera → Review → Crop (4 corners) → Scanner result screen → Core solver
                          │ pixels / file path (FFI, in an Isolate)
                          ▼
Native C++ module: infinit_scanner  (no dependency on Solver)
  1. Preprocess        OpenCV: normalize, binarize, deskew, clean
  2. Segment           connected components → symbol candidates, text lines
  3. Classify          features + templates → top-k candidates per symbol
  4. Lex               group symbols: numbers, function names, d/dx, primes
  5. Structure         baseline tree: fractions, roots, scripts, big operators, matrices
  6. Build tree        expression AST with per-node confidence
  7. Emit              LaTeX printer + Infinit-syntax printer (both from the AST)
  8. Repair            validate with Core parser; re-rank alternatives if invalid
                          │ JSON result
                          ▼
Infinit Core parser / solver (unchanged)
```

Rules:

- The scanner module links OpenCV and C++ standard library only. It must not include Solver headers. The only Core dependency is a **parser-validation callback** injected at runtime (Section 9).
- Every stage is a pure function: `Input → Output + debug artifacts`. This makes stages unit-testable and lets you build a stage-by-stage debug viewer (Section 13.4).
- The AST is the source of truth. Strings are always printed from it.

---

## 3. Stage 1 — Preprocessing (OpenCV)

### 3.1 Geometry

- Apply EXIF orientation first.
- If the user adjusts four corners in the crop screen, use `getPerspectiveTransform` + `warpPerspective` instead of a rectangular crop. This fixes angled photos for free.
- Cap the long side (about 1600–2000 px) before processing. Keep the original untouched on disk.

### 3.2 Illumination and contrast (do this before binarizing)

1. Convert to grayscale.
2. Estimate the background with a large blur or a large morphological close, then divide: `normalized = gray / background * 255`. This removes shadows and uneven lighting better than thresholding alone.
3. Optional mild CLAHE if contrast is still low.
4. Very light denoise (small Gaussian or median 3×3). Heavy denoise destroys primes, dots, and thin fraction bars.

### 3.3 Binarization (produce 2–3 candidates)

| Candidate | Method | Best for |
|---|---|---|
| A | Otsu on the normalized image | Clean prints |
| B | Sauvola (or Niblack) local threshold, implemented with integral images (`cv::integral`) | Shadows, paper texture |
| C | `adaptiveThreshold` (Gaussian) | Fallback |

The whole pipeline runs on each candidate (it is cheap). Pick the winner by **total cost** (Section 9), not by looking at the image.

Always make the foreground white (255) on black (0) for connected components.

### 3.4 Deskew

- Estimate the angle with the minimum-area rectangle of foreground pixels, or a projection-profile search over about ±10° (maximize the variance of the row histogram).
- Rotate only if the estimated angle is above roughly 0.5° **and** the estimate is stable (two methods agree). Otherwise skip.
- Rotation must be applied to the grayscale, then binarize again, or you get jagged edges.

### 3.5 Cleaning

- **Ruled notebook lines:** remove only lines that are long (more than ~85% of image width), thin, and low-contrast. **Do not** remove short horizontal lines: they are fraction bars and minus signs.
- Remove border artifacts (components touching the image edge with large area).
- Drop specks: components below a minimum area relative to the median symbol size.

**Acceptance:** each stage can export a debug image; if processing makes things worse, the original-based candidate is still available.

---

## 4. Stage 2 — Segmentation

### 4.1 Connected components

- `connectedComponentsWithStats` (8-connectivity) gives bounding box, area, centroid.
- Use `findContours` with hierarchy when you need hole counts (Section 5).

### 4.2 Scale normalization

After a first component pass, compute the **median component height** and the typical x-height. Use it to set every later threshold (merge gaps, min area, script ratios). Never hard-code pixel values; they break with different photo resolutions.

### 4.3 Multi-component symbols (merge rules)

| Symbol | Rule |
|---|---|
| `i`, `j`, `!`, `?`, `;`, `:` | Dot above/below a stem, horizontally overlapping, small gap |
| `=`, `≠`, `≡` | Two flat wide bars, overlapping x-range, small vertical gap (`≠`: plus a diagonal crossing both) |
| `÷` | Bar with a dot above and a dot below |
| `±`, `∓` | Plus with a bar underneath/above |
| `≤`, `≥` | `<`/`>` with a bar below |
| `%` | Two small circles plus a slash |
| `∂`, `∫`, `∑`, `∏` | Usually single components, but check |

Do merging **conservatively** and keep the unmerged alternative; the repair stage can retry.

### 4.3.1 Touching and broken symbols

- **Touching** (common in handwriting and low-res prints): try splitting wide components at vertical-projection valleys or at contour concavity points; keep both interpretations as alternatives.
- **Broken** (thin strokes): small morphological close with a tiny kernel, applied only when gaps are below a fraction of stroke width.

### 4.4 Line detection

- Horizontal projection profile on the deskewed binary image, with valleys splitting lines. Fraction bars, large brackets, and stacked fractions mean valleys are unreliable, so first **group by vertical overlap** of component bounding boxes into "line clusters".
- Multi-line input (systems, several equations): split into lines, then recognize each line independently.
- A large left brace `{` spanning several lines marks a **system** (Section 7.6).

---

## 5. Stage 3 — Symbol recognition (classical)

### 5.1 Feature vector per component

- Normalized crop (for example 32×32), preserving aspect ratio with padding. The aspect ratio itself is a separate feature.
- Hole count and hierarchy from `findContours` (0 and 6 and 9: 1 hole; 8: 2 holes; 1 and 7: 0 holes).
- Zoning density (4×4 or 8×8 grid of foreground ratio).
- Hu moments, projections, crossing counts along several scan lines.
- Optional skeleton (Zhang–Suen thinning) for endpoints and junctions.
- Position relative to the current line: size relative to x-height, vertical centroid relative to the baseline.

### 5.2 Classifier tiers

| Tier | Method | Use |
|---|---|---|
| **A (default, v1)** | Template matching over **synthetic templates**: render digits, Latin, Greek, and operators from several math fonts (Latin Modern / STIX / DejaVu) at multiple weights; compare with normalized correlation or chamfer distance plus the features above | Printed text |
| **B (optional, v1.5)** | Hand-written kNN over HOG/zoning features, with a prototype set built offline | Neat handwriting |
| C (not planned) | Any neural model behind `ISymbolClassifier` | Future seam only |

**Decision (resolved):** "no model" means no neural network weights and no model files to load. Tier B is a **prototype set** (plain data: labeled symbol samples, bundled plus user-collected) matched by distance and kNN voting in hand-written C++. Everything stays offline. If this ever needs revisiting, `ISymbolClassifier` is the only place that changes.

Write kNN yourself (a few dozen lines) instead of linking OpenCV's `ml` module; it keeps the binary small.

### 5.3 Output: top-k, never top-1

Every symbol returns a ranked candidate list with distances:

```text
{ glyph: "1", score: 0.91 }, { glyph: "l", score: 0.84 }, { glyph: "|", score: 0.71 }
```

The later stages use context to choose among candidates (Sections 6 and 9).

### 5.4 Known ambiguity classes (build these into the scoring)

| Confusable set | Disambiguation |
|---|---|
| `1` / `l` / `I` / `|` | Neighbors: digit sequences vs letter sequences; `|` is tall, thin, and often paired (absolute value, determinant) |
| `0` / `O` / `o` | Size vs x-height; surrounded by digits |
| `x` / `×` | Between two operands and not a script → multiplication |
| `-` / `−` / fraction bar / `—` | A bar is a fraction bar if symbols sit **above and below** it within its x-range; otherwise minus |
| `.` | Decimal point (between digits, on baseline), multiplication dot (centered), period |
| `'` | Prime if small, raised, and attached to the right of a symbol; count them for `y''` |
| `s`/`5`/`S`, `z`/`2`, `u`/`v`, `c`/`C` | Case via size relative to x-height; digits vs letters via neighbor type |
| `d` in `dx` | Differential if after `∫` or inside `d?/d?`; otherwise a variable |
| `e`, `i` | Constant or variable; resolved by the Core parser (Section 9) |

---

## 6. Stage 4 — Lexing (symbols → tokens)

- **Numbers:** adjacent digits (and a decimal point) with small gaps merge into one number token.
- **Function names:** adjacent letters with small gaps are matched against a dictionary: `sin cos tan cot sec csc sinh cosh tanh arcsin arccos arctan ln log exp lim det tr rank dim min max gcd`. Pick the longest dictionary match; otherwise treat as separate variables.
- **Derivative forms:** `dy/dx`, `d²y/dx²`, `∂f/∂x`, `y'`, `y''`, `f'(x)`, `ẏ` (dot above) recognized as patterns after structure analysis.
- **Differentials:** `dx`, `dt` after `∫` become a single `differential` token.
- **Implicit multiplication:** insert explicit `*` at tree-printing time (Section 8), not here.

---

## 7. Stage 5 — Structure analysis (the core of math OCR)

Use a **baseline structure tree** approach (the classical Zanibbi/Blostein/Cordy family of methods): recursively find dominant structures, then analyze the remaining linear sequence by baseline and symbol typography.

### 7.1 Order of recognition (dominant first)

1. Systems/matrices (big brackets, braces)
2. Fraction bars
3. Radicals
4. Big operators (`∑ ∏ ∫ lim`) with limits
5. Linear sequence with scripts

At each step, remove what was recognized, recurse into the sub-regions, and continue.

### 7.2 Fractions

A horizontal bar is a fraction bar when: it is wide relative to neighbors, has components **above** and **below** within its x-extent (with tolerance), and is not an `=` half. Numerator = components above; denominator = components below. Nested fractions fall out of recursion (the longest bar is processed first; its children contain the shorter ones).

### 7.3 Radicals

Recognize the radical sign by shape (check mark + long overbar). The radicand is every component inside the region **under the overbar**. An index (small digit at the upper left) is a component in the "notch" region left of the check.

### 7.4 Scripts (superscript/subscript)

Classify each symbol by typography:

- **Ascender:** digits, capitals, `b d f h k l t`
- **Descender:** `g j p q y`
- **Centered:** `a c e m n o r s u v w x z`

Compute each symbol's expected baseline and x-height, then classify the next symbol relative to the previous one:

- **Right** (same baseline)
- **Superscript** (centroid above, size smaller)
- **Subscript** (centroid below, size smaller)
- Also **above/below** (for limits and accents) and **inside** (radicals)

Use size ratio (scripts are typically 60–75% of the base) together with vertical offset. Offset alone is fragile on descenders and handwriting.

### 7.5 Big operators

- `∫`, `∑`, `∏`: limits can be **above/below** (display style) or **sub/superscript** (inline). Check both zones.
- `lim` with `x→0` underneath: after lexing `lim`, collect components directly below.
- After `∫`, expect a differential; use that as a parsing hint for repair.

### 7.6 Matrices and systems

- Large brackets `[ ]`, `( )`, `| |` (determinant) or a braces/`{` mark the extent.
- Inside a matrix: build a grid from **row clusters** (vertical overlap) and **column clusters** (consistent x-gaps between aligned cells). Each cell is recognized as its own sub-expression.
- `{` with stacked equations → system of equations (array of lines).
- Validation: all rows must have the same number of columns, otherwise flag low confidence and let the user fix the grid in the editor.

### 7.7 Output of this stage

A **tree of nodes** with spatial provenance (bounding boxes) and relation labels. Every relation decision keeps its margin (how far from the decision boundary), which feeds confidence.

---

## 8. Stage 6–7 — AST and emitters

```text
Node: Number | Variable | Constant | BinaryOp | UnaryOp | Fraction | Power |
      Subscript | Root | Function | Derivative | Integral | Sum | Limit |
      Matrix | Equation | System | Group(parens)
```

Two printers, both from the same tree:

1. `toLatex(node)` → for rendering in the preview (KaTeX/flutter_math_fork-style renderer already used in the app).
2. `toInfinit(node)` → exactly the syntax your Core parser accepts (`x^2`, `sqrt(x)`, `(a)/(b)`, …).

Printer rules worth writing tests for:

- Insert `*` for implicit multiplication (`2x → 2*x`, `)(` → `)*(`), but not between a function name and its argument.
- Wrap numerator/denominator/exponent/radicand in parentheses when it is not a single atom.
- `sin^2(x)`, `f'(x)`, `y''` are explicit patterns, not string hacks.
- Unknown constructs are emitted as an explicit `Unknown` node and **reported**; they are never silently dropped.

**Replace the previous plan's §9 normalizer.** If you ever need to import LaTeX from elsewhere (paste, history), write a real LaTeX→AST parser; do not use chained replaces.

---

## 9. Stage 8 — Parser-guided repair (big accuracy win)

You have a real parser in the Core. Use it as an oracle:

1. Generate the best AST and `toInfinit` string.
2. Ask the Core: `bool validate(const char* expr, char** errorOut)` (injected callback; the scanner never links the Solver).
3. If invalid, **re-rank** with a bounded search over alternatives:
   - Swap ambiguous symbols using their top-k lists (`l ↔ 1`, `O ↔ 0`, `x ↔ ×`).
   - Flip relation decisions that had small margins (superscript ↔ right).
   - Add or remove a missing parenthesis only if the imbalance is exactly one and a candidate bracket symbol exists.
   - Toggle merges from Section 4.3.
4. Score each candidate: `cost = Σ symbol_distance + Σ relation_penalty + parse_penalty + domain_penalty`.
5. Cap the search (for example, max 5–8 ambiguous decision points, beam width 20, a 300 ms budget) and return the best candidates, not just one.

Also run steps 1–5 on each **binarization candidate** (Section 3.3) and keep the best total cost.

Important: **Parse success is not correctness.** A wrong but parseable expression is the dangerous failure. That is why Section 12 requires a visual confirmation.

Domain priors you can add cheaply (Infinit knows its domains): after `∫` expect `d?`; `y'`, `y''` appear in ODE problems; a `det` or matrix suggests linear algebra. Use them as small cost adjustments.

---

## 10. Output contract

```json
{
  "status": "ok | partial | failed | cancelled",
  "lines": [
    {
      "latex": "\\frac{x^2+1}{x-2}",
      "infinit": "(x^2+1)/(x-2)",
      "parse_valid": true,
      "confidence": 0.82,
      "symbols": [
        {
          "id": 0,
          "bbox": [x, y, w, h],
          "glyph": "x",
          "alternatives": [{"glyph": "×", "score": 0.61}],
          "confidence": 0.93,
          "relation": "numerator"
        }
      ],
      "warnings": ["low_margin_relation:id=7", "unknown_construct:id=12"]
    }
  ],
  "alternatives": [ { "latex": "...", "infinit": "...", "cost": 3.1 } ],
  "timing_ms": { "preprocess": 40, "segment": 12, "classify": 25, "structure": 8, "repair": 30 },
  "error": null
}
```

`confidence` here is a **heuristic score** you define (normalized cost), not a probability. Show it as "needs review" thresholds, not as a percentage of correctness.

Error states: empty image, no components found, over-segmentation (too many components), repair budget exceeded, invalid memory/image, cancelled.

---

## 11. Native integration (OpenCV, C++, FFI)

### 11.1 OpenCV build

- Only `core` and `imgproc` are needed. Write kNN/template matching yourself; avoid `ml`, `dnn`, `highgui`, `videoio`.
- For Android, prefer a **minimal OpenCV build** (a stripped build such as opencv-mobile, or your own build with only these modules) to keep APK size down. Verify license, version, and ABI support before adopting.
- Build for `arm64-v8a` first; add other ABIs only if you must.
- Desktop (Linux/Windows): use the system or vcpkg OpenCV via CMake. Keep the scanner library target identical across platforms.
- If you want to avoid writing JNI/FFI glue early, a Dart FFI OpenCV binding package (for example `opencv_dart`) can prototype the preprocessing from Dart. For production, a single C++ module called through one FFI boundary is cleaner and keeps the algorithm testable without Flutter.

### 11.2 FFI contract

```c
// C ABI, no exceptions crossing the boundary
typedef struct ScannerHandle ScannerHandle;

ScannerHandle* scanner_create(const char* config_json);
void           scanner_destroy(ScannerHandle*);

// Input: file path OR raw buffer (RGBA/gray, width, height, stride)
char* scanner_recognize_path(ScannerHandle*, const char* path, const char* options_json);
char* scanner_recognize_buffer(ScannerHandle*, const uint8_t* px, int w, int h, int stride,
                               const char* options_json);

void  scanner_cancel(ScannerHandle*);          // sets an atomic flag checked between stages
void  scanner_set_validator(ScannerHandle*, int (*validate)(const char* expr, char** err));
void  scanner_free_string(char*);              // caller frees every returned char*
```

Rules:

- The returned `char*` is JSON, allocated by native code, freed by `scanner_free_string`. Ownership must be explicit and documented.
- Catch every C++ exception at the boundary and return `status: "failed"` with a message.
- If Infinit's existing FFI uses a different pattern (for example direct C++ symbols), **follow that pattern** and avoid a second parallel bridge.
- Run recognition inside `Isolate.run(...)` (or a dedicated long-lived isolate). Never on the UI isolate.
- Pass the file path when you can; it avoids copying a large buffer through Dart.

### 11.3 Performance targets (set, then measure on a real mid-range phone)

- No model loading, so cold start should be small.
- Aim for under about 300–600 ms per candidate on a 1600 px image; measure peak RAM too.
- Limit stages: cap component count (for example 1500) and fail fast with a clear message on over-segmented images.

---

## 12. UI: symbol-level correction (this is what makes it usable)

Beyond the previous plan's text editor, add:

1. **Overlay view:** show the cropped image with colored boxes per symbol (green high confidence, amber low, red unknown), matching the Infinit interactive-diagram style.
2. **Tap a symbol:** a bottom sheet shows `alternatives` (for example `l`, `1`, `|`). Selecting one re-runs only the **structure + emit + repair** stages (fast, no vision).
3. **Tap a relation marker:** toggle "superscript / inline" or "fraction bar / minus".
4. **Merge/split:** allow the user to merge two boxes or split one; re-run from Segment onward for that region.
5. **Rendered preview + editable text:** keep the LaTeX preview and the Infinit-syntax editor; edits are validated against the Core parser live.
6. **Never auto-solve.** The user always confirms. Always keep manual typing available.

Because re-running later stages is cheap, correction feels instantaneous even though recognition is classical.

---

## 12A. Handwriting mode (ink input on an in-app canvas)

Photos of handwriting lose the most valuable information: **which strokes are which**. A canvas keeps it. This is the recommended path for handwriting, and it needs no model.

### 12A.1 One shared pipeline

Both input paths produce the same intermediate type, and everything from Lexing onward is shared:

```text
Photo path:  image → preprocess → components ──┐
                                               ├─► SymbolHypothesis[] ─► Lex → Structure → AST → Emit → Repair
Ink path:    strokes → grouping → recognition ─┘
```

```text
SymbolHypothesis { bbox, candidates[{glyph, score}], strokes?: [ids], source: image|ink }
```

Do not build a second structure analyzer. Only the front end differs.

### 12A.2 Ink capture (Flutter)

- `Listener` / `GestureDetector` with a `CustomPainter`; store each stroke as `[(x, y, t, pressure?)]`.
- Smooth lightly for display; keep raw points for recognition.
- Distinguish stylus from finger when the device reports it (palm rejection; optional "stylus only" setting).
- Tools: undo/redo, eraser, clear, pan/zoom of the writing area, grid or ruled guide (optional).
- Fixed-size writing area that scrolls horizontally, so long expressions do not get tiny.
- Autosave the in-progress ink so a backgrounded app does not lose it.

**Input devices (all produce the same stroke format):**

| Device | Behavior |
|---|---|
| Finger on Android/touch screen | **One finger draws, two fingers pan/zoom** (no mode switch needed). Optional "stylus only" setting turns finger into pan. |
| Stylus | Draws; pressure used only if reported; palm rejection while the stylus is in range |
| Mouse | Press and drag to draw |
| Laptop touchpad (Linux/Windows) | Flutter exposes it as a pointer, so it works as press-and-drag (or tap-and-drag). It moves a relative cursor, not an absolute position, so it is slower and less accurate than a screen. Offer a larger writing area and slightly stronger smoothing on desktop pointers, and mention in the UI that a touchscreen or stylus works best |

**Board layout:** a large scrollable board (not a small box) with multiple lines. Each line is recognized as a separate expression; a `{` spanning lines becomes a system (Section 7.6). Use finger pan to move around and keep a "fit all" button.

### 12A.3 Grouping strokes into symbols

This is the main algorithm in the ink path.

1. Build a **segmentation lattice** over the stroke sequence: for each position, hypothesize a symbol made of the next 1–4 strokes (in time order, so `=`, `i`, `x`, `+`, `÷`, `≤`, `∑` are covered).
2. Score each hypothesis with the symbol classifier (best candidate distance) plus a spatial plausibility term (strokes overlap or sit close relative to size) and a time-gap term (a long pause suggests a new symbol).
3. Pick the best segmentation with **dynamic programming** over the lattice (lowest total cost).
4. Late strokes are normal: a user adds the fraction bar, a prime, or the dot of `i` after writing the rest. So do not rely only on time order. Run the lattice over strokes sorted by time, then allow reassigning **small stray strokes** (dot, prime, bar) to the nearest plausible symbol by position.
5. Keep the second-best segmentation per region as an alternative for the repair stage and for the merge/split UI.

### 12A.4 Recognizing a symbol from strokes

- Normalize: translate and scale to the unit box, keeping aspect ratio as a feature; resample each stroke to a fixed number of points.
- Primary matcher: **$P-style point-cloud matching** (order- and direction-independent, good for multi-stroke symbols). Optionally add stroke-order-sensitive matching as a secondary score for symbols where order is stable (digits).
- Extra features: number of strokes, start/end points, direction histogram, closed-loop flags (`0`, `6`, `8`, `9`, `o`).
- Vote with **kNN over prototypes**, return top-k with scores, same as the image path.
- Typography features go to the shared stages: size, vertical position relative to the baseline (needed for scripts, primes, and fraction bars).

### 12A.5 Prototypes: bundled and personal

| Source | Role |
|---|---|
| Bundled prototype set | Offline starter set built once from permitted datasets (for example online handwriting data from CROHME-style sets; check licenses) and your own samples |
| **User prototypes** | Every time the user corrects a symbol in the UI, store the strokes as a new prototype for that class, locally |
| Optional calibration | A 1–2 minute "write each symbol once or twice" screen (about 80–100 symbols); skippable, repeatable |

Rules:

- Cap prototypes per class and prune near-duplicates so matching stays fast and memory small.
- Weight user prototypes higher than bundled ones.
- Store them in a local file; provide **export/import/reset**. Nothing leaves the device.
- Prototype files are versioned data, not a model; loading is a plain file read.

### 12A.6 Live recognition UX

- Recognize **incrementally**: after a short pause (tunable debounce) run grouping + structure on the whole ink; update the rendered LaTeX preview live.
- Run it in a native worker or Isolate; cancel the previous run when new ink arrives.
- Display recognized symbols as faint overlays on the ink (same color code as the photo overlay: green/amber/red).
- Tap a symbol or group → alternatives sheet (same component as the photo path) → re-run only the later stages.
- **Gestures:** scratch-out over symbols to erase; lasso to select and move/delete; tap-and-hold to merge/split groups.
- If structure is ambiguous (fraction bar vs minus, script vs inline), show the small toggle marker from Section 12.

### 12A.7 Making neat handwriting more recognizable (UX guidance, not code)

- Show a short hint at first use: "write one symbol at a time, leave a small gap, write fractions as bar then top/bottom."
- Domain-constrained alphabets (Section 5.4) apply here too; in an ODE context prefer `y`, `x`, `'`.
- Optional "digits and basic operators" quick-keyboard next to the canvas for hard-to-write symbols (`∂`, `∑`, `∫`) so they can be tapped instead of drawn.

### 12A.8 Photo handwriting (experimental, later)

Reconstruct strokes from a photo via thinning (skeleton), junction splitting and path tracing, then feed the same ink recognizer. Expect much weaker results than live ink (touching strokes, shared junctions). Ship only after the ink path is solid, behind an "experimental" label.

### 12A.9 Testing the ink path

- Unit tests: lattice segmentation on `=`, `i`, `x`, `+`, `÷`, `≤`; late dot/prime/bar assignment; $P matcher on rotated/scaled samples.
- Dataset: online handwritten math (InkML-style data, such as the CROHME family; verify license) gives ground truth for both symbols and expression structure. Report symbol accuracy, segmentation accuracy, and expression rate.
- Real users: collect a small set of local writing samples from several people (with consent) to avoid overfitting to a dataset's style.
- Performance: end-to-end latency after the last stroke, measured on a mid-range phone; target a feel of "instant" for short expressions.

---

## 12B. UI specification

Applies to Flutter on Android first; desktop (Linux/Windows) uses the same screens with the adaptive layout in 12B.9. Follow the app's existing theme, navigation, and state-management choices; the names below are suggestions, not new dependencies.

### 12B.1 Principles

1. **Never a dead end.** Every screen has a visible way to retry, go back, or type manually.
2. **The user confirms.** Nothing is solved until the user taps Solve on a reviewed expression.
3. **Same result screen for all inputs** (photo, gallery, handwriting). Only the left/top "source" panel differs.
4. **Math is always left-to-right**, even in a Persian (RTL) UI. Wrap every math widget (rendered LaTeX, expression editor, overlay) in `Directionality(textDirection: TextDirection.ltr)`, and keep surrounding labels RTL/LTR according to the app locale.
5. **Scanner is independent of feature pages.** It returns a typed result; Calculus, Linear Algebra, ODE pages never import scanner internals.

### 12B.2 Entry point and flow

An "Input" control (icon button or FAB) on any page that accepts an expression opens a bottom sheet:

```text
[ Camera ]   [ Gallery ]   [ Handwrite ]   [ Type ]
```

```text
Entry sheet
  ├─ Camera ──► S2 Camera ─► S3 Review & Crop ─► S4 Processing ─┐
  ├─ Gallery ─► (system picker, no storage permission) ─► S3 ───┤
  ├─ Handwrite ─► S6 Board ──(auto, debounced)──────────────────┤
  └─ Type ──► existing keyboard input                           ▼
                                                      S5 Result & Correction
                                                               │ Solve
                                                               ▼
                                        caller receives ScanConfirmed{ infinit, latex, lines[] }
```

Back behavior: S5 → S3 ("Crop again") or S2 ("Retake") or S6 ("Edit ink"); never loses the original image or ink until the flow is cancelled or confirmed. Cancel asks for confirmation only if the user has corrected something.

### 12B.3 State machine (controller)

```text
Idle → CameraReady → Captured → Cropping → Processing → Result(valid | needsReview | failed)
                                                   ↘ Cancelled
Board: Drawing → Recognizing → Result(...)
```

- One `ScannerFlowController` (ChangeNotifier/Bloc/Riverpod, whichever the app already uses) owns state, the source (image path or ink), and the latest native result.
- Sealed state classes; the UI is a pure function of state.
- Native calls run in an isolate; each new request cancels the previous one.

### 12B.4 S2 — Camera

- Full-screen preview with a rounded **framing guide** (aspect about 3:1 to 2:1) and hint text.
- Top bar: close, flash (off/auto/on, only if supported).
- Bottom bar: gallery shortcut (left), **shutter** (center), spare slot (right, empty in v1).
- Gestures: pinch to zoom (with a small zoom label), **tap to focus/expose** with a focus ring.
- Capture: disable shutter while capturing; short shutter animation; haptic tick.
- Permission states: first request (explain why), denied (explain + "Open settings"), permanently denied (settings only), camera unavailable (use Gallery or Handwrite instead).
- Lifecycle: release camera on pause, reinitialize on resume with a visible preview-loading state.

### 12B.5 S3 — Review & Crop

- Shows the captured image fitted to screen, with **four draggable corner handles** (hit target at least 48 dp, visible dot smaller) and a connecting quadrilateral; the outside area is dimmed.
- **Magnifier loupe** near the finger while dragging a handle, so corners can be placed precisely.
- Toolbar: rotate 90°, reset crop, brightness/contrast (optional, v1.5).
- Bottom actions: **Retake** (secondary), **Recognize** (primary).
- Quality hints (after capture, before recognition): "Image looks blurry", "Image is dark", "Text is very small, move closer". Hints are suggestions with a "Retake" button and a "Continue anyway" option.

### 12B.6 S4 — Processing

- Stage stepper with plain text: "Preparing image", "Finding symbols", "Recognizing", "Checking expression".
- **Cancel** button always enabled; **Enter manually** link.
- Subtle animation only (stepper progress); no spinners that hide which stage is running.
- If it takes longer than the time budget, show "This is taking longer than usual" with cancel.

### 12B.7 S5 — Result and Correction (the key screen)

Layout (portrait phone, top to bottom):

1. **Source panel:** cropped image (or ink) with the symbol overlay; pinch to zoom; overlay toggle.
2. **Status chip:** `Valid` / `Needs review` / `Could not read`.
3. **Rendered expression** (LaTeX), large, horizontally scrollable if long.
4. **Expression editor:** Infinit-syntax text field with the app's math keyboard; live validation under the field (parse error text with the position highlighted).
5. **Action bar:** Retake · Crop again · **Solve**.

Overlay behavior:

| Confidence | Color | Non-color cue |
|---|---|---|
| High | green | thin solid border |
| Low | amber | dashed border + "?" badge |
| Unknown / failed | red | thick border + "!" badge |

Never rely on color alone (color-blind users): borders and badges are mandatory.

Interactions:

- **Tap a symbol:** bottom sheet with the top-k alternatives (as tappable chips, shown rendered) plus "Type a different symbol". Selecting one re-runs only structure/emit/repair and updates preview and editor within a short time.
- **Tap a relation marker** (small chip between symbols where the decision had a low margin): toggle "superscript ↔ inline", "fraction bar ↔ minus".
- **Merge/split:** long-press a box; choose Merge with neighbor or Split.
- **Alternative expressions:** if `alternatives[]` has entries close in cost, show "Other readings" as a collapsible list under the rendered expression.
- **Editing text** in the editor marks the result as "edited by user" and keeps overlay as reference only; the overlay stops claiming to match.

Multi-line results:

- Each line is a card with its own rendered preview, status, and delete/reorder handle. A system (`{` brace) is shown as one grouped card.
- **Solve is enabled only when all selected lines are valid.** If any line is invalid, Solve is disabled and the line shows the parse error; the user can delete the line or edit it.
- Persistent notice: "Check the expression before solving; recognition can be wrong."

### 12B.8 S6 — Handwriting board

Layout:

- **Board** (maximum area): scrollable/pannable canvas with optional light guide lines.
- **Live preview strip** pinned above or below the board: rendered LaTeX of the recognized expression + status chip. Tapping it opens S5-style correction (bottom sheet on phone, side panel on tablet/desktop).
- **Toolbar:** pen, eraser, undo, redo, clear, fit-all, symbol keyboard toggle, settings (stylus only, guide lines, recognition delay).
- **Primary button:** "Use expression" (becomes enabled when the expression is valid or the user confirms editing).

Gestures:

| Gesture | Action |
|---|---|
| One finger / stylus / mouse drag | Draw |
| Two fingers | Pan and zoom the board |
| Scratch-out zigzag over symbols | Erase those symbols |
| Long-press a recognized group | Merge/split/delete menu |
| Tap overlay symbol | Alternatives sheet |

Behavior:

- Recognition runs after a short pause (user-tunable, default about 0.5 s); the previous run is cancelled when new ink arrives.
- Ink is displayed immediately and never waits for recognition (drawing must feel instantaneous).
- Symbol overlay fades in lightly over the ink; low-confidence groups get the amber/red treatment from 12B.7.
- **Symbol quick keyboard** (collapsible): `∫ ∑ ∏ ∂ √ π θ ≤ ≥ ≠ ± ( ) | { }` and digits; tapping inserts a **typed symbol hypothesis** at the next position (with perfect confidence) for symbols that are hard to draw.
- First-use hint card: "Write one symbol at a time with a small gap. Write fractions as a bar, then top and bottom."
- Autosave ink; on resume restore the board and recognition state.

### 12B.9 Adaptive layouts

| Window | Layout |
|---|---|
| Phone portrait | Single column as described |
| Phone landscape | Source panel on the left (60%), result/editor on the right (40%); camera controls move to the right edge |
| Tablet / desktop | Persistent two-pane: source (image or board) left, result panel right; correction sheets become side panels or popovers; keyboard shortcuts: `Ctrl+Z`/`Ctrl+Y` undo/redo, `Enter` solve, `Esc` cancel |

### 12B.10 Copy (English keys; translate for the app locale)

| Key | English |
|---|---|
| `scan.hint.frame` | Place the problem inside the frame |
| `scan.hint.blur` | The image looks blurry. Try again with steadier hands |
| `scan.hint.dark` | The image is dark. Turn on the flash or move to better light |
| `scan.hint.small` | The text is small. Move closer or crop tighter |
| `scan.stage.prepare` | Preparing image |
| `scan.stage.symbols` | Finding symbols |
| `scan.stage.recognize` | Recognizing |
| `scan.stage.check` | Checking expression |
| `scan.result.valid` | Looks valid |
| `scan.result.review` | Needs review |
| `scan.result.failed` | Could not read this |
| `scan.result.warning` | Check the expression before solving. Recognition can be wrong |
| `scan.action.manual` | Enter manually |
| `scan.error.permission` | Camera access is needed to scan. You can also use Gallery or Handwrite |

### 12B.11 Accessibility and localization

- Minimum touch target 48 dp; handles and chips included.
- `Semantics` labels on every control; announce "Expression recognized: <spoken form>" and status changes (screen readers). Provide a plain-text spoken form of the math, not LaTeX source.
- Respect text scaling and reduce-motion settings; no essential information in animation.
- Color is never the only signal (12B.7). Check contrast in light and dark themes.
- Test with both Persian and English locales: RTL chrome, LTR math, correct bidi in mixed labels.

### 12B.12 Flutter implementation notes

- **Ink canvas:** `CustomPainter` with `RepaintBoundary`; keep a version counter so `shouldRepaint` is cheap; cache finished strokes into a `ui.Picture` and draw only the active stroke live.
- **Image display:** use cached, downscaled `ImageProvider` for the preview (`cacheWidth`); never hold the full-resolution image in several places.
- **Crop handles:** draw the quad and dim mask in one `CustomPainter`; hit-test handles with the 48 dp radius, not the drawn dot.
- **Overlay:** compute overlay rectangles in the image's coordinate space and map through the same transform as the zoomed image, so boxes stay aligned while zooming.
- **Responsiveness:** drawing and gestures must not wait on isolates; all recognition results arrive via streams/futures and update state without blocking the canvas.
- **Navigation:** implement as a nested flow returning `ScanConfirmed?` (null on cancel); avoid pushing feature pages from inside the scanner.

### 12B.13 UI testing

- **Widget/golden tests** for each screen state: camera permission states, crop handles, processing stages, result valid/needs-review/failed, multi-line with one invalid line, board with overlay, RTL and LTR, light and dark, large text.
- **Interaction tests:** change a symbol via alternatives updates preview and editor; invalid edit disables Solve; two-finger gesture pans without drawing; cancel mid-processing returns to the previous screen without leaks.
- **Device checks:** camera lifecycle (background/foreground), low-memory device, stylus vs finger, tablet and desktop layouts.

---

## 12C. Image import (gallery, files, drag-and-drop, share)

Import is a second source for the same flow: **Import → S3 Review & Crop → S4 → S5**. Everything stays on the device; no upload, no network request (see 12C.5).

### 12C.1 Entry points

| Platform | Ways to import |
|---|---|
| Android | Gallery button on the camera screen and in the entry sheet (system photo picker); optional **Share → Infinit** from other apps (v2) |
| iOS (if supported later) | System photo picker; Share extension (v2) |
| Linux / Windows | File picker, **drag-and-drop** onto the scanner window, **paste from clipboard** (`Ctrl+V`) |

Use the system photo picker where it exists: on current Android versions it needs no storage permission, and the app only receives the image the user chose.

### 12C.2 Supported inputs

| Input | Handling |
|---|---|
| JPEG, PNG, WebP | Decode directly |
| HEIC/HEIF (common on iPhones and some Android phones) | OpenCV cannot decode it. Decode with the platform codec (Flutter's image decoding) and pass **raw pixels** through `scanner_recognize_buffer`, or re-encode to PNG at import |
| Screenshots and PDF exports | Allowed; handled by the "clean digital" profile below |
| Multi-image selection | Not supported in v1: one image per scan. Selecting several shows "Choose one image" |
| PDF files | Out of scope for v1; consider rendering one page as an image in v2 |

### 12C.3 Import pipeline (before the native scanner)

1. **Copy** the picked image into the app's temporary directory (never read directly from a shared URI later).
2. **Validate:** non-empty, decodable, reasonable dimensions; reject with a clear message otherwise ("This file is not a supported image").
3. **Normalize orientation once** (apply EXIF rotation, then strip it) so the crop screen and the native code agree on the pixel layout.
4. **Downscale on decode** to the working long side (about 1600–2000 px); keep the original file untouched for the "Crop again" path. Huge photos must not be fully decoded into memory.
5. **Classify the image** with cheap statistics to choose a preprocessing profile:

| Profile | Detected when | Differences in the pipeline |
|---|---|---|
| Photo (default) | Noise, gradients, uneven background | Full Section 3 pipeline: illumination correction, 3 binarizations, deskew |
| Clean digital | Few distinct colors, flat background, sharp edges (screenshots, rendered PDFs) | Skip illumination correction and deskew; one Otsu binarization; preserve thin strokes |
| Low resolution | Median symbol height too small | Show the "text is small" hint; offer upscaling (bicubic) before segmentation |

6. Continue to S3 (crop) so the user can isolate one problem from a bigger page.

### 12C.4 UI behavior

- Gallery button is always visible on S2 and in the entry sheet; on desktop the scanner window shows a drop zone ("Drop an image here, or choose a file").
- Importing a very large image shows a short loading state with cancel.
- A cropped region from a gallery image uses the same 4-corner crop with magnifier loupe as camera captures (Section 12B.5).
- Errors: unsupported format, unreadable file, image too small, out of memory (suggest cropping a smaller part); each has a retry or choose-another action.

### 12C.5 Privacy and offline

- Import never needs the internet. The one exception is outside the app: if the user picks an image that exists only in a cloud gallery and is not yet on the device, the operating system may need a connection to download it before handing it over.
- The scanner module must not request the network permission (Section 11).
- Temporary copies are deleted when the flow ends (confirm, cancel, or app restart cleanup). Keep a debug-only exception behind an explicit opt-in.
- Do not log file paths, EXIF location data, or image content. Strip EXIF (including GPS) when making the working copy.

### 12C.6 Tests

- Import JPEG, PNG, WebP, HEIC, screenshot, and a corrupted file; expected result and message for each.
- Rotated EXIF images (all 8 orientations) display and crop correctly.
- 12 MP and larger images do not crash and stay within the memory budget.
- Clean-digital detection chooses the faster profile and preserves thin fraction bars.
- Airplane-mode run of the complete import flow.
- Desktop: drag-and-drop, paste, and file picker all reach S3.

---

## 13. Testing and data (no training required, but still essential)

### 13.1 Synthetic dataset generation

You do not need to photograph hundreds of problems first.

1. Take a list of ground-truth expressions covering all of Infinit's domains (precalculus, calculus, linear algebra, ODEs, discrete math).
2. Render them to images using the **same renderer the app uses**, in several fonts and sizes.
3. Apply augmentations: rotation (±8°), perspective, Gaussian blur, noise, JPEG compression, uneven lighting/shadows, paper texture, ruled lines, low resolution.
4. Ground truth comes for free (the source expression and per-symbol boxes).

Then add 50–100 **real photos** (permitted, no personal data) with manual ground truth to avoid overfitting to synthetic data.

### 13.2 Per-stage metrics (so you know *what* to fix)

| Stage | Metric |
|---|---|
| Preprocess | Foreground F1 vs. clean render |
| Segmentation | Symbol recall/precision, merge/split error rate |
| Classification | Top-1 and top-3 accuracy per class, confusion matrix |
| Structure | Relation accuracy; tree edit distance |
| End-to-end | Exact expression match, parser acceptance, solver success, correction taps per expression |
| Resources | Latency per stage, peak RAM |

Do not tune the whole pipeline on end-to-end accuracy alone; you will not know which stage is hurting.

### 13.3 Unit tests

- Classifier: one test per symbol class and font.
- Segmentation: `i`, `=`, `÷`, `≤`, `%`, touching digits.
- Structure: `\frac{x^2+1}{x-2}`, `\sqrt{x^2+1}`, nested fractions, `\sin^2(x)`, `y''-5y'+6y=0`, `\int_0^1 x^2\,dx`, `\lim_{x\to0}`, 2×2 and 3×3 matrices, systems.
- Printers: AST → both syntaxes, including implicit multiplication and parenthesization.
- Repair: known-confusable inputs (`l` vs `1`, missing parenthesis).

### 13.4 Debug tooling (strongly recommended)

Build a small desktop **"Scanner Lab"** (Flutter on Linux/Windows) that loads one image and lets you step through stages: original → normalized → each binarization → components with ids → baselines/relations → AST → LaTeX. Every bug in a classical pipeline is a visual bug; this tool will repay its cost in days.

---

## 14. Milestones

| # | Milestone | Key outputs |
|---|---|---|
| M0 | Audit | Core parser grammar, FFI pattern, Flutter/Android versions, baseline build |
| M1 | Camera + Capture + Review (Flutter) | First PR; no OpenCV yet |
| M2 | Crop with 4-corner perspective warp, EXIF, temp-file lifecycle | |
| M2b | Image import (Section 12C): gallery picker, HEIC handling, downscale-on-decode, desktop file/drag-drop/paste | Import → crop path works offline |
| M3 | Native scaffold | C++ module, CMake/NDK build, FFI hello-world, Isolate call, cancel flag |
| M4 | Preprocess | Normalization, 3 binarizations, deskew, cleaning; debug images |
| M5 | Segmentation | Components, merge rules, lines, scale normalization |
| M6 | Template classifier (Tier A) | Font-rendered templates, features, top-k |
| M7 | Lexer + AST + printers | Numbers, function names, both emitters, tests |
| M8 | Structure analysis | Fractions, scripts, radicals first; then big operators, matrices, systems |
| M9 | Parser-guided repair | Validator callback, beam search, cost function |
| M10 | Scanner result UI | Overlay, alternatives sheet, editors, confirm |
| M11 | Quality pass | Dataset, metrics, device tests, performance, accessibility |
| H1 | Ink canvas (Flutter) | Stroke capture, undo/erase, autosave, palm rejection |
| H2 | Stroke grouping | Segmentation lattice + DP, late-stroke reassignment |
| H3 | Ink symbol recognizer | $P-style matcher, kNN over bundled prototypes, top-k |
| H4 | Shared pipeline hookup | `SymbolHypothesis` from ink into Lex → Structure → AST → Repair |
| H5 | Live UX | Incremental recognition, overlay, alternatives sheet, gestures |
| H6 | Personalization | User prototypes, calibration screen, export/reset |
| H7 (experimental) | Photo handwriting via stroke reconstruction | Section 12A.8 |

Suggested PR order: M1 → M2 → M3 → M4/M5 → M6/M7 → M8 (printed linear/fractions/scripts only) → M9 → M10 → extend M8 (big operators, matrices) → H1–H6 (ink path; can start after M7, since it only needs the shared stages) → M11. If handwriting matters more to you than photos, H1–H4 can come right after M8's first slice.

---

## 15. Definition of done (v1)

- [x] Dedicated in-app camera implemented (Camera2 preview, permissions, flash,
      pinch zoom, tap-to-focus, lifecycle, retake). Not verified on a real
      device from this environment; verified by build plus widget tests.
- [x] Crop with perspective correction; the original image is never destroyed.
- [x] Image import from gallery and, on desktop, file picker, drag-and-drop and
      paste; HEIC/HEIF via the platform codec; no temporary copy is written.
- [~] Printed single-line expressions with fractions, powers, roots, standard
      functions and primes ride the correction path. Measured on the synthetic
      set: top-1 **0.946**, top-3 0.982, exact match 7/12, parser acceptance
      81.8 %. The weak spots are named in `scanner/dataset/metrics.md`.
- [x] Both LaTeX and Infinit-syntax outputs are produced from the AST.
- [x] Parser-guided repair active; results re-ranked by cost.
- [x] Symbol-level correction UI works without re-running vision.
- [x] Heavy work runs off the UI isolate and can be cancelled.
- [x] No images leave the device (everything is local).
- [x] Synthetic test set with per-stage metrics recorded. A real-photo set is
      still missing: it needs photographs, which cannot be produced here.
- [x] Unknown constructs are reported, never silently dropped.
- [x] (Handwriting) Ink canvas recognizes neat handwritten single-line
      expressions live, shares the structure/repair/correction pipeline, and
      learns from user corrections locally (stroke prototypes + calibration).
- [~] §17.3 auto-capture: deliberately last in the plan ("later, not now") and
      still unbuilt; the blur, dark and small-text checks it depends on are done.
- [~] §12A.8 photo handwriting exists as an experimental path with its own
      tracer tests; it is not exposed in the normal flow because its measured
      accuracy on drawn glyphs is not good enough yet.

---

## 16. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Handwriting accuracy too low without ML | Scope v1 to printed; Tier B optional; correction UI; `ISymbolClassifier` seam |
| Touching/broken symbols | Keep alternative segmentations; repair stage retries; merge/split UI |
| Wrong-but-parseable output | Mandatory visual confirmation; show rendered LaTeX next to the image |
| Threshold-tuning fragility | Scale-relative thresholds; 2–3 binarization candidates; tune on synthetic + real sets |
| OpenCV size/ABI problems on Android | Minimal build; arm64 first; M3 proves the toolchain before any algorithm work |
| Matrix/integral structure complexity | Ship printed linear/fraction/script support first; add big operators and matrices as separate milestones |

---

## 17. Extra tips

1. **Resolution matters more than filters.** Ask the user to fill the frame with the problem; show a framing guide. Warn if the estimated median symbol height is too small (for example under ~20 px) and suggest moving closer.
2. **Blur detection before processing:** variance of the Laplacian; if below a threshold, prompt a retake instead of producing garbage.
3. **Auto-capture later, not now:** once blur and framing checks exist, auto-capture is a small step.
4. **Keep Persian/English separation:** if problems contain Persian text, treat it as a separate "non-math text region" (skip it or just show it) rather than forcing it into the math recognizer.
5. **Cache per image:** keep component and feature data in the native handle so symbol corrections re-run only the cheap later stages.
6. **Log failures locally (opt-in):** a "report bad scan" button that saves the cropped image and the stage outputs to a local folder helps you grow the test set.
7. **Plan numeric/units conventions with the Core:** decimal comma vs point, `×` vs `*`, `÷` vs `/`, and `^` vs superscript must match the Core parser exactly.

---

## References to verify before adopting

- OpenCV (`core`, `imgproc`): https://opencv.org/ (check license and Android build options)
- Flutter FFI: https://docs.flutter.dev/platform-integration/ffi
- Flutter `camera` package: https://pub.dev/packages/camera
- Classical math-expression recognition: Zanibbi, Blostein, Cordy, "Recognizing Mathematical Expressions Using Tree Transformation" (baseline structure tree)
- Handwritten math datasets for evaluation/Tier B: CROHME; symbol datasets such as HASYv2 (check licenses)

---

## 18. Implementation status (2026-11)

This section records what the repository actually ships today, so the plan above
stays a design document rather than a claim.

| Plan area | State | Where |
|---|---|---|
| §3 Preprocess: illumination flattening, Otsu/Sauvola/adaptive, deskew, cleaning | done | `scanner/src/preprocess.cpp` |
| §4 Segmentation: components, merge rules, lines, and the §4.3.1 projection-valley split for touching glyphs (guarded by density, full height and symmetry so an "m" or a "lim" is never cut) | done | `scanner/src/segment.cpp` |
| §5 Classification: 16×16 template cosine kNN, score and margin, top-3 alternatives, 65 classes over nine faces (upright and math italic), including `× ÷ ≤ ≥ ≠ ± %` | done | `scanner/src/classify.cpp`, `scanner/src/glyphs.cpp` |
| §4.3 multi-part symbols: `i`/`j` dots, `=`, and `÷` (dot–bar–dot, merged only when both dots are present so a fraction bar is never eaten) | done | `scanner/src/segment.cpp` |
| §6–7 Structure: fractions, roots, scripts, sums, integrals, limits, systems, matrices | done | `scanner/src/structure.cpp` |
| §8 Emitters: Infinit syntax **and** LaTeX from the same tree | done | `structure.cpp` (`renderStructure`, `renderLatex`) |
| §9 Repair: native bounded beam search over top-k labels, cost model, pinned manual fixes, kind guard | done | `scanner/src/repair.cpp`, `repair.hpp`, `scanner_set_validator` |
| §10 Contract: `status`, `lines`/`lineDetails`, `parse_valid`, `alternatives`, `timing_ms`, `error` | done | `scanner/src/scanner_api.cpp` |
| §11 FFI: reading in an isolate, session cache, no exceptions across the boundary | done | `scanner_api.cpp`, `app/lib/src/scanner_engine.dart` |
| §12 Correction: overlay boxes, per-symbol alternatives, accumulated fixes, merge/split, relation toggles (superscript ↔ inline, fraction bar ↔ minus), per-line cards, 48 dp hit targets | done | `app/lib/src/scanner/scan_flow.dart`, `scanner_api.cpp` (`scanner_session_merge/split/relation`), `scanner/src/structure.cpp` |
| §12A Handwriting: stroke capture, DP segmentation lattice over 1–4 strokes, late dot/prime/bar reassignment, shared classifier/structure/repair | done | `scanner/src/ink.cpp`, `app/lib/src/scanner/board.dart` |
| §12A.4/12A.5 $P point-cloud matching and **personal stroke prototypes** (strokes, not rasters), capped per class with near-duplicate pruning, export/import/reset, learned on every symbol correction | done | `scanner/src/cloud.cpp`, `scanner/src/inkproto.cpp`, `app/lib/src/scanner/board.dart`, `app/lib/src/scanner/prototype_store.dart` |
| §12A.5 calibration screen (write each symbol once, ~55 classes, skippable and repeatable) | done | `app/lib/src/scanner/calibration_page.dart` |
| §12A.8 photo handwriting: Zhang-Suen thinning, junction cutting, branch tracing, collinear joining | done as an **experimental** path, exposed only in the Scanner Lab | `scanner/src/trace.cpp`, `scanner_read_handwriting`, `app/lib/src/scanner/lab_page.dart` |
| §12A.2/12A.6 Board: undo/redo, autosave, symbol quick keyboard, stylus-only (palm rejection), guide lines, fit-all, one-finger draw with two-finger pan/zoom, scratch-out erase, lasso delete | done | `app/lib/src/scanner/board.dart` |
| §13.4 Scanner Lab: every pipeline stage as an image plus the reading, timings and symbol table, in-app | done | `app/lib/src/scanner/lab_page.dart`, `scanner_debug_stage` in `scanner/src/scanner_api.cpp` |
| §5.1 Features: 16×16 correlation, zoning, hole count, aspect, projection profiles, crossing counts, log-Hu moments | done | `scanner/src/classify.cpp`, `scanner/src/glyphs.cpp` |
| §12B Entry sheet, crop, processing (cancellable), result, adaptive labels, per-line review, live parser hint, retake | done | `app/lib/src/scanner/scan_flow.dart` |
| §12B.5/§12B.6/§17.2 Quality hints (blur, dark, small text) with Retake and Continue, real stage stepper driven by the native stage counter, "taking longer than usual" | done | `perspective.dart`, `scan_flow.dart`, `scanner_stage` in `scanner/src/scanner_api.cpp` |
| §12B.9 Adaptive layouts: two-pane source/result on wide windows, `Esc` and `Ctrl+Enter` shortcuts | done | `scan_flow.dart`, `lab_page.dart` |
| §12C Import: gallery/files, HEIC via the platform codec, clean-digital/low-resolution profiles; in-app Camera2 preview on Android with a system-camera fallback; Linux and Windows drag-and-drop plus Ctrl+V paste | done | `camera.dart`, `camera_page.dart`, `android/.../CameraPreview.kt`, `scan_flow.dart`, `normalize.dart`, `linux/runner/my_application.cc`, `windows/runner/flutter_window.cpp` |
| Matrix entry: rows × columns grid for every matrix field | done | `app/lib/src/scanner/matrix_input.dart`, `topic_workbench_page.dart` |
| §13 Tests: synthetic samples, per-stage unit tests, pipeline tests, ink grouping, stroke tracing and session edits | done; matrices covered by `matrix.png` and `matrix_photo.png` | `scanner/tests/`, `app/test/` |
| §13.1 Synthetic dataset: seeded generator, five math fontsets, ten augmentation recipes, pixel-accurate ground-truth boxes, byte-reproducible | done | `scanner/tools/make_dataset.py` |
| §13.2 Per-stage metrics: foreground F1, symbol recall/precision, top-1/top-3 per class, confusion matrix, exact match, parser acceptance, per-stage latency | done | `scanner/tools/metrics.py`, report in `scanner/dataset/metrics.md` |
| §13.4 Scanner Lab: every pipeline stage as an image plus the reading, timings and symbol table, in-app | done | `app/lib/src/scanner/lab_page.dart`, `scanner_debug_stage` in `scanner/src/scanner_api.cpp` |
| §12B.11 Accessibility: 48 dp targets, spoken expression announcements, colour is never the only signal | done | `scan_flow.dart`, `board.dart` |

Deviations from the document, on purpose:

- **Camera** is a real in-app preview on Android (`Camera2` +
  `SurfaceProducer`, no camera package, no Google Maven) with the framing
  guide, flash toggle, shutter, pinch-to-zoom (sensor crop on the native side),
  tap-to-focus with a focus ring, lifecycle release and capture rotation. The
  system `ACTION_IMAGE_CAPTURE` intent remains as a one-tap fallback inside that
  page. Auto-capture (§17.3) is the one camera step still unbuilt: the plan puts
  it after the quality hints, which now exist.
- **Handwriting** keeps the raw strokes and groups them in C++ (`ink.cpp`) with
  a DP segmentation lattice over 1–4 strokes plus late-stroke reassignment,
  then reuses the photo classifier, structure and repair stages on a rendered
  bitmap. The `$P` point-cloud matcher runs twice: as a third opinion over the
  raster templates (`cloud.cpp`) and over the personal **stroke** bank
  (`inkproto.cpp`), which stores strokes and is fed by every symbol the user
  fixes on the board. Typed quick-key symbols still go through the bitmap path
  because they are text, not strokes. There is no separate calibration screen
  yet: the plan marks it optional and the corrections already produce samples.
- **Privacy (§12C.5).** Import never writes a temporary copy: the picked file
  is decoded in memory and re-encoded to PNG, which drops EXIF (GPS included)
  before any pixel reaches the native side. The one temp file in the product is
  the camera capture, and it is deleted immediately after the bytes are read.
  Nothing is logged and nothing leaves the device.
- **Remaining measured weaknesses.** On the synthetic set the `cm` (Computer
  Modern) fontset still scores 0/2 exact, a rotated *clean* render is read
  worse than an upright one, and ruled paper is only partially recovered (3/6
  exact on a dedicated ruled set). These are recorded in
  `scanner/dataset/metrics.md` rather than papered over.
- **Matrices** are entered as a grid in the app, but the core contract is still
  the documented `a, b; c, d` text. The grid only assembles that string, so the
  workbench and the core did not need a new input type.
- **Repair scope.** The Core parser is the oracle only for readings whose kind
  is `expression`. The `lim(...)`, `integral(...)`, `sum(...)`, `system` and
  `matrix` strings are presentation syntax the parser does not accept, so
  validating them would "repair" a correct reading into a parseable but wrong
  one. Repair is also barred from changing the structure kind and from spending
  more than `maxSymbolCost` (0.75), so a valid-but-degenerate candidate cannot
  win. Glyphs the user fixed are pinned and never revisited.
- **Desktop import** is implemented for Linux (GTK) and Windows (Win32
  `WM_DROPFILES`, `CF_DIB`/`CF_HDROP` clipboard). This repository has no macOS
  runner, so macOS was not added.
- **Crop** ships the four corner handles, the 2.5× loupe, the perspective warp,
  a 90° rotate button, a brightness/contrast dialog (applied to the upright
  original, never compounded), quality hints (blur, dark, small text) with
  Retake and Continue, and a Retake action.
- **Camera extras.** Auto-capture is not built: §17.3 puts it after the blur and
  framing checks, which do exist, and calls it optional for now. Everything else
  in §12B.4 is built, and the shutter carries an explicit semantics label and
  tooltip (§12B.11).
- **Personal prototypes** are two banks: 16×16 rasters (photo path) and stroke
  point clouds (handwriting path). Both are capped and exported side by side as
  `…-samples.bin` and `…-samples-ink.bin`; a correction on the board feeds the
  stroke bank automatically. A guided calibration screen (write each symbol
  once) is the only part of §12A.5 left unbuilt, and the plan lists it as
  optional.
- **Crop screen retake.** Retake is offered on the result screen and reopens the
  same source; the crop screen itself only has Recognize and Back.
- **Parser verdict.** `parse_valid` is `null` for structured readings and for
  scans without an injected validator; the UI shows "needs review" instead of
  claiming a parse that never ran.

### Prior art decision

The classical engines reviewed (Seshat, LaTeX-OCR/pix2tex, texify, UniMERNet,
Tesseract, InftyReader) were rejected for one of three reasons: they are neural,
they are GPL-3.0 or proprietary, or they only do line OCR without math
structure. Details and links are in `scanner/README.md` under "Prior art". The
scanning pipeline here is original code; the only vendored dependency is
`stb_image` (public domain).

### Verification

- `ctest` in `scanner/build`: 8/8 (segment, structure, pipeline, OpenCV
  cross-check, repair, stroke tracing, ink DP grouping/relations/multi-line,
  plus the shared serializer checks).
- `core/build` tests: 266/266, including a malformed-expression block: `sqrt()`
  and eleven other broken strings are rejected instead of reading an empty
  argument list out of bounds.
- `flutter test --no-pub`: 356/356; `flutter analyze --no-pub`: clean. The suite
  covers the entry sheet, crop, processing, result, board toolbar and settings,
  calibration screen and Scanner Lab, in English and Persian.
- `flutter build linux --debug`: succeeds, including the desktop import channel.
- Android arm64 `libodecore.so` is rebuilt and staged; it exports
  `scanner_session_relation`, `scanner_read_handwriting`, `scanner_debug_stage`
  and the stroke-prototype entry points.
- `pytest`: 731/731, including the dataset/metrics smoke test.

### Measured quality (12-image synthetic set, seed 20260101)

| stage | metric | value |
|---|---|---|
| preprocess | foreground F1 | 0.971 |
| segmentation | recall / precision / F1 | 0.950 / 0.926 / 0.938 |
| classification | top-1 / top-3 | **0.946** / 0.982 |
| end-to-end | exact match | 7/12 (58.3 %) |
| end-to-end | parser acceptance | 9/12 (81.8 %) |
| resources | wall clock | 124 ms/image |

The instrument is what found the last round of bugs: a missing italic face for
math variables (`x` read as `?`/`m`), a superscript gate that rejected every
serif `x^2`, ruled paper being classified as a clean render, and a parser crash
on `sqrt()`. Two experiments were measured and **rejected**: Computer Modern
templates (top-1 fell from 0.889 to 0.861) and closing rule gaps with a long
horizontal kernel (it deleted real ink and broke three pipeline cases). The
numbers above are with both reverted.
