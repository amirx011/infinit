// The desktop camera ABI in its closed state. This never opens a device, so it
// runs on a machine with no camera too, and it pins the contract the Dart side
// relies on: a closed feed reports no frame and never claims to be running.
#include <cstdint>
#include <iostream>
#include <string>

#include "camera.hpp"

using namespace infinit::scan;

namespace {

int failures = 0;

void check(const std::string& label, bool ok) {
  std::cout << (ok ? "PASS " : "FAIL ") << label << "\n";
  if (!ok) ++failures;
}

}  // namespace

int main() {
  cameraStop();
  check("closed feed is not running", !cameraRunning());
  check("closed feed has no size", cameraWidth() == 0 && cameraHeight() == 0);

  std::uint8_t buffer[8] = {0};
  check("closed feed reports no frame",
        cameraCopyFrame(buffer, sizeof buffer) == -2);
  check("a null buffer is rejected", cameraCopyFrame(nullptr, 0) == -1);

  // A build without videoio must say so instead of failing silently.
  if (cameraSupported()) {
    check("a supported build reserves a frame buffer",
          cameraFrameCapacity() >= (1u << 20));
  } else {
    check("an unsupported build has no buffer", cameraFrameCapacity() == 0);
    check("an unsupported build explains itself", !cameraError().empty());
    std::string error;
    check("an unsupported build cannot start",
          !cameraStart(640, &error) && !error.empty());
  }

  cameraStop();
  check("stopping twice is harmless", !cameraRunning());

  if (failures == 0) {
    std::cout << "all camera checks passed\n";
    return 0;
  }
  std::cout << failures << " failure(s)\n";
  return 1;
}
