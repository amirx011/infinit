// Parser-guided repair: bounded beam search over the classifier's alternatives.
// The validator here is a stand-in for the Core parser.
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "repair.hpp"

using namespace infinit::scan;

namespace {

int failures = 0;

void check(const std::string& label, bool ok, const std::string& detail = "") {
  std::cout << (ok ? "PASS " : "FAIL ") << label;
  if (!ok) {
    ++failures;
    if (!detail.empty()) std::cout << "   (" << detail << ")";
  }
  std::cout << "\n";
}

void addSymbol(Segmentation* segmentation, std::vector<GlyphMatch>* matches,
               const std::string& label, const Box& box, double score,
               double margin,
               std::vector<std::pair<std::string, double>> alternatives) {
  Symbol symbol;
  symbol.id = static_cast<int>(segmentation->symbols.size());
  symbol.box = box;
  symbol.line = 0;
  segmentation->symbols.push_back(symbol);

  GlyphMatch match;
  match.label = label;
  match.score = score;
  match.margin = margin;
  match.confident = margin >= 0.10;
  if (alternatives.empty()) alternatives.emplace_back(label, score);
  match.alternatives = std::move(alternatives);
  matches->push_back(std::move(match));
}

/// `2l+3=5`: the classifier prefers "l" but "1" is a close second.
void buildAmbiguous(Segmentation* segmentation,
                    std::vector<GlyphMatch>* matches) {
  addSymbol(segmentation, matches, "2", {0, 40, 20, 80}, 0.99, 0.50, {});
  addSymbol(segmentation, matches, "l", {22, 40, 32, 80}, 0.70, 0.03,
            {{"l", 0.70}, {"1", 0.67}});
  addSymbol(segmentation, matches, "+", {36, 50, 56, 62}, 0.99, 0.50, {});
  addSymbol(segmentation, matches, "3", {60, 40, 80, 80}, 0.99, 0.50, {});
  addSymbol(segmentation, matches, "=", {84, 50, 104, 62}, 0.99, 0.50, {});
  addSymbol(segmentation, matches, "5", {108, 40, 128, 80}, 0.99, 0.50, {});
}

}  // namespace

int main() {
  Segmentation segmentation;
  std::vector<GlyphMatch> matches;
  buildAmbiguous(&segmentation, &matches);

  const ExpressionReading baseline = buildReading(segmentation, matches);
  check("baseline reads the ambiguous glyph",
        baseline.text == "2l+3=5", baseline.text);

  // Rejecting the baseline is exactly what the repair is for.
  const Validator wantsDigit = [](const std::string& expression,
                                  std::string* error) {
    const bool ok = expression == "21+3=5";
    if (!ok && error != nullptr) *error = "l is not a digit";
    return ok;
  };
  const RepairResult repaired =
      repairReading(segmentation, matches, baseline, wantsDigit);
  check("repair switches to the second choice",
        repaired.best.text == "21+3=5", repaired.best.text);
  check("repair is marked", repaired.repaired);
  check("repair reports a valid parse", repaired.valid);

  // A valid baseline must never be replaced by a costlier guess.
  const Validator acceptsAll = [](const std::string&, std::string*) {
    return true;
  };
  const RepairResult untouched =
      repairReading(segmentation, matches, baseline, acceptsAll);
  check("valid baseline is kept", untouched.best.text == baseline.text,
        untouched.best.text);
  check("valid baseline is not marked repaired", !untouched.repaired);
  check("valid baseline has zero cost", untouched.bestCost == 0.0,
        std::to_string(untouched.bestCost));

  // A zero budget returns the baseline instead of looping.
  RepairOptions noBudget;
  noBudget.maxEvaluations = 1;
  const RepairResult budgeted =
      repairReading(segmentation, matches, baseline, wantsDigit, noBudget);
  check("evaluation budget is respected",
        budgeted.best.text == baseline.text, budgeted.best.text);

  // Alternatives are offered for the correction sheet.
  check("repair offers alternatives", !repaired.candidates.empty(),
        std::to_string(repaired.candidates.size()));

  if (failures > 0) {
    std::cout << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "all repair checks passed\n";
  return 0;
}
