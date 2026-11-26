// Photo-handwriting front end (§12A.8): thin the ink, cut it at junctions, trace
// the branches and join the ones that continue straight. The shapes below are
// drawn from lines so the expected stroke count is not a guess.
#include <cmath>
#include <cstdio>
#include <string>

#include "image.hpp"
#include "trace.hpp"

using namespace infinit::scan;

namespace {

int failures = 0;

void check(const std::string& label, bool ok, const std::string& detail = "") {
  std::printf("%s %s", ok ? "PASS" : "FAIL", label.c_str());
  if (!ok) {
    ++failures;
    if (!detail.empty()) std::printf("   (%s)", detail.c_str());
  }
  std::printf("\n");
}

void pixel(GrayImage& image, int x, int y) {
  if (x >= 0 && y >= 0 && x < image.width && y < image.height) {
    image.at(x, y) = 255;
  }
}

void line(GrayImage& image, double x0, double y0, double x1, double y1,
          int width = 2) {
  const double length = std::hypot(x1 - x0, y1 - y0);
  const int steps = static_cast<int>(length * 2) + 1;
  for (int step = 0; step <= steps; ++step) {
    const double t = static_cast<double>(step) / steps;
    const double x = x0 + (x1 - x0) * t;
    const double y = y0 + (y1 - y0) * t;
    for (int dy = -width; dy <= width; ++dy) {
      for (int dx = -width; dx <= width; ++dx) {
        pixel(image, static_cast<int>(x) + dx, static_cast<int>(y) + dy);
      }
    }
  }
}

void dot(GrayImage& image, double x, double y, int radius = 2) {
  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      pixel(image, static_cast<int>(x) + dx, static_cast<int>(y) + dy);
    }
  }
}

}  // namespace

int main() {
  // Thinning keeps one row for a bar and never eats the interior.
  {
    GrayImage bar(40, 11, 0);
    for (int x = 6; x < 34; ++x) {
      for (int y = 3; y < 8; ++y) pixel(bar, x, y);
    }
    const GrayImage skeleton = thin(bar);
    int ink = 0;
    for (uint8_t value : skeleton.pixels) {
      if (value > 127) ++ink;
    }
    check("thinning leaves a one-pixel backbone", ink >= 20 && ink <= 34,
          std::to_string(ink) + " px");
  }

  // Crossing strokes: the X has to come back as two strokes, not four.
  {
    GrayImage image(120, 120, 0);
    line(image, 20, 20, 100, 100);
    line(image, 20, 100, 100, 20);
    const auto strokes = traceStrokes(image);
    check("X traces to two strokes", strokes.size() == 2,
          std::to_string(strokes.size()));
  }

  // Two parallel bars stay separate.
  {
    GrayImage image(120, 120, 0);
    line(image, 20, 45, 100, 45);
    line(image, 20, 70, 100, 70);
    const auto strokes = traceStrokes(image);
    check("= traces to two strokes", strokes.size() == 2,
          std::to_string(strokes.size()));
  }

  // A crossing plus is one horizontal and one vertical stroke.
  {
    GrayImage image(200, 120, 0);
    line(image, 20, 60, 180, 60);
    line(image, 80, 25, 80, 95);
    const auto strokes = traceStrokes(image);
    check("+ traces to two strokes", strokes.size() == 2,
          std::to_string(strokes.size()));
  }

  // A dot is a symbol of its own, not noise: "i" has a stem and a dot.
  {
    GrayImage image(120, 160, 0);
    line(image, 60, 60, 60, 140);
    dot(image, 60, 40);
    const auto strokes = traceStrokes(image);
    check("i keeps the dot as a stroke", strokes.size() == 2,
          std::to_string(strokes.size()));
  }

  // An empty page has no strokes and must not crash.
  {
    GrayImage image(40, 40, 0);
    const auto strokes = traceStrokes(image);
    check("a blank page yields no strokes", strokes.empty());
  }

  if (failures > 0) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("all trace checks passed\n");
  return 0;
}
