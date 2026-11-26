#pragma once

#include <map>
#include <string>
#include <vector>

#include "classify.hpp"
#include "segment.hpp"

namespace infinit::scan {

enum class NodeKind {
  Row,
  Atom,
  Fraction,
  Power,
  Radical,
  Sum,
  Integral,
  Limit,
  Matrix,
};

/// Structure tree over the glyphs of one image.
///
/// Row      children are the items left to right
/// Atom     text is the classifier label
/// Fraction children = {numerator, denominator}
/// Power    children = {base, exponent}
/// Radical  children = {radicand}
/// Sum      children = {body, index, start, upper}
/// Integral children = {body, variable, lower, upper}
/// Limit    children = {body, variable, point}
/// Matrix   children = rows; each row is a Row of cell nodes
///
/// A missing bound is an empty Row, so the renderer can leave it out instead
/// of printing a placeholder.
struct Node {
  NodeKind kind = NodeKind::Row;
  std::string text;
  std::vector<Node> children;
  Box box;
  /// Worst glyph confidence below this node.
  double confidence = 1.0;

  bool isAtom() const { return kind == NodeKind::Atom; }
};

/// Manual relation switches from the review screen. Auto keeps the geometric
/// rules; the other values force one reading of an ambiguous pair.
enum class ScriptRelation { Auto, Superscript, Inline };
enum class BarRelation { Auto, Fraction, Minus };

struct StructureOverrides {
  std::map<int, ScriptRelation> scripts;
  std::map<int, BarRelation> bars;

  ScriptRelation scriptFor(int id) const {
    const auto it = scripts.find(id);
    return it == scripts.end() ? ScriptRelation::Auto : it->second;
  }
  BarRelation barFor(int id) const {
    const auto it = bars.find(id);
    return it == bars.end() ? BarRelation::Auto : it->second;
  }
};

struct StructureOptions {
  /// A fraction bar is at most this fraction of its width tall.
  double barAspect = 0.35;
  /// ...at least this wide, relative to the page's median glyph height.
  double barMinWidth = 0.60;
  /// Maximum gap between the bar and its numerator/denominator, relative to
  /// the median glyph height.
  double barGap = 1.00;
  /// A superscript's bottom must sit in the upper part of the base: above
  /// base.y0 + this fraction of the base height.
  double superscriptBottom = 0.50;
  /// ...and be lifted this fraction of the base height off the base's bottom.
  double superscriptLift = 0.25;
  /// A superscript starts at or above the base's top edge, so its top may sink
  /// at most this fraction of the base height below it. This is what separates
  /// 2^(x+1) from 2^x + 1.
  double superscriptTop = 0.05;
  /// Maximum horizontal gap between a base and its superscript, relative to the
  /// base width.
  double superscriptGap = 0.50;
  /// Manual script/bar overrides from the correction UI.
  StructureOverrides overrides;
};

/// Builds the structure tree from the classifier's per-symbol matches. The
/// matches vector must be indexed by symbol id.
Node buildStructure(const Segmentation& segmentation,
                    const std::vector<GlyphMatch>& matches,
                    const StructureOptions& options = {});

/// Renders the tree as an expression the core parser accepts.
std::string renderStructure(const Node& node);

/// Renders the tree as LaTeX for the preview in the app.
std::string renderLatex(const Node& node);

/// True when the tree contains a matrix somewhere below it.
bool containsMatrix(const Node& node);

/// The first matrix in the tree, or nullptr. Used to route a reading into a
/// matrix workbench.
const Node* firstMatrix(const Node& node);

}  // namespace infinit::scan
