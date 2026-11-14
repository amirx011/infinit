import 'package:flutter/material.dart';

import '../app_state.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

/// Derivative workspace: orders 1–3 are revealed progressively, with a picker
/// for orders 4–10. Equations such as `x^2+y^2=4` are differentiated implicitly.
class DerivativePage extends StatefulWidget {
  const DerivativePage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<DerivativePage> createState() => _DerivativePageState();
}

class _DerivativePageState extends State<DerivativePage> {
  final _input = TextEditingController(text: 'x^3*sin(x)');
  final _xv = TextEditingController(text: 'x');
  final _firstKey = GlobalKey();
  final _secondKey = GlobalKey();
  final _thirdKey = GlobalKey();
  final _higherKey = GlobalKey();
  SolveResult? _first;
  SolveResult? _second;
  SolveResult? _third;
  SolveResult? _higher;
  int _higherOrder = 4;
  String? _error;
  bool _busy = false;

  static const _examples = <String>[
    'x^3*sin(x)',
    'exp(x)*cos(x)',
    'ln(x)/x',
    'x*sin(x)',
    'tan(x)',
    'x^2+y^2=4',
  ];

  @override
  void didUpdateWidget(covariant DerivativePage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _first = null;
      _second = null;
      _third = null;
      _higher = null;
      _error = null;
    }
  }

  @override
  void dispose() {
    _input.dispose();
    _xv.dispose();
    super.dispose();
  }

  GlobalKey _keyFor(int order) => order == 1
      ? _firstKey
      : order == 2
      ? _secondKey
      : order == 3
      ? _thirdKey
      : _higherKey;

  Future<void> _compute(int order) async {
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
      setState(() => _error = l.t('ui.enter_an_expression'));
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
      // A new computation invalidates the requested order and the higher
      // orders below it, so no stale answer can survive a failed run.
      if (order == 1) {
        _first = null;
        _second = null;
        _third = null;
        _higher = null;
      } else if (order == 2) {
        _second = null;
        _third = null;
        _higher = null;
      } else if (order == 3) {
        _third = null;
        _higher = null;
      } else {
        _higher = null;
      }
    });
    try {
      final r = await engine.derivativeAsync(
        text,
        x: _xv.text.trim().isEmpty ? 'x' : _xv.text.trim(),
        order: order,
        lang: l.isFa ? 'fa' : 'en',
      );
      if (!mounted) return;
      setState(() {
        if (order == 1) {
          _first = r;
        } else if (order == 2) {
          _second = r;
        } else if (order == 3) {
          _third = r;
        } else {
          _higher = r;
          _higherOrder = order;
        }
        _busy = false;
      });
      revealWidget(_keyFor(order));
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
    final showGraphs = SettingsScope.maybeOf(context)?.showGraphs ?? true;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                icon: Icons.drive_file_rename_outline_rounded,
                title: l.t('ui.derivatives'),
                subtitle: l.t(
                  'ui.derivatives_of_order_1_10_explicit_ast_walk_chai',
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
                  hintText: l.t('ui.e_g_x_3_sin_x_or_x_2_y_2_4'),
                  prefixIcon: Icon(
                    Icons.edit_outlined,
                    size: 20,
                    color: c.muted,
                  ),
                ),
                onSubmitted: (_) => _compute(1),
              ),
              const SizedBox(height: 12),
              SizedBox(
                width: 110,
                child: TextField(
                  controller: _xv,
                  textDirection: TextDirection.ltr,
                  textAlign: TextAlign.left,
                  style: const TextStyle(fontFamily: 'monospace', fontSize: 14),
                  decoration: InputDecoration(
                    labelText: l.t('ui.variable'),
                    isDense: true,
                  ),
                ),
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
                        _compute(1);
                      },
                    ),
                ],
              ),
              const SizedBox(height: 16),
              FilledButton.icon(
                onPressed: _busy ? null : () => _compute(1),
                icon: _busy
                    ? const SizedBox(
                        width: 17,
                        height: 17,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          color: Colors.white,
                        ),
                      )
                    : const Icon(
                        Icons.drive_file_rename_outline_rounded,
                        size: 19,
                      ),
                label: Text(
                  _busy
                      ? l.t('ui.differentiating')
                      : l.t('ui.compute_first_derivative'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.a_derivative_is_a_local_rate'),
          idea: l.t('ui.the_derivative_is_the_slope_of_the_tangent_line'),
          points: [
            l.t('ui.use_the_chain_rule_whenever_the_variable_sits_in'),
            l.t('ui.for_an_equation_such_as_x_y_4_differentiate_both'),
          ],
          example: const ExView(
            plain: "d/dx[x^3*sin(x)] = 3x^2*sin(x) + x^3*cos(x)",
            pretty: "d/dx [x³ sin x] = 3x² sin x + x³ cos x",
            latex: r"\frac{d}{dx}\left(x^{3}\sin x\right) = 3x^{2}\sin x + x^{3}\cos x",
          ),
          exampleCaption: l.t(
            'ui.the_product_rule_keeps_the_first_factor_and_diff',
          ),
          pitfall: l.t('ui.the_derivative_of_a_product_is_not_the_product_o'),
        ),
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
        if (_first != null) ...[
          const SizedBox(height: 18),
          Container(
            key: _firstKey,
            child: ResultView(result: _first!, l10n: l),
          ),
          if (showGraphs &&
              !_first!.implicit &&
              _first!.normalized != null &&
              _first!.solution != null) ...[
            const SizedBox(height: 16),
            MultiCurvePlot(
              engine: widget.engine,
              l10n: l,
              variable: _first!.variable.isEmpty ? 'x' : _first!.variable,
              curves: [
                PlotCurveSpec(
                  expression: _first!.normalized!.plain,
                  color: c.accent,
                  label: 'f(x)',
                ),
                PlotCurveSpec(
                  expression: _first!.solution!.plain,
                  color: c.accentAlt,
                  label: 'f′(x)',
                ),
              ],
              xmin: -8,
              xmax: 8,
              title: l.t('ui.function_and_derivative'),
              caption: l.t(
                'ui.the_derivative_is_the_slope_of_the_original_curv',
              ),
            ),
          ],
          const SizedBox(height: 14),
          Align(
            alignment: AlignmentDirectional.centerStart,
            child: OutlinedButton.icon(
              onPressed: _busy ? null : () => _compute(2),
              icon: const Icon(Icons.exposure_plus_2_rounded, size: 18),
              label: Text(l.t('ui.compute_second_derivative')),
            ),
          ),
        ],
        if (_second != null) ...[
          const SizedBox(height: 18),
          SectionLabel(
            l.t('ui.second_derivative'),
            icon: Icons.exposure_plus_2_rounded,
          ),
          const SizedBox(height: 10),
          Container(
            key: _secondKey,
            child: ResultView(result: _second!, l10n: l),
          ),
          const SizedBox(height: 14),
          Align(
            alignment: AlignmentDirectional.centerStart,
            child: OutlinedButton.icon(
              onPressed: _busy ? null : () => _compute(3),
              icon: const Icon(Icons.looks_3_rounded, size: 18),
              label: Text(l.t('ui.compute_third_derivative')),
            ),
          ),
        ],
        if (_third != null) ...[
          const SizedBox(height: 18),
          SectionLabel(l.t('ui.third_derivative'), icon: Icons.looks_3_rounded),
          const SizedBox(height: 10),
          Container(
            key: _thirdKey,
            child: ResultView(result: _third!, l10n: l),
          ),
        ],
        if (_third != null && _higher == null) ...[
          const SizedBox(height: 14),
          Align(
            alignment: AlignmentDirectional.centerStart,
            child: Text(
              l.t('ui.higher_order_4_10'),
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ),
          const SizedBox(height: 8),
          Wrap(
            spacing: 7,
            runSpacing: 7,
            children: [
              for (var order = 4; order <= 10; order++)
                OutlinedButton(
                  onPressed: _busy ? null : () => _compute(order),
                  child: Text('$order'),
                ),
            ],
          ),
        ],
        if (_higher != null) ...[
          const SizedBox(height: 18),
          SectionLabel(
            l.t('ui.derivative_order_label', {'order': _higherOrder}),
            icon: Icons.functions_rounded,
          ),
          const SizedBox(height: 10),
          Container(
            key: _higherKey,
            child: ResultView(result: _higher!, l10n: l),
          ),
          const SizedBox(height: 14),
          Wrap(
            spacing: 7,
            runSpacing: 7,
            children: [
              for (var order = 4; order <= 10; order++)
                OutlinedButton(
                  onPressed: _busy || order == _higherOrder
                      ? null
                      : () => _compute(order),
                  child: Text('$order'),
                ),
            ],
          ),
        ],
      ],
    );
  }
}
