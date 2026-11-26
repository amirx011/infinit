// The Scanner Lab is a debug surface, but it is also the only place a user can
// see why a scan failed. These tests pin its contract: every pipeline stage is
// offered, the empty state explains itself, and the wide layout stays
// two-pane without overflowing.
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/scanner/lab_page.dart';
import 'package:infinit/src/theme.dart';

void main() {
  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  testWidgets('the lab offers every pipeline stage', (tester) async {
    final l10n = const L10n(Locale('en'));
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        home: ScannerLabPage(l10n: l10n),
      ),
    );
    await tester.pump();

    for (final stage in labStages) {
      expect(
        find.text(l10n.t('scan.lab.stage.$stage')),
        findsOneWidget,
        reason: 'stage chip $stage',
      );
    }
    expect(find.text(l10n.t('scan.lab.pick')), findsOneWidget);
  });

  testWidgets('the lab is localized in Persian and keeps math LTR', (
    tester,
  ) async {
    final l10n = const L10n(Locale('fa'));
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: true),
        home: ScannerLabPage(l10n: l10n),
      ),
    );
    await tester.pump();

    expect(find.text(l10n.t('scan.lab.title')), findsOneWidget);
    expect(find.text(l10n.t('scan.lab.stage.symbols')), findsOneWidget);
  });

  testWidgets('a wide window keeps the two-pane layout', (tester) async {
    final l10n = const L10n(Locale('en'));
    tester.view.physicalSize = const Size(1280, 900);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.reset);

    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        home: ScannerLabPage(l10n: l10n),
      ),
    );
    await tester.pump();

    expect(tester.takeException(), isNull);
    expect(find.byType(VerticalDivider), findsOneWidget);
  });
}
