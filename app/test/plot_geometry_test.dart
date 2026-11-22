// Geometry behind the CustomPainter plots: which window is drawn, where the
// ticks land and how their labels read. These are pure functions, so a plain
// unit test covers behaviour that would otherwise only be visible on screen.
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/charts.dart';

const _ink = Color(0xFF000000);

PlotSeries _series(List<double> xs, List<double> ys) =>
    PlotSeries(xs: xs, ys: ys, color: _ink);

void main() {
  group('resolvePlotBounds', () {
    test('pins the requested x sides and pads only the free one', () {
      final spec = PlotSpec(
        series: [_series([0, 5, 10], [0, 1, 0])],
        xMin: 2,
      );
      final w = resolvePlotBounds(spec)!;
      // xMin stays, xMax comes from the data, and 3 % padding is added.
      expect(w.x0, closeTo(2 - 0.03 * 8, 1e-9));
      expect(w.x1, closeTo(10 + 0.03 * 8, 1e-9));
    });

    test('a pinned xMax without xMin still uses the data on the left', () {
      final spec = PlotSpec(
        series: [_series([0, 5, 10], [0, 1, 0])],
        xMax: 8,
      );
      final w = resolvePlotBounds(spec)!;
      expect(w.x0, closeTo(0 - 0.03 * 8, 1e-9));
      expect(w.x1, closeTo(8 + 0.03 * 8, 1e-9));
    });

    test('a pinned yMax keeps its value while yMin comes from the data', () {
      final spec = PlotSpec(
        series: [_series([0, 1, 2], [-3, 0, 4])],
        yMax: 2,
      );
      final w = resolvePlotBounds(spec)!;
      expect(w.y0, closeTo(-3.3, 1e-9));
      expect(w.y1, closeTo(2.3, 1e-9));
    });

    test('a degenerate explicit window is rejected, not silently widened', () {
      final spec = PlotSpec(
        series: [_series([0, 1], [0, 1])],
        xMin: 1,
        xMax: 1,
      );
      expect(resolvePlotBounds(spec), isNull);
    });

    test('a pole does not flatten the interesting part of the range', () {
      final xs = <double>[];
      final ys = <double>[];
      for (var i = 0; i < 100; i++) {
        xs.add(i.toDouble());
        ys.add(i == 0 ? 1e6 : (i == 1 ? -1e6 : math.sin(i.toDouble())));
      }
      final w = resolvePlotBounds(PlotSpec(series: [_series(xs, ys)]))!;
      expect(w.ySpan, lessThan(100));
      expect(w.y0, lessThan(0));
      expect(w.y1, greaterThan(0));
    });

    test('explicit y limits are never trimmed', () {
      final xs = <double>[];
      final ys = <double>[];
      for (var i = 0; i < 100; i++) {
        xs.add(i.toDouble());
        ys.add(i == 0 ? 1e6 : math.sin(i.toDouble()));
      }
      final w = resolvePlotBounds(
        PlotSpec(series: [_series(xs, ys)], yMin: -2, yMax: 2),
      )!;
      expect(w.y0, lessThan(-2));
      expect(w.y1, greaterThan(2));
      expect(w.ySpan, closeTo(4 * 1.12, 1e-9));
    });

    test('an empty spec has no window', () {
      expect(resolvePlotBounds(const PlotSpec()), isNull);
    });
  });

  group('plotTicks', () {
    test('stays inside the window and uses a readable step', () {
      final ticks = plotTicks(-10, 10);
      expect(ticks.first, greaterThanOrEqualTo(-10));
      expect(ticks.last, lessThanOrEqualTo(10));
      expect(ticks, contains(0));
      expect(ticks, [-10, -5, 0, 5, 10]);
      for (var i = 1; i < ticks.length; i++) {
        expect(ticks[i] - ticks[i - 1], closeTo(5, 1e-9));
      }
    });

    test('a wide window still gets enough ticks to read', () {
      final ticks = plotTicks(-8.24, 8.24);
      expect(ticks.length, greaterThanOrEqualTo(6));
      for (var i = 1; i < ticks.length; i++) {
        expect(ticks[i] - ticks[i - 1], closeTo(2.5, 1e-9));
      }
    });

    test('rejects a degenerate window', () {
      expect(plotTicks(3, 3), isEmpty);
      expect(plotTicks(3, 1), isEmpty);
      expect(plotTicks(double.nan, 1), isEmpty);
    });

    test('keeps the tick count bounded on a huge window', () {
      expect(plotTicks(-1e12, 1e12).length, lessThanOrEqualTo(24));
    });
  });

  group('robustValueRange', () {
    test('leaves well-behaved data alone', () {
      final range = robustValueRange([for (var i = 0; i <= 100; i++) i / 5])!;
      expect(range.lo, 0);
      expect(range.hi, 20);
    });

    test('ignores a pole when it dwarfs the rest', () {
      final values = [
        for (var i = 0; i < 200; i++) math.sin(i.toDouble()),
        1e7,
        -1e7,
      ];
      final range = robustValueRange(values)!;
      expect(range.hi, lessThan(100));
      expect(range.lo, greaterThan(-100));
    });

    test('skips non-finite samples and reports empty input', () {
      expect(robustValueRange([double.nan, double.infinity]), isNull);
      final range = robustValueRange([double.nan, 1, 3])!;
      expect(range.lo, 1);
      expect(range.hi, 3);
    });
  });

  group('plotTickText', () {
    test('splits a scientific label into mantissa and exponent', () {
      final big = plotTickText(2000000);
      expect(big.body, '2');
      expect(big.exponent, 6);

      final small = plotTickText(0.00002);
      expect(small.body, '2');
      expect(small.exponent, -5);
    });

    test('uses the mathematical minus sign and trims zeros', () {
      final negative = plotTickText(-2.5);
      expect(negative.body, '−2.5');
      expect(negative.exponent, isNull);
      expect(plotTickText(1.0).body, '1');
      expect(plotTickText(0).body, '0');
    });
  });
}
