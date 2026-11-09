import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/models.dart';
import 'package:infinit/src/native_engine.dart';

/// Guard: every key the exact-ODE engine emits must resolve in the catalogue.
void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  final engine = NativeEngine.instance;

  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  void expectResolves(String key) {
    if (key.isEmpty) return;
    expect(I18n.instance.hasKey(key), isTrue, reason: 'missing $key');
  }

  void expectResult(SolveResult result) {
    for (final step in result.steps) {
      expectResolves(step.titleKey);
      expectResolves(step.noteKey);
    }
    for (final check in result.checks) {
      expectResolves(check.nameKey);
      expectResolves(check.detailKey);
    }
    for (final warning in result.warnings) {
      expectResolves(warning);
    }
    for (final assumption in result.assumptions) {
      expectResolves(assumption);
    }
    if (result.errorCode.isNotEmpty) {
      expectResolves('error.${result.errorCode}');
    }
  }

  test('first-order families resolve their keys', () {
    for (final input in [
      "y' = y",
      "y' = y^2",
      "y' = (x^2+y^2)/(x*y)",
      "(2*x+y) + (x+2*y)*y' = 0",
      "y' = x*y + y^2",
      "y = x*y' + (y')^2",
      "y'' - 3*y' + 2*y = 0",
      "y'' = 6*x",
      "y'' + y = 0",
      "y'' - y = 0",
    ]) {
      final result = engine.solveOde(input);
      expect(result.ok, isTrue, reason: input);
      expectResult(result);
    }
  });

  test('wronskian, envelope and trajectories resolve their keys', () {
    expectResult(engine.wronskian(['sin(x)', 'cos(x)']));
    expectResult(engine.envelope('y = c*x + c^2'));
    expectResult(engine.envelope('x^2 + y^2 = c^2'));
    expectResult(engine.envelope('x^2 + y^2 = c^2', orthogonal: true));
    expectResult(engine.oblique('y = c*x', angle: 45));
  });

  test('limits and continuity resolve their keys', () {
    expectResult(engine.limit('sin(x)/x', point: '0'));
    expectResult(engine.limit('1/x', point: '0'));
    expectResult(engine.limit('tan(x)', point: '0'));
    expectResult(engine.continuity('sin(x)/x', point: '0'));
    expectResult(engine.continuity('1/x', point: '0'));
    expectResult(engine.continuity('x^2', point: '1'));
  });

  test('failure codes resolve their sentences', () {
    for (final input in ["y' = unknown_fn(x)", "1/", "sin("]) {
      final result = engine.solveOde(input);
      expect(result.ok, isFalse, reason: input);
      expectResult(result);
    }
  });
}
