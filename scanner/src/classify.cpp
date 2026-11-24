#include "classify.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace infinit::scan {

GlyphMatch classifyGlyph(const GrayImage& binary, const Box& box,
                         const TemplateBank& bank,
                         const ClassifyOptions& options) {
  GlyphMatch match;
  if (bank.empty()) return match;

  const std::vector<float> feature = normaliseGlyph(binary, box);
  if (feature.empty()) return match;

  // Best score per label: a class is as good as its closest font.
  std::map<std::string, double> bestPerLabel;
  for (const GlyphTemplate& entry : bank.templates()) {
    if (entry.feature.size() != feature.size()) continue;
    double dot = 0.0;
    for (size_t i = 0; i < feature.size(); ++i) {
      dot += static_cast<double>(feature[i]) * entry.feature[i];
    }
    auto it = bestPerLabel.find(entry.label);
    if (it == bestPerLabel.end() || dot > it->second) {
      bestPerLabel[entry.label] = dot;
    }
  }
  if (bestPerLabel.empty()) return match;

  std::vector<std::pair<double, std::string>> ranked;
  ranked.reserve(bestPerLabel.size());
  for (const auto& item : bestPerLabel) {
    ranked.emplace_back(item.second, item.first);
  }
  std::sort(ranked.begin(), ranked.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  match.score = ranked[0].first;
  match.label = ranked[0].second;
  if (ranked.size() > 1) {
    match.runnerUp = ranked[1].second;
    match.margin = ranked[0].first - ranked[1].first;
  } else {
    match.margin = 1.0;
  }
  match.confident =
      match.score >= options.minScore && match.margin >= options.minMargin;
  if (!match.confident) match.label = "?";
  return match;
}

}  // namespace infinit::scan
