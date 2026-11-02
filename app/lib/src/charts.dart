// Lightweight plotting for the Flutter UI.
//
// Every chart is painted with a CustomPainter so the same widget runs on
// Linux, Android and the web without a Python or gnuplot dependency.
import 'dart:math' as math;

import 'package:flutter/material.dart';

import 'l10n.dart';
import 'models.dart';
import 'native_engine.dart';
import 'theme.dart';

enum PlotMarkerKind { hole, point, singular }

class PlotSeries {
  const PlotSeries({
    required this.xs,
    required this.ys,
    required this.color,
    this.label,
    this.width = 2.2,
    this.dashed = false,
    this.fill = false,
  });

  final List<double> xs;
  final List<double> ys;
  final Color color;
  final String? label;
  final double width;
  final bool dashed;

  /// Shades the area between the curve and the x-axis.
  final bool fill;
}

class PlotMarker {
  const PlotMarker({
    required this.x,
    required this.y,
    required this.color,
    this.kind = PlotMarkerKind.point,
    this.label,
  });

  final double x;
  final double y;
  final Color color;
  final PlotMarkerKind kind;
  final String? label;
}

/// A vertical shaded band, used to show the domain on an x-axis.
class PlotBand {
  const PlotBand({
    required this.interval,
    required this.color,
    this.label,
    this.horizontal = false,
  });

  final IntervalView interval;
  final Color color;
  final String? label;

  /// When true the interval is shaded on the y-axis (a range band).
  final bool horizontal;
}

/// One direction-field arrow at (x, y) with local slope y'.
class PlotArrow {
  const PlotArrow({required this.x, required this.y, required this.slope});

  final double x;
  final double y;
  final double slope;
}

class PlotSpec {
  const PlotSpec({
    this.series = const [],
    this.markers = const [],
    this.bands = const [],
    this.asymptotes = const [],
    this.guides = const [],
    this.arrows = const [],
    this.xMin,
    this.xMax,
    this.yMin,
    this.yMax,
    this.xLabel,
    this.yLabel,
    this.showGrid = true,
    this.showAxes = true,
    this.emptyMessage,
  });

  final List<PlotSeries> series;
  final List<PlotMarker> markers;
  final List<PlotBand> bands;
  final List<double> asymptotes;

  /// Vertical reference lines (for example the point a limit approaches).
  final List<double> guides;
  final List<PlotArrow> arrows;
  final double? xMin;
  final double? xMax;
  final double? yMin;
  final double? yMax;
  final String? xLabel;
  final String? yLabel;
  final bool showGrid;
  final bool showAxes;
  final String? emptyMessage;

  List<String> get legend {
    final seen = <String>{};
    final out = <String>[];
    for (final s in series) {
      final label = s.label;
      if (label == null || label.isEmpty) continue;
      if (seen.add(label)) out.add(label);
    }
    return out;
  }

  Color? colorOfLabel(String label) {
    for (final s in series) {
      if (s.label == label) return s.color;
    }
    return null;
  }
}

/// A responsive plot with axes, ticks, optional domain bands, asymptotes and
/// direction-field arrows.
class MathPlot extends StatelessWidget {
  const MathPlot({
    super.key,
    required this.spec,
    this.height = 300,
    this.legend = true,
    this.caption,
  });

  final PlotSpec spec;
  final double height;
  final bool legend;
  final String? caption;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final showLegend = legend && spec.legend.isNotEmpty;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Container(
          height: height,
          decoration: BoxDecoration(
            color: c.cardAlt,
            borderRadius: BorderRadius.circular(16),
            border: Border.all(color: c.line),
          ),
          clipBehavior: Clip.antiAlias,
          child: Directionality(
            textDirection: TextDirection.ltr,
            child: CustomPaint(
              painter: _PlotPainter(
                spec: spec,
                colors: c,
                textDirection: Directionality.of(context),
              ),
              size: Size.infinite,
            ),
          ),
        ),
        if (showLegend) ...[
          const SizedBox(height: 10),
          Wrap(
            spacing: 14,
            runSpacing: 6,
            children: [
              for (final label in spec.legend)
                Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Container(
                      width: 9,
                      height: 9,
                      decoration: BoxDecoration(
                        color: spec.colorOfLabel(label) ?? c.accent,
                        shape: BoxShape.circle,
                      ),
                    ),
                    const SizedBox(width: 6),
                    Text(label, style: Theme.of(context).textTheme.bodySmall),
                  ],
                ),
            ],
          ),
        ],
        if (caption != null) ...[
          const SizedBox(height: 8),
          Text(caption!, style: Theme.of(context).textTheme.bodySmall),
        ],
      ],
    );
  }
}

class _Bounds {
  const _Bounds(this.x0, this.x1, this.y0, this.y1);

  final double x0;
  final double x1;
  final double y0;
  final double y1;

  double get xSpan => x1 - x0;
  double get ySpan => y1 - y0;

  bool get valid => xSpan > 0 && ySpan > 0;
}

class _PlotPainter extends CustomPainter {
  _PlotPainter({
    required this.spec,
    required this.colors,
    required this.textDirection,
  });

  final PlotSpec spec;
  final OdeColors colors;
  final TextDirection textDirection;

  static const _padLeft = 46.0;
  static const _padTop = 14.0;
  static const _padRight = 16.0;
  static const _padBottom = 28.0;

  @override
  void paint(Canvas canvas, Size size) {
    final rect = Rect.fromLTRB(
      _padLeft,
      _padTop,
      math.max(_padLeft + 10, size.width - _padRight),
      math.max(_padTop + 10, size.height - _padBottom),
    );
    final bounds = _resolveBounds(rect);
    if (bounds == null || !bounds.valid) {
      _drawEmpty(canvas, size);
      return;
    }

    double px(double x) =>
        rect.left + (x - bounds.x0) / bounds.xSpan * rect.width;
    double py(double y) =>
        rect.bottom - (y - bounds.y0) / bounds.ySpan * rect.height;

    canvas.save();
    canvas.clipRect(rect);

    if (spec.showGrid) _drawGrid(canvas, rect, bounds, px, py);

    // A tint that reads as a gentle wash on a dark canvas turns the whole
    // light chart lavender, so scale it to the surface brightness.
    final bandAlpha = colors.background.computeLuminance() > 0.5 ? 0.05 : 0.09;
    for (final band in spec.bands) {
      if (band.horizontal) {
        // An infinite lower bound reaches the bottom of the plot, not the top:
        // py() grows downward, so lo/hi must be mapped to their own edges.
        final y0 = band.interval.loInfinite
            ? rect.bottom
            : py(band.interval.lo);
        final y1 = band.interval.hiInfinite ? rect.top : py(band.interval.hi);
        final top = math.max(rect.top, math.min(y0, y1));
        final bottom = math.min(rect.bottom, math.max(y0, y1));
        if (bottom <= top) continue;
        canvas.drawRect(
          Rect.fromLTRB(rect.left, top, rect.right, bottom),
          Paint()..color = band.color.withValues(alpha: bandAlpha),
        );
      } else {
        final x0 = band.interval.loInfinite ? rect.left : px(band.interval.lo);
        final x1 = band.interval.hiInfinite ? rect.right : px(band.interval.hi);
        final left = math.max(rect.left, math.min(x0, x1));
        final right = math.min(rect.right, math.max(x0, x1));
        if (right <= left) continue;
        canvas.drawRect(
          Rect.fromLTRB(left, rect.top, right, rect.bottom),
          Paint()..color = band.color.withValues(alpha: bandAlpha),
        );
      }
    }

    for (final a in spec.asymptotes) {
      if (!a.isFinite || a < bounds.x0 || a > bounds.x1) continue;
      _dashedLine(
        canvas,
        Offset(px(a), rect.top),
        Offset(px(a), rect.bottom),
        colors.warning.withValues(alpha: 0.55),
      );
    }

    for (final g in spec.guides) {
      if (!g.isFinite || g < bounds.x0 || g > bounds.x1) continue;
      _dashedLine(
        canvas,
        Offset(px(g), rect.top),
        Offset(px(g), rect.bottom),
        colors.accent.withValues(alpha: 0.35),
      );
    }

    if (spec.showAxes) {
      _drawAxes(canvas, rect, bounds, px, py);
    }

    for (final arrow in spec.arrows) {
      _drawArrow(canvas, rect, bounds, arrow, px, py);
    }

    for (final series in spec.series) {
      _drawSeries(canvas, rect, bounds, series, px, py);
    }

    for (final marker in spec.markers) {
      if (!marker.x.isFinite || !marker.y.isFinite) continue;
      final p = Offset(px(marker.x), py(marker.y));
      if (!rect.inflate(8).contains(p)) continue;
      _drawMarker(canvas, p, marker);
    }

    canvas.restore();
    _drawTicks(canvas, rect, bounds, px, py);
  }

  _Bounds? _resolveBounds(Rect rect) {
    final xs = <double>[];
    final ys = <double>[];
    for (final s in spec.series) {
      final n = math.min(s.xs.length, s.ys.length);
      for (var i = 0; i < n; i++) {
        final x = s.xs[i];
        final y = s.ys[i];
        if (x.isFinite && y.isFinite) {
          xs.add(x);
          ys.add(y);
        }
      }
    }
    for (final m in spec.markers) {
      if (m.x.isFinite && m.y.isFinite) {
        xs.add(m.x);
        ys.add(m.y);
      }
    }
    for (final b in spec.bands) {
      final target = b.horizontal ? ys : xs;
      if (!b.interval.loInfinite) target.add(b.interval.lo);
      if (!b.interval.hiInfinite) target.add(b.interval.hi);
    }
    for (final a in spec.asymptotes) {
      if (a.isFinite) xs.add(a);
    }
    for (final a in spec.arrows) {
      if (a.x.isFinite) xs.add(a.x);
      if (a.y.isFinite) ys.add(a.y);
    }

    double x0;
    double x1;
    if (spec.xMin != null && spec.xMax != null) {
      x0 = spec.xMin!;
      x1 = spec.xMax!;
    } else {
      if (xs.isEmpty) return null;
      x0 = xs.reduce(math.min);
      x1 = xs.reduce(math.max);
      if (x1 - x0 < 1e-12) {
        x0 -= 1;
        x1 += 1;
      }
    }

    double y0;
    double y1;
    if (spec.yMin != null && spec.yMax != null) {
      y0 = spec.yMin!;
      y1 = spec.yMax!;
    } else {
      final finite = ys.where((v) => v.isFinite).toList()..sort();
      if (finite.isEmpty) {
        if (spec.series.isEmpty && spec.arrows.isEmpty) return null;
        y0 = spec.yMin ?? -1;
        y1 = spec.yMax ?? 1;
      } else {
        y0 = spec.yMin ?? finite.first;
        y1 = spec.yMax ?? finite.last;
        // Outliers near a pole should not flatten the whole picture.
        if (spec.yMin == null && spec.yMax == null && finite.length > 8) {
          final lo = finite[(finite.length * 0.02).floor()];
          final hiIndex = (finite.length * 0.98)
              .ceil()
              .clamp(0, finite.length - 1)
              .toInt();
          final hi = finite[hiIndex];
          final core = hi - lo;
          if (core > 1e-12 && (y1 - y0) > 8 * core) {
            y0 = lo - core * 0.08;
            y1 = hi + core * 0.08;
          }
        }
        if (y1 - y0 < 1e-12) {
          final pad = math.max(0.5, y0.abs() * 0.2);
          y0 -= pad;
          y1 += pad;
        }
      }
    }

    final xPad = (x1 - x0) * 0.03;
    final yPad = (y1 - y0) * 0.06;
    return _Bounds(x0 - xPad, x1 + xPad, y0 - yPad, y1 + yPad);
  }

  void _drawGrid(
    Canvas canvas,
    Rect rect,
    _Bounds b,
    double Function(double) px,
    double Function(double) py,
  ) {
    final paint = Paint()
      ..color = colors.line.withValues(alpha: 0.45)
      ..strokeWidth = 1;
    for (final x in _ticks(b.x0, b.x1)) {
      final sx = px(x);
      canvas.drawLine(Offset(sx, rect.top), Offset(sx, rect.bottom), paint);
    }
    for (final y in _ticks(b.y0, b.y1)) {
      final sy = py(y);
      canvas.drawLine(Offset(rect.left, sy), Offset(rect.right, sy), paint);
    }
  }

  void _drawAxes(
    Canvas canvas,
    Rect rect,
    _Bounds b,
    double Function(double) px,
    double Function(double) py,
  ) {
    final paint = Paint()
      ..color = colors.muted.withValues(alpha: 0.75)
      ..strokeWidth = 1.3;
    if (b.y0 <= 0 && b.y1 >= 0) {
      final y = py(0);
      canvas.drawLine(Offset(rect.left, y), Offset(rect.right, y), paint);
    }
    if (b.x0 <= 0 && b.x1 >= 0) {
      final x = px(0);
      canvas.drawLine(Offset(x, rect.top), Offset(x, rect.bottom), paint);
    }
  }

  void _drawSeries(
    Canvas canvas,
    Rect rect,
    _Bounds b,
    PlotSeries series,
    double Function(double) px,
    double Function(double) py,
  ) {
    final n = math.min(series.xs.length, series.ys.length);
    if (n < 2) {
      if (n == 1) {
        final x = series.xs[0];
        final y = series.ys[0];
        if (x.isFinite && y.isFinite) {
          canvas.drawCircle(
            Offset(px(x), py(y)),
            2,
            Paint()..color = series.color,
          );
        }
      }
      return;
    }
    final path = Path();
    final fill = Path();
    var penDown = false;
    var lastY = double.nan;
    var lastX = rect.left;
    final jumpLimit = b.ySpan * 1.6;
    for (var i = 0; i < n; i++) {
      final x = series.xs[i];
      final y = series.ys[i];
      if (!x.isFinite || !y.isFinite) {
        penDown = false;
        lastY = double.nan;
        continue;
      }
      final p = Offset(px(x), py(y));
      lastX = p.dx;
      if (!penDown) {
        path.moveTo(p.dx, p.dy);
        fill.moveTo(p.dx, rect.bottom);
        fill.lineTo(p.dx, p.dy);
        penDown = true;
      } else {
        final jump = (y - lastY).abs();
        if (jump > jumpLimit) {
          path.moveTo(p.dx, p.dy);
          fill.lineTo(p.dx, rect.bottom);
          fill.moveTo(p.dx, rect.bottom);
          fill.lineTo(p.dx, p.dy);
        } else {
          path.lineTo(p.dx, p.dy);
          fill.lineTo(p.dx, p.dy);
        }
      }
      lastY = y;
    }
    if (series.fill) {
      fill.lineTo(lastX, rect.bottom);
      fill.close();
      canvas.drawPath(
        fill,
        Paint()..color = series.color.withValues(alpha: 0.12),
      );
    }
    if (series.dashed) {
      _drawDashedPath(canvas, path, series.color, series.width);
    } else {
      canvas.drawPath(
        path,
        Paint()
          ..color = series.color
          ..style = PaintingStyle.stroke
          ..strokeWidth = series.width
          ..strokeCap = StrokeCap.round
          ..strokeJoin = StrokeJoin.round
          ..isAntiAlias = true,
      );
    }
  }

  void _drawMarker(Canvas canvas, Offset p, PlotMarker marker) {
    switch (marker.kind) {
      case PlotMarkerKind.hole:
        canvas.drawCircle(p, 4.5, Paint()..color = colors.cardAlt);
        canvas.drawCircle(
          p,
          4.5,
          Paint()
            ..color = marker.color
            ..style = PaintingStyle.stroke
            ..strokeWidth = 1.8,
        );
      case PlotMarkerKind.singular:
        final paint = Paint()
          ..color = marker.color
          ..style = PaintingStyle.stroke
          ..strokeWidth = 1.8;
        canvas.drawLine(p.translate(-3, -3), p.translate(3, 3), paint);
        canvas.drawLine(p.translate(3, -3), p.translate(-3, 3), paint);
      case PlotMarkerKind.point:
        canvas.drawCircle(p, 3.6, Paint()..color = marker.color);
        canvas.drawCircle(
          p,
          3.6,
          Paint()
            ..color = colors.cardAlt
            ..style = PaintingStyle.stroke
            ..strokeWidth = 1.2,
        );
    }
  }

  void _drawArrow(
    Canvas canvas,
    Rect rect,
    _Bounds b,
    PlotArrow arrow,
    double Function(double) px,
    double Function(double) py,
  ) {
    if (!arrow.x.isFinite || !arrow.y.isFinite || !arrow.slope.isFinite) {
      return;
    }
    final x = arrow.x;
    final y = arrow.y;
    if (x < b.x0 || x > b.x1 || y < b.y0 || y > b.y1) return;
    final scaleX = rect.width / b.xSpan;
    final scaleY = rect.height / b.ySpan;
    var vx = scaleX;
    var vy = -arrow.slope * scaleY;
    final len = math.sqrt(vx * vx + vy * vy);
    if (!len.isFinite || len <= 0) return;
    vx /= len;
    vy /= len;
    final half = 4.6;
    final centre = Offset(px(x), py(y));
    final a = centre.translate(-vx * half, -vy * half);
    final tip = centre.translate(vx * half, vy * half);
    final paint = Paint()
      ..color = colors.accent.withValues(alpha: 0.55)
      ..strokeWidth = 1.15
      ..strokeCap = StrokeCap.round;
    canvas.drawLine(a, tip, paint);
    final back = 2.6;
    final wing = 1.7;
    canvas.drawLine(
      tip,
      tip.translate(-vx * back + vy * wing, -vy * back - vx * wing),
      paint,
    );
    canvas.drawLine(
      tip,
      tip.translate(-vx * back - vy * wing, -vy * back + vx * wing),
      paint,
    );
  }

  void _drawTicks(
    Canvas canvas,
    Rect rect,
    _Bounds b,
    double Function(double) px,
    double Function(double) py,
  ) {
    final style = TextStyle(
      color: colors.muted,
      fontSize: 10,
      fontFamily: 'Vazirmatn',
    );
    for (final x in _ticks(b.x0, b.x1)) {
      final sx = px(x);
      if (sx < rect.left - 1 || sx > rect.right + 1) continue;
      _text(
        canvas,
        _formatTick(x),
        Offset(sx, rect.bottom + 7),
        style,
        align: TextAlign.center,
        direction: TextDirection.ltr,
      );
    }
    for (final y in _ticks(b.y0, b.y1)) {
      final sy = py(y);
      if (sy < rect.top - 1 || sy > rect.bottom + 1) continue;
      _text(
        canvas,
        _formatTick(y),
        Offset(rect.left - 7, sy - 6),
        style,
        align: TextAlign.right,
        direction: TextDirection.ltr,
      );
    }
  }

  void _text(
    Canvas canvas,
    String text,
    Offset offset,
    TextStyle style, {
    TextAlign align = TextAlign.left,
    TextDirection? direction,
  }) {
    final tp = TextPainter(
      text: TextSpan(text: text, style: style),
      textDirection: direction ?? textDirection,
      textAlign: align,
    )..layout();
    var dx = offset.dx;
    if (align == TextAlign.center) dx -= tp.width / 2;
    if (align == TextAlign.right) dx -= tp.width;
    tp.paint(canvas, Offset(dx, offset.dy));
  }

  void _drawEmpty(Canvas canvas, Size size) {
    final tp = TextPainter(
      text: TextSpan(
        text: spec.emptyMessage ?? 'No finite samples in this window.',
        style: TextStyle(
          fontFamily: 'Vazirmatn',
          color: colors.muted,
          fontSize: 12.5,
        ),
      ),
      textDirection: textDirection,
      textAlign: TextAlign.center,
    )..layout(maxWidth: size.width - 32);
    tp.paint(
      canvas,
      Offset((size.width - tp.width) / 2, (size.height - tp.height) / 2),
    );
  }

  static void _dashedLine(Canvas canvas, Offset a, Offset b, Color color) {
    final paint = Paint()
      ..color = color
      ..strokeWidth = 1.3;
    const dash = 5.0;
    const gap = 4.0;
    final delta = b - a;
    final length = delta.distance;
    if (length <= 0) return;
    final unit = delta / length;
    var t = 0.0;
    while (t < length) {
      final end = math.min(t + dash, length);
      canvas.drawLine(a + unit * t, a + unit * end, paint);
      t = end + gap;
    }
  }

  static void _drawDashedPath(
    Canvas canvas,
    Path path,
    Color color,
    double width,
  ) {
    final paint = Paint()
      ..color = color
      ..style = PaintingStyle.stroke
      ..strokeWidth = width
      ..strokeCap = StrokeCap.round;
    for (final metric in path.computeMetrics()) {
      var distance = 0.0;
      while (distance < metric.length) {
        final end = math.min(distance + 6, metric.length);
        canvas.drawPath(metric.extractPath(distance, end), paint);
        distance = end + 4;
      }
    }
  }

  static List<double> _ticks(double lo, double hi) {
    if (!lo.isFinite || !hi.isFinite || hi <= lo) return const [];
    final step = _niceStep((hi - lo) / 5);
    final first = (lo / step).ceil() * step;
    final out = <double>[];
    for (var v = first; v <= hi + step * 1e-6; v += step) {
      if (v.abs() < step * 1e-9) {
        out.add(0);
      } else {
        out.add(v);
      }
      if (out.length > 24) break;
    }
    return out;
  }

  static double _niceStep(double raw) {
    if (!raw.isFinite || raw <= 0) return 1;
    final mag = math.pow(10, (math.log(raw) / math.ln10).floor()).toDouble();
    final norm = raw / mag;
    double nice;
    if (norm <= 1) {
      nice = 1;
    } else if (norm <= 2) {
      nice = 2;
    } else if (norm <= 5) {
      nice = 5;
    } else {
      nice = 10;
    }
    return nice * mag;
  }

  static String _formatTick(double v) {
    if (v == 0) return '0';
    final a = v.abs();
    String text;
    if (a >= 1e6 || a < 1e-4) {
      final exp = (math.log(a) / math.ln10).floor();
      final mant = v / math.pow(10, exp);
      final m = mant.abs() < 1e-9 ? '0' : _trim(mant);
      text = '$m×10^$exp';
    } else {
      text = _trim(v);
    }
    return text.replaceAll('-', '−');
  }

  static String _trim(double v) {
    var s = v.toStringAsFixed(4);
    s = s.replaceFirst(RegExp(r'0+$'), '');
    s = s.replaceFirst(RegExp(r'\.$'), '');
    return s == '-0' ? '0' : s;
  }

  @override
  bool shouldRepaint(covariant _PlotPainter oldDelegate) =>
      oldDelegate.spec != spec ||
      oldDelegate.colors != colors ||
      oldDelegate.textDirection != textDirection;
}

/// Samples a function through the native engine and paints it, keeping the
/// loading/error states local to the chart.
class CurvePlot extends StatefulWidget {
  const CurvePlot({
    super.key,
    required this.engine,
    required this.expression,
    required this.l10n,
    this.variable = 'x',
    this.xmin = -10,
    this.xmax = 10,
    this.samples = 600,
    this.height = 300,
    this.title,
    this.caption,
    this.markers = const [],
    this.asymptotes = const [],
    this.guides = const [],
    this.bands = const [],
    this.extraSeries = const [],
    this.yMin,
    this.yMax,
    this.color,
  });

  final NativeEngine? engine;
  final String expression;
  final L10n l10n;
  final String variable;
  final double xmin;
  final double xmax;
  final int samples;
  final double height;
  final String? title;
  final String? caption;
  final List<PlotMarker> markers;
  final List<double> asymptotes;
  final List<double> guides;
  final List<PlotBand> bands;
  final List<PlotSeries> extraSeries;
  final double? yMin;
  final double? yMax;
  final Color? color;

  @override
  State<CurvePlot> createState() => _CurvePlotState();
}

class _CurvePlotState extends State<CurvePlot> {
  SampleResult? _sample;
  String? _error;
  bool _busy = false;
  int _generation = 0;

  @override
  void initState() {
    super.initState();
    _load();
  }

  @override
  void didUpdateWidget(covariant CurvePlot oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.expression != widget.expression ||
        oldWidget.variable != widget.variable ||
        oldWidget.xmin != widget.xmin ||
        oldWidget.xmax != widget.xmax ||
        oldWidget.engine != widget.engine) {
      _load();
    }
  }

  Future<void> _load() async {
    final engine = widget.engine;
    final generation = ++_generation;
    final expression = widget.expression.trim();
    if (engine == null || expression.isEmpty) {
      setState(() {
        _sample = null;
        _error = null;
        _busy = false;
      });
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final result = await engine.sampleAsync(
        expression,
        x: widget.variable.trim().isEmpty ? 'x' : widget.variable.trim(),
        xmin: widget.xmin,
        xmax: widget.xmax,
        samples: widget.samples,
      );
      if (!mounted || generation != _generation) return;
      setState(() {
        _busy = false;
        if (!result.ok) {
          _sample = null;
          _error = result.error.isEmpty
              ? widget.l10n.t(
                  'Could not sample the function.',
                  'نمونه‌گیری تابع انجام نشد.',
                )
              : result.error;
        } else {
          _sample = result;
        }
      });
    } catch (e) {
      if (!mounted || generation != _generation) return;
      setState(() {
        _busy = false;
        _sample = null;
        _error = widget.l10n.t('Plot error: $e', 'خطای رسم: $e');
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final accent = widget.color ?? c.accent;
    final sample = _sample;
    final spec = PlotSpec(
      series: [
        if (sample != null)
          PlotSeries(
            xs: sample.xs,
            ys: sample.ys,
            color: accent,
            label: 'y = ${widget.expression}',
          ),
        ...widget.extraSeries,
      ],
      markers: widget.markers,
      asymptotes: widget.asymptotes,
      guides: widget.guides,
      bands: widget.bands,
      yMin: widget.yMin,
      yMax: widget.yMax,
      xMin: widget.xmin,
      xMax: widget.xmax,
    );
    return Container(
      decoration: BoxDecoration(
        color: c.card,
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: c.line),
      ),
      padding: const EdgeInsets.all(18),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          if (widget.title != null) ...[
            Row(
              children: [
                Container(
                  width: 3,
                  height: 17,
                  decoration: BoxDecoration(
                    color: accent,
                    borderRadius: BorderRadius.circular(4),
                  ),
                ),
                const SizedBox(width: 9),
                Expanded(
                  child: Text(
                    widget.title!,
                    style: Theme.of(context).textTheme.titleMedium
                        ?.copyWith(fontSize: 14),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
          ],
          if (_busy)
            SizedBox(
              height: widget.height,
              child: Center(
                child: SizedBox(
                  width: 26,
                  height: 26,
                  child: CircularProgressIndicator(
                    strokeWidth: 2.4,
                    color: accent,
                  ),
                ),
              ),
            )
          else if (_error != null)
            SizedBox(
              height: widget.height,
              child: Center(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 18),
                  child: Text(
                    _error!,
                    textAlign: TextAlign.center,
                    style: TextStyle(
                      fontFamily: 'Vazirmatn',
                      color: c.warning,
                      fontSize: 13,
                      height: 1.7,
                    ),
                  ),
                ),
              ),
            )
          else
            MathPlot(
              key: const ValueKey('curve-ready'),
              spec: spec,
              height: widget.height,
              caption: widget.caption,
            ),
        ],
      ),
    );
  }
}

/// Direction field for a first-order equation plus several RK4 trajectories.
class FieldPlot extends StatefulWidget {
  const FieldPlot({
    super.key,
    required this.engine,
    required this.expression,
    required this.l10n,
    this.xVariable = 'x',
    this.yVariable = 'y',
    this.xmin = -6,
    this.xmax = 6,
    this.ymin = -6,
    this.ymax = 6,
    this.cols = 17,
    this.rows = 17,
    this.curves = 6,
    this.height = 330,
    this.title,
    this.caption,
  });

  final NativeEngine? engine;
  final String expression;
  final L10n l10n;
  final String xVariable;
  final String yVariable;
  final double xmin;
  final double xmax;
  final double ymin;
  final double ymax;
  final int cols;
  final int rows;
  final int curves;
  final double height;
  final String? title;
  final String? caption;

  @override
  State<FieldPlot> createState() => _FieldPlotState();
}

class _FieldPlotState extends State<FieldPlot> {
  FieldResult? _field;
  String? _error;
  bool _busy = false;
  int _generation = 0;

  @override
  void initState() {
    super.initState();
    _load();
  }

  @override
  void didUpdateWidget(covariant FieldPlot oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.expression != widget.expression ||
        oldWidget.xVariable != widget.xVariable ||
        oldWidget.yVariable != widget.yVariable ||
        oldWidget.xmin != widget.xmin ||
        oldWidget.xmax != widget.xmax ||
        oldWidget.ymin != widget.ymin ||
        oldWidget.ymax != widget.ymax ||
        oldWidget.engine != widget.engine) {
      _load();
    }
  }

  Future<void> _load() async {
    final engine = widget.engine;
    final generation = ++_generation;
    final expression = widget.expression.trim();
    if (engine == null || expression.isEmpty) {
      setState(() {
        _field = null;
        _busy = false;
        _error = null;
      });
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final result = await engine.fieldAsync(
        expression,
        x: widget.xVariable.trim().isEmpty ? 'x' : widget.xVariable.trim(),
        y: widget.yVariable.trim().isEmpty ? 'y' : widget.yVariable.trim(),
        xmin: widget.xmin,
        xmax: widget.xmax,
        ymin: widget.ymin,
        ymax: widget.ymax,
        cols: widget.cols,
        rows: widget.rows,
        curves: widget.curves,
      );
      if (!mounted || generation != _generation) return;
      setState(() {
        _busy = false;
        if (!result.ok) {
          _field = null;
          _error = result.error.isEmpty
              ? widget.l10n.t(
                  'Could not build the direction field.',
                  'میدان جهت ساخته نشد.',
                )
              : result.error;
        } else {
          _field = result;
        }
      });
    } catch (e) {
      if (!mounted || generation != _generation) return;
      setState(() {
        _busy = false;
        _field = null;
        _error = widget.l10n.t('Plot error: $e', 'خطای رسم: $e');
      });
    }
  }

  PlotSpec _spec(FieldResult field, OdeColors c, L10n l) {
    final arrows = <PlotArrow>[];
    if (field.cols > 1 && field.rows > 1) {
      final dx = (field.xmax - field.xmin) / (field.cols - 1);
      final dy = (field.ymax - field.ymin) / (field.rows - 1);
      for (var r = 0; r < field.rows && r < field.slopes.length; r++) {
        final row = field.slopes[r];
        for (var col = 0; col < field.cols && col < row.length; col++) {
          final slope = row[col];
          if (!slope.isFinite || slope.abs() > 1e4) continue;
          arrows.add(
            PlotArrow(
              x: field.xmin + col * dx,
              y: field.ymin + r * dy,
              slope: slope,
            ),
          );
        }
      }
    }
    final palette = <Color>[
      c.accent,
      c.success,
      c.warning,
      c.accentAlt,
      c.info,
      c.creative,
    ];
    final series = <PlotSeries>[];
    final markers = <PlotMarker>[];
    for (var i = 0; i < field.curves.length; i++) {
      final curve = field.curves[i];
      final color = palette[i % palette.length];
      final label = l.t(
        'Solution through (${_short(curve.icX)}, ${_short(curve.icY)})',
        'جواب گذرنده از (${_short(curve.icX)}, ${_short(curve.icY)})',
      );
      series.add(
        PlotSeries(xs: curve.xs, ys: curve.ys, color: color, label: label),
      );
      markers.add(PlotMarker(x: curve.icX, y: curve.icY, color: color));
    }
    return PlotSpec(
      arrows: arrows,
      series: series,
      markers: markers,
      xMin: field.xmin,
      xMax: field.xmax,
      yMin: field.ymin,
      yMax: field.ymax,
    );
  }

  static String _short(double v) {
    var s = v.toStringAsFixed(2);
    s = s.replaceFirst(RegExp(r'0+$'), '');
    s = s.replaceFirst(RegExp(r'\.$'), '');
    return s;
  }

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final field = _field;
    return Container(
      decoration: BoxDecoration(
        color: c.card,
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: c.line),
      ),
      padding: const EdgeInsets.all(18),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          if (widget.title != null) ...[
            Row(
              children: [
                Container(
                  width: 3,
                  height: 17,
                  decoration: BoxDecoration(
                    color: c.accent,
                    borderRadius: BorderRadius.circular(4),
                  ),
                ),
                const SizedBox(width: 9),
                Expanded(
                  child: Text(
                    widget.title!,
                    style: Theme.of(context).textTheme.titleMedium
                        ?.copyWith(fontSize: 14),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
          ],
          if (field != null && field.rhs.isNotEmpty) ...[
            Directionality(
              textDirection: TextDirection.ltr,
              child: Text(
                '${widget.yVariable}′ = ${field.rhs}',
                style: TextStyle(
                  fontFamily: 'DejaVu Sans Mono',
                  fontFamilyFallback: const ['monospace', 'Vazirmatn'],
                  fontSize: 13.5,
                  color: c.muted,
                ),
              ),
            ),
            const SizedBox(height: 10),
          ],
          if (_busy)
            SizedBox(
              height: widget.height,
              child: Center(
                child: SizedBox(
                  width: 26,
                  height: 26,
                  child: CircularProgressIndicator(
                    strokeWidth: 2.4,
                    color: c.accent,
                  ),
                ),
              ),
            )
          else if (_error != null)
            SizedBox(
              height: widget.height,
              child: Center(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 18),
                  child: Text(
                    _error!,
                    textAlign: TextAlign.center,
                    style: TextStyle(
                      fontFamily: 'Vazirmatn',
                      color: c.warning,
                      fontSize: 13,
                      height: 1.7,
                    ),
                  ),
                ),
              ),
            )
          else if (field != null)
            MathPlot(
              key: const ValueKey('field-ready'),
              spec: _spec(field, c, widget.l10n),
              height: widget.height,
              caption: widget.caption,
            ),
        ],
      ),
    );
  }
}

/// One curve in a [MultiCurvePlot].
class PlotCurveSpec {
  const PlotCurveSpec({
    required this.expression,
    required this.color,
    required this.label,
    this.dashed = false,
    this.width = 2.2,
  });

  final String expression;
  final Color color;
  final String label;
  final bool dashed;
  final double width;
}

/// Draws several sampled curves in one coordinate system. The Cauchy mean value
/// theorem needs f and g side by side, and comparing them is far clearer when
/// they share axes.
class MultiCurvePlot extends StatefulWidget {
  const MultiCurvePlot({
    super.key,
    required this.engine,
    required this.l10n,
    required this.curves,
    required this.xmin,
    required this.xmax,
    this.variable = 'x',
    this.samples = 600,
    this.height = 320,
    this.title,
    this.caption,
    this.markers = const [],
    this.guides = const [],
    this.asymptotes = const [],
    this.extraSeries = const [],
  });

  final NativeEngine? engine;
  final L10n l10n;
  final List<PlotCurveSpec> curves;

  /// Ready-made series drawn on top, for example a tangent computed from the
  /// exact slope rather than sampled from an expression.
  final List<PlotSeries> extraSeries;
  final double xmin;
  final double xmax;
  final String variable;
  final int samples;
  final double height;
  final String? title;
  final String? caption;
  final List<PlotMarker> markers;
  final List<double> guides;
  final List<double> asymptotes;

  @override
  State<MultiCurvePlot> createState() => _MultiCurvePlotState();
}

class _MultiCurvePlotState extends State<MultiCurvePlot> {
  List<SampleResult?> _samples = const [];
  String? _error;
  bool _busy = false;
  int _generation = 0;

  @override
  void initState() {
    super.initState();
    _load();
  }

  @override
  void didUpdateWidget(covariant MultiCurvePlot oldWidget) {
    super.didUpdateWidget(oldWidget);
    final changed =
        oldWidget.xmin != widget.xmin ||
        oldWidget.xmax != widget.xmax ||
        oldWidget.variable != widget.variable ||
        oldWidget.engine != widget.engine ||
        oldWidget.curves.length != widget.curves.length;
    var expressionChanged = false;
    if (!changed) {
      for (var i = 0; i < widget.curves.length; i++) {
        if (oldWidget.curves[i].expression != widget.curves[i].expression) {
          expressionChanged = true;
          break;
        }
      }
    }
    if (changed || expressionChanged) _load();
  }

  Future<void> _load() async {
    final engine = widget.engine;
    final generation = ++_generation;
    if (engine == null) {
      setState(() {
        _samples = const [];
        _busy = false;
        _error = null;
      });
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    final out = <SampleResult?>[];
    String? failure;
    for (final curve in widget.curves) {
      try {
        final r = await engine.sampleAsync(
          curve.expression,
          x: widget.variable,
          xmin: widget.xmin,
          xmax: widget.xmax,
          samples: widget.samples,
        );
        if (!mounted || generation != _generation) return;
        if (!r.ok) {
          failure ??= r.error.isEmpty
              ? widget.l10n.t(
                  'Could not sample the function.',
                  'نمونه‌گیری تابع انجام نشد.',
                )
              : r.error;
          out.add(null);
        } else {
          out.add(r);
        }
      } catch (e) {
        if (!mounted || generation != _generation) return;
        failure ??= widget.l10n.t('Plot error: $e', 'خطای رسم: $e');
        out.add(null);
      }
    }
    if (!mounted || generation != _generation) return;
    setState(() {
      _busy = false;
      _samples = out;
      _error = failure;
    });
  }

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Container(
      decoration: BoxDecoration(
        color: c.card,
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: c.line),
      ),
      padding: const EdgeInsets.all(18),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          if (widget.title != null) ...[
            Row(
              children: [
                Container(
                  width: 3,
                  height: 17,
                  decoration: BoxDecoration(
                    color: c.accent,
                    borderRadius: BorderRadius.circular(4),
                  ),
                ),
                const SizedBox(width: 9),
                Expanded(
                  child: Text(
                    widget.title!,
                    style: Theme.of(
                      context,
                    ).textTheme.titleMedium?.copyWith(fontSize: 14),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
          ],
          if (_busy)
            SizedBox(
              height: widget.height,
              child: Center(
                child: SizedBox(
                  width: 26,
                  height: 26,
                  child: CircularProgressIndicator(
                    strokeWidth: 2.4,
                    color: c.accent,
                  ),
                ),
              ),
            )
          else if (_error != null)
            SizedBox(
              height: widget.height,
              child: Center(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 18),
                  child: Text(
                    _error!,
                    textAlign: TextAlign.center,
                    style: TextStyle(
                      fontFamily: 'Vazirmatn',
                      color: c.warning,
                      fontSize: 13,
                      height: 1.7,
                    ),
                  ),
                ),
              ),
            )
          else
            MathPlot(
              key: const ValueKey('multi-curve-ready'),
              height: widget.height,
              spec: PlotSpec(
                series: [
                  for (var i = 0; i < widget.curves.length; i++)
                    if (i < _samples.length && _samples[i] != null)
                      PlotSeries(
                        xs: _samples[i]!.xs,
                        ys: _samples[i]!.ys,
                        color: widget.curves[i].color,
                        label: widget.curves[i].label,
                        dashed: widget.curves[i].dashed,
                        width: widget.curves[i].width,
                      ),
                  ...widget.extraSeries,
                ],
                markers: widget.markers,
                guides: widget.guides,
                asymptotes: widget.asymptotes,
                xMin: widget.xmin,
                xMax: widget.xmax,
              ),
              caption: widget.caption,
            ),
        ],
      ),
    );
  }
}
