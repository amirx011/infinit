// FFI bridge to the exact ODE engine (libodecore).
import 'dart:convert';
import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';

import 'models.dart';

typedef _SolveNative = Pointer<Utf8> Function(Pointer<Utf8> request);
typedef _SolveDart = Pointer<Utf8> Function(Pointer<Utf8> request);
typedef _FreeNative = Void Function(Pointer<Utf8> text);
typedef _FreeDart = void Function(Pointer<Utf8> text);
typedef _VersionNative = Pointer<Utf8> Function();
typedef _VersionDart = Pointer<Utf8> Function();

class EngineUnavailable implements Exception {
  EngineUnavailable(this.message);
  final String message;
  @override
  String toString() => message;
}

class NativeEngine {
  NativeEngine._(DynamicLibrary library)
      : _solve = library.lookupFunction<_SolveNative, _SolveDart>('ode_solve'),
        _free = library.lookupFunction<_FreeNative, _FreeDart>('ode_free'),
        _version = library.lookupFunction<_VersionNative, _VersionDart>(
            'ode_core_version');

  final _SolveDart _solve;
  final _FreeDart _free;
  final _VersionDart _version;
  String? _cachedVersion;

  static NativeEngine? _instance;
  static String? _loadError;

  static NativeEngine get instance {
    final existing = _instance;
    if (existing != null) return existing;
    try {
      final lib = _open();
      _instance = NativeEngine._(lib);
      return _instance!;
    } catch (e) {
      _loadError = e.toString();
      throw EngineUnavailable(
        'موتور محاسبات بومی بارگذاری نشد.\n$_loadError',
      );
    }
  }

  static String? get loadError => _loadError;

  static DynamicLibrary _open() {
    final candidates = <String>[];

    final override = Platform.environment['ODELAB_LIB'];
    if (override != null && override.isNotEmpty) candidates.add(override);

    if (Platform.isLinux) {
      // Bundled layout: <app>/lib/libodecore.so
      final exeDir = File(Platform.resolvedExecutable).parent.path;
      candidates.addAll([
        '$exeDir/lib/libodecore.so',
        '$exeDir/libodecore.so',
      ]);
    }
    candidates.addAll([
      'libodecore.so',
      'lib/libodecore.so',
      'core/build/lib/libodecore.so',
      '../core/build/lib/libodecore.so',
      '../../core/build/lib/libodecore.so',
      '/home/RooTx/ode-lab/core/build/lib/libodecore.so',
    ]);

    final failures = <String>[];
    for (final path in candidates) {
      try {
        return DynamicLibrary.open(path);
      } catch (e) {
        failures.add('$path → $e');
      }
    }
    throw EngineUnavailable(
      'کتابخانهٔ libodecore.so پیدا نشد. مسیرها بررسی شد:\n${failures.join('\n')}',
    );
  }

  String get version {
    if (_cachedVersion != null) return _cachedVersion!;
    try {
      final ptr = _version();
      _cachedVersion = ptr.toDartString();
    } catch (_) {
      _cachedVersion = '?';
    }
    return _cachedVersion!;
  }

  SolveResult solve(Map<String, dynamic> request) {
    final requestPtr = jsonEncode(request).toNativeUtf8();
    Pointer<Utf8> responsePtr = nullptr;
    try {
      responsePtr = _solve(requestPtr);
      if (responsePtr == nullptr) {
        throw EngineUnavailable('موتور محاسبات پاسخی برنگرداند.');
      }
      final raw = responsePtr.toDartString();
      return SolveResult.fromRaw(raw);
    } finally {
      if (responsePtr != nullptr) _free(responsePtr);
      calloc.free(requestPtr);
    }
  }

  SolveResult solveOde(
    String input, {
    String x = 'x',
    String y = 'y',
    String particular = '',
    String lang = 'en',
  }) =>
      solve({
        'mode': 'ode',
        'input': input,
        'x': x,
        'y': y,
        'particular': particular,
        'lang': lang,
      });

  SolveResult integral(String input, {String x = 'x', String lang = 'en'}) => solve({
        'mode': 'integral',
        'input': input,
        'x': x,
        'lang': lang,
      });

  SolveResult derivative(
    String input, {
    String x = 'x',
    int order = 1,
    String lang = 'en',
  }) =>
      solve({
        'mode': 'derivative',
        'input': input,
        'x': x,
        'order': order,
        'lang': lang,
      });

  SolveResult wronskian(List<String> functions, {String x = 'x', String lang = 'en'}) => solve({
        'mode': 'wronskian',
        'functions': functions,
        'x': x,
        'lang': lang,
      });

  SolveResult envelope(
    String input, {
    String c = 'c',
    bool orthogonal = false,
    String x = 'x',
    String y = 'y',
    String lang = 'en',
  }) =>
      solve({
        'mode': orthogonal ? 'orthogonal' : 'envelope',
        'input': input,
        'c': c,
        'orthogonal': orthogonal,
        'x': x,
        'y': y,
        'lang': lang,
      });
}
