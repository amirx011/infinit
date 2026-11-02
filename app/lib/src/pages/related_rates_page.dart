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
            l.t(
              'The computation engine is unavailable.',
              'موتور محاسبات در دسترس نیست.',
            ),
      );
      return;
    }
    final text = _relation.text.trim();
    if (text.isEmpty) {
      setState(
        () => _error = l.t(
          'Enter the relation between the quantities.',
          'رابطهٔ بین کمیت‌ها را وارد کنید.',
        ),
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
                title: l.t('Related rates', 'نرخ‌های مرتبط'),
                subtitle: l.t(
                  'Differentiate a relation with respect to time to link the rates of two quantities.',
                  'با مشتق‌گیری از یک رابطه نسبت به زمان، نرخ دو کمیت به هم مرتبط می‌شود.',
                ),
              ),
              const SizedBox(height: 18),
              ExpressionField(
                controller: _relation,
                hint: l.t(
                  'e.g. x^2 + y^2 = 25',
                  'مثلاً x^2 + y^2 = 25',
                ),
                onSubmitted: _run,
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  LabelledField(
                    controller: _varQ,
                    label: l.t('Requested', 'خواسته‌شده'),
                    width: 115,
                  ),
                  LabelledField(
                    controller: _varW,
                    label: l.t('Known rate of', 'نرخ معلوم'),
                    width: 130,
                  ),
                  LabelledField(
                    controller: _time,
                    label: l.t('Time variable', 'متغیر زمان'),
                    width: 120,
                  ),
                ],
              ),
              const SizedBox(height: 14),
              Text(
                l.t(
                  'Optional: the instant and the known rate',
                  'اختیاری: مقدار لحظه‌ای و نرخ معلوم',
                ),
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
                    label: l.t('known rate', 'نرخ معلوم'),
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
                      ? l.t('Differentiating…', 'در حال مشتق‌گیری…')
                      : l.t('Relate the rates', 'نرخ‌ها را مرتبط کن'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('Time hides inside both quantities', 'زمان درون هر دو کمیت پنهان است'),
          idea: l.t(
            'A relation such as x² + y² = 25 describes a shape, but if a point moves along it then x and y both depend on time. Differentiating the whole relation with respect to t applies the chain rule to each quantity and leaves an equation linking their rates.',
            'رابطه‌ای مثل x² + y² = 25 یک شکل را توصیف می‌کند، اما اگر نقطه‌ای روی آن حرکت کند، هم x و هم y به زمان وابسته‌اند. مشتق‌گیری از کل رابطه نسبت به t قاعدهٔ زنجیره‌ای را بر هر کمیت اعمال می‌کند و معادله‌ای می‌سازد که نرخ‌ها را به هم می‌بندد.',
          ),
          points: [
            l.t(
              'Differentiate every term, attaching d/dt to each quantity.',
              'از هر جمله مشتق بگیرید و به هر کمیت عامل d/dt بچسبانید.',
            ),
            l.t(
              'Substitute the instantaneous values only after differentiating, never before.',
              'مقادیر لحظه‌ای را فقط بعد از مشتق‌گیری جایگذاری کنید، نه پیش از آن.',
            ),
            l.t(
              'Solve the resulting linear equation for the rate you were asked about.',
              'معادلهٔ خطی حاصل را برای نرخی که خواسته شده حل کنید.',
            ),
          ],
          pitfall: l.t(
            'Substituting the numbers first turns the relation into a constant and every rate becomes zero.',
            'اگر اول اعداد را جایگذاری کنید، رابطه به یک ثابت تبدیل می‌شود و همهٔ نرخ‌ها صفر می‌شوند.',
          ),
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
                l.t(
                  'Fill in the two values and the known rate to see the local picture.',
                  'برای دیدن تصویر موضعی، دو مقدار و نرخ معلوم را پر کنید.',
                ),
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
      caption: l.t(
        'At the chosen instant the relation behaves locally like a line through the marked point, and that line slope is the ratio of the two rates.',
        'در لحظهٔ انتخاب‌شده، رابطه به‌طور موضعی مانند خطی است که از نقطهٔ نشانگر می‌گذرد و شیب آن نسبت دو نرخ است.',
      ),
    );
  }
}
