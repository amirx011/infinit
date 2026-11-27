// In-app camera feed for Linux and Windows.
//
// Android draws the Camera2 texture from native code (camera_page.dart talks to
// the platform channel there). Desktop has no camera plugin, so the scanner
// library owns an OpenCV grab thread and this shim reads its newest JPEG frame.
// When the build has no videoio, `open` returns null and the page shows the
// normal "camera unavailable" state.
import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import '../native_engine.dart';

typedef _AvailableNative = Int32 Function();
typedef _AvailableDart = int Function();
typedef _StartNative = Int32 Function(Int32 maxLongSide);
typedef _StartDart = int Function(int maxLongSide);
typedef _StopNative = Void Function();
typedef _StopDart = void Function();
typedef _RunningNative = Int32 Function();
typedef _RunningDart = int Function();
typedef _SizeNative = Int32 Function();
typedef _SizeDart = int Function();
typedef _FrameNative = Int32 Function(Pointer<Uint8> out, Int32 capacity);
typedef _FrameDart = int Function(Pointer<Uint8> out, int capacity);
typedef _ErrorNative = Pointer<Utf8> Function();
typedef _ErrorDart = Pointer<Utf8> Function();

/// A camera the app owns, not the phone's camera application.
class DesktopCamera {
  DesktopCamera._(
    this._start,
    this._stop,
    this._running,
    this._width,
    this._height,
    this._frame,
    this._capacity,
    this._error,
  );

  final _StartDart _start;
  final _StopDart _stop;
  final _RunningDart _running;
  final _SizeDart _width;
  final _SizeDart _height;
  final _FrameDart _frame;
  final _SizeDart _capacity;
  final _ErrorDart _error;

  Pointer<Uint8>? _buffer;
  int _bufferSize = 0;

  /// Null on Android, on unsupported builds and when the library is not loaded.
  static DesktopCamera? open() {
    if (Platform.isAndroid) return null;
    late final DynamicLibrary library;
    try {
      library = DynamicLibrary.open(NativeEngine.instance.libraryPath);
    } catch (_) {
      return null;
    }
    try {
      final available = library
          .lookupFunction<_AvailableNative, _AvailableDart>(
            'scanner_camera_available',
          );
      if (available() == 0) return null;
      return DesktopCamera._(
        library.lookupFunction<_StartNative, _StartDart>(
          'scanner_camera_start',
        ),
        library.lookupFunction<_StopNative, _StopDart>('scanner_camera_stop'),
        library.lookupFunction<_RunningNative, _RunningDart>(
          'scanner_camera_running',
        ),
        library.lookupFunction<_SizeNative, _SizeDart>('scanner_camera_width'),
        library.lookupFunction<_SizeNative, _SizeDart>('scanner_camera_height'),
        library.lookupFunction<_FrameNative, _FrameDart>(
          'scanner_camera_frame',
        ),
        library.lookupFunction<_SizeNative, _SizeDart>(
          'scanner_camera_frame_capacity',
        ),
        library.lookupFunction<_ErrorNative, _ErrorDart>(
          'scanner_camera_error',
        ),
      );
    } catch (_) {
      // An older library without the camera symbols: behave like a build
      // without videoio instead of crashing the page.
      return null;
    }
  }

  /// Opens the first working device. The preview is capped at [maxLongSide]
  /// pixels on its long side.
  bool start({int maxLongSide = 1280}) {
    try {
      if (_start(maxLongSide) == 0) return false;
      _resizeBuffer(_capacity());
      return true;
    } catch (_) {
      return false;
    }
  }

  bool get isRunning {
    try {
      return _running() != 0;
    } catch (_) {
      return false;
    }
  }

  int get width => _width();
  int get height => _height();

  /// The newest frame as JPEG bytes, or null before the first frame arrives.
  Uint8List? frame() {
    try {
      var buffer = _buffer;
      if (buffer == null || _bufferSize <= 0) return null;
      var length = _frame(buffer, _bufferSize);
      if (length == -1) {
        // The frame outgrew the buffer allocated at start, which happens when
        // the driver switches to a larger resolution than it first announced.
        final needed = _capacity();
        if (needed > _bufferSize) {
          _resizeBuffer(needed);
          buffer = _buffer;
          if (buffer == null) return null;
          length = _frame(buffer, _bufferSize);
        }
      }
      if (length <= 0) return null;
      return Uint8List.fromList(buffer.asTypedList(length));
    } catch (_) {
      return null;
    }
  }

  String get lastError {
    try {
      return _error().toDartString();
    } catch (_) {
      return '';
    }
  }

  void _resizeBuffer(int size) {
    if (size <= 0 || size == _bufferSize) return;
    final previous = _buffer;
    if (previous != null) malloc.free(previous);
    _buffer = malloc<Uint8>(size);
    _bufferSize = size;
  }

  void stop() {
    try {
      _stop();
    } catch (_) {
      // The native side is already gone; nothing left to release.
    }
  }

  void dispose() {
    stop();
    final buffer = _buffer;
    if (buffer != null) malloc.free(buffer);
    _buffer = null;
    _bufferSize = 0;
  }
}
