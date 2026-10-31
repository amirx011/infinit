import 'package:flutter/material.dart';
import 'package:flutter_math_fork/flutter_math.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/main.dart';
import 'package:infinit/src/app_state.dart';
import 'package:infinit/src/settings_store.dart';

Future<AppSettings> _settings() async {
  final settings = AppSettings(persistence: MemorySettingsPersistence());
  await settings.load();
  return settings;
}

void main() {
  testWidgets('defaults to dark mode and English', (tester) async {
    final settings = await _settings();
    expect(settings.themeMode, ThemeMode.dark);
    expect(settings.locale.languageCode, 'en');

    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    expect(find.text('Infinit Lab'), findsOneWidget);
    expect(find.text('Every equation has a form.'), findsOneWidget);
    expect(find.text('Derivative'), findsOneWidget);
    expect(find.text('Integral'), findsWidgets);
    expect(find.text('Forms we recognise'), findsOneWidget);
  });

  testWidgets('switches to Persian and back', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await settings.setLocale(const Locale('fa'));
    await tester.pump(const Duration(milliseconds: 300));
    expect(find.text('Infinit Lab'), findsOneWidget);
    expect(find.text('مشتق'), findsOneWidget);

    await settings.setLocale(const Locale('en'));
    await tester.pump(const Duration(milliseconds: 300));
    expect(find.text('Infinit Lab'), findsOneWidget);
  });

  testWidgets('opens the drawer and enters the integral module', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    expect(find.text('Modules'), findsOneWidget);
    expect(find.text('Integral'), findsWidgets);

    await tester.tap(find.text('Differential & integral'));
    await tester.pumpAndSettle();
    await tester.tap(find.descendant(
        of: find.byType(Drawer), matching: find.text('Integral')));
    await tester.pumpAndSettle();
    expect(find.text('Integrals'), findsOneWidget);
  });

  testWidgets('computes an integral from the drawer module', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 1700));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential & integral'));
    await tester.pumpAndSettle();
    await tester.tap(find.descendant(
        of: find.byType(Drawer), matching: find.text('Integral')));
    await tester.pumpAndSettle();

    await tester.enterText(find.byType(TextField).first, '2*x*cos(x^2)');
    await tester.tap(find.text('Compute integral'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 500));

    expect(find.text('Substitution'), findsOneWidget);
    expect(find.textContaining('Differentiate the result'), findsOneWidget);
    expect(find.byType(Math), findsWidgets);
  });

  testWidgets('offers the second derivative after the first', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2000));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential & integral'));
    await tester.pumpAndSettle();
    await tester.tap(find.descendant(
        of: find.byType(Drawer), matching: find.text('Derivative')));
    await tester.pumpAndSettle();

    await tester.tap(find.text('Compute first derivative'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 500));
    expect(find.text('Compute second derivative'), findsOneWidget);

    await tester.tap(find.text('Compute second derivative'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 500));
    expect(find.text('Second derivative'), findsWidgets);
  });

  testWidgets('opens settings and switches theme', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.tune_rounded).first);
    await tester.pumpAndSettle();
    expect(find.text('Settings'), findsOneWidget);
    expect(find.text('Appearance'), findsOneWidget);
    expect(find.text('Language'), findsOneWidget);

    await tester.tap(find.text('Light'));
    await tester.pumpAndSettle();
    expect(settings.themeMode, ThemeMode.light);

    await tester.tap(find.text('فارسی'));
    await tester.pumpAndSettle();
    expect(settings.locale.languageCode, 'fa');
  });

  testWidgets('solves y\u2032 = y through the native engine', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 1700));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations').last);
    await tester.pumpAndSettle();
    await tester.tap(find.descendant(
        of: find.byType(Drawer), matching: find.text('Solve an equation')));
    await tester.pumpAndSettle();

    await tester.enterText(find.byType(TextField).first, "y' = y");
    await tester.tap(find.text('Solve equation'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 500));

    expect(find.text('Explicit solution'), findsOneWidget);
    expect(find.byType(Math), findsWidgets);
    expect(find.text('Solution path'), findsOneWidget);
    expect(find.text('Validation'), findsOneWidget);
  });
}
