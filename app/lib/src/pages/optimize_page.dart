// Optimisation on a closed interval.
//
// The page compares the stationary points with the two endpoints, which is the
// exhaustive list of places an extreme value can hide on [a, b].
import 'package:flutter/material.dart';

import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class OptimizePage extends StatefulWidget {
  const OptimizePage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<OptimizePage> createState() => _OptimizePageState();
}

class _OptimizePageState extends State<OptimizePage> {
  final _input = TextEditingController(text: 'x^3 - 3*x');
  final _xv = TextEditingController(text: 'x');
  final _xmin = TextEditingController(text: '-2');
  final _xmax = TextEditingController(text: '2');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String? _fieldExpression;
  String _fieldVariable = 'x';
  double _fieldMin = -2;
  double _fieldMax = 2;

  static const _examples = <String>[
    'x^3 - 3*x',
    'x^2 - 4*x + 5',
    'x^4 - 2*x^2',
    '-x^2 + 4*x',
    'x + 4/x',
    'sin(x)',
  ];

  @override
  void didUpdateWidget(covariant OptimizePage oldWidget) {
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
      setState(
        () => _error = l.t('ui.enter_an_expression'),
      );
      return;
    }
    final xVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    final lo = _bound(_xmin, -2);
    final hi = _bound(_xmax, 2);
    if (!(lo < hi)) {
      setState(
        () => _error = l.t('ui.the_interval_must_satisfy_a_b'),
      );
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
      final r = await engine.optimizeAsync(
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
        _error = l.t('Engine error: $e', 'خطا در اجرای موتور: $e');
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
                icon: Icons.vertical_align_bottom_rounded,
                title: l.t('ui.optimisation'),
                subtitle: l.t('ui.largest_and_smallest_value_of_a_function_on_a_cl'),
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
                    : const Icon(Icons.vertical_align_bottom_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('ui.comparing_candidates')
                      : l.t('ui.find_the_extreme_values'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.a_closed_interval_leaves_nothing_to_chance'),
          idea: l.t('ui.on_a_closed_bounded_interval_a_continuous_functi'),
          points: [
            l.t('ui.solving_f_x_0_finds_the_interior_candidates'),
            l.t('ui.the_endpoints_are_candidates_too_even_though_f_n'),
            l.t('ui.the_largest_value_on_the_list_is_the_maximum_and'),
          ],
          pitfall: l.t('ui.skipping_the_endpoints_is_the_classic_mistake_th'),
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
          Container(key: _resultKey, child: ResultView(result: r, l10n: l)),
        ],
      ],
    );
  }

  Widget _plot(SolveResult r) {
    final l = widget.l10n;
    final c = context.colors;
    final maxX = exToDouble(r.maximumAt);
    final maxY = exToDouble(r.maximum);
    final minX = exToDouble(r.minimumAt);
    final minY = exToDouble(r.minimum);
    return CurvePlot(
      engine: widget.engine,
      expression: _fieldExpression!,
      variable: _fieldVariable,
      l10n: l,
      xmin: _fieldMin,
      xmax: _fieldMax,
      height: 320,
      title: l.t('ui.objective_and_its_candidates'),
      guides: [?maxX, ?minX],
      markers: [
        if (maxX != null && maxY != null)
          PlotMarker(
            x: maxX,
            y: maxY,
            color: c.success,
            kind: PlotMarkerKind.point,
          ),
        if (minX != null && minY != null)
          PlotMarker(
            x: minX,
            y: minY,
            color: c.warning,
            kind: PlotMarkerKind.point,
          ),
      ],
      caption: l.t('ui.the_guide_lines_mark_where_the_extreme_values_oc'),
    );
  }
}
