// FFI bridge to the classical math scanner that ships inside libodecore.
//
// The template bank lives in the app bundle (assets/scanner/glyph_templates.bin)
// and is loaded once. Every reading runs in a background isolate, so a large
// photo never blocks the UI thread. The isolate also uses the core parser as an
// oracle: a reading that does not parse is retried with the classifier's second
// choice at the few uncertain glyphs, and the first valid reading wins.
import 'dart:convert';
import 'dart:ffi';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
// dart:ffi and dart:ui both define `Size`; the FFI one is the one we need.
import 'package:flutter/services.dart' hide Size;

import 'native_engine.dart';
import 'scanner/prototype_store.dart';

typedef _LoadNative = Int32 Function(Pointer<Uint8> data, Size size);
typedef _LoadDart = int Function(Pointer<Uint8> data, int size);
typedef _ReadNative = Pointer<Utf8> Function(Pointer<Uint8> data, Size size);
typedef _ReadDart = Pointer<Utf8> Function(Pointer<Uint8> data, int size);
typedef _ReadOverrideNative = Pointer<Utf8> Function(
  Pointer<Uint8> data,
  Size size,
  Pointer<Utf8> json,
);
typedef _ReadOverrideDart = Pointer<Utf8> Function(
  Pointer<Uint8> data,
  int size,
  Pointer<Utf8> json,
);
typedef _SessionOpenNative = Uint64 Function(Pointer<Uint8> data, Size size);
typedef _SessionOpenDart = int Function(Pointer<Uint8> data, int size);
typedef _SessionReadNative = Pointer<Utf8> Function(Uint64 session);
typedef _SessionReadDart = Pointer<Utf8> Function(int session);
typedef _SessionRelabelNative = Pointer<Utf8> Function(
  Uint64 session,
  Pointer<Utf8> json,
);
typedef _SessionRelabelDart = Pointer<Utf8> Function(
  int session,
  Pointer<Utf8> json,
);
typedef _SessionMergeNative = Pointer<Utf8> Function(
  Uint64 session,
  Int32 first,
  Int32 second,
);
typedef _SessionMergeDart = Pointer<Utf8> Function(
  int session,
  int first,
  int second,
);
typedef _SessionSplitNative = Pointer<Utf8> Function(Uint64 session, Int32 symbol);
typedef _SessionSplitDart = Pointer<Utf8> Function(int session, int symbol);
typedef _SessionRelationNative = Pointer<Utf8> Function(
  Uint64 session,
  Int32 symbol,
  Pointer<Utf8> kind,
  Pointer<Utf8> value,
);
typedef _SessionRelationDart = Pointer<Utf8> Function(
  int session,
  int symbol,
  Pointer<Utf8> kind,
  Pointer<Utf8> value,
);
typedef _SolveNative = Pointer<Utf8> Function(Pointer<Utf8> request);
typedef _SolveDart = Pointer<Utf8> Function(Pointer<Utf8> request);
typedef _FreeNative = Void Function(Pointer<Utf8> text);
typedef _FreeDart = void Function(Pointer<Utf8> text);
typedef _SetValidatorNative =
    Void Function(Pointer<NativeFunction<_ValidateNative>> callback);
typedef _SetValidatorDart =
    void Function(Pointer<NativeFunction<_ValidateNative>> callback);
typedef _ValidateNative =
    Int32 Function(Pointer<Utf8> expression, Pointer<Pointer<Utf8>> error);
typedef _CancelNative = Void Function();
typedef _CancelDart = void Function();
typedef _StageNative = Int32 Function();
typedef _StageDart = int Function();
typedef _InkProtoAddNative = Int32 Function(
  Pointer<Utf8> label,
  Pointer<Uint8> data,
  Size size,
);
typedef _InkProtoAddDart = int Function(
  Pointer<Utf8> label,
  Pointer<Uint8> data,
  int size,
);
typedef _InkProtoLoadNative = Int32 Function(Pointer<Uint8> data, Size size);
typedef _InkProtoLoadDart = int Function(Pointer<Uint8> data, int size);
typedef _DebugStageNative = Pointer<Uint8> Function(
  Pointer<Uint8> data,
  Size size,
  Pointer<Utf8> stage,
  Pointer<Size> outLength,
);
typedef _DebugStageDart = Pointer<Uint8> Function(
  Pointer<Uint8> data,
  int size,
  Pointer<Utf8> stage,
  Pointer<Size> outLength,
);
typedef _PrototypeLoadNative = Int32 Function(Pointer<Uint8> data, Size size);
typedef _PrototypeLoadDart = int Function(Pointer<Uint8> data, int size);
typedef _PrototypeAddNative = Int32 Function(
  Pointer<Utf8> label,
  Pointer<Uint8> png,
  Size size,
);
typedef _PrototypeAddDart = int Function(
  Pointer<Utf8> label,
  Pointer<Uint8> png,
  int size,
);
typedef _PrototypeCountNative = Size Function();
typedef _PrototypeCountDart = int Function();
typedef _HandwritingNative = Int32 Function(Int32 enabled);
typedef _HandwritingDart = int Function(int enabled);
typedef _ExportNative = Pointer<Uint8> Function(Pointer<Size> outSize);
typedef _ExportDart = Pointer<Uint8> Function(Pointer<Size> outSize);
typedef _FreeBytesNative = Void Function(Pointer<Uint8> data);
typedef _FreeBytesDart = void Function(Pointer<Uint8> data);

typedef _InkReadNative = Pointer<Utf8> Function(Pointer<Uint8> data, Size size);
typedef _InkReadDart = Pointer<Utf8> Function(Pointer<Uint8> data, int size);
typedef _InkOpenNative = Uint64 Function(Pointer<Uint8> data, Size size);
typedef _InkOpenDart = int Function(Pointer<Uint8> data, int size);
typedef _BankLoadedNative = Int32 Function();
typedef _BankLoadedDart = int Function();

/// The Core parser, exposed to the native scanner as a callback. Results are
/// cached because the repair search asks about the same text several times.
DynamicLibrary? _oracleLibrary;
final Map<String, bool> _oracleCache = {};

int _validateWithCore(Pointer<Utf8> expression, Pointer<Pointer<Utf8>> error) {
  final library = _oracleLibrary;
  if (library == null) return 0;
  final text = expression.toDartString();
  final cached = _oracleCache[text];
  if (cached != null) return cached ? 1 : 0;

  final solve = library.lookupFunction<_SolveNative, _SolveDart>('ode_solve');
  final free = library.lookupFunction<_FreeNative, _FreeDart>('ode_free');
  final request = jsonEncode({
    'mode': 'parse',
    'input': text,
    'x': 'x',
    'lang': 'en',
  });
  final requestPtr = request.toNativeUtf8();
  Pointer<Utf8> response = nullptr;
  var valid = false;
  try {
    response = solve(requestPtr);
    if (response != nullptr) {
      final decoded = jsonDecode(response.toDartString());
      valid = decoded is Map && decoded['ok'] == true;
    }
  } catch (_) {
    valid = false;
  } finally {
    if (response != nullptr) free(response);
    malloc.free(requestPtr);
  }
  if (_oracleCache.length > 256) _oracleCache.clear();
  _oracleCache[text] = valid;
  return valid ? 1 : 0;
}

/// One candidate label for a classified glyph.
class ScanAlternative {
  const ScanAlternative(this.glyph, this.score);

  final String glyph;
  final double score;

  factory ScanAlternative.fromJson(Map<String, dynamic> json) =>
      ScanAlternative(
        json['glyph'] as String? ?? '?',
        (json['score'] as num?)?.toDouble() ?? 0.0,
      );
}

/// One classified glyph, kept for the "why did it read that?" panel.
class ScanGlyph {
  const ScanGlyph({
    required this.id,
    required this.label,
    required this.score,
    required this.margin,
    required this.box,
    required this.alternatives,
  });

  /// Symbol id from the segmentation pass; corrections are addressed by it.
  final int id;
  final String label;
  final double score;
  final double margin;
  final List<int> box;
  final List<ScanAlternative> alternatives;

  factory ScanGlyph.fromJson(Map<String, dynamic> json) => ScanGlyph(
    id: (json['id'] as num?)?.toInt() ?? -1,
    label: json['label'] as String? ?? json['glyph'] as String? ?? '?',
    score: (json['score'] as num?)?.toDouble() ?? 0.0,
    margin: (json['margin'] as num?)?.toDouble() ?? 0.0,
    box: [
      for (final value
          in (json['box'] ?? json['bbox'] as List<dynamic>? ?? const []))
        (value as num).toInt(),
    ],
    alternatives: [
      for (final entry in (json['alternatives'] as List<dynamic>? ?? const []))
        if (entry is Map<String, dynamic>) ScanAlternative.fromJson(entry),
    ],
  );
}

/// One repair alternative: a complete expression the search considered, with
/// its cost. Lower is better; `valid` means the Core parser accepted it.
class ScanReading {
  const ScanReading({
    required this.expression,
    required this.latex,
    required this.cost,
    required this.valid,
  });

  final String expression;
  final String latex;
  final double cost;
  final bool valid;

  factory ScanReading.fromJson(Map<String, dynamic> json) => ScanReading(
    expression: json['infinit'] as String? ?? json['expression'] as String? ?? '',
    latex: json['latex'] as String? ?? '',
    cost: (json['cost'] as num?)?.toDouble() ?? 0.0,
    valid: json['valid'] == true,
  );
}

/// One structured line from the output contract, used by the per-line review
/// cards. `glyphs` are the line's own symbols, renumbered for that card.
class ScanLineReading {
  const ScanLineReading({
    required this.expression,
    required this.latex,
    required this.parseValid,
    required this.confidence,
    required this.warnings,
    this.glyphs = const [],
  });

  final String expression;
  final String latex;
  final bool? parseValid;
  final double confidence;
  final List<String> warnings;
  final List<ScanGlyph> glyphs;

  factory ScanLineReading.fromJson(Map<String, dynamic> json) =>
      ScanLineReading(
        expression: json['infinit'] as String? ?? '',
        latex: json['latex'] as String? ?? '',
        parseValid: json['parse_valid'] as bool?,
        confidence: (json['confidence'] as num?)?.toDouble() ?? 0.0,
        warnings: [
          for (final warning in (json['warnings'] as List<dynamic>? ?? const []))
            '$warning',
        ],
        glyphs: [
          for (final glyph
              in (json['symbols'] as List<dynamic>? ?? const []))
            if (glyph is Map<String, dynamic>) ScanGlyph.fromJson(glyph),
        ],
      );
}

/// An ambiguous structural decision the review screen can flip: a raised glyph
/// read as a superscript or inline text, a short bar read as a fraction bar or
/// a minus. `value` is "auto" until the user chooses.
class ScanRelation {
  const ScanRelation({
    required this.symbol,
    required this.kind,
    required this.value,
  });

  final int symbol;
  final String kind;
  final String value;

  factory ScanRelation.fromJson(Map<String, dynamic> json) => ScanRelation(
    symbol: (json['symbol'] as num?)?.toInt() ?? -1,
    kind: json['kind'] as String? ?? '',
    value: json['value'] as String? ?? 'auto',
  );
}

/// A scanner reading. [kind] is "expression", "sum", "integral", "limit",
/// "system" or "matrix"; [parts] carries the named fields the matching
/// workbench expects.
class ScanResult {
  const ScanResult({
    required this.expression,
    required this.latex,
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
    required this.repaired,
    required this.session,
    required this.overrides,
    this.status = 'ok',
    this.parseValid,
    this.timing = const {},
    this.warnings = const [],
    this.alternatives = const [],
    this.lineDetails = const [],
    this.relations = const [],
  });

  final String expression;
  final String latex;
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

  /// True when the parser-guided repair replaced the first reading.
  final bool repaired;

  /// "ok", "partial", "failed" or "cancelled". `partial` means the reader
  /// produced text but the Core parser rejected it.
  final String status;

  /// Null when no parser was injected, so the app does not claim a verdict.
  final bool? parseValid;

  /// Per-stage milliseconds, keyed preprocess/segment/classify/structure/repair.
  final Map<String, double> timing;

  final List<String> warnings;
  final List<ScanReading> alternatives;
  final List<ScanLineReading> lineDetails;
  final List<ScanRelation> relations;

  /// Native handle for this image. Corrections send a new label map to it and
  /// reuse the binarised pixels and segmentation, so fixing a symbol never
  /// repeats the vision stages. Zero means the session was unavailable.
  final int session;

  /// Labels the repair already applied. The correction sheet starts from this
  /// map, so a later manual fix does not undo an earlier automatic one.
  final Map<int, String> overrides;

  factory ScanResult.fromJson(
    Map<String, dynamic> json, {
    bool repaired = false,
    int session = 0,
  }) {
    final parts = <String, String>{};
    final rawParts = json['parts'];
    if (rawParts is Map) {
      for (final entry in rawParts.entries) {
        parts['${entry.key}'] = '${entry.value}';
      }
    }
    final overrides = <int, String>{};
    final rawOverrides = json['overrides'];
    if (rawOverrides is Map) {
      for (final entry in rawOverrides.entries) {
        final id = int.tryParse('${entry.key}');
        if (id != null) overrides[id] = '${entry.value}';
      }
    }
    final timing = <String, double>{};
    final rawTiming = json['timing_ms'];
    if (rawTiming is Map) {
      for (final entry in rawTiming.entries) {
        final value = entry.value;
        if (value is num) timing['${entry.key}'] = value.toDouble();
      }
    }
    return ScanResult(
      expression: json['expression'] as String? ?? '',
      latex: json['latex'] as String? ?? '',
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
      repaired: repaired,
      session: session,
      overrides: overrides,
      status: json['status'] as String? ?? 'ok',
      parseValid: json['parse_valid'] as bool?,
      timing: timing,
      warnings: [
        for (final warning in (json['warnings'] as List<dynamic>? ?? const []))
          '$warning',
      ],
      alternatives: [
        for (final entry in (json['alternatives'] as List<dynamic>? ?? const []))
          if (entry is Map<String, dynamic>) ScanReading.fromJson(entry),
      ],
      lineDetails: [
        for (final entry in (json['lineDetails'] as List<dynamic>? ?? const []))
          if (entry is Map<String, dynamic>) ScanLineReading.fromJson(entry),
      ],
      relations: [
        for (final entry in (json['relations'] as List<dynamic>? ?? const []))
          if (entry is Map<String, dynamic>) ScanRelation.fromJson(entry),
      ],
    );
  }

  /// Builds the matrix grid string the core's matrix fields take: rows are
  /// separated by ';' and cells by ','.
  String get matrix => parts['matrix'] ?? '';

  int get rows => int.tryParse(parts['rows'] ?? '') ?? 0;

  int get columns => int.tryParse(parts['columns'] ?? '') ?? 0;
}

class ScannerUnavailable implements Exception {
  ScannerUnavailable(this.message);
  final String message;
  @override
  String toString() => message;
}

/// Runs in a background isolate: opens the library, loads the bank, injects
/// the Core parser as the native repair oracle, opens one scan session and
/// returns the reading JSON together with the session id.
///
/// The repair itself runs in C++: a bounded beam search over the classifier's
/// alternatives, scored by symbol distance, parse result and unknown glyphs.
String _handwritingInIsolate((String, Uint8List) request) {
  final (libraryPath, image) = request;
  final library = DynamicLibrary.open(libraryPath);
  final read = library.lookupFunction<_InkReadNative, _InkReadDart>(
    'scanner_read_handwriting',
  );
  final free = library.lookupFunction<_FreeNative, _FreeDart>('scanner_free');
  final dataPtr = malloc<Uint8>(image.length);
  try {
    dataPtr.asTypedList(image.length).setAll(0, image);
    final out = read(dataPtr, image.length);
    if (out == nullptr) return '{"status":"failed","error":"handwriting read failed"}';
    try {
      return out.toDartString();
    } finally {
      free(out);
    }
  } finally {
    malloc.free(dataPtr);
  }
}

Uint8List? _stageInIsolate((String, Uint8List, String) request) {
  final (libraryPath, image, stage) = request;
  final library = DynamicLibrary.open(libraryPath);
  final encode = library.lookupFunction<_DebugStageNative, _DebugStageDart>(
    'scanner_debug_stage',
  );
  final freeBytes = library.lookupFunction<_FreeBytesNative, _FreeBytesDart>(
    'scanner_free_bytes',
  );
  final imagePtr = malloc<Uint8>(image.length);
  final stagePtr = stage.toNativeUtf8();
  final lengthPtr = malloc<Size>();
  try {
    imagePtr.asTypedList(image.length).setAll(0, image);
    final out = encode(imagePtr, image.length, stagePtr, lengthPtr);
    if (out == nullptr || lengthPtr.value == 0) return null;
    try {
      return Uint8List.fromList(out.asTypedList(lengthPtr.value));
    } finally {
      freeBytes(out);
    }
  } finally {
    malloc.free(imagePtr);
    malloc.free(stagePtr);
    malloc.free(lengthPtr);
  }
}

(String, int) _readInIsolate(
  (String, Uint8List, Uint8List, Map<int, String>?) request,
) {
  final (libraryPath, bank, image, overrides) = request;
  final library = DynamicLibrary.open(libraryPath);
  final load = library.lookupFunction<_LoadNative, _LoadDart>(
    'scanner_load_templates',
  );
  final read = library.lookupFunction<_ReadNative, _ReadDart>('scanner_read');
  final readOverride = library
      .lookupFunction<_ReadOverrideNative, _ReadOverrideDart>(
        'scanner_read_with_overrides',
      );
  final open = library.lookupFunction<_SessionOpenNative, _SessionOpenDart>(
    'scanner_open_session',
  );
  final sessionRead = library
      .lookupFunction<_SessionReadNative, _SessionReadDart>(
        'scanner_session_read',
      );
  final sessionRelabel = library
      .lookupFunction<_SessionRelabelNative, _SessionRelabelDart>(
        'scanner_session_relabel',
      );
  final setValidator = library
      .lookupFunction<_SetValidatorNative, _SetValidatorDart>(
        'scanner_set_validator',
      );
  final free = library.lookupFunction<_FreeNative, _FreeDart>('scanner_free');

  _oracleLibrary = library;
  _oracleCache.clear();
  final callable = NativeCallable<_ValidateNative>.isolateLocal(
    _validateWithCore,
    exceptionalReturn: 0,
  );
  setValidator(callable.nativeFunction);

  final bankPtr = malloc<Uint8>(bank.length);
  final imagePtr = malloc<Uint8>(image.length);
  try {
    bankPtr.asTypedList(bank.length).setAll(0, bank);
    imagePtr.asTypedList(image.length).setAll(0, image);
    final bankLoaded = library
        .lookupFunction<_BankLoadedNative, _BankLoadedDart>(
          'scanner_bank_loaded',
        );
    if (bankLoaded() == 0 && load(bankPtr, bank.length) == 0) {
      return ('{"ok":false,"error":"template bank could not be loaded"}', 0);
    }

    String stateless(Map<int, String>? labels) {
      if (labels == null || labels.isEmpty) {
        final ptr = read(imagePtr, image.length);
        if (ptr == nullptr) return '';
        try {
          return ptr.toDartString();
        } finally {
          free(ptr);
        }
      }
      final jsonPtr = jsonEncode({
        for (final entry in labels.entries) '${entry.key}': entry.value,
      }).toNativeUtf8();
      final ptr = readOverride(imagePtr, image.length, jsonPtr);
      malloc.free(jsonPtr);
      if (ptr == nullptr) return '';
      try {
        return ptr.toDartString();
      } finally {
        free(ptr);
      }
    }

    String relabel(int session, Map<int, String> labels) {
      final jsonPtr = jsonEncode({
        for (final entry in labels.entries) '${entry.key}': entry.value,
      }).toNativeUtf8();
      final ptr = sessionRelabel(session, jsonPtr);
      malloc.free(jsonPtr);
      if (ptr == nullptr) return '';
      try {
        return ptr.toDartString();
      } finally {
        free(ptr);
      }
    }

    final session = open(imagePtr, image.length);
    if (session == 0) {
      // Keep the detailed error from the stateless entry point.
      return (stateless(overrides), 0);
    }

    final first = overrides != null && overrides.isNotEmpty
        ? relabel(session, overrides)
        : _readOnce(sessionRead, free, session);
    if (first.isEmpty) {
      return ('{"ok":false,"error":"scanner returned no response"}', session);
    }
    if (overrides != null && overrides.isNotEmpty) {
      // Session relabel reports only the automatic repairs the search made.
      // Keep the user's own corrections in the returned map as well.
      try {
        final decoded = jsonDecode(first);
        if (decoded is Map<String, dynamic> && decoded['ok'] == true) {
          final merged = <String, String>{
            for (final entry in overrides.entries) '${entry.key}': entry.value,
          };
          final automatic = decoded['overrides'];
          if (automatic is Map) {
            for (final entry in automatic.entries) {
              merged.putIfAbsent('${entry.key}', () => '${entry.value}');
            }
          }
          decoded['overrides'] = merged;
          return (jsonEncode(decoded), session);
        }
      } catch (_) {
        // The raw text is still a usable reading.
      }
    }
    return (first, session);
  } finally {
    setValidator(nullptr);
    callable.close();
    malloc.free(bankPtr);
    malloc.free(imagePtr);
  }
}

String _readOnce(
  _SessionReadDart read,
  _FreeDart free,
  int session,
) {
  final ptr = read(session);
  if (ptr == nullptr) return '';
  try {
    return ptr.toDartString();
  } finally {
    free(ptr);
  }
}

/// Packs strokes for the native ink front end: u32 width, height, stroke count,
/// then per stroke u32 point count and float32 x/y pairs.
Uint8List _encodeInk(List<List<Offset>> strokes, int width, int height) {
  final clean = [
    for (final stroke in strokes)
      if (stroke.isNotEmpty) stroke,
  ];
  var points = 0;
  for (final stroke in clean) {
    points += stroke.length;
  }
  final data = ByteData(12 + 4 * clean.length + 8 * points);
  var offset = 0;
  void putU32(int value) {
    data.setUint32(offset, value, Endian.little);
    offset += 4;
  }

  void putF32(double value) {
    data.setFloat32(offset, value, Endian.little);
    offset += 4;
  }

  putU32(width);
  putU32(height);
  putU32(clean.length);
  for (final stroke in clean) {
    putU32(stroke.length);
    for (final point in stroke) {
      putF32(point.dx.clamp(0, width).toDouble());
      putF32(point.dy.clamp(0, height).toDouble());
    }
  }
  return data.buffer.asUint8List();
}

/// Runs in a background isolate: the ink front end groups strokes into symbols
/// in C++ and the shared classifier, structure and repair stages take over.
(String, int) _readInkInIsolate((String, Uint8List, Uint8List, Map<int, String>?) request) {
  final (libraryPath, bank, ink, overrides) = request;
  final library = DynamicLibrary.open(libraryPath);
  final bankLoaded = library.lookupFunction<_BankLoadedNative, _BankLoadedDart>(
    'scanner_bank_loaded',
  );
  final load = library.lookupFunction<_LoadNative, _LoadDart>(
    'scanner_load_templates',
  );
  final open = library.lookupFunction<_InkOpenNative, _InkOpenDart>(
    'scanner_open_ink_session',
  );
  final readInk = library.lookupFunction<_InkReadNative, _InkReadDart>(
    'scanner_read_ink',
  );
  final sessionRead = library
      .lookupFunction<_SessionReadNative, _SessionReadDart>(
        'scanner_session_read',
      );
  final sessionRelabel = library
      .lookupFunction<_SessionRelabelNative, _SessionRelabelDart>(
        'scanner_session_relabel',
      );
  final setValidator = library
      .lookupFunction<_SetValidatorNative, _SetValidatorDart>(
        'scanner_set_validator',
      );
  final free = library.lookupFunction<_FreeNative, _FreeDart>('scanner_free');

  _oracleLibrary = library;
  _oracleCache.clear();
  final callable = NativeCallable<_ValidateNative>.isolateLocal(
    _validateWithCore,
    exceptionalReturn: 0,
  );
  setValidator(callable.nativeFunction);

  final bankPtr = malloc<Uint8>(bank.length);
  final inkPtr = malloc<Uint8>(ink.length);
  try {
    bankPtr.asTypedList(bank.length).setAll(0, bank);
    inkPtr.asTypedList(ink.length).setAll(0, ink);
    if (bankLoaded() == 0 && load(bankPtr, bank.length) == 0) {
      return ('{"ok":false,"error":"template bank could not be loaded"}', 0);
    }

    String copy(Pointer<Utf8> ptr) {
      if (ptr == nullptr) return '';
      try {
        return ptr.toDartString();
      } finally {
        free(ptr);
      }
    }

    final session = open(inkPtr, ink.length);
    if (session == 0) {
      return (copy(readInk(inkPtr, ink.length)), 0);
    }
    if (overrides == null || overrides.isEmpty) {
      return (copy(sessionRead(session)), session);
    }
    final jsonPtr = jsonEncode({
      for (final entry in overrides.entries) '${entry.key}': entry.value,
    }).toNativeUtf8();
    try {
      return (copy(sessionRelabel(session, jsonPtr)), session);
    } finally {
      malloc.free(jsonPtr);
    }
  } finally {
    setValidator(nullptr);
    callable.close();
    malloc.free(bankPtr);
    malloc.free(inkPtr);
  }
}

/// Runs in a background isolate: applies a correction map to an open session.
/// Only the reading is rebuilt; the image is already binarised and segmented.
/// The Core parser is injected again so the native repair can still revisit
/// the glyphs the user did not touch.
String _relabelInIsolate((String, int, Map<int, String>) request) {
  final (libraryPath, session, overrides) = request;
  final library = DynamicLibrary.open(libraryPath);
  final relabel = library
      .lookupFunction<_SessionRelabelNative, _SessionRelabelDart>(
        'scanner_session_relabel',
      );
  final setValidator = library
      .lookupFunction<_SetValidatorNative, _SetValidatorDart>(
        'scanner_set_validator',
      );
  final free = library.lookupFunction<_FreeNative, _FreeDart>('scanner_free');

  _oracleLibrary = library;
  _oracleCache.clear();
  final callable = NativeCallable<_ValidateNative>.isolateLocal(
    _validateWithCore,
    exceptionalReturn: 0,
  );
  setValidator(callable.nativeFunction);
  final jsonPtr = jsonEncode({
    for (final entry in overrides.entries) '${entry.key}': entry.value,
  }).toNativeUtf8();
  try {
    final ptr = relabel(session, jsonPtr);
    if (ptr == nullptr) return '';
    try {
      return ptr.toDartString();
    } finally {
      free(ptr);
    }
  } finally {
    setValidator(nullptr);
    callable.close();
    malloc.free(jsonPtr);
  }
}

/// Runs in a background isolate: a manual merge or split on an open session.
/// The segmentation is rebuilt from the stored ink, then classification,
/// structure and repair run again. [op] is "merge" or "split".
String _editSessionInIsolate((String, String, int, int, int) request) {
  final (libraryPath, op, session, first, second) = request;
  final library = DynamicLibrary.open(libraryPath);
  final merge = library.lookupFunction<_SessionMergeNative, _SessionMergeDart>(
    'scanner_session_merge',
  );
  final split = library.lookupFunction<_SessionSplitNative, _SessionSplitDart>(
    'scanner_session_split',
  );
  final setValidator = library
      .lookupFunction<_SetValidatorNative, _SetValidatorDart>(
        'scanner_set_validator',
      );
  final free = library.lookupFunction<_FreeNative, _FreeDart>('scanner_free');

  _oracleLibrary = library;
  _oracleCache.clear();
  final callable = NativeCallable<_ValidateNative>.isolateLocal(
    _validateWithCore,
    exceptionalReturn: 0,
  );
  setValidator(callable.nativeFunction);
  try {
    final ptr = op == 'merge' ? merge(session, first, second) : split(session, first);
    if (ptr == nullptr) return '';
    try {
      return ptr.toDartString();
    } finally {
      free(ptr);
    }
  } finally {
    setValidator(nullptr);
    callable.close();
  }
}

/// Runs in a background isolate: forces one reading of an ambiguous relation.
/// The segmentation and glyph labels stay untouched; only structure, emit and
/// repair run again.
String _relationInIsolate((String, int, int, String, String) request) {
  final (libraryPath, session, symbol, kind, value) = request;
  final library = DynamicLibrary.open(libraryPath);
  final relation = library
      .lookupFunction<_SessionRelationNative, _SessionRelationDart>(
        'scanner_session_relation',
      );
  final setValidator = library
      .lookupFunction<_SetValidatorNative, _SetValidatorDart>(
        'scanner_set_validator',
      );
  final free = library.lookupFunction<_FreeNative, _FreeDart>('scanner_free');

  _oracleLibrary = library;
  _oracleCache.clear();
  final callable = NativeCallable<_ValidateNative>.isolateLocal(
    _validateWithCore,
    exceptionalReturn: 0,
  );
  setValidator(callable.nativeFunction);
  final kindPtr = kind.toNativeUtf8();
  final valuePtr = value.toNativeUtf8();
  try {
    final ptr = relation(session, symbol, kindPtr, valuePtr);
    if (ptr == nullptr) return '';
    try {
      return ptr.toDartString();
    } finally {
      free(ptr);
    }
  } finally {
    setValidator(nullptr);
    callable.close();
    malloc.free(kindPtr);
    malloc.free(valuePtr);
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
    await _restoreSamples(engine);
    return engine;
  }

  static bool _samplesRestored = false;

  /// Loads the personal prototype set once per process. The bytes are the
  /// native export format, so this is a plain file read, not a model load.
  static Future<void> _restoreSamples(ScannerEngine engine) async {
    if (_samplesRestored) return;
    _samplesRestored = true;
    try {
      final store = PrototypeStore();
      final bytes = await store.load();
      if (bytes != null && bytes.isNotEmpty) engine.loadPrototypes(bytes);
      final ink = await store.loadInk();
      if (ink != null && ink.isNotEmpty) engine.loadInkPrototypes(ink);
    } catch (_) {
      // A missing or unreadable file simply means "no personal samples yet".
    }
  }

  /// Reads one encoded image (PNG, JPEG, BMP, WebP, TIFF, GIF).
  ///
  /// [overrides] is a fallback for platforms where a scan session cannot be
  /// opened. Normal corrections go through [relabel], which reuses the
  /// session's pixels and segmentation.
  Future<ScanResult> read(
    Uint8List image, {
    Map<int, String>? overrides,
  }) async {
    if (image.isEmpty) {
      throw ScannerUnavailable('the selected image is empty');
    }
    final (raw, session) = await Isolate.run(
      () => _readInIsolate((libraryPath, _bank, image, overrides)),
    );
    return _decode(raw, session);
  }

  /// Reads stroke input from the handwriting board. Grouping happens in C++,
  /// then the shared classifier, structure and repair stages run exactly as
  /// they do for a photo. [width] and [height] are the board's logical size.
  Future<ScanResult> readInk(
    List<List<Offset>> strokes, {
    required int width,
    required int height,
    Map<int, String>? overrides,
  }) async {
    final hasInk = strokes.any((stroke) => stroke.isNotEmpty);
    if (!hasInk || width <= 0 || height <= 0) {
      throw ScannerUnavailable('the writing area is empty');
    }
    final ink = _encodeInk(strokes, width, height);
    final (raw, session) = await Isolate.run(
      () => _readInkInIsolate((libraryPath, _bank, ink, overrides)),
    );
    return _decode(raw, session);
  }

  /// Applies a complete correction map to an open session. The native side
  /// keeps the binarised image and the segmentation, so this rebuilds the
  /// reading only. Send every fix together: one call, one rebuild.
  Future<ScanResult> relabel(int session, Map<int, String> overrides) async {
    if (session == 0) {
      throw ScannerUnavailable('the scan session has expired');
    }
    final raw = await Isolate.run(
      () => _relabelInIsolate((libraryPath, session, overrides)),
    );
    return _decode(raw, session);
  }

  /// Joins two symbols of an open session into one. Symbol ids come from the
  /// last [ScanResult.glyphs]. The reading is rebuilt in C++.
  Future<ScanResult> mergeSymbols(
    int session,
    int first,
    int second,
  ) async {
    if (session == 0) {
      throw ScannerUnavailable('the scan session has expired');
    }
    final raw = await Isolate.run(
      () =>
          _editSessionInIsolate((libraryPath, 'merge', session, first, second)),
    );
    return _decode(raw, session);
  }

  /// Splits a symbol of an open session into one symbol per stroke or
  /// component, for glyphs the segmenter glued together by mistake.
  Future<ScanResult> splitSymbol(int session, int symbol) async {
    if (session == 0) {
      throw ScannerUnavailable('the scan session has expired');
    }
    final raw = await Isolate.run(
      () => _editSessionInIsolate((libraryPath, 'split', session, symbol, 0)),
    );
    return _decode(raw, session);
  }

  /// Forces one reading of an ambiguous pair, then rebuilds structure, emit
  /// and repair in C++. [kind] is "script" or "bar"; [value] is "auto",
  /// "superscript"/"inline", or "fraction"/"minus".
  Future<ScanResult> setRelation(
    int session,
    int symbol,
    String kind,
    String value,
  ) async {
    if (session == 0) {
      throw ScannerUnavailable('the scan session has expired');
    }
    final raw = await Isolate.run(
      () => _relationInIsolate((libraryPath, session, symbol, kind, value)),
    );
    return _decode(raw, session);
  }

  /// Cancels the recognition that is currently running. The native flag is
  /// atomic and checked between stages, so this returns immediately and the
  /// in-flight read resolves with status "cancelled". The next read clears it.
  void cancel() {
    final library = DynamicLibrary.open(libraryPath);
    final cancel = library.lookupFunction<_CancelNative, _CancelDart>(
      'scanner_cancel',
    );
    cancel();
  }

  /// Stores one corrected ink symbol as a **stroke** prototype (§12A.5).
  ///
  /// The board knows the strokes, so the sample keeps stroke count, order and
  /// shape instead of a single 16x16 raster. Returns false when the label is
  /// empty or the strokes cannot be encoded.
  bool addInkPrototype(
    String label,
    List<List<Offset>> strokes, {
    required int width,
    required int height,
  }) {
    if (label.isEmpty || strokes.isEmpty || width <= 0 || height <= 0) {
      return false;
    }
    final ink = _encodeInk(strokes, width, height);
    final library = DynamicLibrary.open(libraryPath);
    final add = library
        .lookupFunction<_InkProtoAddNative, _InkProtoAddDart>(
          'scanner_add_ink_prototype',
        );
    final labelPtr = label.toNativeUtf8();
    final dataPtr = malloc<Uint8>(ink.length);
    try {
      dataPtr.asTypedList(ink.length).setAll(0, ink);
      return add(labelPtr, dataPtr, ink.length) == 1;
    } finally {
      malloc.free(labelPtr);
      malloc.free(dataPtr);
    }
  }

  /// Number of personal stroke prototypes currently loaded.
  int get inkPrototypeCount {
    final library = DynamicLibrary.open(libraryPath);
    final count = library.lookupFunction<_PrototypeCountNative, _PrototypeCountDart>(
      'scanner_ink_prototype_count',
    );
    return count();
  }

  /// The personal stroke bank in its wire format, for export and backup.
  Uint8List? exportInkPrototypes() {
    final library = DynamicLibrary.open(libraryPath);
    final export = library.lookupFunction<_ExportNative, _ExportDart>(
      'scanner_export_ink_prototypes',
    );
    final freeBytes = library.lookupFunction<_FreeBytesNative, _FreeBytesDart>(
      'scanner_free_bytes',
    );
    final sizePtr = malloc<Size>();
    try {
      final data = export(sizePtr);
      if (data == nullptr || sizePtr.value == 0) return null;
      try {
        return Uint8List.fromList(data.asTypedList(sizePtr.value));
      } finally {
        freeBytes(data);
      }
    } finally {
      malloc.free(sizePtr);
    }
  }

  /// Replaces the personal stroke bank with a previously exported set.
  bool loadInkPrototypes(Uint8List bytes) {
    if (bytes.isEmpty) return false;
    final library = DynamicLibrary.open(libraryPath);
    final load = library.lookupFunction<_InkProtoLoadNative, _InkProtoLoadDart>(
      'scanner_load_ink_prototypes',
    );
    final ptr = malloc<Uint8>(bytes.length);
    try {
      ptr.asTypedList(bytes.length).setAll(0, bytes);
      return load(ptr, bytes.length) == 1;
    } finally {
      malloc.free(ptr);
    }
  }

  void clearInkPrototypes() {
    final library = DynamicLibrary.open(libraryPath);
    final clear = library.lookupFunction<Void Function(), void Function()>(
      'scanner_clear_ink_prototypes',
    );
    clear();
  }

  /// Reads a photo of handwriting through the ink pipeline (plan §12A.8).
  ///
  /// The ink is thinned, cut at junctions and traced back into strokes, then the
  /// shared grouping/structure/repair path reads it. Experimental: crossing
  /// strokes lose information a live canvas keeps. Exposed in the Scanner Lab,
  /// not in the normal scan flow.
  Future<ScanResult> readHandwriting(Uint8List image) async {
    if (image.isEmpty) {
      throw ScannerUnavailable('the selected image is empty');
    }
    final raw = await Isolate.run(
      () => _handwritingInIsolate((libraryPath, image)),
    );
    return _decode(raw, 0);
  }

  /// One pipeline stage as PNG bytes, for the Scanner Lab. Runs in a worker
  /// isolate so the stage viewer never blocks drawing or scrolling.
  Future<Uint8List?> debugStage(Uint8List image, String stage) {
    if (image.isEmpty) return Future.value(null);
    return Isolate.run(() => _stageInIsolate((libraryPath, image, stage)));
  }

  /// Stage of the read that is running in the worker isolate right now.
  ///
  /// The native library keeps the stage in an atomic, so the UI isolate can
  /// read it while the recognition isolate is busy: no message passing, no
  /// guessing, and the stepper names the stage that is actually running.
  /// 0 idle, 1 prepare, 2 find symbols, 3 recognize, 4 structure, 5 check,
  /// 6 done.
  static _StageDart? _stageFn;

  static int currentStage() {
    try {
      final fn = _stageFn ??= DynamicLibrary.open(
        NativeEngine.instance.libraryPath,
      ).lookupFunction<_StageNative, _StageDart>('scanner_stage');
      return fn();
    } catch (_) {
      return 0;
    }
  }

  /// Loads a personal prototype set (Tier B, kNN over plain feature data).
  /// Merge a bundled set with the user's own samples; no model file is
  /// involved. Returns false when the bank is not loaded or the data is bad.
  bool loadPrototypes(Uint8List bytes) {
    if (bytes.isEmpty) return false;
    final library = DynamicLibrary.open(libraryPath);
    final load = library
        .lookupFunction<_PrototypeLoadNative, _PrototypeLoadDart>(
          'scanner_load_prototypes',
        );
    final ptr = malloc<Uint8>(bytes.length);
    try {
      ptr.asTypedList(bytes.length).setAll(0, bytes);
      return load(ptr, bytes.length) == 1;
    } finally {
      malloc.free(ptr);
    }
  }

  /// Adds one labelled sample from a PNG containing a single symbol. The
  /// handwriting board calls this for every stroke group it has a label for.
  bool addPrototype(String label, Uint8List png) {
    if (label.isEmpty || png.isEmpty) return false;
    final library = DynamicLibrary.open(libraryPath);
    final add = library.lookupFunction<_PrototypeAddNative, _PrototypeAddDart>(
      'scanner_add_prototype',
    );
    final labelPtr = label.toNativeUtf8();
    final ptr = malloc<Uint8>(png.length);
    try {
      ptr.asTypedList(png.length).setAll(0, png);
      return add(labelPtr, ptr, png.length) == 1;
    } finally {
      malloc.free(ptr);
      malloc.free(labelPtr);
    }
  }

  /// Number of loaded prototypes, for the "personal samples: N" line.
  int get prototypeCount {
    final library = DynamicLibrary.open(libraryPath);
    final count = library
        .lookupFunction<_PrototypeCountNative, _PrototypeCountDart>(
          'scanner_prototype_count',
        );
    return count();
  }

  /// Handwriting mode answers with the personal prototypes before the printed
  /// templates. Returns false when the bank is not loaded.
  bool setHandwritingMode(bool enabled) {
    final library = DynamicLibrary.open(libraryPath);
    final set = library
        .lookupFunction<_HandwritingNative, _HandwritingDart>(
          'scanner_set_handwriting_mode',
        );
    return set(enabled ? 1 : 0) == 1;
  }

  /// Serialises the current prototype set so the app can persist it. Null
  /// when there is nothing to save or the bank is not loaded.
  Uint8List? exportPrototypes() {
    final library = DynamicLibrary.open(libraryPath);
    final export = library.lookupFunction<_ExportNative, _ExportDart>(
      'scanner_export_prototypes',
    );
    final freeBytes = library.lookupFunction<_FreeBytesNative, _FreeBytesDart>(
      'scanner_free_bytes',
    );
    final sizePtr = malloc<Size>();
    try {
      final data = export(sizePtr);
      if (data == nullptr || sizePtr.value == 0) return null;
      try {
        return Uint8List.fromList(data.asTypedList(sizePtr.value));
      } finally {
        freeBytes(data);
      }
    } finally {
      malloc.free(sizePtr);
    }
  }

  ScanResult _decode(String raw, int session) {
    final decoded = jsonDecode(raw);
    if (decoded is! Map<String, dynamic>) {
      throw ScannerUnavailable('the scanner returned malformed JSON');
    }
    if (decoded['ok'] != true) {
      throw ScannerUnavailable(
        decoded['error'] as String? ?? 'the scanner could not read the image',
      );
    }
    return ScanResult.fromJson(
      decoded,
      repaired: decoded['repaired'] == true,
      session: session,
    );
  }
}
