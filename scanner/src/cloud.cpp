#include "cloud.hpp"

#include "ink.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace infinit::scan {
namespace {

using Point = std::pair<float, float>;

constexpr std::size_t kCloudPoints = 32;

double distanceBetween(const Point& a, const Point& b) {
  const double dx = static_cast<double>(a.first) - b.first;
  const double dy = static_cast<double>(a.second) - b.second;
  return std::sqrt(dx * dx + dy * dy);
}

/// Translates the cloud to the origin and scales the longer side to one, so
/// scale and position stop mattering. Farthest point sampling then reduces it
/// to `count` points without depending on point order.
std::vector<Point> normaliseSample(const PointCloud& cloud, std::size_t count) {
  std::vector<Point> out;
  if (cloud.points.empty() || count == 0) return out;

  float minX = cloud.points.front().first;
  float maxX = minX;
  float minY = cloud.points.front().second;
  float maxY = minY;
  for (const Point& point : cloud.points) {
    minX = std::min(minX, point.first);
    maxX = std::max(maxX, point.first);
    minY = std::min(minY, point.second);
    maxY = std::max(maxY, point.second);
  }
  const double span = std::max<double>(1e-6,
                                       std::max(maxX - minX, maxY - minY));
  std::vector<Point> scaled;
  scaled.reserve(cloud.points.size());
  for (const Point& point : cloud.points) {
    scaled.emplace_back(static_cast<float>((point.first - minX) / span),
                        static_cast<float>((point.second - minY) / span));
  }

  if (scaled.size() <= count) return scaled;

  // Start from the point farthest from the centroid: deterministic for equal
  // inputs and always on the shape's edge.
  double cx = 0.0;
  double cy = 0.0;
  for (const Point& point : scaled) {
    cx += point.first;
    cy += point.second;
  }
  cx /= static_cast<double>(scaled.size());
  cy /= static_cast<double>(scaled.size());

  std::vector<std::size_t> chosen;
  chosen.reserve(count);
  std::size_t first = 0;
  double best = -1.0;
  for (std::size_t i = 0; i < scaled.size(); ++i) {
    const double d = distanceBetween(scaled[i], {static_cast<float>(cx),
                                                 static_cast<float>(cy)});
    if (d > best) {
      best = d;
      first = i;
    }
  }
  chosen.push_back(first);

  std::vector<double> nearest(scaled.size(),
                              std::numeric_limits<double>::infinity());
  while (chosen.size() < count) {
    const Point& last = scaled[chosen.back()];
    std::size_t next = 0;
    double farthest = -1.0;
    for (std::size_t i = 0; i < scaled.size(); ++i) {
      nearest[i] = std::min(nearest[i], distanceBetween(scaled[i], last));
      if (nearest[i] > farthest) {
        farthest = nearest[i];
        next = i;
      }
    }
    chosen.push_back(next);
  }

  out.reserve(chosen.size());
  for (std::size_t index : chosen) out.push_back(scaled[index]);
  return out;
}

/// One-directional greedy match: every point of `from` claims the closest
/// still-unclaimed point of `to`, or pays the full threshold when none is
/// close enough. Returns the mean cost normalised to 0..1.
double greedyCost(const std::vector<Point>& from, const std::vector<Point>& to,
                  double fraction) {
  if (from.empty() || to.empty()) return 1.0;
  std::vector<char> claimed(to.size(), 0);
  double total = 0.0;
  for (const Point& point : from) {
    double best = fraction;
    std::size_t bestIndex = to.size();
    for (std::size_t i = 0; i < to.size(); ++i) {
      if (claimed[i]) continue;
      const double d = distanceBetween(point, to[i]);
      if (d < best) {
        best = d;
        bestIndex = i;
      }
    }
    if (bestIndex < to.size()) claimed[bestIndex] = 1;
    total += best;
  }
  return total / fraction / static_cast<double>(from.size());
}

}  // namespace

std::vector<std::pair<float, float>> resamplePolyline(
    const std::vector<InkPoint>& points, std::size_t count) {
  if (points.empty() || count == 0) return {};
  if (points.size() == 1 || count == 1) {
    return {std::make_pair(points.front().x, points.front().y)};
  }

  std::vector<double> cumulative(points.size(), 0.0);
  for (std::size_t i = 1; i < points.size(); ++i) {
    const double dx = static_cast<double>(points[i].x) - points[i - 1].x;
    const double dy = static_cast<double>(points[i].y) - points[i - 1].y;
    cumulative[i] = cumulative[i - 1] + std::sqrt(dx * dx + dy * dy);
  }
  const double length = cumulative.back();
  if (length <= 1e-6) {
    return {std::make_pair(points.front().x, points.front().y)};
  }

  std::vector<std::pair<float, float>> out;
  out.reserve(count);
  std::size_t segment = 1;
  for (std::size_t i = 0; i < count; ++i) {
    const double target =
        length * static_cast<double>(i) / static_cast<double>(count - 1);
    while (segment + 1 < points.size() && cumulative[segment] < target) {
      ++segment;
    }
    const double span = cumulative[segment] - cumulative[segment - 1];
    const double t = span <= 1e-9 ? 0.0
                                  : (target - cumulative[segment - 1]) / span;
    const float x = static_cast<float>(
        points[segment - 1].x + t * (points[segment].x - points[segment - 1].x));
    const float y = static_cast<float>(
        points[segment - 1].y + t * (points[segment].y - points[segment - 1].y));
    out.emplace_back(x, y);
  }
  return out;
}

PointCloud cloudFromStrokes(const std::vector<InkStroke>& strokes,
                            const std::vector<int>& ids,
                            std::size_t perStroke) {
  PointCloud cloud;
  Box box;
  for (int id : ids) {
    if (id < 0 || static_cast<std::size_t>(id) >= strokes.size()) continue;
    const InkStroke& stroke = strokes[static_cast<std::size_t>(id)];
    if (stroke.points.empty()) continue;
    const std::vector<Point> sampled = resamplePolyline(stroke.points,
                                                        std::max<std::size_t>(
                                                            2, perStroke));
    cloud.points.insert(cloud.points.end(), sampled.begin(), sampled.end());
    box.include(stroke.box);
  }
  if (!box.empty()) {
    cloud.aspect = static_cast<double>(box.width()) /
                   std::max(1.0, static_cast<double>(box.height()));
  }
  return cloud;
}

PointCloud cloudFromBox(const GrayImage& binary, const Box& box,
                        std::size_t maxPoints) {
  PointCloud cloud;
  const int x0 = std::max(0, box.x0);
  const int y0 = std::max(0, box.y0);
  const int x1 = std::min(binary.width, box.x1);
  const int y1 = std::min(binary.height, box.y1);
  if (x1 <= x0 || y1 <= y0) return cloud;

  int inkWidth = 0;
  int inkHeight = 0;
  int left = x1;
  int top = y1;
  int right = x0;
  int bottom = y0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      if (binary.at(x, y) <= 127) continue;
      cloud.points.emplace_back(static_cast<float>(x), static_cast<float>(y));
      left = std::min(left, x);
      top = std::min(top, y);
      right = std::max(right, x + 1);
      bottom = std::max(bottom, y + 1);
    }
  }
  if (cloud.points.empty()) return cloud;

  inkWidth = right - left;
  inkHeight = bottom - top;
  cloud.aspect = static_cast<double>(inkWidth) / std::max(1, inkHeight);

  // Thin dense ink before the quadratic greedy pass; the matcher samples down
  // again, so this only bounds work and memory.
  if (maxPoints > 0 && cloud.points.size() > maxPoints * 4) {
    const std::size_t stride =
        std::max<std::size_t>(1, cloud.points.size() / (maxPoints * 4));
    std::vector<Point> thinned;
    thinned.reserve(cloud.points.size() / stride + 1);
    for (std::size_t i = 0; i < cloud.points.size(); i += stride) {
      thinned.push_back(cloud.points[i]);
    }
    cloud.points = std::move(thinned);
  }
  return cloud;
}

PointCloud cloudFromGrid(const std::vector<float>& grid, int side,
                         double aspect) {
  PointCloud cloud;
  cloud.aspect = aspect > 0 ? aspect : 1.0;
  if (side <= 0 ||
      grid.size() < static_cast<std::size_t>(side) * side) {
    return cloud;
  }
  for (int y = 0; y < side; ++y) {
    for (int x = 0; x < side; ++x) {
      const float value = grid[static_cast<std::size_t>(y) * side + x];
      if (value > 0.5f) {
        cloud.points.emplace_back(static_cast<float>(x), static_cast<float>(y));
      }
    }
  }
  return cloud;
}

double matchClouds(const PointCloud& a, const PointCloud& b, double fraction) {
  if (a.points.empty() || b.points.empty()) return 0.0;
  const double threshold = std::clamp(fraction, 0.05, 1.0);
  const std::vector<Point> left = normaliseSample(a, kCloudPoints);
  const std::vector<Point> right = normaliseSample(b, kCloudPoints);
  if (left.empty() || right.empty()) return 0.0;

  const double forward = greedyCost(left, right, threshold);
  const double backward = greedyCost(right, left, threshold);
  double score = 1.0 - 0.5 * (forward + backward);

  // Aspect is a soft feature, not a veto: a wide "=" must not score like "1",
  // but a slightly stretched "x" still matches.
  const double ratioA = std::max(1e-6, a.aspect);
  const double ratioB = std::max(1e-6, b.aspect);
  const double stretch = std::abs(std::log(ratioA / ratioB));
  score -= std::min(0.18, 0.18 * stretch);
  return std::clamp(score, 0.0, 1.0);
}

void CloudBank::loadFromTemplates(const TemplateBank& bank) {
  entries_.clear();
  for (const GlyphTemplate& glyph : bank.templates()) {
    PointCloud cloud = cloudFromGrid(glyph.raw, kGlyphGrid, glyph.aspect);
    if (cloud.points.empty()) continue;
    entries_.push_back(Entry{glyph.label, std::move(cloud)});
  }
}

CloudMatch CloudBank::match(const PointCloud& cloud, std::size_t k) const {
  CloudMatch result;
  if (entries_.empty() || cloud.points.empty() || k == 0) return result;

  struct Scored {
    std::string label;
    double score;
  };
  std::vector<Scored> scored;
  scored.reserve(entries_.size());
  for (const Entry& entry : entries_) {
    scored.push_back(Scored{entry.label, matchClouds(cloud, entry.cloud)});
  }
  std::sort(scored.begin(), scored.end(),
            [](const Scored& x, const Scored& y) { return x.score > y.score; });
  if (scored.size() > k) scored.resize(k);

  // Distance-weighted vote: similar prototypes count more, and a label with
  // several near matches beats one lucky neighbour.
  std::vector<std::pair<std::string, double>> weightSum;
  auto slot = [&weightSum](const std::string& label) -> double& {
    for (auto& entry : weightSum) {
      if (entry.first == label) return entry.second;
    }
    weightSum.emplace_back(label, 0.0);
    return weightSum.back().second;
  };
  double totalWeight = 0.0;
  for (const Scored& item : scored) {
    const double weight = item.score * item.score;
    slot(item.label) += weight * item.score;
    totalWeight += weight;
  }
  if (totalWeight <= 1e-9) return result;
  for (auto& entry : weightSum) entry.second /= totalWeight;

  std::sort(weightSum.begin(), weightSum.end(),
            [](const auto& x, const auto& y) { return x.second > y.second; });
  result.label = weightSum.front().first;
  result.score = std::clamp(weightSum.front().second, 0.0, 1.0);
  if (weightSum.size() > 1) {
    result.runnerUp = weightSum[1].first;
    result.margin = result.score - weightSum[1].second;
  } else {
    result.margin = result.score;
  }
  result.confident = result.score >= 0.62 && result.margin >= 0.03;
  for (std::size_t i = 0; i < weightSum.size() && i < 3; ++i) {
    result.alternatives.emplace_back(weightSum[i].first, weightSum[i].second);
  }
  return result;
}

}  // namespace infinit::scan
