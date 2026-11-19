import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/main.dart';
import 'package:infinit/src/app_state.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/settings_store.dart';

Future<AppSettings> _settings() async {
  final settings = AppSettings(persistence: MemorySettingsPersistence());
  await settings.load();
  return settings;
}

Future<void> _awaitEngine(WidgetTester tester, Finder done) async {
  for (var attempt = 0; attempt < 50; attempt++) {
    await tester.runAsync(
      () => Future<void>.delayed(const Duration(milliseconds: 100)),
    );
    await tester.pump();
    if (done.evaluate().isNotEmpty) return;
  }
  fail('The native engine did not finish within 5 seconds.');
}

void main() {
  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  testWidgets('offers a one-click correction for an ambiguous denominator', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2000));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations').last);
    await tester.pumpAndSettle();
    await tester.tap(
      find.descendant(
        of: find.byType(Drawer),
        matching: find.text('Solve an equation'),
      ),
    );
    await tester.pumpAndSettle();

    const ambiguous = "y' = 4xy/ x^2 +1";
    await tester.enterText(find.byType(TextField).first, ambiguous);
    await tester.tap(find.text('Solve equation'));
    await _awaitEngine(tester, find.text('Check the grouping'));

    expect(find.text('Check the grouping'), findsOneWidget);
    expect(
      find.text(
        'Without parentheses this input has two readings. '
        'The engine used standard precedence for the first one.',
      ),
      findsOneWidget,
    );
    expect(find.text('Read as:'), findsOneWidget);
    expect(find.text('Did you mean:'), findsOneWidget);
    expect(find.text("y' = 4xy/(x^2 +1)"), findsOneWidget);

    final apply = find.text('Use the suggested form');
    await tester.ensureVisible(apply);
    await tester.pumpAndSettle();
    await tester.tap(apply);

    // The first reading is linear; the corrected reading is separable, so the
    // type label proves that the second solve actually ran with the fix.
    await _awaitEngine(tester, find.text('Separable'));
    // Let the direction field for the corrected equation finish so the test
    // does not leave the engine timeout timer pending.
    await _awaitEngine(tester, find.byKey(const ValueKey('field-ready')));
    await tester.pumpAndSettle();

    final field = tester.widget<TextField>(find.byType(TextField).first);
    expect(field.controller?.text, "y' = 4xy/(x^2 +1)");
    expect(find.text('Check the grouping'), findsNothing);
  });
}
