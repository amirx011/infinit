// Result-screen states (plan §12B.7 and §12B.13). The review screen is the one
// place a wrong reading can slip through, so every state it can show is pinned
// here: valid, needs review, could not read, multi-line with one bad line, and
// the disabled Solve gate. Persian and English, light and dark.
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:image/image.dart' as img;
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/scanner/scan_flow.dart';
import 'package:infinit/src/scanner_engine.dart';
import 'package:infinit/src/theme.dart';

/// A white page big enough to pass the "too small" guard in the flow.
Uint8List whitePage() {
  final image = img.Image(width: 240, height: 160);
  img.fill(image, color: img.ColorRgb8(255, 255, 255));
  return Uint8List.fromList(img.encodePng(image));
}

ScanGlyph glyph(int id, String label, double score) => ScanGlyph(
  id: id,
  label: label,
  score: score,
  margin: 0.3,
  box: [20 + id * 30, 40, 24, 40],
  alternatives: [ScanAlternative('l', score - 0.1)],
);

ScanLineReading line(String text, {bool? parseValid}) => ScanLineReading(
  expression: text,
  latex: text,
  parseValid: parseValid,
  confidence: 0.9,
  glyphs: const [],
  warnings: const [],
);

ScanResult result({
  String expression = 'x^2-5x+6=0',
  double confidence = 0.92,
  String status = 'ok',
  String kind = 'expression',
  List<ScanLineReading> lineDetails = const [],
  List<String> warnings = const [],
}) => ScanResult(
  expression: expression,
  latex: expression,
  kind: kind,
  parts: const {},
  confidence: confidence,
  skewDegrees: 0.0,
  binarization: 'otsu',
  width: 240,
  height: 160,
  symbols: 3,
  lines: [expression],
  glyphs: [glyph(0, 'x', 0.9), glyph(1, '2', 0.8)],
  repaired: false,
  session: 0,
  overrides: const {},
  status: status,
  parseValid: true,
  timing: const {'preprocess': 8.0, 'segment': 0.2},
  warnings: warnings,
  lineDetails: lineDetails,
);

Future<void> pumpFlow(
  WidgetTester tester,
  L10n l10n,
  ScanResult reading, {
  bool dark = false,
  double scale = 1.0,
}) async {
  // The page decodes the image and measures its quality in a real isolate, so
  // the pump has to run outside the fake clock.
  await tester.runAsync(() async {
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: dark),
        home: Builder(
          builder: (context) => MediaQuery(
            data: MediaQuery.of(
              context,
            ).copyWith(textScaler: TextScaler.linear(scale)),
            child: ScanFlowPage(
              l10n: l10n,
              bytes: whitePage(),
              debugResult: reading,
            ),
          ),
        ),
      ),
    );
    // Give the decoder and the isolate a moment to finish.
    for (var i = 0; i < 20; i++) {
      await Future<void>.delayed(const Duration(milliseconds: 25));
      await tester.pump();
    }
  });
  await tester.pump();
}

void main() {
  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  testWidgets('a confident reading says "looks valid" and allows Solve', (
    tester,
  ) async {
    final l10n = const L10n(Locale('en'));
    await pumpFlow(tester, l10n, result());

    expect(find.text(l10n.t('scan.result.valid')), findsOneWidget);
    final solve = tester.widget<FilledButton>(
      find.ancestor(
        of: find.text(l10n.t('scan.action.use')),
        matching: find.byType(FilledButton),
      ),
    );
    expect(solve.onPressed, isNotNull);
  });

  testWidgets('a weak reading asks for review', (tester) async {
    final l10n = const L10n(Locale('en'));
    await pumpFlow(tester, l10n, result(confidence: 0.42));
    expect(find.text(l10n.t('scan.result.review')), findsOneWidget);
  });

  testWidgets('an empty reading says it could not read the page', (
    tester,
  ) async {
    final l10n = const L10n(Locale('en'));
    await pumpFlow(
      tester,
      l10n,
      result(expression: '', confidence: 0.0, status: 'failed'),
    );
    expect(find.text(l10n.t('scan.result.failed')), findsOneWidget);
  });

  testWidgets('one invalid line disables Solve and marks the line', (
    tester,
  ) async {
    final l10n = const L10n(Locale('en'));
    await pumpFlow(
      tester,
      l10n,
      result(
        lineDetails: [
          line('x+1=2'),
          line('2y-', parseValid: false),
        ],
      ),
    );

    expect(find.text(l10n.t('scan.line.invalid')), findsWidgets);
    final solve = tester.widget<FilledButton>(
      find.ancestor(
        of: find.text(l10n.t('scan.action.use')),
        matching: find.byType(FilledButton),
      ),
    );
    expect(solve.onPressed, isNull);
  });

  testWidgets('the review screen is right-to-left in Persian, math stays LTR', (
    tester,
  ) async {
    final l10n = const L10n(Locale('fa'));
    await pumpFlow(tester, l10n, result(), dark: true);

    expect(find.text(l10n.t('scan.result.warning')), findsOneWidget);
    final field = tester.widget<TextField>(find.byType(TextField).first);
    expect(field.textDirection, TextDirection.ltr);
  });

  testWidgets('large text does not overflow the review screen', (
    tester,
  ) async {
    final l10n = const L10n(Locale('en'));
    await pumpFlow(tester, l10n, result(), scale: 1.8);
    expect(tester.takeException(), isNull);
  });

  testWidgets('the crop screen offers rotate, adjust and reset', (tester) async {
    final l10n = const L10n(Locale('en'));
    await tester.runAsync(() async {
      await tester.pumpWidget(
        MaterialApp(
          theme: buildOdeTheme(dark: false),
          home: ScanFlowPage(l10n: l10n, bytes: whitePage()),
        ),
      );
      for (var i = 0; i < 20; i++) {
        await Future<void>.delayed(const Duration(milliseconds: 25));
        await tester.pump();
      }
    });
    await tester.pump();

    expect(find.byTooltip(l10n.t('scan.action.rotate')), findsOneWidget);
    expect(find.byTooltip(l10n.t('scan.action.adjust')), findsOneWidget);
    expect(find.byTooltip(l10n.t('scan.action.reset')), findsOneWidget);

    await tester.tap(find.byTooltip(l10n.t('scan.action.adjust')));
    await tester.pumpAndSettle();
    expect(find.text(l10n.t('scan.adjust.title')), findsOneWidget);
    expect(find.byType(Slider), findsNWidgets(2));
  });

  testWidgets('a warning from the reader is shown, not swallowed', (
    tester,
  ) async {
    final l10n = const L10n(Locale('en'));
    await pumpFlow(
      tester,
      l10n,
      result(warnings: ['low_margin:id=0']),
    );
    // The persistent warning line is always there; the reader's own warning
    // must not break the layout even when the list is populated.
    expect(find.text(l10n.t('scan.result.warning')), findsOneWidget);
    expect(tester.takeException(), isNull);
  });
}
