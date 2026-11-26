#include "classify.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>

#include "preprocess.hpp"

namespace infinit::scan {
namespace {

constexpr int kZoning = 4;
constexpr int kNeighbours = 5;
constexpr uint32_t kPrototypeVersion = 1;

/// L1 distance between two profile signatures, scaled to [0, 1].
double profileDistance(const std::vector<float>& a,
                       const std::vector<float>& b) {
  if (a.empty() || a.size() != b.size()) return 0.0;
  double sum = 0.0;
  for (size_t i = 0; i < a.size(); ++i) sum += std::abs(a[i] - b[i]);
  return std::min(1.0, sum / 4.0);
}

/// Mean absolute difference of the log-Hu invariants, scaled to [0, 1].
double huDistance(const std::vector<float>& a, const std::vector<float>& b) {
  if (a.empty() || a.size() != b.size()) return 0.0;
  double sum = 0.0;
  for (size_t i = 0; i < a.size(); ++i) sum += std::abs(a[i] - b[i]);
  return std::min(1.0, sum / static_cast<double>(a.size()) / 10.0);
}

/// Mean absolute difference of the crossing counts, scaled to [0, 1].
double crossingDistance(const std::vector<float>& a,
                        const std::vector<float>& b) {
  if (a.empty() || a.size() != b.size()) return 0.0;
  double sum = 0.0;
  for (size_t i = 0; i < a.size(); ++i) sum += std::abs(a[i] - b[i]);
  return std::min(1.0, sum / static_cast<double>(a.size()) / 8.0);
}

double zoningDistance(const std::vector<float>& a,
                      const std::vector<float>& b) {
  if (a.size() != b.size() || a.empty()) return 0.0;
  double sum = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    sum += std::abs(a[i] - b[i]);
  }
  return sum / static_cast<double>(a.size());
}

/// Mean-centred unit vector over the 16x16 raster, the same representation the
/// template bank stores.
std::vector<float> centreFeature(const std::vector<float>& raw) {
  std::vector<float> out = raw;
  if (out.empty()) return out;
  double mean = 0.0;
  for (float v : out) mean += v;
  mean /= static_cast<double>(out.size());
  double norm = 0.0;
  for (float& v : out) {
    v = static_cast<float>(v - mean);
    norm += static_cast<double>(v) * v;
  }
  norm = std::sqrt(norm);
  if (norm > 1e-9) {
    for (float& v : out) v = static_cast<float>(v / norm);
  }
  return out;
}

uint32_t readU32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

bool readFloat(const uint8_t* data, float* value) {
  uint32_t bits = readU32(data);
  std::memcpy(value, &bits, sizeof(float));
  return true;
}

}  // namespace

GlyphMatch classifyGlyph(const GrayImage& binary, const Box& box,
                         const TemplateBank& bank,
                         const ClassifyOptions& options) {
  GlyphMatch match;
  if (bank.empty()) return match;

  std::vector<float> raw;
  double aspect = 1.0;
  if (!rasteriseGlyph(binary, box, &raw, &aspect)) return match;

  // The 16x16 correlation is the primary signal; zoning, hole count and aspect
  // break close calls that a pure pixel correlation cannot separate (1 vs l,
  // O vs 0, . vs ,).
  const std::vector<float> feature = centreFeature(raw);
  if (feature.empty()) return match;

  const int holes = gridHoles(raw);
  const std::vector<float> zoning = gridZoning(raw, kZoning);
  const std::vector<float> profile = gridProfiles(raw);
  const std::vector<float> crossings = gridCrossings(raw);
  const std::vector<float> hu = gridHuMoments(raw);
  const double candidateAspect = std::max(0.05, aspect);

  struct Best {
    double cosine = -1.0;
    double score = -1.0;
  };
  std::map<std::string, Best> bestPerLabel;
  for (const GlyphTemplate& entry : bank.templates()) {
    if (entry.feature.size() != feature.size()) continue;
    double dot = 0.0;
    for (size_t i = 0; i < feature.size(); ++i) {
      dot += static_cast<double>(feature[i]) * entry.feature[i];
    }

    const double zoningTerm = zoningDistance(zoning, entry.zoning);
    const double holeTerm = std::abs(holes - entry.holes);
    const double aspectTerm =
        std::abs(std::log(candidateAspect / std::max(0.05, entry.aspect)));
    const double profileTerm = profileDistance(profile, entry.profile);
    const double crossingTerm = crossingDistance(crossings, entry.crossings);
    const double huTerm = huDistance(hu, entry.hu);
    // Mild shape terms: they break ties without overriding a strong
    // correlation match. The projection, crossing and Hu terms come from the
    // plan's feature list; their weights are small on purpose, so a genuinely
    // strong correlation match still wins.
    const double score = dot - 0.06 * zoningTerm - 0.10 * holeTerm -
                         0.03 * aspectTerm - 0.05 * profileTerm -
                         0.02 * crossingTerm - 0.03 * huTerm;

    auto it = bestPerLabel.find(entry.label);
    if (it == bestPerLabel.end() || score > it->second.score) {
      bestPerLabel[entry.label] = Best{dot, score};
    }
  }
  if (bestPerLabel.empty()) return match;

  std::vector<std::pair<double, std::string>> ranked;
  ranked.reserve(bestPerLabel.size());
  for (const auto& item : bestPerLabel) {
    ranked.emplace_back(item.second.score, item.first);
  }
  std::sort(ranked.begin(), ranked.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  match.score = ranked[0].first;
  match.label = ranked[0].second;
  const size_t top = std::min<size_t>(3, ranked.size());
  for (size_t i = 0; i < top; ++i) {
    match.alternatives.emplace_back(ranked[i].second, ranked[i].first);
  }
  if (ranked.size() > 1) {
    match.runnerUp = ranked[1].second;
    match.margin = ranked[0].first - ranked[1].first;
  } else {
    match.margin = 1.0;
  }
  match.confident =
      match.score >= options.minScore && match.margin >= options.minMargin;
  // A low margin means "needs review", not "unknown": keep the best label so
  // structure analysis can use the surrounding context, and let the correction
  // sheet and the parser-guided repair revisit it. Only a genuinely weak match
  // becomes "?".
  if (match.score < options.minScore) match.label = "?";
  return match;
}

TemplateClassifier::TemplateClassifier(const TemplateBank& bank,
                                       ClassifyOptions options)
    : bank_(bank), options_(options) {}

GlyphMatch TemplateClassifier::classify(const GrayImage& binary,
                                        const Box& box) const {
  return classifyGlyph(binary, box, bank_, options_);
}

bool PrototypeClassifier::load(const std::string& path, std::string* error) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    if (error) *error = "cannot open prototype set: " + path;
    return false;
  }
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(stream)),
                            std::istreambuf_iterator<char>());
  return loadFromMemory(data.data(), data.size(), error);
}

bool PrototypeClassifier::loadFromMemory(const uint8_t* data, size_t size,
                                         std::string* error) {
  prototypes_.clear();
  if (data == nullptr || size < 12) {
    if (error) *error = "prototype set is too short";
    return false;
  }
  if (std::memcmp(data, "IPRO", 4) != 0) {
    if (error) *error = "prototype set has a bad magic";
    return false;
  }
  const uint32_t version = readU32(data + 4);
  if (version != kPrototypeVersion) {
    if (error) *error = "unsupported prototype version";
    return false;
  }
  const uint32_t count = readU32(data + 8);
  size_t offset = 12;
  const size_t gridBytes =
      static_cast<size_t>(kGlyphGrid) * kGlyphGrid * sizeof(float);
  prototypes_.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    if (offset >= size) {
      if (error) *error = "truncated prototype set";
      prototypes_.clear();
      return false;
    }
    const uint8_t labelLength = data[offset++];
    if (offset + labelLength > size) {
      if (error) *error = "truncated prototype label";
      prototypes_.clear();
      return false;
    }
    Prototype entry;
    entry.label.assign(reinterpret_cast<const char*>(data + offset),
                       labelLength);
    offset += labelLength;
    if (offset + gridBytes > size) {
      if (error) *error = "truncated prototype raster";
      prototypes_.clear();
      return false;
    }
    entry.raw.resize(static_cast<size_t>(kGlyphGrid) * kGlyphGrid);
    for (size_t p = 0; p < entry.raw.size(); ++p) {
      float value = 0.0f;
      readFloat(data + offset + p * sizeof(float), &value);
      entry.raw[p] = value;
    }
    offset += gridBytes;
    entry.feature = centreFeature(entry.raw);
    if (entry.feature.empty()) continue;
    entry.holes = gridHoles(entry.raw);
    entry.aspect = gridAspect(entry.raw);
    entry.profile = gridProfiles(entry.raw);
    entry.crossings = gridCrossings(entry.raw);
    entry.hu = gridHuMoments(entry.raw);
    prototypes_.push_back(std::move(entry));
  }
  return true;
}

bool PrototypeClassifier::add(const std::string& label,
                              const std::vector<float>& grid) {
  if (label.empty() ||
      grid.size() != static_cast<size_t>(kGlyphGrid) * kGlyphGrid) {
    return false;
  }
  Prototype entry;
  entry.label = label;
  entry.raw = grid;
  entry.feature = centreFeature(grid);
  if (entry.feature.empty()) return false;
  entry.holes = gridHoles(grid);
  entry.aspect = gridAspect(grid);
  entry.profile = gridProfiles(grid);
  entry.crossings = gridCrossings(grid);
  entry.hu = gridHuMoments(grid);
  prototypes_.push_back(std::move(entry));
  return true;
}

bool PrototypeClassifier::addFromImage(const GrayImage& image,
                                       const std::string& label) {
  if (image.empty()) return false;
  const int threshold = otsuThreshold(image);
  GrayImage binary(image.width, image.height, 0);
  int x0 = image.width;
  int y0 = image.height;
  int x1 = -1;
  int y1 = -1;
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      if (image.at(x, y) > threshold) continue;
      binary.at(x, y) = 255;
      x0 = std::min(x0, x);
      y0 = std::min(y0, y);
      x1 = std::max(x1, x);
      y1 = std::max(y1, y);
    }
  }
  if (x1 < x0 || y1 < y0) return false;

  std::vector<float> grid;
  double aspect = 1.0;
  const Box box{x0, y0, x1 + 1, y1 + 1};
  if (!rasteriseGlyph(binary, box, &grid, &aspect)) return false;
  return add(label, grid);
}

bool PrototypeClassifier::serialize(std::vector<uint8_t>* out) const {
  if (out == nullptr) return false;
  out->clear();
  if (prototypes_.size() > 0xffffffffu) return false;
  const auto appendU32 = [out](uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
      out->push_back(static_cast<uint8_t>((value >> shift) & 0xff));
    }
  };
  out->insert(out->end(), {'I', 'P', 'R', 'O'});
  appendU32(kPrototypeVersion);
  appendU32(static_cast<uint32_t>(prototypes_.size()));
  for (const Prototype& entry : prototypes_) {
    const size_t labelLength = std::min<size_t>(entry.label.size(), 255);
    out->push_back(static_cast<uint8_t>(labelLength));
    out->insert(out->end(), entry.label.begin(),
                entry.label.begin() + static_cast<std::ptrdiff_t>(labelLength));
    for (float value : entry.raw) {
      uint32_t bits = 0;
      std::memcpy(&bits, &value, sizeof(float));
      appendU32(bits);
    }
  }
  return true;
}

GlyphMatch PrototypeClassifier::classify(const GrayImage& binary,
                                         const Box& box,
                                         ClassifyOptions options) const {
  GlyphMatch match;
  if (prototypes_.empty()) return match;

  std::vector<float> raw;
  double aspect = 1.0;
  if (!rasteriseGlyph(binary, box, &raw, &aspect)) return match;
  const std::vector<float> feature = centreFeature(raw);
  if (feature.empty()) return match;

  const int holes = gridHoles(raw);
  const std::vector<float> profile = gridProfiles(raw);
  const std::vector<float> crossings = gridCrossings(raw);
  const std::vector<float> hu = gridHuMoments(raw);
  const double candidateAspect = std::max(0.05, aspect);

  struct Neighbour {
    double distance = 0.0;
    double cosine = 0.0;
    size_t index = 0;
  };
  std::vector<Neighbour> neighbours;
  neighbours.reserve(prototypes_.size());
  for (size_t index = 0; index < prototypes_.size(); ++index) {
    const Prototype& entry = prototypes_[index];
    if (entry.feature.size() != feature.size()) continue;
    double dot = 0.0;
    for (size_t i = 0; i < feature.size(); ++i) {
      dot += static_cast<double>(feature[i]) * entry.feature[i];
    }
    double distance = std::sqrt(std::max(0.0, 2.0 - 2.0 * dot));
    distance += 0.10 * std::abs(holes - entry.holes);
    distance += 0.05 * std::abs(
        std::log(candidateAspect / std::max(0.05, entry.aspect)));
    distance += 0.05 * profileDistance(profile, entry.profile);
    distance += 0.02 * crossingDistance(crossings, entry.crossings);
    distance += 0.03 * huDistance(hu, entry.hu);
    neighbours.push_back(Neighbour{distance, dot, index});
  }
  if (neighbours.empty()) return match;

  const size_t k = std::min<size_t>(kNeighbours, neighbours.size());
  std::partial_sort(neighbours.begin(), neighbours.begin() + k,
                    neighbours.end(),
                    [](const auto& a, const auto& b) {
                      return a.distance < b.distance;
                    });

  struct Vote {
    double weight = 0.0;
    double cosine = -1.0;
  };
  std::map<std::string, Vote> votes;
  double total = 0.0;
  for (size_t i = 0; i < k; ++i) {
    const Neighbour& neighbour = neighbours[i];
    const std::string& label = prototypes_[neighbour.index].label;
    const double weight = 1.0 / (0.05 + neighbour.distance);
    Vote& vote = votes[label];
    vote.weight += weight;
    vote.cosine = std::max(vote.cosine, neighbour.cosine);
    total += weight;
  }
  if (total <= 0.0) return match;

  std::vector<std::pair<double, std::string>> ranked;
  ranked.reserve(votes.size());
  for (const auto& item : votes) {
    ranked.emplace_back(item.second.weight, item.first);
  }
  std::sort(ranked.begin(), ranked.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  match.label = ranked[0].second;
  match.score = votes[ranked[0].second].cosine;
  const size_t top = std::min<size_t>(3, ranked.size());
  for (size_t i = 0; i < top; ++i) {
    match.alternatives.emplace_back(ranked[i].second,
                                    votes[ranked[i].second].cosine);
  }
  if (ranked.size() > 1) {
    match.runnerUp = ranked[1].second;
    match.margin = (ranked[0].first - ranked[1].first) / total;
  } else {
    match.margin = 1.0;
  }
  match.confident =
      match.score >= options.minScore && match.margin >= options.minMargin;
  if (match.score < options.minScore) match.label = "?";
  return match;
}

HybridClassifier::HybridClassifier(const TemplateBank& bank,
                                   ClassifyOptions options)
    : tierA_(bank, options) {
  clouds_.loadFromTemplates(bank);
}

GlyphMatch HybridClassifier::classify(const GrayImage& binary,
                                      const Box& box) const {
  const GlyphMatch fromTemplates = tierA_.classify(binary, box);
  GlyphMatch answer = fromTemplates;

  if (!prototypes_.empty()) {
    const GlyphMatch fromPrototypes = prototypes_.classify(binary, box);
    if (!fromPrototypes.label.empty()) {
      if (preferPrototypes_) {
        // Handwriting: the personal prototype set knows the writer's shapes.
        if (fromPrototypes.score >= 0.45) answer = fromPrototypes;
      } else if (!fromTemplates.confident && fromPrototypes.score >= 0.70 &&
                 fromPrototypes.margin >= 0.15) {
        // Printed text: templates win unless they are genuinely weak and the
        // prototype set has a strong answer.
        answer = fromPrototypes;
      }
    }
  }

  // The $P cloud matcher is blind to stroke thickness and to how much ink was
  // added, so it rescues the multi-stroke symbols the 16x16 raster blurs.
  if (!clouds_.empty() && (preferPrototypes_ || !answer.confident)) {
    const PointCloud cloud = cloudFromBox(binary, box, 64);
    const CloudMatch fromClouds = clouds_.match(cloud);
    if (!fromClouds.label.empty() && fromClouds.score >= 0.55 &&
        fromClouds.score > answer.score + 0.06) {
      answer.label = fromClouds.label;
      answer.runnerUp = fromClouds.runnerUp;
      answer.score = fromClouds.score;
      answer.margin = fromClouds.margin;
      answer.confident = fromClouds.confident;
      answer.alternatives = fromClouds.alternatives;
    }
  }
  return answer;
}

}  // namespace infinit::scan
