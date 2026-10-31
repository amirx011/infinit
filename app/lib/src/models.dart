// Data model mirroring the JSON contract of the native engine.
import 'dart:convert';

class ExView {
  const ExView({required this.plain, required this.pretty, required this.latex});

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
    required this.hasNumeric,
    required this.numericValue,
    required this.vars,
    required this.lower,
    required this.upper,
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
  final bool hasNumeric;
  final double numericValue;
  final List<String> vars;
  final List<String> lower;
  final List<String> upper;

  static List<ExView> _exList(dynamic value) {
    if (value is! List) return const [];
    return value.map(ExView.fromJson).whereType<ExView>().toList(growable: false);
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
            ? (j['checks'] as List).whereType<Map>().map(CheckView.fromJson).toList()
            : const [],
        warnings: _strList(j['warnings']),
        assumptions: _strList(j['assumptions']),
        dimension: (j['dimension'] as num?)?.toInt() ?? 1,
        definite: j['definite'] == true,
        implicit: j['implicit'] == true,
        hasNumeric: j['has_numeric'] == true,
        numericValue: (j['numeric_value'] as num?)?.toDouble() ?? 0,
        vars: _strList(j['vars']),
        lower: _strList(j['lower']),
        upper: _strList(j['upper']),
      );

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
