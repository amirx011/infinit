// libc++ compatibility shim for the prebuilt OpenCV Android archives.
//
// opencv-mobile 4.14 was built against an older NDK whose libc++ exported
// std::__ndk1::__hash_memory. NDK 29 removed that internal symbol, so linking
// libopencv_core.a fails with an undefined reference. The hash is only used
// inside libc++'s unordered containers; any deterministic hash satisfies the
// contract, so provide one here.
#if defined(__ANDROID__)

#include <cstddef>
#include <cstdint>

namespace std {
inline namespace __ndk1 {

std::size_t __hash_memory(const void* data, std::size_t size) noexcept {
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  std::uint64_t hash = 1469598103934665603ull;
  for (std::size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ull;
  }
  return static_cast<std::size_t>(hash);
}

}  // namespace __ndk1
}  // namespace std

#endif  // __ANDROID__
