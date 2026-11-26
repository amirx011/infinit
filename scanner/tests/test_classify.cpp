// Checks for the recognizer seam: Tier A (templates) behind ISymbolClassifier,
// Tier B (kNN prototypes) and the hybrid that chooses between them.
#include <cstdint>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "classify.hpp"
#include "glyphs.hpp"
#include "image.hpp"
#include "preprocess.hpp"
#include "segment.hpp"

#ifndef SCANNER_DATA_DIR
#define SCANNER_DATA_DIR "data"
#endif
#ifndef SCANNER_SAMPLES_DIR
#define SCANNER_SAMPLES_DIR "samples"
#endif

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

std::vector<uint8_t> readBytes(const std::string& path) {
  std::ifstream stream(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(stream)),
                              std::istreambuf_iterator<char>());
}

/// Black cross on white: enough ink for every feature to be defined, and
/// clearly not a template glyph.
GrayImage crossImage() {
  GrayImage image(64, 64, 255);
  for (int x = 12; x < 52; ++x) {
    for (int thickness = 0; thickness < 4; ++thickness) {
      image.at(x, 30 + thickness) = 0;
      image.at(30 + thickness, x) = 0;
    }
  }
  return image;
}

/// The classifier takes an ink-is-white binary image; the board sends black on
/// white, so the test mirrors what preprocess does.
GrayImage inkBinary(const GrayImage& gray) {
  GrayImage out(gray.width, gray.height, 0);
  for (size_t i = 0; i < gray.pixels.size(); ++i) {
    out.pixels[i] = gray.pixels[i] < 128 ? 255 : 0;
  }
  return out;
}

}  // namespace

int main() {
  TemplateBank bank;
  std::string error;
  const std::vector<uint8_t> bankBytes =
      readBytes(std::string(SCANNER_DATA_DIR) + "/glyph_templates.bin");
  check("bank loads",
        bank.loadFromMemory(bankBytes.data(), bankBytes.size(), &error),
        error);

  const PreprocessResult pre =
      preprocess(loadImage(std::string(SCANNER_SAMPLES_DIR) + "/quadratic.png"));
  const Segmentation segmentation = segment(pre.binary);
  check("sample segments", !segmentation.symbols.empty());
  if (segmentation.symbols.empty() || bank.empty()) return 1;
  const Box box = segmentation.symbols[0].box;

  // Tier A behind the interface must agree with the free function.
  const TemplateClassifier tierA(bank);
  const GlyphMatch free = classifyGlyph(pre.binary, box, bank);
  const GlyphMatch wrapped = tierA.classify(pre.binary, box);
  check("template tier matches classifyGlyph",
        free.label == wrapped.label &&
            std::abs(free.score - wrapped.score) < 1e-9,
        free.label + " vs " + wrapped.label);

  // Tier B: the same crop added under a new label must come back as that label,
  // and the set must survive a serialize/load round trip.
  std::vector<float> grid;
  double aspect = 1.0;
  check("crop rasterises", rasteriseGlyph(pre.binary, box, &grid, &aspect));

  PrototypeClassifier prototypes;
  check("prototype adds", prototypes.add("Q", grid));
  check("prototype count", prototypes.size() == 1);

  const GlyphMatch nearest = prototypes.classify(pre.binary, box);
  check("knn returns the prototype label", nearest.label == "Q", nearest.label);
  check("knn score is high", nearest.score > 0.9, std::to_string(nearest.score));

  std::vector<uint8_t> blob;
  check("prototype serialises", prototypes.serialize(&blob));
  PrototypeClassifier reloaded;
  check("prototype set reloads",
        reloaded.loadFromMemory(blob.data(), blob.size(), &error), error);
  check("reloaded count", reloaded.size() == 1);
  const GlyphMatch fromReloaded = reloaded.classify(pre.binary, box);
  check("reloaded knn label", fromReloaded.label == "Q", fromReloaded.label);

  // A whole-symbol raster (what the board sends) goes through addFromImage.
  PrototypeClassifier fromImage;
  check("addFromImage accepts a raster",
        fromImage.addFromImage(crossImage(), "x"));
  const Box whole{0, 0, 64, 64};
  const GrayImage crossBinary = inkBinary(crossImage());
  const GlyphMatch crossMatch = fromImage.classify(crossBinary, whole);
  check("addFromImage round trip", crossMatch.label == "x", crossMatch.label);

  // The hybrid prefers templates for printed text, and prototypes when the
  // handwriting mode asks for them.
  HybridClassifier hybrid(bank);
  hybrid.prototypes().add("Q", grid);
  const GlyphMatch printed = hybrid.classify(pre.binary, box);
  check("hybrid uses templates by default", printed.label == free.label,
        printed.label);
  hybrid.setPreferPrototypes(true);
  const GlyphMatch handwriting = hybrid.classify(pre.binary, box);
  check("hybrid prefers prototypes in handwriting mode",
        handwriting.label == "Q", handwriting.label);

  // Empty tier B must never change a printed reading.
  HybridClassifier withoutPrototypes(bank);
  check("empty prototypes fall back to templates",
        withoutPrototypes.classify(pre.binary, box).label == free.label);

  if (failures > 0) {
    std::cout << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "all classify checks passed\n";
  return 0;
}
