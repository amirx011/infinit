#pragma once

#include <utility>
#include <vector>

#include "image.hpp"
#include "ink.hpp"

namespace infinit::scan {

struct TraceOptions {
  /// Zhang-Suen iterations. 60 is far past convergence for a 1600 px page.
  int maxThinningPasses = 60;
  /// Segments shorter than this many pixels are dropped as noise.
  int minLength = 12;
  /// Branches meeting at a junction are joined when their directions are
  /// opposite within this many degrees.
  double joinDegrees = 35.0;
  /// Junction pixels come in clusters as thick as the pen, so two branches that
  /// continue each other end several pixels apart.
  double joinRadius = 12.0;
  /// Hard cap so a noisy page cannot produce thousands of strokes.
  std::size_t maxStrokes = 400;
};

/// Thins the ink to a one-pixel skeleton (Zhang-Suen). Ink is 255 on 0.
GrayImage thin(const GrayImage& binary,
               int maxPasses = 60);

/// Reconstructs pen strokes from a binary image: thin, cut the skeleton at
/// junctions, trace each branch, then join the branches that continue straight
/// through a junction.
///
/// This is the plan's §12A.8 path: a photo of handwriting is turned back into
/// the stroke representation the ink recognizer already understands. Crossing
/// strokes and shared junctions make it much weaker than live ink, which is why
/// the caller labels the result "experimental".
std::vector<InkStroke> traceStrokes(const GrayImage& binary,
                                    const TraceOptions& options = {});

}  // namespace infinit::scan
