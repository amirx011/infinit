// Tangent and normal lines at a point.
//
// The plot draws the curve, the tangent and the normal together, which is the
// only way the perpendicularity claim becomes believable at a glance. The line
// slopes come from the exact symbolic values the engine returned.
import 'package:flutter/material.dart';

import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class TangentPage extends StatefulWidget {
  const TangentPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<TangentPage> createState() => _TangentPageState();
}

class _TangentPageState extends State<TangentPage> {
  final _input = TextEditingController(text: 'x^2');
  final _xv = TextEditingController(text: 'x');
  final _at = TextEditingController(text: '1');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String? _fieldExpression;
  String _fieldVariable = 'x';
  double _fieldAt = 1;

  static const _examples = <String>[
    'x^2',
    'x^3 - 3*x',
    'sin(x)',
    'exp(x)',
    'ln(x)',
    '1/x',
  ];

  @override
  void didUpdateWidget(covariant TangentPage oldWidget) {
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
    _at.dispose();
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
    final at = _at.text.trim();
    if (text.isEmpty || at.isEmpty) {
      setState(
        () => _error = l.t('ui.enter_a_function_and_a_point'),
      );
      return;
    }
    final xVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    setState(() {
      _busy = true;
      _error = null;
      _fieldExpression = text;
      _fieldVariable = xVar;
      _fieldAt = double.tryParse(at.replaceAll('−', '-')) ?? 0;
    });
    try {
      final r = await engine.tangentAsync(
        text,
        x: xVar,
        at: at,
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
                icon: Icons.show_chart_rounded,
                title: l.t('ui.tangent_and_normal'),
                subtitle: l.t('ui.the_tangent_line_and_the_perpendicular_normal_at'),
              ),
              const SizedBox(height: 18),
              ExpressionField(
                controller: _input,
                hint: l.t('ui.e_g_x_2'),
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
                    controller: _at,
                    label: l.t('ui.at_x_a'),
                    width: 120,
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
                    : const Icon(Icons.show_chart_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('ui.computing')
                      : l.t('ui.find_the_two_lines'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.one_derivative_two_lines'),
          idea: l.t('ui.the_derivative_evaluated_at_a_gives_the_slope_of'),
          points: [
            l.t('ui.point_slope_form_y_f_a_f_a_x_a_is_the_whole_cons'),
            l.t('ui.a_horizontal_tangent_has_a_vertical_normal_whose'),
          ],
          pitfall: l.t('ui.the_normal_line_is_not_the_reciprocal_of_the_fun'),
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
    final fA = exToDouble(r.value);
    final m = exToDouble(r.tangentSlope);
    final n = exToDouble(r.normalSlope);
    final window = plotWindowAround([_fieldAt, ?fA], fallback: 4);

    final series = <PlotSeries>[];
    if (m != null && fA != null) {
      series.add(
        sampleLine(
          slope: m,
          intercept: fA - m * _fieldAt,
          xmin: window.$1,
          xmax: window.$2,
          color: c.success,
          label: l.t('ui.tangent'),
        ),
      );
    }
    if (n != null && fA != null) {
      series.add(
        sampleLine(
          slope: n,
          intercept: fA - n * _fieldAt,
          xmin: window.$1,
          xmax: window.$2,
          color: c.creative,
          label: l.t('ui.normal_2'),
          dashed: true,
        ),
      );
    }

    return CurvePlot(
      engine: widget.engine,
      expression: _fieldExpression!,
      variable: _fieldVariable,
      l10n: l,
      xmin: window.$1,
      xmax: window.$2,
      height: 340,
      title: l.t('ui.curve_tangent_and_normal'),
      extraSeries: series,
      guides: [_fieldAt],
      markers: fA == null
          ? const []
          : [
              PlotMarker(
                x: _fieldAt,
                y: fA,
                color: c.accent,
                kind: PlotMarkerKind.point,
              ),
            ],
      caption: n == null
          ? l.t('ui.the_tangent_is_horizontal_here_so_the_normal_is')
          : l.t('ui.the_solid_line_is_the_tangent_and_the_dashed_lin'),
    );
  }
}
