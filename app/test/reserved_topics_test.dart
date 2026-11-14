import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/native_engine.dart';
import 'package:infinit/src/pages/topic_workbench_page.dart';
import 'package:infinit/src/theme.dart';
import 'package:infinit/src/topic_catalog.dart';

/// The 31 curriculum slots that used to render ComingSoonPage.
const _reservedIds = <String>[
  'series_power',
  'series_singular_points',
  'series_frobenius',
  'euler_equations',
  'special_legendre',
  'special_bessel',
  'system_theory',
  'system_complex_eigen',
  'system_fundamental',
  'system_repeated',
  'system_nonhomogeneous',
  'system_rk4',
  'competing_species',
  'liapunov',
  'limit_cycle',
  'chaos_attractor',
  'local_linearization',
  'bvp_two_point',
  'bvp_occurrence',
  'sturm_liouville',
  'bvp_nonhomogeneous',
  'sturm_liouville_singular',
  'bessel_series',
  'orthogonal_series',
  'fourier_convergence',
  'fourier_even_odd',
  'heat_derivation',
  'wave_derivation',
  'heat_other_bc',
  'transcendental',
  'integration_techniques',
];

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

Widget _host(TopicSpec spec) {
  return MaterialApp(
    theme: buildOdeTheme(dark: true),
    home: Scaffold(
      body: SingleChildScrollView(
        child: TopicWorkbenchPage(
          spec: spec,
          engine: NativeEngine.instance,
          l10n: const L10n(Locale('en')),
        ),
      ),
    ),
  );
}

String _en(String key) => I18n.instance.value('en', key) ?? key;

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  test('the reserved ids are all catalogued', () {
    for (final id in _reservedIds) {
      expect(kTopicCatalog.containsKey(id), isTrue, reason: 'missing $id');
    }
  });

  for (final id in _reservedIds) {
    testWidgets('$id renders its title and fields', (tester) async {
      final spec = kTopicCatalog[id]!;
      await tester.pumpWidget(_host(spec));
      await tester.pump();

      expect(find.text(_en(spec.titleKey)), findsWidgets);
      for (final field in spec.fields) {
        expect(
          find.text(_en(field.labelKey)),
          findsWidgets,
          reason: '$id is missing ${field.labelKey}',
        );
      }
      expect(find.text('Show the answer and steps'), findsOneWidget);
    });
  }

  // One run per module family proves the workbench renders the answer, the
  // numeric evidence and the engine tables, not just the input form.
  for (final id in <String>[
    'series_power',
    'system_theory',
    'bvp_two_point',
    'heat_derivation',
    'transcendental',
  ]) {
    testWidgets('$id runs and shows the answer', (tester) async {
      await tester.binding.setSurfaceSize(const Size(1200, 2400));
      addTearDown(() => tester.binding.setSurfaceSize(null));
      final spec = kTopicCatalog[id]!;
      await tester.pumpWidget(_host(spec));
      await tester.pump();

      final button = find.text('Show the answer and steps');
      await tester.ensureVisible(button);
      await tester.pump();
      await tester.tap(button);
      await _awaitEngine(tester, find.text('Simplest answer'));
      await tester.pump();

      expect(find.text('Simplest answer'), findsOneWidget);
      expect(find.text(_en('ui.computed_results')), findsWidgets);
    });
  }
}
