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
  /// The same grid before centring, values in [0, 1]. Zoning, projections and
  /// hole counts are read from here.
  std::vector<float> raw;
  int holes = 0;
  double aspect = 1.0;
  /// 4x4 block means, computed once at load time.
  std::vector<float> zoning;
  /// Column and row ink profiles (2 x 16, each L1-normalised). Cheap
  /// projections from the plan's feature list: they separate glyphs whose
  /// mass sits in a different place even when the correlation is close.
  std::vector<float> profile;
  /// Ink runs along the middle row, middle column and both diagonals.
  std::vector<float> crossings;
  /// log10 of the seven Hu moment invariants, clipped to a sane range.
  std::vector<float> hu;
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

/// Rasterises the glyph into the same 16x16 grid without centring. `aspect` is
/// the ink width divided by its height. Returns false when the box has no ink.
bool rasteriseGlyph(const GrayImage& binary, const Box& box,
                    std::vector<float>* grid, double* aspect);

/// Enclosed regions in a rasterised glyph, using a 0.5 ink threshold.
int gridHoles(const std::vector<float>& grid);

/// Ink width divided by ink height, using a 0.5 threshold.
double gridAspect(const std::vector<float>& grid);

/// Block means over a `blocks` x `blocks` grid.
std::vector<float> gridZoning(const std::vector<float>& grid, int blocks);

/// Column and row ink profiles of a rasterised glyph, each L1-normalised and
/// concatenated (32 values). Empty when the grid is not 16x16.
std::vector<float> gridProfiles(const std::vector<float>& grid);

/// Number of ink runs along the middle row, the middle column, the main
/// diagonal and the anti-diagonal, at a 0.5 ink threshold (4 values).
std::vector<float> gridCrossings(const std::vector<float>& grid);

/// log10 of the seven Hu moment invariants of the ink mass, clipped to
/// [-30, 30]. Translation/scale/rotation invariants, so they carry shape
/// information the raw raster only encodes implicitly.
std::vector<float> gridHuMoments(const std::vector<float>& grid);

/// Fills every cached grid feature (holes, aspect, zoning, profile, crossings,
/// Hu). Call this once for every template and prototype after `raw` is known.
void cacheGridFeatures(GlyphTemplate* entry);

}  // namespace infinit::scan
