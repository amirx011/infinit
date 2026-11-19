import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'l10n.dart';
import 'theme.dart';

/// Renders the app the way a mobile browser renders a desktop site.
///
/// The layout is computed at a desktop width and then painted scaled down so
/// the whole "window" fits the screen. Two fingers pinch to zoom and drag to
/// pan horizontally; one finger keeps scrolling the page, because this widget
/// listens to raw pointers instead of competing in the gesture arena.
class DesktopViewport extends StatefulWidget {
  const DesktopViewport({
    super.key,
    required this.l10n,
    required this.child,
    this.enabled = true,
    this.designWidth = 1100,
    this.onSearch,
    this.onSettings,
  });

  final L10n l10n;
  final Widget child;
  final VoidCallback? onSearch;
  final VoidCallback? onSettings;

  /// When false the child is laid out at the real screen size with no zoom
  /// controls, so the widget tree stays identical while the setting changes.
  final bool enabled;

  /// Width of the virtual desktop window, in logical pixels.
  final double designWidth;

  static const double minZoom = 1;
  static const double maxZoom = 5;

  @override
  State<DesktopViewport> createState() => _DesktopViewportState();
}

class _DesktopViewportState extends State<DesktopViewport>
    with WidgetsBindingObserver {
  final Map<int, Offset> _pointers = <int, Offset>{};

  double _zoom = 1;
  double _dx = 0;

  double _startZoom = 1;
  double _startDx = 0;
  double _startDistance = 0;
  Offset _startFocal = Offset.zero;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    if (widget.enabled) _hideStatusBar();
  }

  @override
  void didUpdateWidget(DesktopViewport oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.enabled == oldWidget.enabled) return;
    if (widget.enabled) {
      _hideStatusBar();
    } else {
      _restoreSystemUi();
      _reset();
    }
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    if (widget.enabled) _restoreSystemUi();
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    // Android brings the bars back after the app returns from the background.
    if (state == AppLifecycleState.resumed && widget.enabled) _hideStatusBar();
  }

  /// Desktop mode is a full-window view, so the Android clock and battery bar
  /// would only steal taps from the scaled header. The navigation bar stays.
  ///
  /// Flutter's own [SystemChrome] flags are ignored on Android 15+, so the
  /// activity hides the bar with the insets controller instead.
  static const MethodChannel _channel = MethodChannel('ir.infinit/settings');

  void _hideStatusBar() => _setSystemBars(true);

  void _restoreSystemUi() => _setSystemBars(false);

  Future<void> _setSystemBars(bool hidden) async {
    if (kIsWeb || defaultTargetPlatform != TargetPlatform.android) return;
    try {
      await _channel.invokeMethod<void>('set_desktop_mode', hidden);
    } catch (_) {
      // A build without the native handler simply keeps the status bar.
    }
  }

  void _reset() {
    _pointers.clear();
    _startDistance = 0;
    _zoom = 1;
    _dx = 0;
  }

  void _onPointerDown(PointerDownEvent event) {
    if (!widget.enabled) return;
    _pointers[event.pointer] = event.localPosition;
    if (_pointers.length == 2) {
      final points = _pointers.values.take(2).toList();
      _startFocal = (points[0] + points[1]) / 2;
      _startDistance = (points[0] - points[1]).distance;
      _startZoom = _zoom;
      _startDx = _dx;
    }
  }

  void _onPointerMove(
    PointerMoveEvent event,
    Size size,
    double width,
    double fit,
  ) {
    if (!widget.enabled) return;
    if (!_pointers.containsKey(event.pointer)) return;
    _pointers[event.pointer] = event.localPosition;
    if (_pointers.length < 2 || _startDistance <= 0) return;
    final points = _pointers.values.take(2).toList();
    final focal = (points[0] + points[1]) / 2;
    final distance = (points[0] - points[1]).distance;
    final zoom = (_startZoom * distance / _startDistance).clamp(
      DesktopViewport.minZoom,
      DesktopViewport.maxZoom,
    );
    // Keep the page point that was under the fingers pinned to the fingers.
    final startScale = fit * _startZoom;
    final anchorX = (_startFocal.dx - _startDx) / startScale;
    final scale = fit * zoom;
    final dx = _clampDx(focal.dx - anchorX * scale, size.width, width * scale);
    if ((zoom - _zoom).abs() < 0.001 && (dx - _dx).abs() < 0.01) return;
    setState(() {
      _zoom = zoom;
      _dx = dx;
    });
  }

  void _onPointerRelease(PointerEvent event) {
    _pointers.remove(event.pointer);
    if (_pointers.length < 2) _startDistance = 0;
  }

  void _zoomTo(double target, Size size, double width, double fit) {
    final zoom = target.clamp(DesktopViewport.minZoom, DesktopViewport.maxZoom);
    final focalX = size.width / 2;
    final anchorX = (focalX - _dx) / (fit * _zoom);
    final scale = fit * zoom;
    setState(() {
      _zoom = zoom;
      _dx = _clampDx(focalX - anchorX * scale, size.width, width * scale);
    });
  }

  @override
  Widget build(BuildContext context) {
    final media = MediaQuery.of(context);
    return LayoutBuilder(
      builder: (context, constraints) {
        final size = constraints.biggest;
        final enabled = widget.enabled;
        final width = enabled
            ? math.max(widget.designWidth, size.width)
            : size.width;
        final fit = size.width / width;
        final scale = fit * _zoom;
        final height = enabled ? size.height / scale : size.height;
        final dx = enabled ? _clampDx(_dx, size.width, width * scale) : 0.0;

        Widget canvas = Listener(
          behavior: HitTestBehavior.translucent,
          onPointerDown: _onPointerDown,
          onPointerMove: (event) => _onPointerMove(event, size, width, fit),
          onPointerUp: _onPointerRelease,
          onPointerCancel: _onPointerRelease,
          child: OverflowBox(
            alignment: Alignment.topLeft,
            minWidth: 0,
            minHeight: 0,
            maxWidth: double.infinity,
            maxHeight: double.infinity,
            child: Transform(
              key: const ValueKey('desktop-viewport-transform'),
              transform: Matrix4.identity()
                ..translateByDouble(dx, 0, 0, 1)
                ..scaleByDouble(scale, scale, 1, 1),
              child: SizedBox(
                width: width,
                height: height,
                child: MediaQuery(
                  data: media.copyWith(
                    size: Size(width, height),
                    padding: _scaled(media.padding, scale),
                    viewPadding: _scaled(media.viewPadding, scale),
                    viewInsets: _scaled(media.viewInsets, scale),
                  ),
                  child: widget.child,
                ),
              ),
            ),
          ),
        );

        if (enabled && _zoom > 1.001) {
          // One finger pans the zoomed window; vertical drags still belong to
          // the page's own list because this recogniser only claims the
          // horizontal axis.
          canvas = GestureDetector(
            behavior: HitTestBehavior.translucent,
            onHorizontalDragUpdate: (details) {
              if (_pointers.length > 1) return;
              final next = _clampDx(
                _dx + details.delta.dx,
                size.width,
                width * scale,
              );
              if ((next - _dx).abs() < 0.01) return;
              setState(() => _dx = next);
            },
            child: canvas,
          );
        }

        return ClipRect(
          child: Stack(
            fit: StackFit.expand,
            children: [
              canvas,
              if (enabled)
                Positioned(
                  left: 0,
                  right: 0,
                  bottom: media.viewPadding.bottom + 12,
                  child: Center(
                    child: FittedBox(
                      fit: BoxFit.scaleDown,
                      child: Padding(
                        padding: const EdgeInsets.symmetric(horizontal: 8),
                        child: _ZoomBar(
                          l10n: widget.l10n,
                          zoom: _zoom,
                          onSearch: widget.onSearch,
                          onSettings: widget.onSettings,
                          onZoomOut: () =>
                              _zoomTo(_zoom / 1.25, size, width, fit),
                          onZoomIn: () =>
                              _zoomTo(_zoom * 1.25, size, width, fit),
                          onFit: () => _zoomTo(1, size, width, fit),
                        ),
                      ),
                    ),
                  ),
                ),
            ],
          ),
        );
      },
    );
  }

  /// Keeps the window edges outside the screen without tripping over the
  /// rounding error that makes `screen - content` a hair above zero at 100%.
  static double _clampDx(double dx, double screen, double content) {
    final min = math.min(0.0, screen - content);
    return dx.clamp(min, 0.0);
  }

  static EdgeInsets _scaled(EdgeInsets value, double scale) =>
      EdgeInsets.fromLTRB(
        value.left / scale,
        value.top / scale,
        value.right / scale,
        value.bottom / scale,
      );
}

/// Floating zoom controls. They live in screen space, so they stay readable
/// even when the desktop window is scaled far below 100%.
class _ZoomBar extends StatelessWidget {
  const _ZoomBar({
    required this.l10n,
    required this.zoom,
    required this.onZoomOut,
    required this.onZoomIn,
    required this.onFit,
    this.onSearch,
    this.onSettings,
  });

  final L10n l10n;
  final double zoom;
  final VoidCallback onZoomOut;
  final VoidCallback onZoomIn;
  final VoidCallback onFit;
  final VoidCallback? onSearch;
  final VoidCallback? onSettings;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final atFit = zoom <= 1.001;
    return Material(
      key: const ValueKey('desktop-zoom-bar'),
      color: c.card.withValues(alpha: 0.97),
      elevation: 8,
      shadowColor: Colors.black.withValues(alpha: 0.28),
      borderRadius: BorderRadius.circular(22),
      child: Container(
        height: 44,
        padding: const EdgeInsets.symmetric(horizontal: 4),
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(22),
          border: Border.all(color: c.line),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            if (onSearch != null)
              _ZoomButton(
                tooltip: l10n.t('ui.search_modules'),
                icon: Icons.search_rounded,
                onPressed: onSearch,
              ),
            if (onSettings != null)
              _ZoomButton(
                tooltip: l10n.t('ui.settings'),
                icon: Icons.settings_rounded,
                onPressed: onSettings,
              ),
            if (onSearch != null || onSettings != null)
              Container(width: 1, height: 20, color: c.line),
            _ZoomButton(
              tooltip: l10n.t('ui.zoom_out'),
              icon: Icons.remove_rounded,
              onPressed: atFit ? null : onZoomOut,
            ),
            Container(
              width: 52,
              alignment: Alignment.center,
              child: Text(
                '${(zoom * 100).round()}%',
                textDirection: TextDirection.ltr,
                style: Theme.of(context).textTheme.labelMedium?.copyWith(
                  fontFeatures: const [FontFeature.tabularFigures()],
                ),
              ),
            ),
            _ZoomButton(
              tooltip: l10n.t('ui.zoom_in'),
              icon: Icons.add_rounded,
              onPressed: zoom >= DesktopViewport.maxZoom - 0.001
                  ? null
                  : onZoomIn,
            ),
            Container(width: 1, height: 20, color: c.line),
            _ZoomButton(
              tooltip: l10n.t('ui.fit_width'),
              icon: Icons.fit_screen_rounded,
              onPressed: atFit ? null : onFit,
            ),
          ],
        ),
      ),
    );
  }
}

class _ZoomButton extends StatelessWidget {
  const _ZoomButton({
    required this.tooltip,
    required this.icon,
    required this.onPressed,
  });

  final String tooltip;
  final IconData icon;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    return Semantics(
      label: tooltip,
      button: true,
      enabled: onPressed != null,
      child: IconButton(
        onPressed: onPressed,
        icon: Icon(icon, size: 19),
        padding: EdgeInsets.zero,
        constraints: const BoxConstraints.tightFor(width: 38, height: 38),
        visualDensity: VisualDensity.compact,
      ),
    );
  }
}
