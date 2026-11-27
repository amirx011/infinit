// The in-app camera. Android draws a Camera2 texture owned by native code;
// Linux and Windows poll JPEG frames from the scanner library's OpenCV grab
// thread. Either way the shutter reads the expression and asks before anything
// is inserted, so a wrong reading never lands in a field unseen.
import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../l10n.dart';
import '../scanner_engine.dart';
import 'desktop_camera.dart';
import 'scan_confirm.dart';
import 'scan_flow.dart';

class CameraScanPage extends StatefulWidget {
  const CameraScanPage({super.key, required this.l10n, this.useDesktopFeed});

  final L10n l10n;

  /// Forces the frame source in tests. Production leaves it null and the page
  /// picks OpenCV frames on Linux/Windows and the Camera2 texture elsewhere.
  final bool? useDesktopFeed;

  @override
  State<CameraScanPage> createState() => _CameraScanPageState();
}

class _CameraScanPageState extends State<CameraScanPage>
    with WidgetsBindingObserver {
  static const _channel = MethodChannel('ir.infinit/camera');

  int? _texture;
  int _rotation = 0;
  int _width = 0;
  int _height = 0;
  bool _torch = false;
  bool _busy = false;
  bool _detecting = false;
  String? _error;
  double _zoom = 1;
  double _maxZoom = 1;
  double _gestureZoom = 1;
  double _sentZoom = 1;
  Offset? _focusRing;
  Timer? _ringTimer;

  /// Linux and Windows run the OpenCV feed from the scanner library; Android
  /// draws the Camera2 texture through [_channel].
  DesktopCamera? _desktop;

  bool get _isDesktop =>
      widget.useDesktopFeed ?? (Platform.isLinux || Platform.isWindows);

  L10n get l => widget.l10n;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _start();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.paused) {
      // Release the camera when the app goes to the background; Android may
      // otherwise keep the sensor open for the whole task.
      _channel.invokeMethod<void>('previewStop').catchError((_) {});
      _desktop?.stop();
      if (mounted) setState(() => _texture = null);
    } else if (state == AppLifecycleState.resumed &&
        _texture == null &&
        _error == null) {
      _start();
    }
  }

  @override
  void dispose() {
    _ringTimer?.cancel();
    WidgetsBinding.instance.removeObserver(this);
    // Best effort: the controller keeps no Dart state, so a late native stop
    // is harmless.
    _channel.invokeMethod<void>('previewStop').catchError((_) {});
    _desktop?.dispose();
    super.dispose();
  }

  Future<void> _start() async {
    setState(() {
      _error = null;
      _busy = false;
    });
    if (_isDesktop) {
      // A retry, or the resume after a pause, replaces the old feed; release
      // its buffer before opening the device again.
      _desktop?.dispose();
      _desktop = null;
      final camera = DesktopCamera.open();
      if (camera == null || !camera.start()) {
        camera?.dispose();
        if (!mounted) return;
        setState(() => _error = l.t('scan.camera.unavailable'));
        return;
      }
      if (!mounted) {
        camera.dispose();
        return;
      }
      setState(() {
        _desktop = camera;
        _texture = null;
      });
      return;
    }
    try {
      final result = await _channel.invokeMapMethod<String, dynamic>(
        'previewStart',
      );
      if (!mounted) return;
      if (result == null) {
        setState(() => _error = l.t('scan.camera.unavailable'));
        return;
      }
      setState(() {
        _texture = (result['textureId'] as num?)?.toInt();
        _rotation = (result['rotation'] as num?)?.toInt() ?? 0;
        _width = (result['width'] as num?)?.toInt() ?? 0;
        _height = (result['height'] as num?)?.toInt() ?? 0;
        _torch = result['torch'] == true;
        _zoom = (result['zoom'] as num?)?.toDouble() ?? 1;
        _sentZoom = _zoom;
        _maxZoom = (result['maxZoom'] as num?)?.toDouble() ?? 1;
      });
    } on PlatformException catch (error) {
      if (!mounted) return;
      setState(() {
        _error = error.code == 'permission'
            ? l.t('scan.camera.permission')
            : l.t('scan.camera.unavailable');
      });
    } on MissingPluginException {
      if (!mounted) return;
      setState(() => _error = l.t('scan.camera.unavailable'));
    }
  }

  Future<void> _capture() async {
    if (_busy) return;
    HapticFeedback.mediumImpact();
    setState(() {
      _busy = true;
      _detecting = true;
    });
    Uint8List? bytes;
    try {
      if (_isDesktop) {
        // The grab thread delivers a frame every ~75 ms; give it a moment on
        // the very first tap instead of failing with an empty buffer.
        bytes = _desktop?.frame();
        for (var i = 0; i < 20 && bytes == null; i++) {
          await Future<void>.delayed(const Duration(milliseconds: 60));
          bytes = _desktop?.frame();
        }
      } else {
        final path = await _channel.invokeMethod<String>('previewCapture');
        if (path != null) {
          final file = File(path);
          bytes = await file.readAsBytes();
          try {
            await file.delete();
          } catch (_) {
            // The cache directory is app-private; the OS reclaims it.
          }
        }
      }
    } on PlatformException catch (error) {
      if (!mounted) return;
      setState(() {
        _busy = false;
        _detecting = false;
      });
      _snack(
        error.code == 'permission'
            ? l.t('scan.camera.permission')
            : l.t('scan.camera.unavailable'),
      );
      return;
    }
    if (!mounted) return;
    if (bytes == null || bytes.isEmpty) {
      setState(() {
        _busy = false;
        _detecting = false;
      });
      _snack(l.t('scan.camera.unavailable'));
      return;
    }
    await _detect(bytes);
  }

  /// Reads the captured frame and asks the user whether to insert it. Nothing
  /// reaches the field until the confirmation card says so.
  Future<void> _detect(Uint8List bytes) async {
    ScanResult? result;
    try {
      final scanner = await ScannerEngine.load();
      result = await scanner.read(bytes);
    } catch (_) {
      // Native recognition is optional for this path: a failure sends the user
      // back to the shutter instead of leaving a stuck spinner.
    }
    if (!mounted) return;
    setState(() {
      _busy = false;
      _detecting = false;
    });
    final reading = result;
    if (reading == null) {
      _snack(l.t('scan.confirm.failed'));
      return;
    }
    while (true) {
      if (!mounted) return;
      final action = await showScanConfirm(
        context,
        l10n: l,
        result: reading,
        image: bytes,
      );
      if (!mounted) return;
      if (action == null || action == ScanConfirmAction.again) return;
      if (action == ScanConfirmAction.insert) {
        Navigator.of(context).pop(
          ScanOutcome(
            result: reading,
            image: bytes,
            text: readingText(reading),
          ),
        );
        return;
      }
      // Review: the full crop and per-symbol correction screen. Cancelling it
      // returns to the card, so the reading is never thrown away by accident.
      final outcome = await Navigator.of(context).push<ScanOutcome>(
        MaterialPageRoute(
          builder: (_) => ScanFlowPage(l10n: l, bytes: bytes),
        ),
      );
      if (!mounted) return;
      // "Retake" inside the review screen means the camera should stay open,
      // so only a confirmed or manually typed outcome leaves the page.
      if (outcome != null && !outcome.retake) {
        Navigator.of(context).pop(outcome);
        return;
      }
      if (outcome != null) return;
    }
  }

  void _snack(String message) {
    if (!mounted) return;
    ScaffoldMessenger.of(context)
        .showSnackBar(SnackBar(content: Text(message)));
  }

  /// Pinch zoom is a sensor crop on the native side. Sending every frame would
  /// flood the channel, so the last value is only pushed when it moved enough.
  Future<void> _applyZoom(double value) async {
    final next = value.clamp(1.0, _maxZoom);
    if ((next - _sentZoom).abs() < 0.02 && next < _maxZoom) return;
    _sentZoom = next;
    try {
      final applied = await _channel.invokeMethod<double>('previewZoom', {
        'value': next,
      });
      if (!mounted) return;
      setState(() => _zoom = applied ?? next);
    } catch (_) {
      // A camera without digital zoom keeps the raw preview.
    }
  }

  Future<void> _focusAt(Offset normalized) async {
    setState(() => _focusRing = normalized);
    _ringTimer?.cancel();
    _ringTimer = Timer(const Duration(milliseconds: 900), () {
      if (mounted) setState(() => _focusRing = null);
    });
    HapticFeedback.selectionClick();
    try {
      await _channel.invokeMethod<bool>('previewFocus', {
        'x': normalized.dx,
        'y': normalized.dy,
      });
    } catch (_) {
      // Focus is a convenience; the continuous mode still runs.
    }
  }

  Future<void> _toggleTorch() async {
    try {
      final value = await _channel.invokeMethod<bool>('previewTorch', {
        'on': !_torch,
      });
      if (!mounted) return;
      setState(() => _torch = value ?? !_torch);
    } catch (_) {
      // A camera without a flash simply ignores this.
    }
  }

  @override
  Widget build(BuildContext context) {
    final texture = _texture;
    final desktop = _desktop;
    final swap = _rotation == 90 || _rotation == 270;
    final aspect = swap
        ? (_height > 0 && _width > 0 ? _height / _width : 0.75)
        : (_width > 0 && _height > 0 ? _width / _height : 1.333);
    final ready = _isDesktop ? desktop != null : texture != null;
    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        title: Text(l.t('scan.camera.title')),
        backgroundColor: Colors.black,
        foregroundColor: Colors.white,
        actions: [
          if (!_isDesktop)
            IconButton(
              tooltip: l.t('scan.camera.flash'),
              onPressed: texture == null ? null : _toggleTorch,
              icon: Icon(
                _torch ? Icons.flash_on_rounded : Icons.flash_off_rounded,
              ),
            ),
        ],
      ),
      body: SafeArea(
        child: Column(
          children: [
            Expanded(
              child: Center(
                child: _error != null
                    ? _ErrorView(l10n: l, message: _error!, onRetry: _start)
                    : _isDesktop
                    ? (desktop == null
                          ? const CircularProgressIndicator()
                          : Stack(
                              fit: StackFit.expand,
                              children: [
                                _DesktopPreview(camera: desktop),
                                const _FrameGuide(),
                                _CameraHint(l10n: l),
                                if (_detecting) const _DetectingOverlay(),
                              ],
                            ))
                    : texture == null
                    ? const CircularProgressIndicator()
                    : AspectRatio(
                        aspectRatio: aspect,
                        child: LayoutBuilder(
                          builder: (context, constraints) {
                            final size = constraints.biggest;
                            return GestureDetector(
                              behavior: HitTestBehavior.opaque,
                              onTapUp: (details) {
                                final point = details.localPosition;
                                if (size.width <= 0 || size.height <= 0) return;
                                _focusAt(
                                  Offset(
                                    (point.dx / size.width).clamp(0.0, 1.0),
                                    (point.dy / size.height).clamp(0.0, 1.0),
                                  ),
                                );
                              },
                              onScaleStart: (_) => _gestureZoom = _zoom,
                              onScaleUpdate: (details) {
                                if (details.pointerCount < 2) return;
                                final next = (_gestureZoom * details.scale)
                                    .clamp(1.0, _maxZoom);
                                if ((next - _zoom).abs() > 0.01) {
                                  setState(() => _zoom = next);
                                }
                                _applyZoom(next);
                              },
                              child: Stack(
                                fit: StackFit.expand,
                                children: [
                                  RotatedBox(
                                    quarterTurns: _rotation ~/ 90,
                                    child: Texture(textureId: texture),
                                  ),
                                  const _FrameGuide(),
                                  if (_focusRing != null)
                                    Positioned(
                                      left: _focusRing!.dx * size.width - 26,
                                      top: _focusRing!.dy * size.height - 26,
                                      child: const _FocusRing(),
                                    ),
                                  if (_zoom > 1.02)
                                    Positioned(
                                      right: 12,
                                      bottom: 12,
                                      child: Container(
                                        decoration: BoxDecoration(
                                          color: Colors.black54,
                                          borderRadius: BorderRadius.circular(
                                            999,
                                          ),
                                        ),
                                        padding: const EdgeInsets.symmetric(
                                          horizontal: 10,
                                          vertical: 4,
                                        ),
                                        child: Text(
                                          '${_zoom.toStringAsFixed(1)}×',
                                          style: const TextStyle(
                                            color: Colors.white,
                                            fontSize: 12,
                                            fontWeight: FontWeight.w600,
                                          ),
                                        ),
                                      ),
                                    ),
                                  _CameraHint(l10n: l),
                                  if (_detecting) const _DetectingOverlay(),
                                ],
                              ),
                            );
                          },
                        ),
                      ),
              ),
            ),
            Padding(
              padding: const EdgeInsets.fromLTRB(24, 12, 24, 26),
              child: Row(
                children: [
                  const Expanded(child: SizedBox()),
                  // The shutter is a bare gesture target, so it needs an
                  // explicit label: §12B.11 asks for semantics on every
                  // control, and a screen reader would otherwise announce an
                  // unlabelled box.
                  Semantics(
                    button: true,
                    label: l.t('scan.camera.capture'),
                    child: Tooltip(
                      message: l.t('scan.camera.capture'),
                      child: GestureDetector(
                        onTap: _busy || !ready ? null : _capture,
                        child: Container(
                          width: 74,
                          height: 74,
                          decoration: BoxDecoration(
                            shape: BoxShape.circle,
                            border: Border.all(
                              color: ready ? Colors.white : Colors.white38,
                              width: 4,
                            ),
                          ),
                          padding: const EdgeInsets.all(6),
                          child: DecoratedBox(
                            decoration: BoxDecoration(
                              color: ready ? Colors.white : Colors.white24,
                              shape: BoxShape.circle,
                            ),
                            child: _busy
                                ? const Padding(
                                    padding: EdgeInsets.all(16),
                                    child: CircularProgressIndicator(
                                      strokeWidth: 2,
                                    ),
                                  )
                                : null,
                          ),
                        ),
                      ),
                    ),
                  ),
                  const Expanded(child: SizedBox()),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

/// Frames from the desktop grab thread. Polling keeps the widget tree free of
/// platform-channel round trips; the newest frame is drawn as soon as it moves.
class _DesktopPreview extends StatefulWidget {
  const _DesktopPreview({required this.camera});

  final DesktopCamera camera;

  @override
  State<_DesktopPreview> createState() => _DesktopPreviewState();
}

class _DesktopPreviewState extends State<_DesktopPreview> {
  Timer? _poll;
  Uint8List? _frame;

  @override
  void initState() {
    super.initState();
    _frame = widget.camera.frame();
    _poll = Timer.periodic(const Duration(milliseconds: 66), (_) {
      final next = widget.camera.frame();
      if (next == null || !mounted) return;
      setState(() => _frame = next);
    });
  }

  @override
  void dispose() {
    _poll?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final frame = _frame;
    if (frame == null) {
      return const Center(child: CircularProgressIndicator());
    }
    return SizedBox.expand(
      child: Image.memory(frame, fit: BoxFit.contain, gaplessPlayback: true),
    );
  }
}

class _CameraHint extends StatelessWidget {
  const _CameraHint({required this.l10n});

  final L10n l10n;

  @override
  Widget build(BuildContext context) {
    return Positioned(
      left: 16,
      right: 16,
      top: 12,
      child: IgnorePointer(
        child: Text(
          l10n.t('scan.camera.hint'),
          textAlign: TextAlign.center,
          style: const TextStyle(
            color: Colors.white,
            fontSize: 14,
            shadows: [Shadow(color: Colors.black54, blurRadius: 6)],
          ),
        ),
      ),
    );
  }
}

class _DetectingOverlay extends StatelessWidget {
  const _DetectingOverlay();

  @override
  Widget build(BuildContext context) {
    return Container(
      color: Colors.black54,
      alignment: Alignment.center,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const CircularProgressIndicator(),
          const SizedBox(height: 14),
          Text(
            L10n(Localizations.localeOf(context)).t('scan.confirm.detecting'),
            style: const TextStyle(color: Colors.white, fontSize: 14),
          ),
        ],
      ),
    );
  }
}

/// The tap-to-focus ring: a quick pulse that does not block the preview.
class _FocusRing extends StatelessWidget {
  const _FocusRing();

  @override
  Widget build(BuildContext context) {
    return IgnorePointer(
      child: TweenAnimationBuilder<double>(
        tween: Tween(begin: 1.35, end: 1),
        duration: const Duration(milliseconds: 260),
        builder: (context, value, child) =>
            Transform.scale(scale: value, child: child),
        child: Container(
          width: 52,
          height: 52,
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            border: Border.all(color: Colors.white, width: 1.6),
          ),
        ),
      ),
    );
  }
}

class _FrameGuide extends StatelessWidget {
  const _FrameGuide();

  @override
  Widget build(BuildContext context) {
    return IgnorePointer(child: CustomPaint(painter: _FramePainter()));
  }
}

class _FramePainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final rect = Rect.fromLTRB(
      size.width * 0.06,
      size.height * 0.18,
      size.width * 0.94,
      size.height * 0.82,
    );
    final paint = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2.4
      ..strokeCap = StrokeCap.round;
    final length = rect.shortestSide * 0.14;
    // Four corner brackets, not a full rectangle: the expression stays visible.
    canvas.drawLine(rect.topLeft, rect.topLeft + Offset(length, 0), paint);
    canvas.drawLine(rect.topLeft, rect.topLeft + Offset(0, length), paint);
    canvas.drawLine(rect.topRight, rect.topRight - Offset(length, 0), paint);
    canvas.drawLine(rect.topRight, rect.topRight + Offset(0, length), paint);
    canvas.drawLine(
      rect.bottomLeft,
      rect.bottomLeft + Offset(length, 0),
      paint,
    );
    canvas.drawLine(
      rect.bottomLeft,
      rect.bottomLeft - Offset(0, length),
      paint,
    );
    canvas.drawLine(
      rect.bottomRight,
      rect.bottomRight - Offset(length, 0),
      paint,
    );
    canvas.drawLine(
      rect.bottomRight,
      rect.bottomRight - Offset(0, length),
      paint,
    );
  }

  @override
  bool shouldRepaint(covariant _FramePainter oldDelegate) => false;
}

class _ErrorView extends StatelessWidget {
  const _ErrorView({
    required this.l10n,
    required this.message,
    required this.onRetry,
  });

  final L10n l10n;
  final String message;
  final VoidCallback onRetry;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.all(28),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Icon(
            Icons.no_photography_outlined,
            size: 44,
            color: Colors.white70,
          ),
          const SizedBox(height: 14),
          Text(
            message,
            textAlign: TextAlign.center,
            style: const TextStyle(color: Colors.white),
          ),
          const SizedBox(height: 18),
          FilledButton.icon(
            onPressed: onRetry,
            icon: const Icon(Icons.refresh_rounded),
            label: Text(l10n.t('scan.camera.retry')),
          ),
        ],
      ),
    );
  }
}
