// Synthetic, dependency-free checks for the segmentation stage. Each case is
// an ASCII grid, so the expected boxes are readable in the test itself.
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "image.hpp"
#include "preprocess.hpp"
#include "segment.hpp"

using namespace infinit::scan;

namespace {

int failures = 0;

void check(const std::string& label, bool ok, const std::string& detail = "") {
  std::cout << (ok ? "PASS " : "FAIL ") << label;
  if (!ok) {
    ++failures;
    if (!detail.empty()) std::cout << "   (" << detail << ")";
  }
  std::cout << "\n";
}

void checkCount(const std::string& label, size_t got, size_t want) {
  check(label, got == want,
        "got " + std::to_string(got) + ", want " + std::to_string(want));
}

GrayImage ascii(const std::vector<std::string>& rows) {
  size_t width = 0;
  for (const auto& row : rows) width = std::max(width, row.size());
  GrayImage image(static_cast<int>(width), static_cast<int>(rows.size()), 0);
  for (size_t y = 0; y < rows.size(); ++y) {
    for (size_t x = 0; x < rows[y].size(); ++x) {
      if (rows[y][x] == '#') image.at(static_cast<int>(x), static_cast<int>(y)) = 255;
    }
  }
  return image;
}

void testSingleBar() {
  const Segmentation seg = segment(ascii({
      "####################",
      "####################",
      "####################",
      "####################",
  }));
  checkCount("single bar: components", seg.components.size(), 1);
  checkCount("single bar: symbols", seg.symbols.size(), 1);
  checkCount("single bar: lines", seg.lines.size(), 1);
  check("single bar: box", seg.symbols[0].box.x0 == 0 && seg.symbols[0].box.x1 == 20 &&
                               seg.symbols[0].box.y1 == 4);
}

void testEqualsMerges() {
  const Segmentation seg = segment(ascii({
      "##################",
      "##################",
      "##################",
      "..................",
      "..................",
      "##################",
      "##################",
      "##################",
  }));
  checkCount("equals: components", seg.components.size(), 2);
  checkCount("equals: symbols", seg.symbols.size(), 1);
  check("equals: spans both bars", seg.symbols[0].box.height() == 8,
        "height " + std::to_string(seg.symbols[0].box.height()));
}

void testDottedI() {
  const Segmentation seg = segment(ascii({
      "....####....",
      "....####....",
      "............",
      "....####....",
      "....####....",
      "....####....",
      "....####....",
      "....####....",
      "....####....",
      "....####....",
      "....####....",
      "....####....",
      "....####....",
      "....####....",
  }));
  checkCount("dotted i: components", seg.components.size(), 2);
  checkCount("dotted i: symbols", seg.symbols.size(), 1);
}

void testTwoGlyphsOneLine() {
  const Segmentation seg = segment(ascii({
      "..#####.........####..",
      "..#####.........####..",
      "..#####.........####..",
      "..#####.........####..",
      "..#####.........####..",
      "..#####.........####..",
      "..#####.........####..",
      "..#####.........####..",
  }));
  checkCount("two glyphs: components", seg.components.size(), 2);
  checkCount("two glyphs: symbols", seg.symbols.size(), 2);
  checkCount("two glyphs: lines", seg.lines.size(), 1);
  check("two glyphs: left first", seg.symbols[0].box.x0 < seg.symbols[1].box.x0);
}

void testAdjacentGlyphsStaySeparate() {
  // Two glyphs three pixels apart must not become one symbol, even though the
  // gap is small compared to the glyph height.
  const Segmentation seg = segment(ascii({
      "..###...###..",
      "..###...###..",
      "..###...###..",
      "..###...###..",
      "..###...###..",
      "..###...###..",
      "..###...###..",
      "..###...###..",
      "..###...###..",
      "..###...###..",
  }));
  checkCount("adjacent glyphs: components", seg.components.size(), 2);
  checkCount("adjacent glyphs: symbols", seg.symbols.size(), 2);
}

void testTwoLines() {
  const Segmentation seg = segment(ascii({
      "..#####....",
      "..#####....",
      "..#####....",
      "...........",
      "...........",
      "...........",
      "..#####....",
      "..#####....",
      "..#####....",
  }));
  checkCount("two lines: components", seg.components.size(), 2);
  checkCount("two lines: lines", seg.lines.size(), 2);
  checkCount("two lines: symbols", seg.symbols.size(), 2);
  check("two lines: ordered top-down",
        seg.lines[0].box.y0 < seg.lines[1].box.y0);
}

void testNoiseDropped() {
  const Segmentation seg = segment(ascii({
      "..#####.....",
      "..#####.....",
      "..#####.....",
      "..#####.....",
      "..........#.",
  }));
  checkCount("noise: components", seg.components.size(), 1);
}

void testPreprocessFlatPage() {
  // A white page with a black glyph: binarization must keep the glyph and
  // discard the page.
  GrayImage page(120, 80, 255);
  for (int y = 20; y < 60; ++y) {
    for (int x = 30; x < 90; ++x) page.at(x, y) = 20;
  }
  const PreprocessResult result = preprocess(page, PreprocessOptions{});
  check("preprocess: keeps size", result.gray.width == 120 && result.gray.height == 80);
  check("preprocess: not inverted", !result.inverted);
  const Segmentation seg = segment(result.binary);
  checkCount("preprocess: one symbol", seg.symbols.size(), 1);
  check("preprocess: symbol covers glyph",
        seg.symbols[0].box.x0 <= 30 && seg.symbols[0].box.x1 >= 90);
}

void testPreprocessDarkBackground() {
  // Inverted capture (dark mode screenshot): the glyph is lighter than the page.
  GrayImage page(120, 80, 18);
  for (int y = 20; y < 60; ++y) {
    for (int x = 30; x < 90; ++x) page.at(x, y) = 235;
  }
  const PreprocessResult result = preprocess(page, PreprocessOptions{});
  check("dark page: inverted flag", result.inverted);
  const Segmentation seg = segment(result.binary);
  checkCount("dark page: one symbol", seg.symbols.size(), 1);
}

void testDeskew() {
  // Three stacked bars rotated by ~5 degrees: the deskew search must find it.
  GrayImage page(200, 200, 255);
  for (int bar = 0; bar < 3; ++bar) {
    const int y0 = 60 + bar * 30;
    for (int x = 40; x < 160; ++x) {
      const int y = y0 + static_cast<int>(0.0875 * (x - 40));  // ~5 degrees
      for (int t = 0; t < 6; ++t) page.at(x, y + t) = 0;
    }
  }
  const PreprocessResult result = preprocess(page, PreprocessOptions{});
  check("deskew: detects rotation", std::abs(result.skewDegrees) > 3.0,
        "skew " + std::to_string(result.skewDegrees));
}

}  // namespace

int main() {
  testSingleBar();
  testEqualsMerges();
  testDottedI();
  testTwoGlyphsOneLine();
  testAdjacentGlyphsStaySeparate();
  testTwoLines();
  testNoiseDropped();
  testPreprocessFlatPage();
  testPreprocessDarkBackground();
  testDeskew();

  if (failures > 0) {
    std::cout << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "all segmentation tests passed\n";
  return 0;
}
