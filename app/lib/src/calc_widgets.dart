// Shared building blocks for the "applications of the derivative" pages.
//
// The tangent, behaviour and optimisation pages all need the same three things:
// a line drawn next to a curve, a table of reported points, and a compact row of
// hypothesis checks. Keeping them here means each page reads as its own
// mathematics rather than as chart plumbing.
import 'dart:math' as math;

import 'package:flutter/material.dart';

import 'charts.dart';
import 'l10n.dart';
import 'models.dart';
import 'theme.dart';
import 'widgets.dart';

/// Samples the straight line y = m x + b across the plot window. The tangent and
/// normal pages draw their lines from the exact symbolic slope the engine
/// returned, so the picture matches the algebra.
PlotSeries sampleLine({
  required double slope,
  required double intercept,
  required double xmin,
  required double xmax,
  required Color color,
  required String label,
  int samples = 64,
  bool dashed = false,
  double width = 2,
}) {
  final xs = <double>[];
  final ys = <double>[];
  if (!slope.isFinite || !intercept.isFinite || !(xmax > xmin)) {
    return PlotSeries(
      xs: xs,
      ys: ys,
      color: color,
      label: label,
      dashed: dashed,
      width: width,
    );
  }
  for (var i = 0; i <= samples; i++) {
    final x = xmin + (xmax - xmin) * i / samples;
    xs.add(x);
    ys.add(slope * x + intercept);
  }
  return PlotSeries(
    xs: xs,
    ys: ys,
    color: color,
    label: label,
    dashed: dashed,
    width: width,
  );
}

/// A compact table of the points an analysis reported.
class PointTable extends StatelessWidget {
  const PointTable({
    super.key,
    required this.points,
    required this.l10n,
    this.kindLabels = const {},
  });

  final List<PointView> points;
  final L10n l10n;

  /// Maps the engine's machine kind to a translated label.
  final Map<String, String> kindLabels;

  Color _tone(String kind, OdeColors c) {
    switch (kind) {
      case 'maximum':
        return c.success;
      case 'minimum':
        return c.warning;
      case 'inflection':
        return c.info;
      default:
        return c.accentAlt;
    }
  }

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    if (points.isEmpty) return const SizedBox.shrink();
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        for (var i = 0; i < points.length; i++) ...[
          if (i > 0) const SizedBox(height: 7),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 9),
            decoration: BoxDecoration(
              color: c.cardAlt,
              borderRadius: BorderRadius.circular(12),
              border: Border.all(color: c.line),
            ),
            child: Row(
              children: [
                Container(
                  width: 7,
                  height: 7,
                  decoration: BoxDecoration(
                    color: _tone(points[i].kind, c),
                    shape: BoxShape.circle,
                  ),
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: MathInline(
                    math: ExView(
                      plain:
                          'x = ${points[i].x?.plain ?? '?'}'
                          '${points[i].value == null ? '' : '   f(x) = ${points[i].value!.plain}'}',
                      pretty:
                          'x = ${points[i].x?.pretty ?? '?'}'
                          '${points[i].value == null ? '' : '   f(x) = ${points[i].value!.pretty}'}',
                      latex:
                          'x = ${points[i].x?.latex ?? '?'}'
                          '${points[i].value == null ? '' : ', \\quad f(x) = ${points[i].value!.latex}'}',
                    ),
                    size: 14,
                  ),
                ),
                const SizedBox(width: 8),
                Text(
                  kindLabels[points[i].kind] ?? points[i].kind,
                  style: TextStyle(
                    fontFamily: 'Vazirmatn',
                    fontSize: 11.5,
                    color: _tone(points[i].kind, c),
                  ),
                ),
              ],
            ),
          ),
        ],
      ],
    );
  }
}

/// The rising/falling picture of the function as a single readable strip.
class MonotoneStrip extends StatelessWidget {
  const MonotoneStrip({super.key, required this.pieces, required this.l10n});

  final List<MonotonePieceView> pieces;
  final L10n l10n;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    if (pieces.isEmpty) return const SizedBox.shrink();
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        for (var i = 0; i < pieces.length; i++) ...[
          if (i > 0) const SizedBox(height: 6),
          Row(
            children: [
              Icon(
                pieces[i].sign > 0
                    ? Icons.trending_up_rounded
                    : pieces[i].sign < 0
                    ? Icons.trending_down_rounded
                    : Icons.trending_flat_rounded,
                size: 17,
                color: pieces[i].sign > 0
                    ? c.success
                    : pieces[i].sign < 0
                    ? c.warning
                    : c.muted,
              ),
              const SizedBox(width: 9),
              Expanded(
                child: Text(
                  '[${_short(pieces[i].lo)}, ${_short(pieces[i].hi)}]  '
                  '${pieces[i].trend}',
                  textDirection: TextDirection.ltr,
                  textAlign: TextAlign.left,
                  style: TextStyle(
                    fontFamily: 'DejaVu Sans Mono',
                    fontFamilyFallback: const ['monospace', 'Vazirmatn'],
                    fontSize: 12.5,
                    color: c.text,
                  ),
                ),
              ),
            ],
          ),
        ],
      ],
    );
  }

  static String _short(double v) {
    if (!v.isFinite) return v < 0 ? '-∞' : '∞';
    var s = v.toStringAsFixed(3);
    s = s.replaceFirst(RegExp(r'0+$'), '');
    s = s.replaceFirst(RegExp(r'\.$'), '');
    return s.replaceAll('-', '−');
  }
}

/// A row of pass/fail hypothesis checks, used by the mean value theorems.
class HypothesisRow extends StatelessWidget {
  const HypothesisRow({super.key, required this.check, required this.l10n});

  final CheckView check;
  final L10n l10n;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(
            check.ok ? Icons.check_circle_rounded : Icons.cancel_rounded,
            size: 18,
            color: check.ok ? c.success : c.warning,
          ),
          const SizedBox(width: 9),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  check.name,
                  style: Theme.of(
                    context,
                  ).textTheme.bodyMedium?.copyWith(fontWeight: FontWeight.w600),
                ),
                if (check.detail.isNotEmpty)
                  Text(
                    check.detail,
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

/// The label + editable field row used by every applications page.
class LabelledField extends StatelessWidget {
  const LabelledField({
    super.key,
    required this.controller,
    required this.label,
    this.hint,
    this.width = 130,
    this.onSubmitted,
  });

  final TextEditingController controller;
  final String label;
  final String? hint;
  final double width;
  final VoidCallback? onSubmitted;

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: width,
      child: TextField(
        controller: controller,
        textDirection: TextDirection.ltr,
        textAlign: TextAlign.left,
        style: const TextStyle(fontFamily: 'monospace', fontSize: 14),
        decoration: InputDecoration(
          labelText: label,
          hintText: hint,
          isDense: true,
        ),
        onSubmitted: onSubmitted == null ? null : (_) => onSubmitted!(),
      ),
    );
  }
}

/// The monospace expression input shared by the applications pages.
class ExpressionField extends StatelessWidget {
  const ExpressionField({
    super.key,
    required this.controller,
    required this.hint,
    this.onSubmitted,
  });

  final TextEditingController controller;
  final String hint;
  final VoidCallback? onSubmitted;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return TextField(
      controller: controller,
      textDirection: TextDirection.ltr,
      textAlign: TextAlign.left,
      style: const TextStyle(
        fontFamily: 'DejaVu Sans Mono',
        fontFamilyFallback: ['monospace', 'Vazirmatn'],
        fontSize: 16,
        height: 1.5,
      ),
      decoration: InputDecoration(
        hintText: hint,
        prefixIcon: Icon(Icons.edit_outlined, size: 20, color: c.muted),
      ),
      onSubmitted: onSubmitted == null ? null : (_) => onSubmitted!(),
    );
  }
}

/// The shared examples row.
class ExampleRow extends StatelessWidget {
  const ExampleRow({
    super.key,
    required this.examples,
    required this.l10n,
    required this.onPick,
  });

  final List<String> examples;
  final L10n l10n;
  final ValueChanged<String> onPick;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          l10n.t('Examples', 'نمونه‌ها'),
          style: Theme.of(context).textTheme.bodySmall,
        ),
        const SizedBox(height: 8),
        Wrap(
          spacing: 7,
          runSpacing: 7,
          children: [
            for (final ex in examples)
              ExampleChip(label: ex, onTap: () => onPick(ex)),
          ],
        ),
      ],
    );
  }
}

/// The standard error card.
class ErrorCard extends StatelessWidget {
  const ErrorCard({super.key, required this.message});

  final String message;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return SoftCard(
      color: c.warningSoft,
      borderColor: c.warning.withValues(alpha: 0.4),
      padding: const EdgeInsets.all(16),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(Icons.error_outline_rounded, color: c.warning, size: 20),
          const SizedBox(width: 10),
          Expanded(
            child: SelectableText(
              message,
              style: Theme.of(context).textTheme.bodyMedium,
            ),
          ),
        ],
      ),
    );
  }
}

/// Formats a real number for the axis labels and captions.
String formatReal(double v) {
  if (!v.isFinite) return v < 0 ? '−∞' : '∞';
  if (v.abs() < 1e-12) return '0';
  var s = v.toStringAsFixed(4);
  s = s.replaceFirst(RegExp(r'0+$'), '');
  s = s.replaceFirst(RegExp(r'\.$'), '');
  return s.replaceAll('-', '−');
}

/// Convert an [ExView] to a double when it is a plain number, else null.
double? exToDouble(ExView? e) {
  if (e == null) return null;
  return double.tryParse(e.plain.replaceAll('−', '-'));
}

/// Chooses a symmetric plot window around the values a page cares about.
(double, double) plotWindowAround(Iterable<double> values, {double fallback = 6}) {
  final finite = values.where((v) => v.isFinite).toList();
  if (finite.isEmpty) return (-fallback, fallback);
  final lo = finite.reduce(math.min);
  final hi = finite.reduce(math.max);
  final span = math.max(hi - lo, 1);
  return (lo - span * 0.6, hi + span * 0.6);
}
