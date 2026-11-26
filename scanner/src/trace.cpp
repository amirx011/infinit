#include "trace.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>

namespace infinit::scan {
namespace {

/// pi without <cmath>'s M_PI, which MSVC does not define unless asked.
constexpr double kPi = 3.14159265358979323846;

inline bool ink(const GrayImage& image, int x, int y) {
  return x >= 0 && y >= 0 && x < image.width && y < image.height &&
         image.at(x, y) > 127;
}

/// Eight neighbours, clockwise from north. Zhang-Suen needs this exact order.
constexpr int kDx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
constexpr int kDy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

int neighbours(const GrayImage& image, int x, int y) {
  int count = 0;
  for (int k = 0; k < 8; ++k) {
    if (ink(image, x + kDx[k], y + kDy[k])) ++count;
  }
  return count;
}

/// Crossing number of the 8-neighbourhood: how many separate ink arms meet at
/// this pixel. A thick staircase has three neighbours but only two arms, so
/// the plain neighbour count would cut the skeleton everywhere.
int crossingNumber(const GrayImage& image, int x, int y) {
  bool ring[9];
  for (int k = 0; k < 8; ++k) ring[k] = ink(image, x + kDx[k], y + kDy[k]);
  ring[8] = ring[0];
  int transitions = 0;
  for (int k = 0; k < 8; ++k) {
    if (!ring[k] && ring[k + 1]) ++transitions;
  }
  return transitions;
}

/// One thinning sub-iteration. Returns true when anything was removed.
bool thinningPass(GrayImage* image, bool first) {
  std::vector<std::pair<int, int>> remove;
  for (int y = 1; y < image->height - 1; ++y) {
    for (int x = 1; x < image->width - 1; ++x) {
      if (!ink(*image, x, y)) continue;
      const bool p2 = ink(*image, x, y - 1);
      const bool p3 = ink(*image, x + 1, y - 1);
      const bool p4 = ink(*image, x + 1, y);
      const bool p5 = ink(*image, x + 1, y + 1);
      const bool p6 = ink(*image, x, y + 1);
      const bool p7 = ink(*image, x - 1, y + 1);
      const bool p8 = ink(*image, x - 1, y);
      const bool p9 = ink(*image, x - 1, y - 1);
      const int count = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
      if (count < 2 || count > 6) continue;
      // The transition count must be an int: a bool would collapse 2 and 3 into
      // true and remove interior pixels.
      const int transitions = (!p2 && p3) + (!p3 && p4) + (!p4 && p5) +
                              (!p5 && p6) + (!p6 && p7) + (!p7 && p8) +
                              (!p8 && p9) + (!p9 && p2);
      if (transitions != 1) continue;
      if (first) {
        if (p2 && p4 && p6) continue;
        if (p4 && p6 && p8) continue;
      } else {
        if (p2 && p4 && p8) continue;
        if (p2 && p6 && p8) continue;
      }
      remove.emplace_back(x, y);
    }
  }
  for (const auto& point : remove) image->at(point.first, point.second) = 0;
  return !remove.empty();
}

struct Branch {
  std::vector<std::pair<int, int>> points;
  int endA = -1;
  int endB = -1;
  bool merged = false;
};

double directionOf(const std::vector<std::pair<int, int>>& points, bool atEnd) {
  const int n = static_cast<int>(points.size());
  if (n < 3) return 0.0;
  const int span = std::min(12, n - 1);
  const auto& tip = atEnd ? points[n - 1] : points[0];
  const auto& tail = atEnd ? points[n - 1 - span] : points[span];
  return std::atan2(static_cast<double>(tip.second - tail.second),
                    static_cast<double>(tip.first - tail.first));
}

double angleBetween(double a, double b) {
  double delta = std::fabs(a - b);
  while (delta > kPi) delta = std::fabs(delta - 2.0 * kPi);
  return delta * 180.0 / kPi;
}

}  // namespace

GrayImage thin(const GrayImage& binary, int maxPasses) {
  GrayImage image = binary;
  if (image.empty()) return image;
  // Zhang-Suen only reads the interior, so ink on the border would never thin.
  for (int x = 0; x < image.width; ++x) {
    if (image.at(x, 0) > 127) image.at(x, 0) = 0;
    if (image.at(x, image.height - 1) > 127) image.at(x, image.height - 1) = 0;
  }
  for (int y = 0; y < image.height; ++y) {
    if (image.at(0, y) > 127) image.at(0, y) = 0;
    if (image.at(image.width - 1, y) > 127) image.at(image.width - 1, y) = 0;
  }
  for (int pass = 0; pass < maxPasses; ++pass) {
    const bool changed = thinningPass(&image, true);
    const bool changed2 = thinningPass(&image, false);
    if (!changed && !changed2) break;
  }
  return image;
}

std::vector<InkStroke> traceStrokes(const GrayImage& binary,
                                    const TraceOptions& options) {
  std::vector<InkStroke> strokes;
  if (binary.empty()) return strokes;
  const GrayImage skeleton = thin(binary, options.maxThinningPasses);

  // Junctions are cut out so every remaining blob is a simple branch. The cut
  // is dilated by one pixel: removing only the junction pixel itself leaves two
  // L-shaped halves that then look like one bent stroke, which is exactly the
  // wrong answer for an "x" or a "+".
  GrayImage cut = skeleton;
  std::vector<std::pair<int, int>> junctions;
  for (int y = 1; y < skeleton.height - 1; ++y) {
    for (int x = 1; x < skeleton.width - 1; ++x) {
      if (skeleton.at(x, y) <= 127) continue;
      if (crossingNumber(skeleton, x, y) >= 3) junctions.emplace_back(x, y);
    }
  }
  for (const auto& junction : junctions) {
    // One pixel of margin: without it the arms stay connected around the
    // junction and the whole glyph traces as a single bent path.
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        const int x = junction.first + dx;
        const int y = junction.second + dy;
        if (x >= 0 && y >= 0 && x < cut.width && y < cut.height) {
          cut.at(x, y) = 0;
        }
      }
    }
  }

  // Connected components of the cut skeleton, each traced in order.
  std::vector<std::vector<std::pair<int, int>>> branches;
  std::vector<uint8_t> seen(static_cast<std::size_t>(cut.width) * cut.height, 0);
  for (int y = 0; y < cut.height; ++y) {
    for (int x = 0; x < cut.width; ++x) {
      if (cut.at(x, y) <= 127) continue;
      const std::size_t index = static_cast<std::size_t>(y) * cut.width + x;
      if (seen[index]) continue;
      std::vector<std::pair<int, int>> blob;
      std::queue<std::pair<int, int>> queue;
      queue.emplace(x, y);
      seen[index] = 1;
      while (!queue.empty()) {
        const auto point = queue.front();
        queue.pop();
        blob.push_back(point);
        for (int k = 0; k < 8; ++k) {
          const int nx = point.first + kDx[k];
          const int ny = point.second + kDy[k];
          if (nx < 0 || ny < 0 || nx >= cut.width || ny >= cut.height) continue;
          if (cut.at(nx, ny) <= 127) continue;
          const std::size_t ni = static_cast<std::size_t>(ny) * cut.width + nx;
          if (seen[ni]) continue;
          seen[ni] = 1;
          queue.emplace(nx, ny);
        }
      }
      int bx0 = blob.front().first, bx1 = bx0;
      int by0 = blob.front().second, by1 = by0;
      for (const auto& point : blob) {
        bx0 = std::min(bx0, point.first);
        bx1 = std::max(bx1, point.first);
        by0 = std::min(by0, point.second);
        by1 = std::max(by1, point.second);
      }
      // A dot thins down to a couple of pixels; size alone would drop it.
      const bool compactBlob = bx1 - bx0 <= 8 && by1 - by0 <= 8;
      if (!compactBlob &&
          static_cast<int>(blob.size()) < options.minLength) {
        continue;
      }

      // Walk the blob from one of its ends so the points are in pen order.
      std::vector<std::pair<int, int>> ordered;
      std::pair<int, int> start = blob.front();
      int bestDegree = 9;
      for (const auto& point : blob) {
        int degree = 0;
        for (int k = 0; k < 8; ++k) {
          const auto probe = std::make_pair(point.first + kDx[k],
                                            point.second + kDy[k]);
          if (std::find(blob.begin(), blob.end(), probe) != blob.end()) ++degree;
        }
        if (degree < bestDegree) {
          bestDegree = degree;
          start = point;
        }
      }
      std::vector<uint8_t> visited(static_cast<std::size_t>(cut.width) * cut.height, 0);
      std::pair<int, int> current = start;
      ordered.push_back(current);
      visited[static_cast<std::size_t>(current.second) * cut.width + current.first] = 1;
      bool moved = true;
      while (moved) {
        moved = false;
        for (int k = 0; k < 8; ++k) {
          const std::pair<int, int> next{current.first + kDx[k],
                                         current.second + kDy[k]};
          if (std::find(blob.begin(), blob.end(), next) == blob.end()) continue;
          const std::size_t ni =
              static_cast<std::size_t>(next.second) * cut.width + next.first;
          if (visited[ni]) continue;
          visited[ni] = 1;
          ordered.push_back(next);
          current = next;
          moved = true;
          break;
        }
      }
      // A component that turns sharply is two arms that were still joined at a
      // junction, so split it there. Without this an "x" comes back as one bent
      // stroke instead of two crossing ones.
      if (ordered.size() < 6) {
        // A dot or a very short tick: it is a symbol on its own, so it skips
        // the turn splitting and goes straight into the branch list.
        branches.push_back(ordered);
        continue;
      }
      std::size_t segmentStart = 0;
      for (std::size_t i = 2; i + 2 < ordered.size(); ++i) {
        const double ax = static_cast<double>(ordered[i].first - ordered[i - 2].first);
        const double ay = static_cast<double>(ordered[i].second - ordered[i - 2].second);
        const double bx = static_cast<double>(ordered[i + 2].first - ordered[i].first);
        const double by = static_cast<double>(ordered[i + 2].second - ordered[i].second);
        const double na = std::hypot(ax, ay);
        const double nb = std::hypot(bx, by);
        if (na < 2.0 || nb < 2.0) continue;
        const double cosine = (ax * bx + ay * by) / (na * nb);
        const double turn = std::acos(std::max(-1.0, std::min(1.0, cosine))) * 180.0 / kPi;
        if (turn < 75.0) continue;
        if (i - segmentStart >= 6) {
          branches.emplace_back(
              ordered.begin() + static_cast<std::ptrdiff_t>(segmentStart),
              ordered.begin() + static_cast<std::ptrdiff_t>(i + 1));
        }
        segmentStart = i;
      }
      if (ordered.size() - segmentStart >= 6) {
        branches.emplace_back(
            ordered.begin() + static_cast<std::ptrdiff_t>(segmentStart),
            ordered.end());
      }
    }
  }

  // Junction clusters: adjacent junction pixels form one blob as wide as the
  // pen, and the missing piece between two branches is exactly that blob.
  std::vector<Box> clusters;
  {
    std::vector<uint8_t> taken(junctions.size(), 0);
    for (std::size_t i = 0; i < junctions.size(); ++i) {
      if (taken[i]) continue;
      Box box{junctions[i].first, junctions[i].second,
              junctions[i].first + 1, junctions[i].second + 1};
      std::vector<std::size_t> stack{i};
      taken[i] = 1;
      while (!stack.empty()) {
        const std::size_t at = stack.back();
        stack.pop_back();
        box.include(Box{junctions[at].first, junctions[at].second,
                        junctions[at].first + 1, junctions[at].second + 1});
        for (std::size_t j = 0; j < junctions.size(); ++j) {
          if (taken[j]) continue;
          if (std::abs(junctions[j].first - junctions[at].first) <= 2 &&
              std::abs(junctions[j].second - junctions[at].second) <= 2) {
            taken[j] = 1;
            stack.push_back(j);
          }
        }
      }
      clusters.push_back(box);
    }
  }
  const auto bridgeDistance = [&clusters](const std::pair<int, int>& a,
                                          const std::pair<int, int>& b) {
    for (const Box& cluster : clusters) {
      const int pad = 14;
      const bool inA = a.first >= cluster.x0 - pad && a.first <= cluster.x1 + pad &&
                       a.second >= cluster.y0 - pad && a.second <= cluster.y1 + pad;
      const bool inB = b.first >= cluster.x0 - pad && b.first <= cluster.x1 + pad &&
                       b.second >= cluster.y0 - pad && b.second <= cluster.y1 + pad;
      if (inA && inB) return true;
    }
    return false;
  };

  // Join branches that continue straight through a junction.
  std::vector<Branch> merged(branches.size());
  for (std::size_t i = 0; i < branches.size(); ++i) {
    merged[i].points = branches[i];
  }
  const double joinRadius = options.joinRadius;
  struct Candidate {
    double score = 0.0;
    std::size_t a = 0;
    std::size_t b = 0;
    bool aEnd = false;
    bool bEnd = false;
  };
  for (int round = 0; round < 64; ++round) {
    std::vector<Candidate> candidates;
    for (std::size_t a = 0; a < merged.size(); ++a) {
      if (merged[a].merged || merged[a].points.size() < 2) continue;
      for (std::size_t b = a + 1; b < merged.size(); ++b) {
        if (merged[b].merged || merged[b].points.size() < 2) continue;
        for (int ea = 0; ea < 2; ++ea) {
          for (int eb = 0; eb < 2; ++eb) {
            const auto& pa = ea ? merged[a].points.back() : merged[a].points.front();
            const auto& pb = eb ? merged[b].points.back() : merged[b].points.front();
            const double distance =
                std::hypot(pa.first - pb.first, pa.second - pb.second);
            if (distance > joinRadius && !bridgeDistance(pa, pb)) continue;
            const double da = directionOf(merged[a].points, ea == 1);
            const double db = directionOf(merged[b].points, eb == 1);
            const double score = std::fabs(180.0 - angleBetween(da, db));
            if (score >= options.joinDegrees) continue;
            candidates.push_back(Candidate{score, a, b, ea == 1, eb == 1});
          }
        }
      }
    }
    if (candidates.empty()) break;
    // Best continuation first, and a branch joins only once per round: this is
    // what keeps "+" from collapsing into a single bent stroke.
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& x, const Candidate& y) {
                return x.score < y.score;
              });
    const Candidate best = candidates.front();
    if (merged[best.a].merged || merged[best.b].merged) break;
    Branch& a = merged[best.a];
    Branch& b = merged[best.b];
    std::vector<std::pair<int, int>> tail = b.points;
    if (best.bEnd == best.aEnd) std::reverse(tail.begin(), tail.end());
    if (best.aEnd) {
      a.points.insert(a.points.end(), tail.begin(), tail.end());
    } else {
      std::vector<std::pair<int, int>> head(tail.rbegin(), tail.rend());
      head.insert(head.end(), a.points.begin(), a.points.end());
      a.points = std::move(head);
    }
    b.points.clear();
    b.merged = true;
  }

  for (const Branch& branch : merged) {
    if (branch.points.empty()) continue;
    const int spanX = branch.points.back().first - branch.points.front().first;
    const int spanY = branch.points.back().second - branch.points.front().second;
    const bool compact =
        std::abs(spanX) <= 6 && std::abs(spanY) <= 6 &&
        branch.points.size() >= 1;
    if (!compact &&
        branch.points.size() < static_cast<std::size_t>(options.minLength)) {
      continue;
    }
    InkStroke stroke;
    stroke.points.reserve(branch.points.size());
    int minX = branch.points.front().first;
    int maxX = minX;
    int minY = branch.points.front().second;
    int maxY = minY;
    for (const auto& point : branch.points) {
      stroke.points.push_back(
          InkPoint{static_cast<float>(point.first), static_cast<float>(point.second)});
      minX = std::min(minX, point.first);
      maxX = std::max(maxX, point.first);
      minY = std::min(minY, point.second);
      maxY = std::max(maxY, point.second);
    }
    stroke.box = Box{minX, minY, maxX + 1, maxY + 1};
    strokes.push_back(std::move(stroke));
    if (strokes.size() >= options.maxStrokes) break;
  }
  return strokes;
}

}  // namespace infinit::scan
