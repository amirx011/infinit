#include "glyphs.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

namespace infinit::scan {
namespace {

uint32_t readU32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

void normaliseInPlace(std::vector<float>& values) {
  if (values.empty()) return;
  double mean = 0.0;
  for (float v : values) mean += v;
  mean /= static_cast<double>(values.size());

  double norm = 0.0;
  for (float& v : values) {
    v = static_cast<float>(v - mean);
    norm += static_cast<double>(v) * v;
  }
  norm = std::sqrt(norm);
  if (norm <= 1e-9) return;
  for (float& v : values) v = static_cast<float>(v / norm);
}

}  // namespace

bool TemplateBank::load(const std::string& path, std::string* error) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    if (error) *error = "cannot open template bank: " + path;
    return false;
  }
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(stream)),
                            std::istreambuf_iterator<char>());
  return loadFromMemory(data.data(), data.size(), error);
}

bool TemplateBank::loadFromMemory(const uint8_t* data, size_t size,
                                  std::string* error) {
  templates_.clear();
  labels_.clear();

  const auto fail = [error](const std::string& message) {
    if (error) *error = message;
    return false;
  };

  if (data == nullptr || size < 8) return fail("template bank is truncated");
  if (std::memcmp(data, "IGT1", 4) != 0) return fail("template bank has a bad magic");

  const uint32_t count = readU32(data + 4);
  size_t offset = 8;
  const size_t pixels = static_cast<size_t>(kGlyphGrid) * kGlyphGrid;

  for (uint32_t i = 0; i < count; ++i) {
    if (offset + 1 > size) return fail("template bank is truncated");
    const uint8_t labelLength = data[offset++];
    if (offset + labelLength + 3 > size) return fail("template bank is truncated");

    GlyphTemplate entry;
    entry.label.assign(reinterpret_cast<const char*>(data + offset), labelLength);
    offset += labelLength;
    entry.fontIndex = data[offset++];
    entry.width = data[offset++];
    entry.height = data[offset++];
    if (entry.width != kGlyphGrid || entry.height != kGlyphGrid) {
      return fail("template bank has an unexpected grid size");
    }
    if (offset + pixels > size) return fail("template bank is truncated");

    entry.feature.resize(pixels);
    for (size_t p = 0; p < pixels; ++p) {
      entry.feature[p] = static_cast<float>(data[offset + p]) / 255.0f;
    }
    offset += pixels;
    normaliseInPlace(entry.feature);

    if (std::find(labels_.begin(), labels_.end(), entry.label) == labels_.end()) {
      labels_.push_back(entry.label);
    }
    templates_.push_back(std::move(entry));
  }

  if (templates_.empty()) return fail("template bank is empty");
  return true;
}

std::vector<float> normaliseGlyph(const GrayImage& binary, const Box& box) {
  if (binary.empty()) return {};

  const int x0 = std::max(0, box.x0);
  const int y0 = std::max(0, box.y0);
  const int x1 = std::min(binary.width, box.x1);
  const int y1 = std::min(binary.height, box.y1);
  if (x1 <= x0 || y1 <= y0) return {};

  // Tight ink box inside the symbol box.
  int ix0 = x1;
  int iy0 = y1;
  int ix1 = x0;
  int iy1 = y0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      if (binary.at(x, y) <= 127) continue;
      ix0 = std::min(ix0, x);
      iy0 = std::min(iy0, y);
      ix1 = std::max(ix1, x + 1);
      iy1 = std::max(iy1, y + 1);
    }
  }
  if (ix1 <= ix0 || iy1 <= iy0) return {};

  const int inkWidth = ix1 - ix0;
  const int inkHeight = iy1 - iy0;
  const double scale =
      static_cast<double>(kGlyphInner) / std::max(inkWidth, inkHeight);
  const int targetWidth = std::max(1, static_cast<int>(std::lround(inkWidth * scale)));
  const int targetHeight = std::max(1, static_cast<int>(std::lround(inkHeight * scale)));
  const int offsetX = (kGlyphGrid - targetWidth) / 2;
  const int offsetY = (kGlyphGrid - targetHeight) / 2;

  std::vector<float> grid(static_cast<size_t>(kGlyphGrid) * kGlyphGrid, 0.0f);
  for (int ty = 0; ty < targetHeight; ++ty) {
    const int sy0 = iy0 + ty * inkHeight / targetHeight;
    const int sy1 = std::max(sy0 + 1, iy0 + (ty + 1) * inkHeight / targetHeight);
    for (int tx = 0; tx < targetWidth; ++tx) {
      const int sx0 = ix0 + tx * inkWidth / targetWidth;
      const int sx1 = std::max(sx0 + 1, ix0 + (tx + 1) * inkWidth / targetWidth);
      int sum = 0;
      int count = 0;
      for (int y = sy0; y < sy1; ++y) {
        for (int x = sx0; x < sx1; ++x) {
          if (binary.at(x, y) > 127) ++sum;
          ++count;
        }
      }
      const int gx = offsetX + tx;
      const int gy = offsetY + ty;
      if (gx < 0 || gy < 0 || gx >= kGlyphGrid || gy >= kGlyphGrid) continue;
      grid[static_cast<size_t>(gy) * kGlyphGrid + gx] =
          static_cast<float>(sum) / std::max(1, count);
    }
  }

  normaliseInPlace(grid);
  return grid;
}

}  // namespace infinit::scan
