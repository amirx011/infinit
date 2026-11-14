import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/main.dart';
import 'package:infinit/src/curriculum_articles.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';

/// Guards for the textbook-traceability navigation pass.
void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  const removed = <int>{17, 21, 41, 54, 55, 56};

  test('drawer and command palette exclude out-of-core indices', () {
    final indices = navReachableIndices(const L10n(Locale('en')));
    for (final index in removed) {
      expect(
        indices,
        isNot(contains(index)),
        reason: 'index $index must not be reachable',
      );
    }
    expect(indices, contains(4));
    expect(indices, contains(20));
    expect(indices, contains(35));
  });

  test('every drawer label resolves in the English catalogue', () {
    final labels = navResolvedLabels(const L10n(Locale('en')));
    final rawKey = RegExp(r'^(nav|ui|topic)\.');
    for (final label in labels) {
      expect(label, isNotEmpty);
      expect(
        label,
        isNot(matches(rawKey)),
        reason: 'unresolved label "$label"',
      );
    }
  });

  test('every catalogued locale carries the new nav labels', () {
    for (final language in I18n.instance.languages) {
      for (var index = 66; index <= 115; index++) {
        final value = I18n.instance.value(language, 'nav.$index.label');
        expect(
          value,
          isNotNull,
          reason: '$language is missing nav.$index.label',
        );
        expect(value, isNotEmpty);
      }
    }
  });

  test('new curriculum labels are translated, not English copies', () {
    final newKeys = <String>[
      for (var index = 66; index <= 115; index++) 'nav.$index.label',
      for (final chapter in [
        'ch2',
        'ch3',
        'ch4',
        'ch6',
        'ch7',
        'ch8',
        'ch9',
        'ch10',
        'ch11',
      ])
        'nav.group.$chapter',
      for (final group in [
        'calculus_foundations',
        'calculus_differentiation',
        'calculus_integration',
        'calculus_series',
        'calculus_further',
      ])
        'nav.group.$group',
    ];

    for (final language in I18n.instance.languages) {
      if (language == 'en') continue;
      for (final key in newKeys) {
        expect(
          I18n.instance.value(language, key),
          isNot(I18n.instance.value('en', key)),
          reason: '$language still carries the English text for $key',
        );
      }
    }

    final persian = RegExp(r'[\u0600-\u06FF]');
    for (final key in newKeys) {
      expect(
        I18n.instance.value('fa', key),
        matches(persian),
        reason: 'nav label $key is not translated into Persian',
      );
    }
  });

  test('Persian drawer carries no untranslated English labels', () {
    final englishWord = RegExp(r'[A-Za-z]{3,}');
    for (final label in navResolvedLabels(const L10n(Locale('fa')))) {
      expect(
        label,
        isNot(matches(englishWord)),
        reason: 'the Persian drawer still shows "$label" in English',
      );
    }
  });

  test('drawer has no single-child groups', () {
    for (final locale in ['en', 'fa']) {
      expect(
        navSingleChildGroupLabels(L10n(Locale(locale))),
        isEmpty,
        reason: '$locale still wraps a lone entry in a group',
      );
    }
  });

  test('every curriculum index has a real page branch', () {
    // Indices 0–65 render a dedicated page (topic workbench, educational
    // article or theorem/calculator screen). The Laplace additions
    // (72, 73, 74, 114, 115) are workbench topics, 75–97 and 111–113 are the
    // reserved curriculum workbenches, and the chapter 2–4 slots render a
    // curriculum article. No slot falls back to the shared placeholder.
    final pageBranches = <int>{
      for (var index = 0; index <= 65; index++) index,
      72,
      73,
      74,
      114,
      115,
      for (var index = 75; index <= 97; index++) index,
      111,
      112,
      113,
      for (final article in kCurriculumArticles) article.index,
    };

    expect(pageBranches.length, 116);
    for (var index = 0; index <= 115; index++) {
      expect(
        pageBranches,
        contains(index),
        reason: 'index $index has no page branch',
      );
      expect(
        kComingSoonIndices,
        isNot(contains(index)),
        reason: 'index $index still renders the coming-soon page',
      );
    }
  });
}
