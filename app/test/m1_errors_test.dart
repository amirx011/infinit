import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/models.dart';
import 'package:infinit/src/theme.dart';
import 'package:infinit/src/widgets.dart';

void main() {
  setUp(() {
    I18n.instance = I18n.fromMaps({
      'fa': {
        'error.singular': 'undefined at x = {point}',
        'error.unknown_function': 'unknown function',
        'error.unavailable': 'could not analyse',
        'note.interpreted_as': 'read as',
      },
      'en': {
        'error.singular': 'undefined at x = {point}',
        'error.unknown_function': 'unknown function',
        'error.unavailable': 'could not analyse',
        'note.interpreted_as': 'read as',
      },
    });
  });

  tearDown(() {
    I18n.instance = I18n(const {});
  });

  testWidgets('a coded singular error renders a whole translated sentence', (
    tester,
  ) async {
    final result = SolveResult.fromJson({
      'ok': false,
      'mode': 'integral',
      'error_code': 'singular',
      'error_point': 0,
    });

    await tester.pumpWidget(
      MaterialApp(
        theme: ThemeData(extensions: const [OdeColors.light]),
        home: Scaffold(
          body: ResultView(result: result, l10n: const L10n(Locale('fa'))),
        ),
      ),
    );

    expect(find.text('undefined at x = 0'), findsOneWidget);
  });

  testWidgets('an unknown function uses the catalogue key', (tester) async {
    final result = SolveResult.fromJson({
      'ok': false,
      'mode': 'integral',
      'error_code': 'unknown_function',
    });

    await tester.pumpWidget(
      MaterialApp(
        theme: ThemeData(extensions: const [OdeColors.light]),
        home: Scaffold(
          body: ResultView(result: result, l10n: const L10n(Locale('fa'))),
        ),
      ),
    );

    expect(find.text('unknown function'), findsOneWidget);
  });
}
