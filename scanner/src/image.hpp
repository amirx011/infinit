#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace infinit::scan {

/// 8-bit grayscale raster. Row-major, no padding.
struct GrayImage {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;

  GrayImage() = default;
  GrayImage(int w, int h, uint8_t fill = 255);

  bool empty() const { return width <= 0 || height <= 0; }

  uint8_t at(int x, int y) const {
    return pixels[static_cast<size_t>(y) * width + x];
  }
  uint8_t& at(int x, int y) {
    return pixels[static_cast<size_t>(y) * width + x];
  }
};

/// 24-bit RGB raster, used only for debug overlays.
struct RgbImage {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;

  RgbImage() = default;
  RgbImage(int w, int h, uint8_t r = 0, uint8_t g = 0, uint8_t b = 0);

  void set(int x, int y, uint8_t r, uint8_t g, uint8_t b);
  void rect(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b,
            int thickness = 1);
};

/// Decodes PNG, JPEG, BMP, TGA, GIF, PSD and HDR through stb_image.
/// Throws std::runtime_error when the file cannot be read.
GrayImage loadImage(const std::string& path);

/// Same decoder, fed from memory (an uploaded photo, an APK asset).
GrayImage decodeImage(const uint8_t* data, size_t size);

void savePng(const GrayImage& image, const std::string& path);
void savePng(const RgbImage& image, const std::string& path);

/// Box-filter downscale that keeps the aspect ratio. Never upscales.
GrayImage downscaleToLongSide(const GrayImage& image, int longSide);

}  // namespace infinit::scan
