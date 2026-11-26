#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "cloud.hpp"
#include "glyphs.hpp"
#include "image.hpp"
#include "segment.hpp"

namespace infinit::scan {

struct GlyphMatch {
  /// Best label. "?" only when the match is genuinely weak; a close call keeps
  /// the best label with `confident == false` so context can decide.
  std::string label;
  std::string runnerUp;
  /// Cosine similarity of the winning template.
  double score = 0.0;
  /// Difference between the best and the best *other* label.
  double margin = 0.0;
  bool confident = false;
  /// Up to three best labels and their scores, best first. This is what the
  /// correction sheet offers when a glyph is ambiguous.
  std::vector<std::pair<std::string, double>> alternatives;
};

struct ClassifyOptions {
  /// Below this cosine similarity the glyph is reported as "?".
  double minScore = 0.55;
  /// Labels this close to each other are reported as "?" instead of a guess.
  double minMargin = 0.02;
};

/// The only seam a future recognizer would plug into. Everything above this
/// interface (lexing, structure, repair, emitters) is recognizer-agnostic.
/// Tier A is the template matcher; Tier B is a hand-written kNN over a labelled
/// prototype set. No neural weights, no model files.
class ISymbolClassifier {
 public:
  virtual ~ISymbolClassifier() = default;
  virtual GlyphMatch classify(const GrayImage& binary, const Box& box) const = 0;
};

/// Tier A: normalized correlation against the synthetic template bank.
class TemplateClassifier final : public ISymbolClassifier {
 public:
  explicit TemplateClassifier(const TemplateBank& bank,
                              ClassifyOptions options = {});
  GlyphMatch classify(const GrayImage& binary, const Box& box) const override;

 private:
  const TemplateBank& bank_;
  ClassifyOptions options_;
};

/// Tier B: kNN over labelled prototypes (bundled plus user-collected).
///
/// Prototypes are plain data, not a model: a 16x16 raster plus its aspect and
/// hole count. The file format is little-endian:
///
///   "IPRO" | u32 version=1 | u32 count
///   count * ( u8 labelLength | label | 256 * f32 grid )
class PrototypeClassifier {
 public:
  bool load(const std::string& path, std::string* error = nullptr);
  bool loadFromMemory(const uint8_t* data, size_t size,
                      std::string* error = nullptr);

  /// Adds one labelled sample. `grid` is a 16x16 raster in [0, 1].
  bool add(const std::string& label, const std::vector<float>& grid);

  /// Adds a sample from a whole raster that contains one symbol (a board PNG,
  /// a cropped photo). The ink is found with Otsu and rasterised in place.
  bool addFromImage(const GrayImage& image, const std::string& label);

  /// Writes the set in the same format loadFromMemory accepts, so the app can
  /// persist the user's own samples.
  bool serialize(std::vector<uint8_t>* out) const;

  void clear() { prototypes_.clear(); }

  bool empty() const { return prototypes_.empty(); }
  size_t size() const { return prototypes_.size(); }

  /// Nearest-neighbour vote. Returns an empty-label match when the set is
  /// empty or the box has no ink.
  GlyphMatch classify(const GrayImage& binary, const Box& box,
                      ClassifyOptions options = {}) const;

 private:
  struct Prototype {
    std::string label;
    std::vector<float> feature;  // unit-length, mean-centred 16x16
    std::vector<float> raw;      // uncentred raster, for the aspect term
    // Cached grid features, filled by cacheGridFeatures(). Kept on the
    // prototype so a classify call does not recompute them per neighbour.
    int holes = 0;
    double aspect = 1.0;
    std::vector<float> profile;
    std::vector<float> crossings;
    std::vector<float> hu;
  };
  std::vector<Prototype> prototypes_;
};

/// Tier A with a Tier B fallback. Printed photos use the templates; the
/// handwriting path can ask for the prototypes first.
class HybridClassifier final : public ISymbolClassifier {
 public:
  explicit HybridClassifier(const TemplateBank& bank,
                            ClassifyOptions options = {});

  PrototypeClassifier& prototypes() { return prototypes_; }
  const PrototypeClassifier& prototypes() const { return prototypes_; }

  /// Handwriting mode: kNN answers first and the templates fill the gaps.
  void setPreferPrototypes(bool prefer) { preferPrototypes_ = prefer; }
  bool preferPrototypes() const { return preferPrototypes_; }

  GlyphMatch classify(const GrayImage& binary, const Box& box) const override;

  /// Point clouds derived from the same template bank, used as a third opinion
  /// in handwriting mode and when the raster match is unsure.
  const CloudBank& clouds() const { return clouds_; }

 private:
  TemplateClassifier tierA_;
  PrototypeClassifier prototypes_;
  CloudBank clouds_;
  bool preferPrototypes_ = false;
};

/// Tier A as a free function, kept for tests and debug tools.
GlyphMatch classifyGlyph(const GrayImage& binary, const Box& box,
                         const TemplateBank& bank,
                         const ClassifyOptions& options = {});

}  // namespace infinit::scan
