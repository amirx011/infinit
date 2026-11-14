// Partial derivatives, the gradient, the Laplacian and directional derivatives.
import 'package:flutter/material.dart';

import '../app_state.dart';
import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../widgets.dart';

class PartialPage extends StatefulWidget {
  const PartialPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<PartialPage> createState() => _PartialPageState();
}

class _PartialPageState extends State<PartialPage> {
  final _input = TextEditingController(text: 'x^2*y + y^3');
  final _vars = TextEditingController(text: 'x, y');
  final _point = TextEditingController(text: '1, 2');
  final _direction = TextEditingController(text: '1, 1');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;

  static const _examples = <String>[
    'x^2*y + y^3',
    'x^2 + y^2',
    'x*y*exp(x)',
    'sin(x)*cos(y)',
    'ln(x^2 + y^2)',
    'x^3 - 3*x*y + y^3',
  ];

  @override
  void didUpdateWidget(covariant PartialPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _result = null;
      _error = null;
    }
  }

  @override
  void dispose() {
    _input.dispose();
    _vars.dispose();
    _point.dispose();
    _direction.dispose();
    super.dispose();
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
    final text = _input.text.trim();
    if (text.isEmpty) {
      setState(() => _error = l.t('ui.enter_a_function'));
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final r = await engine.partialAsync(
        text,
        vars: _vars.text.trim(),
        point: _point.text.trim(),
        direction: _direction.text.trim(),
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
    final showGraphs = SettingsScope.maybeOf(context)?.showGraphs ?? true;
    final vars = _vars.text
        .split(',')
        .map((v) => v.trim())
        .where((v) => v.isNotEmpty)
        .toList();
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                icon: Icons.grid_4x4_rounded,
                title: l.t('ui.partial_derivatives_gradient'),
                subtitle: l.t(
                  'ui.differentiate_a_multivariable_function_one_varia',
                ),
              ),
              const SizedBox(height: 18),
              ExpressionField(
                controller: _input,
                hint: l.t('ui.e_g_x_2_y_y_3'),
                onSubmitted: _run,
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  LabelledField(
                    controller: _vars,
                    label: l.t('ui.variables'),
                    width: 140,
                  ),
                  LabelledField(
                    controller: _point,
                    label: l.t('ui.point_a_b'),
                    width: 180,
                  ),
                  LabelledField(
                    controller: _direction,
                    label: l.t('ui.direction_optional'),
                    width: 190,
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
                    : const Icon(Icons.grid_4x4_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('ui.computing')
                      : l.t('ui.differentiate_and_evaluate'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.one_variable_moves_the_others_wait'),
          idea: l.t('ui.a_partial_derivative_treats_every_other_variable'),
          points: [
            l.t('ui.f_x_means_differentiate_with_respect_to_x_and_ho'),
            l.t('ui.the_directional_derivative_is_the_gradient_dotte'),
            l.t('ui.clairaut_for_a_smooth_function_the_mixed_partial'),
          ],
          pitfall: l.t('ui.forgetting_that_y_is_constant_is_the_usual_mista'),
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
          if (showGraphs && r.ok && vars.length >= 2) ...[
            const SizedBox(height: 16),
            SurfacePlot(
              engine: widget.engine,
              expression: _input.text.trim(),
              l10n: l,
              xVariable: vars[0],
              yVariable: vars[1],
              title: l.t('ui.contour_map_of_the_surface'),
              caption: l.t(
                'ui.colour_shows_the_value_of_f_x_y_warm_regions_are',
              ),
            ),
          ],
        ],
      ],
    );
  }
}
