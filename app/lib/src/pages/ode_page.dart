import 'package:flutter/material.dart';

import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class OdePage extends StatefulWidget {
  const OdePage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
    this.initialEquation,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  /// Pre-filled equation (used when a form card on the home screen is tapped).
  final String? initialEquation;

  @override
  State<OdePage> createState() => _OdePageState();
}

class _OdePageState extends State<OdePage> {
  late final TextEditingController _input = TextEditingController(
    text: widget.initialEquation ?? "y' = y",
  );
  final _xv = TextEditingController(text: 'x');
  final _yv = TextEditingController(text: 'y');
  final _particular = TextEditingController();
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String? _fieldEquation;
  String _fieldX = 'x';
  String _fieldY = 'y';

  static const _examples = <String>[
    "y' = y",
    "y' + y/x = x^2",
    "y' = x/y",
    "y' = y^2",
    "y' = 1 + y^2",
    "y' = (x+y)^2",
    "y' = x*y + y^2",
    "y' = y^2 - 2/x^2",
    "y = x*y' + (y')^2",
    "y = 2*x*y' + (y')^2",
    "(2x+y) + (x+2y)y' = 0",
    "(x+y)dx + (x+2y)dy = 0",
    "y'' - 3y' + 2y = 0",
    "y'' + y = 0",
    "y'' = 6x",
    "y'' = y'^2",
    "y'' + y' = x",
    "y''' = y''",
    "y'' = y*y'",
    "y'' + y*y' = 0",
    "y'' = y^2",
  ];

  @override
  void initState() {
    super.initState();
    if (const bool.fromEnvironment('INFINIT_DEMO')) {
      WidgetsBinding.instance.addPostFrameCallback((_) => _solve());
    }
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
    _xv.dispose();
    _yv.dispose();
    _particular.dispose();
    super.dispose();
  }

  Future<void> _solve() async {
    final engine = widget.engine;
    if (engine == null) {
      setState(
        () => _error =
            widget.engineError ??
            widget.l10n.t('ui.the_computation_engine_is_unavailable'),
      );
      return;
    }
    final text = _input.text.trim();
    if (text.isEmpty) {
      setState(
        () => _error = widget.l10n.t('ui.enter_an_equation_example_y_y'),
      );
      return;
    }
    final xVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    final yVar = _yv.text.trim().isEmpty ? 'y' : _yv.text.trim();
    setState(() {
      _busy = true;
      _error = null;
      _fieldEquation = text;
      _fieldX = xVar;
      _fieldY = yVar;
    });
    try {
      final result = await engine.solveOdeAsync(
        text,
        x: xVar,
        y: yVar,
        particular: _particular.text.trim(),
        lang: widget.l10n.isFa ? 'fa' : 'en',
      );
      if (!mounted) return;
      setState(() {
        _result = result;
        _busy = false;
      });
      revealWidget(_resultKey);
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _error = widget.l10n.t('Engine error: $e', 'خطا در اجرای موتور: $e');
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
                icon: Icons.functions_rounded,
                title: l.t('ui.differential_equation'),
                subtitle: l.t('ui.enter_an_equation_the_exact_solution_and_step_by'),
              ),
              const SizedBox(height: 18),
              TextField(
                controller: _input,
                textDirection: TextDirection.ltr,
                textAlign: TextAlign.left,
                style: const TextStyle(
                  fontFamily: 'DejaVu Sans Mono',
                  fontFamilyFallback: ['monospace', 'Vazirmatn'],
                  fontSize: 17,
                  height: 1.5,
                ),
                decoration: InputDecoration(
                  hintText: "y' = y    •    y'' - 3y' + 2y = 0",
                  prefixIcon: Icon(
                    Icons.edit_outlined,
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
              const SizedBox(height: 14),
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
                          child: _smallField(
                            _xv,
                            l.t('ui.independent_variable'),
                            'x',
                          ),
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: _smallField(
                            _yv,
                            l.t('ui.dependent_variable'),
                            'y',
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 12),
                    _smallField(
                      _particular,
                      l.t('ui.riccati_particular_solution_optional'),
                      l.t('ui.e_g_tan_x'),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 14),
              Row(
                children: [
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
                        : const Icon(Icons.play_arrow_rounded, size: 19),
                    label: Text(
                      _busy
                          ? l.t('ui.solving')
                          : l.t('ui.solve_equation'),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      l.t('ui.exact_symbolic_answers_non_elementary_integrals'),
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.what_a_general_solution_means'),
          idea: l.t('ui.a_first_order_equation_describes_a_whole_family'),
          points: [
            l.t('ui.classify_the_form_first_separable_linear_exact_b'),
            l.t('ui.differentiate_the_answer_and_substitute_it_back'),
            l.t('ui.higher_order_if_y_is_missing_set_p_y_if_x_is_mis'),
          ],
          example: const ExView(
            plain: "y' = y  =>  y = C*exp(x)",
            pretty: "y′ = y  ⇒  y = C eˣ",
            latex: r"y' = y \quad\Longrightarrow\quad y = C\,e^{x}",
          ),
          exampleCaption: l.t('ui.separable_separate_integrate_both_sides_then_sol'),
          pitfall: l.t('ui.dividing_by_y_loses_the_constant_solution_y_0_ch'),
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
        if (_result != null && _fieldEquation != null) ...[
          const SizedBox(height: 16),
          if ((_result?.order ?? 1) <= 1)
            FieldPlot(
              engine: widget.engine,
              expression: _fieldEquation!,
              l10n: l,
              xVariable: _fieldX,
              yVariable: _fieldY,
              title: l.t('ui.direction_field_solution_family'),
              caption: l.t('ui.each_arrow_is_the_local_slope_of_a_solution_the'),
            )
          else
            OdeSolutionPlot(
              engine: widget.engine,
              expression: _fieldEquation!,
              l10n: l,
              xVariable: _fieldX,
              yVariable: _fieldY,
              title: l.t('ui.solution_family_numeric_rk4'),
              caption: l.t('ui.higher_order_equations_have_no_single_slope_fiel'),
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

