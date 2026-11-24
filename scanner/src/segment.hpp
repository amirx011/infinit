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
};

struct SegmentOptions {
  int minPixels = 6;
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
};

Segmentation segment(const GrayImage& binary, const SegmentOptions& options = {});

/// Debug overlay: draws every symbol box, and line boxes in a second colour.
RgbImage renderSegmentation(const GrayImage& source, const Segmentation& seg);

}  // namespace infinit::scan
