#include "camera.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#if defined(INFINIT_SCANNER_VIDEO) && INFINIT_SCANNER_VIDEO
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#if defined(__linux__)
#include <unistd.h>
#endif
#endif

namespace infinit::scan {
namespace {

#if defined(INFINIT_SCANNER_VIDEO) && INFINIT_SCANNER_VIDEO

struct CameraState {
  std::mutex mutex;
  std::thread worker;
  std::atomic<bool> running{false};
  std::vector<std::uint8_t> frame;
  int width = 0;
  int height = 0;
  std::string error;
};

CameraState& state() {
  static CameraState instance;
  return instance;
}

/// The grab thread owns the capture object; the static guard stops it before
/// the process tears the state down, so a detached cv::VideoCapture cannot run
/// past the end of the program.
struct StopOnExit {
  ~StopOnExit() { cameraStop(); }
};

void grabLoop(cv::VideoCapture capture, int maxLongSide) {
  CameraState& s = state();
  auto last = std::chrono::steady_clock::now() - std::chrono::milliseconds(200);
  int failures = 0;
  while (s.running.load(std::memory_order_relaxed)) {
    cv::Mat bgr;
    if (!capture.read(bgr) || bgr.empty()) {
      if (++failures > 40) {
        std::lock_guard<std::mutex> lock(s.mutex);
        s.error = "the camera stopped delivering frames";
        s.running.store(false, std::memory_order_relaxed);
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
      continue;
    }
    failures = 0;
    // Twelve frames a second is plenty for a live preview and costs far less
    // than encoding every sensor frame; the shutter always takes the newest.
    const auto now = std::chrono::steady_clock::now();
    if (now - last < std::chrono::milliseconds(75)) continue;
    last = now;

    cv::Mat scaled = bgr;
    const int longest = std::max(bgr.cols, bgr.rows);
    if (longest > maxLongSide) {
      const double scale = static_cast<double>(maxLongSide) / longest;
      cv::resize(bgr, scaled, cv::Size(), scale, scale, cv::INTER_AREA);
    }
    std::vector<std::uint8_t> jpeg;
    const std::vector<int> params{cv::IMWRITE_JPEG_QUALITY, 88};
    if (!cv::imencode(".jpg", scaled, jpeg, params) || jpeg.empty()) continue;
    {
      std::lock_guard<std::mutex> lock(s.mutex);
      s.frame = std::move(jpeg);
      s.width = scaled.cols;
      s.height = scaled.rows;
      s.error.clear();
    }
  }
}

bool deviceNodeExists(int index) {
#if defined(__linux__)
  const std::string path = "/dev/video" + std::to_string(index);
  return ::access(path.c_str(), F_OK) == 0;
#else
  (void)index;
  return true;
#endif
}

#endif  // INFINIT_SCANNER_VIDEO

}  // namespace

#if defined(INFINIT_SCANNER_VIDEO) && INFINIT_SCANNER_VIDEO

bool cameraSupported() { return true; }

bool cameraStart(int maxLongSide, std::string* error) {
  cameraStop();
  CameraState& s = state();
  {
    std::lock_guard<std::mutex> lock(s.mutex);
    s.error.clear();
    s.frame.clear();
    s.width = 0;
    s.height = 0;
  }

  cv::VideoCapture capture;
  for (int index = 0; index < 8 && !capture.isOpened(); ++index) {
    if (!deviceNodeExists(index)) continue;
    // Pin the platform backend instead of whatever OpenCV probes first: V4L2 on
    // Linux and Media Foundation on Windows are the driver paths the OS ships.
    // A default open is the fallback for an unusual distro.
#if defined(__linux__)
    capture.open(index, cv::CAP_V4L2);
#elif defined(_WIN32)
    capture.open(index, cv::CAP_MSMF);
#endif
    if (!capture.isOpened()) capture.open(index);
  }
  if (!capture.isOpened()) {
    std::lock_guard<std::mutex> lock(s.mutex);
    s.error = "no camera was found";
    if (error != nullptr) *error = s.error;
    return false;
  }
  capture.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
  capture.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
  // One buffered frame keeps the preview close to real time.
  capture.set(cv::CAP_PROP_BUFFERSIZE, 1);

  s.running.store(true, std::memory_order_relaxed);
  s.worker = std::thread(grabLoop, std::move(capture),
                         std::max(320, maxLongSide));
  static StopOnExit guard;
  (void)guard;
  return true;
}

void cameraStop() {
  CameraState& s = state();
  if (!s.running.exchange(false, std::memory_order_relaxed)) {
    if (s.worker.joinable()) s.worker.join();
    return;
  }
  if (s.worker.joinable()) s.worker.join();
}

bool cameraRunning() { return state().running.load(std::memory_order_relaxed); }

int cameraWidth() {
  std::lock_guard<std::mutex> lock(state().mutex);
  return state().width;
}

int cameraHeight() {
  std::lock_guard<std::mutex> lock(state().mutex);
  return state().height;
}

long cameraCopyFrame(std::uint8_t* out, std::size_t capacity) {
  CameraState& s = state();
  if (out == nullptr) return -1;
  std::lock_guard<std::mutex> lock(s.mutex);
  if (s.frame.empty()) return -2;
  if (s.frame.size() > capacity) return -1;
  std::memcpy(out, s.frame.data(), s.frame.size());
  return static_cast<long>(s.frame.size());
}

std::size_t cameraFrameCapacity() {
  CameraState& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  const std::size_t pixels =
      static_cast<std::size_t>(std::max(1, s.width)) *
      static_cast<std::size_t>(std::max(1, s.height));
  // A JPEG of a natural image stays far below three bytes per pixel; the slack
  // covers an unusually detailed page without another allocation later.
  return std::max<std::size_t>(pixels * 3, 1u << 20);
}

std::string cameraError() {
  std::lock_guard<std::mutex> lock(state().mutex);
  return state().error;
}

#else  // no videoio in this build

bool cameraSupported() { return false; }

bool cameraStart(int, std::string* error) {
  if (error != nullptr) *error = "this build has no camera support";
  return false;
}

void cameraStop() {}
bool cameraRunning() { return false; }
int cameraWidth() { return 0; }
int cameraHeight() { return 0; }

long cameraCopyFrame(std::uint8_t*, std::size_t) { return -2; }
std::size_t cameraFrameCapacity() { return 0; }

std::string cameraError() { return "this build has no camera support"; }

#endif

}  // namespace infinit::scan
