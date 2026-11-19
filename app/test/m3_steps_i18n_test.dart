import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/models.dart';
import 'package:infinit/src/theme.dart';
import 'package:infinit/src/widgets.dart';

Widget _host(SolveResult result, Locale locale) => MaterialApp(
  theme: ThemeData(extensions: const [OdeColors.light]),
  home: Scaffold(
    body: ResultView(result: result, l10n: L10n(locale)),
  ),
);

void main() {
  setUp(() {
    I18n.instance = I18n.fromMaps({
      'fa': {
        'step.title.read_the_final_value': 'مقدار نهایی را بخوانید',
        'step.note.after_steps': 'پس از {n} گام.',
        'step.note.second_component_is': 'مؤلفهٔ دوم:',
      },
      'en': {
        'step.title.read_the_final_value': 'Read the final value',
        'step.note.after_steps': 'After {n} steps.',
        'step.note.second_component_is': 'The second component is',
      },
    });
  });

  tearDown(() {
    I18n.instance = I18n(const {});
  });

  testWidgets('a keyed step renders title and note from the catalogue', (
    tester,
  ) async {
    final result = SolveResult.fromJson({
      'ok': true,
      'mode': 'topic',
      'type_id': 'growth',
      'steps': [
        {
          'title': '',
          'note': '',
          'title_key': 'step.title.read_the_final_value',
          'note_key': 'step.note.after_steps',
          'params': {'n': 4},
        },
      ],
    });

    await tester.pumpWidget(_host(result, const Locale('en')));

    expect(find.text('Read the final value'), findsOneWidget);
    expect(find.text('After 4 steps.'), findsOneWidget);
  });

  testWidgets('a step note keeps math in its own widget', (tester) async {
    final result = SolveResult.fromJson({
      'ok': true,
      'mode': 'topic',
      'type_id': 'system',
      'steps': [
        {
          'title': '',
          'note': '',
          'title_key': 'step.title.read_the_final_value',
          'note_key': 'step.note.second_component_is',
          'note_math': {
            'plain': 'y(t) = C_1 e^t',
            'pretty': 'y(t) = C_1 e^t',
            'latex': 'y(t) = C_1 e^t',
          },
        },
      ],
    });

    await tester.pumpWidget(_host(result, const Locale('en')));

    expect(find.text('The second component is'), findsOneWidget);
    expect(find.byType(MathInline), findsOneWidget);
    expect(find.textContaining('y(t) = C_1 e^t'), findsNothing);
  });

  testWidgets('a string note_math is treated as LaTeX and rendered', (
    tester,
  ) async {
    // The engine sends the converted formula as a bare string, not as an
    // ExView map; dropping it was why table rules showed up as ASCII.
    final result = SolveResult.fromJson({
      'ok': true,
      'mode': 'topic',
      'type_id': 'laplace',
      'steps': [
        {
          'title': '',
          'note': 'L{e^(at) sin(bt)} = b/((s-a)^2+b^2)',
          'note_math': r'\mathcal{L}\left\{e^{at}\sin(bt)\right\}'
              r'=\frac{b}{(s-a)^2+b^2}',
        },
      ],
    });

    await tester.pumpWidget(_host(result, const Locale('en')));

    expect(find.byType(MathInline), findsOneWidget);
    expect(find.textContaining('b/((s-a)^2+b^2)'), findsNothing);
  });

  testWidgets('a mixed note renders prose and formulas line by line', (
    tester,
  ) async {
    final result = SolveResult.fromJson({
      'ok': true,
      'mode': 'topic',
      'type_id': 'equilibria',
      'steps': [
        {
          'title': '',
          'note': 'no equilibrium found in the window',
          'note_lines': [
            {'text': 'no equilibrium found in the window', 'latex': ''},
            {
              'text': 'E1 = (0, 0)',
              'latex': r'E_{1} = \left(0,\;0\right)',
            },
          ],
        },
      ],
    });

    await tester.pumpWidget(_host(result, const Locale('en')));

    expect(find.text('no equilibrium found in the window'), findsOneWidget);
    expect(find.byType(MathInline), findsOneWidget);
    expect(find.textContaining('E1 = (0, 0)'), findsNothing);
  });

  testWidgets('the same step keys render Persian', (tester) async {
    final result = SolveResult.fromJson({
      'ok': true,
      'mode': 'topic',
      'type_id': 'growth',
      'steps': [
        {
          'title': '',
          'note': '',
          'title_key': 'step.title.read_the_final_value',
          'note_key': 'step.note.after_steps',
          'params': {'n': 4},
        },
      ],
    });

    await tester.pumpWidget(_host(result, const Locale('fa')));

    expect(find.text('مقدار نهایی را بخوانید'), findsOneWidget);
    expect(find.text('پس از 4 گام.'), findsOneWidget);
  });

  testWidgets('legacy steps keep their text until migrated', (tester) async {
    final result = SolveResult.fromJson({
      'ok': true,
      'mode': 'topic',
      'type_id': 'legacy',
      'steps': [
        {'title': 'Old title', 'note': 'Old note'},
      ],
    });

    await tester.pumpWidget(_host(result, const Locale('en')));

    expect(find.text('Old title'), findsOneWidget);
    expect(find.text('Old note'), findsOneWidget);
  });
}
