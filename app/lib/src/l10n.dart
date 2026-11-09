import 'package:flutter/material.dart';

import 'i18n/catalog.dart';

/// One selectable interface language.  Everything about it — code, display
/// names, direction and order — comes from the `__meta.*` keys of
/// `assets/i18n/<code>.json`, so adding a language means adding one file.
class AppLanguage {
  const AppLanguage(
    this.code,
    this.englishName,
    this.nativeName, {
    this.rtl = false,
    this.order = 999,
  });

  final String code;
  final String englishName;
  final String nativeName;
  final bool rtl;
  final int order;
}

const _fallbackLanguages = <AppLanguage>[
  AppLanguage('fa', 'Persian', 'Persian', rtl: true, order: 1),
  AppLanguage('en', 'English', 'English', order: 0),
];

/// The languages that have a loaded catalogue, sorted by `__meta.order`.
List<AppLanguage> get appLanguages {
  final i18n = I18n.instance;
  final languages = <AppLanguage>[];
  for (final code in i18n.languages) {
    final english = i18n.value(code, '__meta.english_name') ?? code;
    final native = i18n.value(code, '__meta.native_name') ?? english;
    final rtlFlag = i18n.value(code, '__meta.rtl');
    languages.add(
      AppLanguage(
        code,
        english,
        native,
        rtl: rtlFlag == null
            ? const {'fa', 'ar', 'he', 'ur'}.contains(code)
            : rtlFlag == 'true',
        order: int.tryParse(i18n.value(code, '__meta.order') ?? '') ?? 999,
      ),
    );
  }
  if (languages.isEmpty) return _fallbackLanguages;
  languages.sort((a, b) => a.order.compareTo(b.order));
  return languages;
}

AppLanguage languageFor(String code) {
  final languages = appLanguages;
  for (final language in languages) {
    if (language.code == code) return language;
  }
  return languages.first;
}

bool isSupportedLanguage(String code) =>
    appLanguages.any((language) => language.code == code);

class L10n {
  const L10n(this.locale);

  final Locale locale;

  String get code => locale.languageCode;

  /// Persian still drives the native engine and the Persian-only study notes.
  bool get isFa => code == 'fa';

  bool get isRtl {
    for (final language in appLanguages) {
      if (language.code == code) return language.rtl;
    }
    return code == 'fa' || code == 'ar' || code == 'he' || code == 'ur';
  }

  /// The single source of sentence direction for the UI layer.
  TextDirection get direction => isRtl ? TextDirection.rtl : TextDirection.ltr;

  bool has(String key) => I18n.instance.hasKey(key);

  /// Looks up a stable dotted key for this locale.  Only plain values
  /// (numbers, symbols) belong in [params]; formulas travel as separate math
  /// widgets and are never substituted into a sentence.
  String t(String key, [Map<String, Object?>? params]) =>
      I18n.instance.text(code, key, params);

  /// Reads the contiguous numbered keys `<prefix>.0`, `<prefix>.1`, ...
  /// Stops at the first missing index so content lists keep their length in
  /// the catalogue instead of in code.
  List<String> tList(String prefix) {
    final out = <String>[];
    for (var i = 0; ; i++) {
      final key = '$prefix.$i';
      if (!I18n.instance.hasKey(key)) break;
      out.add(t(key));
    }
    return out;
  }

  static L10n of(BuildContext context) => L10n(Localizations.localeOf(context));
}
