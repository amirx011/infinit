# Infinit Math Scanner

Photo of a printed math problem → text → the existing C++ core.

This module is the classical (no machine learning) half of the plan in
`docs/scanner/classical-plan.md`. It is a standalone CMake project with no
Flutter, no font and no image-library dependency at runtime: `stb_image` decodes
the input, everything else is hand-written.

## Status

| Stage | State |
|---|---|
| Decode (PNG/JPEG/BMP/TGA/GIF) | done |
| Illumination flattening, Otsu + Sauvola, skew search | done |
| 8-connected components, symbol grouping, line grouping | done |
| Template bank (9 faces including math italics, 65 classes, 342 templates) | done |
| Glyph classification (cosine kNN, margin rejection) | done |
| Structure: fractions, powers, radicals | done |
| Sums, integrals, limits, systems | done |
| Matrices (bracketed grid, rows/columns) | done |
| LaTeX printer from the same tree | done |
| Top-k alternatives and label overrides (repair) | done |
| Expression text and named parts for the app | done |
| Flutter flow: in-app Camera2 preview, gallery, board, 4-corner crop | done |
| Matrix grid for every matrix field in the app | done |
| Symbol overlay, correction sheet, merge/split, relation toggles | done |
| Per-line review cards for multi-line pages | done |

Accuracy on the committed samples, clean and rotated/blurred/noisy alike: 28 of
28 expressions read exactly, at 0.54–0.98 confidence. Covered inputs:

```
3x + 5 = 2x - 7              -> 3x+5=2x-7
x^2 - 5x + 6 = 0             -> x^2-5x+6=0
2^(x+1) = 8                  -> 2^(x+1)=8
(x+1)/(x-3) = 2              -> (x+1)/(x-3)=2
(1/x+1)/(x^2-4)              -> (1/x+1)/(x^2-4)
sqrt(x^2+9) = x-1            -> sqrt(x^2+9)=x-1
dy/dx = 3x^2-2x              -> (dy)/(dx)=3x^2-2x
sin(2x) = cos(x)             -> sin(2x)=cos(x)
ln(x+1)+ln(x-1) = 0          -> ln(x+1)+ln(x-1)=0
sum 1/n^2, n=1..inf          -> sum(1/(n^2), n=1, inf)
integral x^2 dx, 0..1        -> integral(x^2, x, 0, 1)
lim sin(x)/x, x->0           -> lim(sin(x)/x, x->0)
2x+y=5, x-y=1                -> 2x+y=5,x-y=1
[[1,2],[3,4]]                -> matrix 1, 2; 3, 4    (2x2, clean and photo)
```

Known limits: two glyphs that touch in the scan become one blob and are reported
as `?`; tilted matrices where a row is not horizontally aligned can lose a cell,
and the correction sheet is there for exactly that case.

## App integration

The scanner is compiled into the same `libodecore.so` the rest of the app uses,
so there is no second native library to ship. The exported C entry points are:

```c
int      scanner_load_templates(const unsigned char* data, size_t size);
char*    scanner_read(const unsigned char* data, size_t size);  /* caller frees */
char*    scanner_read_with_overrides(const unsigned char* data, size_t size,
                                     const char* overrides_json);
char*    scanner_read_ink(const unsigned char* data, size_t size);
char*    scanner_read_ink_with_overrides(const unsigned char* data, size_t size,
                                         const char* overrides_json);
uint64_t scanner_open_session(const unsigned char* data, size_t size);
uint64_t scanner_open_ink_session(const unsigned char* data, size_t size);
char*    scanner_session_read(uint64_t session);
char*    scanner_session_relabel(uint64_t session, const char* overrides_json);
char*    scanner_session_merge(uint64_t session, int first, int second);
char*    scanner_session_split(uint64_t session, int symbol);
char*    scanner_session_relation(uint64_t session, int symbol,
                                  const char* kind, const char* value);
void     scanner_close_session(uint64_t session);
void     scanner_free(char* text);
const char* scanner_version(void);
```

`scanner_session_relation` takes `kind = "script"` with `value` `auto`,
`superscript` or `inline`, or `kind = "bar"` with `auto`, `fraction` or
`minus`. It rebuilds only structure/emit/repair and returns the same contract,
which is what the review chips call.

`scanner_open_session` decodes, binarises and segments once and keeps the result
under a session id. `scanner_session_read` builds the full reading from that
cache, and `scanner_session_relabel` takes a JSON object such as
`{"3": "1", "7": "l"}` and rebuilds only the reading with those labels. The app
uses this for the correction sheet, so a symbol fix never repeats the vision
stages. A new session replaces the previous one, and an old id is rejected; the
stateless entry points stay for callers that only need one reading.

`scanner_read` decodes the image, runs the whole pipeline and returns JSON:

```json
{
  "ok": true,
  "session": 7,
  "expression": "sum(1/(n^2), n=1, inf)",
  "latex": "\\sum_{n=1}^{\\infty} \\frac{1}{n^{2}}",
  "kind": "sum",
  "parts": {"term": "1/(n^2)", "index": "n", "start": "1", "end": "inf"},
  "confidence": 0.83,
  "symbols": 9,
  "lines": ["sum(1/(n^2), n=1, inf)"],
  "glyphs": [{
    "id": 4,
    "label": "n",
    "score": 0.93,
    "margin": 0.21,
    "box": [x0,y0,x1,y1],
    "alternatives": [{"glyph": "n", "score": 0.93}, {"glyph": "h", "score": 0.71}]
  }]
}
```

`kind` is `expression`, `sum`, `integral`, `limit`, `system` or `matrix`. A
matrix reading carries `parts.matrix` in the core's field syntax (`1, 2; 3, 4`),
plus `parts.rows` and `parts.columns`. The Flutter side
(`app/lib/src/scanner_engine.dart`) loads `assets/scanner/glyph_templates.bin`
once, runs the reading in a background isolate, and retries with the
classifier's second choice when the core parser rejects the first reading.
`app/lib/src/scanner/` holds the shared flow: source sheet, four-corner crop,
result/correction screen and the handwriting board. Any expression field can open
it, and every matrix field in the curriculum renders a rows × columns grid
instead of asking for semicolons.

The bank must stay in sync with the C++ side: after
`tools/build_glyph_templates.py` changes `data/glyph_templates.bin`, copy it to
`app/assets/scanner/glyph_templates.bin` as well.

## Build and test

```sh
cd scanner
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
cd build && ctest --output-on-failure
```

`scanner_segment` is synthetic and dependency-free. `scanner_pipeline` reads the
committed samples. `scanner_crosscheck` compares the hand-written connected
components with `cv::connectedComponentsWithStats` and only builds when OpenCV
is installed (`dnf install opencv-devel`).

## Inspect one image

```sh
./build/scan_debug samples/plain_linear.png /tmp/scan --read --symbols
```

The tool writes `01-gray.png` … `06-symbols.png` (each stage, with the detected
boxes drawn) and prints a JSON report: working resolution, skew, threshold,
component/symbol/line counts and, with `--read`, the classifier's reading of
every line plus per-glyph score and margin.

## Regenerate generated files

```sh
python3 tools/build_glyph_templates.py     # data/glyph_templates.bin
python3 tools/make_plain_samples.py        # samples/plain_*.png
python3 tools/make_samples.py              # samples with real math layout (needs matplotlib)
```

`glyph_templates.bin` is the only binary the app needs. Format: magic `IGT1`,
`uint32` count, then per entry `uint8 labelLength`, label bytes, `uint8 font`,
`uint8 width`, `uint8 height`, `width*height` bytes. Every glyph is stored on a
16×16 grid, ink fitted into 14×14 and centred; the C++ side reproduces that
normalisation for the input, so the bank is never compared against a differently
scaled glyph.

## Prior art

Checked before writing the structure stage, since a maintained classical engine
would have been preferable to a new one:

| Project | Model | Why not adopted |
|---|---|---|
| [Seshat](https://github.com/falvaro/seshat) | C++, but symbol recognition uses RNNLIB (a neural net); GPL-3.0 | The core requirement is no ML, and GPL-3.0 is incompatible with this Apache-2.0 project |
| [LaTeX-OCR / pix2tex](https://github.com/lukas-blecher/LaTeX-OCR), [texify](https://github.com/VikParuchuri/texify), [UniMERNet](https://github.com/opendatalab/UniMERNet) | Transformer/ViT | Neural, large model files, nothing classical to reuse |
| Tesseract | LSTM line recogniser | Neural since v4, and it has no math structure model |
| InftyReader | Classical, closed source | Proprietary; cannot ship or inspect |

So the pipeline in this directory is original code: connected components,
integral-image binarisation, template cosine matching, geometric structure
analysis, and a LaTeX/Infinit printer over the same tree. The only external code
is `stb_image` (public domain, vendored) and OpenCV in the optional cross-check
test.

## Why no OpenCV in the pipeline

The same code has to run on Android with a small binary. The hand-written
labelling, integral-image binarisation and deskew are exact and fast enough, and
the cross-check test keeps them honest. OpenCV stays a development aid for
tuning and dataset tooling.

## Design rules

- Every stage is a pure function over an image struct; no globals, no I/O.
- Thresholds are scale-relative (fractions of glyph height or line width), never
  absolute pixels, so a 300 dpi scan and a phone crop behave alike.
- The classifier reports a score and a margin, and says `?` instead of guessing.
  The correction UI depends on that honesty.
- Nothing is discarded: alternative binarisations stay in `PreprocessResult` and
  the component list stays in `Segmentation` for the repair stage.
