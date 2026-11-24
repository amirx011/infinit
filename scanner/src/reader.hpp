#pragma once

#include <string>
#include <vector>

#include "classify.hpp"
#include "glyphs.hpp"
#include "segment.hpp"
#include "structure.hpp"

namespace infinit::scan {

struct SymbolReading {
  int symbol = -1;
  GlyphMatch match;
  Box box;
};

struct LineReading {
  int line = -1;
  Box box;
  /// Left-to-right concatenation of the confident labels.
  std::string text;
  std::vector<SymbolReading> glyphs;
};

/// Classifies every symbol of every line, left to right. Unconfident symbols
/// become '?' in the text but keep their score for the correction UI.
std::vector<LineReading> readLines(const GrayImage& binary,
                                   const Segmentation& segmentation,
                                   const TemplateBank& bank,
                                   const ClassifyOptions& options = {});

/// One named field of a structured reading: the term of a series, the bounds of
/// an integral, one equation of a system.
struct ReadingPart {
  std::string name;
  std::string value;
};

/// Whole-image reading: per-line glyphs plus the structure tree and the
/// expression text the core parser should receive.
struct ExpressionReading {
  Node root;
  std::string text;
  double confidence = 1.0;
  std::vector<LineReading> lines;
  /// "expression", "sum", "integral", "limit" or "system". The app uses this
  /// to route a reading to the right workbench instead of guessing.
  std::string kind = "expression";
  std::vector<ReadingPart> parts;
};

ExpressionReading readExpression(const GrayImage& binary,
                                 const Segmentation& segmentation,
                                 const TemplateBank& bank,
                                 const ClassifyOptions& classifyOptions = {},
                                 const StructureOptions& structureOptions = {});

}  // namespace infinit::scan
