import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/models.dart';
import 'package:infinit/src/native_engine.dart';

/// M6 guard: the calculus-application solvers must return catalogue keys, not
/// user-facing text, for every step, check and warning they emit.
void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  final engine = NativeEngine.instance;

  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  Future<void> expectKeyed(SolveResult result) async {
    expect(result.ok, isTrue, reason: result.errorCode);
    expect(result.steps, isNotEmpty);
    for (final step in result.steps) {
      expect(step.titleKey, isNotEmpty);
      expect(
        I18n.instance.hasKey(step.titleKey),
        isTrue,
        reason: 'missing step title key ${step.titleKey}',
      );
      if (step.noteKey.isNotEmpty) {
        expect(
          I18n.instance.hasKey(step.noteKey),
          isTrue,
          reason: 'missing step note key ${step.noteKey}',
        );
      }
    }
    for (final check in result.checks) {
      for (final key in [check.nameKey, check.detailKey]) {
        if (key.isEmpty) continue;
        expect(
          I18n.instance.hasKey(key),
          isTrue,
          reason: 'missing check key $key',
        );
      }
    }
    for (final warning in result.warnings) {
      expect(
        I18n.instance.hasKey(warning),
        isTrue,
        reason: 'missing warning key $warning',
      );
    }
  }

  test('optimisation ships catalogue keys', () async {
    await expectKeyed(
      await engine.optimizeAsync('x^2 - 4*x', xmin: 0, xmax: 5),
    );
  });

  test('tangent and normal ship catalogue keys', () async {
    await expectKeyed(await engine.tangentAsync('x^2', at: '1'));
  });

  test('behaviour analysis ships catalogue keys', () async {
    await expectKeyed(
      await engine.behaviourAsync('x^3 - 3*x', xmin: -3, xmax: 3),
    );
  });

  test("L'Hopital ships catalogue keys", () async {
    await expectKeyed(await engine.lhopitalAsync('sin(x)', 'x', point: '0'));
  });

  for (final topic in ['euler', 'heun', 'rk4', 'fourier_series']) {
    test('$topic ships catalogue keys', () async {
      final result = await engine.topicAsync(topic);
      await expectKeyed(result);
      expect(result.typeId, topic);
    });
  }
}
