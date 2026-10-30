import 'package:flutter/material.dart';

import 'settings_store.dart';

class AppSettings extends ChangeNotifier {
  AppSettings({SettingsPersistence? persistence})
      : _persistence = persistence ?? PlatformSettingsPersistence();

  static const _themeKey = 'theme';
  static const _localeKey = 'locale';

  final SettingsPersistence _persistence;

  ThemeMode _themeMode = ThemeMode.dark;
  Locale _locale = const Locale('en');
  bool _loaded = false;

  ThemeMode get themeMode => _themeMode;
  Locale get locale => _locale;
  bool get loaded => _loaded;
  bool get isPersian => _locale.languageCode == 'fa';
  bool get isDark => _themeMode == ThemeMode.dark;

  Future<void> load() async {
    try {
      final values = await _persistence.load();
      final theme = values[_themeKey];
      final locale = values[_localeKey];
      _themeMode = theme == 'light' ? ThemeMode.light : ThemeMode.dark;
      _locale = Locale(locale == 'fa' ? 'fa' : 'en');
    } catch (_) {
      _themeMode = ThemeMode.dark;
      _locale = const Locale('en');
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

  void toggleTheme() => setThemeMode(isDark ? ThemeMode.light : ThemeMode.dark);

  Future<void> _persist() async {
    try {
      await _persistence.save({
        _themeKey: _themeMode == ThemeMode.light ? 'light' : 'dark',
        _localeKey: _locale.languageCode,
      });
    } catch (_) {
      // Preferences are best-effort only.
    }
  }
}
