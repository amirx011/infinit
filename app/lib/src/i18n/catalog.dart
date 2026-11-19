import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

/// Language-independent message catalogue.
///
/// Keys are stable ASCII dotted strings (`error.singular`), values live in
/// `assets/i18n/<language>.json`.  A missing key renders as the key itself so a
/// release build never crashes; the L1/L2 guard tests keep the files in parity
/// and catch keys that code uses but the catalogue lacks.
class I18n {
  I18n(this._catalogues, {this.defaultLanguage = 'fa'});

  final Map<String, Map<String, String>> _catalogues;
  final String defaultLanguage;

  /// Replaced by [load] during startup and by tests that need a fixture.
  static I18n instance = I18n(const {});

  static const _prefix = 'assets/i18n/';
  static const _fallbackLanguages = ['fa', 'en'];

  static I18n fromMaps(
    Map<String, Map<String, String>> catalogues, {
    String defaultLanguage = 'fa',
  }) => I18n(catalogues, defaultLanguage: defaultLanguage);

  static Future<I18n> load({AssetBundle? bundle}) async {
    final assets = bundle ?? rootBundle;
    final catalogues = <String, Map<String, String>>{};
    for (final language in await _languages(assets)) {
      final raw = await _read(assets, language);
      if (raw != null) catalogues[language] = raw;
    }
    return I18n(catalogues);
  }

  static Future<List<String>> _languages(AssetBundle assets) async {
    try {
      final manifest = await AssetManifest.loadFromAssetBundle(assets);
      final languages = <String>{};
      for (final path in manifest.listAssets()) {
        if (!path.startsWith(_prefix) || !path.endsWith('.json')) continue;
        languages.add(path.substring(_prefix.length, path.length - 5));
      }
      if (languages.isNotEmpty) return languages.toList()..sort();
    } catch (_) {
      // Fall through: an older bundle still gets the two base languages.
    }
    return _fallbackLanguages;
  }

  static Future<Map<String, String>?> _read(
    AssetBundle assets,
    String language,
  ) async {
    try {
      final decoded = jsonDecode(
        await assets.loadString('$_prefix$language.json'),
      );
      if (decoded is! Map) return null;
      return decoded.map(
        (key, value) => MapEntry(key.toString(), value.toString()),
      );
    } catch (_) {
      return null;
    }
  }

  bool hasKey(String key) {
    for (final catalogue in _catalogues.values) {
      if (catalogue.containsKey(key)) return true;
    }
    return false;
  }

  /// Languages that have a loaded catalogue, in declaration order.
  Iterable<String> get languages => _catalogues.keys;

  /// Every key of the default catalogue (the reference set for parity).
  Iterable<String> get keys =>
      _catalogues[defaultLanguage]?.keys ?? const <String>[];

  /// A raw value for one language, without fallback.  Used by metadata
  /// lookups (display names, direction) and by the export tooling.
  String? value(String language, String key) => _catalogues[language]?[key];

  /// The English source of a key, used as the lookup key in the legacy Dart
  /// tables until every language has its own JSON catalogue.
  String? english(String key) => _catalogues['en']?[key];

  /// Looks up [key] for [language]; falls back to the default language, then to
  /// the key itself.  Only plain values (numbers, symbols) belong in [params];
  /// formulas travel as separate math widgets.
  String text(String language, String key, [Map<String, Object?>? params]) {
    final template =
        _catalogues[language]?[key] ?? _catalogues[defaultLanguage]?[key];
    if (template == null) {
      if (kDebugMode) debugPrint('i18n: missing "$key" for "$language"');
      return key;
    }
    return _fill(language, template, params);
  }

  static final _placeholder = RegExp(r'\{([A-Za-z_][A-Za-z0-9_]*)\}');

  String _fill(
    String language,
    String template,
    Map<String, Object?>? params, [
    int depth = 0,
  ]) {
    if (params == null || params.isEmpty) return template;
    return template.replaceAllMapped(_placeholder, (match) {
      final value = params[match[1]];
      if (value == null) return match[0]!;
      // A value of the form `@key` is another catalogue key (for example a
      // translated method name); resolve it instead of showing the id.
      if (value is String && value.startsWith('@') && depth < 4) {
        final nested =
            _catalogues[language]?[value.substring(1)] ??
            _catalogues[defaultLanguage]?[value.substring(1)];
        if (nested != null) {
          return _fill(language, nested, params, depth + 1);
        }
      }
      return value.toString();
    });
  }
}
