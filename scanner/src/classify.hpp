#pragma once

#include <string>

#include "glyphs.hpp"
#include "image.hpp"
#include "segment.hpp"

namespace infinit::scan {

struct GlyphMatch {
  /// Best label, or "?" when the match is not trustworthy.
  std::string label;
  std::string runnerUp;
  /// Cosine similarity of the winning template.
  double score = 0.0;
  /// Difference between the best and the best *other* label.
  double margin = 0.0;
  bool confident = false;
};

struct ClassifyOptions {
  /// Below this cosine similarity the glyph is reported as "?".
  double minScore = 0.55;
  /// Labels this close to each other are reported as "?" instead of a guess.
  double minMargin = 0.02;
};

GlyphMatch classifyGlyph(const GrayImage& binary, const Box& box,
                         const TemplateBank& bank,
                         const ClassifyOptions& options = {});

}  // namespace infinit::scan
