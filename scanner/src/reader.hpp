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
                                   const ISymbolClassifier& classifier);

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
  /// LaTeX printed from the same tree, for the preview.
  std::string latex;
  double confidence = 1.0;
  std::vector<LineReading> lines;
  /// "expression", "sum", "integral", "limit" or "system". The app uses this
  /// to route a reading to the right workbench instead of guessing.
  std::string kind = "expression";
  std::vector<ReadingPart> parts;
};

ExpressionReading readExpression(const GrayImage& binary,
                                 const Segmentation& segmentation,
                                 const ISymbolClassifier& classifier,
                                 const StructureOptions& structureOptions = {},
                                 const std::vector<std::string>* labels = nullptr);

/// Classifies every symbol once and applies the context fixes (punctuation and
/// digit neighbours). `readExpression` and repair both start from this.
std::vector<GlyphMatch> classifySymbols(
    const GrayImage& binary, const Segmentation& segmentation,
    const ISymbolClassifier& classifier,
    const std::vector<std::string>* labels = nullptr);

/// Builds the reading from glyph matches that are already classified. Repair
/// re-renders candidates with this, so it never classifies the same image twice.
ExpressionReading buildReading(const Segmentation& segmentation,
                               std::vector<GlyphMatch> matches,
                               const StructureOptions& structureOptions = {});

/// Builds a reading for one text line only, reindexing that line's symbols.
/// Multi-line review cards use this so each line gets its own expression and
/// LaTeX without re-classifying the page.
ExpressionReading buildLineReading(const Segmentation& segmentation,
                                   const std::vector<GlyphMatch>& matches,
                                   int lineId,
                                   const StructureOptions& structureOptions = {});

}  // namespace infinit::scan
