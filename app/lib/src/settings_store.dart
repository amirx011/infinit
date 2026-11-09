import 'dart:convert';
import 'dart:io';

import 'package:flutter/services.dart';

/// Storage for the user preferences.  Linux uses a small JSON file; Android
/// uses native SharedPreferences through a platform channel (no extra plugin).
abstract class SettingsPersistence {
  Future<Map<String, String>> load();
  Future<void> save(Map<String, String> values);

  /// Android cannot recolor a launcher icon in place, so the native side
  /// switches to the pre-rendered icon for this palette.  Other platforms use
  /// their static icon and inherit this no-op.
  Future<void> setLauncherPalette(String palette) async {}
}

class PlatformSettingsPersistence implements SettingsPersistence {
  static const MethodChannel _channel = MethodChannel('ir.infinit/settings');

  @override
  Future<Map<String, String>> load() async {
    if (Platform.isAndroid) {
      final values = await _channel.invokeMapMethod<String, String>('load');
      return values ?? <String, String>{};
    }
    try {
      final file = _file();
      if (!await file.exists()) return <String, String>{};
      final decoded = jsonDecode(await file.readAsString());
      if (decoded is! Map) return <String, String>{};
      return decoded.map((k, v) => MapEntry(k.toString(), v.toString()));
    } catch (_) {
      return <String, String>{};
    }
  }

  @override
  Future<void> save(Map<String, String> values) async {
    if (Platform.isAndroid) {
      await _channel.invokeMethod<void>('save', values);
      return;
    }
    try {
      final file = _file();
      await file.parent.create(recursive: true);
      await file.writeAsString(jsonEncode(values), flush: true);
    } catch (_) {
      // Preferences are non-critical; the app still works with defaults.
    }
  }

  @override
  Future<void> setLauncherPalette(String palette) async {
    if (!Platform.isAndroid) return;
    try {
      await _channel.invokeMethod<bool>('set_launcher_icon', palette);
    } catch (_) {
      // Some launchers ignore component changes; the UI palette still applies.
    }
  }

  File _file() {
    final xdg = Platform.environment['XDG_CONFIG_HOME'];
    final home = Platform.environment['HOME'];
    final base = (xdg != null && xdg.isNotEmpty)
        ? xdg
        : (home != null && home.isNotEmpty ? '$home/.config' : '.config');
    return File('$base/infinit/settings.json');
  }
}

class MemorySettingsPersistence implements SettingsPersistence {
  MemorySettingsPersistence([Map<String, String>? initial])
    : _values = Map<String, String>.from(initial ?? const {});

  final Map<String, String> _values;

  @override
  Future<Map<String, String>> load() async => Map<String, String>.from(_values);

  @override
  Future<void> save(Map<String, String> values) async {
    _values
      ..clear()
      ..addAll(values);
  }

  @override
  Future<void> setLauncherPalette(String palette) async {}
}
