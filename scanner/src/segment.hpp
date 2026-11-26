#pragma once

#include <vector>

#include "image.hpp"

namespace infinit::scan {

/// Axis-aligned box in pixel coordinates; x1/y1 are exclusive.
struct Box {
  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;

  int width() const { return x1 - x0; }
  int height() const { return y1 - y0; }
  int area() const { return width() * height(); }
  bool empty() const { return x1 <= x0 || y1 <= y0; }
  int centerX() const { return (x0 + x1) / 2; }
  int centerY() const { return (y0 + y1) / 2; }

  void expand(int px) {
    x0 -= px;
    y0 -= px;
    x1 += px;
    y1 += px;
  }
  void include(const Box& other);
  bool intersects(const Box& other) const;
  int overlapX(const Box& other) const;
  int overlapY(const Box& other) const;
};

/// One 8-connected blob of ink pixels.
struct Component {
  int id = 0;
  Box box;
  int pixels = 0;
  /// Enclosed white regions (0 for "1", 1 for "0"/"6"/"9", 2 for "8").
  int holes = 0;
  /// Sum of pixel coordinates, used for the centroid.
  double sumX = 0.0;
  double sumY = 0.0;

  double centroidX() const { return pixels > 0 ? sumX / pixels : box.centerX(); }
  double centroidY() const { return pixels > 0 ? sumY / pixels : box.centerY(); }
};

/// 8-connected component labelling on a binary image where ink is > 127.
/// Components smaller than minPixels are dropped as noise.
std::vector<Component> connectedComponents(const GrayImage& binary,
                                           int minPixels = 6);

/// Number of enclosed white regions inside `box` (OpenCV contour hierarchy).
int countHoles(const GrayImage& binary, const Box& box);

/// A visual symbol: one or more components that belong to the same glyph.
struct Symbol {
  int id = 0;
  Box box;
  std::vector<int> components;
  /// Index of the text line this symbol was grouped into.
  int line = 0;
};

struct TextLine {
  int id = 0;
  Box box;
  std::vector<int> symbols;
  /// Median symbol height, used to normalise later thresholds.
  double medianHeight = 0.0;
};

struct Segmentation {
  std::vector<Component> components;
  std::vector<Symbol> symbols;
  std::vector<TextLine> lines;
  /// True when the image produced more symbols than `maxSymbols`; the reading
  /// is truncated and the host should ask for a tighter crop.
  bool overSegmented = false;
};

struct SegmentOptions {
  int minPixels = 6;
  /// Fail fast on a dense page instead of spending seconds on noise.
  int maxSymbols = 1500;
  /// Side-by-side fragments join only when they overlap horizontally and are
  /// this aligned vertically.
  double fragmentOverlapRatio = 0.75;
  /// ...and when their combined width stays within this multiple of the taller
  /// fragment, which stops two touching glyphs from becoming one symbol.
  double fragmentWidthRatio = 1.30;
  /// A box is "bar-like" when its height is at most this fraction of its
  /// width. Only bar-like pairs stack into one glyph (the two bars of =).
  double barAspectRatio = 0.40;
  /// Maximum vertical gap between two stacked bars, relative to their width.
  double stackedBarGapRatio = 0.50;
  /// Touching glyphs (§4.3.1): a component wider than this multiple of the
  /// median symbol height is a candidate for a projection-valley split.
  double splitWidthRatio = 1.55;
  /// The valley column must be this much emptier than the component average.
  double splitValleyRatio = 0.45;
  /// Solid ink fraction a component needs before it may be split: digits are
  /// dense, letter pairs are mostly white between the strokes.
  double splitDensity = 0.34;
  /// Splitting is conservative by design: a wrong split costs more than a wrong
  /// merge, because the correction UI can merge but the reading already broke.
  bool splitTouching = true;
};

Segmentation segment(const GrayImage& binary, const SegmentOptions& options = {});

/// Rebuilds symbols and lines from explicit component groups, keeping the
/// original components and pixel boxes. The correction UI uses this after a
/// manual merge or split; ids and reading order are recomputed from scratch.
Segmentation regroup(const Segmentation& original,
                     const std::vector<std::vector<int>>& groups);

/// Debug overlay: draws every symbol box, and line boxes in a second colour.
RgbImage renderSegmentation(const GrayImage& source, const Segmentation& seg);

}  // namespace infinit::scan
