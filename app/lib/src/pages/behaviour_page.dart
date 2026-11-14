// Function behaviour analysis: where f rises, falls, bends and turns.
//
// The page pairs the native analysis with one picture that carries the whole
// story: the curve, the critical points as markers, the inflection points, and
// dashed guides at every special x.
import 'package:flutter/material.dart';

import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class BehaviourPage extends StatefulWidget {
  const BehaviourPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<BehaviourPage> createState() => _BehaviourPageState();
}

class _BehaviourPageState extends State<BehaviourPage> {
  final _input = TextEditingController(text: 'x^3 - 3*x');
  final _xv = TextEditingController(text: 'x');
  final _xmin = TextEditingController(text: '-3');
  final _xmax = TextEditingController(text: '3');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String? _fieldExpression;
  String _fieldVariable = 'x';
  double _fieldMin = -3;
  double _fieldMax = 3;

  static const _examples = <String>[
    'x^3 - 3*x',
    'x^4 - 2*x^2',
    'x/(1+x^2)',
    'exp(-x^2)',
    'x*exp(-x)',
    'x^3',
  ];

  @override
  void didUpdateWidget(covariant BehaviourPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _result = null;
      _error = null;
    }
  }

  @override
  void dispose() {
    _input.dispose();
    _xv.dispose();
    _xmin.dispose();
    _xmax.dispose();
    super.dispose();
  }

  double _bound(TextEditingController c, double fallback) =>
      double.tryParse(c.text.trim().replaceAll('−', '-')) ?? fallback;

  Future<void> _run() async {
    final engine = widget.engine;
    // A previous answer must not stay on screen once a new run starts.
    if (_result != null) setState(() => _result = null);
    final l = widget.l10n;
    if (engine == null) {
      setState(
        () => _error =
            widget.engineError ??
            l.t('ui.the_computation_engine_is_unavailable'),
      );
      return;
    }
    final text = _input.text.trim();
    if (text.isEmpty) {
      setState(() => _error = l.t('ui.enter_an_expression'));
      return;
    }
    final xVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    final lo = _bound(_xmin, -3);
    final hi = _bound(_xmax, 3);
    if (!(lo < hi)) {
      setState(() => _error = l.t('ui.the_interval_must_satisfy_a_b'));
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
      _fieldExpression = text;
      _fieldVariable = xVar;
      _fieldMin = lo;
      _fieldMax = hi;
    });
    try {
      final r = await engine.behaviourAsync(
        text,
        x: xVar,
        xmin: lo,
        xmax: hi,
        lang: l.isFa ? 'fa' : 'en',
      );
      if (!mounted) return;
      setState(() {
        _result = r;
        _busy = false;
      });
      revealWidget(_resultKey);
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _error = l.t('ui.engine_error');
        _busy = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final l = widget.l10n;
    final r = _result;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                icon: Icons.insights_rounded,
                title: l.t('ui.function_behaviour'),
                subtitle: l.t(
                  'ui.first_and_second_derivative_tests_monotonicity_i',
                ),
              ),
              const SizedBox(height: 18),
              ExpressionField(
                controller: _input,
                hint: l.t('ui.e_g_x_3_3_x'),
                onSubmitted: _run,
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
                  LabelledField(
                    controller: _xmin,
                    label: l.t('ui.from_a'),
                    width: 110,
                  ),
                  LabelledField(
                    controller: _xmax,
                    label: l.t('ui.to_b'),
                    width: 110,
                  ),
                ],
              ),
              const SizedBox(height: 14),
              ExampleRow(
                examples: _examples,
                l10n: l,
                onPick: (ex) {
                  _input.text = ex;
                  _run();
                },
              ),
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
                    : const Icon(Icons.insights_rounded, size: 19),
                label: Text(
                  _busy ? l.t('ui.analysing') : l.t('ui.analyse_the_function'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.two_derivatives_three_questions'),
          idea: l.t('ui.the_first_derivative_answers_where_the_function'),
          points: [
            l.t('ui.a_sign_change_of_f_from_to_is_a_local_maximum_an'),
            l.t('ui.a_sign_change_of_f_marks_an_inflection_point_whe'),
            l.t('ui.f_c_0_on_its_own_decides_nothing_the_sign_on_bot'),
          ],
          pitfall: l.t('ui.a_stationary_turning_point_such_as_x_0_for_x_has'),
        ),
        if (_error != null) ...[
          const SizedBox(height: 16),
          ErrorCard(message: _error!),
        ],
        if (r != null && _fieldExpression != null) ...[
          const SizedBox(height: 16),
          _plot(r),
        ],
        if (r != null) ...[
          const SizedBox(height: 18),
          Container(
            key: _resultKey,
            child: ResultView(result: r, l10n: l),
          ),
        ],
      ],
    );
  }

  Widget _plot(SolveResult r) {
    final l = widget.l10n;
    final c = context.colors;
    final criticalX = [
      for (final p in r.critical)
        if (exToDouble(p.x) != null) exToDouble(p.x)!,
    ];
    final inflectionX = [
      for (final p in r.inflections)
        if (exToDouble(p.x) != null) exToDouble(p.x)!,
    ];
    return CurvePlot(
      engine: widget.engine,
      expression: _fieldExpression!,
      variable: _fieldVariable,
      l10n: l,
      xmin: _fieldMin,
      xmax: _fieldMax,
      height: 320,
      title: l.t('ui.curve_extrema_and_inflections'),
      guides: [...criticalX, ...inflectionX],
      markers: [
        for (final p in r.critical)
          if (exToDouble(p.x) != null && exToDouble(p.value) != null)
            PlotMarker(
              x: exToDouble(p.x)!,
              y: exToDouble(p.value)!,
              color: p.kind == 'maximum' ? c.success : c.warning,
              kind: PlotMarkerKind.point,
            ),
        for (final p in r.inflections)
          if (exToDouble(p.x) != null && exToDouble(p.value) != null)
            PlotMarker(
              x: exToDouble(p.x)!,
              y: exToDouble(p.value)!,
              color: c.info,
              kind: PlotMarkerKind.hole,
            ),
      ],
      caption: l.t('ui.dashed_guides_mark_the_critical_points_and_infle'),
    );
  }
}
