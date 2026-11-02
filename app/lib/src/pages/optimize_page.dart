// Optimisation on a closed interval.
//
// The page compares the stationary points with the two endpoints, which is the
// exhaustive list of places an extreme value can hide on [a, b].
import 'package:flutter/material.dart';

import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class OptimizePage extends StatefulWidget {
  const OptimizePage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<OptimizePage> createState() => _OptimizePageState();
}

class _OptimizePageState extends State<OptimizePage> {
  final _input = TextEditingController(text: 'x^3 - 3*x');
  final _xv = TextEditingController(text: 'x');
  final _xmin = TextEditingController(text: '-2');
  final _xmax = TextEditingController(text: '2');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String? _fieldExpression;
  String _fieldVariable = 'x';
  double _fieldMin = -2;
  double _fieldMax = 2;

  static const _examples = <String>[
    'x^3 - 3*x',
    'x^2 - 4*x + 5',
    'x^4 - 2*x^2',
    '-x^2 + 4*x',
    'x + 4/x',
    'sin(x)',
  ];

  @override
  void didUpdateWidget(covariant OptimizePage oldWidget) {
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
    final lo = _bound(_xmin, -2);
    final hi = _bound(_xmax, 2);
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
      final r = await engine.optimizeAsync(
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
                icon: Icons.vertical_align_bottom_rounded,
                title: l.t('Optimisation', 'مسایل بهینه‌سازی'),
                subtitle: l.t(
                  'Largest and smallest value of a function on a closed interval.',
                  'بزرگ‌ترین و کوچک‌ترین مقدار یک تابع روی یک بازهٔ بسته.',
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
                    : const Icon(Icons.vertical_align_bottom_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('Comparing candidates…', 'در حال مقایسهٔ کاندیدها…')
                      : l.t('Find the extreme values', 'یافتن مقادیر اکسترمم'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t(
            'A closed interval leaves nothing to chance',
            'بازهٔ بسته هیچ‌چیز را به شانس نمی‌سپارد',
          ),
          idea: l.t(
            'On a closed bounded interval a continuous function always reaches its extremes, and they can only occur at a stationary point inside the interval or at one of the two endpoints. Comparing that short list is the whole method.',
            'روی بازهٔ بستهٔ کراندار، تابع پیوسته همیشه به اکسترمم‌هایش می‌رسد و این مقادیر فقط در نقطهٔ سکون داخل بازه یا در یکی از دو سر بازه ممکن است رخ دهند. مقایسهٔ همین فهرست کوتاه، تمام روش است.',
          ),
          points: [
            l.t(
              "Solving f'(x) = 0 finds the interior candidates.",
              'حل معادلهٔ f′(x) = 0 کاندیدهای داخلی را می‌یابد.',
            ),
            l.t(
              'The endpoints are candidates too, even though f\' need not vanish there.',
              'دو سر بازه هم کاندید هستند، هرچند مشتق در آن‌ها لازم نیست صفر شود.',
            ),
            l.t(
              'The largest value on the list is the maximum and the smallest is the minimum.',
              'بزرگ‌ترین مقدار این فهرست بیشینه و کوچک‌ترین مقدار آن کمینه است.',
            ),
          ],
          pitfall: l.t(
            'Skipping the endpoints is the classic mistake; the extreme value often sits exactly there.',
            'فراموش کردن دو سر بازه اشتباه کلاسیک است؛ مقدار اکسترمم اغلب دقیقاً همان‌جاست.',
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
    final maxX = exToDouble(r.maximumAt);
    final maxY = exToDouble(r.maximum);
    final minX = exToDouble(r.minimumAt);
    final minY = exToDouble(r.minimum);
    return CurvePlot(
      engine: widget.engine,
      expression: _fieldExpression!,
      variable: _fieldVariable,
      l10n: l,
      xmin: _fieldMin,
      xmax: _fieldMax,
      height: 320,
      title: l.t('Objective and its candidates', 'تابع هدف و کاندیدهای آن'),
      guides: [?maxX, ?minX],
      markers: [
        if (maxX != null && maxY != null)
          PlotMarker(
            x: maxX,
            y: maxY,
            color: c.success,
            kind: PlotMarkerKind.point,
          ),
        if (minX != null && minY != null)
          PlotMarker(
            x: minX,
            y: minY,
            color: c.warning,
            kind: PlotMarkerKind.point,
          ),
      ],
      caption: l.t(
        'The guide lines mark where the extreme values occur: the top marker is the maximum and the bottom marker is the minimum on this interval.',
        'خط‌های راهنما محل وقوع اکسترمم‌ها را نشان می‌دهند: نشانگر بالا بیشینه و نشانگر پایین کمینهٔ این بازه است.',
      ),
    );
  }
}
