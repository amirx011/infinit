import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';

import 'l10n.dart';
import 'settings_store.dart';

/// Text sizes offered by the settings page. Stored as a number so the list can
/// grow without another migration.
const kTextScaleOptions = <double>[0.9, 1.0, 1.12, 1.25];

class AppSettings extends ChangeNotifier {
  AppSettings({SettingsPersistence? persistence})
    : _persistence = persistence ?? PlatformSettingsPersistence();

  static const _themeKey = 'theme';
  static const _localeKey = 'locale';
  static const _textScaleKey = 'text_scale';
  static const _reduceMotionKey = 'reduce_motion';
  static const _paletteKey = 'palette';
  static const _highContrastKey = 'high_contrast';
  static const _compactKey = 'compact';
  static const _startPageKey = 'start_page';
  static const _showStepsKey = 'show_steps';
  static const _showChecksKey = 'show_checks';
  static const _showGraphsKey = 'show_graphs';
  static const _decimalPlacesKey = 'decimal_places';
  static const _restoreSessionKey = 'restore_session';
  static const _lastPageKey = 'last_page';
  static const _hapticsKey = 'haptics';
  static const _showTheoryKey = 'show_theory';
  static const _copyLatexKey = 'copy_latex';
  static const _desktopModeKey = 'desktop_mode';

  final SettingsPersistence _persistence;

  ThemeMode _themeMode = ThemeMode.dark;
  Locale _locale = defaultLocale();
  double _textScale = 1.0;
  bool _reduceMotion = false;
  String _palette = 'indigo';
  bool _highContrast = false;
  bool _compact = false;
  int _startPage = 0;
  bool _showSteps = true;
  bool _showChecks = true;
  bool _showGraphs = true;
  int _decimalPlaces = 0; // 0 means automatic precision
  bool _restoreSession = false;
  int _lastPage = 0;
  bool _haptics = true;
  bool _showTheory = true;
  bool _copyLatex = false;
  bool _desktopMode = false;
  bool _loaded = false;

  ThemeMode get themeMode => _themeMode;
  Locale get locale => _locale;
  double get textScale => _textScale;
  bool get reduceMotion => _reduceMotion;
  String get palette => _palette;
  bool get highContrast => _highContrast;
  bool get compact => _compact;
  int get startPage => _startPage;
  bool get showSteps => _showSteps;
  bool get showChecks => _showChecks;
  bool get showGraphs => _showGraphs;
  int get decimalPlaces => _decimalPlaces;
  bool get restoreSession => _restoreSession;
  int get lastPage => _lastPage;
  bool get haptics => _haptics;
  bool get showTheory => _showTheory;
  bool get copyLatex => _copyLatex;
  bool get desktopMode => _desktopMode;
  bool get loaded => _loaded;
  bool get isMobile =>
      !kIsWeb &&
      (defaultTargetPlatform == TargetPlatform.android ||
          defaultTargetPlatform == TargetPlatform.iOS);

  /// Page opened at launch: the last visited one when the reader asked for it.
  int get resumePage => _restoreSession ? _lastPage : _startPage;
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
      _locale = Locale(
        isSupportedLanguage(locale ?? '')
            ? locale!
            : defaultLocale().languageCode,
      );
      _textScale = _readTextScale(values[_textScaleKey]);
      _reduceMotion = values[_reduceMotionKey] == 'true';
      _palette = _knownPalette(values[_paletteKey]);
      _highContrast = values[_highContrastKey] == 'true';
      _compact = values[_compactKey] == 'true';
      _startPage = int.tryParse(values[_startPageKey] ?? '') ?? 0;
      _showSteps = values[_showStepsKey] != 'false';
      _showChecks = values[_showChecksKey] != 'false';
      _showGraphs = values[_showGraphsKey] != 'false';
      _decimalPlaces = _readDecimalPlaces(values[_decimalPlacesKey]);
      _restoreSession = values[_restoreSessionKey] == 'true';
      _lastPage = int.tryParse(values[_lastPageKey] ?? '') ?? 0;
      _haptics = values[_hapticsKey] != 'false';
      _showTheory = values[_showTheoryKey] != 'false';
      _copyLatex = values[_copyLatexKey] == 'true';
      _desktopMode = values[_desktopModeKey] == 'true';
    } catch (_) {
      _resetFields();
    }
    _loaded = true;
    notifyListeners();
    await _persistence.setLauncherPalette(_palette);
  }

  static double _readTextScale(String? raw) {
    // Older builds wrote the words "normal" and "large".
    if (raw == 'large') return 1.12;
    final value = double.tryParse(raw ?? '');
    if (value == null) return 1.0;
    final clamped = value.clamp(
      kTextScaleOptions.first,
      kTextScaleOptions.last,
    );
    return kTextScaleOptions.reduce(
      (best, option) =>
          (option - clamped).abs() < (best - clamped).abs() ? option : best,
    );
  }

  static String _knownPalette(String? raw) => switch (raw) {
    'ocean' || 'indigo' || 'emerald' || 'amber' || 'rose' || 'graphite' => raw!,
    _ => 'indigo',
  };

  static int _readDecimalPlaces(String? raw) {
    final value = int.tryParse(raw ?? '') ?? 0;
    return const [0, 2, 4, 6].contains(value) ? value : 0;
  }

  Future<void> setThemeMode(ThemeMode mode) async {
    if (_themeMode == mode) return;
    _themeMode = mode;
    notifyListeners();
    await _persist();
  }

  Future<void> setLocale(Locale locale) async {
    final code = isSupportedLanguage(locale.languageCode)
        ? locale.languageCode
        : 'en';
    if (_locale.languageCode == code) return;
    _locale = Locale(code);
    notifyListeners();
    await _persist();
  }

  Future<void> setTextScale(double scale) async {
    final normalised = scale.clamp(
      kTextScaleOptions.first,
      kTextScaleOptions.last,
    );
    if ((_textScale - normalised).abs() < 0.001) return;
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

  Future<void> setPalette(String value) async {
    final palette = _knownPalette(value);
    if (_palette == palette) return;
    _palette = palette;
    notifyListeners();
    await _persist();
    await _persistence.setLauncherPalette(palette);
  }

  Future<void> setHighContrast(bool value) async {
    if (_highContrast == value) return;
    _highContrast = value;
    notifyListeners();
    await _persist();
  }

  Future<void> setCompact(bool value) async {
    if (_compact == value) return;
    _compact = value;
    notifyListeners();
    await _persist();
  }

  Future<void> setStartPage(int index) async {
    if (_startPage == index) return;
    _startPage = index;
    notifyListeners();
    await _persist();
  }

  Future<void> setShowSteps(bool value) async {
    if (_showSteps == value) return;
    _showSteps = value;
    notifyListeners();
    await _persist();
  }

  Future<void> setShowChecks(bool value) async {
    if (_showChecks == value) return;
    _showChecks = value;
    notifyListeners();
    await _persist();
  }

  Future<void> setShowGraphs(bool value) async {
    if (_showGraphs == value) return;
    _showGraphs = value;
    notifyListeners();
    await _persist();
  }

  Future<void> setDecimalPlaces(int value) async {
    final next = const [0, 2, 4, 6].contains(value) ? value : 0;
    if (_decimalPlaces == next) return;
    _decimalPlaces = next;
    notifyListeners();
    await _persist();
  }

  Future<void> setRestoreSession(bool value) async {
    if (_restoreSession == value) return;
    _restoreSession = value;
    notifyListeners();
    await _persist();
  }

  Future<void> setHaptics(bool value) async {
    if (_haptics == value) return;
    _haptics = value;
    notifyListeners();
    await _persist();
  }

  Future<void> setShowTheory(bool value) async {
    if (_showTheory == value) return;
    _showTheory = value;
    notifyListeners();
    await _persist();
  }

  Future<void> setCopyLatex(bool value) async {
    if (_copyLatex == value) return;
    _copyLatex = value;
    notifyListeners();
    await _persist();
  }

  Future<void> setDesktopMode(bool value) async {
    if (_desktopMode == value) return;
    _desktopMode = value;
    notifyListeners();
    await _persist();
  }

  /// Records the page the reader is on so the next launch can resume it.
  Future<void> notePage(int index) async {
    if (_lastPage == index) return;
    _lastPage = index;
    await _persist();
  }

  Future<void> resetDefaults() async {
    _resetFields();
    notifyListeners();
    await _persist();
    await _persistence.setLauncherPalette(_palette);
  }

  void _resetFields() {
    _themeMode = ThemeMode.dark;
    _locale = defaultLocale();
    _textScale = 1.0;
    _reduceMotion = false;
    _palette = 'indigo';
    _highContrast = false;
    _compact = false;
    _startPage = 0;
    _showSteps = true;
    _showChecks = true;
    _showGraphs = true;
    _decimalPlaces = 0;
    _restoreSession = false;
    _lastPage = 0;
    _haptics = true;
    _showTheory = true;
    _copyLatex = false;
    _desktopMode = false;
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
        _textScaleKey: _textScale.toStringAsFixed(2),
        _reduceMotionKey: _reduceMotion ? 'true' : 'false',
        _paletteKey: _palette,
        _highContrastKey: _highContrast ? 'true' : 'false',
        _compactKey: _compact ? 'true' : 'false',
        _startPageKey: '$_startPage',
        _showStepsKey: _showSteps ? 'true' : 'false',
        _showChecksKey: _showChecks ? 'true' : 'false',
        _showGraphsKey: _showGraphs ? 'true' : 'false',
        _decimalPlacesKey: '$_decimalPlaces',
        _restoreSessionKey: _restoreSession ? 'true' : 'false',
        _lastPageKey: '$_lastPage',
        _hapticsKey: _haptics ? 'true' : 'false',
        _showTheoryKey: _showTheory ? 'true' : 'false',
        _copyLatexKey: _copyLatex ? 'true' : 'false',
        _desktopModeKey: _desktopMode ? 'true' : 'false',
      });
    } catch (_) {
      // Preferences are best-effort only.
    }
  }
}

/// Makes the live preferences available to every page and result card without
/// threading [AppSettings] through every widget constructor.
class SettingsScope extends InheritedNotifier<AppSettings> {
  const SettingsScope({
    super.key,
    required AppSettings settings,
    required super.child,
  }) : super(notifier: settings);

  static AppSettings? maybeOf(BuildContext context) =>
      context.dependOnInheritedWidgetOfExactType<SettingsScope>()?.notifier;

  static AppSettings of(BuildContext context) {
    final settings = maybeOf(context);
    assert(settings != null, 'SettingsScope is missing above this widget.');
    return settings!;
  }
}
