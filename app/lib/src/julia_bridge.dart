// Optional Julia sidecar for desktop.
//
// The app never bundles Julia.  On Linux, Windows and macOS it looks for a
// local `julia` executable (or a copy under `julia/bin`) and starts the small
// JSON bridge in `julia/src/InfinitBridge.jl`.  If anything is missing, the
// caller falls back to the C++ core.  Android never starts this bridge.
import 'dart:async';
import 'dart:convert';
import 'dart:io';

class JuliaBridge {
  JuliaBridge._();

  static final JuliaBridge instance = JuliaBridge._();

  Process? _process;
  bool _starting = false;
  bool _available = false;
  bool _permanentlyUnavailable = false;
  int _nextId = 1;
  final _pending = <int, Completer<Map<String, dynamic>>>{};
  StreamSubscription<String>? _stdoutSub;
  StreamSubscription<String>? _stderrSub;

  bool get isDesktop =>
      Platform.isLinux || Platform.isWindows || Platform.isMacOS;

  bool get isAvailable => _available;

  /// Set `INFINIT_NO_JULIA=1` to force the universal C++ core even when a
  /// Julia installation is present.
  static bool get _disabled {
    final value = Platform.environment['INFINIT_NO_JULIA'];
    return value != null && value.isNotEmpty && value != '0';
  }

  /// Starts the sidecar once.  Safe to call from every solver request.
  Future<bool> ensureStarted() async {
    if (_available && _process != null) return true;
    if (!isDesktop || _disabled || _permanentlyUnavailable) return false;
    if (_starting) {
      // Wait for the in-flight start instead of launching a second process.
      for (var i = 0; i < 100 && _starting; i++) {
        await Future<void>.delayed(const Duration(milliseconds: 50));
      }
      return _available;
    }
    _starting = true;
    try {
      final launch = await _findLaunch();
      if (launch == null) {
        _permanentlyUnavailable = true;
        return false;
      }
      final process = await Process.start(
        launch.executable,
        launch.arguments,
        runInShell: Platform.isWindows,
      );
      _process = process;
      _stdoutSub = process.stdout
          .transform(utf8.decoder)
          .transform(const LineSplitter())
          .listen(_onLine);
      _stderrSub = process.stderr
          .transform(utf8.decoder)
          .transform(const LineSplitter())
          .listen((_) {
        // Diagnostics are intentionally ignored; the UI never exposes the
        // engine choice and the fallback is silent.
      });
      process.exitCode.then((_) {
        _available = false;
        _process = null;
        _failPending('Julia sidecar stopped.');
      });
      final pong = await _send('ping', const {}, timeout: const Duration(seconds: 12));
      _available = pong != null && pong['pong'] == true;
      if (!_available) _permanentlyUnavailable = true;
      return _available;
    } catch (_) {
      _permanentlyUnavailable = true;
      _available = false;
      return false;
    } finally {
      _starting = false;
    }
  }

  Future<_Launch?> _findLaunch() async {
    final env = Platform.environment;
    final executable = Platform.isWindows ? 'julia.exe' : 'julia';
    final candidates = <String>[];
    final override = env['INFINIT_JULIA'];
    if (override != null && override.isNotEmpty) candidates.add(override);
    final exeDir = File(Platform.resolvedExecutable).parent;
    candidates.addAll([
      '${exeDir.path}/julia/bin/$executable',
      '${exeDir.path}/julia/$executable',
      '${exeDir.path}/../julia/bin/$executable',
    ]);
    // Development layout: walk up from the executable/current directory until
    // a `julia/src/InfinitBridge.jl` appears.
    var dir = Directory.current;
    for (var i = 0; i < 6; i++) {
      candidates.add('${dir.path}/julia/bin/$executable');
      candidates.add('${dir.path}/$executable');
      final project = File('${dir.path}/julia/src/InfinitBridge.jl');
      if (project.existsSync()) {
        candidates.add(executable);
        break;
      }
      final parent = dir.parent;
      if (parent.path == dir.path) break;
      dir = parent;
    }
    candidates.add(executable);

    String? found;
    for (final candidate in candidates) {
      if (candidate == executable) {
        found = candidate;
        break;
      }
      if (File(candidate).existsSync()) {
        found = candidate;
        break;
      }
    }
    if (found == null) return null;

    final projectDir = _findProjectDir();
    if (projectDir == null) return null;
    return _Launch(found, [
      '--startup-file=no',
      '--project=$projectDir',
      '$projectDir/src/InfinitBridge.jl',
    ]);
  }

  String? _findProjectDir() {
    final env = Platform.environment;
    final override = env['INFINIT_JULIA_PROJECT'];
    if (override != null && override.isNotEmpty) {
      if (File('$override/src/InfinitBridge.jl').existsSync()) return override;
    }
    var dir = Directory.current;
    for (var i = 0; i < 6; i++) {
      final candidate = '${dir.path}/julia';
      if (File('$candidate/src/InfinitBridge.jl').existsSync()) {
        return candidate;
      }
      final parent = dir.parent;
      if (parent.path == dir.path) break;
      dir = parent;
    }
    final exeDir = File(Platform.resolvedExecutable).parent;
    for (final candidate in [
      '${exeDir.path}/julia',
      '${exeDir.path}/../julia',
      '${exeDir.path}/../../julia',
    ]) {
      if (File('$candidate/src/InfinitBridge.jl').existsSync()) return candidate;
    }
    return null;
  }

  /// Calls a bridge method.  Returns null when the sidecar is unavailable or
  /// the call fails; callers then fall back to the native core.
  Future<Map<String, dynamic>?> call(
    String method,
    Map<String, dynamic> params, {
    Duration timeout = const Duration(seconds: 45),
  }) async {
    if (!await ensureStarted()) return null;
    final response = await _send(method, params, timeout: timeout);
    if (response == null) return null;
    // `_send` already unwraps the envelope and returns the result payload.
    if (response['ok'] == false && response.containsKey('error')) return null;
    return response;
  }

  Future<Map<String, dynamic>?> _send(
    String method,
    Map<String, dynamic> params, {
    required Duration timeout,
  }) async {
    final process = _process;
    if (process == null) return null;
    final id = _nextId++;
    final completer = Completer<Map<String, dynamic>>();
    _pending[id] = completer;
    try {
      process.stdin.writeln(jsonEncode({
        'id': id,
        'method': method,
        'params': params,
      }));
      await process.stdin.flush();
      return await completer.future.timeout(timeout);
    } catch (_) {
      return null;
    } finally {
      _pending.remove(id);
    }
  }

  void _onLine(String line) {
    if (line.trim().isEmpty) return;
    try {
      final decoded = jsonDecode(line);
      if (decoded is! Map) return;
      final id = decoded['id'];
      if (id is! int) return;
      final completer = _pending.remove(id);
      if (completer == null) return;
      if (decoded['ok'] == true) {
        final result = decoded['result'];
        completer.complete(
          result is Map ? Map<String, dynamic>.from(result) : <String, dynamic>{},
        );
      } else {
        completer.complete(<String, dynamic>{
          'ok': false,
          'error': (decoded['error'] ?? 'Julia sidecar error').toString(),
        });
      }
    } catch (_) {
      // Ignore malformed lines; the pending request will time out and fall
      // back to C++.
    }
  }

  void _failPending(String message) {
    for (final completer in _pending.values) {
      if (!completer.isCompleted) {
        completer.complete(<String, dynamic>{'ok': false, 'error': message});
      }
    }
    _pending.clear();
  }

  Future<void> dispose() async {
    await _stdoutSub?.cancel();
    await _stderrSub?.cancel();
    _process?.kill();
    _process = null;
    _available = false;
  }
}

class _Launch {
  const _Launch(this.executable, this.arguments);

  final String executable;
  final List<String> arguments;
}
