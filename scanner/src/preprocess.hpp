#pragma once

#include <string>

#include "image.hpp"

namespace infinit::scan {

struct PreprocessOptions {
  int maxLongSide = 1600;
  int backgroundRadiusDiv = 24;
  int sauvolaWindowDiv = 16;
  double sauvolaK = 0.20;
  double maxDeskewDegrees = 8.0;
  /// CLAHE is applied only when the flattened image is still low contrast.
  bool applyClahe = true;
  /// Long ruled notebook lines are removed; short bars (minus, fraction bars)
  /// are left alone.
  bool removeRuledLines = true;
  int adaptiveBlockDiv = 32;
  double adaptiveC = 8.0;
  /// Auto-detect screenshots and rendered pages. Tests and callers that want
  /// the full photo pipeline can turn the detection off.
  bool allowCleanDigital = true;
};

struct PreprocessResult {
  GrayImage gray;        // working resolution, orientation corrected
  GrayImage normalized;  // illumination flattened
  GrayImage binaryOtsu;
  GrayImage binarySauvola;
  GrayImage binaryAdaptive;
  GrayImage binary;      // the candidate the next stage should use
  std::string chosen = "otsu";
  /// "photo" for uneven lighting; "clean" for screenshots and rendered pages,
  /// where illumination flattening and deskew would only add artifacts.
  std::string profile = "photo";
  double skewDegrees = 0.0;
  int otsuThreshold = 0;
  bool inverted = false;
  /// True when a ruled line was found and subtracted.
  bool ruledLinesRemoved = false;
  /// Skew estimates, kept for the debug viewer.
  double baselineDegrees = 0.0;
  double projectionDegrees = 0.0;
  double rectangleDegrees = 0.0;
  double houghDegrees = 0.0;
};

PreprocessResult preprocess(const GrayImage& input,
                            const PreprocessOptions& options = {});

/// Rotates around the centre with bilinear sampling.
GrayImage rotate(const GrayImage& image, double degrees, uint8_t fill);

int otsuThreshold(const GrayImage& image);

}  // namespace infinit::scan
