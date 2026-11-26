#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "glyphs.hpp"
#include "image.hpp"

namespace infinit::scan {

struct InkPoint;
struct InkStroke;

/// A shape as an unordered point set. Points live in the unit square after
/// normalisation; `aspect` (ink width / ink height) is kept as a separate
/// feature because a square normalisation alone turns "1" and "—" into the same
/// point set.
struct PointCloud {
  std::vector<std::pair<float, float>> points;
  double aspect = 1.0;
};

/// Resamples a polyline to `count` points spaced evenly by arc length. A
/// single-point stroke (a dot) keeps its point repeated.
std::vector<std::pair<float, float>> resamplePolyline(
    const std::vector<InkPoint>& points, std::size_t count);

/// One stroke group as a cloud: every stroke is resampled to `perStroke`
/// points, so a long bar and a dot contribute the same number of samples and
/// stroke order stops mattering.
PointCloud cloudFromStrokes(const std::vector<InkStroke>& strokes,
                            const std::vector<int>& ids,
                            std::size_t perStroke = 24);

/// The ink pixels inside `box` as a cloud, thinned to `maxPoints` by farthest
/// point sampling. Used for the raster path (photo corrections, tests).
PointCloud cloudFromBox(const GrayImage& binary, const Box& box,
                        std::size_t maxPoints = 32);

/// A template bank grid as a cloud: cells above the ink threshold become
/// points. This is how bundled prototypes get stroke-free point clouds, so the
/// matcher needs no handwriting dataset to ship.
PointCloud cloudFromGrid(const std::vector<float>& grid, int side,
                         double aspect);

/// $P-style greedy cloud match ("$P recognizer", Vatavu et al. 2012).
///
/// Both clouds are translated to the origin and scaled so their longer side is
/// one, then reduced to the same number of points by farthest point sampling.
/// Each candidate point then takes its nearest still-unmatched prototype point
/// when that distance is below `fraction` (of the unit square); points left
/// unmatched cost the full threshold. The result is symmetric in `a` and `b`.
/// Returns 0..1, where 1 means identical clouds.
double matchClouds(const PointCloud& a, const PointCloud& b,
                   double fraction = 0.30);

/// One vote from the cloud matcher. Mirrors GlyphMatch without depending on the
/// classifier header, which in turn depends on this one.
struct CloudMatch {
  std::string label;
  std::string runnerUp;
  double score = 0.0;
  double margin = 0.0;
  bool confident = false;
  std::vector<std::pair<std::string, double>> alternatives;
};

/// Prototype clouds derived from the bundled raster templates. The ink path
/// asks this bank when the personal prototypes and the raster templates
/// disagree: shape matching is insensitive to stroke thickness and to the
/// amount of ink a bar or an accent adds.
class CloudBank {
 public:
  void loadFromTemplates(const TemplateBank& bank);

  void clear() { entries_.clear(); }
  bool empty() const { return entries_.empty(); }
  std::size_t size() const { return entries_.size(); }

  /// kNN vote over the cloud distance. Returns an empty label when the bank is
  /// empty or the candidate has no points.
  CloudMatch match(const PointCloud& cloud, std::size_t k = 7) const;

 private:
  struct Entry {
    std::string label;
    PointCloud cloud;
  };
  std::vector<Entry> entries_;
};

}  // namespace infinit::scan
