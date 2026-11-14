import 'package:flutter/material.dart';

import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

/// Limits: value at a point from both sides (or one side), with the limit laws,
/// L'Hôpital and squeeze recorded as steps.
class LimitPage extends StatefulWidget {
  const LimitPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<LimitPage> createState() => _LimitPageState();
}

class _LimitPageState extends State<LimitPage> {
  final _input = TextEditingController(text: 'sin(x)/x');
  final _xv = TextEditingController(text: 'x');
  final _point = TextEditingController(text: '0');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String _side = 'both';
  String? _plottedExpr;
  String _plottedVar = 'x';
  String _plottedPoint = '0';

  static const _examples = <(String, String)>[
    ('sin(x)/x', '0'),
    ('(1-cos(x))/x^2', '0'),
    ('(1+1/x)^x', 'inf'),
    ('x*ln(x)', '0'),
    ('(x^2-4)/(x-2)', '2'),
    ('(3*x^2+2)/(x^2-1)', 'inf'),
    ('tan(x)/x', '0'),
    ('1/x', '0'),
  ];

  @override
  void didUpdateWidget(covariant LimitPage oldWidget) {
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
    _point.dispose();
    super.dispose();
  }

  double? get _guideX {
    final raw = _plottedPoint
        .replaceAll('−', '-')
        .replaceAll(' ', '')
        .toLowerCase();
    if (raw == 'inf' || raw == '+inf' || raw == 'infinity') return null;
    return double.tryParse(raw);
  }

  (double, double) get _plotWindow {
    final x = _guideX;
    if (x == null) return (-12, 12);
    return (x - 4, x + 4);
  }

  Future<void> _compute() async {
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
    final variable = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    final point = _point.text.trim().isEmpty ? '0' : _point.text.trim();
    setState(() {
      _busy = true;
      _error = null;
      _plottedExpr = text;
      _plottedVar = variable;
      _plottedPoint = point;
    });
    try {
      final r = await engine.limitAsync(
        text,
        x: variable,
        point: point,
        side: _side,
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
    final c = context.colors;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                tone: c.warning,
                icon: Icons.linear_scale_rounded,
                title: l.t('ui.limits'),
                subtitle: l.t(
                  'ui.limit_laws_on_the_expression_tree_l_h_pital_for',
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
                  hintText: l.t('ui.e_g_sin_x_x'),
                  prefixIcon: Icon(
                    Icons.edit_outlined,
                    size: 20,
                    color: c.muted,
                  ),
                ),
                onSubmitted: (_) => _compute(),
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  SizedBox(
                    width: 100,
                    child: TextField(
                      controller: _xv,
                      textDirection: TextDirection.ltr,
                      textAlign: TextAlign.left,
                      style: const TextStyle(
                        fontFamily: 'monospace',
                        fontSize: 14,
                      ),
                      decoration: InputDecoration(
                        labelText: l.t('ui.variable'),
                        isDense: true,
                      ),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: TextField(
                      controller: _point,
                      textDirection: TextDirection.ltr,
                      textAlign: TextAlign.left,
                      style: const TextStyle(
                        fontFamily: 'monospace',
                        fontSize: 14,
                      ),
                      decoration: InputDecoration(
                        labelText: l.t('ui.approaches'),
                        hintText: l.t('ui.0_pi_2_inf'),
                        isDense: true,
                      ),
                      onSubmitted: (_) => _compute(),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              SegmentedButton<String>(
                segments: [
                  ButtonSegment(
                    value: 'both',
                    label: Text(l.t('ui.two_sided')),
                    icon: const Icon(Icons.compare_arrows_rounded, size: 17),
                  ),
                  ButtonSegment(
                    value: 'left',
                    label: Text(l.t('ui.left')),
                    icon: const Icon(Icons.west_rounded, size: 17),
                  ),
                  ButtonSegment(
                    value: 'right',
                    label: Text(l.t('ui.right')),
                    icon: const Icon(Icons.east_rounded, size: 17),
                  ),
                ],
                selected: {_side},
                onSelectionChanged: (v) => setState(() => _side = v.first),
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
                      label: ex.$1,
                      onTap: () {
                        _input.text = ex.$1;
                        _point.text = ex.$2;
                        _compute();
                      },
                    ),
                ],
              ),
              const SizedBox(height: 16),
              FilledButton.icon(
                onPressed: _busy ? null : _compute,
                icon: _busy
                    ? const SizedBox(
                        width: 17,
                        height: 17,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          color: Colors.white,
                        ),
                      )
                    : const Icon(Icons.linear_scale_rounded, size: 19),
                label: Text(
                  _busy ? l.t('ui.computing') : l.t('ui.compute_the_limit'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.a_limit_describes_approach'),
          idea: l.t('ui.the_limit_at_a_point_depends_on_nearby_values_no'),
          points: [
            l.t('ui.try_direct_substitution_first_0_0_or_signals_tha'),
            l.t('ui.for_jumps_absolute_values_and_pieces_compare_the'),
          ],
          example: const ExView(
            plain: 'lim x->0 sin(x)/x = 1',
            pretty: 'lim x→0 sin x / x = 1',
            latex: r'\lim_{x\to 0}\frac{\sin x}{x} = 1',
          ),
          exampleCaption: l.t(
            'ui.the_expression_is_0_0_at_x_0_the_limit_exists_ev',
          ),
          pitfall: l.t('ui.l_h_pital_applies_to_indeterminate_quotients_onl'),
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
        if (_result != null && _plottedExpr != null) ...[
          const SizedBox(height: 16),
          CurvePlot(
            engine: widget.engine,
            expression: _plottedExpr!,
            variable: _plottedVar,
            l10n: l,
            xmin: _plotWindow.$1,
            xmax: _plotWindow.$2,
            height: 280,
            color: c.accent,
            title: l.t('ui.near_point', {
              'var': _plottedVar,
              'point': _plottedPoint,
            }),
            caption: l.t('ui.the_dashed_guide_marks_the_point_being_approache'),
            guides: [
              if (_guideX != null && !(_result!.limitInfinite)) _guideX!,
            ],
            asymptotes: [
              if (_guideX != null && _result!.limitInfinite) _guideX!,
            ],
          ),
        ],
        if (_result != null) ...[
          const SizedBox(height: 18),
          Container(
            key: _resultKey,
            child: ResultView(result: _result!, l10n: l),
          ),
        ],
      ],
    );
  }
}
