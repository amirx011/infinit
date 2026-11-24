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
};

struct PreprocessResult {
  GrayImage gray;        // working resolution, orientation corrected
  GrayImage normalized;  // illumination flattened
  GrayImage binaryOtsu;
  GrayImage binarySauvola;
  GrayImage binary;      // the candidate the next stage should use
  std::string chosen = "otsu";
  double skewDegrees = 0.0;
  int otsuThreshold = 0;
  bool inverted = false;
};

PreprocessResult preprocess(const GrayImage& input,
                            const PreprocessOptions& options = {});

/// Rotates around the centre with bilinear sampling.
GrayImage rotate(const GrayImage& image, double degrees, uint8_t fill);

int otsuThreshold(const GrayImage& image);

}  // namespace infinit::scan
