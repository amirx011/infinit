import 'package:flutter/material.dart';
import 'package:flutter_math_fork/flutter_math.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/curriculum_articles.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/pages/curriculum_article_page.dart';
import 'package:infinit/src/pages/study_article_page.dart';
import 'package:infinit/src/study_articles.dart';
import 'package:infinit/src/theme.dart';

Widget _curriculum(CurriculumArticle article, String code) => MaterialApp(
  theme: buildOdeTheme(dark: false),
  home: Directionality(
    textDirection: code == 'fa' ? TextDirection.rtl : TextDirection.ltr,
    child: Scaffold(
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(24),
        child: CurriculumArticlePage(
          article: article,
          l10n: L10n(Locale(code)),
          engine: null,
          onSolve: (_) {},
        ),
      ),
    ),
  ),
);

Widget _study(StudyArticle article, String code) => MaterialApp(
  theme: buildOdeTheme(dark: false),
  home: Directionality(
    textDirection: code == 'fa' ? TextDirection.rtl : TextDirection.ltr,
    child: Scaffold(
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(24),
        child: StudyArticlePage(article: article, l10n: L10n(Locale(code))),
      ),
    ),
  ),
);

void _expectCleanMath(WidgetTester tester) {
  final widgets = tester.widgetList<Math>(find.byType(Math)).toList();
  final broken = widgets.where((m) => m.parseError != null).toList();
  expect(
    broken,
    isEmpty,
    reason: broken.map((m) => m.parseError!.message).join('\n'),
  );
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  for (final code in ['fa', 'en']) {
    testWidgets('every curriculum formula parses ($code)', (tester) async {
      await tester.binding.setSurfaceSize(const Size(1200, 4000));
      addTearDown(() => tester.binding.setSurfaceSize(null));
      for (final article in kCurriculumArticles) {
        await tester.pumpWidget(_curriculum(article, code));
        await tester.pumpAndSettle();
        _expectCleanMath(tester);
      }
    });

    testWidgets('every study formula parses ($code)', (tester) async {
      await tester.binding.setSurfaceSize(const Size(1200, 4000));
      addTearDown(() => tester.binding.setSurfaceSize(null));
      for (final article in kStudyArticles) {
        await tester.pumpWidget(_study(article, code));
        await tester.pumpAndSettle();
        _expectCleanMath(tester);
      }
    });
  }
}
