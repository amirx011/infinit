// Series convergence: which test decides, the exact sum when known, and the
// numeric evidence from partial sums.
import 'package:flutter/material.dart';

import '../app_state.dart';
import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class SeriesPage extends StatefulWidget {
  const SeriesPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<SeriesPage> createState() => _SeriesPageState();
}

class _SeriesPageState extends State<SeriesPage> {
  final _input = TextEditingController(text: '1/n^2');
  final _nv = TextEditingController(text: 'n');
  final _start = TextEditingController(text: '1');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;

  static const _examples = <String>[
    '1/n^2',
    '1/n',
    '1/2^n',
    '(-1)^n/n',
    '1/n^0.5',
    'n/2^n',
    '(-1)^n/sqrt(n)',
    '1/n!',
    '2^n/n!',
    '(n/(2*n+1))^n',
  ];

  @override
  void didUpdateWidget(covariant SeriesPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _result = null;
      _error = null;
    }
  }

  @override
  void dispose() {
    _input.dispose();
    _nv.dispose();
    _start.dispose();
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
    final start = int.tryParse(_start.text.trim());
    if (text.isEmpty || start == null) {
      setState(
        () => _error = l.t('ui.enter_the_series_term_and_a_whole_start_index'),
      );
      return;
    }
    final nVar = _nv.text.trim().isEmpty ? 'n' : _nv.text.trim();
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final r = await engine.seriesAsync(
        text,
        n: nVar,
        start: start,
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
                icon: Icons.waves_rounded,
                title: l.t('ui.series_convergence'),
                subtitle: l.t(
                  'ui.decide_whether_an_infinite_sum_settles_and_find',
                ),
              ),
              const SizedBox(height: 18),
              ExpressionField(
                controller: _input,
                hint: l.t('ui.e_g_1_n_2'),
                onSubmitted: _run,
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  LabelledField(
                    controller: _nv,
                    label: l.t('ui.index_variable'),
                    width: 150,
                  ),
                  LabelledField(
                    controller: _start,
                    label: l.t('ui.start_index'),
                    width: 130,
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
                    : const Icon(Icons.waves_rounded, size: 19),
                label: Text(
                  _busy ? l.t('ui.computing') : l.t('ui.test_for_convergence'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.pick_the_test_that_fits_the_shape'),
          idea: l.t('ui.a_series_is_a_sequence_of_partial_sums_convergen'),
          points: [
            l.t('ui.geometric_a_constant_ratio_r_converges_exactly_w'),
            l.t('ui.p_series_1_n_p_converges_exactly_when_p_1'),
            l.t('ui.ratio_and_root_tests_compute_a_limit_less_than_1'),
            l.t('ui.alternating_if_a_n_decreases_to_0_the_series_con'),
          ],
          pitfall: l.t('ui.a_term_that_tends_to_0_is_necessary_but_not_suff'),
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
          if (showGraphs && r.ok && r.partialSums.length > 1) ...[
            const SizedBox(height: 16),
            _PartialSumPlot(
              sums: r.partialSums,
              start: int.tryParse(_start.text.trim()) ?? 1,
              color: c.accent,
              limitColor: c.accentAlt,
              l10n: l,
            ),
          ],
        ],
      ],
    );
  }
}

/// Draws S_k against k. A convergent series flattens towards its limit; a
/// divergent one keeps climbing or oscillating, which is exactly the numeric
/// evidence the page talks about.
class _PartialSumPlot extends StatelessWidget {
  const _PartialSumPlot({
    required this.sums,
    required this.start,
    required this.color,
    required this.limitColor,
    required this.l10n,
  });

  final List<double> sums;
  final int start;
  final Color color;
  final Color limitColor;
  final L10n l10n;

  @override
  Widget build(BuildContext context) {
    final xs = <double>[];
    final ys = <double>[];
    for (var i = 0; i < sums.length; i++) {
      final y = sums[i];
      if (!y.isFinite) continue;
      xs.add((start + i).toDouble());
      ys.add(y);
    }
    if (ys.length < 2) return const SizedBox.shrink();
    final last = ys.last;
    final series = <PlotSeries>[
      PlotSeries(xs: xs, ys: ys, color: color, width: 2.4),
      PlotSeries(
        xs: [xs.first, xs.last],
        ys: [last, last],
        color: limitColor,
        width: 1.4,
        dashed: true,
        label: l10n.t('ui.last_partial_sum'),
      ),
    ];
    return Container(
      decoration: BoxDecoration(
        color: context.colors.card,
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: context.colors.line),
      ),
      padding: const EdgeInsets.all(18),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(
            l10n.t('ui.partial_sums_s_k'),
            style: Theme.of(context).textTheme.titleMedium
                ?.copyWith(fontSize: 14),
          ),
          const SizedBox(height: 12),
          MathPlot(
            spec: PlotSpec(series: series, xLabel: 'k', yLabel: 'S_k'),
            height: 280,
            caption: l10n.t(
              'ui.each_point_adds_one_more_term_flattening_suggest',
            ),
          ),
        ],
      ),
    );
  }
}
