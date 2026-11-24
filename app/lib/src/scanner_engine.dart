// FFI bridge to the classical math scanner that ships inside libodecore.
//
// The template bank lives in the app bundle (assets/scanner/glyph_templates.bin)
// and is loaded once. Every reading runs in a background isolate, so a large
// photo never blocks the UI thread.
import 'dart:convert';
import 'dart:ffi';
import 'dart:isolate';

import 'package:ffi/ffi.dart';
// dart:ffi and dart:ui both define `Size`; the FFI one is the one we need.
import 'package:flutter/services.dart' hide Size;

import 'native_engine.dart';

typedef _LoadNative = Int32 Function(Pointer<Uint8> data, Size size);
typedef _LoadDart = int Function(Pointer<Uint8> data, int size);
typedef _ReadNative = Pointer<Utf8> Function(Pointer<Uint8> data, Size size);
typedef _ReadDart = Pointer<Utf8> Function(Pointer<Uint8> data, int size);
typedef _FreeNative = Void Function(Pointer<Utf8> text);
typedef _FreeDart = void Function(Pointer<Utf8> text);

/// One classified glyph, kept for the "why did it read that?" panel.
class ScanGlyph {
  const ScanGlyph({
    required this.label,
    required this.score,
    required this.margin,
    required this.box,
  });

  final String label;
  final double score;
  final double margin;
  final List<int> box;

  factory ScanGlyph.fromJson(Map<String, dynamic> json) => ScanGlyph(
    label: json['label'] as String? ?? '?',
    score: (json['score'] as num?)?.toDouble() ?? 0.0,
    margin: (json['margin'] as num?)?.toDouble() ?? 0.0,
    box: [
      for (final value in (json['box'] as List<dynamic>? ?? const []))
        (value as num).toInt(),
    ],
  );
}

/// A scanner reading. [kind] is "expression", "sum", "integral", "limit" or
/// "system"; [parts] carries the named fields the matching workbench expects.
class ScanResult {
  const ScanResult({
    required this.expression,
    required this.kind,
    required this.parts,
    required this.confidence,
    required this.skewDegrees,
    required this.binarization,
    required this.width,
    required this.height,
    required this.symbols,
    required this.lines,
    required this.glyphs,
  });

  final String expression;
  final String kind;
  final Map<String, String> parts;
  final double confidence;
  final double skewDegrees;
  final String binarization;
  final int width;
  final int height;
  final int symbols;
  final List<String> lines;
  final List<ScanGlyph> glyphs;

  factory ScanResult.fromJson(Map<String, dynamic> json) {
    final parts = <String, String>{};
    final rawParts = json['parts'];
    if (rawParts is Map) {
      for (final entry in rawParts.entries) {
        parts['${entry.key}'] = '${entry.value}';
      }
    }
    return ScanResult(
      expression: json['expression'] as String? ?? '',
      kind: json['kind'] as String? ?? 'expression',
      parts: parts,
      confidence: (json['confidence'] as num?)?.toDouble() ?? 0.0,
      skewDegrees: (json['skewDegrees'] as num?)?.toDouble() ?? 0.0,
      binarization: json['binarization'] as String? ?? '',
      width: (json['width'] as num?)?.toInt() ?? 0,
      height: (json['height'] as num?)?.toInt() ?? 0,
      symbols: (json['symbols'] as num?)?.toInt() ?? 0,
      lines: [
        for (final line in (json['lines'] as List<dynamic>? ?? const []))
          '$line',
      ],
      glyphs: [
        for (final glyph in (json['glyphs'] as List<dynamic>? ?? const []))
          if (glyph is Map<String, dynamic>) ScanGlyph.fromJson(glyph),
      ],
    );
  }
}

class ScannerUnavailable implements Exception {
  ScannerUnavailable(this.message);
  final String message;
  @override
  String toString() => message;
}

/// Runs in a background isolate: opens the library, loads the bank and reads
/// one image. Nothing native crosses the isolate boundary.
String _readInIsolate((String, Uint8List, Uint8List) request) {
  final (libraryPath, bank, image) = request;
  final library = DynamicLibrary.open(libraryPath);
  final load = library.lookupFunction<_LoadNative, _LoadDart>(
    'scanner_load_templates',
  );
  final read = library.lookupFunction<_ReadNative, _ReadDart>('scanner_read');
  final free = library.lookupFunction<_FreeNative, _FreeDart>('scanner_free');

  final bankPtr = malloc<Uint8>(bank.length);
  final imagePtr = malloc<Uint8>(image.length);
  Pointer<Utf8> response = nullptr;
  try {
    bankPtr.asTypedList(bank.length).setAll(0, bank);
    imagePtr.asTypedList(image.length).setAll(0, image);
    if (load(bankPtr, bank.length) == 0) {
      return '{"ok":false,"error":"template bank could not be loaded"}';
    }
    response = read(imagePtr, image.length);
    if (response == nullptr) {
      return '{"ok":false,"error":"scanner returned no response"}';
    }
    return response.toDartString();
  } finally {
    if (response != nullptr) free(response);
    malloc.free(bankPtr);
    malloc.free(imagePtr);
  }
}

class ScannerEngine {
  ScannerEngine._(this.libraryPath, this._bank);

  final String libraryPath;
  final Uint8List _bank;

  static ScannerEngine? _instance;
  static Future<ScannerEngine>? _loading;

  /// Loads the template bank once. Throws [ScannerUnavailable] when the native
  /// library or the bank asset is missing.
  static Future<ScannerEngine> load({AssetBundle? bundle}) {
    final existing = _instance;
    if (existing != null) return Future.value(existing);
    final pending = _loading;
    if (pending != null) return pending;

    final future = _loadOnce(bundle).whenComplete(() => _loading = null);
    _loading = future;
    return future;
  }

  static Future<ScannerEngine> _loadOnce(AssetBundle? bundle) async {
    final native = NativeEngine.instance;
    final data = await (bundle ?? rootBundle).load(
      'assets/scanner/glyph_templates.bin',
    );
    final bank = data.buffer.asUint8List(
      data.offsetInBytes,
      data.lengthInBytes,
    );
    final library = DynamicLibrary.open(native.libraryPath);
    final load = library.lookupFunction<_LoadNative, _LoadDart>(
      'scanner_load_templates',
    );
    final bankPtr = malloc<Uint8>(bank.length);
    try {
      bankPtr.asTypedList(bank.length).setAll(0, bank);
      if (load(bankPtr, bank.length) == 0) {
        throw ScannerUnavailable('the scanner template bank is corrupt');
      }
    } finally {
      malloc.free(bankPtr);
    }
    final engine = ScannerEngine._(native.libraryPath, bank);
    _instance = engine;
    return engine;
  }

  /// Reads one encoded image (PNG, JPEG, BMP, WebP, TIFF, GIF).
  Future<ScanResult> read(Uint8List image) async {
    if (image.isEmpty) {
      throw ScannerUnavailable('the selected image is empty');
    }
    final raw = await Isolate.run(
      () => _readInIsolate((libraryPath, _bank, image)),
    );
    final decoded = jsonDecode(raw);
    if (decoded is! Map<String, dynamic>) {
      throw ScannerUnavailable('the scanner returned malformed JSON');
    }
    if (decoded['ok'] != true) {
      throw ScannerUnavailable(
        decoded['error'] as String? ?? 'the scanner could not read the image',
      );
    }
    return ScanResult.fromJson(decoded);
  }
}
