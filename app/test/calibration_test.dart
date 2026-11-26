// Calibration is optional, so the contract is small: it shows one symbol at a
// time, it can be skipped, and it never asks for more than a stroke.
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/scanner/calibration_page.dart';
import 'package:infinit/src/theme.dart';

void main() {
  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  testWidgets('the calibration screen shows the first symbol and a progress', (
    tester,
  ) async {
    final l10n = const L10n(Locale('en'));
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        home: CalibrationPage(l10n: l10n),
      ),
    );
    await tester.pump();

    expect(find.text(calibrationSymbols.first), findsOneWidget);
    expect(
      find.text(
        l10n.t('scan.calib.progress', {
          'current': '1',
          'total': '${calibrationSymbols.length}',
        }),
      ),
      findsOneWidget,
    );
    // Saving needs ink first.
    final save = tester.widget<FilledButton>(
      find.ancestor(
        of: find.text(l10n.t('scan.calib.save')),
        matching: find.byType(FilledButton),
      ),
    );
    expect(save.onPressed, isNull);
  });

  testWidgets('skipping moves to the next symbol', (tester) async {
    final l10n = const L10n(Locale('en'));
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        home: CalibrationPage(l10n: l10n),
      ),
    );
    await tester.pump();

    await tester.tap(find.text(l10n.t('scan.calib.skip')));
    await tester.pump();

    expect(find.text(calibrationSymbols[1]), findsOneWidget);
  });

  testWidgets('the screen is localized in Persian', (tester) async {
    final l10n = const L10n(Locale('fa'));
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: true),
        home: CalibrationPage(l10n: l10n),
      ),
    );
    await tester.pump();
    expect(find.text(l10n.t('scan.calib.title')), findsOneWidget);
  });
}
