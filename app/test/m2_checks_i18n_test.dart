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
        'check.initial_condition': 'شرط اولیه',
        'check.long_term_temperature': 'دمای بلندمدت',
        'check.approaches_room': 'با بزرگ شدن t، دما به {room} میل می‌کند.',
        'check.table_steps': 'جدول {n} گام دارد.',
      },
      'en': {
        'check.initial_condition': 'Initial condition',
        'check.long_term_temperature': 'Long-term temperature',
        'check.approaches_room': 'As t grows, T approaches {room}.',
        'check.table_steps': 'The table contains {n} steps.',
      },
    });
  });

  tearDown(() {
    I18n.instance = I18n(const {});
  });

  testWidgets('a keyed check renders name and math separately', (tester) async {
    final result = SolveResult.fromJson({
      'ok': true,
      'mode': 'topic',
      'type_id': 'growth',
      'checks': [
        {
          'name': '',
          'ok': true,
          'detail': '',
          'name_key': 'check.initial_condition',
          'detail_math': 'y(0) = 5',
        },
      ],
    });

    await tester.pumpWidget(_host(result, const Locale('en')));

    expect(find.text('Initial condition'), findsOneWidget);
    expect(find.text('Initial condition: y(0) = 5'), findsNothing);
  });

  testWidgets('a check detail uses params instead of concatenation', (
    tester,
  ) async {
    final result = SolveResult.fromJson({
      'ok': true,
      'mode': 'topic',
      'type_id': 'cooling',
      'checks': [
        {
          'name': '',
          'ok': true,
          'detail': '',
          'name_key': 'check.long_term_temperature',
          'detail_key': 'check.approaches_room',
          'params': {'room': 5},
        },
      ],
    });

    await tester.pumpWidget(_host(result, const Locale('en')));

    expect(find.text('Long-term temperature'), findsOneWidget);
    expect(find.text('As t grows, T approaches 5.'), findsOneWidget);
    expect(find.textContaining(': '), findsNothing);
  });

  testWidgets('the same key renders Persian when the locale is fa', (
    tester,
  ) async {
    final result = SolveResult.fromJson({
      'ok': true,
      'mode': 'topic',
      'type_id': 'cooling',
      'checks': [
        {
          'name': '',
          'ok': true,
          'detail': '',
          'name_key': 'check.long_term_temperature',
          'detail_key': 'check.approaches_room',
          'params': {'room': 5},
        },
      ],
    });

    await tester.pumpWidget(_host(result, const Locale('fa')));

    expect(find.text('دمای بلندمدت'), findsOneWidget);
    expect(find.text('با بزرگ شدن t، دما به 5 میل می‌کند.'), findsOneWidget);
  });

  testWidgets('legacy checks keep their concatenated text until migrated', (
    tester,
  ) async {
    final result = SolveResult.fromJson({
      'ok': true,
      'mode': 'topic',
      'type_id': 'legacy',
      'checks': [
        {'name': 'Old name', 'ok': false, 'detail': 'old detail'},
      ],
    });

    await tester.pumpWidget(_host(result, const Locale('en')));

    expect(find.text('Old name: old detail'), findsOneWidget);
  });
}
