#include "ink.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "classify.hpp"
#include "cv_bridge.hpp"

namespace infinit::scan {
namespace {

constexpr int kMaxWidth = 4096;
constexpr int kMaxHeight = 4096;
constexpr int kMaxStrokes = 600;
constexpr int kMaxPointsPerStroke = 4096;

std::uint32_t readU32(const uint8_t*& cursor, const uint8_t* end) {
  if (cursor + 4 > end) return 0;
  std::uint32_t value = 0;
  std::memcpy(&value, cursor, sizeof(value));
  cursor += 4;
  return value;
}

float readF32(const uint8_t*& cursor, const uint8_t* end) {
  if (cursor + 4 > end) return 0.0f;
  float value = 0.0f;
  std::memcpy(&value, cursor, sizeof(value));
  cursor += 4;
  return value;
}

double medianOf(std::vector<double> values) {
  if (values.empty()) return 0.0;
  const size_t middle = values.size() / 2;
  std::nth_element(values.begin(), values.begin() + middle, values.end());
  return values[middle];
}

int overlapY(const Box& a, const Box& b) {
  return std::max(0, std::min(a.y1, b.y1) - std::max(a.y0, b.y0));
}

int overlapX(const Box& a, const Box& b) {
  return std::max(0, std::min(a.x1, b.x1) - std::max(a.x0, b.x0));
}

double ratio(int part, int whole) {
  return whole > 0 ? static_cast<double>(part) / whole : 0.0;
}

bool shouldMerge(const Box& a, const Box& b, double medianHeight) {
  if (a.empty() || b.empty()) return false;
  const int gapX = std::max(0, std::max(a.x0, b.x0) - std::min(a.x1, b.x1));
  const int gapY = std::max(0, std::max(a.y0, b.y0) - std::min(a.y1, b.y1));
  const int minW = std::min(a.width(), b.width());
  const int maxW = std::max(a.width(), b.width());
  const int minH = std::min(a.height(), b.height());
  const int maxH = std::max(a.height(), b.height());
  const int sharedX = overlapX(a, b);
  const int sharedY = overlapY(a, b);

  // Crossing or touching strokes: the arms of +, ×, ≤, or a cursive join.
  if (gapX <= std::max(2.0, 0.18 * medianHeight) &&
      sharedY >= 0.35 * minH) {
    return true;
  }

  // Stacked strokes of similar width: the two bars of =, the dot of i, a
  // colon, a prime. A fraction bar is much wider than its numerator, so the
  // width ratio keeps them apart.
  if (sharedX >= 0.55 * minW &&
      gapY <= 0.55 * std::max(maxW, maxH) && maxW <= 2.5 * minW) {
    return true;
  }

  // A small stray mark sitting right next to a much larger stroke: a prime,
  // an accent, the dot of j. Keep it at the top or bottom, not the middle of
  // a long vertical stroke.
  if (minH <= 0.45 * medianHeight && maxW >= 1.6 * minW &&
      maxW <= 1.2 * medianHeight && sharedX >= 0.35 * minW &&
      gapY <= 0.55 * medianHeight) {
    return true;
  }
  return false;
}

double boxGap(const Box& a, const Box& b) {
  const int dx = std::max(0, std::max(a.x0, b.x0) - std::min(a.x1, b.x1));
  const int dy = std::max(0, std::max(a.y0, b.y0) - std::min(a.y1, b.y1));
  return std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy);
}

bool plausibleGroup(const std::vector<InkStroke>& strokes,
                    const std::vector<int>& ids, double medianHeight) {
  for (size_t a = 0; a < ids.size(); ++a) {
    for (size_t b = a + 1; b < ids.size(); ++b) {
      if (!shouldMerge(strokes[static_cast<size_t>(ids[a])].box,
                       strokes[static_cast<size_t>(ids[b])].box,
                       medianHeight)) {
        return false;
      }
    }
  }
  return true;
}

/// Dynamic programming over the stroke sequence. A symbol may cover one to
/// four consecutive strokes (in time order); a multi-stroke hypothesis is only
/// allowed when every pair is geometrically plausible. The cost favours
/// grouping touching strokes, penalises distance and, when the template bank
/// is loaded, a candidate box the classifier cannot name. Late strokes (a dot,
/// a prime, a fraction bar) are moved by `reassignStrays` afterwards.
std::vector<std::vector<int>> latticeGroups(
    const std::vector<InkStroke>& strokes, double medianHeight,
    const GrayImage* binary, const ISymbolClassifier* classifier) {
  const int count = static_cast<int>(strokes.size());
  constexpr double kInf = 1e9;
  constexpr int kMaxStrokesPerSymbol = 4;
  std::vector<double> best(static_cast<size_t>(count) + 1, kInf);
  std::vector<int> previous(static_cast<size_t>(count) + 1, -1);
  std::vector<std::vector<int>> groupAt(static_cast<size_t>(count) + 1);
  best[0] = 0.0;

  const bool scored = classifier != nullptr && binary != nullptr && count <= 120;
  for (int start = 0; start < count; ++start) {
    if (best[static_cast<size_t>(start)] >= kInf) continue;
    std::vector<int> ids;
    Box box = strokes[static_cast<size_t>(start)].box;
    for (int take = 1;
         take <= kMaxStrokesPerSymbol && start + take <= count; ++take) {
      const int index = start + take - 1;
      ids.push_back(index);
      if (take > 1) {
        box.include(strokes[static_cast<size_t>(index)].box);
        if (!plausibleGroup(strokes, ids, medianHeight)) break;
      }
      // Every extra symbol costs something: a plausible multi-stroke group
      // should beat the same strokes read as separate marks.
      double cost = best[static_cast<size_t>(start)] + 0.55;
      if (take > 1) {
        // A small bonus for joining strokes: touching strokes are one symbol
        // unless the spatial or classifier term says otherwise.
        cost += 0.10 * (take - 1);
        for (size_t a = 0; a + 1 < ids.size(); ++a) {
          cost += 0.12 * boxGap(strokes[static_cast<size_t>(ids[a])].box,
                                strokes[static_cast<size_t>(ids[a + 1])].box) /
                  std::max(1.0, medianHeight);
        }
        if (scored) {
          const GlyphMatch match = classifier->classify(*binary, box);
          cost += (1.0 - std::clamp(match.score, 0.0, 1.0)) * 0.9;
          if (match.label.empty() || match.label == "?") cost += 0.4;
        }
      }
      const int end = start + take;
      if (cost < best[static_cast<size_t>(end)]) {
        best[static_cast<size_t>(end)] = cost;
        previous[static_cast<size_t>(end)] = start;
        groupAt[static_cast<size_t>(end)] = ids;
      }
    }
  }

  std::vector<std::vector<int>> groups;
  int at = count;
  while (at > 0 && previous[static_cast<size_t>(at)] >= 0) {
    groups.push_back(groupAt[static_cast<size_t>(at)]);
    at = previous[static_cast<size_t>(at)];
  }
  std::reverse(groups.begin(), groups.end());
  return groups;
}

/// Moves a small stray stroke (dot of i/j, prime, accent) to the nearest
/// plausible symbol. Wide bars and long strokes are never stolen: the bar of a
/// fraction stays its own symbol for the structure stage.
void reassignStrays(const std::vector<InkStroke>& strokes, double medianHeight,
                    std::vector<std::vector<int>>* groups) {
  if (medianHeight <= 0) return;
  for (size_t g = 0; g < groups->size(); ++g) {
    std::vector<int>& group = (*groups)[g];
    if (group.size() != 1) continue;
    const Box stray = strokes[static_cast<size_t>(group.front())].box;
    const double longest = std::max(stray.width(), stray.height());
    if (longest > 0.32 * medianHeight) continue;
    int bestGroup = -1;
    double bestDistance = 1e9;
    for (size_t other = 0; other < groups->size(); ++other) {
      if (other == g) continue;
      Box box;
      for (int id : (*groups)[other]) box.include(strokes[static_cast<size_t>(id)].box);
      if (box.empty()) continue;
      const int dx = std::max(0, std::max(box.x0, stray.x0) - std::min(box.x1, stray.x1));
      const int dy = std::max(0, std::max(box.y0, stray.y0) - std::min(box.y1, stray.y1));
      if (dx > 0.35 * medianHeight || dy > 0.55 * medianHeight) continue;
      const double distance = boxGap(stray, box);
      if (distance < bestDistance) {
        bestDistance = distance;
        bestGroup = static_cast<int>(other);
      }
    }
    if (bestGroup < 0) continue;
    (*groups)[static_cast<size_t>(bestGroup)].push_back(group.front());
    group.clear();
  }
  groups->erase(std::remove_if(groups->begin(), groups->end(),
                               [](const std::vector<int>& ids) {
                                 return ids.empty();
                               }),
                groups->end());
}

}  // namespace

InkDocument decodeInk(const uint8_t* data, std::size_t size) {
  InkDocument document;
  if (data == nullptr || size < 12) return document;
  const uint8_t* cursor = data;
  const uint8_t* end = data + size;
  const std::uint32_t width = readU32(cursor, end);
  const std::uint32_t height = readU32(cursor, end);
  const std::uint32_t strokeCount = readU32(cursor, end);
  if (width == 0 || height == 0 || width > kMaxWidth || height > kMaxHeight ||
      strokeCount > kMaxStrokes) {
    return document;
  }
  document.width = static_cast<int>(width);
  document.height = static_cast<int>(height);
  document.strokes.reserve(strokeCount);
  for (std::uint32_t s = 0; s < strokeCount; ++s) {
    const std::uint32_t pointCount = readU32(cursor, end);
    if (pointCount > kMaxPointsPerStroke) return InkDocument{};
    InkStroke stroke;
    stroke.points.reserve(pointCount);
    for (std::uint32_t p = 0; p < pointCount; ++p) {
      InkPoint point;
      point.x = readF32(cursor, end);
      point.y = readF32(cursor, end);
      if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        return InkDocument{};
      }
      point.x = std::clamp(point.x, 0.0f, static_cast<float>(document.width));
      point.y = std::clamp(point.y, 0.0f, static_cast<float>(document.height));
      stroke.points.push_back(point);
    }
    if (stroke.points.empty()) continue;
    float x0 = stroke.points.front().x;
    float y0 = stroke.points.front().y;
    float x1 = x0;
    float y1 = y0;
    for (const InkPoint& point : stroke.points) {
      x0 = std::min(x0, point.x);
      y0 = std::min(y0, point.y);
      x1 = std::max(x1, point.x);
      y1 = std::max(y1, point.y);
    }
    stroke.box = Box{static_cast<int>(std::floor(x0)),
                     static_cast<int>(std::floor(y0)),
                     static_cast<int>(std::ceil(x1)) + 1,
                     static_cast<int>(std::ceil(y1)) + 1};
    document.strokes.push_back(std::move(stroke));
  }
  return document;
}

GrayImage renderInk(const InkDocument& document) {
  GrayImage out(document.width, document.height, 0);
  if (out.empty() || document.strokes.empty()) return out;

  // Pen width matters: the classifier compares against printed glyphs, so a
  // hairline skeleton reads like a different symbol. Scale the stroke with the
  // writing itself (median stroke height) instead of the page, which is what
  // makes live ink and traced photo strokes land in the same range.
  std::vector<double> heights;
  heights.reserve(document.strokes.size());
  for (const InkStroke& stroke : document.strokes) {
    const int size = std::max(stroke.box.height(), stroke.box.width());
    if (size > 2) heights.push_back(size);
  }
  double medianHeight = document.height / 22.0;
  if (!heights.empty()) {
    std::sort(heights.begin(), heights.end());
    medianHeight = heights[heights.size() / 2];
  }
  const int thickness = std::max(
      2, std::min(static_cast<int>(std::lround(medianHeight / 8.0)),
                  std::max(3, document.height / 40)));
  cv::Mat canvas = view(out);
  for (const InkStroke& stroke : document.strokes) {
    if (stroke.points.empty()) continue;
    if (stroke.points.size() == 1) {
      const cv::Point center(cvRound(stroke.points[0].x),
                             cvRound(stroke.points[0].y));
      cv::circle(canvas, center, thickness, cv::Scalar(255), cv::FILLED,
                 cv::LINE_AA);
      continue;
    }
    for (size_t i = 1; i < stroke.points.size(); ++i) {
      const cv::Point a(cvRound(stroke.points[i - 1].x),
                        cvRound(stroke.points[i - 1].y));
      const cv::Point b(cvRound(stroke.points[i].x),
                        cvRound(stroke.points[i].y));
      cv::line(canvas, a, b, cv::Scalar(255), thickness, cv::LINE_AA);
    }
  }
  cv::Mat binary;
  cv::threshold(canvas, binary, 60, 255, cv::THRESH_BINARY);
  return copyOf(binary);
}

InkSegmentation segmentInk(const InkDocument& document) {
  return segmentInk(document, nullptr);
}

InkSegmentation segmentInk(const InkDocument& document,
                           const ISymbolClassifier* classifier) {
  InkSegmentation result;
  result.binary = renderInk(document);
  if (document.strokes.empty()) return result;

  const int count = static_cast<int>(document.strokes.size());
  std::vector<double> heights;
  heights.reserve(document.strokes.size());
  for (const InkStroke& stroke : document.strokes) {
    // Use the longer dimension: a dot or a bar is thin in one axis but its
    // size relative to the writing is what matters for the stray-mark rule.
    heights.push_back(std::max(1, std::max(stroke.box.height(), stroke.box.width())));
  }
  const double medianHeight = std::max(1.0, medianOf(heights));

  std::vector<std::vector<int>> lattice = latticeGroups(
      document.strokes, medianHeight, &result.binary, classifier);
  reassignStrays(document.strokes, medianHeight, &lattice);

  // Order symbols by time of their first stroke, so a fraction written bar
  // first still reads top-to-bottom in the structure stage.
  std::sort(lattice.begin(), lattice.end(),
            [](const std::vector<int>& a, const std::vector<int>& b) {
              return a.front() < b.front();
            });
  std::vector<Box> boxes;
  boxes.reserve(lattice.size());
  for (const std::vector<int>& ids : lattice) {
    Box box;
    for (int id : ids) box.include(document.strokes[static_cast<size_t>(id)].box);
    boxes.push_back(box);
    result.groups.push_back(ids);
  }

  // Expand each box by the drawn stroke width so classification sees the ink
  // the renderer added outside the raw point bounds.
  const int pad = std::max(2, static_cast<int>(std::lround(medianHeight / 20.0)));
  for (Box& box : boxes) {
    box.expand(pad);
    box.x0 = std::max(0, box.x0);
    box.y0 = std::max(0, box.y0);
    box.x1 = std::min(document.width, box.x1);
    box.y1 = std::min(document.height, box.y1);
  }

  std::vector<size_t> order(boxes.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&boxes](size_t a, size_t b) {
    if (boxes[a].y0 != boxes[b].y0) return boxes[a].y0 < boxes[b].y0;
    return boxes[a].x0 < boxes[b].x0;
  });

  std::vector<std::vector<int>> orderedGroups;
  orderedGroups.reserve(order.size());
  for (size_t index : order) {
    orderedGroups.push_back(std::move(result.groups[index]));
    Symbol symbol;
    symbol.id = static_cast<int>(result.segmentation.symbols.size());
    symbol.box = boxes[index];
    result.segmentation.symbols.push_back(symbol);
  }
  result.groups = std::move(orderedGroups);

  // Mirror each stroke as a component so a manual split can separate strokes
  // again. Photos get theirs from connected-component labelling; ink has no
  // pixel components of its own, only strokes.
  for (size_t s = 0; s < result.segmentation.symbols.size(); ++s) {
    Symbol& symbol = result.segmentation.symbols[s];
    for (int strokeIndex : result.groups[s]) {
      const InkStroke& stroke =
          document.strokes[static_cast<size_t>(strokeIndex)];
      Component component;
      component.id = static_cast<int>(result.segmentation.components.size());
      component.box = stroke.box;
      component.pixels = std::max(1, component.box.area());
      result.segmentation.components.push_back(component);
      symbol.components.push_back(component.id);
    }
  }

  for (Symbol& symbol : result.segmentation.symbols) {
    int bestLine = -1;
    double bestScore = 0.0;
    for (size_t l = 0; l < result.segmentation.lines.size(); ++l) {
      const Box& lineBox = result.segmentation.lines[l].box;
      const double score =
          ratio(overlapY(symbol.box, lineBox),
                std::min(symbol.box.height(), lineBox.height()));
      if (score > bestScore) {
        bestScore = score;
        bestLine = static_cast<int>(l);
      }
    }
    if (bestLine < 0 || bestScore < 0.45) {
      TextLine line;
      line.id = static_cast<int>(result.segmentation.lines.size());
      line.box = symbol.box;
      result.segmentation.lines.push_back(line);
      bestLine = static_cast<int>(result.segmentation.lines.size()) - 1;
    }
    result.segmentation.lines[bestLine].box.include(symbol.box);
    symbol.line = bestLine;
    result.segmentation.lines[bestLine].symbols.push_back(symbol.id);
  }

  for (TextLine& line : result.segmentation.lines) {
    std::vector<double> lineHeights;
    lineHeights.reserve(line.symbols.size());
    for (int id : line.symbols) {
      lineHeights.push_back(
          result.segmentation.symbols[static_cast<size_t>(id)].box.height());
    }
    line.medianHeight = medianOf(lineHeights);
    std::sort(line.symbols.begin(), line.symbols.end(),
              [&result](int a, int b) {
                return result.segmentation.symbols[static_cast<size_t>(a)]
                           .box.x0 <
                       result.segmentation.symbols[static_cast<size_t>(b)]
                           .box.x0;
              });
  }
  std::sort(result.segmentation.lines.begin(), result.segmentation.lines.end(),
            [](const TextLine& a, const TextLine& b) {
              if (a.box.y0 != b.box.y0) return a.box.y0 < b.box.y0;
              return a.box.x0 < b.box.x0;
            });
  for (size_t l = 0; l < result.segmentation.lines.size(); ++l) {
    result.segmentation.lines[l].id = static_cast<int>(l);
  }
  return result;
}

}  // namespace infinit::scan
