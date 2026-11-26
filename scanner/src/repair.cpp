#include "repair.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>
namespace infinit::scan {
namespace {

struct Decision {
  int id = -1;
  double margin = 1.0;
  std::vector<std::pair<std::string, double>> options;  // label, extra cost
};

struct State {
  std::map<int, std::string> changes;
  double symbolCost = 0.0;
  double totalCost = 0.0;
  bool valid = false;
};

std::vector<GlyphMatch> withChanges(const std::vector<GlyphMatch>& matches,
                                    const std::map<int, std::string>& changes) {
  std::vector<GlyphMatch> out = matches;
  for (const auto& change : changes) {
    if (change.first >= 0 && change.first < static_cast<int>(out.size())) {
      out[change.first].label = change.second;
    }
  }
  return out;
}

double countUnknown(const std::string& text) {
  return static_cast<double>(std::count(text.begin(), text.end(), '?'));
}

}  // namespace

RepairResult repairReading(const Segmentation& segmentation,
                           const std::vector<GlyphMatch>& matches,
                           const ExpressionReading& baseline,
                           const Validator& validator,
                           const RepairOptions& options) {
  RepairResult result;
  result.best = baseline;
  if (!validator) {
    result.bestCost = 0.0;
    result.valid = true;
    result.candidates.push_back(
        RepairCandidate{baseline.text, baseline.latex, 0.0, true});
    return result;
  }

  std::string error;
  result.valid = validator(baseline.text, &error);
  result.bestCost =
      (result.valid ? 0.0 : options.parsePenalty) +
      options.unknownPenalty * countUnknown(baseline.text);
  result.candidates.push_back(RepairCandidate{baseline.text, baseline.latex,
                                              result.bestCost, result.valid});

  // Ambiguous glyphs, worst margin first: those are the decisions the search
  // is allowed to revisit.
  std::vector<Decision> decisions;
  for (size_t id = 0; id < matches.size(); ++id) {
    if (std::find(options.pinned.begin(), options.pinned.end(),
                  static_cast<int>(id)) != options.pinned.end()) {
      continue;
    }
    const GlyphMatch& match = matches[id];
    if (match.alternatives.size() < 2) continue;
    if (match.label != "?" && match.margin >= options.closeMargin) continue;

    Decision decision;
    decision.id = static_cast<int>(id);
    decision.margin = match.margin;
    const size_t top = std::min<size_t>(3, match.alternatives.size());
    for (size_t k = 1; k < top; ++k) {
      const std::string& label = match.alternatives[k].first;
      if (label.empty() || label == match.label) continue;
      const double delta =
          std::max(0.0, match.score - match.alternatives[k].second) +
          options.changePenalty;
      decision.options.emplace_back(label, delta);
    }
    if (!decision.options.empty()) decisions.push_back(std::move(decision));
  }
  std::sort(decisions.begin(), decisions.end(),
            [](const Decision& a, const Decision& b) {
              return a.margin < b.margin;
            });
  if (static_cast<int>(decisions.size()) > options.maxDecisions) {
    decisions.resize(static_cast<size_t>(options.maxDecisions));
  }

  struct Scored {
    State state;
    RepairCandidate candidate;
  };
  const std::string baselineKind = baseline.kind;
  State baselineState;
  baselineState.totalCost = result.bestCost;
  baselineState.valid = result.valid;
  std::vector<Scored> evaluated;
  evaluated.push_back(Scored{baselineState, result.candidates.front()});

  std::vector<State> beam{baselineState};
  int evaluations = 1;
  for (const Decision& decision : decisions) {
    std::vector<State> next;
    next.reserve(beam.size() * (decision.options.size() + 1));
    for (const State& state : beam) {
      next.push_back(state);  // keep this glyph
      for (const auto& option : decision.options) {
        if (evaluations >= options.maxEvaluations) break;
        State candidate = state;
        candidate.changes[decision.id] = option.first;
        candidate.symbolCost = state.symbolCost + option.second;
        if (candidate.symbolCost > options.maxSymbolCost) continue;

        const ExpressionReading reading =
            buildReading(segmentation, withChanges(matches, candidate.changes),
                         options.structure);
        ++evaluations;
        // A label change that rewrites the structure (a limit or an integral
        // turning into a product) is not a repair; it is a different reading.
        if (reading.kind != baselineKind) continue;
        std::string reason;
        candidate.valid = validator(reading.text, &reason);
        candidate.totalCost =
            candidate.symbolCost +
            (candidate.valid ? 0.0 : options.parsePenalty) +
            options.unknownPenalty * countUnknown(reading.text);
        next.push_back(candidate);
        evaluated.push_back(
            Scored{candidate,
                   RepairCandidate{reading.text, reading.latex,
                                   candidate.totalCost, candidate.valid}});
      }
    }
    std::sort(next.begin(), next.end(), [](const State& a, const State& b) {
      return a.totalCost < b.totalCost;
    });
    if (static_cast<int>(next.size()) > options.beamWidth) {
      next.resize(static_cast<size_t>(options.beamWidth));
    }
    beam = std::move(next);
  }

  // The baseline is always in `evaluated`, so a valid reading is never
  // replaced by a costlier guess.
  std::sort(evaluated.begin(), evaluated.end(),
            [](const Scored& a, const Scored& b) {
              return a.candidate.cost < b.candidate.cost;
            });
  const State& winner = evaluated.front().state;
  result.bestCost = evaluated.front().candidate.cost;
  result.valid = evaluated.front().candidate.valid;
  result.repaired = !winner.changes.empty() &&
                    evaluated.front().candidate.text != baseline.text;
  result.changes = result.repaired ? winner.changes : std::map<int, std::string>{};
  result.best = winner.changes.empty()
                    ? baseline
                    : buildReading(segmentation,
                                   withChanges(matches, winner.changes),
                                   options.structure);

  // Alternatives for the UI: distinct texts, best first, without the winner.
  std::vector<RepairCandidate> ranked;
  for (const Scored& entry : evaluated) {
    if (entry.candidate.text == result.best.text) continue;
    const bool seen = std::any_of(
        ranked.begin(), ranked.end(), [&entry](const RepairCandidate& other) {
          return other.text == entry.candidate.text;
        });
    if (!seen) ranked.push_back(entry.candidate);
    if (ranked.size() >= 3) break;
  }
  result.candidates.insert(result.candidates.end(), ranked.begin(),
                           ranked.end());
  return result;
}

}  // namespace infinit::scan
