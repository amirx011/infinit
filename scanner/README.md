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
| Template bank (4 fonts, 58 classes, 236 templates) | done |
| Glyph classification (cosine kNN, margin rejection) | done |
| Structure: fractions, powers, radicals | done |
| Sums, integrals, limits, systems | done |
| Expression text and named parts for the app | done |
| Flutter screen (image import, routing) | done |
| Camera, crop and symbol-correction UI | later |

Accuracy on the committed samples, clean and rotated/blurred/noisy alike: 26 of
26 expressions read exactly, at 0.54–0.93 confidence. Covered inputs:

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
```

Known limits: matrices are not structured yet; italic `f` is not in the bank;
two glyphs that touch in the scan become one blob and are reported as `?`.

## App integration

The scanner is compiled into the same `libodecore.so` the rest of the app uses,
so there is no second native library to ship. Four C entry points are exported:

```c
int  scanner_load_templates(const unsigned char* data, size_t size);
char* scanner_read(const unsigned char* data, size_t size);  /* caller frees */
void scanner_free(char* text);
const char* scanner_version(void);
```

`scanner_read` decodes the image, runs the whole pipeline and returns JSON:

```json
{
  "ok": true,
  "expression": "sum(1/(n^2), n=1, inf)",
  "kind": "sum",
  "parts": {"term": "1/(n^2)", "index": "n", "start": "1", "end": "inf"},
  "confidence": 0.83,
  "symbols": 9,
  "lines": ["sum(1/(n^2), n=1, inf)"],
  "glyphs": [{"label": "n", "score": 0.93, "margin": 0.21, "box": [x0,y0,x1,y1]}]
}
```

`kind` is `expression`, `sum`, `integral`, `limit` or `system`. The Flutter side
(`app/lib/src/scanner_engine.dart`) loads `assets/scanner/glyph_templates.bin`
once, runs `scanner_read` in a background isolate and hands the result to the
Scanner page, which can open the series, integral or limit workbench with the
detected `parts` already filled in.

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
