import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  setUp(() {
    I18n.instance = I18n.fromMaps({
      'fa': {
        'error.singular': 'at x = {point}',
        'validation.match': 'ok {value}',
      },
      'en': {
        'error.singular': 'undefined at x = {point}',
        'validation.match': 'ok {value}',
      },
    });
  });

  tearDown(() {
    I18n.instance = I18n(const {});
  });

  test('dotted keys use params and the requested language', () {
    expect(const L10n(Locale('fa')).t('error.singular', {'point': 0}), 'at x = 0');
    expect(
      const L10n(Locale('en')).t('error.singular', {'point': 2}),
      'undefined at x = 2',
    );
  });

  test('missing keys stay visible and never throw', () {
    expect(const L10n(Locale('fa')).t('missing.key'), 'missing.key');
  });

  test('untranslated languages fall back to the default catalogue', () {
    expect(const L10n(Locale('de')).t('error.singular', {'point': 1}), 'at x = 1');
  });

  test('legacy English-key tables still resolve', () {
    expect(const L10n(Locale('fr')).t('System'), isNot('System'));
    expect(const L10n(Locale('en')).t('Settings'), 'Settings');
  });

  test('ships the fa and en catalogues', () async {
    final catalog = await I18n.load();
    expect(catalog.hasKey('error.singular'), isTrue);
    expect(catalog.hasKey('validation.match'), isTrue);
  });
}
