#include "image.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "cv_bridge.hpp"

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

namespace {

uint16_t read16(const uint8_t* p, bool little) {
  return little ? static_cast<uint16_t>(p[0] | (p[1] << 8))
                : static_cast<uint16_t>((p[0] << 8) | p[1]);
}

uint32_t read32(const uint8_t* p, bool little) {
  if (little) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
  }
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

/// Reads the EXIF orientation tag (0x0112) from a JPEG byte stream. Returns 1
/// when the tag is absent or the data is not a JPEG.
int exifOrientation(const uint8_t* data, size_t size) {
  if (data == nullptr || size < 4 || data[0] != 0xFF || data[1] != 0xD8) {
    return 1;
  }
  size_t offset = 2;
  while (offset + 4 <= size) {
    if (data[offset] != 0xFF) {
      ++offset;
      continue;
    }
    const uint8_t marker = data[offset + 1];
    if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
      offset += 2;
      continue;
    }
    if (marker == 0xD9 || marker == 0xDA) break;
    if (offset + 4 > size) break;
    const size_t length = (static_cast<size_t>(data[offset + 2]) << 8) |
                          data[offset + 3];
    if (length < 2 || offset + 2 + length > size) break;
    const uint8_t* segment = data + offset + 4;
    const size_t segmentSize = length - 2;
    if (marker == 0xE1 && segmentSize >= 6 &&
        std::memcmp(segment, "Exif\0\0", 6) == 0) {
      const uint8_t* tiff = segment + 6;
      const size_t tiffSize = segmentSize - 6;
      if (tiffSize < 8) return 1;
      const bool little = tiff[0] == 'I' && tiff[1] == 'I';
      if (!little && !(tiff[0] == 'M' && tiff[1] == 'M')) return 1;
      const uint32_t ifdOffset = read32(tiff + 4, little);
      if (ifdOffset + 2 > tiffSize) return 1;
      const uint16_t entries = read16(tiff + ifdOffset, little);
      for (uint16_t i = 0; i < entries; ++i) {
        const size_t entry = ifdOffset + 2 + static_cast<size_t>(i) * 12;
        if (entry + 12 > tiffSize) break;
        const uint16_t tag = read16(tiff + entry, little);
        if (tag != 0x0112) continue;
        const uint16_t type = read16(tiff + entry + 2, little);
        const uint32_t count = read32(tiff + entry + 4, little);
        if (type != 3 || count < 1) return 1;
        const uint16_t value = read16(tiff + entry + 8, little);
        return value >= 1 && value <= 8 ? value : 1;
      }
      return 1;
    }
    offset += 2 + length;
  }
  return 1;
}

GrayImage applyOrientation(const GrayImage& image, int orientation) {
  if (image.empty() || orientation <= 1 || orientation > 8) return image;
  cv::Mat source = view(image);
  cv::Mat out;
  switch (orientation) {
    case 2:
      cv::flip(source, out, 1);
      break;
    case 3:
      cv::rotate(source, out, cv::ROTATE_180);
      break;
    case 4:
      cv::flip(source, out, 0);
      break;
    case 5:
      cv::transpose(source, out);
      break;
    case 6:
      cv::rotate(source, out, cv::ROTATE_90_CLOCKWISE);
      break;
    case 7: {
      cv::Mat flipped;
      cv::flip(source, flipped, -1);
      cv::transpose(flipped, out);
      break;
    }
    case 8:
      cv::rotate(source, out, cv::ROTATE_90_COUNTERCLOCKWISE);
      break;
    default:
      return image;
  }
  return copyOf(out);
}

}  // namespace

GrayImage loadImage(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("cannot open image: " + path);
  }
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
  if (bytes.empty()) {
    throw std::runtime_error("empty image: " + path);
  }
  return decodeImage(bytes.data(), bytes.size());
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
  return applyOrientation(image, exifOrientation(data, size));
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

namespace {

void appendPngBytes(void* context, void* data, int size) {
  auto* out = static_cast<std::vector<uint8_t>*>(context);
  const auto* bytes = static_cast<const uint8_t*>(data);
  out->insert(out->end(), bytes, bytes + size);
}

}  // namespace

std::vector<uint8_t> encodePng(const GrayImage& image) {
  std::vector<uint8_t> out;
  if (image.empty()) return out;
  stbi_write_png_to_func(appendPngBytes, &out, image.width, image.height, 1,
                         image.pixels.data(), image.width);
  return out;
}

std::vector<uint8_t> encodePng(const RgbImage& image) {
  std::vector<uint8_t> out;
  if (image.width <= 0 || image.height <= 0) return out;
  stbi_write_png_to_func(appendPngBytes, &out, image.width, image.height, 3,
                         image.pixels.data(), image.width * 3);
  return out;
}

GrayImage downscaleToLongSide(const GrayImage& image, int longSide) {
  if (image.empty()) return image;
  const int longest = std::max(image.width, image.height);
  if (longSide <= 0 || longest <= longSide) return image;
  const double scale = static_cast<double>(longSide) / longest;
  const int w = std::max(1, static_cast<int>(std::lround(image.width * scale)));
  const int h = std::max(1, static_cast<int>(std::lround(image.height * scale)));
  cv::Mat out;
  cv::resize(view(image), out, cv::Size(w, h), 0.0, 0.0, cv::INTER_AREA);
  return copyOf(out);
}

}  // namespace infinit::scan
