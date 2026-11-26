#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "classify.hpp"
#include "image.hpp"
#include "segment.hpp"

namespace infinit::scan {

/// One pen point in board pixels. Time is preserved implicitly by stroke order
/// and by the stroke's position in the document.
struct InkPoint {
  float x = 0.0f;
  float y = 0.0f;
};

/// One continuous pen-down to pen-up segment.
struct InkStroke {
  std::vector<InkPoint> points;
  Box box;
};

/// The ink the board collected. `width`/`height` are the board's logical size;
/// the renderer scales nothing, so the caller sends pixels it already drew at.
struct InkDocument {
  int width = 0;
  int height = 0;
  std::vector<InkStroke> strokes;
};

/// Wire format, little endian:
///   uint32 width, uint32 height, uint32 strokeCount,
///   then per stroke: uint32 pointCount, pointCount * (float x, float y).
/// Returns an empty document on malformed input.
InkDocument decodeInk(const uint8_t* data, std::size_t size);

/// Draws the ink as white strokes on a black background, the same convention
/// the segmentation stage uses for photos (ink = 255).
GrayImage renderInk(const InkDocument& document);

struct InkSegmentation {
  GrayImage binary;
  Segmentation segmentation;
  /// Joints of the stroke groups, for the merge/split UI.
  std::vector<std::vector<int>> groups;
};

/// Groups strokes into symbols and lines. A dynamic program over the stroke
/// sequence considers one-to-four-stroke symbols, only allowing a group when
/// every pair is geometrically plausible; small late strokes (a dot, a prime)
/// are then moved to the nearest symbol by position. Passing a classifier adds
/// its score to the lattice cost, so a three-stroke blob that resembles no
/// glyph is less likely to win. `classifier` may be null for tests and debug.
InkSegmentation segmentInk(const InkDocument& document);
InkSegmentation segmentInk(const InkDocument& document,
                           const ISymbolClassifier* classifier);

}  // namespace infinit::scan
