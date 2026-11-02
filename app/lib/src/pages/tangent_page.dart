// Tangent and normal lines at a point.
//
// The plot draws the curve, the tangent and the normal together, which is the
// only way the perpendicularity claim becomes believable at a glance. The line
// slopes come from the exact symbolic values the engine returned.
import 'package:flutter/material.dart';

import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class TangentPage extends StatefulWidget {
  const TangentPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<TangentPage> createState() => _TangentPageState();
}

class _TangentPageState extends State<TangentPage> {
  final _input = TextEditingController(text: 'x^2');
  final _xv = TextEditingController(text: 'x');
  final _at = TextEditingController(text: '1');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String? _fieldExpression;
  String _fieldVariable = 'x';
  double _fieldAt = 1;

  static const _examples = <String>[
    'x^2',
    'x^3 - 3*x',
    'sin(x)',
    'exp(x)',
    'ln(x)',
    '1/x',
  ];

  @override
  void didUpdateWidget(covariant TangentPage oldWidget) {
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
    _at.dispose();
    super.dispose();
  }

  Future<void> _run() async {
    final engine = widget.engine;
    final l = widget.l10n;
    if (engine == null) {
      setState(
        () => _error =
            widget.engineError ??
            l.t(
              'The computation engine is unavailable.',
              'موتور محاسبات در دسترس نیست.',
            ),
      );
      return;
    }
    final text = _input.text.trim();
    final at = _at.text.trim();
    if (text.isEmpty || at.isEmpty) {
      setState(
        () => _error = l.t(
          'Enter a function and a point.',
          'یک تابع و یک نقطه وارد کنید.',
        ),
      );
      return;
    }
    final xVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    setState(() {
      _busy = true;
      _error = null;
      _fieldExpression = text;
      _fieldVariable = xVar;
      _fieldAt = double.tryParse(at.replaceAll('−', '-')) ?? 0;
    });
    try {
      final r = await engine.tangentAsync(
        text,
        x: xVar,
        at: at,
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
                icon: Icons.show_chart_rounded,
                title: l.t('Tangent and normal', 'مماس و خط عمود'),
                subtitle: l.t(
                  'The tangent line and the perpendicular normal at a chosen point of a curve.',
                  'خط مماس و خط عمود بر آن در یک نقطهٔ دلخواه از منحنی.',
                ),
              ),
              const SizedBox(height: 18),
              ExpressionField(
                controller: _input,
                hint: l.t('e.g. x^2', 'مثلاً x^2'),
                onSubmitted: _run,
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  LabelledField(
                    controller: _xv,
                    label: l.t('Variable', 'متغیر'),
                    width: 100,
                  ),
                  LabelledField(
                    controller: _at,
                    label: l.t('At x = a', 'در x = a'),
                    width: 120,
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
                    : const Icon(Icons.show_chart_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('Computing…', 'در حال محاسبه…')
                      : l.t('Find the two lines', 'یافتن دو خط'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('One derivative, two lines', 'یک مشتق، دو خط'),
          idea: l.t(
            "The derivative evaluated at a gives the slope of the tangent. The normal is the line through the same point whose slope is the negative reciprocal, because perpendicular slopes multiply to -1.",
            'مشتق در نقطهٔ a شیب مماس را می‌دهد. خط عمود همان خطی است که از همان نقطه می‌گذرد و شیبش وارون منفی شیب مماس است، چون حاصل‌ضرب شیب دو خط عمود برابر −۱ است.',
          ),
          points: [
            l.t(
              'Point-slope form y = f(a) + f\'(a)(x − a) is the whole construction.',
              'شکل نقطه‌شیب y = f(a) + f′(a)(x − a) تمام ساختار مسئله است.',
            ),
            l.t(
              'A horizontal tangent has a vertical normal, whose slope does not exist.',
              'مماس افقی خط عمودی دارد که شیبش وجود ندارد.',
            ),
          ],
          pitfall: l.t(
            'The normal line is not the reciprocal of the function; it is the line with the reciprocal slope.',
            'خط عمود وارون تابع نیست؛ خطی است که شیبش وارون شیب مماس است.',
          ),
        ),
        if (_error != null) ...[
          const SizedBox(height: 16),
          ErrorCard(message: _error!),
        ],
        if (r != null && _fieldExpression != null) ...[
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
    final fA = exToDouble(r.value);
    final m = exToDouble(r.tangentSlope);
    final n = exToDouble(r.normalSlope);
    final window = plotWindowAround([_fieldAt, ?fA], fallback: 4);

    final series = <PlotSeries>[];
    if (m != null && fA != null) {
      series.add(
        sampleLine(
          slope: m,
          intercept: fA - m * _fieldAt,
          xmin: window.$1,
          xmax: window.$2,
          color: c.success,
          label: l.t('Tangent', 'مماس'),
        ),
      );
    }
    if (n != null && fA != null) {
      series.add(
        sampleLine(
          slope: n,
          intercept: fA - n * _fieldAt,
          xmin: window.$1,
          xmax: window.$2,
          color: c.creative,
          label: l.t('Normal', 'خط عمود'),
          dashed: true,
        ),
      );
    }

    return CurvePlot(
      engine: widget.engine,
      expression: _fieldExpression!,
      variable: _fieldVariable,
      l10n: l,
      xmin: window.$1,
      xmax: window.$2,
      height: 340,
      title: l.t('Curve, tangent and normal', 'منحنی، مماس و خط عمود'),
      extraSeries: series,
      guides: [_fieldAt],
      markers: fA == null
          ? const []
          : [
              PlotMarker(
                x: _fieldAt,
                y: fA,
                color: c.accent,
                kind: PlotMarkerKind.point,
              ),
            ],
      caption: n == null
          ? l.t(
              'The tangent is horizontal here, so the normal is the vertical dashed guide at the point.',
              'مماس در این نقطه افقی است، پس خط عمود همان راهنمای چین قائم در آن نقطه است.',
            )
          : l.t(
              'The solid line is the tangent and the dashed line is the normal; they meet at right angles at the point of tangency.',
              'خط توپر مماس و خط چین عمود است؛ این دو در نقطهٔ مماس با زاویهٔ قائمه یکدیگر را قطع می‌کنند.',
            ),
    );
  }
}
