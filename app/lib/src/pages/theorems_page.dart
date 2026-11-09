// The important theorems of the derivative.
//
// One page serves every theorem: each mode states the theorem in full, verifies
// its hypotheses on the supplied interval, finds the point the theorem promises,
// and draws the picture. The catalogue mode renders the complete reference.
import 'package:flutter/material.dart';

import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../theorems.dart';
import '../widgets.dart';

/// Which theorem the page presents.
enum TheoremMode { mvt, rolle, cauchy, lhopital, catalogue }

class TheoremsPage extends StatefulWidget {
  const TheoremsPage({
    super.key,
    required this.mode,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final TheoremMode mode;
  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<TheoremsPage> createState() => _TheoremsPageState();
}

class _TheoremsPageState extends State<TheoremsPage> {
  final _f = TextEditingController();
  final _g = TextEditingController();
  final _xv = TextEditingController(text: 'x');
  final _a = TextEditingController(text: '0');
  final _b = TextEditingController(text: '1');
  final _point = TextEditingController(text: '0');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String? _fieldF;
  String? _fieldG;
  double _fieldA = 0;
  double _fieldB = 1;
  String? _fieldPoint;

  @override
  void initState() {
    super.initState();
    _applyDefaults();
  }

  @override
  void didUpdateWidget(covariant TheoremsPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.mode != widget.mode) {
      _applyDefaults();
      _result = null;
      _error = null;
    }
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _result = null;
      _error = null;
    }
  }

  void _applyDefaults() {
    switch (widget.mode) {
      case TheoremMode.mvt:
        _f.text = 'x^2';
        _a.text = '0';
        _b.text = '2';
      case TheoremMode.rolle:
        _f.text = 'x^2 - 4*x + 3';
        _a.text = '1';
        _b.text = '3';
      case TheoremMode.cauchy:
        _f.text = 'x^2';
        _g.text = 'x^3';
        _a.text = '0';
        _b.text = '1';
      case TheoremMode.lhopital:
        _f.text = 'sin(x)';
        _g.text = 'x';
        _point.text = '0';
      case TheoremMode.catalogue:
        break;
    }
  }

  @override
  void dispose() {
    _f.dispose();
    _g.dispose();
    _xv.dispose();
    _a.dispose();
    _b.dispose();
    _point.dispose();
    super.dispose();
  }

  String get _theoremId => switch (widget.mode) {
    TheoremMode.mvt => 'mvt',
    TheoremMode.rolle => 'rolle',
    TheoremMode.cauchy => 'cauchy',
    TheoremMode.lhopital => 'lhopital',
    TheoremMode.catalogue => 'mvt',
  };

  TheoremEntry get _entry =>
      theoremCatalog.firstWhere((t) => t.id == _theoremId);

  double _bound(TextEditingController c, double fallback) =>
      double.tryParse(c.text.trim().replaceAll('−', '-')) ?? fallback;

  Future<void> _run() async {
    final engine = widget.engine;
    final l = widget.l10n;
    if (engine == null) {
      setState(
        () => _error =
            widget.engineError ??
            l.t('ui.the_computation_engine_is_unavailable'),
      );
      return;
    }
    final f = _f.text.trim();
    if (f.isEmpty) {
      setState(
        () => _error = l.t('ui.enter_a_function'),
      );
      return;
    }
    final xVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    final a = _bound(_a, 0);
    final b = _bound(_b, 1);
    setState(() {
      _busy = true;
      _error = null;
      _fieldF = f;
      _fieldG = _g.text.trim().isEmpty ? null : _g.text.trim();
      _fieldA = a;
      _fieldB = b;
      _fieldPoint = _point.text.trim();
    });
    try {
      final lang = l.isFa ? 'fa' : 'en';
      final SolveResult r;
      switch (widget.mode) {
        case TheoremMode.cauchy:
          final g = _g.text.trim();
          if (g.isEmpty) {
            setState(() {
              _busy = false;
              _error = l.t('ui.cauchy_needs_a_second_function_g');
            });
            return;
          }
          r = await engine.cauchyAsync(f, g, x: xVar, a: a, b: b, lang: lang);
        case TheoremMode.lhopital:
          final g = _g.text.trim();
          if (g.isEmpty) {
            setState(() {
              _busy = false;
              _error = l.t('ui.l_h_pital_needs_a_denominator_g');
            });
            return;
          }
          r = await engine.lhopitalAsync(
            f,
            g,
            x: xVar,
            point: _point.text.trim(),
            lang: lang,
          );
        case TheoremMode.mvt:
        case TheoremMode.rolle:
          if (!(a < b)) {
            setState(() {
              _busy = false;
              _error = l.t('ui.the_interval_must_satisfy_a_b');
            });
            return;
          }
          r = await engine.meanValueAsync(
            f,
            x: xVar,
            a: a,
            b: b,
            lang: lang,
          );
        case TheoremMode.catalogue:
          return;
      }
      if (!mounted) return;
      setState(() {
        _result = r;
        _busy = false;
      });
      revealWidget(_resultKey);
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _error = l.t('Engine error: $e', 'خطا در اجرای موتور: $e');
        _busy = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    if (widget.mode == TheoremMode.catalogue) return _catalogue();
    return _interactive();
  }

  // ------------------------------------------------------------- interactive
  Widget _interactive() {
    final l = widget.l10n;
    final entry = _entry;
    final result = _result;
    final isLhopital = widget.mode == TheoremMode.lhopital;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                icon: isLhopital
                    ? Icons.compare_arrows_rounded
                    : Icons.rule_rounded,
                title: entry.title(l),
                subtitle: l.t('ui.state_the_theorem_verify_its_hypotheses_then_fin'),
              ),
              const SizedBox(height: 18),
              _statementBlock(entry),
              const SizedBox(height: 16),
              _hypothesesBlock(entry),
              const SizedBox(height: 20),
              if (!isLhopital)
                ExpressionField(
                  controller: _f,
                  hint: l.t('ui.e_g_x_2'),
                  onSubmitted: _run,
                )
              else
                Row(
                  children: [
                    Expanded(
                      child: ExpressionField(
                        controller: _f,
                        hint: l.t('ui.numerator_f'),
                        onSubmitted: _run,
                      ),
                    ),
                    const SizedBox(width: 10),
                    Expanded(
                      child: ExpressionField(
                        controller: _g,
                        hint: l.t('ui.denominator_g'),
                        onSubmitted: _run,
                      ),
                    ),
                  ],
                ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  LabelledField(
                    controller: _xv,
                    label: l.t('ui.variable'),
                    width: 100,
                  ),
                  if (isLhopital)
                    LabelledField(
                      controller: _point,
                      label: l.t('x →', 'x ←'),
                      width: 100,
                    )
                  else ...[
                    LabelledField(
                      controller: _a,
                      label: l.t('ui.from_a'),
                      width: 110,
                    ),
                    LabelledField(
                      controller: _b,
                      label: l.t('ui.to_b'),
                      width: 110,
                    ),
                  ],
                ],
              ),
              if (widget.mode == TheoremMode.cauchy) ...[
                const SizedBox(height: 12),
                ExpressionField(
                  controller: _g,
                  hint: l.t('ui.second_function_g'),
                  onSubmitted: _run,
                ),
              ],
              const SizedBox(height: 16),
              FilledButton.icon(
                onPressed: _busy ? null : _run,
                icon: _busy
                    ? const SizedBox(
                        width: 17,
                        height: 17,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          color: Colors.white,
                        ),
                      )
                    : const Icon(Icons.verified_outlined, size: 19),
                label: Text(
                  _busy
                      ? l.t('ui.checking')
                      : l.t('ui.check_the_theorem'),
                ),
              ),
            ],
          ),
        ),
        if (_error != null) ...[
          const SizedBox(height: 16),
          ErrorCard(message: _error!),
        ],
        if (result != null) ...[
          const SizedBox(height: 16),
          _verificationCard(result),
        ],
        if (result != null && _fieldF != null) ...[
          const SizedBox(height: 16),
          _plot(result),
        ],
        if (result != null) ...[
          const SizedBox(height: 18),
          Container(key: _resultKey, child: ResultView(result: result, l10n: l)),
        ],
      ],
    );
  }

  Widget _statementBlock(TheoremEntry entry) {
    final c = context.colors;
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.fromLTRB(16, 14, 16, 14),
      decoration: BoxDecoration(
        color: c.accentAltSoft,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: c.accentAlt.withValues(alpha: 0.25)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            entry.statement(widget.l10n),
            style: Theme.of(
              context,
            ).textTheme.bodyMedium?.copyWith(height: 1.85),
          ),
          const SizedBox(height: 12),
          MathBlock(
            math: ExView(
              plain: entry.statementEn,
              pretty: entry.statementEn,
              latex: entry.latex,
            ),
            size: 17,
          ),
        ],
      ),
    );
  }

  Widget _hypothesesBlock(TheoremEntry entry) {
    final c = context.colors;
    final items = entry.hypotheses(widget.l10n);
    if (items.isEmpty) return const SizedBox.shrink();
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          widget.l10n.t('ui.hypotheses_to_check'),
          style: Theme.of(context).textTheme.bodySmall,
        ),
        const SizedBox(height: 8),
        for (final item in items)
          Padding(
            padding: const EdgeInsets.only(bottom: 6),
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Icon(
                  Icons.circle,
                  size: 6,
                  color: c.accent.withValues(alpha: 0.7),
                ),
                const SizedBox(width: 9),
                Expanded(
                  child: Text(
                    item,
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                ),
              ],
            ),
          ),
      ],
    );
  }

  Widget _verificationCard(SolveResult r) {
    final l = widget.l10n;
    final c = context.colors;
    final checks = r.checks;
    final points = r.points;
    final isLhopital = widget.mode == TheoremMode.lhopital;
    return SoftCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SectionLabel(
            l.t('ui.theorem_check'),
            icon: Icons.fact_check_outlined,
          ),
          const SizedBox(height: 14),
          for (final check in checks)
            HypothesisRow(check: check, l10n: l),
          if (!isLhopital && r.rolleApplies && widget.mode != TheoremMode.rolle)
            Padding(
              padding: const EdgeInsets.only(bottom: 10),
              child: Row(
                children: [
                  Icon(Icons.info_outline_rounded, size: 18, color: c.info),
                  const SizedBox(width: 9),
                  Expanded(
                    child: Text(
                      l.t('ui.since_f_a_f_b_rolle_s_theorem_applies_here_as_a'),
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ),
                ],
              ),
            ),
          if (!isLhopital) ...[
            const SizedBox(height: 6),
            _valueRow(
              l.t('ui.secant_slope'),
              r.slope ??
                  const ExView(plain: '?', pretty: '?', latex: '?'),
            ),
          ],
          if (!isLhopital && points.isNotEmpty) ...[
            const SizedBox(height: 6),
            _valueRow(
              l.t('ui.promised_point_c'),
              ExView(
                plain: points.map((p) => 'c = ${p.plain}').join(', '),
                pretty: points.map((p) => 'c = ${p.pretty}').join(', '),
                latex: points
                    .asMap()
                    .entries
                    .map(
                      (e) => points.length == 1
                          ? 'c = ${e.value.latex}'
                          : 'c_{${e.key + 1}} = ${e.value.latex}',
                    )
                    .join(r',\quad '),
              ),
            ),
          ],
          if (isLhopital && r.solution != null) ...[
            const SizedBox(height: 6),
            _valueRow(l.t('ui.limit'), r.solution!),
          ],
          if (isLhopital && r.limitInfinite) ...[
            const SizedBox(height: 6),
            _valueRow(
              l.t('ui.limit'),
              ExView(
                plain: r.limitSign < 0 ? '-∞' : '∞',
                pretty: r.limitSign < 0 ? '−∞' : '∞',
                latex: r.limitSign < 0 ? r'-\infty' : r'\infty',
              ),
            ),
          ],
          if (isLhopital && !r.limitExists && !r.limitInfinite) ...[
            const SizedBox(height: 6),
            _textValueRow(
              l.t('ui.limit'),
              l.t('ui.does_not_exist'),
            ),
          ],
          if (points.isEmpty &&
              !isLhopital &&
              r.warnings.isNotEmpty) ...[
            const SizedBox(height: 8),
            for (final w in r.warnings)
              Padding(
                padding: const EdgeInsets.only(bottom: 6),
                child: Text(
                  w,
                  style: Theme.of(
                    context,
                  ).textTheme.bodySmall?.copyWith(color: c.warning),
                ),
              ),
          ],
        ],
      ),
    );
  }

  Widget _valueRow(String label, ExView math) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 7),
      child: Row(
        children: [
          SizedBox(
            width: 150,
            child: Text(label, style: Theme.of(context).textTheme.bodySmall),
          ),
          Expanded(child: MathInline(math: math, size: 15)),
        ],
      ),
    );
  }

  Widget _textValueRow(String label, String value) {
    final c = context.colors;
    return Padding(
      padding: const EdgeInsets.only(bottom: 7),
      child: Row(
        children: [
          SizedBox(
            width: 150,
            child: Text(label, style: Theme.of(context).textTheme.bodySmall),
          ),
          Expanded(
            child: Text(
              value,
              textDirection: TextDirection.ltr,
              textAlign: TextAlign.left,
              style: TextStyle(
                fontFamily: 'DejaVu Sans Mono',
                fontFamilyFallback: const ['monospace', 'Vazirmatn'],
                fontSize: 13.5,
                color: c.text,
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _plot(SolveResult r) {
    final l = widget.l10n;
    final c = context.colors;
    final f = _fieldF!;
    final xVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();

    if (widget.mode == TheoremMode.lhopital) {
      final point = double.tryParse(
        (_fieldPoint ?? '0').replaceAll('−', '-'),
      );
      final window = point == null
          ? (-3.0, 3.0)
          : (point - 3, point + 3);
      return MultiCurvePlot(
        engine: widget.engine,
        l10n: l,
        xmin: window.$1,
        xmax: window.$2,
        variable: xVar,
        height: 320,
        title: l.t('ui.numerator_and_denominator_near_the_point'),
        curves: [
          PlotCurveSpec(
            expression: f,
            color: c.accent,
            label: 'f($xVar)',
          ),
          PlotCurveSpec(
            expression: _fieldG ?? 'x',
            color: c.creative,
            label: 'g($xVar)',
            dashed: true,
          ),
        ],
        guides: [?point],
        caption: l.t('ui.the_dashed_guide_is_the_point_being_approached_w'),
      );
    }

    final a = _fieldA;
    final b = _fieldB;
    final fa = exToDouble(r.valueA);
    final fb = exToDouble(r.valueB);
    final secant = exToDouble(r.slope);
    final window = plotWindowAround([a, b], fallback: 4);

    final series = <PlotSeries>[];
    if (secant != null && fa != null) {
      series.add(
        sampleLine(
          slope: secant,
          intercept: fa - secant * a,
          xmin: window.$1,
          xmax: window.$2,
          color: c.warning,
          label: l.t('ui.secant'),
          dashed: true,
        ),
      );
    }

    final markers = <PlotMarker>[
      if (fa != null)
        PlotMarker(x: a, y: fa, color: c.muted, kind: PlotMarkerKind.point),
      if (fb != null)
        PlotMarker(x: b, y: fb, color: c.muted, kind: PlotMarkerKind.point),
    ];
    final curves = <PlotCurveSpec>[
      PlotCurveSpec(expression: f, color: c.accent, label: 'f($xVar)'),
    ];

    if (widget.mode == TheoremMode.cauchy) {
      curves.add(
        PlotCurveSpec(
          expression: _fieldG ?? 'x',
          color: c.creative,
          label: 'g($xVar)',
          dashed: true,
        ),
      );
    }

    // A tangent at each promised point makes the conclusion visible: the whole
    // claim is that this tangent is parallel to the secant.
    final tangents = <PlotSeries>[];
    for (var i = 0; i < r.points.length; i++) {
      final cv = exToDouble(r.points[i]);
      final fv = i < r.pointValues.length
          ? exToDouble(r.pointValues[i])
          : null;
      if (cv == null) continue;
      if (fv != null) {
        markers.add(
          PlotMarker(
            x: cv,
            y: fv,
            color: c.success,
            kind: PlotMarkerKind.point,
          ),
        );
      }
      if (fv != null && secant != null) {
        tangents.add(
          sampleLine(
            slope: secant,
            intercept: fv - secant * cv,
            xmin: window.$1,
            xmax: window.$2,
            color: c.success,
            label: l.t('ui.tangent_at_c'),
          ),
        );
      }
    }

    return MultiCurvePlot(
      engine: widget.engine,
      l10n: l,
      xmin: window.$1,
      xmax: window.$2,
      variable: xVar,
      height: 330,
      title: l.t('ui.curve_secant_and_the_promised_point'),
      curves: curves,
      extraSeries: tangents,
      markers: markers,
      guides: [a, b],
      caption: l.t('ui.the_dashed_line_joins_the_two_endpoints_the_poin'),
    );
  }

  // --------------------------------------------------------------- catalogue
  Widget _catalogue() {
    final l = widget.l10n;
    final c = context.colors;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                icon: Icons.menu_book_rounded,
                title: l.t('ui.theorems_of_the_derivative'),
                subtitle: l.t('ui.every_theorem_stated_in_full_hypotheses_conclusi'),
              ),
              const SizedBox(height: 14),
              Text(
                l.t('ui.these_are_the_results_the_rest_of_the_calculus_r'),
                style: Theme.of(context).textTheme.bodySmall?.copyWith(height: 1.8),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        for (var i = 0; i < theoremCatalog.length; i++) ...[
          if (i > 0) const SizedBox(height: 12),
          _catalogueCard(theoremCatalog[i], i + 1),
        ],
        const SizedBox(height: 16),
        SoftCard(
          color: c.accentAltSoft,
          borderColor: c.accentAlt.withValues(alpha: 0.25),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Icon(Icons.lightbulb_outline_rounded, size: 19, color: c.accentAlt),
                  const SizedBox(width: 9),
                  Text(
                    l.t('ui.the_mistakes_that_cost_marks'),
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                ],
              ),
              const SizedBox(height: 12),
              for (final pitfall in _commonPitfalls(l))
                Padding(
                  padding: const EdgeInsets.only(bottom: 7),
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Icon(Icons.remove_rounded, size: 14, color: c.warning),
                      const SizedBox(width: 8),
                      Expanded(
                        child: Text(
                          pitfall,
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ),
                    ],
                  ),
                ),
            ],
          ),
        ),
      ],
    );
  }

  List<String> _commonPitfalls(L10n l) => [
    l.t('ui.rolle_and_the_mvt_need_continuity_on_the_closed'),
    l.t('ui.f_c_0_never_proves_an_extremum_on_its_own_the_si'),
    l.t('ui.l_h_pital_requires_a_genuinely_indeterminate_for'),
    l.t('ui.cauchy_needs_g_x_0_on_the_open_interval_which_is'),
    l.t('ui.the_promised_point_c_is_guaranteed_to_exist_not'),
  ];

  Widget _catalogueCard(TheoremEntry entry, int index) {
    final l = widget.l10n;
    final c = context.colors;
    return SoftCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Container(
                width: 26,
                height: 26,
                alignment: Alignment.center,
                decoration: BoxDecoration(
                  color: c.accentSoft,
                  shape: BoxShape.circle,
                  border: Border.all(color: c.accent.withValues(alpha: 0.35)),
                ),
                child: Text(
                  '$index',
                  style: TextStyle(
                    fontFamily: 'Vazirmatn',
                    fontSize: 12,
                    fontWeight: FontWeight.w700,
                    color: c.accent,
                  ),
                ),
              ),
              const SizedBox(width: 11),
              Expanded(
                child: Text(
                  entry.title(l),
                  style: Theme.of(
                    context,
                  ).textTheme.titleMedium?.copyWith(fontSize: 15),
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          Text(
            entry.statement(l),
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(height: 1.85),
          ),
          const SizedBox(height: 12),
          MathBlock(
            math: ExView(
              plain: entry.statementEn,
              pretty: entry.statementEn,
              latex: entry.latex,
            ),
            size: 15.5,
          ),
          const SizedBox(height: 12),
          Text(
            entry.why(l),
            style: Theme.of(context).textTheme.bodySmall?.copyWith(height: 1.75),
          ),
          if (entry.hypotheses(l).isNotEmpty) ...[
            const SizedBox(height: 12),
            Text(
              l.t('ui.hypotheses'),
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                fontWeight: FontWeight.w700,
                color: c.text,
              ),
            ),
            const SizedBox(height: 6),
            for (final h in entry.hypotheses(l))
              Padding(
                padding: const EdgeInsets.only(bottom: 4),
                child: Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Icon(Icons.circle, size: 5, color: c.accent),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        h,
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ),
                  ],
                ),
              ),
          ],
          if (entry.pitfalls(l).isNotEmpty) ...[
            const SizedBox(height: 10),
            for (final p in entry.pitfalls(l))
              Padding(
                padding: const EdgeInsets.only(bottom: 5),
                child: Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Icon(
                      Icons.error_outline_rounded,
                      size: 14,
                      color: c.warning,
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        p,
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ),
                  ],
                ),
              ),
          ],
        ],
      ),
    );
  }
}
