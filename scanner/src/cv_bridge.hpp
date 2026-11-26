#pragma once

#include <cstring>

#include <opencv2/core.hpp>

#include "image.hpp"

namespace infinit::scan {

/// Zero-copy view of a GrayImage as an 8-bit single-channel cv::Mat.
inline cv::Mat view(GrayImage& image) {
  return cv::Mat(image.height, image.width, CV_8UC1, image.pixels.data(),
                 static_cast<size_t>(image.width));
}

inline cv::Mat view(const GrayImage& image) {
  return cv::Mat(image.height, image.width, CV_8UC1,
                 const_cast<uint8_t*>(image.pixels.data()),
                 static_cast<size_t>(image.width));
}

/// Copies a cv::Mat into a tightly packed GrayImage.
inline GrayImage copyOf(const cv::Mat& source) {
  GrayImage out(source.cols, source.rows);
  if (source.isContinuous()) {
    std::memcpy(out.pixels.data(), source.data, out.pixels.size());
  } else {
    for (int y = 0; y < source.rows; ++y) {
      std::memcpy(out.pixels.data() + static_cast<size_t>(y) * source.cols,
                  source.ptr(y), static_cast<size_t>(source.cols));
    }
  }
  return out;
}

}  // namespace infinit::scan
