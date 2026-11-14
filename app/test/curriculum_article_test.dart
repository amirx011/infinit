import 'package:flutter/material.dart';
import 'package:flutter_math_fork/flutter_math.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/main.dart';
import 'package:infinit/src/app_state.dart';
import 'package:infinit/src/curriculum_articles.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/native_engine.dart';
import 'package:infinit/src/pages/curriculum_article_page.dart';
import 'package:infinit/src/settings_store.dart';
import 'package:infinit/src/theme.dart';
import 'package:infinit/src/widgets.dart';

Widget _host({
  required CurriculumArticle article,
  required String code,
  bool rtl = false,
  NativeEngine? engine,
  ValueChanged<String>? onSolve,
}) {
  return MaterialApp(
    theme: buildOdeTheme(dark: false),
    home: Directionality(
      textDirection: rtl ? TextDirection.rtl : TextDirection.ltr,
      child: Scaffold(
        body: SingleChildScrollView(
          padding: const EdgeInsets.all(24),
          child: CurriculumArticlePage(
            article: article,
            l10n: L10n(Locale(code)),
            engine: engine,
            onSolve: onSolve ?? (_) {},
          ),
        ),
      ),
    ),
  );
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
  TestWidgetsFlutterBinding.ensureInitialized();

  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  test('every curriculum article key resolves in all catalogued locales', () {
    // Chapter keys (subtitle) are shared by every article in that chapter, so
    // collect unique keys only.
    final keys = <String>{
      for (final article in kCurriculumArticles) ...[
        article.titleKey,
        article.subtitleKey,
        article.ideaKey,
        article.algorithmKey,
        article.exampleKey,
        article.answerKey,
        article.pitfallKey,
      ],
    };

    for (final language in I18n.instance.languages) {
      for (final key in keys) {
        final value = I18n.instance.value(language, key);
        expect(value, isNotNull, reason: '$language is missing $key');
        expect(value, isNotEmpty, reason: '$language has an empty $key');
        expect(value, isNot(key), reason: '$language did not translate $key');
      }
    }
  });

  test('every curriculum article carries non-empty mathematics', () {
    for (final article in kCurriculumArticles) {
      expect(article.standard.plain, isNotEmpty, reason: '${article.index}');
      expect(article.standard.latex, isNotEmpty, reason: '${article.index}');
      expect(
        article.exampleAnswerMath.plain,
        isNotEmpty,
        reason: '${article.index}',
      );
      expect(
        article.exampleAnswerMath.latex,
        isNotEmpty,
        reason: '${article.index}',
      );
    }
  });

  testWidgets('renders a curriculum article with algorithm and example', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2600));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    final article = kCurriculumArticles.firstWhere((a) => a.index == 102);
    await tester.pumpWidget(_host(article: article, code: 'en'));
    await tester.pumpAndSettle();

    expect(find.text('Homogeneous equations'), findsOneWidget);
    expect(find.text('Short answer'), findsOneWidget);
    expect(find.text('Standard form'), findsOneWidget);
    expect(find.text('Algorithm'), findsOneWidget);
    expect(find.text('Worked example'), findsOneWidget);
    expect(find.text('Simplest answer'), findsOneWidget);
    expect(find.text('Common pitfalls'), findsOneWidget);
    expect(find.text('Step-by-step solution'), findsOneWidget);
    expect(find.text('Solve an equation'), findsOneWidget);
    expect(find.byType(Math), findsWidgets);

    // The computation comes first; the explanations start below it.
    expect(
      tester.getTopLeft(find.text('Step-by-step solution')).dy,
      lessThan(tester.getTopLeft(find.text('Short answer')).dy),
    );

    // Each algorithm line is rendered as its own numbered step.
    final l = const L10n(Locale('en'));
    final steps = article
        .algorithm(l)
        .split('\n')
        .where((line) => line.trim().isNotEmpty)
        .length;
    expect(find.text('1'), findsOneWidget);
    expect(find.text('$steps'), findsOneWidget);
  });

  testWidgets('routes a curriculum slot to its article page', (tester) async {
    final settings = AppSettings(persistence: MemorySettingsPersistence());
    await settings.load();
    await settings.setStartPage(102);

    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    // Index 102 used to fall through to the coming-soon page.
    expect(find.text('Homogeneous equations'), findsWidgets);
    expect(find.text('Algorithm'), findsOneWidget);
    expect(find.text('Standard form'), findsOneWidget);
    expect(find.text('Worked example'), findsOneWidget);
  });

  testWidgets('hands the worked example to the shared solver', (tester) async {
    final settings = AppSettings(persistence: MemorySettingsPersistence());
    await settings.load();
    await settings.setStartPage(102);

    await tester.pumpWidget(InfinitApp(settings: settings));
    await tester.pump(const Duration(milliseconds: 300));

    final article = kCurriculumArticles.firstWhere((a) => a.index == 102);
    final open = find.widgetWithText(FilledButton, 'Solve an equation');
    await tester.ensureVisible(open);
    await tester.pumpAndSettle();
    await tester.tap(open);
    await tester.pumpAndSettle();

    final field = tester.widget<TextField>(find.byType(TextField).first);
    expect(field.controller?.text, article.input);
  });

  testWidgets('computes the difference-equation table', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2600));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    final article = kCurriculumArticles.firstWhere((a) => a.index == 68);
    await tester.pumpWidget(_host(article: article, code: 'en'));
    await tester.pumpAndSettle();

    await tester.tap(find.text('Show the answer and steps'));
    await tester.pumpAndSettle();

    // y0 = 1, a = 0.5, b = 3 gives y1 = 3.5 and y2 = 4.75.
    expect(find.text('3.5'), findsOneWidget);
    expect(find.text('4.75'), findsOneWidget);
    expect(find.text('Converges'), findsOneWidget);
    final fixedPoint = tester.widget<MathText>(
      find.byKey(const ValueKey('difference-fixed-point')),
    );
    expect(fixedPoint.text, contains('6'));
  });

  testWidgets('draws the direction field for the section example', (
    tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2600));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    final article = kCurriculumArticles.firstWhere((a) => a.index == 98);
    await tester.pumpWidget(
      _host(article: article, code: 'en', engine: NativeEngine.instance),
    );
    // The loading indicator keeps scheduling frames, so pumpAndSettle would
    // advance the fake clock past the plot timeout before the isolate answers.
    await tester.pump();

    await _awaitEngine(tester, find.byKey(const ValueKey('field-ready')));
    expect(find.byKey(const ValueKey('field-ready')), findsOneWidget);
  });

  testWidgets('classifies an equation with the engine', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2600));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    final article = kCurriculumArticles.firstWhere((a) => a.index == 66);
    await tester.pumpWidget(
      _host(article: article, code: 'en', engine: NativeEngine.instance),
    );
    await tester.pumpAndSettle();

    await tester.tap(find.text('Classify the equation'));
    await _awaitEngine(tester, find.text('Order 2'));

    expect(find.text('Order 2'), findsOneWidget);
    expect(find.text('Constant coefficients'), findsOneWidget);
  });

  testWidgets('checks existence and uniqueness at a point', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2600));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    final article = kCurriculumArticles.firstWhere((a) => a.index == 67);
    await tester.pumpWidget(
      _host(article: article, code: 'en', engine: NativeEngine.instance),
    );
    await tester.pumpAndSettle();

    await tester.tap(find.text('Analyse the function'));
    await _awaitEngine(tester, find.textContaining('Both f and'));

    expect(find.text('Value at the point'), findsOneWidget);
    expect(find.text('Partial derivatives'), findsOneWidget);
    expect(find.textContaining('Both f and'), findsOneWidget);
  });

  testWidgets('renders the Persian article right to left', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1200, 2600));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    final article = kCurriculumArticles.firstWhere((a) => a.index == 100);
    await tester.pumpWidget(_host(article: article, code: 'fa', rtl: true));
    await tester.pumpAndSettle();

    final title = find.text(article.title(const L10n(Locale('fa'))));
    expect(Directionality.of(tester.element(title)), TextDirection.rtl);
    expect(find.byType(Math), findsWidgets);
  });
}
