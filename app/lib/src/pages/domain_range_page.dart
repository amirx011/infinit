import 'package:flutter/material.dart';

import '../charts.dart';
import '../interval_text.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

/// Domain and range of a real function, estimated by the native scan and
/// illustrated with a curve, domain bands and range bands on the same axes.
class DomainRangePage extends StatefulWidget {
  const DomainRangePage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<DomainRangePage> createState() => _DomainRangePageState();
}

class _DomainRangePageState extends State<DomainRangePage> {
  final _input = TextEditingController(text: '1/(x^2-1)');
  final _xv = TextEditingController(text: 'x');
  final _xmin = TextEditingController(text: '-12');
  final _xmax = TextEditingController(text: '12');
  final _resultKey = GlobalKey();
  DomainResult? _result;
  String? _error;
  bool _busy = false;

  static const _examples = <(String, String, String)>[
    ('1/(x^2-1)', '-12', '12'),
    ('sqrt(x^2-1)', '-12', '12'),
    ('sqrt(4-x^2)', '-12', '12'),
    ('ln(x)', '-4', '12'),
    ('exp(x)', '-12', '12'),
    ('atan(x)', '-12', '12'),
    ('1/sqrt(x)', '-4', '12'),
    ('sin(x)/x', '-12', '12'),
    ('x^3-3*x', '-6', '6'),
  ];

  @override
  void didUpdateWidget(covariant DomainRangePage oldWidget) {
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
    _xmin.dispose();
    _xmax.dispose();
    super.dispose();
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
      setState(() => _error = l.t('ui.enter_a_function'));
      return;
    }
    final xmin = double.tryParse(_xmin.text.trim().replaceAll('−', '-'));
    final xmax = double.tryParse(_xmax.text.trim().replaceAll('−', '-'));
    if (xmin == null || xmax == null || !(xmin < xmax)) {
      setState(
        () =>
            _error = l.t('ui.the_sampling_window_needs_two_numbers_with_the_l'),
      );
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final r = await engine.domainAsync(
        text,
        x: _xv.text.trim().isEmpty ? 'x' : _xv.text.trim(),
        xmin: xmin,
        xmax: xmax,
      );
      if (!mounted) return;
      setState(() {
        _result = r;
        _busy = false;
        if (!r.ok) {
          _error = r.error.isEmpty ? l.t('ui.the_scan_failed') : r.error;
        }
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

  String _kindLabel(String kind, L10n l) {
    switch (kind) {
      case 'rational':
        return l.t('ui.rational');
      case 'root':
        return l.t('ui.radical');
      case 'logarithm':
        return l.t('ui.logarithmic');
      case 'trigonometric':
        return l.t('ui.trigonometric');
      case 'mixed':
        return l.t('ui.mixed');
      default:
        return l.t('ui.polynomial');
    }
  }

  @override
  Widget build(BuildContext context) {
    final l = widget.l10n;
    final c = context.colors;
    final result = _result;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                tone: c.info,
                icon: Icons.explore_rounded,
                title: l.t('ui.domain_range'),
                subtitle: l.t(
                  'ui.find_where_a_function_lives_and_which_values_it',
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
                  hintText: l.t('ui.e_g_1_x_2_1'),
                  prefixIcon: Icon(
                    Icons.functions_rounded,
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
                  const SizedBox(width: 10),
                  Expanded(
                    child: TextField(
                      controller: _xmin,
                      textDirection: TextDirection.ltr,
                      textAlign: TextAlign.left,
                      style: const TextStyle(
                        fontFamily: 'monospace',
                        fontSize: 14,
                      ),
                      decoration: InputDecoration(
                        labelText: l.t('ui.window_from'),
                        isDense: true,
                      ),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: TextField(
                      controller: _xmax,
                      textDirection: TextDirection.ltr,
                      textAlign: TextAlign.left,
                      style: const TextStyle(
                        fontFamily: 'monospace',
                        fontSize: 14,
                      ),
                      decoration: InputDecoration(
                        labelText: l.t('ui.window_to'),
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
                        _xmin.text = ex.$2;
                        _xmax.text = ex.$3;
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
                    : const Icon(Icons.explore_rounded, size: 19),
                label: Text(
                  _busy ? l.t('ui.scanning') : l.t('ui.find_domain_and_range'),
                ),
              ),
            ],
          ),
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
        if (result != null && result.ok) ...[
          const SizedBox(height: 18),
          Container(
            key: _resultKey,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                SoftCard(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      SectionLabel(
                        l.t('ui.one_picture_two_sets'),
                        icon: Icons.insights_rounded,
                        trailing: TypeBadge(
                          _kindLabel(result.kind, l),
                          icon: Icons.category_outlined,
                          tone: c.accentAlt,
                        ),
                      ),
                      const SizedBox(height: 14),
                      MathPlot(
                        key: const ValueKey('domain-chart'),
                        height: 330,
                        spec: PlotSpec(
                          series: [
                            PlotSeries(
                              xs: result.xs,
                              ys: result.ys,
                              color: c.accent,
                              label: 'f(${_xv.text.trim()})',
                            ),
                          ],
                          bands: [
                            for (final iv in result.domain)
                              PlotBand(interval: iv, color: c.accent),
                            for (final iv in result.range)
                              if (!(iv.loInfinite && iv.hiInfinite))
                                PlotBand(
                                  interval: iv,
                                  color: c.accentAlt,
                                  horizontal: true,
                                ),
                          ],
                          asymptotes: result.asymptotes,
                          markers: [
                            for (
                              var i = 0;
                              i < result.holes.length &&
                                  i < result.rangeHoles.length;
                              i++
                            )
                              PlotMarker(
                                x: result.holes[i],
                                y: result.rangeHoles[i],
                                color: c.accentAlt,
                                kind: PlotMarkerKind.hole,
                              ),
                          ],
                          xMin: double.tryParse(_xmin.text.trim()),
                          xMax: double.tryParse(_xmax.text.trim()),
                        ),
                        legend: false,
                      ),
                      const SizedBox(height: 12),
                      _BandLegend(l10n: l),
                    ],
                  ),
                ),
                const SizedBox(height: 12),
                LayoutBuilder(
                  builder: (context, constraints) {
                    final wide = constraints.maxWidth >= 620;
                    final domainCard = _SetCard(
                      title: l.t('ui.domain'),
                      subtitle: l.t('ui.allowed_inputs_on_the_x_axis'),
                      value: intervalsText(result.domain),
                      tone: c.accent,
                      icon: Icons.swap_horiz_rounded,
                      details: [
                        if (result.holes.isNotEmpty)
                          l.t('ui.punctures_at', {
                            'points': puncturedText(
                              result.holes,
                              emptySymbol: '·',
                            ),
                          }),
                        if (result.asymptotes.isNotEmpty)
                          l.t('ui.vertical_asymptotes_at', {
                            'points': puncturedText(
                              result.asymptotes,
                              emptySymbol: '·',
                            ),
                          }),
                      ],
                    );
                    final rangeCard = _SetCard(
                      title: l.t('ui.range'),
                      subtitle: l.t('ui.produced_outputs_on_the_y_axis'),
                      value: intervalsText(result.range),
                      tone: c.accentAlt,
                      icon: Icons.swap_vert_rounded,
                      details: [
                        l.t('ui.numeric_estimate_samples', {
                          'n': result.xs.length,
                        }),
                      ],
                    );
                    if (!wide) {
                      return Column(
                        children: [
                          domainCard,
                          const SizedBox(height: 12),
                          rangeCard,
                        ],
                      );
                    }
                    return IntrinsicHeight(
                      child: Row(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          Expanded(child: domainCard),
                          const SizedBox(width: 12),
                          Expanded(child: rangeCard),
                        ],
                      ),
                    );
                  },
                ),
              ],
            ),
          ),
        ],
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.domain_asks_where_range_asks_what_values'),
          idea: l.t('ui.the_domain_is_the_set_of_inputs_the_formula_acce'),
          points: [
            l.t('ui.denominators_forbid_zeros_even_roots_forbid_nega'),
            l.t('ui.for_a_one_to_one_function_the_range_of_f_is_the'),
            l.t('ui.open_circles_and_dashed_lines_mark_values_that_a'),
          ],
          example: const ExView(
            plain: 'y = sqrt(4 - x^2): domain [-2, 2], range [0, 2]',
            pretty: 'y = √(4 − x²)  →  D = [−2, 2],  R = [0, 2]',
            latex: r'y=\sqrt{4-x^2}\;\Rightarrow\;D=[-2,2],\;R=[0,2]',
          ),
          exampleCaption: l.t(
            'ui.the_semicircle_only_exists_between_2_and_2_and_i',
          ),
          pitfall: l.t('ui.a_removable_hole_belongs_to_neither_the_domain_n'),
        ),
      ],
    );
  }
}

class _BandLegend extends StatelessWidget {
  const _BandLegend({required this.l10n});

  final L10n l10n;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Wrap(
      spacing: 16,
      runSpacing: 8,
      children: [
        _LegendDot(color: c.accent, label: l10n.t('ui.domain_band_x')),
        _LegendDot(color: c.accentAlt, label: l10n.t('ui.range_band_y')),
        _LegendDot(
          color: c.warning,
          label: l10n.t('ui.asymptote'),
          outlined: true,
        ),
      ],
    );
  }
}

class _LegendDot extends StatelessWidget {
  const _LegendDot({
    required this.color,
    required this.label,
    this.outlined = false,
  });

  final Color color;
  final String label;
  final bool outlined;

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(
          width: 12,
          height: 12,
          decoration: BoxDecoration(
            color: outlined
                ? Colors.transparent
                : color.withValues(alpha: 0.35),
            borderRadius: BorderRadius.circular(3),
            border: Border.all(color: color, width: 1.2),
          ),
        ),
        const SizedBox(width: 6),
        Text(label, style: Theme.of(context).textTheme.bodySmall),
      ],
    );
  }
}

class _SetCard extends StatelessWidget {
  const _SetCard({
    required this.title,
    required this.subtitle,
    required this.value,
    required this.tone,
    required this.icon,
    required this.details,
  });

  final String title;
  final String subtitle;
  final String value;
  final Color tone;
  final IconData icon;
  final List<String> details;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    // Blend against the opaque card surface: a translucent fill would let the
    // card's own shadow bleed through and turn the panel muddy grey.
    return SoftCard(
      color: Color.alphaBlend(tone.withValues(alpha: 0.06), c.card),
      borderColor: tone.withValues(alpha: 0.30),
      padding: const EdgeInsets.all(18),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Container(
                width: 32,
                height: 32,
                decoration: BoxDecoration(
                  color: tone.withValues(alpha: 0.12),
                  borderRadius: BorderRadius.circular(10),
                ),
                child: Icon(icon, size: 18, color: tone),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(title, style: Theme.of(context).textTheme.titleMedium),
                    Text(
                      subtitle,
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          Directionality(
            textDirection: TextDirection.ltr,
            child: SelectableText(
              value,
              style: TextStyle(
                fontFamily: 'DejaVu Sans Mono',
                fontFamilyFallback: const ['monospace', 'Vazirmatn'],
                fontSize: 17,
                height: 1.5,
                fontWeight: FontWeight.w600,
                color: tone,
              ),
            ),
          ),
          for (final detail in details) ...[
            const SizedBox(height: 6),
            Text(detail, style: Theme.of(context).textTheme.bodySmall),
          ],
        ],
      ),
    );
  }
}
