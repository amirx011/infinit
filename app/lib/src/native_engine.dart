// FFI bridge to the exact ODE engine (libodecore).
import 'dart:async';
import 'dart:convert';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';

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

class _OpenedLibrary {
  const _OpenedLibrary(this.path, this.library);

  final String path;
  final DynamicLibrary library;
}

// Runs in a background isolate: it opens the library independently and returns
// the raw JSON response, so no native pointer ever crosses an isolate boundary.
String _solveInIsolate(Map<String, String> payload) {
  final library = DynamicLibrary.open(payload['library']!);
  final solve = library.lookupFunction<_SolveNative, _SolveDart>('ode_solve');
  final free = library.lookupFunction<_FreeNative, _FreeDart>('ode_free');

  final requestPtr = (payload['request'] ?? '').toNativeUtf8();
  Pointer<Utf8> responsePtr = nullptr;
  try {
    responsePtr = solve(requestPtr);
    if (responsePtr == nullptr) {
      throw EngineUnavailable('موتور محاسبات پاسخی برنگرداند.');
    }
    return responsePtr.toDartString();
  } finally {
    if (responsePtr != nullptr) free(responsePtr);
    calloc.free(requestPtr);
  }
}

class NativeEngine {
  NativeEngine._(DynamicLibrary library, this.libraryPath)
    : _solve = library.lookupFunction<_SolveNative, _SolveDart>('ode_solve'),
      _free = library.lookupFunction<_FreeNative, _FreeDart>('ode_free'),
      _version = library.lookupFunction<_VersionNative, _VersionDart>(
        'ode_core_version',
      );

  final _SolveDart _solve;
  final _FreeDart _free;
  final _VersionDart _version;

  /// Path that successfully loaded the native library. The background isolate
  /// re-opens this same path.
  final String libraryPath;
  String? _cachedVersion;

  /// Symbolic work runs off the UI thread. Long ODE analyses are bounded so a
  /// pathological input cannot leave the page spinning forever.
  static const Duration timeout = Duration(seconds: 60);

  static NativeEngine? _instance;
  static String? _loadError;

  static NativeEngine get instance {
    final existing = _instance;
    if (existing != null) return existing;
    try {
      final opened = _open();
      _instance = NativeEngine._(opened.library, opened.path);
      return _instance!;
    } catch (e) {
      _loadError = e.toString();
      throw EngineUnavailable('موتور محاسبات بومی بارگذاری نشد.\n$_loadError');
    }
  }

  static String? get loadError => _loadError;

  static _OpenedLibrary _open() {
    final candidates = <String>[];

    final override = Platform.environment['INFINIT_LIB'];
    if (override != null && override.isNotEmpty) candidates.add(override);

    if (Platform.isAndroid) {
      // The APK carries the library once per ABI (android/app/src/main/jniLibs);
      // the platform loader resolves the bare name inside the app's lib folder.
      candidates.addAll([
        'libodecore.so',
        'lib/libodecore.so',
        '/data/local/tmp/libodecore.so',
      ]);
    } else if (Platform.isLinux) {
      // Bundled layout: <app>/lib/libodecore.so
      final exeDir = File(Platform.resolvedExecutable).parent.path;
      candidates.addAll(['$exeDir/lib/libodecore.so', '$exeDir/libodecore.so']);
    }
    candidates.addAll([
      'libodecore.so',
      'lib/libodecore.so',
      'core/build/lib/libodecore.so',
      '../core/build/lib/libodecore.so',
      '../../core/build/lib/libodecore.so',
    ]);

    final failures = <String>[];
    for (final path in candidates) {
      try {
        return _OpenedLibrary(path, DynamicLibrary.open(path));
      } catch (e) {
        failures.add('$path → $e');
      }
    }
    final hint = Platform.isAndroid
        ? '\n\nراهنما: libodecore.so باید داخل خود APK باشد. APK را با '
              'scripts/build_android.sh بسازید یا با Gradle بیلد کنید تا وظیفهٔ '
              'buildOdeCoreAndroid آن را در jniLibs بگذارد.\n'
              'Hint: the APK must contain libodecore.so for the device ABI '
              '(build with scripts/build_android.sh or through Gradle).'
        : '';
    throw EngineUnavailable(
      'کتابخانهٔ libodecore.so پیدا نشد. مسیرها بررسی شد:\n${failures.join('\n')}$hint',
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

  /// Synchronous call kept for CLI/tests and for callers that already run on a
  /// worker isolate. UI code should prefer [solveAsync].
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

  /// Solves on a background isolate so the Flutter UI thread stays responsive.
  Future<SolveResult> solveAsync(Map<String, dynamic> request) async {
    final payload = <String, String>{
      'library': libraryPath,
      'request': jsonEncode(request),
    };
    try {
      final raw = await Isolate.run(() => _solveInIsolate(payload))
          .timeout(timeout);
      return SolveResult.fromRaw(raw);
    } on TimeoutException {
      throw EngineUnavailable(
        'محاسبه بیش از ${timeout.inSeconds} ثانیه طول کشید و متوقف شد.\n'
        'The computation took longer than ${timeout.inSeconds}s and was stopped.',
      );
    }
  }

  // --------------------------------------------------------------- requests
  Map<String, dynamic> _odeRequest(
    String input, {
    required String x,
    required String y,
    required String particular,
    required String lang,
  }) => {
    'mode': 'ode',
    'input': input,
    'x': x,
    'y': y,
    'particular': particular,
    'lang': lang,
  };

  Map<String, dynamic> _integralRequest(
    String input, {
    required String x,
    required List<String> vars,
    required bool definite,
    required List<String> lower,
    required List<String> upper,
    required String lang,
  }) => {
    'mode': 'integral',
    'input': input,
    'x': x,
    'dimension': vars.length,
    'vars': vars,
    'definite': definite,
    'lower': lower,
    'upper': upper,
    'lang': lang,
  };

  Map<String, dynamic> _derivativeRequest(
    String input, {
    required String x,
    required int order,
    required String lang,
  }) => {
    'mode': 'derivative',
    'input': input,
    'x': x,
    'order': order,
    'lang': lang,
  };

  Map<String, dynamic> _limitRequest(
    String input, {
    required String x,
    required String point,
    required String side,
    required String lang,
  }) => {
    'mode': 'limit',
    'input': input,
    'x': x,
    'point': point,
    'side': side,
    'lang': lang,
  };

  Map<String, dynamic> _continuityRequest(
    String input, {
    required String x,
    required String point,
    required String lang,
  }) => {
    'mode': 'continuity',
    'input': input,
    'x': x,
    'point': point,
    'lang': lang,
  };

  Map<String, dynamic> _wronskianRequest(
    List<String> functions, {
    required String x,
    required String lang,
  }) => {'mode': 'wronskian', 'functions': functions, 'x': x, 'lang': lang};

  Map<String, dynamic> _envelopeRequest(
    String input, {
    required String c,
    required bool orthogonal,
    required String x,
    required String y,
    required String lang,
  }) => {
    'mode': orthogonal ? 'orthogonal' : 'envelope',
    'input': input,
    'c': c,
    'orthogonal': orthogonal,
    'x': x,
    'y': y,
    'lang': lang,
  };

  Map<String, dynamic> _obliqueRequest(
    String input, {
    required String c,
    required double angle,
    required String x,
    required String y,
    required String lang,
  }) => {
    'mode': 'oblique',
    'input': input,
    'c': c,
    'angle': angle,
    'x': x,
    'y': y,
    'lang': lang,
  };

  // ------------------------------------------- applications of the derivative
  Map<String, dynamic> _behaviourRequest(
    String input, {
    required String x,
    required double xmin,
    required double xmax,
    required String lang,
  }) => {
    'mode': 'behaviour',
    'input': input,
    'x': x,
    'xmin': xmin,
    'xmax': xmax,
    'lang': lang,
  };

  Map<String, dynamic> _tangentRequest(
    String input, {
    required String x,
    required String at,
    required String lang,
  }) => {'mode': 'tangent', 'input': input, 'x': x, 'at': at, 'lang': lang};

  Map<String, dynamic> _optimizeRequest(
    String input, {
    required String x,
    required double xmin,
    required double xmax,
    required String lang,
  }) => {
    'mode': 'optimize',
    'input': input,
    'x': x,
    'xmin': xmin,
    'xmax': xmax,
    'lang': lang,
  };

  Map<String, dynamic> _relatedRatesRequest(
    String input, {
    required String variable,
    required String rateVariable,
    required String time,
    required String lang,
    double? qValue,
    double? wValue,
    double? givenRate,
  }) => {
    'mode': 'related_rates',
    'input': input,
    'var': variable,
    'rate_var': rateVariable,
    't': time,
    'lang': lang,
    // Omitted numbers let the engine keep the answer symbolic.
    'q_value': ?qValue,
    'w_value': ?wValue,
    'given_rate': ?givenRate,
  };

  Map<String, dynamic> _mvtRequest(
    String input, {
    required String x,
    required double a,
    required double b,
    required String lang,
  }) => {
    'mode': 'mvt',
    'input': input,
    'x': x,
    'a': a,
    'b': b,
    'lang': lang,
  };

  Map<String, dynamic> _cauchyRequest(
    String f,
    String g, {
    required String x,
    required double a,
    required double b,
    required String lang,
  }) => {
    'mode': 'cauchy',
    'input': f,
    'g': g,
    'x': x,
    'a': a,
    'b': b,
    'lang': lang,
  };

  Map<String, dynamic> _lhopitalRequest(
    String f,
    String g, {
    required String x,
    required String point,
    required String lang,
  }) => {
    'mode': 'lhopital',
    'input': f,
    'g': g,
    'x': x,
    'point': point,
    'lang': lang,
  };

  // ------------------------------------------------------------------- ODE
  SolveResult solveOde(
    String input, {
    String x = 'x',
    String y = 'y',
    String particular = '',
    String lang = 'en',
  }) =>
      solve(_odeRequest(input, x: x, y: y, particular: particular, lang: lang));

  Future<SolveResult> solveOdeAsync(
    String input, {
    String x = 'x',
    String y = 'y',
    String particular = '',
    String lang = 'en',
  }) => solveAsync(
    _odeRequest(input, x: x, y: y, particular: particular, lang: lang),
  );

  // --------------------------------------------------------------- integral
  SolveResult integral(
    String input, {
    String x = 'x',
    List<String>? vars,
    int dimension = 1,
    bool definite = false,
    List<String> lower = const [],
    List<String> upper = const [],
    String lang = 'en',
  }) {
    const defaults = <String>['x', 'y', 'z'];
    final activeVars = vars ?? defaults.take(dimension).toList();
    return solve(
      _integralRequest(
        input,
        x: x,
        vars: activeVars,
        definite: definite,
        lower: lower,
        upper: upper,
        lang: lang,
      ),
    );
  }

  Future<SolveResult> integralAsync(
    String input, {
    String x = 'x',
    List<String>? vars,
    int dimension = 1,
    bool definite = false,
    List<String> lower = const [],
    List<String> upper = const [],
    String lang = 'en',
  }) {
    const defaults = <String>['x', 'y', 'z'];
    final activeVars = vars ?? defaults.take(dimension).toList();
    return solveAsync(
      _integralRequest(
        input,
        x: x,
        vars: activeVars,
        definite: definite,
        lower: lower,
        upper: upper,
        lang: lang,
      ),
    );
  }

  // ------------------------------------------------------------- derivative
  SolveResult derivative(
    String input, {
    String x = 'x',
    int order = 1,
    String lang = 'en',
  }) => solve(_derivativeRequest(input, x: x, order: order, lang: lang));

  Future<SolveResult> derivativeAsync(
    String input, {
    String x = 'x',
    int order = 1,
    String lang = 'en',
  }) => solveAsync(_derivativeRequest(input, x: x, order: order, lang: lang));

  // ------------------------------------------------------------------ limit
  SolveResult limit(
    String input, {
    String x = 'x',
    String point = '0',
    String side = 'both',
    String lang = 'en',
  }) => solve(_limitRequest(input, x: x, point: point, side: side, lang: lang));

  Future<SolveResult> limitAsync(
    String input, {
    String x = 'x',
    String point = '0',
    String side = 'both',
    String lang = 'en',
  }) => solveAsync(
    _limitRequest(input, x: x, point: point, side: side, lang: lang),
  );

  // ------------------------------------------------------------- continuity
  SolveResult continuity(
    String input, {
    String x = 'x',
    String point = '0',
    String lang = 'en',
  }) => solve(_continuityRequest(input, x: x, point: point, lang: lang));

  Future<SolveResult> continuityAsync(
    String input, {
    String x = 'x',
    String point = '0',
    String lang = 'en',
  }) => solveAsync(_continuityRequest(input, x: x, point: point, lang: lang));

  // -------------------------------------------------------------- wronskian
  SolveResult wronskian(
    List<String> functions, {
    String x = 'x',
    String lang = 'en',
  }) => solve(_wronskianRequest(functions, x: x, lang: lang));

  Future<SolveResult> wronskianAsync(
    List<String> functions, {
    String x = 'x',
    String lang = 'en',
  }) => solveAsync(_wronskianRequest(functions, x: x, lang: lang));

  // --------------------------------------------------------------- envelope
  SolveResult envelope(
    String input, {
    String c = 'c',
    bool orthogonal = false,
    String x = 'x',
    String y = 'y',
    String lang = 'en',
  }) => solve(
    _envelopeRequest(
      input,
      c: c,
      orthogonal: orthogonal,
      x: x,
      y: y,
      lang: lang,
    ),
  );

  Future<SolveResult> envelopeAsync(
    String input, {
    String c = 'c',
    bool orthogonal = false,
    String x = 'x',
    String y = 'y',
    String lang = 'en',
  }) => solveAsync(
    _envelopeRequest(
      input,
      c: c,
      orthogonal: orthogonal,
      x: x,
      y: y,
      lang: lang,
    ),
  );

  // ------------------------------------------------------- oblique trajectories
  SolveResult oblique(
    String input, {
    String c = 'c',
    double angle = 45,
    String x = 'x',
    String y = 'y',
    String lang = 'en',
  }) => solve(
    _obliqueRequest(
      input,
      c: c,
      angle: angle,
      x: x,
      y: y,
      lang: lang,
    ),
  );

  Future<SolveResult> obliqueAsync(
    String input, {
    String c = 'c',
    double angle = 45,
    String x = 'x',
    String y = 'y',
    String lang = 'en',
  }) => solveAsync(
    _obliqueRequest(
      input,
      c: c,
      angle: angle,
      x: x,
      y: y,
      lang: lang,
    ),
  );

  // ------------------------------------------- applications of the derivative
  Future<SolveResult> behaviourAsync(
    String input, {
    String x = 'x',
    double xmin = -10,
    double xmax = 10,
    String lang = 'en',
  }) => solveAsync(
    _behaviourRequest(input, x: x, xmin: xmin, xmax: xmax, lang: lang),
  );

  Future<SolveResult> tangentAsync(
    String input, {
    String x = 'x',
    String at = '1',
    String lang = 'en',
  }) => solveAsync(_tangentRequest(input, x: x, at: at, lang: lang));

  Future<SolveResult> optimizeAsync(
    String input, {
    String x = 'x',
    double xmin = -5,
    double xmax = 5,
    String lang = 'en',
  }) => solveAsync(
    _optimizeRequest(input, x: x, xmin: xmin, xmax: xmax, lang: lang),
  );

  Future<SolveResult> relatedRatesAsync(
    String input, {
    String variable = 'x',
    String rateVariable = 'y',
    String time = 't',
    String lang = 'en',
    double? qValue,
    double? wValue,
    double? givenRate,
  }) => solveAsync(
    _relatedRatesRequest(
      input,
      variable: variable,
      rateVariable: rateVariable,
      time: time,
      lang: lang,
      qValue: qValue,
      wValue: wValue,
      givenRate: givenRate,
    ),
  );

  Future<SolveResult> meanValueAsync(
    String input, {
    String x = 'x',
    double a = 0,
    double b = 1,
    String lang = 'en',
  }) => solveAsync(_mvtRequest(input, x: x, a: a, b: b, lang: lang));

  Future<SolveResult> cauchyAsync(
    String f,
    String g, {
    String x = 'x',
    double a = 0,
    double b = 1,
    String lang = 'en',
  }) => solveAsync(_cauchyRequest(f, g, x: x, a: a, b: b, lang: lang));

  Future<SolveResult> lhopitalAsync(
    String f,
    String g, {
    String x = 'x',
    String point = '0',
    String lang = 'en',
  }) => solveAsync(_lhopitalRequest(f, g, x: x, point: point, lang: lang));

  // ----------------------------------------------------------------- graphs
  Future<Map<String, dynamic>> _graphAsync(Map<String, dynamic> request) async {
    final payload = <String, String>{
      'library': libraryPath,
      'request': jsonEncode(request),
    };
    try {
      final raw = await Isolate.run(() => _solveInIsolate(payload))
          .timeout(timeout);
      final decoded = jsonDecode(raw);
      if (decoded is! Map<String, dynamic>) {
        throw const FormatException('پاسخ گراف معتبر نیست.');
      }
      return decoded;
    } on TimeoutException {
      throw EngineUnavailable(
        'رسم نمودار بیش از ${timeout.inSeconds} ثانیه طول کشید.\n'
        'Plotting took longer than ${timeout.inSeconds}s and was stopped.',
      );
    }
  }

  /// Samples y = f(x) on a uniform grid for plotting.
  Future<SampleResult> sampleAsync(
    String input, {
    String x = 'x',
    double xmin = -10,
    double xmax = 10,
    int samples = 600,
    Map<String, double> constants = const {},
  }) async => SampleResult.fromJson(
    await _graphAsync({
      'mode': 'sample',
      'input': input,
      'x': x,
      'xmin': xmin,
      'xmax': xmax,
      'samples': samples,
      'constants': constants,
    }),
  );

  /// Numeric domain/range scan with holes and asymptotes.
  Future<DomainResult> domainAsync(
    String input, {
    String x = 'x',
    double xmin = -12,
    double xmax = 12,
    int samples = 2401,
  }) async => DomainResult.fromJson(
    await _graphAsync({
      'mode': 'domain',
      'input': input,
      'x': x,
      'xmin': xmin,
      'xmax': xmax,
      'samples': samples,
    }),
  );

  /// Direction field for y' = f(x, y) plus RK4 trajectories.
  Future<FieldResult> fieldAsync(
    String input, {
    String x = 'x',
    String y = 'y',
    double xmin = -6,
    double xmax = 6,
    double ymin = -6,
    double ymax = 6,
    int cols = 21,
    int rows = 21,
    int curves = 6,
    List<List<double>> ics = const [],
  }) async => FieldResult.fromJson(
    await _graphAsync({
      'mode': 'field',
      'input': input,
      'x': x,
      'y': y,
      'xmin': xmin,
      'xmax': xmax,
      'ymin': ymin,
      'ymax': ymax,
      'cols': cols,
      'rows': rows,
      'curves': curves,
      'ics': ics,
    }),
  );
}
