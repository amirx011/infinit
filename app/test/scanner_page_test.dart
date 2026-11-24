import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/pages/scanner_page.dart';
import 'package:infinit/src/theme.dart';

void main() {
  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  testWidgets('scanner page shows the import controls', (tester) async {
    final l10n = const L10n(Locale('en'));
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        home: Scaffold(
          body: SingleChildScrollView(child: ScannerPage(l10n: l10n)),
        ),
      ),
    );
    await tester.pump();

    expect(find.text(l10n.t('ui.scanner')), findsOneWidget);
    expect(find.text(l10n.t('ui.choose_image')), findsOneWidget);
    expect(find.text(l10n.t('ui.scanner_no_image')), findsOneWidget);
  });

  testWidgets('scanner page is localized in Persian', (tester) async {
    final l10n = const L10n(Locale('fa'));
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: true),
        home: Scaffold(
          body: SingleChildScrollView(child: ScannerPage(l10n: l10n)),
        ),
      ),
    );
    await tester.pump();

    expect(find.text(l10n.t('ui.scanner')), findsOneWidget);
    expect(find.text(l10n.t('ui.choose_image')), findsOneWidget);
  });
}
