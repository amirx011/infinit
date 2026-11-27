#include "reader.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace infinit::scan {
namespace {

bool isBlank(const Node& node) {
  return node.kind == NodeKind::Row && node.children.empty();
}

/// Primes sit high on the line, commas sit on the baseline. The classifier
/// confuses them on a low-resolution photo, but the position is unambiguous.
void fixPunctuation(const Segmentation& segmentation,
                    std::vector<GlyphMatch>* matches) {
  for (const TextLine& line : segmentation.lines) {
    if (line.box.height() <= 0) continue;
    for (int id : line.symbols) {
      GlyphMatch& match = (*matches)[id];
      const Box& box = segmentation.symbols[id].box;
      const double where =
          static_cast<double>(box.centerY() - line.box.y0) / line.box.height();
      const bool low = where > 0.60;
      const bool high = where < 0.35;
      if (match.label == "'" && low) {
        match.label = ",";
      } else if (match.label == "," && high) {
        match.label = "'";
      } else if (match.label == "?" && match.runnerUp == "," && low) {
        // A blurred comma is small and low; position is more reliable than a
        // 0.02 margin against the apostrophe template.
        match.label = ",";
      } else if (match.label == "?" && match.runnerUp == "'" && high) {
        match.label = "'";
      }
    }
  }
}

bool isDigitLabel(const std::string& label) {
  return label.size() == 1 &&
         std::isdigit(static_cast<unsigned char>(label[0])) != 0;
}

bool isAlphaLabel(const std::string& label) {
  return label.size() == 1 &&
         std::isalpha(static_cast<unsigned char>(label[0])) != 0;
}

/// The plan's confusable classes, resolved by neighbours: a letter wedged
/// between two digits is almost always the digit it resembles. The correction
/// sheet still shows the original alternatives, so a wrong call is one tap
/// away.
void fixDigitNeighbours(const Segmentation& segmentation,
                        std::vector<GlyphMatch>* matches) {
  for (const TextLine& line : segmentation.lines) {
    if (line.symbols.size() < 3) continue;
    std::vector<int> ids = line.symbols;
    std::sort(ids.begin(), ids.end(), [&segmentation](int a, int b) {
      return segmentation.symbols[a].box.x0 < segmentation.symbols[b].box.x0;
    });
    const double scale = std::max(1.0, static_cast<double>(line.box.height()));
    for (size_t i = 1; i + 1 < ids.size(); ++i) {
      const Box& box = segmentation.symbols[ids[i]].box;
      const Box& leftBox = segmentation.symbols[ids[i - 1]].box;
      const Box& rightBox = segmentation.symbols[ids[i + 1]].box;
      const bool sameLine =
          std::abs(leftBox.centerY() - box.centerY()) <= 0.30 * scale &&
          std::abs(rightBox.centerY() - box.centerY()) <= 0.30 * scale;
      const bool close =
          box.x0 - leftBox.x1 <= 0.40 * scale &&
          rightBox.x0 - box.x1 <= 0.40 * scale;
      if (!sameLine || !close) continue;
      if (!isDigitLabel((*matches)[ids[i - 1]].label) ||
          !isDigitLabel((*matches)[ids[i + 1]].label)) {
        continue;
      }

      GlyphMatch& match = (*matches)[ids[i]];
      const std::string& candidate =
          match.label == "?" ? match.runnerUp : match.label;
      if (candidate == "l" || candidate == "I" || candidate == "|") {
        match.label = "1";
      } else if (candidate == "O" || candidate == "o") {
        match.label = "0";
      } else if (candidate == "S") {
        match.label = "5";
      } else if (candidate == "z") {
        match.label = "2";
      }
    }
  }
}

/// Neighbours that make a lone `l` unambiguous: a digit, a bracket or an
/// operator. The `l` of "log" and "lim" always has a letter on one side, so it
/// is never touched.
bool isOneContext(const std::string& label) {
  if (isDigitLabel(label)) return true;
  return label == "+" || label == "-" || label == "=" || label == "(" ||
         label == ")" || label == "[" || label == "]" || label == "{" ||
         label == "}" || label == "/" || label == "," || label == "." ||
         label == "^" || label == "*" || label == "<" || label == ">" ||
         label == "|";
}

/// Resolves `l`/`I`/`|` against the digit one. At 16x16 a serif `1` and a
/// lowercase `l` are almost the same raster, and the classifier picks `l` by a
/// hair (margin 0.00-0.06 on the benchmark). Context breaks the tie: `(1/x`,
/// `x+1`, a matrix cell `[1,` are always digits, while `log` and `lim` keep
/// their letter because one neighbour is a letter.
void fixAmbiguousOne(const Segmentation& segmentation,
                     std::vector<GlyphMatch>* matches) {
  for (const TextLine& line : segmentation.lines) {
    if (line.symbols.size() < 2) continue;
    std::vector<int> ids = line.symbols;
    std::sort(ids.begin(), ids.end(), [&segmentation](int a, int b) {
      return segmentation.symbols[a].box.x0 < segmentation.symbols[b].box.x0;
    });
    for (size_t i = 0; i < ids.size(); ++i) {
      GlyphMatch& match = (*matches)[ids[i]];
      const std::string& candidate =
          match.label == "?" ? match.runnerUp : match.label;
      if (candidate != "l" && candidate != "I" && candidate != "|") continue;
      if (match.label != "?" && match.runnerUp != "1") continue;
      if (match.label == "?" && match.runnerUp != "l" &&
          match.runnerUp != "I" && match.runnerUp != "|") {
        continue;
      }
      if (match.margin > 0.30) continue;
      bool context = false;
      bool blocked = false;
      if (i > 0) {
        const std::string& left = (*matches)[ids[i - 1]].label;
        if (isOneContext(left)) context = true; else blocked = true;
      }
      if (i + 1 < ids.size()) {
        const std::string& right = (*matches)[ids[i + 1]].label;
        if (isOneContext(right)) context = true; else blocked = true;
      }
      if (!context || blocked) continue;
      match.label = "1";
      match.runnerUp = candidate;
      match.alternatives.clear();
      match.alternatives.emplace_back("1", match.score);
      match.alternatives.emplace_back(candidate, match.score - match.margin);
    }
  }
}

/// A dot drawn on the math axis is multiplication, not a decimal point. The
/// classifier cannot see position: the raster is fitted to its own ink box, so
/// both dots are the same 3x3 blob. Compare the dot with the letters beside it
/// instead; a period sits on their baseline, `·` sits half an x-height above.
void fixMultiplicationDots(const Segmentation& segmentation,
                           std::vector<GlyphMatch>* matches) {
  for (const TextLine& line : segmentation.lines) {
    for (int id : line.symbols) {
      GlyphMatch& match = (*matches)[id];
      if (match.label != ".") continue;
      const Box& box = segmentation.symbols[id].box;
      int left = -1;
      int right = -1;
      for (int other : line.symbols) {
        if (other == id) continue;
        const Box& candidate = segmentation.symbols[other].box;
        if (candidate.x1 <= box.x0) {
          if (left < 0 || candidate.x1 > segmentation.symbols[left].box.x1) {
            left = other;
          }
        } else if (candidate.x0 >= box.x1) {
          if (right < 0 || candidate.x0 < segmentation.symbols[right].box.x0) {
            right = other;
          }
        }
      }
      auto raisedAgainst = [&](int neighbour) {
        if (neighbour < 0) return true;
        const Box& other = segmentation.symbols[neighbour].box;
        if (other.height() < 2 * box.height()) return false;
        return box.centerY() < other.y1 - 0.35 * other.height();
      };
      if ((left < 0 && right < 0) || !raisedAgainst(left) ||
          !raisedAgainst(right)) {
        continue;
      }
      match.label = "*";
      match.runnerUp = ".";
      match.alternatives.clear();
      match.alternatives.emplace_back("*", match.score);
    }
  }
}

/// A tall narrow stroke is an integral, and a tall block is a summation, even
/// when the raster bank answers "l" or "?". The classifier fits every glyph
/// into its own ink box, so size — the one cue that separates ∫ and Σ from a
/// letter — never reaches it.
void fixBigOperators(const Segmentation& segmentation,
                     std::vector<GlyphMatch>* matches) {
  std::vector<int> heights;
  heights.reserve(segmentation.symbols.size());
  for (const Symbol& symbol : segmentation.symbols) {
    if (symbol.box.height() > 0) heights.push_back(symbol.box.height());
  }
  if (heights.size() < 3) return;
  std::sort(heights.begin(), heights.end());
  const double median = heights[heights.size() / 2];
  for (size_t i = 0; i < segmentation.symbols.size(); ++i) {
    const Box& box = segmentation.symbols[i].box;
    if (box.height() <= 0) continue;
    const double aspect = static_cast<double>(box.width()) / box.height();
    GlyphMatch& match = (*matches)[i];
    const std::string& label = match.label;
    const bool strokeLike = label == "l" || label == "1" || label == "I" ||
                            label == "|" || label == "i" || label == "?" ||
                            label == "'" || label == "/";
    if (box.height() >= 2.8 * median && aspect <= 0.48 && strokeLike) {
      match.runnerUp = label;
      match.label = "integral";
      match.alternatives.clear();
      match.alternatives.emplace_back("integral", match.score);
      continue;
    }
    const bool blockLike = label == "?" || label == "z" || label == "2" ||
                           label == "S" || label == "s";
    if (box.height() >= 2.6 * median && aspect >= 0.52 && aspect <= 1.70 &&
        blockLike) {
      match.runnerUp = label;
      match.label = "sum";
      match.alternatives.clear();
      match.alternatives.emplace_back("sum", match.score);
    }
  }
}

/// A `1` or `l` wedged between two letters is a dotless i: a rule crossing the
/// letter or a blur ate the dot, and the remaining stem scores as a digit. In
/// math typesetting a digit never sits between two letter factors.
void fixDotlessI(const Segmentation& segmentation,
                 std::vector<GlyphMatch>* matches) {
  for (const TextLine& line : segmentation.lines) {
    if (line.symbols.size() < 3) continue;
    std::vector<int> ids = line.symbols;
    std::sort(ids.begin(), ids.end(), [&segmentation](int a, int b) {
      return segmentation.symbols[a].box.x0 < segmentation.symbols[b].box.x0;
    });
    for (size_t i = 1; i + 1 < ids.size(); ++i) {
      GlyphMatch& match = (*matches)[ids[i]];
      const std::string& candidate =
          match.label == "?" ? match.runnerUp : match.label;
      if (candidate != "1" && candidate != "l" && candidate != "I" &&
          candidate != "|") {
        continue;
      }
      const std::string& left = (*matches)[ids[i - 1]].label;
      const std::string& right = (*matches)[ids[i + 1]].label;
      if (!isAlphaLabel(left) || !isAlphaLabel(right)) continue;
      const Box& box = segmentation.symbols[ids[i]].box;
      const int taller = std::max(segmentation.symbols[ids[i - 1]].box.height(),
                                  segmentation.symbols[ids[i + 1]].box.height());
      if (box.height() > 1.15 * taller) continue;
      match.label = "i";
      match.runnerUp = candidate;
      match.alternatives.clear();
      match.alternatives.emplace_back("i", match.score);
    }
  }
}

std::string trim(const std::string& text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

/// Splits at commas that are not inside brackets.
std::vector<std::string> splitTopLevel(const std::string& text) {
  std::vector<std::string> out;
  std::string current;
  int depth = 0;
  for (char c : text) {
    if (c == '(' || c == '[' || c == '{') ++depth;
    if (c == ')' || c == ']' || c == '}') depth = std::max(0, depth - 1);
    if (c == ',' && depth == 0) {
      out.push_back(trim(current));
      current.clear();
    } else {
      current += c;
    }
  }
  out.push_back(trim(current));
  return out;
}

int countTopLevel(const std::string& text, char wanted) {
  int depth = 0;
  int count = 0;
  for (char c : text) {
    if (c == '(' || c == '[' || c == '{') ++depth;
    if (c == ')' || c == ']' || c == '}') depth = std::max(0, depth - 1);
    if (c == wanted && depth == 0) ++count;
  }
  return count;
}

void addPart(ExpressionReading* reading, const char* name, const Node& node) {
  if (isBlank(node)) return;
  reading->parts.push_back(ReadingPart{name, renderStructure(node)});
}

/// Fills kind/parts so the app can route the reading to the right workbench.
void describeReading(ExpressionReading* reading) {
  const Node* node = &reading->root;
  if (node->kind == NodeKind::Row && node->children.size() == 1) {
    node = &node->children.front();
  }

  switch (node->kind) {
    // Every structured node carries fixed slots; a malformed tree falls back
    // to the plain expression instead of reading past the children.
    case NodeKind::Matrix: {
      reading->kind = "matrix";
      const size_t rows = node->children.size();
      size_t columns = 0;
      for (const Node& row : node->children) {
        columns = std::max(columns, row.children.size());
      }
      reading->parts.push_back(
          ReadingPart{"rows", std::to_string(rows)});
      reading->parts.push_back(
          ReadingPart{"columns", std::to_string(columns)});
      // The core's matrix fields take rows separated by ';' and cells by ','.
      std::string cells;
      for (size_t r = 0; r < rows; ++r) {
        if (r) cells += "; ";
        const Node& row = node->children[r];
        for (size_t c = 0; c < row.children.size(); ++c) {
          if (c) cells += ", ";
          const Node& cell = row.children[c];
          cells += isBlank(cell) ? "0" : renderStructure(cell);
        }
      }
      reading->parts.push_back(ReadingPart{"matrix", cells});
      return;
    }
    case NodeKind::Sum:
      if (node->children.size() != 4) break;
      reading->kind = "sum";
      addPart(reading, "term", node->children[0]);
      addPart(reading, "index", node->children[1]);
      addPart(reading, "start", node->children[2]);
      addPart(reading, "end", node->children[3]);
      return;
    case NodeKind::Integral:
      if (node->children.size() != 4) break;
      reading->kind = "integral";
      addPart(reading, "integrand", node->children[0]);
      addPart(reading, "variable", node->children[1]);
      addPart(reading, "lower", node->children[2]);
      addPart(reading, "upper", node->children[3]);
      return;
    case NodeKind::Limit:
      if (node->children.size() != 3) break;
      reading->kind = "limit";
      addPart(reading, "expression", node->children[0]);
      addPart(reading, "variable", node->children[1]);
      addPart(reading, "point", node->children[2]);
      return;
    default:
      break;
  }

  if (containsMatrix(reading->root)) {
    const Node* matrix = firstMatrix(reading->root);
    if (matrix != nullptr && matrix->children.size() >= 2) {
      const size_t rows = matrix->children.size();
      size_t columns = 0;
      for (const Node& row : matrix->children) {
        columns = std::max(columns, row.children.size());
      }
      reading->kind = "matrix";
      reading->parts.push_back(ReadingPart{"rows", std::to_string(rows)});
      reading->parts.push_back(
          ReadingPart{"columns", std::to_string(columns)});
      std::string cells;
      for (size_t r = 0; r < rows; ++r) {
        if (r) cells += "; ";
        const Node& row = matrix->children[r];
        for (size_t c = 0; c < row.children.size(); ++c) {
          if (c) cells += ", ";
          const Node& cell = row.children[c];
          cells += isBlank(cell) ? "0" : renderStructure(cell);
        }
      }
      reading->parts.push_back(ReadingPart{"matrix", cells});
      return;
    }
  }

  if (countTopLevel(reading->text, '=') < 2) return;
  const std::vector<std::string> equations = splitTopLevel(reading->text);
  if (equations.size() < 2) return;
  reading->kind = "system";
  for (size_t i = 0; i < equations.size(); ++i) {
    reading->parts.push_back(
        ReadingPart{"equation" + std::to_string(i + 1), equations[i]});
  }
}

}  // namespace

std::vector<LineReading> readLines(const GrayImage& binary,
                                   const Segmentation& segmentation,
                                   const ISymbolClassifier& classifier,
                                   const std::vector<std::string>* labels) {
  std::vector<LineReading> readings;
  readings.reserve(segmentation.lines.size());

  std::vector<GlyphMatch> matches =
      classifySymbols(binary, segmentation, classifier, labels);

  for (const TextLine& line : segmentation.lines) {
    LineReading reading;
    reading.line = line.id;
    reading.box = line.box;
    reading.text.reserve(line.symbols.size());

    for (int id : line.symbols) {
      SymbolReading glyph;
      glyph.symbol = id;
      glyph.box = segmentation.symbols[id].box;
      glyph.match = matches[id];
      reading.text += glyph.match.label.empty() ? "?" : glyph.match.label;
      reading.glyphs.push_back(std::move(glyph));
    }
    readings.push_back(std::move(reading));
  }
  return readings;
}

std::vector<GlyphMatch> classifySymbols(
    const GrayImage& binary, const Segmentation& segmentation,
    const ISymbolClassifier& classifier,
    const std::vector<std::string>* labels) {
  std::vector<GlyphMatch> matches(segmentation.symbols.size());
  for (size_t i = 0; i < segmentation.symbols.size(); ++i) {
    matches[i] = classifier.classify(binary, segmentation.symbols[i].box);
  }
  if (labels != nullptr) {
    for (size_t i = 0; i < matches.size() && i < labels->size(); ++i) {
      if (!(*labels)[i].empty()) matches[i].label = (*labels)[i];
    }
  }
  fixPunctuation(segmentation, &matches);
  fixBigOperators(segmentation, &matches);
  fixDigitNeighbours(segmentation, &matches);
  fixAmbiguousOne(segmentation, &matches);
  fixDotlessI(segmentation, &matches);
  fixMultiplicationDots(segmentation, &matches);
  return matches;
}

ExpressionReading readExpression(const GrayImage& binary,
                                 const Segmentation& segmentation,
                                 const ISymbolClassifier& classifier,
                                 const StructureOptions& structureOptions,
                                 const std::vector<std::string>* labels) {
  return buildReading(segmentation,
                      classifySymbols(binary, segmentation, classifier, labels),
                      structureOptions);
}

ExpressionReading buildLineReading(const Segmentation& segmentation,
                                   const std::vector<GlyphMatch>& matches,
                                   int lineId,
                                   const StructureOptions& structureOptions) {
  const TextLine* source = nullptr;
  for (const TextLine& line : segmentation.lines) {
    if (line.id == lineId) {
      source = &line;
      break;
    }
  }
  if (source == nullptr || source->symbols.empty()) return ExpressionReading{};

  Segmentation single;
  single.components = segmentation.components;
  std::vector<int> remap(segmentation.symbols.size(), -1);
  std::vector<GlyphMatch> subMatches;
  subMatches.reserve(source->symbols.size());
  for (int oldId : source->symbols) {
    if (oldId < 0 || oldId >= static_cast<int>(segmentation.symbols.size())) {
      continue;
    }
    Symbol symbol = segmentation.symbols[static_cast<size_t>(oldId)];
    symbol.id = static_cast<int>(single.symbols.size());
    remap[static_cast<size_t>(oldId)] = symbol.id;
    single.symbols.push_back(std::move(symbol));
    subMatches.push_back(matches[static_cast<size_t>(oldId)]);
  }
  if (single.symbols.empty()) return ExpressionReading{};

  TextLine line = *source;
  line.id = 0;
  line.symbols.clear();
  for (int oldId : source->symbols) {
    if (oldId >= 0 && remap[static_cast<size_t>(oldId)] >= 0) {
      line.symbols.push_back(remap[static_cast<size_t>(oldId)]);
    }
  }
  single.lines.push_back(std::move(line));
  return buildReading(single, std::move(subMatches), structureOptions);
}

/// Two or more independent equations, one per line, with nothing spanning the
/// lines. A matrix has a bracket tall enough to touch every row, and a sum has
/// limits that also cross lines, so those keep the whole-image parse.
bool looksLikeEquationSystem(const Segmentation& segmentation,
                             const std::vector<GlyphMatch>& matches) {
  if (segmentation.lines.size() < 2) return false;
  for (const Symbol& symbol : segmentation.symbols) {
    int spanned = 0;
    for (const TextLine& line : segmentation.lines) {
      const int overlap = std::min(symbol.box.y1, line.box.y1) -
                          std::max(symbol.box.y0, line.box.y0);
      if (overlap > 0.25 * symbol.box.height()) ++spanned;
    }
    if (spanned >= 2) return false;
  }
  for (const TextLine& line : segmentation.lines) {
    int equals = 0;
    for (int id : line.symbols) {
      if (id >= 0 && id < static_cast<int>(matches.size()) &&
          matches[id].label == "=") {
        ++equals;
      }
    }
    if (equals != 1) return false;
  }
  return true;
}

ExpressionReading buildReading(const Segmentation& segmentation,
                               std::vector<GlyphMatch> matches,
                               const StructureOptions& structureOptions) {
  ExpressionReading reading;
  for (const TextLine& line : segmentation.lines) {
    LineReading lineReading;
    lineReading.line = line.id;
    lineReading.box = line.box;
    for (int id : line.symbols) {
      SymbolReading glyph;
      glyph.symbol = id;
      glyph.box = segmentation.symbols[id].box;
      glyph.match = matches[id];
      lineReading.text += glyph.match.label.empty() ? "?" : glyph.match.label;
      lineReading.glyphs.push_back(std::move(glyph));
    }
    reading.lines.push_back(std::move(lineReading));
  }

  reading.root = buildStructure(segmentation, matches, structureOptions);
  reading.text = renderStructure(reading.root);
  reading.latex = renderLatex(reading.root);
  reading.confidence = reading.root.confidence;

  // A system is written as one equation per line. Parsing all lines as a
  // single row interleaves them by x and turns the second equation into
  // exponents of the first, so each line is structured on its own and the
  // results are joined with the comma the app's system fields expect.
  if (looksLikeEquationSystem(segmentation, matches)) {
    Node root;
    root.confidence = 1.0;
    std::string text;
    for (const TextLine& line : segmentation.lines) {
      ExpressionReading part =
          buildLineReading(segmentation, matches, line.id, structureOptions);
      if (part.text.empty()) continue;
      if (!text.empty()) {
        text += ",";
        Node comma;
        comma.kind = NodeKind::Atom;
        comma.text = ",";
        root.children.push_back(std::move(comma));
      }
      text += part.text;
      root.confidence = std::min(root.confidence, part.confidence);
      root.box.include(part.root.box);
      root.children.push_back(part.root);
    }
    if (!root.children.empty()) {
      reading.root = std::move(root);
      reading.text = text;
      reading.latex = renderLatex(reading.root);
      reading.confidence = reading.root.confidence;
    }
  }

  describeReading(&reading);
  return reading;
}

}  // namespace infinit::scan
