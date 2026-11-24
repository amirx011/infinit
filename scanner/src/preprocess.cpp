#include "preprocess.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "segment.hpp"

namespace infinit::scan {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct Integral {
  int width = 0;
  int height = 0;
  std::vector<uint64_t> sum;
  std::vector<uint64_t> sq;

  Integral(const GrayImage& image, bool withSquares) {
    width = image.width + 1;
    height = image.height + 1;
    sum.assign(static_cast<size_t>(width) * height, 0);
    if (withSquares) sq.assign(sum.size(), 0);
    for (int y = 0; y < image.height; ++y) {
      uint64_t row = 0;
      uint64_t rowSq = 0;
      for (int x = 0; x < image.width; ++x) {
        const uint64_t v = image.at(x, y);
        row += v;
        rowSq += v * v;
        sum[static_cast<size_t>(y + 1) * width + x + 1] =
            sum[static_cast<size_t>(y) * width + x + 1] + row;
        if (withSquares) {
          sq[static_cast<size_t>(y + 1) * width + x + 1] =
              sq[static_cast<size_t>(y) * width + x + 1] + rowSq;
        }
      }
    }
  }

  uint64_t area(int x0, int y0, int x1, int y1, bool squares = false) const {
    const auto& table = squares ? sq : sum;
    const size_t a = static_cast<size_t>(y0) * width + x0;
    const size_t b = static_cast<size_t>(y0) * width + x1;
    const size_t c = static_cast<size_t>(y1) * width + x0;
    const size_t d = static_cast<size_t>(y1) * width + x1;
    return table[d] - table[b] - table[c] + table[a];
  }
};

GrayImage boxMean(const GrayImage& image, int radius) {
  Integral integral(image, false);
  GrayImage out(image.width, image.height);
  for (int y = 0; y < image.height; ++y) {
    const int y0 = std::max(0, y - radius);
    const int y1 = std::min(image.height, y + radius + 1);
    for (int x = 0; x < image.width; ++x) {
      const int x0 = std::max(0, x - radius);
      const int x1 = std::min(image.width, x + radius + 1);
      const uint64_t total = integral.area(x0, y0, x1, y1);
      const int count = (x1 - x0) * (y1 - y0);
      out.at(x, y) = static_cast<uint8_t>(total / std::max(1, count));
    }
  }
  return out;
}

GrayImage flattenIllumination(const GrayImage& image, int radius) {
  const GrayImage background = boxMean(image, radius);
  GrayImage out(image.width, image.height);
  for (size_t i = 0; i < image.pixels.size(); ++i) {
    const int bg = std::max(1, static_cast<int>(background.pixels[i]));
    const int value = static_cast<int>(image.pixels[i]) * 255 / bg;
    out.pixels[i] = static_cast<uint8_t>(std::min(255, value));
  }
  return out;
}

GrayImage otsuBinary(const GrayImage& image, int threshold) {
  GrayImage out(image.width, image.height);
  for (size_t i = 0; i < image.pixels.size(); ++i) {
    out.pixels[i] = image.pixels[i] < threshold ? 255 : 0;
  }
  return out;
}

GrayImage sauvolaBinary(const GrayImage& image, int window, double k) {
  const Integral integral(image, true);
  GrayImage out(image.width, image.height);
  const int half = window / 2;
  for (int y = 0; y < image.height; ++y) {
    const int y0 = std::max(0, y - half);
    const int y1 = std::min(image.height, y + half + 1);
    for (int x = 0; x < image.width; ++x) {
      const int x0 = std::max(0, x - half);
      const int x1 = std::min(image.width, x + half + 1);
      const int count = (x1 - x0) * (y1 - y0);
      const double mean = static_cast<double>(integral.area(x0, y0, x1, y1)) / count;
      const double meanSq =
          static_cast<double>(integral.area(x0, y0, x1, y1, true)) / count;
      const double variance = std::max(0.0, meanSq - mean * mean);
      const double deviation = std::sqrt(variance);
      const double threshold = mean * (1.0 + k * (deviation / 128.0 - 1.0));
      out.at(x, y) = image.at(x, y) < threshold ? 255 : 0;
    }
  }
  return out;
}

double inkRatio(const GrayImage& binary) {
  size_t ink = 0;
  for (uint8_t v : binary.pixels) {
    if (v > 127) ++ink;
  }
  const size_t total = binary.pixels.size();
  return total == 0 ? 0.0 : static_cast<double>(ink) / total;
}

/// Cheap quality score used only to pick a binarization candidate. The real
/// decision belongs to the structure stage's total cost; until that exists,
/// this rejects obviously broken thresholds.
double binaryScore(const GrayImage& binary) {
  const double ratio = inkRatio(binary);
  if (ratio < 0.0005) return -100.0;
  if (ratio > 0.45) return -100.0;
  double score = 0.0;
  if (ratio >= 0.004 && ratio <= 0.20) score += 10.0;
  score -= std::abs(ratio - 0.05) * 40.0;
  return score;
}

double rowVariance(const GrayImage& image) {
  std::vector<double> rows(image.height, 0.0);
  for (int y = 0; y < image.height; ++y) {
    double sum = 0.0;
    for (int x = 0; x < image.width; ++x) sum += image.at(x, y);
    rows[y] = sum;
  }
  double mean = 0.0;
  for (double v : rows) mean += v;
  mean /= std::max<size_t>(1, rows.size());
  double variance = 0.0;
  for (double v : rows) variance += (v - mean) * (v - mean);
  return variance / std::max<size_t>(1, rows.size());
}

struct SkewEstimate {
  double degrees = 0.0;
  /// Row variance at `degrees`.
  double score = 0.0;
  /// score / score at 0 degrees. 1.0 means "no rotation explains the ink".
  double ratio = 1.0;
};

/// Maximises the row projection variance. A vertical stroke (an integral sign,
/// a slash) also raises it, so the caller compares this against the estimate
/// with tall symbols removed.
SkewEstimate bestSkew(const GrayImage& image, double maxDegrees) {
  SkewEstimate best;
  best.score = rowVariance(image);
  const double atZero = best.score;
  for (double angle = -maxDegrees; angle <= maxDegrees + 0.001; angle += 0.5) {
    const GrayImage rotated = rotate(image, angle, 0);
    const double score = rowVariance(rotated);
    if (score > best.score) {
      best.score = score;
      best.degrees = angle;
    }
  }
  best.ratio = atZero > 0.0 ? best.score / atZero : 1.0;
  return best;
}

/// The same probe with symbols taller than 1.5x the median blanked out. Big
/// operators and radicals are slanted by design and would otherwise be read as
/// page rotation.
GrayImage withoutTallSymbols(const GrayImage& probe) {
  GrayImage filtered = probe;
  const std::vector<Component> components = connectedComponents(probe, 4);
  if (components.size() < 3) return filtered;

  std::vector<int> heights;
  heights.reserve(components.size());
  for (const Component& component : components) {
    if (component.box.height() > 0) heights.push_back(component.box.height());
  }
  if (heights.empty()) return filtered;
  const size_t mid = heights.size() / 2;
  std::nth_element(heights.begin(), heights.begin() + mid, heights.end());
  const int median = heights[mid];

  for (const Component& component : components) {
    if (component.box.height() <= 1.5 * median) continue;
    for (int y = component.box.y0; y < component.box.y1; ++y) {
      for (int x = component.box.x0; x < component.box.x1; ++x) {
        filtered.at(x, y) = 0;
      }
    }
  }
  return filtered;
}

}  // namespace

int otsuThreshold(const GrayImage& image) {
  double histogram[256] = {0.0};
  const double total = static_cast<double>(image.pixels.size());
  for (uint8_t v : image.pixels) histogram[v] += 1.0;

  double sum = 0.0;
  for (int i = 0; i < 256; ++i) sum += i * histogram[i];

  double sumB = 0.0;
  double weightB = 0.0;
  double best = -1.0;
  int threshold = 127;
  for (int t = 0; t < 256; ++t) {
    weightB += histogram[t];
    if (weightB == 0.0) continue;
    const double weightF = total - weightB;
    if (weightF == 0.0) break;
    sumB += t * histogram[t];
    const double meanB = sumB / weightB;
    const double meanF = (sum - sumB) / weightF;
    const double between = weightB * weightF * (meanB - meanF) * (meanB - meanF);
    if (between > best) {
      best = between;
      threshold = t;
    }
  }
  return threshold;
}

GrayImage rotate(const GrayImage& image, double degrees, uint8_t fill) {
  if (std::abs(degrees) < 0.01) return image;
  const double radians = degrees * kPi / 180.0;
  const double c = std::cos(radians);
  const double s = std::sin(radians);
  const double cx = (image.width - 1) * 0.5;
  const double cy = (image.height - 1) * 0.5;
  GrayImage out(image.width, image.height, fill);

  for (int y = 0; y < out.height; ++y) {
    for (int x = 0; x < out.width; ++x) {
      const double dx = x - cx;
      const double dy = y - cy;
      const double sx = c * dx + s * dy + cx;
      const double sy = -s * dx + c * dy + cy;
      if (sx < 0.0 || sy < 0.0 || sx > image.width - 1 || sy > image.height - 1) {
        continue;
      }
      const int x0 = static_cast<int>(sx);
      const int y0 = static_cast<int>(sy);
      const int x1 = std::min(image.width - 1, x0 + 1);
      const int y1 = std::min(image.height - 1, y0 + 1);
      const double fx = sx - x0;
      const double fy = sy - y0;
      const double top = image.at(x0, y0) * (1.0 - fx) + image.at(x1, y0) * fx;
      const double bottom = image.at(x0, y1) * (1.0 - fx) + image.at(x1, y1) * fx;
      out.at(x, y) = static_cast<uint8_t>(std::lround(top * (1.0 - fy) + bottom * fy));
    }
  }
  return out;
}

PreprocessResult preprocess(const GrayImage& input,
                            const PreprocessOptions& options) {
  PreprocessResult result;
  result.gray = downscaleToLongSide(input, options.maxLongSide);
  if (result.gray.empty()) return result;

  double mean = 0.0;
  for (uint8_t v : result.gray.pixels) mean += v;
  mean /= std::max<size_t>(1, result.gray.pixels.size());
  if (mean < 100.0) {
    result.inverted = true;
    for (uint8_t& v : result.gray.pixels) v = static_cast<uint8_t>(255 - v);
  }

  const int longSide = std::max(result.gray.width, result.gray.height);
  const int backgroundRadius = std::max(8, longSide / options.backgroundRadiusDiv);
  result.normalized = flattenIllumination(result.gray, backgroundRadius);

  result.otsuThreshold = otsuThreshold(result.normalized);
  result.binaryOtsu = otsuBinary(result.normalized, result.otsuThreshold);

  const int window = std::max(15, longSide / options.sauvolaWindowDiv) | 1;
  result.binarySauvola = sauvolaBinary(result.normalized, window, options.sauvolaK);

  const double otsuScore = binaryScore(result.binaryOtsu);
  const double sauvolaScore = binaryScore(result.binarySauvola);
  result.chosen = sauvolaScore > otsuScore + 1.0 ? "sauvola" : "otsu";
  result.binary = result.chosen == "sauvola" ? result.binarySauvola : result.binaryOtsu;

  GrayImage probe = downscaleToLongSide(result.binary, 420);
  const SkewEstimate full = bestSkew(probe, options.maxDeskewDegrees);
  const SkewEstimate stable =
      bestSkew(withoutTallSymbols(probe), options.maxDeskewDegrees);
  // Trust the tall-symbol-free estimate: a slanted integral or radical raises
  // the full-probe score at the wrong angle. Only keep the full estimate when
  // both agree, so real page rotation still wins over a vertical stroke.
  double bestAngle = stable.ratio >= 1.03 ? stable.degrees : 0.0;
  if (bestAngle != 0.0 && std::abs(full.degrees - stable.degrees) <= 1.0 &&
      full.score > stable.score) {
    bestAngle = full.degrees;
  }
  if (std::abs(bestAngle) >= 0.5) {
    result.skewDegrees = bestAngle;
    result.gray = rotate(result.gray, bestAngle, 255);
    result.normalized = rotate(result.normalized, bestAngle, 255);
    result.otsuThreshold = otsuThreshold(result.normalized);
    result.binaryOtsu = otsuBinary(result.normalized, result.otsuThreshold);
    result.binarySauvola =
        sauvolaBinary(result.normalized, window, options.sauvolaK);
    const double againOtsu = binaryScore(result.binaryOtsu);
    const double againSauvola = binaryScore(result.binarySauvola);
    result.chosen = againSauvola > againOtsu + 1.0 ? "sauvola" : "otsu";
    result.binary =
        result.chosen == "sauvola" ? result.binarySauvola : result.binaryOtsu;
  }

  return result;
}

}  // namespace infinit::scan
