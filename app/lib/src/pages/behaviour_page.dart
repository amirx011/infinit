// Function behaviour analysis: where f rises, falls, bends and turns.
//
// The page pairs the native analysis with one picture that carries the whole
// story: the curve, the critical points as markers, the inflection points, and
// dashed guides at every special x.
import 'package:flutter/material.dart';

import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class BehaviourPage extends StatefulWidget {
  const BehaviourPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<BehaviourPage> createState() => _BehaviourPageState();
}

class _BehaviourPageState extends State<BehaviourPage> {
  final _input = TextEditingController(text: 'x^3 - 3*x');
  final _xv = TextEditingController(text: 'x');
  final _xmin = TextEditingController(text: '-3');
  final _xmax = TextEditingController(text: '3');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String? _fieldExpression;
  String _fieldVariable = 'x';
  double _fieldMin = -3;
  double _fieldMax = 3;

  static const _examples = <String>[
    'x^3 - 3*x',
    'x^4 - 2*x^2',
    'x/(1+x^2)',
    'exp(-x^2)',
    'x*exp(-x)',
    'x^3',
  ];

  @override
  void didUpdateWidget(covariant BehaviourPage oldWidget) {
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

  double _bound(TextEditingController c, double fallback) =>
      double.tryParse(c.text.trim().replaceAll('−', '-')) ?? fallback;

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
    if (text.isEmpty) {
      setState(
        () => _error = l.t('Enter an expression.', 'یک عبارت وارد کنید.'),
      );
      return;
    }
    final xVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    final lo = _bound(_xmin, -3);
    final hi = _bound(_xmax, 3);
    if (!(lo < hi)) {
      setState(
        () => _error = l.t(
          'The interval must satisfy a < b.',
          'باید a < b باشد.',
        ),
      );
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
      _fieldExpression = text;
      _fieldVariable = xVar;
      _fieldMin = lo;
      _fieldMax = hi;
    });
    try {
      final r = await engine.behaviourAsync(
        text,
        x: xVar,
        xmin: lo,
        xmax: hi,
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
                icon: Icons.insights_rounded,
                title: l.t('Function behaviour', 'تحلیل رفتار تابع'),
                subtitle: l.t(
                  'First and second derivative tests, monotonicity intervals and inflection points.',
                  'آزمون مشتق اول و دوم، بازه‌های یکنوایی و نقطه‌های عطف.',
                ),
              ),
              const SizedBox(height: 18),
              ExpressionField(
                controller: _input,
                hint: l.t('e.g. x^3 - 3*x', 'مثلاً x^3 - 3*x'),
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
                    controller: _xmin,
                    label: l.t('From a', 'از a'),
                    width: 110,
                  ),
                  LabelledField(
                    controller: _xmax,
                    label: l.t('To b', 'تا b'),
                    width: 110,
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
                    : const Icon(Icons.insights_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('Analysing…', 'در حال تحلیل…')
                      : l.t('Analyse the function', 'تحلیل تابع'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t(
            'Two derivatives, three questions',
            'دو مشتق، سه پرسش',
          ),
          idea: l.t(
            'The first derivative answers where the function rises and falls; the second answers which way it bends. Reading both signs together is what identifies a maximum, a minimum and an inflection point.',
            'مشتق اول پاسخ می‌دهد تابع کجا صعودی و کجا نزولی است؛ مشتق دوم می‌گوید به کدام سمت خم می‌شود. خواندن هم‌زمان این دو علامت، بیشینه، کمینه و نقطهٔ عطف را مشخص می‌کند.',
          ),
          points: [
            l.t(
              "A sign change of f' from + to - is a local maximum, and from - to + is a local minimum.",
              'تغییر علامت f′ از مثبت به منفی بیشینهٔ محلی و از منفی به مثبت کمینهٔ محلی است.',
            ),
            l.t(
              "A sign change of f'' marks an inflection point, where concavity flips.",
              'تغییر علامت f″ نقطهٔ عطف را مشخص می‌کند؛ همان‌جا که تقعر عوض می‌شود.',
            ),
            l.t(
              "f'(c) = 0 on its own decides nothing; the sign on both sides is the evidence.",
              'صفر بودن f′(c) به‌تنهایی چیزی را تعیین نمی‌کند؛ شاهد واقعی علامت دو طرف است.',
            ),
          ],
          pitfall: l.t(
            'A stationary turning point such as x = 0 for x³ has f\' = 0 but is neither a maximum nor a minimum.',
            'نقطهٔ تخت مانند x = 0 برای x³ دارای f′ = 0 است ولی نه بیشینه است و نه کمینه.',
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
    final criticalX = [
      for (final p in r.critical)
        if (exToDouble(p.x) != null) exToDouble(p.x)!,
    ];
    final inflectionX = [
      for (final p in r.inflections)
        if (exToDouble(p.x) != null) exToDouble(p.x)!,
    ];
    return CurvePlot(
      engine: widget.engine,
      expression: _fieldExpression!,
      variable: _fieldVariable,
      l10n: l,
      xmin: _fieldMin,
      xmax: _fieldMax,
      height: 320,
      title: l.t('Curve, extrema and inflections', 'منحنی، اکسترمم‌ها و نقطه‌های عطف'),
      guides: [...criticalX, ...inflectionX],
      markers: [
        for (final p in r.critical)
          if (exToDouble(p.x) != null && exToDouble(p.value) != null)
            PlotMarker(
              x: exToDouble(p.x)!,
              y: exToDouble(p.value)!,
              color: p.kind == 'maximum' ? c.success : c.warning,
              kind: PlotMarkerKind.point,
            ),
        for (final p in r.inflections)
          if (exToDouble(p.x) != null && exToDouble(p.value) != null)
            PlotMarker(
              x: exToDouble(p.x)!,
              y: exToDouble(p.value)!,
              color: c.info,
              kind: PlotMarkerKind.hole,
            ),
      ],
      caption: l.t(
        'Dashed guides mark the critical points and inflection points. Filled markers are extrema; hollow markers are inflection points.',
        'خط‌های چین راهنما نقطه‌های بحرانی و عطف را نشان می‌دهند. نشانگرهای توپر اکسترمم و نشانگرهای توخالی نقطهٔ عطف‌اند.',
      ),
    );
  }
}
