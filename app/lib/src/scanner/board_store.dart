// Autosave for the handwriting board. The ink is a few thousand numbers, so a
// small JSON blob is enough; no database and no image round-trip. Android keeps
// it in SharedPreferences through the settings channel, desktop in a file.
import 'dart:convert';
import 'dart:io';

import 'package:flutter/services.dart';

class BoardStore {
  static const _channel = MethodChannel('ir.infinit/settings');

  Future<Map<String, dynamic>?> load() async {
    try {
      final String? text;
      if (Platform.isAndroid) {
        text = await _channel.invokeMethod<String>('load_board');
      } else {
        final file = _file();
        text = await file.exists() ? await file.readAsString() : null;
      }
      if (text == null || text.isEmpty) return null;
      final decoded = jsonDecode(text);
      return decoded is Map<String, dynamic> ? decoded : null;
    } catch (_) {
      return null;
    }
  }

  Future<void> save(Map<String, dynamic> ink) async {
    try {
      final text = jsonEncode(ink);
      if (Platform.isAndroid) {
        await _channel.invokeMethod<void>('save_board', {'data': text});
        return;
      }
      final file = _file();
      await file.parent.create(recursive: true);
      await file.writeAsString(text, flush: true);
    } catch (_) {
      // Autosave must never interrupt drawing.
    }
  }

  Future<void> clear() async {
    try {
      if (Platform.isAndroid) {
        await _channel.invokeMethod<void>('clear_board');
        return;
      }
      final file = _file();
      if (await file.exists()) await file.delete();
    } catch (_) {}
  }

  File _file() {
    if (Platform.isWindows) {
      final appData = Platform.environment['APPDATA'];
      final base = (appData != null && appData.isNotEmpty) ? appData : '.';
      return File('$base\\infinit\\board_ink.json');
    }
    final xdg = Platform.environment['XDG_CONFIG_HOME'];
    final home = Platform.environment['HOME'];
    final base = (xdg != null && xdg.isNotEmpty)
        ? xdg
        : (home != null && home.isNotEmpty ? '$home/.config' : '.config');
    return File('$base/infinit/board_ink.json');
  }
}
