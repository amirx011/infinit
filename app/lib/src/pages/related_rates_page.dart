// Related rates.
//
// Two quantities tied by a relation move together. Differentiating the relation
// with respect to time turns that tie into a statement about their rates, and
// the page lets the student supply the instant so the answer becomes a number.
import 'package:flutter/material.dart';

import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class RelatedRatesPage extends StatefulWidget {
  const RelatedRatesPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<RelatedRatesPage> createState() => _RelatedRatesPageState();
}

class _RelatedRatesPageState extends State<RelatedRatesPage> {
  final _relation = TextEditingController(text: 'x^2 + y^2 = 25');
  final _varQ = TextEditingController(text: 'x');
  final _varW = TextEditingController(text: 'y');
  final _time = TextEditingController(text: 't');
  final _qValue = TextEditingController();
  final _wValue = TextEditingController();
  final _givenRate = TextEditingController();
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String? _fieldRelation;

  static const _examples = <String>[
    'x^2 + y^2 = 25',
    'x^2 + y^2 = z^2',
    'x*y = 12',
    'x^3 + y^3 = 9',
    'y = x^2',
  ];

  @override
  void didUpdateWidget(covariant RelatedRatesPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _result = null;
      _error = null;
    }
  }

  @override
  void dispose() {
    _relation.dispose();
    _varQ.dispose();
    _varW.dispose();
    _time.dispose();
    _qValue.dispose();
    _wValue.dispose();
    _givenRate.dispose();
    super.dispose();
  }

  double? _number(TextEditingController c) {
    final text = c.text.trim().replaceAll('−', '-');
    if (text.isEmpty) return null;
    return double.tryParse(text);
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
    final text = _relation.text.trim();
    if (text.isEmpty) {
      setState(
        () => _error = l.t('ui.enter_the_relation_between_the_quantities'),
      );
      return;
    }
    final q = _varQ.text.trim().isEmpty ? 'x' : _varQ.text.trim();
    final w = _varW.text.trim().isEmpty ? 'y' : _varW.text.trim();
    final t = _time.text.trim().isEmpty ? 't' : _time.text.trim();
    setState(() {
      _busy = true;
      _error = null;
      _fieldRelation = text;
    });
    try {
      final r = await engine.relatedRatesAsync(
        text,
        variable: q,
        rateVariable: w,
        time: t,
        lang: l.isFa ? 'fa' : 'en',
        qValue: _number(_qValue),
        wValue: _number(_wValue),
        givenRate: _number(_givenRate),
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
        _error = l.t('Engine error: $e', 'خطا در اجرای موتور: $e');
        _busy = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final l = widget.l10n;
    final r = _result;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                icon: Icons.speed_rounded,
                title: l.t('ui.related_rates'),
                subtitle: l.t('ui.differentiate_a_relation_with_respect_to_time_to'),
              ),
              const SizedBox(height: 18),
              ExpressionField(
                controller: _relation,
                hint: l.t('ui.e_g_x_2_y_2_25'),
                onSubmitted: _run,
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  LabelledField(
                    controller: _varQ,
                    label: l.t('ui.requested'),
                    width: 115,
                  ),
                  LabelledField(
                    controller: _varW,
                    label: l.t('ui.known_rate_of'),
                    width: 130,
                  ),
                  LabelledField(
                    controller: _time,
                    label: l.t('ui.time_variable'),
                    width: 120,
                  ),
                ],
              ),
              const SizedBox(height: 14),
              Text(
                l.t('ui.optional_the_instant_and_the_known_rate'),
                style: Theme.of(context).textTheme.bodySmall,
              ),
              const SizedBox(height: 8),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  LabelledField(
                    controller: _qValue,
                    label: _varQ.text.trim().isEmpty
                        ? 'x ='
                        : '${_varQ.text.trim()} =',
                    width: 100,
                  ),
                  LabelledField(
                    controller: _wValue,
                    label: _varW.text.trim().isEmpty
                        ? 'y ='
                        : '${_varW.text.trim()} =',
                    width: 100,
                  ),
                  LabelledField(
                    controller: _givenRate,
                    label: l.t('ui.known_rate'),
                    width: 120,
                  ),
                ],
              ),
              const SizedBox(height: 14),
              ExampleRow(
                examples: _examples,
                l10n: l,
                onPick: (ex) {
                  _relation.text = ex;
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
                    : const Icon(Icons.speed_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('ui.differentiating')
                      : l.t('ui.relate_the_rates'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('ui.time_hides_inside_both_quantities'),
          idea: l.t('ui.a_relation_such_as_x_y_25_describes_a_shape_but'),
          points: [
            l.t('ui.differentiate_every_term_attaching_d_dt_to_each'),
            l.t('ui.substitute_the_instantaneous_values_only_after_d'),
            l.t('ui.solve_the_resulting_linear_equation_for_the_rate'),
          ],
          pitfall: l.t('ui.substituting_the_numbers_first_turns_the_relatio'),
        ),
        if (_error != null) ...[
          const SizedBox(height: 16),
          ErrorCard(message: _error!),
        ],
        if (r != null && _fieldRelation != null) ...[
          const SizedBox(height: 16),
          _plot(r),
        ],
        if (r != null) ...[
          const SizedBox(height: 18),
          Container(key: _resultKey, child: ResultView(result: r, l10n: l)),
        ],
      ],
    );
  }

  Widget _plot(SolveResult r) {
    final l = widget.l10n;
    final c = context.colors;
    final q = _varQ.text.trim().isEmpty ? 'x' : _varQ.text.trim();
    final w = _varW.text.trim().isEmpty ? 'y' : _varW.text.trim();
    final qv = _number(_qValue);
    final wv = _number(_wValue);
    final coefficient = exToDouble(r.slope);

    // When the instant is known the relation becomes a local straight-line
    // model: the tangent to the level curve at that point. Its slope is the
    // coefficient itself, which is exactly the rate ratio.
    if (coefficient == null || qv == null || wv == null) {
      return SoftCard(
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Icon(Icons.insights_outlined, size: 19, color: c.muted),
            const SizedBox(width: 10),
            Expanded(
              child: Text(
                l.t('ui.fill_in_the_two_values_and_the_known_rate_to_see'),
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ),
          ],
        ),
      );
    }

    final window = plotWindowAround([qv], fallback: 5);
    final localLine = sampleLine(
      slope: coefficient,
      intercept: wv - coefficient * qv,
      xmin: window.$1,
      xmax: window.$2,
      color: c.accent,
      label: l.t(
        'slope = ${formatReal(coefficient)}',
        'شیب = ${formatReal(coefficient)}',
      ),
    );

    return MathPlot(
      height: 300,
      legend: true,
      spec: PlotSpec(
        series: [localLine],
        xMin: window.$1,
        xMax: window.$2,
        markers: [
          PlotMarker(
            x: qv,
            y: wv,
            color: c.success,
            kind: PlotMarkerKind.point,
          ),
        ],
        xLabel: q,
        yLabel: w,
      ),
      caption: l.t('ui.at_the_chosen_instant_the_relation_behaves_local'),
    );
  }
}
