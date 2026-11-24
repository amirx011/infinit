#include "segment.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>

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
  // A dot is about as wide as the stem it belongs to; an infinity sign above a
  // summation sign is only half as wide as the operator.
  if (small.width() < 0.5 * big.width()) return false;
  const int gap = big.y0 - small.y1;
  if (gap < 0 || gap > 0.22 * big.height()) return false;
  return small.centerX() >= big.x0 - big.width() * 0.1 &&
         small.centerX() <= big.x1 + big.width() * 0.1;
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

  const int w = binary.width;
  const int h = binary.height;
  std::vector<int> labels(static_cast<size_t>(w) * h, -1);
  std::vector<int> stack;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t start = static_cast<size_t>(y) * w + x;
      if (binary.pixels[start] <= 127 || labels[start] != -1) continue;

      Component comp;
      comp.id = static_cast<int>(out.size());
      comp.box = Box{x, y, x + 1, y + 1};
      stack.clear();
      stack.push_back(static_cast<int>(start));
      labels[start] = comp.id;

      while (!stack.empty()) {
        const int index = stack.back();
        stack.pop_back();
        const int cx = index % w;
        const int cy = index / w;
        ++comp.pixels;
        comp.sumX += cx;
        comp.sumY += cy;
        comp.box.include(Box{cx, cy, cx + 1, cy + 1});

        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            const int nx = cx + dx;
            const int ny = cy + dy;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            const size_t next = static_cast<size_t>(ny) * w + nx;
            if (binary.pixels[next] <= 127 || labels[next] != -1) continue;
            labels[next] = comp.id;
            stack.push_back(static_cast<int>(next));
          }
        }
      }

      if (comp.pixels >= minPixels) out.push_back(comp);
    }
  }
  return out;
}

Segmentation segment(const GrayImage& binary, const SegmentOptions& options) {
  Segmentation seg;
  seg.components = connectedComponents(binary, options.minPixels);
  if (seg.components.empty()) return seg;

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

      // Side-by-side fragments of one glyph. Two separate glyphs never overlap
      // horizontally, so a positive gap is left to the repair stage instead of
      // being merged blindly.
      const int combinedWidth = std::max(a.x1, b.x1) - std::min(a.x0, b.x0);
      if (hGap <= 0 && vOverlapRatio >= options.fragmentOverlapRatio &&
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

  std::sort(seg.symbols.begin(), seg.symbols.end(), [](const Symbol& a, const Symbol& b) {
    if (a.box.y0 != b.box.y0) return a.box.y0 < b.box.y0;
    return a.box.x0 < b.box.x0;
  });
  for (size_t i = 0; i < seg.symbols.size(); ++i) {
    seg.symbols[i].id = static_cast<int>(i);
  }

  // Group symbols into text lines by vertical overlap.
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
