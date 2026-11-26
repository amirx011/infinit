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
    final bytes = await File('../scanner/data/glyph_templates.bin')
        .readAsBytes();
    return ByteData.sublistView(bytes);
  }
}

Future<Uint8List> _sample(String name) =>
    File('../scanner/samples/$name').readAsBytes();

void main() {
  _relationContractTests();
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

  test('reads a bracketed matrix into its cells', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    for (final name in ['matrix.png', 'matrix_photo.png']) {
      final result = await scanner.read(await _sample(name));
      expect(result.kind, 'matrix');
      expect(result.session, greaterThan(0));
      expect(result.matrix, '1, 2; 3, 4');
      expect(result.rows, 2);
      expect(result.columns, 2);
      expect(result.latex, contains(r'\begin{bmatrix}'));
    }
  });

  test('a symbol override rebuilds the reading', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    final image = await _sample('plain_linear.png');
    final first = await scanner.read(image);
    expect(first.session, greaterThan(0));
    expect(first.overrides, isEmpty);
    final glyph = first.glyphs.firstWhere((glyph) => glyph.label == '3');
    final changed = await scanner.relabel(first.session, {glyph.id: '8'});
    expect(changed.session, first.session);
    expect(changed.expression, contains('8'));
    expect(changed.expression, isNot(first.expression));
  });

  test('several overrides apply in one pass', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    final image = await _sample('quadratic.png');
    final first = await scanner.read(image);
    expect(first.expression, 'x^2-5x+6=0');
    final fixes = <int, String>{};
    for (final glyph in first.glyphs) {
      if (glyph.label == '5') fixes[glyph.id] = '9';
      if (glyph.label == '6') fixes[glyph.id] = '8';
    }
    expect(fixes, hasLength(2));
    final changed = await scanner.relabel(first.session, fixes);
    expect(changed.expression, 'x^2-9x+8=0');
  });

  test('bundled template bank matches the scanner checkout', () async {
    final bundled = await File('assets/scanner/glyph_templates.bin')
        .readAsBytes();
    final source = await File('../scanner/data/glyph_templates.bin')
        .readAsBytes();
    expect(bundled, source);
  });

  test('rejects an empty image', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    expect(
      () => scanner.read(Uint8List(0)),
      throwsA(isA<ScannerUnavailable>()),
    );
  });

  test('exposes the parser verdict and the stage timings', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    final result = await scanner.read(await _sample('plain_linear.png'));
    expect(result.status, 'ok');
    expect(result.parseValid, isTrue);
    expect(
      result.timing.keys,
      containsAll(['preprocess', 'segment', 'classify', 'repair']),
    );
    expect(result.timing['preprocess'], greaterThan(0));
    expect(result.timing['repair'], greaterThan(0));
    expect(result.lineDetails, hasLength(1));
    expect(result.lineDetails.first.parseValid, isTrue);
  });

  test('a cancelled request does not poison the next read', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    scanner.cancel();
    final result = await scanner.read(await _sample('quadratic.png'));
    expect(result.status, 'ok');
    expect(result.expression, 'x^2-5x+6=0');
  });

  test('prototype helpers are reachable and safe when empty', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    expect(scanner.setHandwritingMode(true), isTrue);
    expect(scanner.setHandwritingMode(false), isTrue);
    expect(scanner.prototypeCount, greaterThanOrEqualTo(0));
    final exported = scanner.exportPrototypes();
    if (exported != null) {
      expect(scanner.loadPrototypes(exported), isTrue);
    }
  });

  test('reads ink from the handwriting board', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    List<Offset> line(double x0, double y0, double x1, double y1) => [
      for (var i = 0; i <= 16; i++)
        Offset(x0 + (x1 - x0) * i / 16, y0 + (y1 - y0) * i / 16),
    ];
    final strokes = <List<Offset>>[
      line(40, 48, 70, 92),
      line(70, 48, 40, 92),
      line(100, 70, 130, 70),
      line(115, 55, 115, 85),
      line(160, 45, 160, 95),
      line(200, 62, 234, 62),
      line(200, 78, 234, 78),
      [
        const Offset(250, 45),
        const Offset(275, 45),
        const Offset(275, 70),
        const Offset(250, 95),
        const Offset(275, 95),
      ],
    ];
    final result = await scanner.readInk(
      strokes,
      width: 420,
      height: 140,
    );
    expect(result.status, anyOf('ok', 'partial'));
    expect(result.binarization, 'ink');
    expect(result.session, greaterThan(0));
    expect(result.expression, isNotEmpty);
    expect(result.symbols, 5);
    expect(result.timing['preprocess'], greaterThan(0));
  });

  test('merges and splits symbols on a live session', () async {
    final scanner = await ScannerEngine.load(bundle: _CheckoutBundle());
    List<Offset> bar(double x) => [
      for (var i = 0; i <= 16; i++) Offset(x, 40 + i * 4),
    ];
    final result = await scanner.readInk(
      [bar(40), bar(140)],
      width: 200,
      height: 200,
    );
    expect(result.symbols, 2);

    final merged = await scanner.mergeSymbols(result.session, 0, 1);
    expect(merged.symbols, 1);
    expect(merged.session, result.session);

    final split = await scanner.splitSymbol(result.session, 0);
    expect(split.symbols, 2);
  });
}

void _relationContractTests() {
  test('parses relation markers and per-line glyphs', () {
    final result = ScanResult.fromJson({
      'expression': 'x^2+1',
      'latex': r'x^{2}+1',
      'kind': 'expression',
      'relations': [
        {'symbol': 3, 'kind': 'script', 'value': 'superscript'},
        {'symbol': 7, 'kind': 'bar', 'value': 'minus'},
      ],
      'lineDetails': [
        {
          'line': 0,
          'infinit': 'x^2+1',
          'latex': r'x^{2}+1',
          'parse_valid': true,
          'confidence': 0.9,
          'symbols': [
            {
              'id': 0,
              'glyph': 'x',
              'bbox': [1, 2, 10, 12],
              'confidence': 0.9,
            },
          ],
        },
        {
          'line': 1,
          'infinit': 'y=0',
          'latex': 'y=0',
          'parse_valid': false,
          'confidence': 0.4,
          'symbols': [],
        },
      ],
    });
    expect(result.relations, hasLength(2));
    expect(result.relations.first.kind, 'script');
    expect(result.relations.last.value, 'minus');
    expect(result.lineDetails, hasLength(2));
    expect(result.lineDetails.first.glyphs, hasLength(1));
    expect(result.lineDetails.first.glyphs.first.label, 'x');
    expect(result.lineDetails.last.parseValid, isFalse);
  });
}
