import 'package:flutter/material.dart';

import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

/// Continuity at a point, following the definition itself:
/// the left limit and the right limit must both exist and be equal, and the
/// value of the function must equal that common limit.
class ContinuityPage extends StatefulWidget {
  const ContinuityPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<ContinuityPage> createState() => _ContinuityPageState();
}

class _ContinuityPageState extends State<ContinuityPage> {
  final _input = TextEditingController(text: '(x^2-4)/(x-2)');
  final _xv = TextEditingController(text: 'x');
  final _point = TextEditingController(text: '2');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String? _plottedExpr;
  String _plottedVar = 'x';
  String _plottedPoint = '0';

  static const _examples = <(String, String)>[
    ('x^2', '2'),
    ('(x^2-4)/(x-2)', '2'),
    ('1/x', '0'),
    ('abs(x)', '0'),
    ('x/abs(x)', '0'),
    ('sin(x)/x', '0'),
    ('sin(1/x)', '0'),
    ('(x^2-1)/(x-1)', '1'),
  ];

  @override
  void didUpdateWidget(covariant ContinuityPage oldWidget) {
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
    if (x == null) return (-8, 8);
    return (x - 3, x + 3);
  }

  Future<void> _compute() async {
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
      setState(() => _error = l.t('ui.enter_a_function'));
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
      final r = await engine.continuityAsync(
        text,
        x: variable,
        point: point,
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
                tone: c.accentAlt,
                icon: Icons.timeline_rounded,
                title: l.t('ui.continuity'),
                subtitle: l.t(
                  'ui.a_function_is_continuous_at_a_point_when_the_lef',
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
                  hintText: l.t('ui.e_g_x_2_4_x_2'),
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
                        labelText: l.t('ui.point'),
                        hintText: '0, 1, pi/2',
                        isDense: true,
                      ),
                      onSubmitted: (_) => _compute(),
                    ),
                  ),
                ],
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
                    : const Icon(Icons.timeline_rounded, size: 19),
                label: Text(
                  _busy ? l.t('ui.checking') : l.t('ui.check_continuity'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.three_conditions_for_continuity'),
          idea: l.t('ui.a_function_is_continuous_at_a_point_when_it_is_d'),
          points: [
            l.t('ui.compute_the_left_and_right_limits_independently'),
            l.t('ui.a_removable_gap_can_be_repaired_by_redefining_on'),
          ],
          example: const ExView(
            plain: 'f(x) = (x^2-4)/(x-2)  has a removable gap at x = 2',
            pretty: 'f(x) = (x² − 4)/(x − 2)  ⇒  gap at x = 2',
            latex:
                r'f(x)=\frac{x^{2}-4}{x-2}\ \text{has a removable gap at }x=2',
          ),
          exampleCaption: l.t(
            'ui.the_limit_is_4_but_f_2_is_undefined_so_the_funct',
          ),
          pitfall: l.t('ui.an_existing_limit_is_not_enough_the_value_f_a_mu'),
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
            height: 270,
            color: c.accent,
            title: l.t('ui.around_point', {
              'var': _plottedVar,
              'point': _plottedPoint,
            }),
            caption: l.t('ui.continuity_is_a_visual_agreement_the_curve_must'),
            guides: [?_guideX],
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
