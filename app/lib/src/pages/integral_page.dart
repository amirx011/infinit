import 'package:flutter/material.dart';

import '../app_state.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

/// Integral workspace: single / double / triple, each in indefinite or
/// definite form (definite mode asks for the two bounds of every variable).
class IntegralPage extends StatefulWidget {
  const IntegralPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<IntegralPage> createState() => _IntegralPageState();
}

class _IntegralPageState extends State<IntegralPage> {
  final _input = TextEditingController(text: '2*x*cos(x^2)');
  final _xv = TextEditingController(text: 'x');
  final _resultKey = GlobalKey();
  final _lo = <String, TextEditingController>{
    'x': TextEditingController(text: '0'),
    'y': TextEditingController(text: '0'),
    'z': TextEditingController(text: '0'),
  };
  final _hi = <String, TextEditingController>{
    'x': TextEditingController(text: '1'),
    'y': TextEditingController(text: '1'),
    'z': TextEditingController(text: '1'),
  };
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  int _dimension = 1;
  bool _definite = false;

  static const _examples = <int, List<String>>{
    1: [
      '2*x*cos(x^2)',
      'x*exp(x)',
      'x*ln(x)',
      'exp(-x^2)',
      '1/(x^2-x+1)',
      'sin(x)^2',
    ],
    2: ['x*y', 'x^2+y^2', 'sin(x)*cos(y)', 'exp(-(x^2+y^2))', 'x*exp(y)'],
    3: ['x*y*z', 'x^2+y^2+z^2', 'exp(-(x^2+y^2+z^2))', 'sin(x)*y*z'],
  };

  List<String> get _vars => const ['x', 'y', 'z'].take(_dimension).toList();

  @override
  void didUpdateWidget(covariant IntegralPage oldWidget) {
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
    for (final c in [..._lo.values, ..._hi.values]) {
      c.dispose();
    }
    super.dispose();
  }

  void _setDimension(int value) {
    if (value == _dimension) return;
    setState(() {
      _dimension = value;
      _result = null;
      _error = null;
      final ex = _examples[value]!;
      _input.text = ex.first;
    });
  }

  void _setDefinite(bool value) {
    if (value == _definite) return;
    setState(() {
      _definite = value;
      _result = null;
      _error = null;
    });
  }

  Future<void> _solve() async {
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
    final singleVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    final vars = _dimension == 1 ? <String>[singleVar] : _vars;
    final lower = <String>[];
    final upper = <String>[];
    if (_definite) {
      for (final v in vars) {
        final lo = _lo[v]!.text.trim();
        final hi = _hi[v]!.text.trim();
        if (lo.isEmpty || hi.isEmpty) {
          setState(
            () => _error = l.t('ui.enter_both_bounds_for_every_variable'),
          );
          return;
        }
        lower.add(lo);
        upper.add(hi);
      }
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final r = await engine.integralAsync(
        text,
        x: singleVar,
        vars: vars,
        dimension: _dimension,
        definite: _definite,
        lower: lower,
        upper: upper,
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
    final showGraphs = SettingsScope.maybeOf(context)?.showGraphs ?? true;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                tone: c.success,
                icon: Icons.integration_instructions_rounded,
                title: l.t('ui.integrals'),
                subtitle: l.t(
                  'ui.single_double_and_triple_integrals_indefinite_or',
                ),
              ),
              const SizedBox(height: 18),
              Text(
                l.t('ui.order'),
                style: Theme.of(context).textTheme.bodySmall,
              ),
              const SizedBox(height: 8),
              SegmentedButton<int>(
                segments: [
                  ButtonSegment(
                    value: 1,
                    icon: const Icon(Icons.looks_one_outlined, size: 17),
                    label: Text(l.t('ui.single')),
                  ),
                  ButtonSegment(
                    value: 2,
                    icon: const Icon(Icons.looks_two_outlined, size: 17),
                    label: Text(l.t('ui.double')),
                  ),
                  ButtonSegment(
                    value: 3,
                    icon: const Icon(Icons.looks_3_outlined, size: 17),
                    label: Text(l.t('ui.triple')),
                  ),
                ],
                selected: {_dimension},
                showSelectedIcon: false,
                onSelectionChanged: (s) => _setDimension(s.first),
              ),
              const SizedBox(height: 16),
              Text(
                l.t('ui.type'),
                style: Theme.of(context).textTheme.bodySmall,
              ),
              const SizedBox(height: 8),
              SegmentedButton<bool>(
                segments: [
                  ButtonSegment(
                    value: false,
                    icon: const Icon(Icons.all_inclusive_rounded, size: 17),
                    label: Text(l.t('ui.indefinite')),
                  ),
                  ButtonSegment(
                    value: true,
                    icon: const Icon(Icons.straighten_rounded, size: 17),
                    label: Text(l.t('ui.definite')),
                  ),
                ],
                selected: {_definite},
                showSelectedIcon: false,
                onSelectionChanged: (s) => _setDefinite(s.first),
              ),
              if (_definite) ...[
                const SizedBox(height: 16),
                Text(
                  l.t('ui.bounds'),
                  style: Theme.of(context).textTheme.bodySmall,
                ),
                const SizedBox(height: 8),
                Wrap(
                  spacing: 10,
                  runSpacing: 10,
                  children: [
                    for (final v in _vars)
                      _BoundPair(
                        variable: v,
                        lower: _lo[v]!,
                        upper: _hi[v]!,
                        l10n: l,
                      ),
                  ],
                ),
              ],
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
                  hintText: l.t('ui.e_g_x_exp_x'),
                  prefixIcon: Icon(
                    Icons.functions_rounded,
                    size: 20,
                    color: c.muted,
                  ),
                ),
                onSubmitted: (_) => _solve(),
              ),
              if (_dimension == 1) ...[
                const SizedBox(height: 12),
                SizedBox(
                  width: 110,
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
              ] else ...[
                const SizedBox(height: 10),
                Wrap(
                  spacing: 6,
                  runSpacing: 6,
                  children: [
                    for (final v in _vars)
                      Container(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 10,
                          vertical: 5,
                        ),
                        decoration: BoxDecoration(
                          color: c.accentSoft,
                          borderRadius: BorderRadius.circular(8),
                          border: Border.all(
                            color: c.accent.withValues(alpha: 0.25),
                          ),
                        ),
                        child: Text(
                          'd$v',
                          style: TextStyle(
                            fontFamily: 'Vazirmatn',
                            fontSize: 12,
                            fontWeight: FontWeight.w600,
                            color: c.accent,
                          ),
                        ),
                      ),
                  ],
                ),
              ],
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
                  for (final ex in _examples[_dimension]!)
                    ExampleChip(
                      label: ex,
                      onTap: () {
                        _input.text = ex;
                        _solve();
                      },
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
                    : const Icon(
                        Icons.integration_instructions_rounded,
                        size: 19,
                      ),
                label: Text(
                  _busy ? l.t('ui.integrating') : l.t('ui.compute_integral'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.integration_is_accumulation'),
          idea: l.t('ui.a_definite_integral_adds_up_signed_area_an_indef'),
          points: [
            l.t('ui.substitution_reverses_the_chain_rule_look_for_an'),
            l.t('ui.for_definite_integrals_apply_the_bounds_after_fi'),
          ],
          example: const ExView(
            plain: 'integral 2*x*cos(x^2) dx = sin(x^2) + C',
            pretty: '∫ 2x cos(x²) dx = sin(x²) + C',
            latex: r'\int 2x\cos\left(x^{2}\right)\,dx = \sin\left(x^{2}\right) + C',
          ),
          exampleCaption: l.t(
            'ui.with_u_x_du_2x_dx_the_integral_becomes_cos_u_du',
          ),
          pitfall: l.t('ui.an_indefinite_integral_without_c_is_incomplete_a'),
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
          if (showGraphs &&
              _result!.ok &&
              _result!.normalized != null &&
              _dimension == 1) ...[
            const SizedBox(height: 16),
            if (!_result!.definite && _result!.solution != null)
              MultiCurvePlot(
                engine: widget.engine,
                l10n: l,
                variable: _result!.variable.isEmpty ? 'x' : _result!.variable,
                curves: [
                  PlotCurveSpec(
                    expression: _result!.normalized!.plain,
                    color: c.accent,
                    label: l.t('ui.integrand_f_x'),
                  ),
                  PlotCurveSpec(
                    expression: _result!.solution!.plain,
                    color: c.accentAlt,
                    label: l.t('ui.antiderivative_f_x'),
                  ),
                ],
                xmin: -8,
                xmax: 8,
                title: l.t('ui.integrand_and_antiderivative'),
                caption: l.t(
                  'ui.f_is_one_antiderivative_of_f_its_slope_at_every',
                ),
              )
            else
              CurvePlot(
                engine: widget.engine,
                l10n: l,
                expression: _result!.normalized!.plain,
                variable: _result!.variable.isEmpty ? 'x' : _result!.variable,
                title: l.t('ui.integrand_over_the_bounds'),
                caption: l.t(
                  'ui.the_definite_integral_is_the_signed_area_between',
                ),
              ),
          ],
        ],
      ],
    );
  }
}

/// Two small numeric fields for the lower/upper bound of one variable.
class _BoundPair extends StatelessWidget {
  const _BoundPair({
    required this.variable,
    required this.lower,
    required this.upper,
    required this.l10n,
  });

  final String variable;
  final TextEditingController lower;
  final TextEditingController upper;
  final L10n l10n;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    Widget field(TextEditingController controller, String label) => SizedBox(
      width: 74,
      child: TextField(
        controller: controller,
        textDirection: TextDirection.ltr,
        textAlign: TextAlign.center,
        style: const TextStyle(fontFamily: 'monospace', fontSize: 13.5),
        decoration: InputDecoration(
          labelText: label,
          isDense: true,
          contentPadding: const EdgeInsets.symmetric(
            horizontal: 8,
            vertical: 10,
          ),
        ),
      ),
    );
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
      decoration: BoxDecoration(
        color: c.cardAlt,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: c.line),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text(
            variable,
            style: const TextStyle(fontFamily: 'monospace', fontSize: 14),
          ),
          const SizedBox(width: 8),
          field(lower, l10n.t('ui.from')),
          const SizedBox(width: 6),
          Icon(Icons.arrow_forward_rounded, size: 15, color: c.muted),
          const SizedBox(width: 6),
          field(upper, l10n.t('ui.to')),
        ],
      ),
    );
  }
}
