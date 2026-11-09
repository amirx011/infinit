import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/native_engine.dart';

void main() {
  final engine = NativeEngine.instance;

  test('native engine version is available', () {
    expect(engine.version, isNotEmpty);
    expect(engine.version, isNot('?'));
  });

  test('solves a first-order linear equation', () {
    final result = engine.solveOde("y' = y");
    expect(result.ok, isTrue);
    expect(result.typeId, 'linear');
    expect(result.explicitY?.pretty, contains('exp'));
    expect(result.steps, isNotEmpty);
    expect(result.checks.single.ok, isTrue);
  });

  test('solves a constant-coefficient second-order equation', () {
    final result = engine.solveOde("y'' - 3y' + 2y = 0");
    expect(result.ok, isTrue);
    expect(result.order, 2);
    expect(result.explicitY?.pretty, contains('C2'));
  });

  test('wronskian detects dependence', () {
    final independent = engine.wronskian(['sin(x)', 'cos(x)']);
    expect(independent.ok, isTrue);
    expect(independent.wronskian?.plain, '-1');
    expect(independent.independent, isTrue);

    final dependent = engine.wronskian(['x', '2x']);
    expect(dependent.wronskian?.plain, '0');
    expect(dependent.independent, isFalse);
  });

  test('envelope of a line family and a family without envelope', () {
    final line = engine.envelope('y = c*x + c^2');
    expect(line.ok, isTrue);
    expect(line.envelope, isNotEmpty);

    final circles = engine.envelope('x^2 + y^2 = c^2');
    expect(circles.ok, isTrue);
    expect(circles.envelope, isEmpty);
    expect(circles.warnings, isNotEmpty);
  });

  test('integrates by substitution and by parts automatically', () {
    final sub = engine.integral('2*x*cos(x^2)');
    expect(sub.ok, isTrue);
    expect(sub.method, 'substitution');
    expect(sub.solution?.plain, contains('sin'));

    final parts = engine.integral('x*exp(x)');
    expect(parts.ok, isTrue);
    expect(parts.method, 'parts');
    expect(parts.solution?.plain, contains('exp'));
  });

  test('computes first and second derivatives', () {
    final first = engine.derivative('x^3*sin(x)', order: 1);
    expect(first.ok, isTrue);
    expect(first.order, 1);
    expect(first.solution?.plain, contains('sin'));

    final second = engine.derivative('x^3', order: 2);
    expect(second.ok, isTrue);
    expect(second.order, 2);
    expect(second.solution?.plain, '6*x');
  });

  test('orthogonal trajectories are returned separately', () {
    final result = engine.envelope('x^2 + y^2 = c^2', orthogonal: true);
    expect(result.ok, isTrue);
    expect(result.orthogonal, isNotNull);
    expect(result.orthogonal!.pretty, contains('x'));
  });

  test('derivative orders are not clamped', () {
    final fifth = engine.derivative('x^6', order: 5);
    expect(fifth.ok, isTrue);
    expect(fifth.order, 5);
    expect(fifth.solution?.plain, contains('720'));

    final tenth = engine.derivative('x^10', order: 10);
    expect(tenth.ok, isTrue);
    expect(tenth.order, 10);
    expect(tenth.solution?.plain, '3628800');

    final rejected = engine.derivative('x^2', order: 11);
    expect(rejected.ok, isFalse);
    expect(rejected.error, isNotEmpty);
  });

  test('improper and divergent integrals are reported honestly', () {
    final divergent = engine.integral(
      '1/x^2',
      definite: true,
      lower: ['-1'],
      upper: ['1'],
    );
    expect(divergent.ok, isTrue);
    expect(divergent.divergent, isTrue);
    expect(divergent.solution, isNull);

    final convergent = engine.integral(
      '1/sqrt(x)',
      definite: true,
      lower: ['0'],
      upper: ['1'],
    );
    expect(convergent.ok, isTrue);
    expect(convergent.improper, isTrue);
    expect(convergent.divergent, isFalse);
    expect(convergent.solution?.plain, '2');
  });

  test('async solver runs on a background isolate', () async {
    final result = await engine.derivativeAsync('x^4', order: 4);
    expect(result.ok, isTrue);
    expect(result.order, 4);
    expect(result.solution?.plain, '24');

    final ode = await engine.solveOdeAsync("y' = y");
    expect(ode.ok, isTrue);
    expect(ode.explicitY?.pretty, contains('exp'));
  });

  test('samples a function and marks undefined points', () async {
    final sample = await engine.sampleAsync(
      '1/x',
      xmin: -2,
      xmax: 2,
      samples: 41,
    );
    expect(sample.ok, isTrue);
    expect(sample.xs.length, 41);
    expect(sample.ys.length, 41);
    expect(sample.ys.any((v) => v.isNaN), isTrue);
  });

  test('domain and range scan reports intervals and asymptotes', () async {
    final domain = await engine.domainAsync('1/(x^2-1)');
    expect(domain.ok, isTrue);
    expect(domain.domain.length, 3);
    expect(domain.asymptotes.length, 2);
    expect(domain.range.any((i) => i.loInfinite), isTrue);
    expect(domain.range.any((i) => i.hiInfinite), isTrue);
  });

  test('direction field returns slopes and RK4 curves', () async {
    final field = await engine.fieldAsync(
      "y' = y",
      cols: 9,
      rows: 9,
      curves: 3,
    );
    expect(field.ok, isTrue);
    expect(field.rhs, contains('y'));
    expect(field.curves, isNotEmpty);
    expect(
      field.slopes.expand((row) => row).where((v) => v.isFinite),
      isNotEmpty,
    );
  });

  test('topic workbench solves growth, systems and transforms', () {
    final growth = engine.solve({
      'mode': 'topic',
      'topic': 'growth_decay',
      'k': 0.2,
      'y0': 100,
      't': 5,
    });
    expect(growth.ok, isTrue);
    expect(growth.solution?.pretty, contains('exp'));
    expect(growth.steps, isNotEmpty);

    final system = engine.solve({
      'mode': 'topic',
      'topic': 'linear_system',
      'a': 0,
      'b': 1,
      'c': -1,
      'd': 0,
    });
    expect(system.ok, isTrue);
    expect(system.solution?.pretty, contains('cos'));
    expect(system.checks, isNotEmpty);

    final laplace = engine.solve({
      'mode': 'topic',
      'topic': 'laplace',
      'f': 'exp(2*t)*sin(3*t)',
    });
    expect(laplace.ok, isTrue);
    expect(laplace.solution?.pretty, contains('s'));
  });

  test('C++ fallback handles linear and stiff ODE topics', () {
    final linear = engine.solve({
      'mode': 'topic',
      'topic': 'ode_linear',
      'coeffs': '1, 0, 1',
      'ics': '1, 0',
    });
    expect(linear.ok, isTrue);
    expect(linear.solution?.pretty, contains('cos'));
    expect(linear.steps.length, greaterThanOrEqualTo(4));

    final stiff = engine.solve({
      'mode': 'topic',
      'topic': 'ode_stiff',
      'f': '-50*(y - cos(x))',
      'x0': 0,
      'y0': 0,
      'xend': 1,
      'h': 0.01,
    });
    expect(stiff.ok, isTrue);
    expect(stiff.hasNumeric, isTrue);
    expect(stiff.numericValue, closeTo(0.54, 0.1));
  });

  test(
    'async topic routing returns the same answer with or without Julia',
    () async {
      final result = await engine.topicAsync(
        'ode_linear',
        params: {'coeffs': '1, 0, 1', 'ics': '1, 0'},
      );
      expect(result.ok, isTrue);
      expect(result.solution?.pretty, contains('cos'));
      expect(result.steps, isNotEmpty);
    },
    timeout: const Timeout(Duration(minutes: 2)),
  );

  test('Heun and RK4 both advance the same initial-value problem', () {
    Map<String, dynamic> request(String topic) => {
      'mode': 'topic',
      'topic': topic,
      'f': 'x + y',
      'x0': 0,
      'y0': 1,
      'h': 0.1,
      'n': 10,
    };

    final heun = engine.solve(request('heun'));
    final rk4 = engine.solve(request('rk4'));
    expect(heun.ok, isTrue);
    expect(rk4.ok, isTrue);
    expect(heun.hasNumeric, isTrue);
    expect(rk4.hasNumeric, isTrue);
    // RK4 is more accurate than Heun on the same step count.
    expect(
      (rk4.numericValue - 3.43656).abs(),
      lessThan((heun.numericValue - 3.43656).abs()),
    );
  });

  test('parser reports when input was reinterpreted', () {
    final interpreted = engine.solve({
      'mode': 'parse',
      'input': 'e^2t',
    });
    expect(interpreted.ok, isTrue);
    expect(interpreted.interpretedAs, contains('exp'));

    final canonical = engine.solve({
      'mode': 'parse',
      'input': 'x^2 + 3*exp(3*x)',
    });
    expect(canonical.ok, isTrue);
    expect(canonical.interpretedAs, isEmpty);

    final derivative = engine.derivative('csinx');
    expect(derivative.ok, isTrue);
    expect(derivative.interpretedAs, contains('sin(x)'));
  });

  test('unicode input survives the Dart bridge intact', () {
    final superscript = engine.solve({
      'mode': 'topic',
      'topic': 'rk4',
      'f': 'x⁶y + sinx × e^y',
      'x0': 0,
      'y0': 1,
      'h': 0.1,
      'n': 1,
      'lang': 'fa',
    });
    expect(superscript.ok, isTrue);
    expect(superscript.hasNumeric, isTrue);

    final persian = engine.solve({
      'mode': 'parse',
      'input': 'تابع',
      'lang': 'fa',
    });
    expect(persian.ok, isFalse);
    expect(persian.errorCode, 'bad_characters');
    expect(persian.error, isEmpty);
    expect(persian.errorDetail, isNotEmpty);
  });

  test('singular start point returns its location', () {
    final result = engine.solve({
      'mode': 'topic',
      'topic': 'heun',
      'f': 'x + csc(x)^2',
      'x0': 0,
      'y0': 1,
      'h': 0.1,
      'n': 10,
    });
    expect(result.ok, isFalse);
    expect(result.errorCode, 'singular');
    expect(result.hasErrorPoint, isTrue);
    expect(result.errorPoint, closeTo(0, 1e-9));
  });

  test('an unknown function is named in the log detail', () {
    final result = engine.solve({
      'mode': 'taylor',
      'input': 'foo(x)',
      'order': 2,
    });
    expect(result.ok, isFalse);
    expect(result.errorCode, 'unknown_function');
    expect(result.error, isEmpty);
    expect(result.errorDetail, contains('foo'));
  });

  test('application topics keep the correct sign and readable constants', () {
    final falling = engine.solve({
      'mode': 'topic',
      'topic': 'falling_body',
      'm': 1,
      'k': 0.5,
      'g': 9.81,
      'v0': 0,
      't': 5,
    });
    expect(falling.ok, isTrue);
    // v(t) = mg/k - (mg/k)e^(-kt/m).  The exponential term must be negative.
    expect(falling.solution?.plain, contains('-981/50*exp'));
    expect(falling.numericValue, closeTo(18.0095, 0.001));

    final pde = engine.solve({
      'mode': 'topic',
      'topic': 'laplace_pde',
      'a': 3.141592653589793,
      'b': 3.141592653589793,
      'n': 3,
      'f': 'sin(pi*x/a)',
    });
    expect(pde.ok, isTrue);
    // sinh(pi) is shown as a short decimal, never as a huge rational.
    expect(pde.solution?.plain, isNot(contains('80143857')));
    expect(pde.solution?.plain, contains('sinh'));
  });
}
