#include "inkproto.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace infinit::scan {
namespace {

constexpr uint32_t kInkPrototypeVersion = 1;
constexpr std::size_t kPointsPerStroke = 24;
/// Two samples this close to each other are the same writing, so keeping both
/// only slows matching down.
constexpr double kDuplicateScore = 0.97;

uint32_t readU32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

void appendU32(std::vector<uint8_t>* out, uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    out->push_back(static_cast<uint8_t>((value >> shift) & 0xff));
  }
}

void appendFloat(std::vector<uint8_t>* out, float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(float));
  appendU32(out, bits);
}

float readFloat(const uint8_t* data) {
  const uint32_t bits = readU32(data);
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(float));
  return value;
}

std::vector<int> sequentialIds(std::size_t count) {
  std::vector<int> ids(count);
  for (std::size_t i = 0; i < count; ++i) ids[i] = static_cast<int>(i);
  return ids;
}

}  // namespace

bool InkPrototypeBank::loadFromMemory(const uint8_t* data, std::size_t size,
                                      std::string* error) {
  entries_.clear();
  if (data == nullptr || size < 12) {
    if (error) *error = "ink prototype set is too short";
    return false;
  }
  if (std::memcmp(data, "IINK", 4) != 0) {
    if (error) *error = "ink prototype set has a bad magic";
    return false;
  }
  if (readU32(data + 4) != kInkPrototypeVersion) {
    if (error) *error = "unsupported ink prototype version";
    return false;
  }
  const uint32_t count = readU32(data + 8);
  std::size_t offset = 12;
  entries_.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    if (offset >= size) {
      if (error) *error = "truncated ink prototype set";
      entries_.clear();
      return false;
    }
    const std::size_t labelLength = data[offset++];
    if (offset + labelLength + 4 > size) {
      if (error) *error = "truncated ink prototype label";
      entries_.clear();
      return false;
    }
    InkPrototype entry;
    entry.label.assign(reinterpret_cast<const char*>(data + offset), labelLength);
    offset += labelLength;
    const uint32_t strokeCount = readU32(data + offset);
    offset += 4;
    entry.strokes.reserve(strokeCount);
    for (uint32_t s = 0; s < strokeCount; ++s) {
      if (offset + 4 > size) {
        if (error) *error = "truncated ink prototype stroke";
        entries_.clear();
        return false;
      }
      const uint32_t pointCount = readU32(data + offset);
      offset += 4;
      if (offset + static_cast<std::size_t>(pointCount) * 8 > size) {
        if (error) *error = "truncated ink prototype points";
        entries_.clear();
        return false;
      }
      InkStroke stroke;
      stroke.points.reserve(pointCount);
      for (uint32_t p = 0; p < pointCount; ++p) {
        InkPoint point;
        point.x = readFloat(data + offset);
        point.y = readFloat(data + offset + 4);
        offset += 8;
        stroke.box.include(
            Box{static_cast<int>(std::floor(point.x)),
                static_cast<int>(std::floor(point.y)),
                static_cast<int>(std::ceil(point.x)) + 1,
                static_cast<int>(std::ceil(point.y)) + 1});
        stroke.points.push_back(point);
      }
      entry.strokes.push_back(std::move(stroke));
    }
    if (!entry.strokes.empty()) {
      entry.cloud = cloudFromStrokes(entry.strokes,
                                     sequentialIds(entry.strokes.size()),
                                     kPointsPerStroke);
      entries_.push_back(std::move(entry));
    }
  }
  return true;
}

std::vector<uint8_t> InkPrototypeBank::serialize() const {
  std::vector<uint8_t> out;
  out.insert(out.end(), {'I', 'I', 'N', 'K'});
  appendU32(&out, kInkPrototypeVersion);
  appendU32(&out, static_cast<uint32_t>(entries_.size()));
  for (const InkPrototype& entry : entries_) {
    const std::size_t labelLength = std::min<std::size_t>(entry.label.size(), 255);
    out.push_back(static_cast<uint8_t>(labelLength));
    out.insert(out.end(), entry.label.begin(),
               entry.label.begin() + static_cast<std::ptrdiff_t>(labelLength));
    appendU32(&out, static_cast<uint32_t>(entry.strokes.size()));
    for (const InkStroke& stroke : entry.strokes) {
      const auto points = std::min<std::size_t>(stroke.points.size(), 0xffffffu);
      appendU32(&out, static_cast<uint32_t>(points));
      for (std::size_t p = 0; p < points; ++p) {
        appendFloat(&out, stroke.points[p].x);
        appendFloat(&out, stroke.points[p].y);
      }
    }
  }
  return out;
}

void InkPrototypeBank::prune(const std::string& label) {
  std::vector<InkPrototype> sameLabel;
  std::vector<InkPrototype> others;
  for (auto& entry : entries_) {
    if (entry.label == label) {
      sameLabel.push_back(std::move(entry));
    } else {
      others.push_back(std::move(entry));
    }
  }
  std::vector<InkPrototype> kept;
  for (auto& entry : sameLabel) {
    bool duplicate = false;
    for (const auto& existing : kept) {
      if (matchClouds(entry.cloud, existing.cloud) >= kDuplicateScore) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) kept.push_back(std::move(entry));
  }
  if (kept.size() > kInkPrototypesPerClass) {
    kept.resize(kInkPrototypesPerClass);
  }
  others.insert(others.end(), std::make_move_iterator(kept.begin()),
                std::make_move_iterator(kept.end()));
  entries_ = std::move(others);
}

bool InkPrototypeBank::add(const std::string& label,
                           const std::vector<InkStroke>& strokes) {
  if (label.empty() || strokes.empty()) return false;
  InkPrototype entry;
  entry.label = label;
  entry.strokes = strokes;
  entry.cloud = cloudFromStrokes(entry.strokes,
                                 sequentialIds(entry.strokes.size()),
                                 kPointsPerStroke);
  if (entry.cloud.points.empty()) return false;
  entries_.push_back(std::move(entry));
  prune(label);
  return true;
}

CloudMatch InkPrototypeBank::match(const std::vector<InkStroke>& strokes,
                                   const std::vector<int>& ids,
                                   std::size_t k) const {
  CloudMatch best;
  if (entries_.empty() || ids.empty()) return best;
  const PointCloud query = cloudFromStrokes(strokes, ids, kPointsPerStroke);
  if (query.points.empty()) return best;

  std::map<std::string, double> votes;
  std::map<std::string, double> bestScore;
  for (const InkPrototype& entry : entries_) {
    const double score = matchClouds(query, entry.cloud);
    const double weight = 1.0 / (0.05 + std::max(0.0, 1.0 - score));
    votes[entry.label] += weight;
    bestScore[entry.label] = std::max(bestScore[entry.label], score);
  }
  if (votes.empty()) return best;

  std::vector<std::pair<double, std::string>> ranked;
  ranked.reserve(votes.size());
  double total = 0.0;
  for (const auto& item : votes) {
    ranked.emplace_back(item.second, item.first);
    total += item.second;
  }
  std::sort(ranked.begin(), ranked.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  best.label = ranked[0].second;
  best.score = bestScore[ranked[0].second];
  const std::size_t top = std::min<std::size_t>(3, ranked.size());
  for (std::size_t i = 0; i < top; ++i) {
    best.alternatives.emplace_back(ranked[i].second,
                                   bestScore[ranked[i].second]);
  }
  if (ranked.size() > 1) {
    best.runnerUp = ranked[1].second;
    best.margin = total > 0.0 ? (ranked[0].first - ranked[1].first) / total : 0.0;
  } else {
    best.margin = 1.0;
  }
  best.confident = best.score >= 0.55 && best.margin >= 0.15;
  return best;
}

}  // namespace infinit::scan
