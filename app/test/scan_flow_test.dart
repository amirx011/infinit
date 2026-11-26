import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/theme.dart';
import 'package:infinit/src/scanner/scan_flow.dart';

void main() {
  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  testWidgets('the source sheet offers every way in', (tester) async {
    // The test host is Linux, so the camera option is Android-only by design.
    if (Platform.isAndroid) return;
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        locale: const Locale('en'),
        home: Scaffold(
          body: Builder(
            builder: (context) => TextButton(
              onPressed: () => scanExpression(context),
              child: const Text('open'),
            ),
          ),
        ),
      ),
    );
    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();

    final l10n = const L10n(Locale('en'));
    expect(find.text(l10n.t('scan.entry_title')), findsOneWidget);
    expect(find.text(l10n.t('scan.source.gallery')), findsOneWidget);
    expect(find.text(l10n.t('scan.source.handwrite')), findsOneWidget);
    expect(find.text(l10n.t('scan.source.type')), findsOneWidget);
    expect(find.text(l10n.t('scan.source.camera')), findsNothing);

    await tester.tap(find.text(l10n.t('scan.source.type')));
    await tester.pumpAndSettle();
    expect(find.text(l10n.t('scan.entry_title')), findsNothing);
  });

  testWidgets('the sheet is localized in Persian', (tester) async {
    const fa = L10n(Locale('fa'));
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        locale: const Locale('fa'),
        home: Scaffold(
          body: Builder(
            builder: (context) => TextButton(
              onPressed: () => scanExpression(context, l10n: fa),
              child: const Text('open'),
            ),
          ),
        ),
      ),
    );
    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();

    expect(find.text(fa.t('scan.source.gallery')), findsOneWidget);
    expect(find.text(fa.t('scan.source.handwrite')), findsOneWidget);
  });
}
