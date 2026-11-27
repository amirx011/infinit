// In-app desktop camera feed.
//
// The Android preview lives in Camera2 (CameraPreview.kt) and never touches
// this file. Linux and Windows get the same in-app experience through OpenCV's
// videoio: a grab thread keeps the newest encoded frame, the Dart side draws it
// and the shutter reuses it. On a build without videoio every function below is
// a stub, so the library still loads and the app can say "camera unavailable".
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace infinit::scan {

/// True when this build was compiled with videoio and imgcodecs.
bool cameraSupported();

/// Opens the first working device (0..7) and starts the grab thread. The long
/// side of an encoded frame never exceeds `maxLongSide`. Returns false and sets
/// `error` when no device opens or the backend is missing.
bool cameraStart(int maxLongSide, std::string* error);

/// Stops the grab thread and releases the device. Safe to call repeatedly.
void cameraStop();

bool cameraRunning();

/// Size of the newest frame, or 0 before the first one arrives.
int cameraWidth();
int cameraHeight();

/// Copies the newest encoded frame (JPEG) into `out`. Returns the byte count,
/// 0 when no frame is ready, -1 when `capacity` is too small, and -2 when no
/// camera has ever been started.
long cameraCopyFrame(std::uint8_t* out, std::size_t capacity);

/// Worst-case encoded frame size for the current device; 0 when no camera is
/// running. The Dart side allocates its buffer with this once per session.
std::size_t cameraFrameCapacity();

/// Human-readable reason the last start or grab failed, empty when healthy.
std::string cameraError();

}  // namespace infinit::scan
