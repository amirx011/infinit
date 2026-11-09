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
  const StepView({
    required this.title,
    required this.note,
    this.math,
    this.titleKey = '',
    this.noteKey = '',
    this.params = const {},
    this.noteMath,
  });

  final String title;
  final String note;
  final ExView? math;

  /// Catalogue key for the step title; empty for unmigrated sections.
  final String titleKey;

  /// Catalogue key for the note sentence; empty when the note is math or raw.
  final String noteKey;

  /// Plain numeric params for the two keys above.
  final Map<String, Object?> params;

  /// Formula for the note, rendered as its own left-to-right widget.
  final ExView? noteMath;

  static StepView fromJson(Map value) => StepView(
    title: (value['title'] ?? '').toString(),
    note: (value['note'] ?? '').toString(),
    math: ExView.fromJson(value['math']),
    titleKey: (value['title_key'] ?? '').toString(),
    noteKey: (value['note_key'] ?? '').toString(),
    params: value['params'] is Map
        ? {
            for (final entry in (value['params'] as Map).entries)
              entry.key.toString(): entry.value,
          }
        : const {},
    noteMath: ExView.fromJson(value['note_math']),
  );
}

class CheckView {
  const CheckView({
    required this.name,
    required this.ok,
    required this.detail,
    this.nameKey = '',
    this.detailKey = '',
    this.params = const {},
    this.detailMath,
  });

  final String name;
  final bool ok;
  final String detail;

  /// Catalogue key for the check name; empty for unmigrated sections.
  final String nameKey;

  /// Catalogue key for the detail sentence; empty when the detail is math.
  final String detailKey;

  /// Plain numeric params for the two keys above.
  final Map<String, Object?> params;

  /// Formula rendered as its own left-to-right widget next to the sentences.
  final ExView? detailMath;

  static CheckView fromJson(Map value) {
    final math = value['detail_math'];
    return CheckView(
      name: (value['name'] ?? '').toString(),
      ok: value['ok'] == true,
      detail: (value['detail'] ?? '').toString(),
      nameKey: (value['name_key'] ?? '').toString(),
      detailKey: (value['detail_key'] ?? '').toString(),
      params: value['params'] is Map
          ? {
              for (final entry in (value['params'] as Map).entries)
                entry.key.toString(): entry.value,
            }
          : const {},
      detailMath: math is String && math.isNotEmpty
          ? ExView(plain: math, pretty: math, latex: math)
          : ExView.fromJson(math),
    );
  }
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

/// One partial derivative (first or second order) with its display label.
class PartialView {
  const PartialView({required this.label, this.value});

  final String label;
  final ExView? value;

  static PartialView? fromJson(dynamic v) {
    if (v is! Map) return null;
    return PartialView(
      label: (v['label'] ?? '').toString(),
      value: ExView.fromJson(v['value']),
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
    required this.orthogonalSlope,
    required this.trajectory,
    required this.trajectoryExplicit,
    required this.trajectorySlope,
    required this.trajectoryAngle,
    required this.trajectoryMode,
    required this.familyBranches,
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
    this.asymptotes = const [],
    this.xIntercepts = const [],
    this.yIntercept,
    this.hasYIntercept = false,
    this.sketch = const [],
    this.polynomial,
    this.remainder,
    this.coefficients = const [],
    this.integrand,
    this.application = '',
    this.partials = const [],
    this.gradient = const [],
    this.laplacian,
    this.directional,
    this.functionValue,
    this.pointLabel = '',
    this.seriesVerdict = '',
    this.seriesTest = '',
    this.converges = false,
    this.seriesSum,
    this.partialSum = 0,
    this.partialSums = const [],
    this.numericOnly = false,
    this.numericRhs = '',
    this.numericNote = '',
    this.errorCode = '',
    this.errorDetail = '',
    this.errorPoint,
    this.hasErrorPoint = false,
    this.interpretedAs = '',
  });

  final bool ok;
  final String mode;
  final String typeId;
  final String typeFa;
  final String typeEn;
  final String error;

  /// Stable engine error code (`bad_characters`, `singular`,
  /// `not_integrable`, `unknown_function`) used to pick a user-facing message.
  final String errorCode;

  /// Technical context for logs only. Never shown to the student.
  final String errorDetail;

  /// Point where a singular domain error was detected, when known.
  final double? errorPoint;
  final bool hasErrorPoint;

  /// Canonical form of the input when the parser had to reinterpret it.
  final String interpretedAs;

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

  /// Right side of y' = f(x, y) for the orthogonal family after c is removed.
  final ExView? orthogonalSlope;

  /// Oblique/orthogonal trajectory family.  The dedicated oblique mode fills
  /// these fields; the orthogonal mode keeps the legacy [orthogonal] fields.
  final ExView? trajectory;
  final bool trajectoryExplicit;
  final ExView? trajectorySlope;

  /// Requested crossing angle in degrees (90 for orthogonal trajectories).
  final double trajectoryAngle;
  final String trajectoryMode;

  /// Explicit y(x, c) branches of the original family, used to overlay it on
  /// the trajectory direction field.
  final List<ExView> familyBranches;
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

  /// Asymptotes and curve-sketch summary (behaviour mode).
  final List<String> asymptotes;
  final List<ExView> xIntercepts;
  final ExView? yIntercept;
  final bool hasYIntercept;
  final List<String> sketch;

  /// Taylor / Maclaurin payload.
  final ExView? polynomial;
  final ExView? remainder;
  final List<ExView> coefficients;

  /// Definite-integral applications: the integrand and the application id.
  final ExView? integrand;
  final String application;

  /// Partial derivatives and gradient.
  final List<PartialView> partials;
  final List<ExView> gradient;
  final ExView? laplacian;
  final ExView? directional;
  final ExView? functionValue;
  final String pointLabel;

  /// Series convergence.
  final String seriesVerdict;
  final String seriesTest;
  final bool converges;
  final ExView? seriesSum;
  final double partialSum;

  /// S_1 ... S_400 for the partial-sum graph.
  final List<double> partialSums;

  /// True when the engine could not find a closed form and returned RK4
  /// trajectories instead. The UI must label the answer as approximate.
  final bool numericOnly;
  final String numericRhs;
  final String numericNote;

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
    errorCode: (j['error_code'] ?? '').toString(),
    errorDetail: (j['error_detail'] ?? '').toString(),
    errorPoint: (j['error_point'] as num?)?.toDouble(),
    hasErrorPoint: j['error_point'] != null,
    interpretedAs: (j['interpreted_as'] ?? '').toString(),
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
    orthogonalSlope: ExView.fromJson(j['orthogonal_slope']),
    trajectory: ExView.fromJson(j['trajectory']),
    trajectoryExplicit: j['trajectory_explicit'] == true,
    trajectorySlope: ExView.fromJson(j['trajectory_slope']),
    trajectoryAngle: (j['trajectory_angle'] as num?)?.toDouble() ?? 90,
    trajectoryMode: (j['trajectory_mode'] ?? '').toString(),
    familyBranches: _exList(j['family_branches']),
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
    asymptotes: _strList(j['asymptotes']),
    xIntercepts: _exList(j['x_intercepts']),
    yIntercept: ExView.fromJson(j['y_intercept']),
    hasYIntercept: j['has_y_intercept'] == true,
    sketch: _strList(j['sketch']),
    polynomial: ExView.fromJson(j['polynomial']),
    remainder: ExView.fromJson(j['remainder']),
    coefficients: _exList(j['coefficients']),
    integrand: ExView.fromJson(j['integrand']),
    application: (j['application'] ?? '').toString(),
    partials: j['partials'] is List
        ? (j['partials'] as List)
              .map(PartialView.fromJson)
              .whereType<PartialView>()
              .toList(growable: false)
        : const [],
    gradient: _exList(j['gradient']),
    laplacian: ExView.fromJson(j['laplacian']),
    directional: ExView.fromJson(j['directional']),
    functionValue: ExView.fromJson(j['function_value']),
    pointLabel: (j['point_label'] ?? '').toString(),
    seriesVerdict: (j['series_verdict'] ?? '').toString(),
    seriesTest: (j['series_test'] ?? '').toString(),
    converges: j['converges'] == true,
    seriesSum: ExView.fromJson(j['series_sum']),
    partialSum: (j['partial_sum'] as num?)?.toDouble() ?? 0,
    partialSums: doubleList(j['partial_sums']),
    numericOnly: j['numeric_only'] == true,
    numericRhs: (j['numeric_rhs'] ?? '').toString(),
    numericNote: (j['numeric_note'] ?? '').toString(),
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
      throw const FormatException('invalid engine response');
    }
    return SolveResult.fromJson(decoded);
  }

  /// The expression that should be shown as the main answer.
  ExView? get primary {
    if (mode == 'wronskian') return wronskian;
    if (mode == 'envelope') return envelope.isEmpty ? null : envelope.first;
    if (mode == 'oblique') return trajectory ?? solution;
    if (isExplicit && explicitY != null) return explicitY;
    return solution;
  }

  /// The trajectory family for the orthogonal/oblique modes.
  ExView? get trajectorySolution => trajectory ?? orthogonal;
  bool get trajectoryIsExplicit =>
      trajectory != null ? trajectoryExplicit : orthogonalExplicit;

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

class TrajectoryCurveView {
  const TrajectoryCurveView({
    required this.x0,
    required this.y0,
    required this.xs,
    required this.ys,
  });

  final double x0;
  final double y0;
  final List<double> xs;
  final List<double> ys;

  static TrajectoryCurveView? fromJson(dynamic value) {
    if (value is! Map) return null;
    return TrajectoryCurveView(
      x0: (value['x0'] as num?)?.toDouble() ?? 0,
      y0: (value['y0'] as num?)?.toDouble() ?? 0,
      xs: doubleList(value['xs']),
      ys: doubleList(value['ys']),
    );
  }
}

class TrajectoryResult {
  const TrajectoryResult({
    required this.ok,
    required this.error,
    required this.rhs,
    required this.order,
    required this.xmin,
    required this.xmax,
    required this.curves,
  });

  final bool ok;
  final String error;
  final String rhs;
  final int order;
  final double xmin;
  final double xmax;
  final List<TrajectoryCurveView> curves;

  factory TrajectoryResult.fromJson(Map<String, dynamic> j) => TrajectoryResult(
    ok: j['ok'] == true,
    error: (j['error'] ?? '').toString(),
    rhs: (j['rhs'] ?? '').toString(),
    order: (j['order'] as num?)?.toInt() ?? 2,
    xmin: (j['xmin'] as num?)?.toDouble() ?? -6,
    xmax: (j['xmax'] as num?)?.toDouble() ?? 6,
    curves: j['curves'] is List
        ? (j['curves'] as List)
              .map(TrajectoryCurveView.fromJson)
              .whereType<TrajectoryCurveView>()
              .toList(growable: false)
        : const [],
  );
}

class SurfaceResult {
  const SurfaceResult({
    required this.ok,
    required this.error,
    required this.xmin,
    required this.xmax,
    required this.ymin,
    required this.ymax,
    required this.nx,
    required this.ny,
    required this.zs,
  });

  final bool ok;
  final String error;
  final double xmin;
  final double xmax;
  final double ymin;
  final double ymax;
  final int nx;
  final int ny;

  /// Row-major grid; NaN marks an undefined sample.
  final List<List<double>> zs;

  factory SurfaceResult.fromJson(Map<String, dynamic> j) => SurfaceResult(
    ok: j['ok'] == true,
    error: (j['error'] ?? '').toString(),
    xmin: (j['xmin'] as num?)?.toDouble() ?? -4,
    xmax: (j['xmax'] as num?)?.toDouble() ?? 4,
    ymin: (j['ymin'] as num?)?.toDouble() ?? -4,
    ymax: (j['ymax'] as num?)?.toDouble() ?? 4,
    nx: (j['nx'] as num?)?.toInt() ?? 0,
    ny: (j['ny'] as num?)?.toInt() ?? 0,
    zs: j['zs'] is List
        ? (j['zs'] as List)
              .whereType<List>()
              .map(doubleList)
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
