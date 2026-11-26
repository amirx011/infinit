// Persistence for the user's own handwriting prototypes. The native scanner
// already exports its Tier B set as plain bytes; this class only decides where
// those bytes live. Android uses SharedPreferences through the existing
// settings channel (base64), desktop uses a small file next to settings.json.
import 'dart:convert';
import 'dart:io';

import 'package:flutter/services.dart';

class PrototypeStore {
  static const _channel = MethodChannel('ir.infinit/settings');

  /// The stroke bank lives next to the raster bank. Two files, not one: the
  /// photo path and the handwriting path are separate formats (§12A.5).
  Future<Uint8List?> loadInk() => _load('ink_samples', 'ink_prototypes.bin');

  Future<void> saveInk(Uint8List bytes) =>
      _save('ink_samples', 'ink_prototypes.bin', bytes);

  Future<void> clearInk() => _clear('ink_samples', 'ink_prototypes.bin');

  Future<Uint8List?> load() => _load('load_samples', 'prototypes.bin');

  Future<Uint8List?> _load(String channelKey, String fileName) async {
    if (Platform.isAndroid) {
      try {
        final text = await _channel.invokeMethod<String>(channelKey);
        if (text == null || text.isEmpty) return null;
        return base64Decode(text);
      } catch (_) {
        return null;
      }
    }
    try {
      final file = _file(fileName);
      if (!await file.exists()) return null;
      return await file.readAsBytes();
    } catch (_) {
      return null;
    }
  }

  Future<void> save(Uint8List bytes) =>
      _save('save_samples', 'prototypes.bin', bytes);

  Future<void> _save(String channelKey, String fileName, Uint8List bytes) async {
    if (bytes.isEmpty) return;
    if (Platform.isAndroid) {
      try {
        await _channel.invokeMethod<void>(channelKey, {
          'data': base64Encode(bytes),
        });
      } catch (_) {
        // Losing personal samples is better than blocking the scan.
      }
      return;
    }
    try {
      final file = _file(fileName);
      await file.parent.create(recursive: true);
      await file.writeAsBytes(bytes, flush: true);
    } catch (_) {
      // Same: persistence is a convenience.
    }
  }

  Future<void> clear() => _clear('clear_samples', 'prototypes.bin');

  Future<void> _clear(String channelKey, String fileName) async {
    if (Platform.isAndroid) {
      try {
        await _channel.invokeMethod<void>(channelKey);
      } catch (_) {}
      return;
    }
    try {
      final file = _file(fileName);
      if (await file.exists()) await file.delete();
    } catch (_) {}
  }

  File _file(String fileName) {
    if (Platform.isWindows) {
      final appData = Platform.environment['APPDATA'];
      final base = (appData != null && appData.isNotEmpty) ? appData : '.';
      return File('$base\\infinit\\$fileName');
    }
    final xdg = Platform.environment['XDG_CONFIG_HOME'];
    final home = Platform.environment['HOME'];
    final base = (xdg != null && xdg.isNotEmpty)
        ? xdg
        : (home != null && home.isNotEmpty ? '$home/.config' : '.config');
    return File('$base/infinit/$fileName');
  }
}
