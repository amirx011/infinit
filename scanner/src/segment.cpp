#include "segment.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "cv_bridge.hpp"

namespace infinit::scan {
namespace {

struct UnionFind {
  std::vector<int> parent;

  explicit UnionFind(size_t n) : parent(n) {
    std::iota(parent.begin(), parent.end(), 0);
  }

  int find(int x) {
    while (parent[x] != x) {
      parent[x] = parent[parent[x]];
      x = parent[x];
    }
    return x;
  }

  void unite(int a, int b) {
    a = find(a);
    b = find(b);
    if (a != b) parent[std::max(a, b)] = std::min(a, b);
  }
};

int overlap(int a0, int a1, int b0, int b1) {
  return std::max(0, std::min(a1, b1) - std::max(a0, b0));
}

double ratio(double numerator, double denominator) {
  return denominator <= 0.0 ? 0.0 : numerator / denominator;
}

/// True when `small` looks like a dot or accent sitting on top of `big`.
bool isDiacriticOf(const Box& small, const Box& big) {
  if (small.height() > 0.4 * big.height()) return false;
  // A bar (a fraction line, an equals sign) is too flat to be an accent.
  if (small.width() > 2.5 * small.height()) return false;
  // Only compact marks belong on top of a glyph. An infinity sign over a
  // summation is twice as wide as it is tall and is a bound of its own, not a
  // diacritic; merging it hid the upper limit and turned the sum into "±".
  // A dot drawn wide and flat (the synthetic dotted-i case) is still a dot when
  // it is as wide as the stem it caps.
  // A dot is round; an infinity sign is half again as wide as it is tall and
  // must stay a symbol of its own so the summation keeps its upper bound.
  const bool dotLike = small.width() <= 1.25 * small.height();
  const bool flatCap = small.width() >= 0.80 * big.width() &&
                       small.height() <= 0.25 * big.height();
  if (!dotLike && !flatCap) return false;
  // A dot can be narrower than half of a wide stem (the dot of an italic "i"
  // sits on a slanted stroke), so the ratio is looser than the old 0.5.
  if (small.width() < 0.30 * big.width()) return false;
  // A ruled line or a rotation nudges the dot of an "i" a few rows higher than
  // the tight gap most accents have; a round dot is still a dot.
  const bool roundDot =
      dotLike && small.height() <= 0.25 * big.height() && small.width() > 0;
  const int gap = big.y0 - small.y1;
  if (gap < 0 || gap > (roundDot ? 0.50 : 0.30) * big.height()) return false;
  return small.centerX() >= big.x0 - big.width() * 0.1 &&
         small.centerX() <= big.x1 + big.width() * 0.1;
}

/// Fills seg.lines from seg.symbols by vertical overlap. Shared by the initial
/// segmentation and by manual merge/split, which rebuild symbols in place.
void assignLines(Segmentation& seg) {
  seg.lines.clear();
  for (Symbol& symbol : seg.symbols) {
    int bestLine = -1;
    double bestScore = 0.0;
    for (size_t l = 0; l < seg.lines.size(); ++l) {
      const Box& lineBox = seg.lines[l].box;
      const int vOverlap = symbol.box.overlapY(lineBox);
      const double score = ratio(vOverlap, std::min(symbol.box.height(), lineBox.height()));
      if (score > bestScore) {
        bestScore = score;
        bestLine = static_cast<int>(l);
      }
    }
    if (bestLine < 0 || bestScore < 0.45) {
      TextLine line;
      line.id = static_cast<int>(seg.lines.size());
      line.box = symbol.box;
      seg.lines.push_back(line);
      bestLine = static_cast<int>(seg.lines.size()) - 1;
    }
    seg.lines[bestLine].box.include(symbol.box);
    symbol.line = bestLine;
    seg.lines[bestLine].symbols.push_back(symbol.id);
  }

  std::sort(seg.lines.begin(), seg.lines.end(), [](const TextLine& a, const TextLine& b) {
    return a.box.y0 < b.box.y0;
  });
  for (size_t l = 0; l < seg.lines.size(); ++l) {
    seg.lines[l].id = static_cast<int>(l);
    std::vector<int> heights;
    heights.reserve(seg.lines[l].symbols.size());
    for (int id : seg.lines[l].symbols) {
      seg.symbols[id].line = static_cast<int>(l);
      heights.push_back(seg.symbols[id].box.height());
    }
    if (!heights.empty()) {
      const size_t mid = heights.size() / 2;
      std::nth_element(heights.begin(), heights.begin() + mid, heights.end());
      seg.lines[l].medianHeight = heights[mid];
    }
    std::sort(seg.lines[l].symbols.begin(), seg.lines[l].symbols.end(),
              [&seg](int a, int b) {
                return seg.symbols[a].box.x0 < seg.symbols[b].box.x0;
              });
  }

}

}  // namespace

void Box::include(const Box& other) {
  if (other.empty()) return;
  if (empty()) {
    *this = other;
    return;
  }
  x0 = std::min(x0, other.x0);
  y0 = std::min(y0, other.y0);
  x1 = std::max(x1, other.x1);
  y1 = std::max(y1, other.y1);
}

bool Box::intersects(const Box& other) const {
  return x0 < other.x1 && other.x0 < x1 && y0 < other.y1 && other.y0 < y1;
}

int Box::overlapX(const Box& other) const {
  return overlap(x0, x1, other.x0, other.x1);
}

int Box::overlapY(const Box& other) const {
  return overlap(y0, y1, other.y0, other.y1);
}

std::vector<Component> connectedComponents(const GrayImage& binary,
                                           int minPixels) {
  std::vector<Component> out;
  if (binary.empty()) return out;

  cv::Mat labels;
  cv::Mat stats;
  cv::Mat centroids;
  const int count = cv::connectedComponentsWithStats(
      view(binary), labels, stats, centroids, 8, CV_32S);
  out.reserve(static_cast<size_t>(std::max(0, count - 1)));
  for (int label = 1; label < count; ++label) {
    const int area = stats.at<int>(label, cv::CC_STAT_AREA);
    if (area < minPixels) continue;
    Component comp;
    comp.id = static_cast<int>(out.size());
    const int x = stats.at<int>(label, cv::CC_STAT_LEFT);
    const int y = stats.at<int>(label, cv::CC_STAT_TOP);
    const int w = stats.at<int>(label, cv::CC_STAT_WIDTH);
    const int h = stats.at<int>(label, cv::CC_STAT_HEIGHT);
    comp.box = Box{x, y, x + w, y + h};
    comp.pixels = area;
    comp.sumX = centroids.at<double>(label, 0) * area;
    comp.sumY = centroids.at<double>(label, 1) * area;
    out.push_back(comp);
  }
  return out;
}

int countHoles(const GrayImage& binary, const Box& box) {
  if (binary.empty() || box.empty()) return 0;
  const int x0 = std::max(0, box.x0);
  const int y0 = std::max(0, box.y0);
  const int x1 = std::min(binary.width, box.x1);
  const int y1 = std::min(binary.height, box.y1);
  if (x1 - x0 < 2 || y1 - y0 < 2) return 0;
  cv::Mat crop = view(binary)(cv::Rect(x0, y0, x1 - x0, y1 - y0)).clone();
  std::vector<std::vector<cv::Point>> contours;
  std::vector<cv::Vec4i> hierarchy;
  cv::findContours(crop, contours, hierarchy, cv::RETR_CCOMP,
                   cv::CHAIN_APPROX_SIMPLE);
  int holes = 0;
  for (size_t i = 0; i < contours.size(); ++i) {
    if (!hierarchy.empty() && hierarchy[i][3] >= 0) ++holes;
  }
  return holes;
}

/// Splits components that are two glyphs fused together, at the emptiest
/// vertical-projection column inside them. This is the plan's §4.3.1 rule; the
/// alternative (one wide symbol) stays reachable through the correction UI.
void splitWideComponents(const GrayImage& binary,
                         const SegmentOptions& options,
                         std::vector<Component>* components) {
  if (!options.splitTouching || components->empty()) return;
  std::vector<double> heights;
  std::vector<double> widths;
  heights.reserve(components->size());
  widths.reserve(components->size());
  for (const Component& component : *components) {
    if (component.box.height() > 2) heights.push_back(component.box.height());
    if (component.box.width() > 2) widths.push_back(component.box.width());
  }
  if (heights.empty() || widths.empty()) return;
  std::sort(heights.begin(), heights.end());
  std::sort(widths.begin(), widths.end());
  const double medianHeight = heights[heights.size() / 2];
  const double medianWidth = widths[widths.size() / 2];
  const int limit = std::max(
      8, static_cast<int>(std::lround(medianWidth * options.splitWidthRatio)));

  std::vector<Component> out;
  out.reserve(components->size());
  for (const Component& component : *components) {
    if (component.box.width() <= limit) {
      out.push_back(component);
      continue;
    }
    // A radical or a big operator is wide on purpose; splitting one destroys a
    // structure the next stage needs.
    if (component.box.height() > medianHeight * 1.5) {
      out.push_back(component);
      continue;
    }
    // Short glyphs are x-height letters, and the classic short wide glyph is
    // "m": its two humps have a valley that looks exactly like a seam. Touching
    // digits and capitals are full height, so only those are candidates.
    if (component.box.height() < medianHeight * 0.95) {
      out.push_back(component);
      continue;
    }
    const Box& box = component.box;
    // Ink per column inside the box.
    std::vector<int> columns(static_cast<size_t>(box.width()), 0);
    int total = 0;
    for (int y = std::max(0, box.y0); y < std::min(binary.height, box.y1); ++y) {
      for (int x = std::max(0, box.x0); x < std::min(binary.width, box.x1); ++x) {
        if (binary.at(x, y) > 127) {
          ++columns[static_cast<size_t>(x - box.x0)];
          ++total;
        }
      }
    }
    if (total <= 0) {
      out.push_back(component);
      continue;
    }
    const double average =
        static_cast<double>(total) / static_cast<double>(box.width());
    const double density =
        box.area() > 0 ? static_cast<double>(total) / box.area() : 0.0;
    if (density < options.splitDensity) {
      out.push_back(component);
      continue;
    }
    // Look for the emptiest column in the middle half: the seam between two
    // touching glyphs sits there, never at the edges.
    int best = -1;
    int bestInk = std::max(1, static_cast<int>(average * options.splitValleyRatio));
    for (int x = box.width() / 4; x < box.width() - box.width() / 4; ++x) {
      if (columns[static_cast<size_t>(x)] < bestInk) {
        bestInk = columns[static_cast<size_t>(x)];
        best = x;
      }
    }
    if (best < 0) {
      out.push_back(component);
      continue;
    }
    Component left;
    Component right;
    left.box = Box{box.x0, box.y0, box.x0 + best, box.y1};
    right.box = Box{box.x0 + best, box.y0, box.x1, box.y1};
    int leftInk = 0;
    int rightInk = 0;
    for (auto value : columns) {
      (void)value;
    }
    for (int x = 0; x < box.width(); ++x) {
      if (x < best) {
        leftInk += columns[static_cast<size_t>(x)];
      } else {
        rightInk += columns[static_cast<size_t>(x)];
      }
    }
    // Both halves must carry real ink and look like glyphs, otherwise this was
    // one glyph with a thin stroke (a "1", a bracket) or a letter pair and
    // splitting would invent a symbol.
    const int leftWidth = best;
    const int rightWidth = box.width() - best;
    const double minHalf = 0.45 * medianWidth;
    const double maxHalf = 1.70 * medianWidth;
    // Two touching glyphs are roughly the same size; a 3:1 "split" is a stroke
    // next to a blob, which is one glyph (an "l" beside its dot, a bracket
    // beside its content).
    const double symmetry =
        static_cast<double>(std::min(leftWidth, rightWidth)) /
        std::max(1, std::max(leftWidth, rightWidth));
    if (leftInk < options.minPixels || rightInk < options.minPixels ||
        leftWidth < minHalf || rightWidth < minHalf ||
        leftWidth > maxHalf || rightWidth > maxHalf || symmetry < 0.62) {
      out.push_back(component);
      continue;
    }
    left.pixels = leftInk;
    right.pixels = rightInk;
    left.id = static_cast<int>(out.size());
    out.push_back(left);
    right.id = static_cast<int>(out.size());
    out.push_back(right);
  }
  *components = std::move(out);
}

Segmentation segment(const GrayImage& binary, const SegmentOptions& options) {
  Segmentation seg;
  seg.components = connectedComponents(binary, options.minPixels);
  if (seg.components.empty()) return seg;
  splitWideComponents(binary, options, &seg.components);

  UnionFind uf(seg.components.size());
  const size_t count = seg.components.size();
  for (size_t i = 0; i < count; ++i) {
    const Box& a = seg.components[i].box;
    for (size_t j = i + 1; j < count; ++j) {
      const Box& b = seg.components[j].box;
      const int taller = std::max(a.height(), b.height());
      const int vOverlap = a.overlapY(b);
      const int hOverlap = a.overlapX(b);
      const int hGap = std::max(0, std::max(a.x0, b.x0) - std::min(a.x1, b.x1));
      const int vGap = std::max(0, std::max(a.y0, b.y0) - std::min(a.y1, b.y1));

      const double vOverlapRatio =
          ratio(vOverlap, std::min(a.height(), b.height()));
      const double hOverlapRatio =
          ratio(hOverlap, std::min(a.width(), b.width()));
      const double widthRatio =
          ratio(std::min(a.width(), b.width()),
                std::max(a.width(), b.width()));
      const double narrower = std::min(a.width(), b.width());
      const double barLike = options.barAspectRatio;
      const double heightRatio =
          ratio(std::min(a.height(), b.height()), taller);

      // Side-by-side fragments of one glyph. Two separate glyphs never overlap
      // horizontally, so a positive gap is left to the repair stage instead of
      // being merged blindly. Italic glyphs *do* overlap a following bracket by
      // a few kerned pixels, so fragments must also be of comparable height;
      // an x under a closing parenthesis is not one broken glyph.
      const int combinedWidth = std::max(a.x1, b.x1) - std::min(a.x0, b.x0);
      if (hGap <= 0 && vOverlapRatio >= options.fragmentOverlapRatio &&
          heightRatio >= options.fragmentHeightRatio &&
          combinedWidth <= options.fragmentWidthRatio * taller) {
        uf.unite(static_cast<int>(i), static_cast<int>(j));
        continue;
      }
      // The two bars of '=': both thin, aligned, and close together.
      if (a.height() <= barLike * a.width() && b.height() <= barLike * b.width() &&
          widthRatio >= 0.70 && hOverlapRatio >= 0.70 &&
          vGap <= options.stackedBarGapRatio * narrower) {
        uf.unite(static_cast<int>(i), static_cast<int>(j));
        continue;
      }
      // A dot or accent on top of a taller glyph.
      if (isDiacriticOf(a, b) || isDiacriticOf(b, a)) {
        uf.unite(static_cast<int>(i), static_cast<int>(j));
      }
    }
  }

  // "÷" is a dot above and a dot below one short bar. Both dots have to be
  // present: a single dot near a wide bar is a decimal point or a centered
  // multiplication dot, and merging it into a fraction bar would break the
  // fraction.
  for (size_t bar = 0; bar < seg.components.size(); ++bar) {
    const Box& barBox = seg.components[bar].box;
    if (barBox.height() > 0.45 * std::max(1, barBox.width())) continue;
    if (barBox.width() < 3 * std::max(1, barBox.height())) continue;
    int above = -1;
    int below = -1;
    for (size_t dot = 0; dot < seg.components.size(); ++dot) {
      if (dot == bar) continue;
      const Box& dotBox = seg.components[dot].box;
      if (dotBox.width() > 0.45 * barBox.width()) continue;
      if (dotBox.height() > 0.9 * std::max(1, barBox.height()) + 3) continue;
      const int centerX = dotBox.centerX();
      if (centerX < barBox.x0 || centerX > barBox.x1) continue;
      const int gapAbove = barBox.y0 - dotBox.y1;
      const int gapBelow = dotBox.y0 - barBox.y1;
      if (gapAbove >= 0 && gapAbove <= 0.9 * barBox.height() + 3) above = static_cast<int>(dot);
      if (gapBelow >= 0 && gapBelow <= 0.9 * barBox.height() + 3) below = static_cast<int>(dot);
    }
    if (above >= 0 && below >= 0 && above != below) {
      uf.unite(static_cast<int>(bar), above);
      uf.unite(static_cast<int>(bar), below);
    }
  }

  std::vector<int> groupOf(seg.components.size(), -1);
  std::vector<int> groupIndex(seg.components.size(), -1);
  for (size_t i = 0; i < seg.components.size(); ++i) {
    const int root = uf.find(static_cast<int>(i));
    if (groupIndex[root] < 0) {
      groupIndex[root] = static_cast<int>(seg.symbols.size());
      Symbol symbol;
      symbol.id = static_cast<int>(seg.symbols.size());
      seg.symbols.push_back(symbol);
    }
    groupOf[i] = groupIndex[root];
  }
  for (size_t i = 0; i < seg.components.size(); ++i) {
    Symbol& symbol = seg.symbols[groupOf[i]];
    symbol.components.push_back(static_cast<int>(i));
    symbol.box.include(seg.components[i].box);
  }

  if (static_cast<int>(seg.symbols.size()) > options.maxSymbols) {
    seg.overSegmented = true;
    seg.symbols.resize(static_cast<size_t>(options.maxSymbols));
  }

  std::sort(seg.symbols.begin(), seg.symbols.end(), [](const Symbol& a, const Symbol& b) {
    if (a.box.y0 != b.box.y0) return a.box.y0 < b.box.y0;
    return a.box.x0 < b.box.x0;
  });
  for (size_t i = 0; i < seg.symbols.size(); ++i) {
    seg.symbols[i].id = static_cast<int>(i);
  }

  assignLines(seg);

  return seg;
}

Segmentation regroup(const Segmentation& original,
                     const std::vector<std::vector<int>>& groups) {
  Segmentation seg;
  seg.components = original.components;
  const int componentCount = static_cast<int>(seg.components.size());
  for (const std::vector<int>& group : groups) {
    Symbol symbol;
    for (int component : group) {
      if (component < 0 || component >= componentCount) continue;
      symbol.components.push_back(component);
      symbol.box.include(seg.components[static_cast<size_t>(component)].box);
    }
    if (symbol.components.empty() || symbol.box.empty()) continue;
    symbol.id = static_cast<int>(seg.symbols.size());
    seg.symbols.push_back(symbol);
  }
  if (seg.symbols.empty()) return seg;
  std::sort(seg.symbols.begin(), seg.symbols.end(),
            [](const Symbol& a, const Symbol& b) {
              if (a.box.y0 != b.box.y0) return a.box.y0 < b.box.y0;
              return a.box.x0 < b.box.x0;
            });
  for (size_t i = 0; i < seg.symbols.size(); ++i) {
    seg.symbols[i].id = static_cast<int>(i);
  }
  assignLines(seg);
  return seg;
}

RgbImage renderSegmentation(const GrayImage& source, const Segmentation& seg) {
  RgbImage out(source.width, source.height);
  for (int y = 0; y < source.height; ++y) {
    for (int x = 0; x < source.width; ++x) {
      const uint8_t v = source.at(x, y);
      out.set(x, y, v, v, v);
    }
  }
  for (const TextLine& line : seg.lines) {
    out.rect(line.box.x0 - 2, line.box.y0 - 2, line.box.x1 + 2, line.box.y1 + 2,
             60, 130, 255, 1);
  }
  for (const Symbol& symbol : seg.symbols) {
    out.rect(symbol.box.x0, symbol.box.y0, symbol.box.x1, symbol.box.y1,
             60, 220, 120, 1);
  }
  return out;
}

}  // namespace infinit::scan
