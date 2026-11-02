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

/// Native calls run on a background isolate, so widget tests must give the real
/// event loop time to deliver the result before pumping a frame. Poll for the
/// expected widget so slow CI runners do not fail on a fixed delay.
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

Future<void> _drawerTap(WidgetTester tester, String label) async {
  final finder = find.descendant(
    of: find.byType(Drawer),
    matching: find.text(label),
  );
  await tester.scrollUntilVisible(
    finder,
    120,
    scrollable: find.descendant(
      of: find.byType(Drawer),
      matching: find.byType(Scrollable),
    ),
  );
  await tester.pumpAndSettle();
  await tester.tap(finder);
  await tester.pumpAndSettle();
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
    expect(find.text('STUDY NOTE'), findsOneWidget);
    expect(find.text('How to read a solution'), findsOneWidget);
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

  testWidgets('opens the drawer and enters the integral module', (
    tester,
  ) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    expect(find.text('Modules'), findsOneWidget);
    expect(find.text('Integral'), findsWidgets);

    await tester.tap(find.text('Differential & integral'));
    await tester.pumpAndSettle();
    await tester.tap(
      find.descendant(of: find.byType(Drawer), matching: find.text('Integral')),
    );
    await tester.pumpAndSettle();
    expect(find.text('Integrals'), findsOneWidget);
    expect(find.text('STUDY NOTE'), findsOneWidget);
    expect(find.text('Integration is accumulation'), findsOneWidget);

    final expand = find.text('Worked example & pitfalls');
    await tester.ensureVisible(expand);
    await tester.pumpAndSettle();
    await tester.tap(expand);
    await tester.pumpAndSettle();
    expect(find.textContaining('With u = x², du = 2x dx'), findsOneWidget);
  });

  testWidgets('nests applications of the derivative inside the drawer', (
    tester,
  ) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();

    // The new top level section exists.
    expect(find.text('Applications of the derivative'), findsOneWidget);
    expect(find.text('Educational explanations'), findsOneWidget);

    await _drawerTap(tester, 'Differential & integral');
    await _drawerTap(tester, 'Applications of the derivative');
    expect(find.text('Function behaviour'), findsOneWidget);
    expect(find.text('Related rates'), findsOneWidget);
    expect(find.text('Important theorems'), findsOneWidget);

    // The theorems are a nested group inside the applications group.
    await _drawerTap(tester, 'Important theorems');
    expect(find.text("L'Hôpital's rule"), findsOneWidget);
    expect(find.text('Mean value theorem'), findsOneWidget);
    expect(find.text("Rolle's theorem"), findsOneWidget);
    expect(find.text("Cauchy's theorem"), findsOneWidget);
    expect(find.text('Derivative theorems, complete'), findsOneWidget);
  });

  testWidgets('educational section states that it is still empty', (
    tester,
  ) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Educational explanations');
    expect(
      find.text('Nothing has been added to this section yet.'),
      findsOneWidget,
    );
  });

  testWidgets('shows the new sections in Persian', (tester) async {
    final settings = await _settings();
    await settings.setLocale(const Locale('fa'));
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();

    expect(find.text('حساب دیفرانسیل و انتگرال'), findsOneWidget);
    expect(find.text('توضیحات آموزشی'), findsOneWidget);

    await _drawerTap(tester, 'حساب دیفرانسیل و انتگرال');
    await _drawerTap(tester, 'کاربردهای مشتق');
    expect(find.text('تحلیل رفتار تابع'), findsOneWidget);
    expect(find.text('مماس و خط عمود'), findsOneWidget);
    expect(find.text('مسایل بهینه‌سازی'), findsOneWidget);
    expect(find.text('نرخ‌های مرتبط'), findsOneWidget);

    await _drawerTap(tester, 'قضایای مهم');
    expect(find.text('قاعدهٔ هوپیتال'), findsOneWidget);
    expect(find.text('قضیهٔ مقدار میانگین'), findsOneWidget);
    expect(find.text('قضیهٔ رول'), findsOneWidget);
    expect(find.text('قضیهٔ کوشی'), findsOneWidget);
    expect(find.text('قضایای مشتق، کامل'), findsOneWidget);
  });

  testWidgets('analyses function behaviour and draws the plot', (
    tester,
  ) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Differential & integral');
    await _drawerTap(tester, 'Applications of the derivative');
    await _drawerTap(tester, 'Function behaviour');
    expect(find.text('Function behaviour'), findsWidgets);

    final button = find.text('Analyse the function');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();

    // The answer card, the solution path and the CustomPainter plot all appear.
    expect(find.byType(CustomPaint), findsWidgets);
    expect(find.textContaining('Critical point'), findsWidgets);
  });

  testWidgets('computes the tangent and normal lines', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Differential & integral');
    await _drawerTap(tester, 'Applications of the derivative');
    await _drawerTap(tester, 'Tangent & normal');

    final button = find.text('Find the two lines');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();

    // The equations render through Math.tex, so assert on the labels rather
    // than on the TeX source.
    expect(find.text('Tangent line'), findsWidgets);
    expect(find.text('Normal line'), findsWidgets);
  });

  testWidgets('verifies the mean value theorem', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Differential & integral');
    await _drawerTap(tester, 'Applications of the derivative');
    await _drawerTap(tester, 'Important theorems');
    await _drawerTap(tester, 'Mean value theorem');

    final button = find.text('Check the theorem');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();

    // The verdict also lands in the result card, which is on screen after the
    // page scrolls to it.
    expect(find.textContaining('Continuity on [a, b]'), findsWidgets);

    // Scroll back to the top so the page-level theorem check builds.
    await tester.drag(find.byType(Scrollable).first, const Offset(0, 2400));
    await tester.pumpAndSettle();
    expect(find.text('Theorem check'), findsOneWidget);
    expect(find.text('Continuity on [a, b]'), findsWidgets);
    expect(find.text('Differentiability on (a, b)'), findsWidgets);
    expect(find.text('Promised point c'), findsOneWidget);
  });

  testWidgets('renders the complete theorem catalogue', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Differential & integral');
    await _drawerTap(tester, 'Applications of the derivative');
    await _drawerTap(tester, 'Important theorems');
    await _drawerTap(tester, 'Derivative theorems, complete');

    expect(find.text('Theorems of the derivative'), findsOneWidget);
    expect(find.text("Fermat's theorem"), findsOneWidget);
    expect(find.text('Taylor\'s theorem with Lagrange remainder'), findsOneWidget);
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
    await tester.tap(
      find.descendant(of: find.byType(Drawer), matching: find.text('Integral')),
    );
    await tester.pumpAndSettle();

    await tester.enterText(find.byType(TextField).first, '2*x*cos(x^2)');
    await tester.tap(find.text('Compute integral'));
    await _awaitEngine(tester, find.text('Substitution'));

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
    await tester.tap(
      find.descendant(
        of: find.byType(Drawer),
        matching: find.text('Derivative'),
      ),
    );
    await tester.pumpAndSettle();

    await tester.tap(find.text('Compute first derivative'));
    await _awaitEngine(tester, find.text('Compute second derivative'));
    expect(find.text('Compute second derivative'), findsOneWidget);

    await tester.tap(find.text('Compute second derivative'));
    await _awaitEngine(tester, find.text('Second derivative'));
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
    await tester.tap(
      find.descendant(
        of: find.byType(Drawer),
        matching: find.text('Solve an equation'),
      ),
    );
    await tester.pumpAndSettle();

    await tester.enterText(find.byType(TextField).first, "y' = y");
    await tester.tap(find.text('Solve equation'));
    await _awaitEngine(tester, find.text('Explicit solution'));
    await _awaitEngine(tester, find.byKey(const ValueKey('field-ready')));

    expect(find.text('Explicit solution'), findsOneWidget);
    expect(find.byType(Math), findsWidgets);
    expect(find.text('Solution path'), findsOneWidget);
    expect(find.text('Validation'), findsOneWidget);
  });

  testWidgets('plots the direction field for an exact differential form', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 1700));
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

    // This is the equation from the bug report: the solver always answered it,
    // but the direction field used to be skipped because the slope extractor
    // only understood y' and not the M dx + N dy form.
    await tester.enterText(
      find.byType(TextField).first,
      '(x+y)dx + (x+2y)dy = 0',
    );
    await tester.tap(find.text('Solve equation'));
    await _awaitEngine(tester, find.byKey(const ValueKey('field-ready')));

    // The chart painted itself, which only happens when slopes and RK4 curves
    // came back from the engine.
    expect(find.byKey(const ValueKey('field-ready')), findsOneWidget);
    expect(find.text('Direction field & solution family'), findsOneWidget);
    expect(find.text('Solution path'), findsOneWidget);
    expect(find.text('Exactness test'), findsOneWidget);
  });

  testWidgets('finds domain and range and plots the function', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2200));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential & integral'));
    await tester.pumpAndSettle();
    await tester.tap(
      find.descendant(
        of: find.byType(Drawer),
        matching: find.text('Domain & range'),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('Domain & range'), findsWidgets);
    await tester.enterText(find.byType(TextField).first, 'sqrt(4-x^2)');
    await tester.ensureVisible(find.text('Find domain and range'));
    await tester.tap(find.text('Find domain and range'));
    await _awaitEngine(tester, find.text('Range'));

    expect(find.textContaining('[−2, 2]'), findsWidgets);
    expect(find.textContaining('[0, 2]'), findsWidgets);
    expect(find.byKey(const ValueKey('domain-chart')), findsOneWidget);
  });
}
