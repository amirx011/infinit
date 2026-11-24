import 'dart:io';

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/scanner_engine.dart';

/// Serves the template bank from the scanner checkout, so the test does not
/// depend on the Flutter asset bundle being generated first.
class _CheckoutBundle extends CachingAssetBundle {
  @override
  Future<ByteData> load(String key) async {
    if (key != 'assets/scanner/glyph_templates.bin') {
      throw StateError('unexpected asset: $key');
    }
    final bytes = await File(
      '../scanner/data/glyph_templates.bin',
    ).readAsBytes();
    return ByteData.sublistView(bytes);
  }
}

Future<Uint8List> _sample(String name) =>
    File('../scanner/samples/$name').readAsBytes();

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('reads a printed equation end to end', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    final result = await scanner.read(await _sample('plain_linear.png'));
    expect(result.kind, 'expression');
    expect(result.expression, '3x+5=2x-7');
    expect(result.parts, isEmpty);
    expect(result.symbols, greaterThan(0));
    expect(result.confidence, greaterThan(0.5));
  });

  test('routes a series to its named parts', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    final result = await scanner.read(await _sample('sum.png'));
    expect(result.kind, 'sum');
    expect(result.parts['term'], '1/(n^2)');
    expect(result.parts['index'], 'n');
    expect(result.parts['start'], '1');
    expect(result.parts['end'], 'inf');
  });

  test('routes an integral to its named parts', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    final result = await scanner.read(await _sample('integral.png'));
    expect(result.kind, 'integral');
    expect(result.parts['integrand'], 'x^2');
    expect(result.parts['variable'], 'x');
    expect(result.parts['lower'], '0');
    expect(result.parts['upper'], '1');
  });

  test('routes a limit to its named parts', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    final result = await scanner.read(await _sample('limit.png'));
    expect(result.kind, 'limit');
    expect(result.parts['expression'], 'sin(x)/x');
    expect(result.parts['variable'], 'x');
    expect(result.parts['point'], '0');
  });

  test('splits a two-equation system', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    final result = await scanner.read(await _sample('system.png'));
    expect(result.kind, 'system');
    expect(result.parts['equation1'], '2x+y=5');
    expect(result.parts['equation2'], 'x-y=1');
  });

  test('bundled template bank matches the scanner checkout', () async {
    final bundled = await File(
      'assets/scanner/glyph_templates.bin',
    ).readAsBytes();
    final source = await File(
      '../scanner/data/glyph_templates.bin',
    ).readAsBytes();
    expect(bundled, source);
  });

  test('rejects an empty image', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    expect(
      () => scanner.read(Uint8List(0)),
      throwsA(isA<ScannerUnavailable>()),
    );
  });
}
