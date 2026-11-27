#include "preprocess.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "cv_bridge.hpp"
#include "segment.hpp"

namespace infinit::scan {
namespace {

constexpr double kPi = 3.14159265358979323846;

GrayImage fromMat(const cv::Mat& source) { return copyOf(source); }

/// Divides the image by a heavily blurred copy of itself, which removes
/// shadows and uneven lighting without losing thin strokes.
GrayImage flattenIllumination(const GrayImage& image, int radius) {
  cv::Mat gray = view(image);
  cv::Mat background;
  const int k = std::max(3, radius * 2 + 1);
  cv::blur(gray, background, cv::Size(k, k), cv::Point(-1, -1),
           cv::BORDER_REPLICATE);
  cv::Mat normalized;
  cv::divide(gray, background, normalized, 255.0);
  return fromMat(normalized);
}

GrayImage otsuBinary(const GrayImage& image, int threshold) {
  cv::Mat gray = view(image);
  cv::Mat out;
  cv::threshold(gray, out, threshold, 255, cv::THRESH_BINARY_INV);
  return fromMat(out);
}

/// Sauvola thresholding. The local mean and standard deviation come from
/// OpenCV's box filter, which uses integral images internally for big windows.
GrayImage sauvolaBinary(const GrayImage& image, int window, double k) {
  if (image.empty()) return image;
  cv::Mat gray = view(image);
  cv::Mat f;
  gray.convertTo(f, CV_32F);
  const int win = std::max(3, window | 1);
  cv::Mat mean;
  cv::Mat meanSq;
  cv::boxFilter(f, mean, CV_32F, cv::Size(win, win), cv::Point(-1, -1), true,
                cv::BORDER_REPLICATE);
  cv::Mat squared = f.mul(f);
  cv::boxFilter(squared, meanSq, CV_32F, cv::Size(win, win), cv::Point(-1, -1),
                true, cv::BORDER_REPLICATE);
  cv::Mat variance = meanSq - mean.mul(mean);
  cv::max(variance, 0.0, variance);
  cv::Mat deviation;
  cv::sqrt(variance, deviation);

  cv::Mat out(image.height, image.width, CV_8UC1);
  for (int y = 0; y < image.height; ++y) {
    const float* fp = f.ptr<float>(y);
    const float* mp = mean.ptr<float>(y);
    const float* dp = deviation.ptr<float>(y);
    uint8_t* op = out.ptr<uint8_t>(y);
    for (int x = 0; x < image.width; ++x) {
      const double threshold =
          mp[x] * (1.0 + k * (dp[x] / 128.0 - 1.0));
      op[x] = fp[x] < threshold ? 255 : 0;
    }
  }
  return fromMat(out);
}

GrayImage adaptiveBinary(const GrayImage& image, int block, double c) {
  cv::Mat gray = view(image);
  cv::Mat out;
  const int size = std::max(3, block | 1);
  cv::adaptiveThreshold(gray, out, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                        cv::THRESH_BINARY_INV, size, c);
  return fromMat(out);
}

double inkRatio(const GrayImage& binary) {
  if (binary.empty()) return 0.0;
  const double ink = cv::countNonZero(view(binary));
  return ink / static_cast<double>(binary.pixels.size());
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
  cv::Mat rows;
  cv::reduce(view(image), rows, 1, cv::REDUCE_SUM, CV_64F);
  cv::Scalar mean;
  cv::Scalar deviation;
  cv::meanStdDev(rows, mean, deviation);
  return deviation[0] * deviation[0];
}

struct SkewEstimate {
  double degrees = 0.0;
  double score = 0.0;
  double ratio = 1.0;
};

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

  cv::Mat mask = view(filtered);
  for (const Component& component : components) {
    if (component.box.height() <= 1.5 * median) continue;
    cv::rectangle(mask,
                  cv::Point(component.box.x0, component.box.y0),
                  cv::Point(component.box.x1, component.box.y1),
                  cv::Scalar(0), cv::FILLED);
  }
  return filtered;
}

/// Skew from the baseline of the glyphs themselves. Each component votes with
/// its bottom edge; a RANSAC line finds the dominant baseline even when
/// superscripts, descenders and big operators are present. Returns the angle
/// to apply (the correction), in degrees.
double baselineSkew(const GrayImage& image, double maxDegrees) {
  const std::vector<Component> components = connectedComponents(image, 4);
  if (components.size() < 3) return 0.0;

  std::vector<double> heights;
  heights.reserve(components.size());
  for (const Component& component : components) {
    if (component.box.height() > 1) heights.push_back(component.box.height());
  }
  if (heights.size() < 3) return 0.0;
  const size_t mid = heights.size() / 2;
  std::nth_element(heights.begin(), heights.begin() + mid, heights.end());
  const double medianHeight = std::max(4.0, heights[mid]);
  const double tolerance = std::max(2.0, 0.14 * medianHeight);

  struct Point {
    double x = 0.0;
    double y = 0.0;
    int weight = 1;
  };
  std::vector<Point> points;
  points.reserve(components.size());
  for (const Component& component : components) {
    if (component.box.height() < 0.35 * medianHeight) continue;
    points.push_back(Point{component.centroidX(),
                           static_cast<double>(component.box.y1),
                           std::max(1, component.pixels)});
  }
  if (points.size() < 3) return 0.0;

  double bestSlope = 0.0;
  double bestScore = 0.0;
  const double maxSlope = std::tan(maxDegrees * kPi / 180.0) * 1.5;
  for (size_t i = 0; i < points.size(); ++i) {
    for (size_t j = i + 1; j < points.size(); ++j) {
      const double dx = points[j].x - points[i].x;
      if (std::abs(dx) < 0.20 * image.width) continue;
      const double slope = (points[j].y - points[i].y) / dx;
      if (std::abs(slope) > maxSlope) continue;
      double score = 0.0;
      for (const Point& point : points) {
        const double expected = points[i].y + slope * (point.x - points[i].x);
        if (std::abs(point.y - expected) <= tolerance) {
          score += point.weight;
        }
      }
      if (score > bestScore) {
        bestScore = score;
        bestSlope = slope;
      }
    }
  }
  // rotate(+θ) tilts the content counter-clockwise, so the correction for a
  // baseline whose y grows with x is the negative slope.
  const double angle = std::atan(bestSlope) * 180.0 / kPi;
  return std::abs(angle) <= maxDegrees ? angle : 0.0;
}

/// Skew from the dominant near-horizontal edges. Hough sees the actual
/// horizontal strokes (equals signs, fraction bars, the base of "1") instead
/// of the bounding shape of the whole expression, so superscripts and tall
/// operators do not drag it around. Returns the correction angle.
double houghSkew(const GrayImage& image, double maxDegrees) {
  if (image.empty()) return 0.0;
  cv::Mat edges;
  cv::Canny(view(image), edges, 30, 90);
  std::vector<cv::Vec4i> lines;
  cv::HoughLinesP(edges, lines, 1.0, CV_PI / 180.0, 12,
                  std::max(10, image.width / 16), 8);
  if (std::getenv("SCANNER_DEBUG_SKEW") != nullptr) {
    std::fprintf(stderr, "hough: %zu lines in %dx%d\n", lines.size(),
                 image.width, image.height);
  }
  if (lines.size() < 3) return 0.0;

  struct Vote {
    double angle = 0.0;
    double weight = 0.0;
    bool vertical = false;
  };
  std::vector<Vote> votes;
  votes.reserve(lines.size());
  for (const cv::Vec4i& line : lines) {
    const double dx = line[2] - line[0];
    const double dy = line[3] - line[1];
    const double length = std::hypot(dx, dy);
    if (length < 0.05 * image.width) continue;
    double angle = std::atan2(dy, dx) * 180.0 / kPi;
    if (angle > 90.0) angle -= 180.0;
    if (angle < -90.0) angle += 180.0;
    const bool vertical = std::abs(angle) >= 60.0;
    if (vertical) {
      // The correction is the deviation from a true vertical.
      if (std::abs(std::abs(angle) - 90.0) > maxDegrees * 1.5) continue;
    } else if (std::abs(angle) > maxDegrees * 1.5) {
      continue;
    }
    votes.push_back(Vote{angle, length, vertical});
  }
  if (votes.size() < 3) return 0.0;

  constexpr double kBin = 0.5;
  auto peakOf = [&votes, kBin](bool vertical, double* peak, double* weight) {
    std::vector<std::pair<int, double>> bins;
    for (const Vote& vote : votes) {
      if (vote.vertical != vertical) continue;
      const double value = vertical
          ? (vote.angle > 0.0 ? vote.angle - 90.0 : vote.angle + 90.0)
          : vote.angle;
      const int index = static_cast<int>(std::lround(value / kBin));
      bool found = false;
      for (auto& entry : bins) {
        if (entry.first == index) {
          entry.second += vote.weight;
          found = true;
          break;
        }
      }
      if (!found) bins.emplace_back(index, vote.weight);
    }
    if (bins.empty()) return false;
    const auto best = std::max_element(
        bins.begin(), bins.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    *peak = best->first * kBin;
    *weight = best->second;
    return true;
  };

  double horizontalPeak = 0.0;
  double horizontalWeight = 0.0;
  double verticalPeak = 0.0;
  double verticalWeight = 0.0;
  const bool hasHorizontal =
      peakOf(false, &horizontalPeak, &horizontalWeight);
  const bool hasVertical = peakOf(true, &verticalPeak, &verticalWeight);
  if (std::getenv("SCANNER_DEBUG_SKEW") != nullptr) {
    std::fprintf(stderr,
                 "hough peaks: horizontal=%.2f/%.1f vertical=%.2f/%.1f\n",
                 horizontalPeak, horizontalWeight, verticalPeak,
                 verticalWeight);
  }

  // Horizontal strokes are the strongest signal; tall vertical strokes
  // (integral signs, "1", "l") are the fallback when the expression has no
  // long horizontal ink.
  double correction = 0.0;
  if (hasHorizontal && horizontalWeight >= 0.15 * image.width) {
    correction = horizontalPeak;
  } else if (hasVertical && verticalWeight >= 0.15 * image.height) {
    correction = verticalPeak;
  } else {
    return 0.0;
  }
  if (std::abs(correction) > maxDegrees * 1.5) return 0.0;
  return std::clamp(correction, -maxDegrees, maxDegrees);
}

/// minAreaRect of the ink, normalised to [-45, 45]. It is used as a second
/// opinion: when it disagrees with the projection profile the profile wins,
/// because a tall integral sign can dominate the rectangle.
double minAreaSkew(const GrayImage& image, double maxDegrees) {
  std::vector<cv::Point> points;
  cv::findNonZero(view(image), points);
  if (points.size() < 20) return 0.0;
  const cv::RotatedRect rect = cv::minAreaRect(points);
  double angle = rect.angle;
  if (angle >= 45.0) angle -= 90.0;
  if (angle < -45.0) angle += 90.0;
  return std::abs(angle) <= maxDegrees ? angle : 0.0;
}

/// Removes very long, thin horizontal lines (notebook rules). Fraction bars
/// and minus signs are much shorter than 85% of the page, so they survive.
bool removeRuledLines(GrayImage& binary) {
  if (binary.empty()) return false;
  const int length = std::max(24, static_cast<int>(binary.width * 0.85));
  cv::Mat lines;
  cv::morphologyEx(view(binary), lines, cv::MORPH_OPEN,
                   cv::getStructuringElement(cv::MORPH_RECT,
                                             cv::Size(length, 1)));
  if (cv::countNonZero(lines) < length) return false;
  cv::Mat ink = view(binary);
  cv::Mat removed;
  cv::bitwise_and(ink, lines, removed);
  ink.setTo(0, lines > 0);
  // A glyph that crosses the rule loses the pixels the rule occupied, which
  // cuts it in two halves the segmenter then treats as separate symbols. Bridge
  // each removed pixel that has ink both a few rows above and below it, so the
  // rule disappears but the glyph stays one piece. The window is wider than the
  // rule is tall because the stroke of a "3" or an integral leans: the ink a
  // few rows below the cut can sit a couple of pixels to the side.
  auto hasInk = [&binary](int x, int y) {
    return x >= 0 && y >= 0 && x < binary.width && y < binary.height &&
           binary.at(x, y) > 127;
  };
  for (int pass = 0; pass < 4; ++pass) {
    for (int y = 0; y < binary.height; ++y) {
      for (int x = 0; x < binary.width; ++x) {
        if (binary.at(x, y) != 0 || removed.at<uint8_t>(y, x) == 0) continue;
        bool above = false;
        bool below = false;
        for (int dx = -2; dx <= 2 && !(above && below); ++dx) {
          for (int dy = 1; dy <= 4; ++dy) {
            if (!above && hasInk(x + dx, y - dy)) above = true;
            if (!below && hasInk(x + dx, y + dy)) below = true;
          }
        }
        if (above && below) binary.at(x, y) = 255;
      }
    }
  }
  return true;
}

/// True when the page carries a light horizontal rule across most of its
/// width. Such a rule is the strongest level cue a page has, and a skew
/// estimate taken from slanted italic strokes must not overrule it.
bool hasLightRuleRows(const GrayImage& gray) {
  if (gray.empty()) return false;
  int rows = 0;
  for (int y = 1; y + 1 < gray.height; ++y) {
    int light = 0;
    int dark = 0;
    for (int x = 0; x < gray.width; ++x) {
      const int value = gray.at(x, y);
      if (value >= 150 && value <= 245) ++light;
      else if (value < 120) ++dark;
    }
    if (light * 100 >= gray.width * 45 && dark * 4 <= gray.width) ++rows;
  }
  return rows >= 2;
}

/// A light notebook rule drawn over the ink erases a one- or two-pixel slice of
/// every glyph it crosses, and the threshold then has nothing to keep there. The
/// rules themselves are background in the binary, so `removeRuledLines` never
/// sees them; find the rows in the grey image and close the gap in the binary
/// where ink sits on both sides.
void bridgeLightRules(const GrayImage& gray, GrayImage& binary) {
  if (gray.empty() || binary.empty()) return;
  const int width = std::min(gray.width, binary.width);
  const int height = std::min(gray.height, binary.height);
  auto hasInk = [&binary](int x, int y) {
    return x >= 0 && y >= 0 && x < binary.width && y < binary.height &&
           binary.at(x, y) > 127;
  };
  for (int y = 1; y + 1 < height; ++y) {
    int light = 0;
    int dark = 0;
    for (int x = 0; x < width; ++x) {
      const int value = gray.at(x, y);
      if (value >= 150 && value <= 245) ++light;
      else if (value < 120) ++dark;
    }
    if (light * 100 < width * 45 || dark * 4 > width) continue;
    for (int x = 0; x < width; ++x) {
      if (binary.at(x, y) != 0) continue;
      bool above = false;
      bool below = false;
      // A slanted stroke shifts sideways as it crosses the rule, so the ink
      // just above and just below can sit a few columns apart.
      for (int dx = -2; dx <= 2 && !(above && below); ++dx) {
        for (int dy = 1; dy <= 4; ++dy) {
          if (!above && hasInk(x + dx, y - dy)) above = true;
          if (!below && hasInk(x + dx, y + dy)) below = true;
        }
      }
      if (above && below) binary.at(x, y) = 255;
    }
  }
}

/// Drops components that touch the image border and cover a large area. A
/// tightly cropped expression touches the border too, but it is small.
void removeBorderArtifacts(GrayImage& binary) {
  if (binary.empty()) return;
  cv::Mat labels;
  cv::Mat stats;
  cv::Mat centroids;
  const int count = cv::connectedComponentsWithStats(
      view(binary), labels, stats, centroids, 8, CV_32S);
  const double limit = 0.10 * binary.width * binary.height;
  cv::Mat ink = view(binary);
  for (int label = 1; label < count; ++label) {
    const int x = stats.at<int>(label, cv::CC_STAT_LEFT);
    const int y = stats.at<int>(label, cv::CC_STAT_TOP);
    const int w = stats.at<int>(label, cv::CC_STAT_WIDTH);
    const int h = stats.at<int>(label, cv::CC_STAT_HEIGHT);
    const int area = stats.at<int>(label, cv::CC_STAT_AREA);
    const bool touches = x <= 0 || y <= 0 || x + w >= binary.width ||
                         y + h >= binary.height;
    if (touches && area >= limit) {
      ink.setTo(0, labels == label);
    }
  }
}

/// Screenshots and rendered pages have a flat background (a huge fraction of
/// pixels equal the mode) and few distinct levels; a photo of paper has noise,
/// gradients and hundreds of levels even when it looks white.
bool looksCleanDigital(const GrayImage& image) {
  const GrayImage probe = downscaleToLongSide(image, 320);
  if (probe.empty()) return false;
  int histogram[256] = {0};
  for (uint8_t value : probe.pixels) ++histogram[value];
  const int total = probe.width * probe.height;
  int modeCount = 0;
  int used = 0;
  for (int value = 0; value < 256; ++value) {
    modeCount = std::max(modeCount, histogram[value]);
    if (histogram[value] > total / 2000) ++used;
  }
  if (total <= 0 || modeCount * 100 < total * 65 || used > 64) return false;
  // Ruled paper has a flat background but is not a screen render: the rule rows
  // are ink that only the photo pipeline (Sauvola + rule removal) can suppress.
  // Treating it as "clean" shipped the rules to the segmenter.
  int background = 255;
  for (int value = 0; value < 256; ++value) {
    if (histogram[value] == modeCount) {
      background = value;
      break;
    }
  }
  // A ruled page's rule is often lighter than the text, so "dark" has to be
  // measured against the page background, not a fixed threshold.
  const int ruleCutoff = background - 10;
  for (int y = 0; y < probe.height; ++y) {
    int dark = 0;
    for (int x = 0; x < probe.width; ++x) {
      if (probe.at(x, y) < ruleCutoff) ++dark;
    }
    if (dark * 100 >= probe.width * 55) return false;
  }
  return true;
}

}  // namespace

int otsuThreshold(const GrayImage& image) {
  if (image.empty()) return 127;
  cv::Mat out;
  const double threshold =
      cv::threshold(view(image), out, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
  return static_cast<int>(std::lround(threshold));
}

GrayImage rotate(const GrayImage& image, double degrees, uint8_t fill) {
  if (std::abs(degrees) < 0.01 || image.empty()) return image;
  const double radians = degrees * kPi / 180.0;
  const double c = std::cos(radians);
  const double s = std::sin(radians);
  const double cx = (image.width - 1) * 0.5;
  const double cy = (image.height - 1) * 0.5;
  // Same inverse mapping as the previous hand-written bilinear rotation, so
  // callers keep their sign convention.
  const cv::Mat matrix = (cv::Mat_<double>(2, 3) <<
      c, s, (1.0 - c) * cx - s * cy,
      -s, c, s * cx + (1.0 - c) * cy);
  cv::Mat out;
  cv::warpAffine(view(image), out, matrix, view(image).size(),
                 cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(fill));
  return fromMat(out);
}

PreprocessResult preprocess(const GrayImage& input,
                            const PreprocessOptions& options) {
  PreprocessResult result;
  result.gray = downscaleToLongSide(input, options.maxLongSide);
  if (result.gray.empty()) return result;

  cv::Scalar mean;
  cv::Scalar deviation;
  cv::meanStdDev(view(result.gray), mean, deviation);
  if (mean[0] < 100.0) {
    result.inverted = true;
    for (uint8_t& v : result.gray.pixels) {
      v = static_cast<uint8_t>(255 - v);
    }
  }

  const bool cleanDigital =
      options.allowCleanDigital && looksCleanDigital(result.gray);
  result.profile = cleanDigital ? "clean" : "photo";

  const int longSide = std::max(result.gray.width, result.gray.height);
  const int backgroundRadius =
      std::max(8, longSide / options.backgroundRadiusDiv);
  result.normalized = cleanDigital
                          ? result.gray
                          : flattenIllumination(result.gray, backgroundRadius);

  if (!cleanDigital && options.applyClahe) {
    cv::Mat normalized = view(result.normalized);
    cv::Scalar nMean;
    cv::Scalar nDeviation;
    cv::meanStdDev(normalized, nMean, nDeviation);
    if (nDeviation[0] < 45.0) {
      cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
      cv::Mat enhanced;
      clahe->apply(normalized, enhanced);
      result.normalized = fromMat(enhanced);
    }
  }

  // A light blur keeps primes, dots and thin fraction bars alive. A clean
  // digital image has no sensor noise to suppress, and blurring would soften
  // the 1 px strokes of a rendered fraction bar.
  if (!cleanDigital) {
    cv::Mat normalized = view(result.normalized);
    cv::GaussianBlur(normalized, normalized, cv::Size(3, 3), 0.0);
  }

  result.otsuThreshold = otsuThreshold(result.normalized);
  result.binaryOtsu = otsuBinary(result.normalized, result.otsuThreshold);

  const int window = std::max(15, longSide / options.sauvolaWindowDiv) | 1;
  result.binarySauvola =
      sauvolaBinary(result.normalized, window, options.sauvolaK);

  const int block =
      std::max(15, longSide / options.adaptiveBlockDiv) | 1;
  result.binaryAdaptive =
      adaptiveBinary(result.normalized, block, options.adaptiveC);

  const double otsuScore = binaryScore(result.binaryOtsu);
  const double sauvolaScore = binaryScore(result.binarySauvola);
  const double adaptiveScore = binaryScore(result.binaryAdaptive);
  if (cleanDigital) {
    result.chosen = "otsu";
    result.binary = result.binaryOtsu;
  } else if (sauvolaScore > otsuScore + 1.0 && sauvolaScore >= adaptiveScore) {
    result.chosen = "sauvola";
    result.binary = result.binarySauvola;
  } else if (adaptiveScore > otsuScore + 1.0 &&
             adaptiveScore > sauvolaScore) {
    result.chosen = "adaptive";
    result.binary = result.binaryAdaptive;
  } else {
    result.chosen = "otsu";
    result.binary = result.binaryOtsu;
  }

  {
    // Deskew runs for the clean-digital profile too: a screenshot or a rendered
    // PDF can be tilted (a rotated page, a crooked export), and skipping the
    // estimate made a 3° render unreadable. The two-estimator agreement below
    // is what keeps this safe, not the profile.
    GrayImage probe = downscaleToLongSide(result.binary, 420);
    const double baseline = baselineSkew(probe, options.maxDeskewDegrees);
    const double hough = houghSkew(probe, options.maxDeskewDegrees);
    const SkewEstimate full = bestSkew(probe, options.maxDeskewDegrees);
    const SkewEstimate stable =
        bestSkew(withoutTallSymbols(probe), options.maxDeskewDegrees);
    const double projection = stable.ratio >= 1.03 ? stable.degrees : 0.0;
    const double rectangleAngle = minAreaSkew(probe, options.maxDeskewDegrees);
    result.baselineDegrees = baseline;
    result.projectionDegrees = projection;
    result.rectangleDegrees = rectangleAngle;
    result.houghDegrees = hough;

    // A single line of math has no page edges and contains tall operators,
    // superscripts and bounds, so no single estimator is trustworthy alone.
    // Hough sees the horizontal strokes directly and is the strongest signal;
    // the glyph baseline and the projection profile confirm it. Without
    // agreement the page stays as captured.
    const bool baselineUsable = std::abs(baseline) >= 0.5;
    const bool rectangleUsable = std::abs(rectangleAngle) >= 0.5;
    const bool projectionUsable = std::abs(projection) >= 0.5;
    const bool baselineRectangleAgree =
        baselineUsable && rectangleUsable &&
        std::abs(baseline - rectangleAngle) <= 2.0;
    double bestAngle = 0.0;
    if (std::abs(hough) >= 0.5) {
      if (baselineUsable && std::abs(hough - baseline) <= 2.0) {
        bestAngle = 0.5 * (hough + baseline);
      } else if (rectangleUsable &&
                 std::abs(hough - rectangleAngle) <= 2.0) {
        bestAngle = 0.5 * (hough + rectangleAngle);
      } else if (baselineRectangleAgree) {
        // Hough locked onto a single slanted stroke (a radical bar, a prime).
        bestAngle = 0.5 * (baseline + rectangleAngle);
      }
    } else if (baselineRectangleAgree) {
      bestAngle = 0.5 * (baseline + rectangleAngle);
    } else if (baselineUsable && projectionUsable &&
               std::abs(baseline - projection) <= 1.2) {
      bestAngle = 0.5 * (baseline + projection);
    }
    // A rendered page is usually already upright, and rotating by half a
    // degree only smears the glyphs. Clean-digital pages therefore rotate only
    // for a real tilt; a photo keeps the 0.5 degree floor.
    // Notebook rules are drawn horizontal, so if the page still shows them and
    // the Hough estimator did not see a tilt, the italic lean of the glyphs is
    // what the baseline/rectangle estimators measured. Rotating by that angle
    // would tilt an upright page and break symbols like "=" and "inf".
    if (std::abs(bestAngle) >= 4.0 && std::abs(hough) < 0.5 &&
        hasLightRuleRows(result.normalized)) {
      bestAngle = 0.0;
    }
    const double floor = cleanDigital ? 1.5 : 0.5;
    if (std::abs(bestAngle) < floor) bestAngle = 0.0;

    if (std::abs(bestAngle) >= 0.5) {
      result.skewDegrees = bestAngle;
      result.gray = rotate(result.gray, bestAngle, 255);
      result.normalized = rotate(result.normalized, bestAngle, 255);
      result.otsuThreshold = otsuThreshold(result.normalized);
      result.binaryOtsu = otsuBinary(result.normalized, result.otsuThreshold);
      result.binarySauvola =
          sauvolaBinary(result.normalized, window, options.sauvolaK);
      result.binaryAdaptive =
          adaptiveBinary(result.normalized, block, options.adaptiveC);

      const double againOtsu = binaryScore(result.binaryOtsu);
      const double againSauvola = binaryScore(result.binarySauvola);
      const double againAdaptive = binaryScore(result.binaryAdaptive);
      if (againSauvola > againOtsu + 1.0 && againSauvola >= againAdaptive) {
        result.chosen = "sauvola";
        result.binary = result.binarySauvola;
      } else if (againAdaptive > againOtsu + 1.0 &&
                 againAdaptive > againSauvola) {
        result.chosen = "adaptive";
        result.binary = result.binaryAdaptive;
      } else {
        result.chosen = "otsu";
        result.binary = result.binaryOtsu;
      }
    }
  }

  if (options.removeRuledLines) {
    result.ruledLinesRemoved = removeRuledLines(result.binary);
    bridgeLightRules(result.normalized, result.binary);
  }
  removeBorderArtifacts(result.binary);

  return result;
}

}  // namespace infinit::scan
