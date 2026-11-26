#include "flutter_window.h"

#include <flutter/standard_method_codec.h>
#include <shellapi.h>
#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>

#include "flutter/generated_plugin_registrant.h"
#include "utils.h"

namespace {

// Copies the clipboard image into a temporary BMP, which the Dart decoder can
// read without a new image dependency. Returns an empty string on failure.
std::string ClipboardImagePath() {
  if (!::OpenClipboard(nullptr)) return std::string();
  std::string path;

  if (::IsClipboardFormatAvailable(CF_HDROP)) {
    HANDLE data = ::GetClipboardData(CF_HDROP);
    if (data != nullptr) {
      HDROP drop = static_cast<HDROP>(data);
      const UINT length = ::DragQueryFileW(drop, 0, nullptr, 0);
      if (length > 0) {
        std::wstring file(length + 1, L'\0');
        if (::DragQueryFileW(drop, 0, file.data(), length + 1) > 0) {
          file.resize(length);
          path = Utf8FromUtf16(file.c_str());
        }
      }
    }
  }

  if (path.empty()) {
    HANDLE data = ::GetClipboardData(CF_DIB);
    if (data != nullptr) {
      const auto* header =
          static_cast<const BITMAPINFOHEADER*>(::GlobalLock(data));
      if (header != nullptr) {
        const DWORD dibSize = static_cast<DWORD>(::GlobalSize(data));
        const DWORD headerSize = header->biSize;
        DWORD paletteEntries = header->biClrUsed;
        if (paletteEntries == 0 && header->biBitCount <= 8) {
          paletteEntries = 1u << header->biBitCount;
        }
        const DWORD offset = sizeof(BITMAPFILEHEADER) + headerSize +
                             paletteEntries * sizeof(RGBQUAD);
        BITMAPFILEHEADER fileHeader{};
        fileHeader.bfType = 0x4D42;  // "BM"
        fileHeader.bfOffBits = offset;
        fileHeader.bfSize = offset + dibSize;

        wchar_t directory[MAX_PATH] = {};
        if (::GetTempPathW(MAX_PATH, directory) > 0) {
          std::wstring file = std::wstring(directory) + L"infinit-paste-" +
                              std::to_wstring(::GetTickCount64()) + L".bmp";
          HANDLE out = ::CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                     nullptr);
          if (out != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            const BOOL ok =
                ::WriteFile(out, &fileHeader, sizeof(fileHeader), &written,
                            nullptr) &&
                ::WriteFile(out, header, dibSize, &written, nullptr);
            ::CloseHandle(out);
            if (ok) path = Utf8FromUtf16(file.c_str());
          }
        }
        ::GlobalUnlock(data);
      }
    }
  }

  ::CloseClipboard();
  return path;
}

}  // namespace

FlutterWindow::FlutterWindow(const flutter::DartProject& project)
    : project_(project) {}

FlutterWindow::~FlutterWindow() {}

bool FlutterWindow::OnCreate() {
  if (!Win32Window::OnCreate()) {
    return false;
  }

  RECT frame = GetClientArea();

  // The size here must match the window dimensions to avoid unnecessary surface
  // creation / destruction in the startup path.
  flutter_controller_ = std::make_unique<flutter::FlutterViewController>(
      frame.right - frame.left, frame.bottom - frame.top, project_);
  // Ensure that basic setup of the controller was successful.
  if (!flutter_controller_->engine() || !flutter_controller_->view()) {
    return false;
  }
  RegisterPlugins(flutter_controller_->engine());
  SetChildContent(flutter_controller_->view()->GetNativeWindow());

  // Desktop import: a dropped file and a clipboard image reach Dart as paths.
  ::DragAcceptFiles(GetHandle(), TRUE);
  import_channel_ =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          flutter_controller_->engine()->messenger(), "infinit/import",
          &flutter::StandardMethodCodec::GetInstance());
  import_channel_->SetMethodCallHandler(
      [](const flutter::MethodCall<flutter::EncodableValue>& call,
         std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>>
             result) {
        if (call.method_name() == "paste") {
          const std::string path = ClipboardImagePath();
          if (path.empty()) {
            result->Success();
          } else {
            result->Success(flutter::EncodableValue(path));
          }
          return;
        }
        result->NotImplemented();
      });

  flutter_controller_->engine()->SetNextFrameCallback([&]() {
    this->Show();
  });

  // Flutter can complete the first frame before the "show window" callback is
  // registered. The following call ensures a frame is pending to ensure the
  // window is shown. It is a no-op if the first frame hasn't completed yet.
  flutter_controller_->ForceRedraw();

  return true;
}

void FlutterWindow::OnDestroy() {
  if (import_channel_) {
    import_channel_->SetMethodCallHandler(nullptr);
    import_channel_ = nullptr;
  }
  ::DragAcceptFiles(GetHandle(), FALSE);
  if (flutter_controller_) {
    flutter_controller_ = nullptr;
  }

  Win32Window::OnDestroy();
}

LRESULT
FlutterWindow::MessageHandler(HWND hwnd, UINT const message,
                              WPARAM const wparam,
                              LPARAM const lparam) noexcept {
  // Give Flutter, including plugins, an opportunity to handle window messages.
  if (flutter_controller_) {
    std::optional<LRESULT> result =
        flutter_controller_->HandleTopLevelWindowProc(hwnd, message, wparam,
                                                      lparam);
    if (result) {
      return *result;
    }
  }

  switch (message) {
    case WM_FONTCHANGE:
      flutter_controller_->engine()->ReloadSystemFonts();
      break;
    case WM_DROPFILES: {
      HDROP drop = reinterpret_cast<HDROP>(wparam);
      const UINT length = ::DragQueryFileW(drop, 0, nullptr, 0);
      if (length > 0) {
        std::wstring file(length + 1, L'\0');
        if (::DragQueryFileW(drop, 0, file.data(), length + 1) > 0) {
          file.resize(length);
          if (import_channel_) {
            import_channel_->InvokeMethod(
                "drop", std::make_unique<flutter::EncodableValue>(
                            Utf8FromUtf16(file.c_str())));
          }
        }
      }
      ::DragFinish(drop);
      return 0;
    }
  }

  return Win32Window::MessageHandler(hwnd, message, wparam, lparam);
}
