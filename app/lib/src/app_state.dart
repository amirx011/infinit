import 'package:flutter/material.dart';

import 'settings_store.dart';

class AppSettings extends ChangeNotifier {
  AppSettings({SettingsPersistence? persistence})
      : _persistence = persistence ?? PlatformSettingsPersistence();

  static const _themeKey = 'theme';
  static const _localeKey = 'locale';
  static const _textScaleKey = 'text_scale';
  static const _reduceMotionKey = 'reduce_motion';

  final SettingsPersistence _persistence;

  ThemeMode _themeMode = ThemeMode.dark;
  Locale _locale = const Locale('en');
  double _textScale = 1.0;
  bool _reduceMotion = false;
  bool _loaded = false;

  ThemeMode get themeMode => _themeMode;
  Locale get locale => _locale;
  double get textScale => _textScale;
  bool get reduceMotion => _reduceMotion;
  bool get loaded => _loaded;
  bool get isPersian => _locale.languageCode == 'fa';
  bool get isDark => _themeMode == ThemeMode.dark;

  Future<void> load() async {
    try {
      final values = await _persistence.load();
      final theme = values[_themeKey];
      final locale = values[_localeKey];
      _themeMode = switch (theme) {
        'light' => ThemeMode.light,
        'system' => ThemeMode.system,
        _ => ThemeMode.dark,
      };
      _locale = Locale(locale == 'fa' ? 'fa' : 'en');
      _textScale = switch (values[_textScaleKey]) {
        'large' => 1.15,
        _ => 1.0,
      };
      _reduceMotion = values[_reduceMotionKey] == 'true';
    } catch (_) {
      _themeMode = ThemeMode.dark;
      _locale = const Locale('en');
      _textScale = 1.0;
      _reduceMotion = false;
    }
    _loaded = true;
    notifyListeners();
  }

  Future<void> setThemeMode(ThemeMode mode) async {
    if (_themeMode == mode) return;
    _themeMode = mode;
    notifyListeners();
    await _persist();
  }

  Future<void> setLocale(Locale locale) async {
    final code = locale.languageCode == 'fa' ? 'fa' : 'en';
    if (_locale.languageCode == code) return;
    _locale = Locale(code);
    notifyListeners();
    await _persist();
  }

  Future<void> setTextScale(double scale) async {
    final normalised = scale >= 1.1 ? 1.15 : 1.0;
    if (_textScale == normalised) return;
    _textScale = normalised;
    notifyListeners();
    await _persist();
  }

  Future<void> setReduceMotion(bool value) async {
    if (_reduceMotion == value) return;
    _reduceMotion = value;
    notifyListeners();
    await _persist();
  }

  Future<void> resetDefaults() async {
    _themeMode = ThemeMode.dark;
    _locale = const Locale('en');
    _textScale = 1.0;
    _reduceMotion = false;
    notifyListeners();
    await _persist();
  }

  void toggleTheme() => setThemeMode(isDark ? ThemeMode.light : ThemeMode.dark);

  Future<void> _persist() async {
    try {
      await _persistence.save({
        _themeKey: switch (_themeMode) {
          ThemeMode.light => 'light',
          ThemeMode.system => 'system',
          ThemeMode.dark => 'dark',
        },
        _localeKey: _locale.languageCode,
        _textScaleKey: _textScale >= 1.1 ? 'large' : 'normal',
        _reduceMotionKey: _reduceMotion ? 'true' : 'false',
      });
    } catch (_) {
      // Preferences are best-effort only.
    }
  }
}
