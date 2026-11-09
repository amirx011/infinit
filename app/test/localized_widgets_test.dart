import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/i18n/result.dart';
import 'package:infinit/src/i18n/result_view.dart';
import 'package:infinit/src/i18n/sentence.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/models.dart';

void main() {
  setUp(() {
    I18n.instance = I18n.fromMaps({
      'fa': {
        'taylor.value_at_center': 'value at center',
        'note.interpreted_as': 'read as',
        'note.rounded': 'rounded',
        'validation.match': 'ok {value}',
        'error.singular': 'undefined at {point}',
      },
      'en': {
        'taylor.value_at_center': 'value at center EN',
        'note.interpreted_as': 'read as EN',
        'note.rounded': 'rounded EN',
        'validation.match': 'ok {value} EN',
        'error.singular': 'undefined at {point} EN',
      },
    });
  });

  tearDown(() {
    I18n.instance = I18n(const {});
  });

  testWidgets('sentence follows the language direction and math stays LTR', (
    tester,
  ) async {
    var mathDirection = TextDirection.rtl;
    await tester.pumpWidget(
      MaterialApp(
        home: LocalizedSentence(
          l10n: const L10n(Locale('fa')),
          messageKey: 'taylor.value_at_center',
          math: [
            Builder(
              builder: (context) {
                mathDirection = Directionality.of(context);
                return const Text('4y');
              },
            ),
          ],
        ),
      ),
    );

    final text = tester.widget<Text>(find.text('value at center'));
    expect(text.textDirection, TextDirection.rtl);
    expect(mathDirection, TextDirection.ltr);
  });

  testWidgets('sentence interpolates plain params only', (tester) async {
    await tester.pumpWidget(
      MaterialApp(
        home: LocalizedSentence(
          l10n: const L10n(Locale('en')),
          messageKey: 'validation.match',
          params: const {'value': 3},
        ),
      ),
    );

    final text = tester.widget<Text>(find.text('ok 3 EN'));
    expect(text.textDirection, TextDirection.ltr);
  });

  testWidgets('result view shows an error key instead of raw text', (
    tester,
  ) async {
    final result = LocalizedResult.fromJson({
      'error': {
        'error': 'error.singular',
        'params': {'point': 0},
      },
    });

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: LocalizedResultView(
            l10n: const L10n(Locale('fa')),
            result: result,
            mathBuilder: (math) => Text('MATH:${math.plain}'),
          ),
        ),
      ),
    );

    expect(find.text('undefined at 0'), findsOneWidget);
  });

  testWidgets('result view renders answer, steps, notices and validations', (
    tester,
  ) async {
    final result = LocalizedResult.fromJson({
      'answer_math': 'x^2',
      'steps': [
        {
          'step': 'taylor.value_at_center',
          'math': '4y',
          'note_key': 'note.interpreted_as',
        },
      ],
      'validations': [
        {
          'validation': 'validation.match',
          'ok': true,
          'params': {'value': 3},
        },
      ],
      'notices': [
        {'notice': 'note.rounded', 'level': 'warning'},
      ],
    });

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: LocalizedResultView(
            l10n: const L10n(Locale('en')),
            result: result,
            mathBuilder: (math) => Text('MATH:${math.plain}'),
          ),
        ),
      ),
    );

    expect(find.text('MATH:x^2'), findsOneWidget);
    expect(find.text('value at center EN'), findsOneWidget);
    expect(find.text('MATH:4y'), findsOneWidget);
    expect(find.text('read as EN'), findsOneWidget);
    expect(find.text('rounded EN'), findsOneWidget);
    expect(find.text('ok 3 EN'), findsOneWidget);
  });

  test('math accepts plain and structured payloads', () {
    final plain = LocalizedStep.fromJson({'step': 'a.b', 'math': 'x+1'});
    final structured = LocalizedStep.fromJson({
      'step': 'a.b',
      'math': {'plain': 'x+1', 'pretty': 'x + 1', 'latex': 'x+1'},
    });

    expect(plain.math, isA<ExView>());
    expect(plain.math?.latex, 'x+1');
    expect(structured.math?.pretty, 'x + 1');
  });
}
