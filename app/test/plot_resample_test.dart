// A plot must resample when a property that changes the samples changes.
// The bug these tests pin down is a stale chart: the widget rebuilt, but the
// painter kept the old points because `didUpdateWidget` compared too little.
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/charts.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/native_engine.dart';
import 'package:infinit/src/theme.dart';

final _l10n = L10n(const Locale('en'));

Widget _app(Widget child) => MaterialApp(
  theme: buildOdeTheme(dark: false),
  home: Scaffold(body: Center(child: child)),
);

/// Pumps until the plot under [ready] reports [accept] for its spec.
Future<PlotSpec> _awaitSpec(
  WidgetTester tester,
  Key ready,
  bool Function(PlotSpec spec) accept,
) async {
  for (var attempt = 0; attempt < 60; attempt++) {
    await tester.runAsync(
      () => Future<void>.delayed(const Duration(milliseconds: 100)),
    );
    await tester.pump();
    final plots = find.byKey(ready);
    if (plots.evaluate().isEmpty) continue;
    final spec = tester.widget<MathPlot>(plots).spec;
    if (accept(spec)) return spec;
  }
  fail('The plot did not reach the expected state.');
}

void main() {
  setUpAll(() async {
    TestWidgetsFlutterBinding.ensureInitialized();
    I18n.instance = await I18n.load();
  });

  final engine = NativeEngine.instance;

  testWidgets('CurvePlot resamples when the sample count changes', (
    tester,
  ) async {
    Widget curve(int samples) => _app(
      SizedBox(
        width: 420,
        child: CurvePlot(
          engine: engine,
          expression: 'x^2',
          l10n: _l10n,
          xmin: -2,
          xmax: 2,
          samples: samples,
        ),
      ),
    );

    await tester.pumpWidget(curve(80));
    final first = await _awaitSpec(
      tester,
      const ValueKey('curve-ready'),
      (s) => s.series.isNotEmpty && s.series.first.xs.length == 80,
    );
    expect(first.series.first.xs.length, 80);

    await tester.pumpWidget(curve(240));
    final second = await _awaitSpec(
      tester,
      const ValueKey('curve-ready'),
      (s) => s.series.isNotEmpty && s.series.first.xs.length == 240,
    );
    expect(second.series.first.xs.length, 240);
    // The new samples still describe the same function.
    expect(second.series.first.ys.last, closeTo(4, 1e-6));
  });

  testWidgets('MultiCurvePlot resamples when a curve constant changes', (
    tester,
  ) async {
    Widget family(double c) => _app(
      SizedBox(
        width: 420,
        child: MultiCurvePlot(
          engine: engine,
          l10n: _l10n,
          curves: [
            PlotCurveSpec(
              expression: 'c*x',
              color: Colors.blue,
              label: 'c = $c',
              constants: {'c': c},
            ),
          ],
          xmin: -2,
          xmax: 2,
        ),
      ),
    );

    await tester.pumpWidget(family(1));
    final one = await _awaitSpec(
      tester,
      const ValueKey('multi-curve-ready'),
      (s) => s.series.isNotEmpty && s.series.first.ys.last.abs() < 2.5,
    );
    expect(one.series.first.ys.last, closeTo(2, 1e-6));

    await tester.pumpWidget(family(3));
    final three = await _awaitSpec(
      tester,
      const ValueKey('multi-curve-ready'),
      (s) => s.series.isNotEmpty && s.series.first.ys.last.abs() > 2.5,
    );
    expect(three.series.first.ys.last, closeTo(6, 1e-6));
  });

  testWidgets('FieldPlot rebuilds the arrows when the grid changes', (
    tester,
  ) async {
    Widget field(int cols, int rows) => _app(
      SizedBox(
        width: 420,
        child: FieldPlot(
          engine: engine,
          expression: 'x + y',
          l10n: _l10n,
          xmin: -2,
          xmax: 2,
          ymin: -2,
          ymax: 2,
          cols: cols,
          rows: rows,
          curves: 0,
        ),
      ),
    );

    await tester.pumpWidget(field(5, 5));
    final coarse = await _awaitSpec(
      tester,
      const ValueKey('field-ready'),
      (s) => s.arrows.length == 25,
    );
    expect(coarse.arrows.length, 25);

    await tester.pumpWidget(field(9, 7));
    final fine = await _awaitSpec(
      tester,
      const ValueKey('field-ready'),
      (s) => s.arrows.length == 63,
    );
    expect(fine.arrows.length, 63);
  });
}
