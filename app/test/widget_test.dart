import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_math_fork/flutter_math.dart';
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

class _RecordingPersistence extends MemorySettingsPersistence {
  String? launcherPalette;

  @override
  Future<void> setLauncherPalette(String palette) async {
    launcherPalette = palette;
  }
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
  final list = find.descendant(
    of: find.byType(Drawer),
    matching: find.byType(ListView),
  );
  await tester.scrollUntilVisible(
    finder,
    120,
    scrollable: find.descendant(of: list, matching: find.byType(Scrollable)),
  );
  await tester.pumpAndSettle();
  await tester.tap(finder);
  await tester.pumpAndSettle();
}

void main() {
  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  testWidgets('defaults to dark mode and English', (tester) async {
    final settings = await _settings();
    expect(settings.themeMode, ThemeMode.dark);
    expect(settings.locale.languageCode, 'en');
    expect(settings.palette, 'rose');

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
    expect(find.text('\u0645\u0634\u062a\u0642'), findsOneWidget);

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

  testWidgets('opens the dedicated orthogonal-trajectories page', (
    tester,
  ) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations'));
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Applications');
    await _drawerTap(tester, 'Orthogonal trajectories');

    expect(find.text('Orthogonal trajectories'), findsOneWidget);
    expect(find.text('Find orthogonal trajectories'), findsOneWidget);
    expect(find.byType(SwitchListTile), findsNothing);
    expect(find.textContaining('replace y′ by −1/y′'), findsOneWidget);

    await tester.tap(find.text('Find orthogonal trajectories'));
    await _awaitEngine(tester, find.text('Orthogonal family'));
    expect(find.text('Orthogonal trajectories'), findsWidgets);
    await _awaitEngine(tester, find.byKey(const ValueKey('field-ready')));
    expect(find.textContaining('direction field'), findsOneWidget);
  });

  testWidgets('opens the dedicated oblique-trajectories page', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations'));
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Applications');
    await _drawerTap(tester, 'Oblique trajectories');

    expect(find.text('Oblique trajectories'), findsOneWidget);
    expect(find.text('Find oblique trajectories'), findsOneWidget);
    expect(find.text('Crossing angle α (degrees)'), findsOneWidget);
    expect(find.byType(SwitchListTile), findsNothing);
    expect(find.textContaining('constant angle'), findsWidgets);

    await tester.tap(find.text('Find oblique trajectories'));
    await _awaitEngine(tester, find.text('Oblique family'));
    expect(find.text('Oblique trajectories'), findsWidgets);
    await _awaitEngine(tester, find.byKey(const ValueKey('field-ready')));
    expect(find.textContaining('direction field'), findsOneWidget);
  });

  testWidgets('nests applications of the derivative inside the drawer', (
    tester,
  ) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();

    // The new top level section exists. Nested groups only appear once their
    // parent is open.
    expect(find.text('Educational explanations'), findsOneWidget);

    await _drawerTap(tester, 'Differential & integral');
    expect(find.text('Applications of the derivative'), findsOneWidget);
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

  testWidgets('educational section opens the derivative article', (
    tester,
  ) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Educational explanations');
    await _drawerTap(tester, 'Derivative');

    // The article shows the simplest answer first, then the numbered solution.
    expect(find.text('Short answer'), findsOneWidget);
    expect(find.text('Simplest answer'), findsOneWidget);
    expect(find.text('Step by step'), findsOneWidget);
    expect(find.textContaining('lim'), findsWidgets);
  });

  testWidgets('linear algebra sits just before the educational section', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2200));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();

    final linear = find.descendant(
      of: find.byType(Drawer),
      matching: find.text('Linear algebra'),
    );
    final education = find.descendant(
      of: find.byType(Drawer),
      matching: find.text('Educational explanations'),
    );
    expect(linear, findsOneWidget);
    expect(education, findsOneWidget);
    expect(
      tester.getTopLeft(linear).dy,
      lessThan(tester.getTopLeft(education).dy),
    );
  });

  testWidgets('filters modules from the drawer search field', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();

    final search = find.descendant(
      of: find.byType(Drawer),
      matching: find.byType(TextField),
    );
    await tester.enterText(search, 'bessel');
    await tester.pumpAndSettle();

    final result = find.descendant(
      of: find.byType(Drawer),
      matching: find.text('Bessel equations & functions'),
    );
    expect(result, findsOneWidget);
    expect(
      find.descendant(
        of: find.byType(Drawer),
        matching: find.text('Derivative'),
      ),
      findsNothing,
    );

    await tester.tap(result);
    await tester.pumpAndSettle();
    expect(find.text('Coming soon'), findsOneWidget);
  });

  testWidgets('shows the current path in the drawer breadcrumb', (
    tester,
  ) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations'));
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Applications');
    await _drawerTap(tester, 'Wronskian');

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();

    final trail = find.byWidgetPredicate(
      (widget) =>
          widget is Text &&
          RegExp(r'Differential equations\s*/\s*Applications\s*/\s*Wronskian')
              .hasMatch(widget.textSpan?.toPlainText() ?? ''),
    );
    expect(trail, findsOneWidget);
  });

  testWidgets('shows the new sections in Persian', (tester) async {
    final settings = await _settings();
    await settings.setLocale(const Locale('fa'));
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();

    expect(find.text('\u062d\u0633\u0627\u0628 \u062f\u06cc\u0641\u0631\u0627\u0646\u0633\u06cc\u0644 \u0648 \u0627\u0646\u062a\u06af\u0631\u0627\u0644'), findsOneWidget);
    expect(find.text('\u062a\u0648\u0636\u06cc\u062d\u0627\u062a \u0622\u0645\u0648\u0632\u0634\u06cc'), findsOneWidget);

    await _drawerTap(tester, '\u062d\u0633\u0627\u0628 \u062f\u06cc\u0641\u0631\u0627\u0646\u0633\u06cc\u0644 \u0648 \u0627\u0646\u062a\u06af\u0631\u0627\u0644');
    await _drawerTap(tester, '\u06a9\u0627\u0631\u0628\u0631\u062f\u0647\u0627\u06cc \u0645\u0634\u062a\u0642');
    expect(find.text('\u062a\u062d\u0644\u06cc\u0644 \u0631\u0641\u062a\u0627\u0631 \u062a\u0627\u0628\u0639'), findsOneWidget);
    expect(find.text('\u0645\u0645\u0627\u0633 \u0648 \u062e\u0637 \u0639\u0645\u0648\u062f'), findsOneWidget);
    expect(find.text('\u0645\u0633\u0627\u06cc\u0644 \u0628\u0647\u06cc\u0646\u0647\u200c\u0633\u0627\u0632\u06cc'), findsOneWidget);
    expect(find.text('\u0646\u0631\u062e\u200c\u0647\u0627\u06cc \u0645\u0631\u062a\u0628\u0637'), findsOneWidget);
    await _drawerTap(tester, '\u0642\u0636\u0627\u06cc\u0627\u06cc \u0645\u0647\u0645');
    expect(find.text('\u0642\u0627\u0639\u062f\u0647\u0654 \u0647\u0648\u067e\u06cc\u062a\u0627\u0644'), findsOneWidget);
    expect(find.text('\u0642\u0636\u06cc\u0647\u0654 \u0645\u0642\u062f\u0627\u0631 \u0645\u06cc\u0627\u0646\u06af\u06cc\u0646'), findsOneWidget);
    expect(find.text('\u0642\u0636\u06cc\u0647\u0654 \u0631\u0648\u0644'), findsOneWidget);
    expect(find.text('\u0642\u0636\u06cc\u0647\u0654 \u06a9\u0648\u0634\u06cc'), findsOneWidget);
    expect(find.text('\u0642\u0636\u0627\u06cc\u0627\u06cc \u0645\u0634\u062a\u0642\u060c \u06a9\u0627\u0645\u0644'), findsOneWidget);
  });

  testWidgets('opens the systems of ODEs workbench', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2000));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations').last);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Systems & stability');
    await _drawerTap(tester, 'Systems of ODEs');

    expect(find.text('Systems of ODEs'), findsWidgets);
    expect(find.text('Show the answer and steps'), findsOneWidget);
    expect(find.text('Coming soon'), findsNothing);
  });

  testWidgets('localises the systems of ODEs workbench', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2000));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await settings.setLocale(const Locale('fa'));
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, '\u0645\u0639\u0627\u062f\u0644\u0627\u062a \u062f\u06cc\u0641\u0631\u0627\u0646\u0633\u06cc\u0644');
    await _drawerTap(tester, '\u062f\u0633\u062a\u06af\u0627\u0647\u200c\u0647\u0627 \u0648 \u067e\u0627\u06cc\u062f\u0627\u0631\u06cc');
    await _drawerTap(tester, '\u062f\u0633\u062a\u06af\u0627\u0647\u200c\u0647\u0627\u06cc \u0645\u0639\u0627\u062f\u0644\u0627\u062a \u062f\u06cc\u0641\u0631\u0627\u0646\u0633\u06cc\u0644');

    expect(find.text('\u062f\u0633\u062a\u06af\u0627\u0647\u200c\u0647\u0627\u06cc \u0645\u0639\u0627\u062f\u0644\u0627\u062a \u062f\u06cc\u0641\u0631\u0627\u0646\u0633\u06cc\u0644'), findsWidgets);
    expect(find.text('\u0646\u0645\u0627\u06cc\u0634 \u062c\u0648\u0627\u0628 \u0648 \u0645\u0631\u0627\u062d\u0644'), findsOneWidget);
  });

  testWidgets('analyses function behaviour and draws the plot', (tester) async {
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
    expect(
      find.text('Taylor\'s theorem with Lagrange remainder'),
      findsOneWidget,
    );
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
    await _awaitEngine(tester, find.byKey(const ValueKey('multi-curve-ready')));

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
    await _awaitEngine(tester, find.byKey(const ValueKey('multi-curve-ready')));
    expect(find.text('Compute second derivative'), findsOneWidget);

    await tester.ensureVisible(find.text('Compute second derivative'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('Compute second derivative'));
    await _awaitEngine(tester, find.text('Second derivative'));
    expect(find.text('Second derivative'), findsWidgets);
  });

  testWidgets('honours the solution and graph visibility settings', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 1700));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await settings.setShowSteps(false);
    await settings.setShowChecks(false);
    await settings.setShowGraphs(false);
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
    await tester.pumpAndSettle();

    expect(find.text('Solution path'), findsNothing);
    expect(find.byKey(const ValueKey('multi-curve-ready')), findsNothing);

    await settings.setShowGraphs(true);
    await tester.pump();
    expect(find.text('Function and derivative'), findsOneWidget);
    await _awaitEngine(tester, find.byKey(const ValueKey('multi-curve-ready')));
  });

  testWidgets('opens settings and switches theme', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.settings_rounded).first);
    await tester.pumpAndSettle();
    expect(find.text('Settings'), findsOneWidget);
    expect(find.text('Appearance'), findsWidgets);
    expect(find.text('Language'), findsOneWidget);
    expect(find.text('System'), findsOneWidget);

    await tester.tap(find.text('Light'));
    await tester.pumpAndSettle();
    expect(settings.themeMode, ThemeMode.light);

    await tester.tap(find.text('System'));
    await tester.pumpAndSettle();
    expect(settings.themeMode, ThemeMode.system);
  });

  testWidgets('quick language chips are gone from the settings header', (
    tester,
  ) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.settings_rounded).first);
    await tester.pumpAndSettle();

    expect(find.text('English'), findsNothing);
    expect(find.text('\u0641\u0627\u0631\u0633\u06cc'), findsNothing);
  });

  testWidgets('selects another language from the locale dropdown', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1000, 1500));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.settings_rounded).first);
    await tester.pumpAndSettle();

    final dropdown = find.byType(DropdownButtonFormField<String>);
    await tester.ensureVisible(dropdown);
    await tester.pumpAndSettle();
    await tester.tap(dropdown);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Español  ·  Spanish').last);
    await tester.pumpAndSettle();

    expect(settings.locale.languageCode, 'es');
    expect(find.text('Ajustes'), findsWidgets);
  });

  testWidgets('settings lives in the header, not the drawer', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    expect(
      find.descendant(of: find.byType(Drawer), matching: find.text('Settings')),
      findsNothing,
    );
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

  testWidgets('copies the ODE answer as plain text, not LaTeX', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 1700));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    String? copied;
    tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(
      SystemChannels.platform,
      (call) async {
        if (call.method == 'Clipboard.setData') {
          copied = (call.arguments as Map)['text'] as String?;
        }
        return null;
      },
    );
    addTearDown(
      () => tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.platform,
        null,
      ),
    );

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

    final copyButton = find.byIcon(Icons.copy_rounded).first;
    await tester.ensureVisible(copyButton);
    await tester.pumpAndSettle();
    await tester.tap(copyButton);
    await tester.pump();

    expect(copied, isNotNull);
    expect(copied, isNot(contains(r'\')));
    expect(copied, contains('y ='));
    expect(copied, contains('exp(x)'));
  });

  testWidgets('copies a derivative as plain text, not LaTeX', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2000));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    String? copied;
    tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(
      SystemChannels.platform,
      (call) async {
        if (call.method == 'Clipboard.setData') {
          copied = (call.arguments as Map)['text'] as String?;
        }
        return null;
      },
    );
    addTearDown(
      () => tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.platform,
        null,
      ),
    );

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

    await tester.enterText(find.byType(TextField).first, 'x^3');
    await tester.tap(find.text('Compute first derivative'));
    await _awaitEngine(tester, find.text('Compute second derivative'));

    final copyButton = find.byIcon(Icons.copy_rounded).first;
    await tester.ensureVisible(copyButton);
    await tester.pumpAndSettle();
    await tester.tap(copyButton);
    await tester.pump();

    expect(copied, isNotNull);
    expect(copied, isNot(contains(r'\')));
    expect(copied, contains('3*x^2'));
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

  test('palette changes follow the launcher icon', () async {
    final persistence = _RecordingPersistence();
    final settings = AppSettings(persistence: persistence);
    await settings.load();
    expect(settings.palette, 'rose');
    expect(persistence.launcherPalette, 'rose');

    await settings.setPalette('emerald');
    expect(settings.palette, 'emerald');
    expect(persistence.launcherPalette, 'emerald');

    await settings.resetDefaults();
    expect(settings.palette, 'rose');
    expect(persistence.launcherPalette, 'rose');
  });

  test('an unknown stored palette falls back to rose', () async {
    final settings = AppSettings(
      persistence: MemorySettingsPersistence({'palette': 'unknown'}),
    );
    await settings.load();
    expect(settings.palette, 'rose');
  });

  testWidgets('changes the accent palette from settings', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1000, 1400));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.settings_rounded).first);
    await tester.pumpAndSettle();
    expect(find.text('Accent palette'), findsOneWidget);

    await tester.ensureVisible(find.text('Emerald'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('Emerald'));
    await tester.pumpAndSettle();
    expect(settings.palette, 'emerald');
  });

  testWidgets('searches modules from the command palette', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.search_rounded).first);
    await tester.pumpAndSettle();
    expect(find.byType(Dialog), findsOneWidget);

    await tester.enterText(find.byType(TextField).last, 'Wronskian');
    await tester.pumpAndSettle();
    final result = find.descendant(
      of: find.byType(Dialog),
      matching: find.text('Wronskian'),
    );
    expect(result, findsNWidgets(2));
    await tester.tap(result.last);
    await tester.pumpAndSettle();
    expect(find.text('Wronskian & linear independence'), findsOneWidget);
  });

  testWidgets('builds a Taylor polynomial with its remainder', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2000));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Differential & integral');
    await _drawerTap(tester, 'Taylor & Maclaurin');

    final button = find.text('Build the polynomial');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();

    expect(find.text('Lagrange remainder'), findsWidgets);
    expect(find.text('Coefficients'), findsWidgets);
  });

  testWidgets('computes the area between two curves', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2000));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Differential & integral');
    await _drawerTap(tester, 'Integral applications');

    final button = find.text('Set up and evaluate');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();

    expect(find.text('Area between curves'), findsWidgets);
    expect(find.text('Integrand'), findsWidgets);
  });

  testWidgets('reads the gradient and Laplacian at a point', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2200));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Differential & integral');
    await _drawerTap(tester, 'Partial derivatives');

    final button = find.text('Differentiate and evaluate');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();

    expect(find.text('Gradient'), findsWidgets);
    expect(find.text('Laplacian'), findsWidgets);
    expect(find.text('Directional derivative'), findsWidgets);
  });

  testWidgets('decides series convergence and shows the sum', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2000));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Differential & integral');
    await _drawerTap(tester, 'Series & convergence');

    final button = find.text('Test for convergence');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();

    expect(find.text('Converges'), findsWidgets);
    expect(find.text('Exact sum'), findsWidgets);
    expect(find.text('p-series'), findsWidgets);
  });

  testWidgets('handles a factorial term with the ratio test', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2000));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Differential & integral');
    await _drawerTap(tester, 'Series & convergence');

    await tester.enterText(find.byType(TextField).first, '1/n!');
    final button = find.text('Test for convergence');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();

    expect(find.text('Converges'), findsWidgets);
    expect(find.text('Ratio test'), findsWidgets);
    expect(find.textContaining('n!'), findsWidgets);
  });

  testWidgets('solves a linear algebra system through the workbench', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2200));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Linear algebra');

    final button = find.text('Show the answer and steps');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();

    expect(find.text('Solution path'), findsOneWidget);
    expect(find.text('Simplest answer'), findsOneWidget);
    expect(find.byType(Math), findsWidgets);
    expect(find.textContaining('= C'), findsNothing);
  });

  testWidgets('solves a growth and decay application', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2200));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Differential equations');
    await _drawerTap(tester, 'Applications');
    await _drawerTap(tester, 'Growth & decay');

    final button = find.text('Show the answer and steps');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();

    expect(find.text('Solution path'), findsOneWidget);
    expect(find.text('Simplest answer'), findsOneWidget);
    expect(find.textContaining('exp'), findsWidgets);
    // The answer is a finished expression, not an implicit ODE family.
    expect(find.textContaining('= C'), findsNothing);
  });

  testWidgets('opens the integral, types and applications study articles', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2400));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Educational explanations');

    for (final title in [
      'Integral',
      'Types of differential equations',
      'Applications of differential equations',
    ]) {
      if (title != 'Integral') {
        await tester.tap(find.byIcon(Icons.menu_rounded).first);
        await tester.pumpAndSettle();
      }
      await _drawerTap(tester, title);
      expect(find.text('Short answer'), findsOneWidget);
      expect(find.text('Simplest answer'), findsOneWidget);
      expect(find.text('Step by step'), findsOneWidget);
    }
  });

  testWidgets('settings no longer advertises the computation engine', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2200));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.settings_rounded).first);
    await tester.pumpAndSettle();

    expect(find.textContaining('Native engine'), findsNothing);
    expect(find.textContaining('Engine ready'), findsNothing);
  });

  testWidgets('routes the damped oscillator and Heun workbenches', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2400));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations').last);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Applications');
    await _drawerTap(tester, 'Damped & forced oscillations');
    expect(find.text('Damped & forced oscillations'), findsWidgets);
    expect(find.text('Show the answer and steps'), findsOneWidget);

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Numerical methods');
    await _drawerTap(tester, 'Improved Euler (Heun)');
    expect(find.text('Improved Euler (Heun)'), findsWidgets);
    expect(find.text('Show the answer and steps'), findsOneWidget);
  });
}
