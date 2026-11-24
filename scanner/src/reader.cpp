#include "reader.hpp"

#include <algorithm>
#include <cctype>

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
                                   const TemplateBank& bank,
                                   const ClassifyOptions& options) {
  std::vector<LineReading> readings;
  readings.reserve(segmentation.lines.size());

  for (const TextLine& line : segmentation.lines) {
    LineReading reading;
    reading.line = line.id;
    reading.box = line.box;
    reading.text.reserve(line.symbols.size());

    for (int id : line.symbols) {
      const Symbol& symbol = segmentation.symbols[id];
      SymbolReading glyph;
      glyph.symbol = id;
      glyph.box = symbol.box;
      glyph.match = classifyGlyph(binary, symbol.box, bank, options);
      reading.text += glyph.match.label.empty() ? "?" : glyph.match.label;
      reading.glyphs.push_back(std::move(glyph));
    }
    readings.push_back(std::move(reading));
  }
  return readings;
}

ExpressionReading readExpression(const GrayImage& binary,
                                 const Segmentation& segmentation,
                                 const TemplateBank& bank,
                                 const ClassifyOptions& classifyOptions,
                                 const StructureOptions& structureOptions) {
  ExpressionReading reading;

  std::vector<GlyphMatch> matches(segmentation.symbols.size());
  for (size_t i = 0; i < segmentation.symbols.size(); ++i) {
    matches[i] = classifyGlyph(binary, segmentation.symbols[i].box, bank,
                               classifyOptions);
  }
  fixPunctuation(segmentation, &matches);

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
  reading.confidence = reading.root.confidence;
  describeReading(&reading);
  return reading;
}

}  // namespace infinit::scan
