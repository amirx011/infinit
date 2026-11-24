// Cross-checks the hand-written 8-connected labelling against
// cv::connectedComponentsWithStats on pseudo-random blobs. The scanner does not
// depend on OpenCV; this test only runs when OpenCV is installed.
#include <iostream>
#include <random>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "image.hpp"
#include "segment.hpp"

using namespace infinit::scan;

namespace {

int failures = 0;

void check(const std::string& label, bool ok, const std::string& detail = "") {
  std::cout << (ok ? "PASS " : "FAIL ") << label;
  if (!ok) {
    ++failures;
    if (!detail.empty()) std::cout << "   (" << detail << ")";
  }
  std::cout << "\n";
}

GrayImage randomBlobs(int width, int height, unsigned seed) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> xDist(0, width - 1);
  std::uniform_int_distribution<int> yDist(0, height - 1);
  std::uniform_int_distribution<int> sizeDist(1, 9);
  GrayImage image(width, height, 0);
  for (int blob = 0; blob < 40; ++blob) {
    const int cx = xDist(rng);
    const int cy = yDist(rng);
    const int w = sizeDist(rng);
    const int h = sizeDist(rng);
    for (int y = cy; y < std::min(height, cy + h); ++y) {
      for (int x = cx; x < std::min(width, cx + w); ++x) {
        image.at(x, y) = 255;
      }
    }
  }
  return image;
}

}  // namespace

int main() {
  for (unsigned seed = 1; seed <= 5; ++seed) {
    const int width = 120;
    const int height = 90;
    const GrayImage ours = randomBlobs(width, height, seed);

    cv::Mat mat(height, width, CV_8UC1);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        mat.at<uchar>(y, x) = ours.at(x, y);
      }
    }

    cv::Mat labels;
    cv::Mat stats;
    cv::Mat centroids;
    const int count = cv::connectedComponentsWithStats(mat, labels, stats, centroids, 8);

    const auto components = connectedComponents(ours, 6);
    int cvKept = 0;
    for (int label = 1; label < count; ++label) {
      if (stats.at<int>(label, cv::CC_STAT_AREA) >= 6) ++cvKept;
    }
    check("seed " + std::to_string(seed) + ": component count",
          static_cast<int>(components.size()) == cvKept,
          "ours " + std::to_string(components.size()) + ", cv " +
              std::to_string(cvKept));

    // Every kept component must match one cv component's bounding box.
    bool boxesMatch = true;
    for (const auto& component : components) {
      bool found = false;
      for (int label = 1; label < count && !found; ++label) {
        if (stats.at<int>(label, cv::CC_STAT_AREA) < 6) continue;
        const int x = stats.at<int>(label, cv::CC_STAT_LEFT);
        const int y = stats.at<int>(label, cv::CC_STAT_TOP);
        const int w = stats.at<int>(label, cv::CC_STAT_WIDTH);
        const int h = stats.at<int>(label, cv::CC_STAT_HEIGHT);
        found = component.box.x0 == x && component.box.y0 == y &&
                component.box.x1 == x + w && component.box.y1 == y + h;
      }
      if (!found) boxesMatch = false;
    }
    check("seed " + std::to_string(seed) + ": bounding boxes", boxesMatch);
  }

  if (failures > 0) {
    std::cout << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "open cv cross-check passed\n";
  return 0;
}
