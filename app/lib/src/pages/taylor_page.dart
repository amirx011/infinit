// Taylor and Maclaurin polynomials with the Lagrange remainder.
import 'package:flutter/material.dart';

import '../app_state.dart';
import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class TaylorPage extends StatefulWidget {
  const TaylorPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<TaylorPage> createState() => _TaylorPageState();
}

class _TaylorPageState extends State<TaylorPage> {
  final _input = TextEditingController(text: 'exp(x)');
  final _xv = TextEditingController(text: 'x');
  final _center = TextEditingController(text: '0');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  int _order = 4;

  static const _examples = <String>[
    'exp(x)',
    'sin(x)',
    'cos(x)',
    'ln(1+x)',
    '1/(1-x)',
    'sqrt(1+x)',
    'tan(x)',
  ];

  @override
  void didUpdateWidget(covariant TaylorPage oldWidget) {
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
    _center.dispose();
    super.dispose();
  }

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
    final center = double.tryParse(_center.text.trim().replaceAll('−', '-'));
    if (text.isEmpty || center == null) {
      setState(
        () => _error = l.t('ui.enter_a_function_and_a_numeric_centre_a'),
      );
      return;
    }
    final xVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final r = await engine.taylorAsync(
        text,
        x: xVar,
        a: center,
        order: _order,
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
    final c = context.colors;
    final showGraphs = SettingsScope.maybeOf(context)?.showGraphs ?? true;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                icon: Icons.polyline_rounded,
                title: l.t('ui.taylor_maclaurin'),
                subtitle: l.t(
                  'ui.approximate_a_function_by_a_polynomial_built_fro',
                ),
              ),
              const SizedBox(height: 18),
              ExpressionField(
                controller: _input,
                hint: l.t('ui.e_g_exp_x'),
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
                    controller: _center,
                    label: l.t('ui.centre_a_0_maclaurin'),
                    width: 180,
                  ),
                  SizedBox(
                    width: 150,
                    child: InputDecorator(
                      decoration: InputDecoration(labelText: l.t('ui.order_n')),
                      child: DropdownButtonHideUnderline(
                        child: DropdownButton<int>(
                          value: _order,
                          isDense: true,
                          items: [
                            for (var i = 0; i <= 12; i++)
                              DropdownMenuItem(value: i, child: Text('$i')),
                          ],
                          onChanged: (v) => setState(() => _order = v ?? 4),
                        ),
                      ),
                    ),
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
                    : const Icon(Icons.polyline_rounded, size: 19),
                label: Text(
                  _busy ? l.t('ui.computing') : l.t('ui.build_the_polynomial'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.derivatives_at_one_point_reused'),
          idea: l.t('ui.the_taylor_polynomial_matches_the_value_and_the'),
          points: [
            l.t('ui.maclaurin_is_taylor_with_a_0'),
            l.t('ui.the_lagrange_remainder_bounds_the_error_the_next'),
            l.t('ui.higher_order_is_not_always_better_the_series_mus'),
          ],
          pitfall: l.t('ui.do_not_forget_the_factorial_in_every_coefficient'),
        ),
        if (_error != null) ...[
          const SizedBox(height: 16),
          ErrorCard(message: _error!),
        ],
        if (r != null) ...[
          const SizedBox(height: 18),
          Container(
            key: _resultKey,
            child: ResultView(result: r, l10n: l),
          ),
          if (showGraphs &&
              r.ok &&
              r.normalized != null &&
              r.polynomial != null) ...[
            const SizedBox(height: 16),
            MultiCurvePlot(
              engine: widget.engine,
              l10n: l,
              variable: r.variable.isEmpty ? 'x' : r.variable,
              curves: [
                PlotCurveSpec(
                  expression: r.normalized!.plain,
                  color: c.accent,
                  label: l.t('f(x)', 'f(x)'),
                ),
                PlotCurveSpec(
                  expression: r.polynomial!.plain,
                  color: c.accentAlt,
                  dashed: true,
                  label: l.t('ui.taylor_polynomial'),
                ),
              ],
              xmin: _plotCentre() - 5,
              xmax: _plotCentre() + 5,
              title: l.t('ui.function_and_taylor_polynomial'),
              caption: l.t(
                'ui.the_polynomial_agrees_with_f_at_the_centre_and_m',
              ),
            ),
          ],
        ],
      ],
    );
  }

  double _plotCentre() => double.tryParse(_center.text.trim()) ?? 0;
}
