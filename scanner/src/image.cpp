#include "image.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STBI_ONLY_GIF
#include "../third_party/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

namespace infinit::scan {

GrayImage::GrayImage(int w, int h, uint8_t fill)
    : width(w), height(h), pixels(static_cast<size_t>(w) * h, fill) {}

RgbImage::RgbImage(int w, int h, uint8_t r, uint8_t g, uint8_t b)
    : width(w), height(h), pixels(static_cast<size_t>(w) * h * 3) {
  for (size_t i = 0; i < pixels.size(); i += 3) {
    pixels[i] = r;
    pixels[i + 1] = g;
    pixels[i + 2] = b;
  }
}

void RgbImage::set(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (x < 0 || y < 0 || x >= width || y >= height) return;
  const size_t i = (static_cast<size_t>(y) * width + x) * 3;
  pixels[i] = r;
  pixels[i + 1] = g;
  pixels[i + 2] = b;
}

void RgbImage::rect(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g,
                    uint8_t b, int thickness) {
  if (thickness < 1) thickness = 1;
  for (int t = 0; t < thickness; ++t) {
    for (int x = x0 + t; x <= x1 - t; ++x) {
      set(x, y0 + t, r, g, b);
      set(x, y1 - t, r, g, b);
    }
    for (int y = y0 + t; y <= y1 - t; ++y) {
      set(x0 + t, y, r, g, b);
      set(x1 - t, y, r, g, b);
    }
  }
}

GrayImage loadImage(const std::string& path) {
  int w = 0;
  int h = 0;
  int channels = 0;
  stbi_uc* data = stbi_load(path.c_str(), &w, &h, &channels, 1);
  if (data == nullptr) {
    throw std::runtime_error("cannot decode image: " + path);
  }
  GrayImage image(w, h);
  std::copy(data, data + static_cast<size_t>(w) * h, image.pixels.begin());
  stbi_image_free(data);
  return image;
}

GrayImage decodeImage(const uint8_t* data, size_t size) {
  int w = 0;
  int h = 0;
  int channels = 0;
  stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(size), &w, &h,
                                          &channels, 1);
  if (pixels == nullptr) {
    throw std::runtime_error("cannot decode image from memory");
  }
  GrayImage image(w, h);
  std::copy(pixels, pixels + static_cast<size_t>(w) * h, image.pixels.begin());
  stbi_image_free(pixels);
  return image;
}

void savePng(const GrayImage& image, const std::string& path) {
  if (image.empty()) return;
  stbi_write_png(path.c_str(), image.width, image.height, 1,
                 image.pixels.data(), image.width);
}

void savePng(const RgbImage& image, const std::string& path) {
  if (image.width <= 0 || image.height <= 0) return;
  stbi_write_png(path.c_str(), image.width, image.height, 3,
                 image.pixels.data(), image.width * 3);
}

GrayImage downscaleToLongSide(const GrayImage& image, int longSide) {
  const int longest = std::max(image.width, image.height);
  if (longSide <= 0 || longest <= longSide) return image;
  const double scale = static_cast<double>(longSide) / longest;
  const int w = std::max(1, static_cast<int>(std::lround(image.width * scale)));
  const int h = std::max(1, static_cast<int>(std::lround(image.height * scale)));

  GrayImage out(w, h);
  const double sx = static_cast<double>(image.width) / w;
  const double sy = static_cast<double>(image.height) / h;
  for (int y = 0; y < h; ++y) {
    const int y0 = static_cast<int>(y * sy);
    const int y1 = std::min(image.height, std::max(y0 + 1, static_cast<int>((y + 1) * sy)));
    for (int x = 0; x < w; ++x) {
      const int x0 = static_cast<int>(x * sx);
      const int x1 = std::min(image.width, std::max(x0 + 1, static_cast<int>((x + 1) * sx)));
      int sum = 0;
      int count = 0;
      for (int yy = y0; yy < y1; ++yy) {
        for (int xx = x0; xx < x1; ++xx) {
          sum += image.at(xx, yy);
          ++count;
        }
      }
      out.at(x, y) = static_cast<uint8_t>(sum / std::max(1, count));
    }
  }
  return out;
}

}  // namespace infinit::scan
