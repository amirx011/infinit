// Data model mirroring the JSON contract of the native engine.
import 'dart:convert';

class ExView {
  const ExView({
    required this.plain,
    required this.pretty,
    required this.latex,
  });

  final String plain;
  final String pretty;
  final String latex;

  static ExView? fromJson(dynamic value) {
    if (value is! Map) return null;
    return ExView(
      plain: (value['plain'] ?? '').toString(),
      pretty: (value['pretty'] ?? '').toString(),
      latex: (value['latex'] ?? '').toString(),
    );
  }
}

class StepView {
  const StepView({required this.title, required this.note, this.math});

  final String title;
  final String note;
  final ExView? math;

  static StepView fromJson(Map value) => StepView(
    title: (value['title'] ?? '').toString(),
    note: (value['note'] ?? '').toString(),
    math: ExView.fromJson(value['math']),
  );
}

class CheckView {
  const CheckView({required this.name, required this.ok, required this.detail});

  final String name;
  final bool ok;
  final String detail;

  static CheckView fromJson(Map value) => CheckView(
    name: (value['name'] ?? '').toString(),
    ok: value['ok'] == true,
    detail: (value['detail'] ?? '').toString(),
  );
}

/// A point reported by the behaviour or optimisation analysis.
class PointView {
  const PointView({this.x, this.value, required this.kind});

  final ExView? x;
  final ExView? value;

  /// maximum / minimum / inflection / stationary / endpoint / undecided
  final String kind;

  static PointView? fromJson(dynamic v) {
    if (v is! Map) return null;
    return PointView(
      x: ExView.fromJson(v['x']),
      value: ExView.fromJson(v['value']),
      kind: (v['kind'] ?? '').toString(),
    );
  }
}

/// One monotonicity interval of f, with the sign of f' on it.
class MonotonePieceView {
  const MonotonePieceView({
    required this.lo,
    required this.hi,
    required this.sign,
    required this.trend,
  });

  final double lo;
  final double hi;
  final int sign;
  final String trend;

  static MonotonePieceView? fromJson(dynamic v) {
    if (v is! Map) return null;
    return MonotonePieceView(
      lo: (v['lo'] as num?)?.toDouble() ?? 0,
      hi: (v['hi'] as num?)?.toDouble() ?? 0,
      sign: (v['sign'] as num?)?.toInt() ?? 0,
      trend: (v['trend'] ?? '').toString(),
    );
  }
}

class SolveResult {
  const SolveResult({
    required this.ok,
    required this.mode,
    required this.typeId,
    required this.typeFa,
    required this.typeEn,
    required this.error,
    required this.engine,
    required this.method,
    required this.variable,
    required this.order,
    required this.closedForm,
    required this.timeMs,
    required this.normalized,
    required this.solution,
    required this.isExplicit,
    required this.explicitY,
    required this.paramX,
    required this.paramY,
    required this.paramName,
    required this.singular,
    required this.wronskian,
    required this.independent,
    required this.envelope,
    required this.orthogonal,
    required this.orthogonalExplicit,
    required this.functions,
    required this.family,
    required this.steps,
    required this.checks,
    required this.warnings,
    required this.assumptions,
    required this.dimension,
    required this.definite,
    required this.implicit,
    required this.improper,
    required this.divergent,
    required this.singularities,
    required this.verified,
    required this.hasNumeric,
    required this.numericValue,
    required this.vars,
    required this.lower,
    required this.upper,
    required this.point,
    required this.side,
    required this.limitExists,
    required this.limitInfinite,
    required this.limitSign,
    required this.leftExists,
    required this.rightExists,
    required this.leftInfinite,
    required this.rightInfinite,
    required this.leftSign,
    required this.rightSign,
    required this.leftValue,
    required this.rightValue,
    required this.valueDefined,
    required this.valueAt,
    required this.continuous,
    required this.discontinuity,
    required this.laws,
    this.derivative,
    this.secondDerivative,
    this.value,
    this.slope,
    this.tangent,
    this.normal,
    this.tangentSlope,
    this.normalSlope,
    this.valueA,
    this.valueB,
    this.maximum,
    this.minimum,
    this.maximumAt,
    this.minimumAt,
    this.functionF,
    this.functionG,
    this.critical = const [],
    this.candidates = const [],
    this.inflections = const [],
    this.pieces = const [],
    this.points = const [],
    this.pointValues = const [],
    this.functionValueA,
    this.functionValueB,
    this.hypothesesContinuous = false,
    this.hypothesesDifferentiable = false,
    this.rolleApplies = false,
  });

  final bool ok;
  final String mode;
  final String typeId;
  final String typeFa;
  final String typeEn;
  final String error;
  final String engine;
  final String method;
  final String variable;
  final int order;
  final bool closedForm;
  final int timeMs;
  final ExView? normalized;
  final ExView? solution;
  final bool isExplicit;
  final ExView? explicitY;
  final ExView? paramX;
  final ExView? paramY;
  final String paramName;
  final List<ExView> singular;
  final ExView? wronskian;
  final bool independent;
  final List<ExView> envelope;
  final ExView? orthogonal;
  final bool orthogonalExplicit;
  final List<String> functions;
  final String family;
  final List<StepView> steps;
  final List<CheckView> checks;
  final List<String> warnings;
  final List<String> assumptions;

  /// 1, 2 or 3 for single / double / triple integrals.
  final int dimension;
  final bool definite;
  final bool implicit;
  final bool improper;
  final bool divergent;
  final List<String> singularities;
  final bool verified;
  final bool hasNumeric;
  final double numericValue;
  final List<String> vars;
  final List<String> lower;
  final List<String> upper;

  /// Limit / continuity payload.
  final String point;
  final String side;
  final bool limitExists;
  final bool limitInfinite;
  final int limitSign;
  final bool leftExists;
  final bool rightExists;
  final bool leftInfinite;
  final bool rightInfinite;
  final int leftSign;
  final int rightSign;
  final ExView? leftValue;
  final ExView? rightValue;
  final bool valueDefined;
  final ExView? valueAt;
  final bool continuous;
  final String discontinuity;
  final List<String> laws;

  /// Applications of the derivative. The behaviour, tangent, optimisation,
  /// related-rates and mean-value modes fill these in.
  final ExView? derivative;
  final ExView? secondDerivative;
  final ExView? value;
  final ExView? slope;
  final ExView? tangent;
  final ExView? normal;
  final ExView? tangentSlope;
  final ExView? normalSlope;
  final ExView? valueA;
  final ExView? valueB;
  final ExView? maximum;
  final ExView? minimum;
  final ExView? maximumAt;
  final ExView? minimumAt;
  final ExView? functionF;
  final ExView? functionG;
  final List<PointView> critical;
  final List<PointView> candidates;
  final List<PointView> inflections;
  final List<MonotonePieceView> pieces;
  final List<ExView> points;

  /// f(c) for each entry of [points], in the same order.
  final List<ExView> pointValues;

  /// Cauchy: the values g(a) and g(b).
  final ExView? functionValueA;
  final ExView? functionValueB;
  final bool hypothesesContinuous;
  final bool hypothesesDifferentiable;
  final bool rolleApplies;

  static List<ExView> _exList(dynamic value) {
    if (value is! List) return const [];
    return value
        .map(ExView.fromJson)
        .whereType<ExView>()
        .toList(growable: false);
  }

  static List<String> _strList(dynamic value) {
    if (value is! List) return const [];
    return value.map((e) => e.toString()).toList(growable: false);
  }

  factory SolveResult.fromJson(Map<String, dynamic> j) => SolveResult(
    ok: j['ok'] == true,
    mode: (j['mode'] ?? '').toString(),
    typeId: (j['type_id'] ?? '').toString(),
    typeFa: (j['type_fa'] ?? '').toString(),
    typeEn: (j['type_en'] ?? '').toString(),
    error: (j['error'] ?? '').toString(),
    engine: (j['engine'] ?? 'native').toString(),
    method: (j['method'] ?? '').toString(),
    variable: (j['variable'] ?? 'x').toString(),
    order: (j['order'] as num?)?.toInt() ?? 0,
    closedForm: j['closed_form'] != false,
    timeMs: (j['time_ms'] as num?)?.toInt() ?? 0,
    normalized: ExView.fromJson(j['normalized']),
    solution: ExView.fromJson(j['solution']),
    isExplicit: j['is_explicit'] == true,
    explicitY: ExView.fromJson(j['explicit_y']),
    paramX: ExView.fromJson(j['param_x']),
    paramY: ExView.fromJson(j['param_y']),
    paramName: (j['param_name'] ?? 'p').toString(),
    singular: _exList(j['singular']),
    wronskian: ExView.fromJson(j['wronskian']),
    independent: j['independent'] == true,
    envelope: _exList(j['envelope']),
    orthogonal: ExView.fromJson(j['orthogonal']),
    orthogonalExplicit: j['orthogonal_explicit'] == true,
    functions: _strList(j['functions']),
    family: (j['family'] ?? '').toString(),
    steps: (j['steps'] is List)
        ? (j['steps'] as List).whereType<Map>().map(StepView.fromJson).toList()
        : const [],
    checks: (j['checks'] is List)
        ? (j['checks'] as List)
              .whereType<Map>()
              .map(CheckView.fromJson)
              .toList()
        : const [],
    warnings: _strList(j['warnings']),
    assumptions: _strList(j['assumptions']),
    dimension: (j['dimension'] as num?)?.toInt() ?? 1,
    definite: j['definite'] == true,
    implicit: j['implicit'] == true,
    improper: j['improper'] == true,
    divergent: j['divergent'] == true,
    singularities: _strList(j['singularities']),
    verified: j['verified'] != false,
    hasNumeric: j['has_numeric'] == true,
    numericValue: (j['numeric_value'] as num?)?.toDouble() ?? 0,
    vars: _strList(j['vars']),
    lower: _strList(j['lower']),
    upper: _strList(j['upper']),
    point: (j['point'] ?? '').toString(),
    side: (j['side'] ?? 'both').toString(),
    limitExists: j['limit_exists'] == true,
    limitInfinite: j['limit_infinite'] == true,
    limitSign: (j['limit_sign'] as num?)?.toInt() ?? 1,
    leftExists: j['left_exists'] == true,
    rightExists: j['right_exists'] == true,
    leftInfinite: j['left_infinite'] == true,
    rightInfinite: j['right_infinite'] == true,
    leftSign: (j['left_sign'] as num?)?.toInt() ?? 1,
    rightSign: (j['right_sign'] as num?)?.toInt() ?? 1,
    leftValue: ExView.fromJson(j['left_value']),
    rightValue: ExView.fromJson(j['right_value']),
    valueDefined: j['value_defined'] == true,
    valueAt: ExView.fromJson(j['value_at']),
    continuous: j['continuous'] == true,
    discontinuity: (j['discontinuity'] ?? '').toString(),
    laws: _strList(j['laws']),
    derivative: ExView.fromJson(j['derivative']),
    secondDerivative: ExView.fromJson(j['second_derivative']),
    value: ExView.fromJson(j['value']),
    slope: ExView.fromJson(j['slope']),
    tangent: ExView.fromJson(j['tangent']),
    normal: ExView.fromJson(j['normal']),
    tangentSlope: ExView.fromJson(j['tangent_slope']),
    normalSlope: ExView.fromJson(j['normal_slope']),
    valueA: ExView.fromJson(j['value_a']),
    valueB: ExView.fromJson(j['value_b']),
    maximum: ExView.fromJson(j['maximum']),
    minimum: ExView.fromJson(j['minimum']),
    maximumAt: ExView.fromJson(j['maximum_at']),
    minimumAt: ExView.fromJson(j['minimum_at']),
    functionF: ExView.fromJson(j['function_f']),
    functionG: ExView.fromJson(j['function_g']),
    critical: _pointList(j['critical']),
    candidates: _pointList(j['candidates']),
    inflections: _pointList(j['inflections']),
    pieces: j['pieces'] is List
        ? (j['pieces'] as List)
              .map(MonotonePieceView.fromJson)
              .whereType<MonotonePieceView>()
              .toList(growable: false)
        : const [],
    points: _exList(j['points']),
    pointValues: _exList(j['point_values']),
    functionValueA: ExView.fromJson(j['function_value_a']),
    functionValueB: ExView.fromJson(j['function_value_b']),
    hypothesesContinuous: j['hypotheses_continuous'] == true,
    hypothesesDifferentiable: j['hypotheses_differentiable'] == true,
    rolleApplies: j['rolle_applies'] == true,
  );

  static List<PointView> _pointList(dynamic value) {
    if (value is! List) return const [];
    return value
        .map(PointView.fromJson)
        .whereType<PointView>()
        .toList(growable: false);
  }

  factory SolveResult.fromRaw(String raw) {
    final dynamic decoded = jsonDecode(raw);
    if (decoded is! Map<String, dynamic>) {
      throw const FormatException('پاسخ موتور محاسباتی معتبر نیست.');
    }
    return SolveResult.fromJson(decoded);
  }

  /// The expression that should be shown as the main answer.
  ExView? get primary {
    if (mode == 'wronskian') return wronskian;
    if (mode == 'envelope') return envelope.isEmpty ? null : envelope.first;
    if (isExplicit && explicitY != null) return explicitY;
    return solution;
  }

  bool get parametricAvailable => paramX != null || paramY != null;
}

/// A closed, open or infinite interval reported by the native domain scan.
class IntervalView {
  const IntervalView({
    required this.lo,
    required this.hi,
    required this.loInfinite,
    required this.hiInfinite,
    required this.loOpen,
    required this.hiOpen,
  });

  final double lo;
  final double hi;
  final bool loInfinite;
  final bool hiInfinite;
  final bool loOpen;
  final bool hiOpen;

  bool contains(double v) {
    final above = loInfinite || (loOpen ? v > lo : v >= lo);
    final below = hiInfinite || (hiOpen ? v < hi : v <= hi);
    return above && below;
  }

  static IntervalView? fromJson(dynamic value) {
    if (value is! Map) return null;
    double number(dynamic v, double fallback) =>
        v is num ? v.toDouble() : fallback;
    return IntervalView(
      lo: number(value['lo'], double.negativeInfinity),
      hi: number(value['hi'], double.infinity),
      loInfinite: value['lo_inf'] == true,
      hiInfinite: value['hi_inf'] == true,
      loOpen: value['lo_open'] != false,
      hiOpen: value['hi_open'] != false,
    );
  }
}

class SampleResult {
  const SampleResult({
    required this.ok,
    required this.error,
    required this.xs,
    required this.ys,
  });

  final bool ok;
  final String error;
  final List<double> xs;
  final List<double> ys;

  factory SampleResult.fromJson(Map<String, dynamic> j) => SampleResult(
    ok: j['ok'] == true,
    error: (j['error'] ?? '').toString(),
    xs: doubleList(j['xs']),
    ys: doubleList(j['ys']),
  );
}

class DomainResult {
  const DomainResult({
    required this.ok,
    required this.error,
    required this.kind,
    required this.domain,
    required this.range,
    required this.holes,
    required this.asymptotes,
    required this.rangeHoles,
    required this.xs,
    required this.ys,
    required this.rangeUnboundedBelow,
    required this.rangeUnboundedAbove,
    required this.splitAtZero,
  });

  final bool ok;
  final String error;
  final String kind;
  final List<IntervalView> domain;
  final List<IntervalView> range;
  final List<double> holes;
  final List<double> asymptotes;
  final List<double> rangeHoles;
  final List<double> xs;
  final List<double> ys;
  final bool rangeUnboundedBelow;
  final bool rangeUnboundedAbove;
  final bool splitAtZero;

  factory DomainResult.fromJson(Map<String, dynamic> j) => DomainResult(
    ok: j['ok'] == true,
    error: (j['error'] ?? '').toString(),
    kind: (j['kind'] ?? 'polynomial').toString(),
    domain: intervalList(j['domain']),
    range: intervalList(j['range']),
    holes: doubleList(j['holes']),
    asymptotes: doubleList(j['asymptotes']),
    rangeHoles: doubleList(j['range_holes']),
    xs: doubleList(j['xs']),
    ys: doubleList(j['ys']),
    rangeUnboundedBelow: j['range_unbounded_below'] == true,
    rangeUnboundedAbove: j['range_unbounded_above'] == true,
    splitAtZero: j['split_at_zero'] == true,
  );
}

class FieldCurveView {
  const FieldCurveView({
    required this.icX,
    required this.icY,
    required this.xs,
    required this.ys,
  });

  final double icX;
  final double icY;
  final List<double> xs;
  final List<double> ys;

  static FieldCurveView? fromJson(dynamic value) {
    if (value is! Map) return null;
    return FieldCurveView(
      icX: (value['ic_x'] as num?)?.toDouble() ?? 0,
      icY: (value['ic_y'] as num?)?.toDouble() ?? 0,
      xs: doubleList(value['xs']),
      ys: doubleList(value['ys']),
    );
  }
}

class FieldResult {
  const FieldResult({
    required this.ok,
    required this.error,
    required this.rhs,
    required this.xmin,
    required this.xmax,
    required this.ymin,
    required this.ymax,
    required this.cols,
    required this.rows,
    required this.slopes,
    required this.curves,
  });

  final bool ok;
  final String error;
  final String rhs;
  final double xmin;
  final double xmax;
  final double ymin;
  final double ymax;
  final int cols;
  final int rows;

  /// Row-major slopes; NaN marks a singular point.
  final List<List<double>> slopes;
  final List<FieldCurveView> curves;

  factory FieldResult.fromJson(Map<String, dynamic> j) => FieldResult(
    ok: j['ok'] == true,
    error: (j['error'] ?? '').toString(),
    rhs: (j['rhs'] ?? '').toString(),
    xmin: (j['xmin'] as num?)?.toDouble() ?? -6,
    xmax: (j['xmax'] as num?)?.toDouble() ?? 6,
    ymin: (j['ymin'] as num?)?.toDouble() ?? -6,
    ymax: (j['ymax'] as num?)?.toDouble() ?? 6,
    cols: (j['cols'] as num?)?.toInt() ?? 0,
    rows: (j['rows'] as num?)?.toInt() ?? 0,
    slopes: j['slopes'] is List
        ? (j['slopes'] as List)
              .whereType<List>()
              .map((row) => doubleList(row))
              .toList(growable: false)
        : const [],
    curves: j['curves'] is List
        ? (j['curves'] as List)
              .map(FieldCurveView.fromJson)
              .whereType<FieldCurveView>()
              .toList(growable: false)
        : const [],
  );
}

/// JSON numbers may be `null` (native NaN), so this keeps the index alignment
/// of the sample arrays and stores NaN for missing points.
List<double> doubleList(dynamic value) {
  if (value is! List) return const [];
  return value
      .map((e) => e is num ? e.toDouble() : double.nan)
      .toList(growable: false);
}

List<IntervalView> intervalList(dynamic value) {
  if (value is! List) return const [];
  return value
      .map(IntervalView.fromJson)
      .whereType<IntervalView>()
      .toList(growable: false);
}
