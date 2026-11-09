import 'package:flutter/material.dart';

import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

enum _TrajectoryMode { envelope, orthogonal, oblique }

class EnvelopePage extends StatefulWidget {
  const EnvelopePage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
    this.orthogonalOnly = false,
    this.obliqueOnly = false,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  /// Dedicated orthogonal-trajectory entry: the family still has a parameter,
  /// but the page only asks for the perpendicular family and hides the envelope
  /// switch so the two workflows do not blur together.
  final bool orthogonalOnly;

  /// Dedicated oblique-trajectory entry: the crossing angle is a first-class
  /// input and the envelope switch is hidden.
  final bool obliqueOnly;

  @override
  State<EnvelopePage> createState() => _EnvelopePageState();
}

class _EnvelopePageState extends State<EnvelopePage> {
  final _input = TextEditingController(text: 'y = c*x + c^2');
  final _cv = TextEditingController(text: 'c');
  final _xv = TextEditingController(text: 'x');
  final _yv = TextEditingController(text: 'y');
  final _angle = TextEditingController(text: '45');
  final _resultKey = GlobalKey();
  bool _orthogonal = false;
  SolveResult? _result;
  String? _error;
  bool _busy = false;

  static const _envelopeExamples = <String>[
    'y = c*x + c^2',
    'y = c*x - c^2',
    'x^2 + y^2 = c^2',
    'y = c/x',
    '(y - c)^2 = x',
  ];

  static const _orthogonalExamples = <String>[
    'x^2 + y^2 = c^2',
    'y = c*x',
    'y = c/x',
    'x = c',
    'y = c',
  ];

  static const _obliqueExamples = <String>[
    'y = c*x',
    'x^2 + y^2 = c^2',
    'y = c/x',
    'y = c',
    'x = c',
  ];

  _TrajectoryMode get _mode {
    if (widget.obliqueOnly) return _TrajectoryMode.oblique;
    if (widget.orthogonalOnly) return _TrajectoryMode.orthogonal;
    return _orthogonal ? _TrajectoryMode.orthogonal : _TrajectoryMode.envelope;
  }

  List<String> get _examples {
    switch (_mode) {
      case _TrajectoryMode.oblique:
        return _obliqueExamples;
      case _TrajectoryMode.orthogonal:
        return _orthogonalExamples;
      case _TrajectoryMode.envelope:
        return _envelopeExamples;
    }
  }

  @override
  void initState() {
    super.initState();
    _orthogonal = widget.orthogonalOnly;
    if (widget.orthogonalOnly) _input.text = 'x^2 + y^2 = c^2';
    if (widget.obliqueOnly) _input.text = 'y = c*x';
  }

  @override
  void didUpdateWidget(covariant dynamic oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _result = null;
      _error = null;
    }
  }

  @override
  void dispose() {
    _input.dispose();
    _cv.dispose();
    _xv.dispose();
    _yv.dispose();
    _angle.dispose();
    super.dispose();
  }

  double? _angleValue() {
    final raw = _angle.text.trim().replaceAll('°', '').replaceAll('−', '-');
    final value = double.tryParse(raw);
    if (value == null || !value.isFinite || value <= 0 || value > 90) {
      return null;
    }
    return value;
  }

  Future<void> _solve() async {
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
      setState(() => _error = l.t('ui.enter_a_family_of_curves'));
      return;
    }
    final mode = _mode;
    final angle = _angleValue();
    if (mode == _TrajectoryMode.oblique && angle == null) {
      setState(
        () =>
            _error = l.t('ui.the_crossing_angle_must_be_a_number_between_0_an'),
      );
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final c = _cv.text.trim().isEmpty ? 'c' : _cv.text.trim();
      final x = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
      final y = _yv.text.trim().isEmpty ? 'y' : _yv.text.trim();
      final lang = l.isFa ? 'fa' : 'en';
      final r = mode == _TrajectoryMode.oblique
          ? await engine.obliqueAsync(
              text,
              c: c,
              angle: angle!,
              x: x,
              y: y,
              lang: lang,
            )
          : await engine.envelopeAsync(
              text,
              c: c,
              orthogonal: mode == _TrajectoryMode.orthogonal,
              x: x,
              y: y,
              lang: lang,
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
    final c = context.colors;
    final mode = _mode;
    final oblique = mode == _TrajectoryMode.oblique;
    final orthogonal = mode == _TrajectoryMode.orthogonal;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                tone: c.creative,
                icon: oblique
                    ? Icons.straighten_rounded
                    : orthogonal
                    ? Icons.swap_calls_rounded
                    : Icons.join_inner_rounded,
                title: oblique
                    ? l.t('ui.oblique_trajectories')
                    : orthogonal
                    ? l.t('ui.orthogonal_trajectories')
                    : l.t('ui.envelope_orthogonal_trajectories'),
                subtitle: oblique
                    ? l.t('ui.differentiate_the_family_eliminate_its_parameter')
                    : orthogonal
                    ? l.t(
                        'ui.differentiate_the_family_eliminate_its_parameter_2',
                      )
                    : l.t(
                        'ui.eliminate_the_parameter_to_find_the_envelope_and',
                      ),
              ),
              const SizedBox(height: 18),
              TextField(
                controller: _input,
                textDirection: TextDirection.ltr,
                textAlign: TextAlign.left,
                style: const TextStyle(
                  fontFamily: 'DejaVu Sans Mono',
                  fontFamilyFallback: ['monospace', 'Vazirmatn'],
                  fontSize: 16,
                  height: 1.5,
                ),
                decoration: InputDecoration(
                  hintText: l.t('ui.e_g_y_c_x_c_2'),
                  prefixIcon: Icon(
                    Icons.timeline_rounded,
                    size: 20,
                    color: c.muted,
                  ),
                ),
                onSubmitted: (_) => _solve(),
              ),
              const SizedBox(height: 14),
              Text(
                l.t('ui.examples'),
                style: Theme.of(context).textTheme.bodySmall,
              ),
              const SizedBox(height: 8),
              Wrap(
                spacing: 7,
                runSpacing: 7,
                children: [
                  for (final ex in _examples)
                    ExampleChip(
                      label: ex,
                      onTap: () {
                        _input.text = ex;
                        _solve();
                      },
                    ),
                ],
              ),
              if (oblique) ...[
                const SizedBox(height: 14),
                Row(
                  crossAxisAlignment: CrossAxisAlignment.center,
                  children: [
                    Expanded(
                      flex: 2,
                      child: _smallField(
                        _angle,
                        l.t('ui.crossing_angle_degrees'),
                        '45',
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      flex: 3,
                      child: Text(
                        l.t(
                          'ui.the_trajectories_meet_every_family_member_at_thi',
                        ),
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ),
                  ],
                ),
              ],
              const SizedBox(height: 10),
              if (!widget.orthogonalOnly && !widget.obliqueOnly)
                SwitchListTile(
                  value: _orthogonal,
                  onChanged: (v) => setState(() {
                    _orthogonal = v;
                    _result = null;
                  }),
                  contentPadding: EdgeInsets.zero,
                  activeThumbColor: c.accent,
                  title: Text(
                    l.t('ui.orthogonal_trajectories'),
                    style: Theme.of(context).textTheme.bodyMedium,
                  ),
                  subtitle: Text(
                    l.t('ui.curves_that_intersect_the_family_at_right_angles'),
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                ),
              Theme(
                data: Theme.of(context)
                    .copyWith(dividerColor: Colors.transparent),
                child: ExpansionTile(
                  tilePadding: EdgeInsets.zero,
                  childrenPadding: const EdgeInsets.only(top: 4, bottom: 4),
                  leading: Icon(Icons.tune_rounded, size: 20, color: c.muted),
                  title: Text(
                    l.t('ui.advanced_options'),
                    style: Theme.of(context).textTheme.bodyMedium,
                  ),
                  children: [
                    Row(
                      children: [
                        Expanded(
                          child: _smallField(_cv, l.t('ui.parameter'), 'c'),
                        ),
                        const SizedBox(width: 10),
                        Expanded(
                          child: _smallField(_xv, l.t('ui.variable_x'), 'x'),
                        ),
                        const SizedBox(width: 10),
                        Expanded(
                          child: _smallField(_yv, l.t('ui.variable_y'), 'y'),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 10),
              FilledButton.icon(
                onPressed: _busy ? null : _solve,
                icon: _busy
                    ? const SizedBox(
                        width: 17,
                        height: 17,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          color: Colors.white,
                        ),
                      )
                    : const Icon(Icons.alt_route_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('ui.analysing')
                      : oblique
                      ? l.t('ui.find_oblique_trajectories')
                      : orthogonal
                      ? l.t('ui.find_orthogonal_trajectories')
                      : l.t('ui.analyse_family'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        _learningCard(l, mode),
        if (_error != null) ...[
          const SizedBox(height: 16),
          SoftCard(
            color: c.warningSoft,
            borderColor: c.warning.withValues(alpha: 0.4),
            padding: const EdgeInsets.all(16),
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Icon(Icons.error_outline_rounded, color: c.warning, size: 20),
                const SizedBox(width: 10),
                Expanded(
                  child: SelectableText(
                    _error!,
                    style: Theme.of(context).textTheme.bodyMedium,
                  ),
                ),
              ],
            ),
          ),
        ],
        if (_result != null) ...[
          const SizedBox(height: 18),
          Container(
            key: _resultKey,
            child: ResultView(
              result: _result!,
              l10n: l,
              beforeSteps: _fieldPlot(l, c),
            ),
          ),
        ],
      ],
    );
  }

  Widget _learningCard(L10n l, _TrajectoryMode mode) {
    if (mode == _TrajectoryMode.oblique) {
      return LearningCard(
        l10n: l,
        title: l.t('ui.how_oblique_trajectories_are_found'),
        idea: l.t('ui.a_curve_crosses_the_family_at_the_constant_angle'),
        points: [
          l.t('ui.differentiate_f_x_y_c_0_implicitly_to_get_the_fa'),
          l.t('ui.eliminate_c_substitute_m_t_m_tan_1_m_tan_and_sol'),
          l.t('ui.at_90_the_formula_reduces_to_y_1_y_the_orthogona'),
        ],
        example: const ExView(
          plain: 'y = c,  alpha = 45  =>  y = x + C',
          pretty: 'y = c,  α = 45°  ⇒  y = x + C',
          latex: r'y = c,\quad \alpha = 45^{\circ} \quad\Longrightarrow\quad y = x + C',
        ),
        exampleCaption: l.t(
          'ui.a_horizontal_family_has_slope_0_the_oblique_traj',
        ),
        pitfall: l.t('ui.the_formula_fails_when_the_trajectory_is_vertica'),
      );
    }
    if (mode == _TrajectoryMode.orthogonal) {
      return LearningCard(
        l10n: l,
        title: l.t('ui.how_orthogonal_trajectories_are_found'),
        idea: l.t('ui.two_curves_cross_at_a_right_angle_when_the_produ'),
        points: [
          l.t('ui.differentiate_f_x_y_c_0_implicitly_to_get_the_fa_2'),
          l.t('ui.eliminate_c_using_the_family_equation_then_set_y'),
        ],
        example: const ExView(
          plain: 'x^2 + y^2 = c^2  =>  y = C*x',
          pretty: 'x² + y² = c²  ⇒  y = Cx',
          latex: r'x^{2} + y^{2} = c^{2} \quad\Longrightarrow\quad y = Cx',
        ),
        exampleCaption: l.t(
          'ui.the_circles_have_slope_x_y_the_perpendicular_slo',
        ),
        pitfall: l.t('ui.a_vertical_tangent_has_no_finite_slope_the_solve'),
      );
    }
    return LearningCard(
      l10n: l,
      title: l.t('ui.envelopes_and_orthogonal_families'),
      idea: l.t('ui.an_envelope_is_a_curve_tangent_to_every_member_o'),
      points: [
        l.t('ui.to_find_an_envelope_eliminate_the_parameter_betw'),
        l.t('ui.for_orthogonal_trajectories_replace_y_by_1_y_in'),
      ],
      example: const ExView(
        plain: 'y = c*x + c^2  =>  envelope y = -x^2/4',
        pretty: 'y = cx + c²  ⇒  y = −x²/4',
        latex: r'y = cx + c^{2} \quad\Longrightarrow\quad y = -\frac{x^{2}}{4}',
      ),
      exampleCaption: l.t('ui.differentiate_with_respect_to_c_solve_for_c_x_2'),
      pitfall: l.t('ui.not_every_family_has_a_real_envelope_a_formal_el'),
    );
  }

  Widget? _fieldPlot(L10n l, OdeColors c) {
    final result = _result;
    if (result == null) return null;
    final slope = result.trajectorySlope ?? result.orthogonalSlope;
    if (slope == null) return null;
    final oblique = _mode == _TrajectoryMode.oblique;
    return FieldPlot(
      engine: widget.engine,
      // The field reader expects an equation, not a bare right-hand side, so
      // build y' = f(x, y) from the solved slope.
      expression:
          "${_yv.text.trim().isEmpty ? 'y' : _yv.text.trim()}' = "
          '${slope.plain}',
      l10n: l,
      xVariable: _xv.text.trim().isEmpty ? 'x' : _xv.text.trim(),
      yVariable: _yv.text.trim().isEmpty ? 'y' : _yv.text.trim(),
      xmin: -5,
      xmax: 5,
      ymin: -5,
      ymax: 5,
      title: oblique
          ? l.t('ui.oblique_trajectories_direction_field')
          : l.t('ui.orthogonal_trajectories_direction_field'),
      caption: oblique
          ? l.t('ui.the_arrows_are_the_slope_m_t_m_tan_1_m_tan_after')
          : l.t('ui.the_arrows_are_the_slope_y_1_f_x_y_after_elimina'),
      extraCurves: _familyOverlay(c),
    );
  }

  /// Overlays a few members of the original family on the trajectory field.
  /// The native sampler substitutes c, so each branch keeps its own constant.
  List<PlotCurveSpec> _familyOverlay(OdeColors c) {
    final result = _result;
    if (result == null || result.familyBranches.isEmpty) return const [];
    final param = _cv.text.trim().isEmpty ? 'c' : _cv.text.trim();
    const values = <double>[-2, -1, 1, 2];
    final palette = <Color>[c.info, c.success, c.warning, c.accentAlt];
    final curves = <PlotCurveSpec>[];
    for (final branch in result.familyBranches) {
      for (var i = 0; i < values.length; i++) {
        curves.add(
          PlotCurveSpec(
            expression: branch.plain,
            constants: {param: values[i]},
            color: palette[i % palette.length].withValues(alpha: 0.55),
            label: '$param = ${values[i].toStringAsFixed(0)}',
            dashed: true,
            width: 1.5,
          ),
        );
      }
    }
    return curves;
  }

  Widget _smallField(
    TextEditingController controller,
    String label,
    String hint,
  ) {
    return TextField(
      controller: controller,
      textDirection: TextDirection.ltr,
      textAlign: TextAlign.left,
      style: const TextStyle(fontFamily: 'monospace', fontSize: 14),
      decoration: InputDecoration(
        labelText: label,
        hintText: hint,
        isDense: true,
      ),
    );
  }
}
