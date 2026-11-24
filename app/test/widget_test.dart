import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_math_fork/flutter_math.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/main.dart';
import 'package:infinit/src/app_state.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/settings_store.dart';
import 'package:infinit/src/theme.dart';

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

/// Expands collapsed drawer groups until [target] exists in the tree.
///
/// The curriculum tree nests most leaves two or three levels deep, so a label
/// is not built until its parent groups are open. Expanding in tree order and
/// stopping as soon as the target appears keeps unrelated groups closed.
Future<void> _expandUntilVisible(WidgetTester tester, Finder target) async {
  for (var round = 0; round < 40; round++) {
    if (target.evaluate().isNotEmpty) return;
    final closed = find.descendant(
      of: find.byType(Drawer),
      matching: find.byWidgetPredicate(
        (widget) =>
            widget is Semantics &&
            widget.properties.button == true &&
            widget.properties.expanded == false,
      ),
    );
    if (closed.evaluate().isEmpty) return;
    final header = closed.first;
    await tester.ensureVisible(header);
    await tester.pumpAndSettle();
    await tester.tap(header);
    await tester.pumpAndSettle();
  }
}

Finder _drawerLabel(String label) {
  final list = find.descendant(
    of: find.byType(Drawer),
    matching: find.byType(ListView),
  );
  return find.descendant(
    of: list,
    matching: find.byWidgetPredicate(
      (widget) => widget is Text && widget.data == label,
    ),
  );
}

Future<void> _drawerTap(WidgetTester tester, String label) async {
  var all = _drawerLabel(label);
  if (all.evaluate().isEmpty) {
    await _expandUntilVisible(tester, all);
    all = _drawerLabel(label);
  }
  if (all.evaluate().isEmpty) {
    // The leaf may still be unbuilt because it is far below the viewport.
    // The search field flattens the tree, so use it as a reliable fallback.
    final search = find.descendant(
      of: find.byType(Drawer),
      matching: find.byType(TextField),
    );
    await tester.enterText(search, label);
    await tester.pumpAndSettle();
    all = _drawerLabel(label);
  }
  // The same label can appear twice (a group header and a link, or the
  // library link on two shelves); the first row is the one to tap.
  final finder = all.evaluate().isEmpty ? all : all.first;
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
    expect(settings.palette, 'indigo');

    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    expect(find.text('Infinit Lab'), findsOneWidget);
    expect(find.text('Mathematics workbench'), findsOneWidget);
    expect(find.text('Derivative'), findsWidgets);
    expect(find.text('Integral'), findsWidgets);
    expect(find.text('Differential equation forms'), findsOneWidget);
    expect(find.text('Study note'), findsOneWidget);
    expect(find.text('How to read a solution'), findsOneWidget);
  });

  testWidgets('switches to Persian and back', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await settings.setLocale(const Locale('fa'));
    await tester.pump(const Duration(milliseconds: 300));
    expect(find.text('Infinit Lab'), findsOneWidget);
    expect(find.text('\u0645\u0634\u062a\u0642'), findsWidgets);

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
    await _drawerTap(tester, 'Integral');
    expect(find.text('Integrals'), findsOneWidget);
    expect(find.text('Study note'), findsOneWidget);
    expect(find.text('Integration is accumulation'), findsOneWidget);

    final expand = find.text('Worked example & pitfalls');
    await tester.ensureVisible(expand);
    await tester.pumpAndSettle();
    await tester.tap(expand);
    await tester.pumpAndSettle();
    expect(
      find.textContaining('the integral becomes', findRichText: true),
      findsOneWidget,
    );
  });

  testWidgets('opens the dedicated orthogonal-trajectories page', (
    tester,
  ) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations').last);
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

  testWidgets('the drawer no longer lists oblique trajectories', (
    tester,
  ) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations').last);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Applications');

    expect(
      find.descendant(
        of: find.byType(Drawer),
        matching: find.text('Oblique trajectories'),
      ),
      findsNothing,
    );
    expect(
      find.descendant(
        of: find.byType(Drawer),
        matching: find.text('Orthogonal trajectories'),
      ),
      findsOneWidget,
    );
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
    // Applications sit inside the differentiation group, which is collapsed
    // until the learner opens it.
    await _drawerTap(tester, 'Differentiation & applications');
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
    // Cauchy's theorem is reached from the MVT page, not from this menu.
    expect(find.text("Cauchy's theorem"), findsNothing);
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
    await _drawerTap(tester, 'Derivative: rate and tangent');

    // The article shows one simplest answer and one numbered solution per
    // worked example; the derivative article now carries two examples.
    expect(find.text('Short answer'), findsOneWidget);
    expect(find.text('Simplest answer'), findsNWidgets(2));
    expect(find.text('Step by step'), findsNWidgets(2));
    expect(find.textContaining('lim'), findsWidgets);
  });

  testWidgets('explanations library opens an article from its section', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2200));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Explanations library');

    expect(find.text('Explanations library'), findsOneWidget);
    expect(find.text('Differential equations'), findsWidgets);
    expect(find.text('Solving first-order equations'), findsOneWidget);

    await tester.ensureVisible(find.text('Solving first-order equations'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('Solving first-order equations'));
    await tester.pumpAndSettle();

    expect(find.text('Solving first-order equations'), findsOneWidget);
    expect(find.text('Short answer'), findsOneWidget);
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
    expect(find.text('Coming soon'), findsNothing);
    expect(find.text('Show the answer and steps'), findsOneWidget);
  });

  testWidgets('shows the current path in the drawer breadcrumb', (
    tester,
  ) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations').last);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Second-order linear equations');
    await _drawerTap(tester, 'Wronskian');

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();

    final trail = find.byWidgetPredicate(
      (widget) =>
          widget is Text &&
          RegExp(
            r'Differential equations\s*/\s*Second-order linear equations\s*/\s*Wronskian',
          ).hasMatch(widget.textSpan?.toPlainText() ?? ''),
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

    expect(
      find.text(
        '\u062d\u0633\u0627\u0628 \u062f\u06cc\u0641\u0631\u0627\u0646\u0633\u06cc\u0644 \u0648 \u0627\u0646\u062a\u06af\u0631\u0627\u0644',
      ),
      findsOneWidget,
    );
    expect(
      find.text(
        '\u062a\u0648\u0636\u06cc\u062d\u0627\u062a \u0622\u0645\u0648\u0632\u0634\u06cc',
      ),
      findsOneWidget,
    );

    await _drawerTap(
      tester,
      '\u062d\u0633\u0627\u0628 \u062f\u06cc\u0641\u0631\u0627\u0646\u0633\u06cc\u0644 \u0648 \u0627\u0646\u062a\u06af\u0631\u0627\u0644',
    );
    await _drawerTap(
      tester,
      '\u0645\u0634\u062a\u0642 \u0648 \u06a9\u0627\u0631\u0628\u0631\u062f\u200c\u0647\u0627',
    );
    await _drawerTap(
      tester,
      '\u06a9\u0627\u0631\u0628\u0631\u062f\u0647\u0627\u06cc \u0645\u0634\u062a\u0642',
    );
    expect(
      find.text(
        '\u062a\u062d\u0644\u06cc\u0644 \u0631\u0641\u062a\u0627\u0631 \u062a\u0627\u0628\u0639',
      ),
      findsOneWidget,
    );
    expect(
      find.text(
        '\u0645\u0645\u0627\u0633 \u0648 \u062e\u0637 \u0639\u0645\u0648\u062f',
      ),
      findsOneWidget,
    );
    expect(
      find.text(
        '\u0645\u0633\u0627\u06cc\u0644 \u0628\u0647\u06cc\u0646\u0647\u200c\u0633\u0627\u0632\u06cc',
      ),
      findsOneWidget,
    );
    expect(
      find.text(
        '\u0646\u0631\u062e\u200c\u0647\u0627\u06cc \u0645\u0631\u062a\u0628\u0637',
      ),
      findsOneWidget,
    );
    await _drawerTap(
      tester,
      '\u0642\u0636\u0627\u06cc\u0627\u06cc \u0645\u0647\u0645',
    );
    expect(
      find.text(
        '\u0642\u0627\u0639\u062f\u0647\u0654 \u0647\u0648\u067e\u06cc\u062a\u0627\u0644',
      ),
      findsOneWidget,
    );
    expect(
      find.text(
        '\u0642\u0636\u06cc\u0647\u0654 \u0645\u0642\u062f\u0627\u0631 \u0645\u06cc\u0627\u0646\u06af\u06cc\u0646',
      ),
      findsOneWidget,
    );
    expect(
      find.text('\u0642\u0636\u06cc\u0647\u0654 \u0631\u0648\u0644'),
      findsOneWidget,
    );
    expect(
      find.text('\u0642\u0636\u06cc\u0647\u0654 \u06a9\u0648\u0634\u06cc'),
      findsNothing,
    );
    expect(
      find.text(
        '\u0642\u0636\u0627\u06cc\u0627\u06cc \u0645\u0634\u062a\u0642\u060c \u06a9\u0627\u0645\u0644',
      ),
      findsOneWidget,
    );
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
    await _drawerTap(tester, 'Nonlinear systems & stability');
    await _drawerTap(tester, 'Systems of ODEs');

    expect(find.text('Systems of ODEs'), findsWidgets);
    expect(find.text('Show the answer and steps'), findsOneWidget);
    expect(find.text('Coming soon'), findsNothing);
  });

  testWidgets('runs the matrix and eigenvalue workbench', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2400));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations').last);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Nonlinear systems & stability');
    await _drawerTap(tester, 'Matrix & eigenvalue methods');

    expect(find.text('Matrix & eigenvalue methods'), findsWidgets);
    expect(find.text('Show the answer and steps'), findsOneWidget);
    expect(find.text('Coming soon'), findsNothing);

    final button = find.text('Show the answer and steps');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Find the eigenvalues'));
    await tester.pumpAndSettle();
    expect(find.text('Simplest answer'), findsOneWidget);
    expect(
      find.text('Characteristic polynomial vanishes at every eigenvalue'),
      findsOneWidget,
    );
    // The check sentence now carries inline math, so the plain string is
    // split into spans; match the prose part instead.
    expect(
      find.byWidgetPredicate(
        (w) =>
            w is Text &&
            (w.data ?? w.textSpan?.toPlainText() ?? '')
                .contains('Eigenvectors satisfy'),
      ),
      findsOneWidget,
    );
  });

  testWidgets('runs the phase portrait workbench', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2400));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations').last);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Nonlinear systems & stability');
    await _drawerTap(tester, 'Phase portraits');

    expect(find.text('Phase portraits'), findsWidgets);
    expect(find.text('Show the answer and steps'), findsOneWidget);
    expect(find.text('Coming soon'), findsNothing);

    final button = find.text('Show the answer and steps');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Find the eigenvalues'));
    await tester.pumpAndSettle();
    expect(find.text('Simplest answer'), findsOneWidget);
    expect(
      find.text('Eigenvalues satisfy the characteristic polynomial'),
      findsOneWidget,
    );
    expect(
      find.text('The portrait type matches the eigenvalues'),
      findsOneWidget,
    );
  });

  testWidgets('runs the critical points workbench', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2600));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations').last);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Nonlinear systems & stability');
    await _drawerTap(tester, 'Critical points & stability');

    expect(find.text('Critical points & stability'), findsWidgets);
    expect(find.text('Coming soon'), findsNothing);

    final button = find.text('Show the answer and steps');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Find the critical points'));
    await tester.pumpAndSettle();
    expect(
      find.text('3 critical point(s) were found in the given box.'),
      findsOneWidget,
    );
    expect(
      find.text('The Jacobian matches finite differences'),
      findsOneWidget,
    );
    expect(
      find.text('Every critical point satisfies the system'),
      findsOneWidget,
    );
  });

  testWidgets('runs the predator–prey workbench', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2800));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations').last);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Nonlinear systems & stability');
    await _drawerTap(tester, 'Predator–prey models');

    expect(find.text('Predator–prey models'), findsWidgets);
    expect(find.text('Coming soon'), findsNothing);

    final button = find.text('Show the answer and steps');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Integrate the populations'));
    await tester.pumpAndSettle();
    expect(find.text('The conserved quantity is constant'), findsOneWidget);
    expect(find.text('The populations stay positive'), findsOneWidget);
    expect(
      find.text('The coexistence eigenvalues are purely imaginary'),
      findsOneWidget,
    );
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
    await _drawerTap(
      tester,
      '\u0645\u0639\u0627\u062f\u0644\u0627\u062a \u062f\u06cc\u0641\u0631\u0627\u0646\u0633\u06cc\u0644',
    );
    await _drawerTap(
      tester,
      '\u062f\u0633\u062a\u06af\u0627\u0647\u200c\u0647\u0627\u06cc \u063a\u06cc\u0631\u062e\u0637\u06cc \u0648 \u067e\u0627\u06cc\u062f\u0627\u0631\u06cc',
    );
    await _drawerTap(
      tester,
      '\u062f\u0633\u062a\u06af\u0627\u0647\u200c\u0647\u0627\u06cc \u0645\u0639\u0627\u062f\u0644\u0627\u062a \u062f\u06cc\u0641\u0631\u0627\u0646\u0633\u06cc\u0644',
    );

    expect(
      find.text(
        '\u062f\u0633\u062a\u06af\u0627\u0647\u200c\u0647\u0627\u06cc \u0645\u0639\u0627\u062f\u0644\u0627\u062a \u062f\u06cc\u0641\u0631\u0627\u0646\u0633\u06cc\u0644',
      ),
      findsWidgets,
    );
    expect(
      find.text(
        '\u0646\u0645\u0627\u06cc\u0634 \u062c\u0648\u0627\u0628 \u0648 \u0645\u0631\u0627\u062d\u0644',
      ),
      findsOneWidget,
    );
  });

  testWidgets('analyses function behaviour and draws the plot', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Differential & integral');
    await _drawerTap(tester, 'Differentiation & applications');
    await _drawerTap(tester, 'Applications of the derivative');
    await _drawerTap(tester, 'Function behaviour');
    expect(find.text('Function behaviour'), findsWidgets);

    final button = find.text('Analyse the function');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();

    // The answer section, the solution path and the CustomPainter plot all appear.
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
    await _drawerTap(tester, 'Differentiation & applications');
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
    await _drawerTap(tester, 'Differentiation & applications');
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

  testWidgets('reaches Cauchy\'s theorem from the mean value theorem', (
    tester,
  ) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Differential & integral');
    await _drawerTap(tester, 'Differentiation & applications');
    await _drawerTap(tester, 'Applications of the derivative');
    await _drawerTap(tester, 'Important theorems');
    await _drawerTap(tester, 'Mean value theorem');

    final link = find.text("Cauchy's theorem");
    expect(link, findsOneWidget);
    await tester.ensureVisible(link);
    await tester.pumpAndSettle();
    await tester.tap(link);
    await tester.pumpAndSettle();

    expect(find.text('second function g'), findsOneWidget);
    expect(find.text("Cauchy's Mean Value Theorem"), findsOneWidget);
  });

  testWidgets('renders the complete theorem catalogue', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Differential & integral');
    await _drawerTap(tester, 'Differentiation & applications');
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
    await _drawerTap(tester, 'Integral');

    await tester.enterText(find.byType(TextField).first, '2*x*cos(x^2)');
    await tester.tap(find.text('Compute integral'));
    await _awaitEngine(tester, find.text('Substitution'));
    await _awaitEngine(tester, find.byKey(const ValueKey('multi-curve-ready')));

    expect(find.text('Substitution'), findsOneWidget);
    expect(find.text('How this is solved'), findsOneWidget);
    expect(find.text('Standard form'), findsOneWidget);
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
    await _drawerTap(tester, 'Derivative');

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
    await _drawerTap(tester, 'Derivative');

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

  test('desktop mode is stored with the other preferences', () async {
    final store = MemorySettingsPersistence();
    final first = AppSettings(persistence: store);
    await first.load();
    expect(first.desktopMode, isFalse);

    await first.setDesktopMode(true);
    final second = AppSettings(persistence: store);
    await second.load();
    expect(second.desktopMode, isTrue);
  });

  double viewportScale(WidgetTester tester) {
    final transform = tester.widget<Transform>(
      find.byKey(const ValueKey('desktop-viewport-transform')),
    );
    return transform.transform.entry(0, 0);
  }

  testWidgets('desktop mode renders a scaled desktop window', (tester) async {
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();

    Future<void> pumpAt(double width, double height) async {
      await tester.binding.setSurfaceSize(Size(width, height));
      await tester.pumpWidget(InfinitApp(settings: settings));
      await tester.pump(const Duration(milliseconds: 300));
    }

    await pumpAt(1400, 1600);
    expect(find.byKey(const ValueKey('nav-side')), findsOneWidget);
    expect(find.byKey(const ValueKey('desktop-zoom-bar')), findsNothing);
    expect(viewportScale(tester), 1);

    await pumpAt(800, 1600);
    expect(find.byKey(const ValueKey('nav-side')), findsNothing);

    await settings.setDesktopMode(true);
    await pumpAt(800, 1600);
    expect(find.byKey(const ValueKey('nav-side')), findsOneWidget);
    expect(find.byKey(const ValueKey('desktop-zoom-bar')), findsOneWidget);
    expect(viewportScale(tester), closeTo(800 / 1100, 1e-6));

    await pumpAt(360, 2400);
    expect(find.byKey(const ValueKey('nav-side')), findsOneWidget);
    expect(viewportScale(tester), closeTo(360 / 1100, 1e-6));

    // Wider than the design width: keep the real window instead of stretching.
    await pumpAt(1200, 1600);
    expect(viewportScale(tester), 1);
  });

  testWidgets('pinch and buttons zoom the desktop window', (tester) async {
    addTearDown(() => tester.binding.setSurfaceSize(null));
    await tester.binding.setSurfaceSize(const Size(400, 900));
    final settings = await _settings();
    await settings.setDesktopMode(true);
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    final fit = 400 / 1100;
    expect(viewportScale(tester), closeTo(fit, 1e-6));
    expect(find.text('100%'), findsOneWidget);

    final first = await tester.startGesture(const Offset(120, 300));
    final second = await tester.startGesture(const Offset(280, 300));
    await tester.pump();
    await first.moveTo(const Offset(40, 300));
    await second.moveTo(const Offset(360, 300));
    await tester.pump();
    await first.up();
    await second.up();
    await tester.pumpAndSettle();

    expect(viewportScale(tester), closeTo(fit * 2, 0.001));
    expect(find.text('200%'), findsOneWidget);

    final bar = find.byKey(const ValueKey('desktop-zoom-bar'));
    await tester.tap(
      find.descendant(of: bar, matching: find.byIcon(Icons.remove_rounded)),
    );
    await tester.pumpAndSettle();
    expect(find.text('160%'), findsOneWidget);
    expect(viewportScale(tester), closeTo(fit * 1.6, 0.001));

    await tester.tap(
      find.descendant(of: bar, matching: find.byIcon(Icons.fit_screen_rounded)),
    );
    await tester.pumpAndSettle();
    expect(find.text('100%'), findsOneWidget);
    expect(viewportScale(tester), closeTo(fit, 1e-6));
  });

  testWidgets('one finger pans the zoomed desktop window', (tester) async {
    addTearDown(() => tester.binding.setSurfaceSize(null));
    await tester.binding.setSurfaceSize(const Size(400, 900));
    final settings = await _settings();
    await settings.setDesktopMode(true);
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    final bar = find.byKey(const ValueKey('desktop-zoom-bar'));
    await tester.tap(
      find.descendant(of: bar, matching: find.byIcon(Icons.add_rounded)),
    );
    await tester.pumpAndSettle();
    final before = tester
        .widget<Transform>(
          find.byKey(const ValueKey('desktop-viewport-transform')),
        )
        .transform
        .entry(0, 3);
    expect(before, lessThan(0));

    await tester.dragFrom(const Offset(200, 300), const Offset(120, 0));
    await tester.pumpAndSettle();

    final after = tester
        .widget<Transform>(
          find.byKey(const ValueKey('desktop-viewport-transform')),
        )
        .transform
        .entry(0, 3);
    expect(after, greaterThan(before));
    expect(after, lessThanOrEqualTo(0));
  });

  testWidgets('vertical scrolling still works while zoomed', (tester) async {
    addTearDown(() => tester.binding.setSurfaceSize(null));
    await tester.binding.setSurfaceSize(const Size(400, 600));
    final settings = await _settings();
    await settings.setDesktopMode(true);
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    final bar = find.byKey(const ValueKey('desktop-zoom-bar'));
    await tester.tap(
      find.descendant(of: bar, matching: find.byIcon(Icons.add_rounded)),
    );
    await tester.pumpAndSettle();

    final list = find.byKey(const ValueKey('home-scroll'));
    final controller = tester.widget<ListView>(list).controller!;
    expect(controller.offset, 0);

    await tester.drag(list, const Offset(0, -300));
    await tester.pumpAndSettle();
    expect(controller.offset, greaterThan(0));
  });

  testWidgets('desktop toolbar opens search and settings', (tester) async {
    addTearDown(() => tester.binding.setSurfaceSize(null));
    await tester.binding.setSurfaceSize(const Size(400, 900));
    final settings = await _settings();
    await settings.setDesktopMode(true);
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    final bar = find.byKey(const ValueKey('desktop-zoom-bar'));
    await tester.tap(
      find.descendant(of: bar, matching: find.byIcon(Icons.search_rounded)),
    );
    await tester.pumpAndSettle();
    expect(find.byType(Dialog), findsOneWidget);

    await tester.tapAt(const Offset(6, 6));
    await tester.pumpAndSettle();
    expect(find.byType(Dialog), findsNothing);

    await tester.tap(
      find.descendant(of: bar, matching: find.byIcon(Icons.settings_rounded)),
    );
    await tester.pumpAndSettle();
    expect(find.text('Settings'), findsOneWidget);
  });

  testWidgets('desktop mode hides the Android status bar', (tester) async {
    final calls = <MethodCall>[];
    final messenger =
        TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;
    const channel = MethodChannel('ir.infinit/settings');
    messenger.setMockMethodCallHandler(channel, (call) async {
      calls.add(call);
      return null;
    });
    addTearDown(() => messenger.setMockMethodCallHandler(channel, null));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    await tester.binding.setSurfaceSize(const Size(400, 900));
    final settings = await _settings();
    await settings.setDesktopMode(true);
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    List<Object?> bars() => calls
        .where((call) => call.method == 'set_desktop_mode')
        .map((call) => call.arguments)
        .toList();
    expect(bars(), contains(true));

    await settings.setDesktopMode(false);
    await tester.pumpAndSettle();
    expect(bars().last, isFalse);
  });

  testWidgets('one finger still scrolls inside the desktop window', (
    tester,
  ) async {
    addTearDown(() => tester.binding.setSurfaceSize(null));
    await tester.binding.setSurfaceSize(const Size(400, 600));
    final settings = await _settings();
    await settings.setDesktopMode(true);
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    final list = find.byKey(const ValueKey('home-scroll'));
    final controller = tester.widget<ListView>(list).controller!;
    expect(controller.offset, 0);

    await tester.drag(list, const Offset(0, -400));
    await tester.pumpAndSettle();
    expect(controller.offset, greaterThan(0));
  });

  testWidgets('enables desktop mode from settings', (tester) async {
    await tester.binding.setSurfaceSize(const Size(900, 1800));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.settings_rounded).first);
    await tester.pumpAndSettle();
    expect(find.text('Desktop mode'), findsOneWidget);

    final row = find
        .ancestor(of: find.text('Desktop mode'), matching: find.byType(Row))
        .first;
    final toggle = find.descendant(of: row, matching: find.byType(Switch));
    await tester.ensureVisible(toggle);
    await tester.pumpAndSettle();
    await tester.tap(toggle);
    await tester.pumpAndSettle();

    expect(settings.desktopMode, isTrue);
    expect(find.byKey(const ValueKey('nav-side')), findsOneWidget);
    expect(find.byKey(const ValueKey('desktop-zoom-bar')), findsOneWidget);
    // The settings page stays open while the layout switches.
    expect(find.text('Desktop mode'), findsOneWidget);
  });

  testWidgets('narrow Persian home fits the desktop window', (tester) async {
    await tester.binding.setSurfaceSize(const Size(360, 2400));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await settings.setDesktopMode(true);
    await settings.setLocale(const Locale('fa'));
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    expect(
      find.text(
        '\u0641\u0631\u0645\u200c\u0647\u0627\u06cc \u0645\u0639\u0627\u062f\u0644\u0627\u062a \u062f\u06cc\u0641\u0631\u0627\u0646\u0633\u06cc\u0644',
      ),
      findsOneWidget,
    );
    expect(find.byKey(const ValueKey('nav-side')), findsOneWidget);
    expect(tester.takeException(), isNull);
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
    expect(find.text('Also: First-order linear'), findsOneWidget);
  });

  testWidgets('shows the graph above the answer and names the method', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 4000));
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

    await tester.enterText(find.byType(TextField).first, "y' + y/x = x^2");
    await tester.tap(find.text('Solve equation'));
    await _awaitEngine(tester, find.byKey(const ValueKey('field-ready')));
    await tester.pumpAndSettle();

    // The plan card tells the reader which method was used, the standard form
    // and the substitution, not just the arithmetic of each step.
    expect(find.text('How this is solved'), findsOneWidget);
    expect(find.text('Standard form'), findsOneWidget);
    expect(find.text('Substitution'), findsOneWidget);
    expect(find.text('Key formulas'), findsOneWidget);
    expect(find.textContaining('First-order linear'), findsWidgets);

    // The picture belongs above the answer and the solution path.
    final plot = tester.getTopLeft(
      find.text('Direction field & solution family'),
    );
    final steps = tester.getTopLeft(find.text('Solution path'));
    expect(plot.dy, lessThan(steps.dy));
    // The plan sits inside the solution card, above the numbered steps.
    final plan = tester.getTopLeft(find.text('How this is solved'));
    expect(plan.dy, greaterThan(steps.dy));
    expect(plan.dy, lessThan(tester.getTopLeft(find.text('Key formulas')).dy));
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
    expect(copied, contains('e^x'));
    expect(copied, isNot(contains('exp(')));
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
    await _drawerTap(tester, 'Derivative');

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
    await _drawerTap(tester, 'Foundations');
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
    expect(settings.palette, 'indigo');
    expect(persistence.launcherPalette, 'indigo');

    await settings.setPalette('emerald');
    expect(settings.palette, 'emerald');
    expect(persistence.launcherPalette, 'emerald');

    await settings.resetDefaults();
    expect(settings.palette, 'indigo');
    expect(persistence.launcherPalette, 'indigo');
  });

  test('an unknown stored palette falls back to indigo', () async {
    final settings = AppSettings(
      persistence: MemorySettingsPersistence({'palette': 'unknown'}),
    );
    await settings.load();
    expect(settings.palette, 'indigo');
  });

  testWidgets('the default accent is the Indigo palette', (tester) async {
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    final context = tester.element(find.text('Mathematics workbench'));
    expect(context.colors.accent, OdePalette.indigo.darkAccent);
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
    expect(find.text('Indigo'), findsOneWidget);
    expect(settings.palette, 'indigo');

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
    await _drawerTap(tester, 'Section overview');

    // The section opens on its map, not on a single workbench.
    expect(find.text('Matrices and elimination'), findsOneWidget);
    expect(find.text('Rank & fundamental subspaces'), findsOneWidget);

    await tester.tap(find.text('Rank & fundamental subspaces'));
    await tester.pumpAndSettle();

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

  testWidgets('solves a vector-space workbench', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2200));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Linear algebra');
    await _drawerTap(tester, 'Coordinates in a basis');

    final button = find.text('Show the answer and steps');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();

    expect(find.text('Solution path'), findsOneWidget);
    expect(find.text('Simplest answer'), findsOneWidget);
    expect(find.byType(Math), findsWidgets);
  });

  testWidgets('solves an application workbench', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2200));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Linear algebra');
    await _drawerTap(tester, 'QR decomposition');

    final button = find.text('Show the answer and steps');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();

    expect(find.text('Solution path'), findsOneWidget);
    expect(find.text('Simplest answer'), findsOneWidget);
    expect(find.byType(Math), findsWidgets);
  });

  testWidgets('solves a linear system by Gauss and by Cramer', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2400));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Linear algebra');
    await _drawerTap(tester, 'Matrices and elimination');
    await _drawerTap(tester, 'Linear systems (Gauss / Cramer)');

    // The method selector must offer both methods and start on Gauss.
    expect(find.text('Gauss'), findsOneWidget);
    expect(find.text('Cramer'), findsOneWidget);

    final button = find.text('Show the answer and steps');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Solution path'));
    await tester.pumpAndSettle();
    expect(find.text('Back substitution'), findsOneWidget);

    // Choosing Cramer must actually change the path, not just the label.
    await tester.ensureVisible(find.text('Cramer'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('Cramer'));
    await tester.pumpAndSettle();
    final runAgain = find.text('Show the answer and steps');
    await tester.ensureVisible(runAgain);
    await tester.pumpAndSettle();
    await tester.tap(runAgain);
    await _awaitEngine(tester, find.text("Apply Cramer's rule"));
    await tester.pumpAndSettle();
    expect(find.text("Apply Cramer's rule"), findsOneWidget);
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

  testWidgets('runs the Laplace chapter workbenches', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2400));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    for (final title in [
      'Transform of derivatives',
      'Solving IVPs with Laplace',
      'Step functions',
      'Discontinuous forcing',
      'Impulse functions',
    ]) {
      final settings = await _settings();
      // A fresh key gives each chapter its own app state, so the scroll offset
      // of the previous workbench cannot hide the header on the next one.
      await tester.pumpWidget(
        InfinitApp(key: ValueKey<String>(title), settings: settings),
      );
      await tester.pump(const Duration(milliseconds: 300));

      await tester.tap(find.byIcon(Icons.menu_rounded).first);
      await tester.pumpAndSettle();
      await _drawerTap(tester, 'Differential equations');
      await _drawerTap(tester, 'Transforms');
      await _drawerTap(tester, title);

      final button = find.text('Show the answer and steps');
      await tester.ensureVisible(button);
      await tester.pumpAndSettle();
      await tester.tap(button);
      await _awaitEngine(tester, find.text('Solution path'));
      await tester.pumpAndSettle();

      expect(find.text('Solution path'), findsOneWidget, reason: title);
      expect(find.text('Simplest answer'), findsOneWidget, reason: title);
    }
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
      'Integral: area and accumulation',
      'Classifying differential equations',
      'Modelling with differential equations',
    ]) {
      if (title != 'Integral: area and accumulation') {
        await tester.tap(find.byIcon(Icons.menu_rounded).first);
        await tester.pumpAndSettle();
      }
      await _drawerTap(tester, title);
      expect(find.text('Short answer'), findsOneWidget);
      expect(find.text('Simplest answer'), findsNWidgets(2));
      expect(find.text('Step by step'), findsNWidgets(2));
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

  testWidgets('runs the Adams–Bashforth workbench', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2400));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations').last);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Numerical methods');
    await _drawerTap(tester, 'Adams–Bashforth multistep');
    expect(find.text('Adams–Bashforth multistep'), findsWidgets);

    final button = find.text('Show the answer and steps');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Write the multistep rule'));
    await tester.pumpAndSettle();
    expect(find.text('Simplest answer'), findsOneWidget);
    expect(find.text('Agreement with RK4'), findsOneWidget);
    expect(find.textContaining('= C'), findsNothing);
  });

  testWidgets('runs the error and convergence workbench', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2400));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final settings = await _settings();
    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    await tester.tap(find.byIcon(Icons.menu_rounded).first);
    await tester.pumpAndSettle();
    await tester.tap(find.text('Differential equations').last);
    await tester.pumpAndSettle();
    await _drawerTap(tester, 'Numerical methods');
    await _drawerTap(tester, 'Error & convergence');
    expect(find.text('Error & convergence'), findsWidgets);

    final button = find.text('Show the answer and steps');
    await tester.ensureVisible(button);
    await tester.pumpAndSettle();
    await tester.tap(button);
    await _awaitEngine(tester, find.text('Measure the convergence order'));
    await tester.pumpAndSettle();
    expect(find.text('Observed convergence order'), findsOneWidget);
    expect(find.text('Error shrinks when the step is halved'), findsOneWidget);
  });
}
