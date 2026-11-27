// The camera's confirmation card. Recognition never writes into a field on its
// own, so these tests pin the three outcomes and the "unsure leads with review"
// rule that protects a faint reading from a confident tap.
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/scanner/scan_confirm.dart';
import 'package:infinit/src/scanner_engine.dart';
import 'package:infinit/src/theme.dart';

// A 1x1 transparent PNG, small enough to decode in a widget test.
final _pixel = Uint8List.fromList(const [
  0x89,
  0x50,
  0x4E,
  0x47,
  0x0D,
  0x0A,
  0x1A,
  0x0A,
  0x00,
  0x00,
  0x00,
  0x0D,
  0x49,
  0x48,
  0x44,
  0x52,
  0x00,
  0x00,
  0x00,
  0x01,
  0x00,
  0x00,
  0x00,
  0x01,
  0x08,
  0x06,
  0x00,
  0x00,
  0x00,
  0x1F,
  0x15,
  0xC4,
  0x89,
  0x00,
  0x00,
  0x00,
  0x0A,
  0x49,
  0x44,
  0x41,
  0x54,
  0x78,
  0x9C,
  0x63,
  0x00,
  0x01,
  0x00,
  0x00,
  0x05,
  0x00,
  0x01,
  0x0D,
  0x0A,
  0x2D,
  0xB4,
  0x00,
  0x00,
  0x00,
  0x00,
  0x49,
  0x45,
  0x4E,
  0x44,
  0xAE,
  0x42,
  0x60,
  0x82,
]);

ScanResult reading({
  String expression = 'x+1=2',
  double confidence = 0.9,
  String status = 'ok',
  bool? parseValid = true,
}) {
  return ScanResult(
    expression: expression,
    latex: r'x + 1 = 2',
    kind: 'expression',
    parts: const {},
    confidence: confidence,
    skewDegrees: 0,
    binarization: 'adaptive',
    width: 100,
    height: 60,
    symbols: 5,
    lines: [expression],
    glyphs: const [],
    repaired: false,
    status: status,
    parseValid: parseValid,
    session: 0,
    overrides: const {},
  );
}

/// Keeps the action the sheet returned, because the sheet's future finishes
/// after the tap and a local in the helper would go out of scope.
class _Opened {
  ScanConfirmAction? action;
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  test('a sure reading needs a score, a parse and no unknowns', () {
    expect(isSureReading(reading()), isTrue);
    expect(isSureReading(reading(confidence: 0.3)), isFalse);
    expect(isSureReading(reading(status: 'partial')), isFalse);
    expect(isSureReading(reading(parseValid: false)), isFalse);
    expect(isSureReading(reading(expression: 'x+?=2')), isFalse);
    expect(isSureReading(reading(expression: '  ')), isFalse);
  });

  test('a matrix reading hands back the grid, not the parser text', () {
    final matrix = ScanResult(
      expression: 'matrix([[1,2],[3,4]])',
      latex: r'\begin{pmatrix}1&2\\3&4\end{pmatrix}',
      kind: 'matrix',
      parts: const {'matrix': '1 2\n3 4'},
      confidence: 0.9,
      skewDegrees: 0,
      binarization: 'local',
      width: 100,
      height: 60,
      symbols: 4,
      lines: const [],
      glyphs: const [],
      repaired: false,
      parseValid: true,
      session: 0,
      overrides: const {},
    );
    expect(readingText(matrix), '1 2\n3 4');
  });

  Future<_Opened> open(
    WidgetTester tester,
    ScanResult result,
    L10n l10n,
  ) async {
    final opened = _Opened();
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        home: Builder(
          builder: (context) => Scaffold(
            body: Center(
              child: FilledButton(
                onPressed: () async {
                  opened.action = await showScanConfirm(
                    context,
                    l10n: l10n,
                    result: result,
                    image: _pixel,
                  );
                },
                child: const Text('open'),
              ),
            ),
          ),
        ),
      ),
    );
    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();
    return opened;
  }

  testWidgets('insert is the action for a sure reading', (tester) async {
    final l10n = const L10n(Locale('en'));
    final opened = await open(tester, reading(), l10n);

    expect(find.text(l10n.t('scan.confirm.title')), findsOneWidget);
    expect(find.text(l10n.t('scan.confirm.hint')), findsOneWidget);
    expect(find.text(l10n.t('scan.confirm.unsure')), findsNothing);

    await tester.tap(find.text(l10n.t('scan.confirm.insert')));
    await tester.pumpAndSettle();
    expect(opened.action, ScanConfirmAction.insert);
  });

  testWidgets('a weak reading asks for review before insert', (tester) async {
    final l10n = const L10n(Locale('en'));
    final opened = await open(tester, reading(confidence: 0.2), l10n);

    expect(find.text(l10n.t('scan.confirm.unsure')), findsOneWidget);
    final filled = tester.widget<FilledButton>(
      find.widgetWithText(FilledButton, l10n.t('scan.confirm.review')),
    );
    expect(filled.onPressed, isNotNull);

    await tester.tap(find.text(l10n.t('scan.confirm.review')));
    await tester.pumpAndSettle();
    expect(opened.action, ScanConfirmAction.review);
  });

  testWidgets('retake closes the card without inserting', (tester) async {
    final l10n = const L10n(Locale('en'));
    final opened = await open(tester, reading(), l10n);

    await tester.tap(find.text(l10n.t('scan.confirm.again')));
    await tester.pumpAndSettle();
    expect(opened.action, ScanConfirmAction.again);
  });

  testWidgets('an unreadable capture explains itself and offers review only', (
    tester,
  ) async {
    final l10n = const L10n(Locale('en'));
    await open(tester, reading(expression: '', status: 'failed'), l10n);

    expect(find.text(l10n.t('scan.confirm.failed')), findsOneWidget);
    // Nothing to insert, so the card does not pretend otherwise.
    expect(find.text(l10n.t('scan.confirm.insert')), findsNothing);
    final review = tester.widget<FilledButton>(
      find.widgetWithText(FilledButton, l10n.t('scan.confirm.review')),
    );
    expect(review.onPressed, isNotNull);
  });

  testWidgets('the card is localized in Persian', (tester) async {
    final l10n = const L10n(Locale('fa'));
    await open(tester, reading(), l10n);

    expect(find.text(l10n.t('scan.confirm.title')), findsOneWidget);
    expect(find.text(l10n.t('scan.confirm.insert')), findsOneWidget);
  });
}
