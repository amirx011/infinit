#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "image.hpp"
#include "segment.hpp"

namespace infinit::scan {

/// One labelled example of a printed glyph, normalised to a 16x16 grid.
struct GlyphTemplate {
  std::string label;
  int fontIndex = 0;
  int width = 0;
  int height = 0;
  /// Unit-length, mean-centred 16x16 feature vector.
  std::vector<float> feature;
};

/// The template bank built by tools/build_glyph_templates.py.
class TemplateBank {
 public:
  bool load(const std::string& path, std::string* error = nullptr);
  bool loadFromMemory(const uint8_t* data, size_t size, std::string* error = nullptr);

  const std::vector<GlyphTemplate>& templates() const { return templates_; }
  const std::vector<std::string>& labels() const { return labels_; }
  bool empty() const { return templates_.empty(); }

 private:
  std::vector<GlyphTemplate> templates_;
  std::vector<std::string> labels_;
};

/// Grid side used by both the generator and the classifier.
constexpr int kGlyphGrid = 16;
/// Ink is fitted into this inner square so the border stays empty.
constexpr int kGlyphInner = 14;

/// Crops the ink inside `box` and resamples it onto the 16x16 grid the same way
/// the generator resampled the training glyphs. Returns a mean-centred,
/// unit-length feature vector, or an empty vector when the box has no ink.
std::vector<float> normaliseGlyph(const GrayImage& binary, const Box& box);

}  // namespace infinit::scan
