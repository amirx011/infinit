import 'package:flutter/material.dart';

import '../app_state.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class WronskianPage extends StatefulWidget {
  const WronskianPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<WronskianPage> createState() => _WronskianPageState();
}

class _WronskianPageState extends State<WronskianPage> {
  final List<TextEditingController> _functions = [
    TextEditingController(text: 'sin(x)'),
    TextEditingController(text: 'cos(x)'),
  ];
  final _xv = TextEditingController(text: 'x');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;

  static const _exampleSets = <List<String>>[
    ['sin(x)', 'cos(x)'],
    ['1', 'x', 'x^2'],
    ['x', '2x'],
    ['exp(x)', 'exp(2x)'],
    ['1', 'x', 'x^2', 'x^3'],
  ];

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
    for (final c in _functions) {
      c.dispose();
    }
    _xv.dispose();
    super.dispose();
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
    final fns = _functions
        .map((c) => c.text.trim())
        .where((s) => s.isNotEmpty)
        .toList(growable: false);
    if (fns.isEmpty) {
      setState(() => _error = l.t('ui.enter_at_least_one_function'));
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final r = await engine.wronskianAsync(
        fns,
        x: _xv.text.trim().isEmpty ? 'x' : _xv.text.trim(),
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

  void _loadExample(List<String> fns) {
    setState(() {
      for (final c in _functions) {
        c.dispose();
      }
      _functions
        ..clear()
        ..addAll(fns.map((f) => TextEditingController(text: f)));
    });
    _solve();
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
                tone: c.accentAlt,
                icon: Icons.grid_on_rounded,
                title: l.t('ui.wronskian_linear_independence'),
                subtitle: l.t(
                  'ui.build_the_wronskian_determinant_and_test_whether',
                ),
              ),
              const SizedBox(height: 18),
              for (var i = 0; i < _functions.length; i++) ...[
                Row(
                  children: [
                    Container(
                      width: 26,
                      height: 26,
                      alignment: Alignment.center,
                      decoration: BoxDecoration(
                        color: c.cardAlt,
                        borderRadius: BorderRadius.circular(8),
                        border: Border.all(color: c.line),
                      ),
                      child: Text(
                        '${i + 1}',
                        style: TextStyle(
                          fontFamily: 'Vazirmatn',
                          fontSize: 12,
                          color: c.muted,
                        ),
                      ),
                    ),
                    const SizedBox(width: 10),
                    Expanded(
                      child: TextField(
                        controller: _functions[i],
                        textDirection: TextDirection.ltr,
                        textAlign: TextAlign.left,
                        style: const TextStyle(
                          fontFamily: 'DejaVu Sans Mono',
                          fontFamilyFallback: ['monospace', 'Vazirmatn'],
                          fontSize: 15,
                        ),
                        decoration: InputDecoration(
                          hintText: l.t('ui.e_g_sin_x'),
                          isDense: true,
                          contentPadding: const EdgeInsets.symmetric(
                            horizontal: 14,
                            vertical: 13,
                          ),
                        ),
                        onSubmitted: (_) => _solve(),
                      ),
                    ),
                    if (_functions.length > 1)
                      IconButton(
                        tooltip: l.t('ui.remove_function'),
                        icon: Icon(
                          Icons.remove_circle_outline_rounded,
                          size: 20,
                          color: c.muted,
                        ),
                        onPressed: () {
                          setState(() {
                            _functions.removeAt(i).dispose();
                          });
                        },
                      ),
                  ],
                ),
                const SizedBox(height: 9),
              ],
              const SizedBox(height: 2),
              Row(
                children: [
                  TextButton.icon(
                    onPressed: _functions.length >= 6
                        ? null
                        : () => setState(
                            () =>
                                _functions.add(TextEditingController(text: '')),
                          ),
                    icon: const Icon(Icons.add_rounded, size: 18),
                    label: Text(l.t('ui.add_function')),
                  ),
                  const SizedBox(width: 8),
                  SizedBox(
                    width: 96,
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
                  for (final set in _exampleSets)
                    ExampleChip(
                      label: set.join(' , '),
                      onTap: () => _loadExample(set),
                    ),
                ],
              ),
              const SizedBox(height: 16),
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
                    : const Icon(Icons.calculate_rounded, size: 19),
                label: Text(
                  _busy ? l.t('ui.computing') : l.t('ui.compute_wronskian'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.what_the_wronskian_tests'),
          idea: l.t('ui.the_wronskian_packs_the_functions_and_their_deri'),
          points: [
            l.t('ui.the_test_is_decisive_for_solutions_of_a_linear_h'),
            l.t('ui.a_zero_wronskian_at_a_single_point_does_not_by_i'),
          ],
          example: const ExView(
            plain: 'W(exp(x), exp(2x)) = exp(3x) != 0',
            pretty: 'W(eˣ, e²ˣ) = e³ˣ ≠ 0',
            latex: r'W\left(e^{x}, e^{2x}\right) = e^{3x} \neq 0',
          ),
          exampleCaption: l.t(
            'ui.the_determinant_never_vanishes_so_the_two_expone',
          ),
          pitfall: l.t('ui.do_not_confuse_w_0_at_one_point_with_w_0_on_the'),
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
        if (_result != null) ...[
          const SizedBox(height: 18),
          Container(
            key: _resultKey,
            child: ResultView(result: _result!, l10n: l),
          ),
          if (showGraphs && _result!.ok) ...[
            const SizedBox(height: 16),
            _functionsPlot(context, l, c),
          ],
        ],
      ],
    );
  }

  Widget _functionsPlot(BuildContext context, L10n l, OdeColors c) {
    final palette = <Color>[
      c.accent,
      c.accentAlt,
      c.success,
      c.warning,
      c.info,
      c.creative,
    ];
    final curves = <PlotCurveSpec>[];
    for (var i = 0; i < _functions.length; i++) {
      final expression = _functions[i].text.trim();
      if (expression.isEmpty) continue;
      curves.add(
        PlotCurveSpec(
          expression: expression,
          color: palette[curves.length % palette.length],
          label: 'f${curves.length + 1}(x)',
        ),
      );
    }
    if (curves.length < 2) return const SizedBox.shrink();
    return MultiCurvePlot(
      engine: widget.engine,
      l10n: l,
      variable: _xv.text.trim().isEmpty ? 'x' : _xv.text.trim(),
      curves: curves,
      xmin: -8,
      xmax: 8,
      title: l.t('ui.functions_under_test'),
      caption: l.t('ui.the_wronskian_is_built_from_these_curves_and_the'),
    );
  }
}
