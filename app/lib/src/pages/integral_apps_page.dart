// Definite-integral applications: area between curves, volume of revolution
// and arc length.
import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../app_state.dart';
import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

enum _IntegralApp { area, volume, arcLength }

class IntegralAppsPage extends StatefulWidget {
  const IntegralAppsPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<IntegralAppsPage> createState() => _IntegralAppsPageState();
}

class _IntegralAppsPageState extends State<IntegralAppsPage> {
  final _f = TextEditingController(text: 'x');
  final _g = TextEditingController(text: 'x^2');
  final _a = TextEditingController(text: '0');
  final _b = TextEditingController(text: '1');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  _IntegralApp _kind = _IntegralApp.area;
  String _axis = 'x';

  static const _areaExamples = <String>[
    'x|x^2',
    'sin(x)|0',
    '4-x^2|x^2',
    'x^2|2*x',
  ];
  static const _volumeExamples = <String>['x^2|', 'sqrt(x)|0', 'x|0', '1/x|0'];
  static const _arcExamples = <String>[
    'x^2',
    'sin(x)',
    'exp(x)',
    'ln(x)',
    'sqrt(1-x^2)',
  ];

  @override
  void didUpdateWidget(covariant IntegralAppsPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _result = null;
      _error = null;
    }
  }

  @override
  void dispose() {
    _f.dispose();
    _g.dispose();
    _a.dispose();
    _b.dispose();
    super.dispose();
  }

  String get _engineKind => switch (_kind) {
    _IntegralApp.area => 'area',
    _IntegralApp.volume => 'volume',
    _IntegralApp.arcLength => 'arc_length',
  };

  List<String> get _examples => switch (_kind) {
    _IntegralApp.area => _areaExamples,
    _IntegralApp.volume => _volumeExamples,
    _IntegralApp.arcLength => _arcExamples,
  };

  void _pick(String example) {
    if (_kind == _IntegralApp.arcLength) {
      _f.text = example;
    } else {
      final parts = example.split('|');
      _f.text = parts.first;
      _g.text = parts.length > 1 ? parts[1] : '';
    }
    _run();
  }

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
    final f = _f.text.trim();
    final a = double.tryParse(_a.text.trim().replaceAll('−', '-'));
    final b = double.tryParse(_b.text.trim().replaceAll('−', '-'));
    if (f.isEmpty || a == null || b == null) {
      setState(
        () => _error = l.t('ui.enter_the_function_and_numeric_bounds_a_and_b'),
      );
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final r = await engine.integralApplicationAsync(
        _engineKind,
        f,
        g: _kind == _IntegralApp.arcLength ? '' : _g.text.trim(),
        a: a,
        b: b,
        axis: _axis,
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
                icon: Icons.architecture_rounded,
                title: l.t('ui.integral_applications'),
                subtitle: l.t(
                  'ui.area_between_curves_volume_of_revolution_and_arc',
                ),
              ),
              const SizedBox(height: 18),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  SizedBox(
                    width: 200,
                    child: InputDecorator(
                      decoration: InputDecoration(
                        labelText: l.t('ui.application'),
                      ),
                      child: DropdownButtonHideUnderline(
                        child: DropdownButton<_IntegralApp>(
                          value: _kind,
                          isExpanded: true,
                          items: [
                            DropdownMenuItem(
                              value: _IntegralApp.area,
                              child: Text(l.t('ui.area_between_curves')),
                            ),
                            DropdownMenuItem(
                              value: _IntegralApp.volume,
                              child: Text(l.t('ui.volume_of_revolution')),
                            ),
                            DropdownMenuItem(
                              value: _IntegralApp.arcLength,
                              child: Text(l.t('ui.arc_length')),
                            ),
                          ],
                          onChanged: (v) {
                            setState(() {
                              _kind = v ?? _IntegralApp.area;
                              _result = null;
                            });
                          },
                        ),
                      ),
                    ),
                  ),
                  if (_kind == _IntegralApp.volume)
                    SizedBox(
                      width: 140,
                      child: InputDecorator(
                        decoration: InputDecoration(labelText: l.t('ui.axis')),
                        child: DropdownButtonHideUnderline(
                          child: DropdownButton<String>(
                            value: _axis,
                            isExpanded: true,
                            items: [
                              DropdownMenuItem(
                                value: 'x',
                                child: Text(l.t('ui.x_axis_disk_washer')),
                              ),
                              DropdownMenuItem(
                                value: 'y',
                                child: Text(l.t('ui.y_axis_shells')),
                              ),
                            ],
                            onChanged: (v) => setState(() => _axis = v ?? 'x'),
                          ),
                        ),
                      ),
                    ),
                ],
              ),
              const SizedBox(height: 14),
              ExpressionField(
                controller: _f,
                hint: l.t('ui.upper_curve_f_x'),
                onSubmitted: _run,
              ),
              if (_kind != _IntegralApp.arcLength) ...[
                const SizedBox(height: 12),
                ExpressionField(
                  controller: _g,
                  hint: l.t('ui.lower_curve_g_x_or_empty_for_0'),
                  onSubmitted: _run,
                ),
              ],
              const SizedBox(height: 12),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  LabelledField(
                    controller: _a,
                    label: l.t('ui.lower_bound_a'),
                    width: 150,
                  ),
                  LabelledField(
                    controller: _b,
                    label: l.t('ui.upper_bound_b'),
                    width: 150,
                  ),
                ],
              ),
              const SizedBox(height: 14),
              ExampleRow(examples: _examples, l10n: l, onPick: _pick),
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
                    : const Icon(Icons.architecture_rounded, size: 19),
                label: Text(
                  _busy ? l.t('ui.computing') : l.t('ui.set_up_and_evaluate'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.slice_then_add_up'),
          idea: l.t('ui.every_definite_integral_application_is_a_sum_of'),
          points: [
            l.t('ui.area_integrate_f_g_splitting_at_the_intersection'),
            l.t('ui.volume_about_x_pi_outer_inner_dx_about_y_2_pi_x'),
            l.t('ui.arc_length_sqrt_1_f_dx'),
          ],
          pitfall: l.t('ui.check_which_curve_is_on_top_before_subtracting_a'),
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
          if (showGraphs && r.ok) ...[
            const SizedBox(height: 16),
            if (_kind == _IntegralApp.area)
              MultiCurvePlot(
                engine: widget.engine,
                l10n: l,
                curves: [
                  PlotCurveSpec(
                    expression: _f.text.trim(),
                    color: c.accent,
                    label: 'f(x)',
                  ),
                  if (_g.text.trim().isNotEmpty)
                    PlotCurveSpec(
                      expression: _g.text.trim(),
                      color: c.accentAlt,
                      label: 'g(x)',
                    ),
                ],
                xmin: _plotLo(),
                xmax: _plotHi(),
                title: l.t('ui.curves_and_the_enclosed_region'),
                caption: l.t(
                  'ui.the_area_integral_adds_thin_vertical_strips_of_h',
                ),
              )
            else
              CurvePlot(
                engine: widget.engine,
                l10n: l,
                expression: _f.text.trim(),
                xmin: _plotLo(),
                xmax: _plotHi(),
                title: _kind == _IntegralApp.volume
                    ? l.t('ui.generating_curve')
                    : l.t('ui.curve_whose_length_is_measured'),
                caption: _kind == _IntegralApp.volume
                    ? l.t('ui.rotating_this_curve_about_the_chosen_axis_sweeps')
                    : l.t(
                        'ui.the_arc_length_adds_the_hypotenuses_of_tiny_step',
                      ),
              ),
          ],
        ],
      ],
    );
  }

  double _plotLo() {
    final a = double.tryParse(_a.text.trim()) ?? 0;
    final b = double.tryParse(_b.text.trim()) ?? 1;
    final lo = math.min(a, b);
    final hi = math.max(a, b);
    final pad = math.max((hi - lo) * 0.25, 0.5);
    return lo - pad;
  }

  double _plotHi() {
    final a = double.tryParse(_a.text.trim()) ?? 0;
    final b = double.tryParse(_b.text.trim()) ?? 1;
    final lo = math.min(a, b);
    final hi = math.max(a, b);
    final pad = math.max((hi - lo) * 0.25, 0.5);
    return hi + pad;
  }
}
