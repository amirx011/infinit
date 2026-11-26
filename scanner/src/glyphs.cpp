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

    entry.raw.resize(pixels);
    for (size_t p = 0; p < pixels; ++p) {
      entry.raw[p] = static_cast<float>(data[offset + p]) / 255.0f;
    }
    offset += pixels;
    entry.feature = entry.raw;
    normaliseInPlace(entry.feature);
    cacheGridFeatures(&entry);

    if (std::find(labels_.begin(), labels_.end(), entry.label) == labels_.end()) {
      labels_.push_back(entry.label);
    }
    templates_.push_back(std::move(entry));
  }

  if (templates_.empty()) return fail("template bank is empty");
  return true;
}

bool rasteriseGlyph(const GrayImage& binary, const Box& box,
                    std::vector<float>* grid, double* aspect) {
  if (grid == nullptr || binary.empty()) return false;
  grid->clear();
  if (aspect != nullptr) *aspect = 1.0;

  const int x0 = std::max(0, box.x0);
  const int y0 = std::max(0, box.y0);
  const int x1 = std::min(binary.width, box.x1);
  const int y1 = std::min(binary.height, box.y1);
  if (x1 <= x0 || y1 <= y0) return false;

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
  if (ix1 <= ix0 || iy1 <= iy0) return false;

  const int inkWidth = ix1 - ix0;
  const int inkHeight = iy1 - iy0;
  if (aspect != nullptr) {
    *aspect = static_cast<double>(inkWidth) / std::max(1, inkHeight);
  }
  const double scale =
      static_cast<double>(kGlyphInner) / std::max(inkWidth, inkHeight);
  const int targetWidth = std::max(1, static_cast<int>(std::lround(inkWidth * scale)));
  const int targetHeight = std::max(1, static_cast<int>(std::lround(inkHeight * scale)));
  const int offsetX = (kGlyphGrid - targetWidth) / 2;
  const int offsetY = (kGlyphGrid - targetHeight) / 2;

  grid->assign(static_cast<size_t>(kGlyphGrid) * kGlyphGrid, 0.0f);
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
      (*grid)[static_cast<size_t>(gy) * kGlyphGrid + gx] =
          static_cast<float>(sum) / std::max(1, count);
    }
  }
  return true;
}

std::vector<float> normaliseGlyph(const GrayImage& binary, const Box& box) {
  std::vector<float> grid;
  if (!rasteriseGlyph(binary, box, &grid, nullptr)) return {};
  normaliseInPlace(grid);
  return grid;
}

double gridAspect(const std::vector<float>& grid) {
  if (grid.size() != static_cast<size_t>(kGlyphGrid) * kGlyphGrid) return 1.0;
  int x0 = kGlyphGrid;
  int y0 = kGlyphGrid;
  int x1 = -1;
  int y1 = -1;
  for (int y = 0; y < kGlyphGrid; ++y) {
    for (int x = 0; x < kGlyphGrid; ++x) {
      if (grid[static_cast<size_t>(y) * kGlyphGrid + x] < 0.5f) continue;
      x0 = std::min(x0, x);
      y0 = std::min(y0, y);
      x1 = std::max(x1, x);
      y1 = std::max(y1, y);
    }
  }
  if (x1 < x0 || y1 < y0) return 1.0;
  return static_cast<double>(x1 - x0 + 1) / std::max(1, y1 - y0 + 1);
}

std::vector<float> gridProfiles(const std::vector<float>& grid) {
  std::vector<float> out;
  if (grid.size() != static_cast<size_t>(kGlyphGrid) * kGlyphGrid) return out;
  out.assign(2 * kGlyphGrid, 0.0f);
  for (int y = 0; y < kGlyphGrid; ++y) {
    for (int x = 0; x < kGlyphGrid; ++x) {
      const float v = grid[static_cast<size_t>(y) * kGlyphGrid + x];
      out[static_cast<size_t>(x)] += v;
      out[static_cast<size_t>(kGlyphGrid + y)] += v;
    }
  }
  double columns = 0.0;
  double rows = 0.0;
  for (int i = 0; i < kGlyphGrid; ++i) columns += out[static_cast<size_t>(i)];
  for (int i = 0; i < kGlyphGrid; ++i) rows += out[static_cast<size_t>(kGlyphGrid + i)];
  const double columnNorm = columns > 1e-9 ? columns : 1.0;
  const double rowNorm = rows > 1e-9 ? rows : 1.0;
  for (int i = 0; i < kGlyphGrid; ++i) {
    out[static_cast<size_t>(i)] = static_cast<float>(out[static_cast<size_t>(i)] / columnNorm);
    out[static_cast<size_t>(kGlyphGrid + i)] =
        static_cast<float>(out[static_cast<size_t>(kGlyphGrid + i)] / rowNorm);
  }
  return out;
}

std::vector<float> gridCrossings(const std::vector<float>& grid) {
  std::vector<float> out(4, 0.0f);
  if (grid.size() != static_cast<size_t>(kGlyphGrid) * kGlyphGrid) return out;
  const auto ink = [&grid](int x, int y) {
    return grid[static_cast<size_t>(y) * kGlyphGrid + x] >= 0.5f;
  };
  const auto runs = [](const std::vector<bool>& line) {
    int count = 0;
    bool inside = false;
    for (bool on : line) {
      if (on && !inside) ++count;
      inside = on;
    }
    return count;
  };
  constexpr int mid = kGlyphGrid / 2;
  std::vector<bool> row(kGlyphGrid);
  std::vector<bool> column(kGlyphGrid);
  std::vector<bool> diagonal(kGlyphGrid);
  std::vector<bool> anti(kGlyphGrid);
  for (int i = 0; i < kGlyphGrid; ++i) {
    row[static_cast<size_t>(i)] = ink(i, mid);
    column[static_cast<size_t>(i)] = ink(mid, i);
    diagonal[static_cast<size_t>(i)] = ink(i, i);
    anti[static_cast<size_t>(i)] = ink(kGlyphGrid - 1 - i, i);
  }
  out[0] = static_cast<float>(runs(row));
  out[1] = static_cast<float>(runs(column));
  out[2] = static_cast<float>(runs(diagonal));
  out[3] = static_cast<float>(runs(anti));
  return out;
}

std::vector<float> gridHuMoments(const std::vector<float>& grid) {
  std::vector<float> out(7, 0.0f);
  if (grid.size() != static_cast<size_t>(kGlyphGrid) * kGlyphGrid) return out;
  double mass = 0.0;
  double mx = 0.0;
  double my = 0.0;
  for (int y = 0; y < kGlyphGrid; ++y) {
    for (int x = 0; x < kGlyphGrid; ++x) {
      const double w = std::max(0.0, static_cast<double>(
                                        grid[static_cast<size_t>(y) * kGlyphGrid + x]));
      mass += w;
      mx += w * x;
      my += w * y;
    }
  }
  if (mass <= 1e-9) return out;
  mx /= mass;
  my /= mass;
  double mu[4][4] = {};
  for (int y = 0; y < kGlyphGrid; ++y) {
    for (int x = 0; x < kGlyphGrid; ++x) {
      const double w = std::max(0.0, static_cast<double>(
                                        grid[static_cast<size_t>(y) * kGlyphGrid + x]));
      if (w <= 0.0) continue;
      const double dx = x - mx;
      const double dy = y - my;
      double px = 1.0;
      for (int p = 0; p < 4; ++p) {
        double py = 1.0;
        for (int q = 0; q < 4 - p; ++q) {
          mu[p][q] += w * px * py;
          py *= dy;
        }
        px *= dx;
      }
    }
  }
  const auto normalised = [&mu, mass](int p, int q) {
    return mu[p][q] / std::pow(mass, 1.0 + (p + q) / 2.0);
  };
  const double n20 = normalised(2, 0);
  const double n02 = normalised(0, 2);
  const double n11 = normalised(1, 1);
  const double n30 = normalised(3, 0);
  const double n03 = normalised(0, 3);
  const double n21 = normalised(2, 1);
  const double n12 = normalised(1, 2);
  const double h[7] = {
      n20 + n02,
      (n20 - n02) * (n20 - n02) + 4.0 * n11 * n11,
      (n30 - 3.0 * n12) * (n30 - 3.0 * n12) +
          3.0 * (n21 + n03) * (n21 + n03),
      (n30 + n12) * (n30 + n12) + (n21 + n03) * (n21 + n03),
      (n30 - 3.0 * n12) * (n30 + n12) *
              ((n30 + n12) * (n30 + n12) -
               3.0 * (n21 + n03) * (n21 + n03)) +
          3.0 * (n21 + n03) * (n30 + n12) *
              (3.0 * (n30 + n12) * (n30 + n12) -
               (n21 + n03) * (n21 + n03)),
      (n20 - n02) * ((n30 + n12) * (n30 + n12) -
                     (n21 + n03) * (n21 + n03)) +
          4.0 * n11 * (n30 + n12) * (n21 + n03),
      3.0 * (n21 + n03) * (n30 + n12) *
              ((n30 + n12) * (n30 + n12) -
               3.0 * (n21 + n03) * (n21 + n03)) +
          (3.0 * n12 - n30) * (n30 + n12) *
              (3.0 * (n30 + n12) * (n30 + n12) -
               (n21 + n03) * (n21 + n03)),
  };
  for (int i = 0; i < 7; ++i) {
    // Log-compress: the invariants span many orders of magnitude, so their raw
    // difference would be dominated by the largest one.
    const double value = std::log10(std::abs(h[i]) + 1e-12);
    out[static_cast<size_t>(i)] =
        static_cast<float>(std::max(-30.0, std::min(30.0, value)));
  }
  return out;
}

void cacheGridFeatures(GlyphTemplate* entry) {
  if (entry == nullptr) return;
  if (entry->raw.size() != static_cast<size_t>(kGlyphGrid) * kGlyphGrid) return;
  entry->holes = gridHoles(entry->raw);
  entry->aspect = gridAspect(entry->raw);
  entry->zoning = gridZoning(entry->raw, 4);
  entry->profile = gridProfiles(entry->raw);
  entry->crossings = gridCrossings(entry->raw);
  entry->hu = gridHuMoments(entry->raw);
}

std::vector<float> gridZoning(const std::vector<float>& grid, int blocks) {
  if (blocks < 1) return {};
  std::vector<float> out(static_cast<size_t>(blocks) * blocks, 0.0f);
  if (grid.size() != static_cast<size_t>(kGlyphGrid) * kGlyphGrid) return out;
  const int cell = kGlyphGrid / blocks;
  for (int by = 0; by < blocks; ++by) {
    for (int bx = 0; bx < blocks; ++bx) {
      float sum = 0.0f;
      int count = 0;
      for (int y = by * cell; y < (by + 1) * cell; ++y) {
        for (int x = bx * cell; x < (bx + 1) * cell; ++x) {
          sum += grid[static_cast<size_t>(y) * kGlyphGrid + x];
          ++count;
        }
      }
      out[static_cast<size_t>(by) * blocks + bx] =
          count > 0 ? sum / count : 0.0f;
    }
  }
  return out;
}

int gridHoles(const std::vector<float>& grid) {
  if (grid.size() != static_cast<size_t>(kGlyphGrid) * kGlyphGrid) return 0;
  std::vector<uint8_t> visited(grid.size(), 0);
  std::vector<int> stack;
  const auto isInk = [&grid](size_t index) { return grid[index] >= 0.5f; };
  const auto isBorder = [](int x, int y) {
    return x == 0 || y == 0 || x == kGlyphGrid - 1 || y == kGlyphGrid - 1;
  };
  // Flood the background from the border; whatever background remains is a
  // hole.
  for (int y = 0; y < kGlyphGrid; ++y) {
    for (int x = 0; x < kGlyphGrid; ++x) {
      if (!isBorder(x, y)) continue;
      const size_t start = static_cast<size_t>(y) * kGlyphGrid + x;
      if (visited[start] || isInk(start)) continue;
      visited[start] = 1;
      stack.push_back(static_cast<int>(start));
      while (!stack.empty()) {
        const int index = stack.back();
        stack.pop_back();
        const int cx = index % kGlyphGrid;
        const int cy = index / kGlyphGrid;
        const int nx[4] = {cx - 1, cx + 1, cx, cx};
        const int ny[4] = {cy, cy, cy - 1, cy + 1};
        for (int k = 0; k < 4; ++k) {
          if (nx[k] < 0 || ny[k] < 0 || nx[k] >= kGlyphGrid ||
              ny[k] >= kGlyphGrid) {
            continue;
          }
          const size_t next = static_cast<size_t>(ny[k]) * kGlyphGrid + nx[k];
          if (visited[next] || isInk(next)) continue;
          visited[next] = 1;
          stack.push_back(static_cast<int>(next));
        }
      }
    }
  }

  int holes = 0;
  for (int y = 0; y < kGlyphGrid; ++y) {
    for (int x = 0; x < kGlyphGrid; ++x) {
      const size_t start = static_cast<size_t>(y) * kGlyphGrid + x;
      if (visited[start] || isInk(start)) continue;
      ++holes;
      visited[start] = 1;
      stack.push_back(static_cast<int>(start));
      while (!stack.empty()) {
        const int index = stack.back();
        stack.pop_back();
        const int cx = index % kGlyphGrid;
        const int cy = index / kGlyphGrid;
        const int nx[4] = {cx - 1, cx + 1, cx, cx};
        const int ny[4] = {cy, cy, cy - 1, cy + 1};
        for (int k = 0; k < 4; ++k) {
          if (nx[k] < 0 || ny[k] < 0 || nx[k] >= kGlyphGrid ||
              ny[k] >= kGlyphGrid) {
            continue;
          }
          const size_t next = static_cast<size_t>(ny[k]) * kGlyphGrid + nx[k];
          if (visited[next] || isInk(next)) continue;
          visited[next] = 1;
          stack.push_back(static_cast<int>(next));
        }
      }
    }
  }
  return holes;
}

}  // namespace infinit::scan
