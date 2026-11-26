#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "classify.hpp"
#include "reader.hpp"
#include "segment.hpp"

namespace infinit::scan {

/// The Core's parser, injected. Returning false fills `error` with the reason.
/// The scanner never links the Solver; the host passes its own parser in.
using Validator = std::function<bool(const std::string&, std::string*)>;

struct RepairOptions {
  /// Ambiguous glyphs the search may change, worst margin first.
  int maxDecisions = 6;
  /// States kept after each decision.
  int beamWidth = 20;
  /// Hard cap on rendered + validated candidates.
  int maxEvaluations = 60;
  /// Cost of a candidate the parser rejects.
  double parsePenalty = 10.0;
  /// Cost of changing one glyph, so a valid baseline is never replaced for
  /// free.
  double changePenalty = 0.02;
  /// Cost per unresolved "?" in the expression.
  double unknownPenalty = 2.0;
  /// Total symbol distance a repaired candidate may spend. Keeps the search
  /// from rewriting half the reading just to make the parser accept it.
  double maxSymbolCost = 0.75;
  /// A glyph is a decision point only when its top two labels are this close.
  double closeMargin = 0.20;
  /// Symbol ids the host set by hand. The search never changes them, so a
  /// correction the user made always survives a later repair.
  std::vector<int> pinned;
  /// Structure rules (including manual relation overrides) used for every
  /// candidate, so a repair never silently undoes a structural fix.
  StructureOptions structure;
};

struct RepairCandidate {
  std::string text;
  std::string latex;
  double cost = 0.0;
  bool valid = false;
};

struct RepairResult {
  ExpressionReading best;
  double bestCost = 0.0;
  bool repaired = false;
  bool valid = false;
  /// Glyphs the search changed, id → new label. The correction sheet starts
  /// from this map so a later manual fix does not undo an automatic one.
  std::map<int, std::string> changes;
  /// Baseline first, then the best alternatives by cost.
  std::vector<RepairCandidate> candidates;
};

/// Bounded beam search over the classifier's top-k labels, scored by symbol
/// distance, parse result and unknown-glyph count. Never mutates `matches`.
RepairResult repairReading(const Segmentation& segmentation,
                           const std::vector<GlyphMatch>& matches,
                           const ExpressionReading& baseline,
                           const Validator& validator,
                           const RepairOptions& options = {});

}  // namespace infinit::scan
