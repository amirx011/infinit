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
            l.t(
              'The computation engine is unavailable.',
              'موتور محاسبات در دسترس نیست.',
            ),
      );
      return;
    }
    final text = _input.text.trim();
    final start = int.tryParse(_start.text.trim());
    if (text.isEmpty || start == null) {
      setState(
        () => _error = l.t(
          'Enter the series term and a whole start index.',
          'جملهٔ سری و یک اندیس شروع صحیح وارد کنید.',
        ),
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
        _error = l.t('Engine error: $e', 'خطا در اجرای موتور: $e');
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
                title: l.t('Series & convergence', 'سری و همگرایی'),
                subtitle: l.t(
                  'Decide whether an infinite sum settles, and find its exact value when a closed form exists.',
                  'تصمیم بگیرید یک مجموع بی‌نهایت همگرا می‌شود یا نه، و در صورت وجود فرم بسته مقدار دقیق آن را بیابید.',
                ),
              ),
              const SizedBox(height: 18),
              ExpressionField(
                controller: _input,
                hint: l.t('e.g. 1/n^2', 'مثلاً 1/n^2'),
                onSubmitted: _run,
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  LabelledField(
                    controller: _nv,
                    label: l.t('Index variable', 'متغیر اندیس'),
                    width: 150,
                  ),
                  LabelledField(
                    controller: _start,
                    label: l.t('Start index', 'اندیس شروع'),
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
                  _busy
                      ? l.t('Computing…', 'در حال محاسبه…')
                      : l.t('Test for convergence', 'آزمون همگرایی'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('Pick the test that fits the shape', 'آزمونی را انتخاب کن که با شکل می‌خواند'),
          idea: l.t(
            'A series is a sequence of partial sums. Convergence means those sums approach a limit; the tests are shortcuts that prove this without computing every sum.',
            'یک سری دنباله‌ای از مجموع‌های جزئی است. همگرایی یعنی این مجموع‌ها به یک حد نزدیک می‌شوند؛ آزمون‌ها میان‌برهایی برای اثبات این هستند بدون محاسبهٔ همهٔ جمله‌ها.',
          ),
          points: [
            l.t(
              'Geometric: a constant ratio r; converges exactly when |r| < 1.',
              'هندسی: نسبت ثابت r؛ دقیقاً وقتی |r| < 1 همگرا می‌شود.',
            ),
            l.t(
              'p-series: 1/n^p converges exactly when p > 1.',
              'سری p: 1/n^p دقیقاً وقتی p > 1 همگرا می‌شود.',
            ),
            l.t(
              'Ratio and root tests: compute a limit; less than 1 converges, more than 1 diverges, 1 is inconclusive.',
              'آزمون نسبت و ریشه: حد را حساب کن؛ کمتر از ۱ همگرا، بیشتر از ۱ واگرا، و ۱ بی‌نتیجه است.',
            ),
            l.t(
              'Alternating: if |a_n| decreases to 0, the series converges (possibly conditionally).',
              'متناوب: اگر |a_n| نزولی به صفر برود، سری (احتمالاً شرطی) همگرا می‌شود.',
            ),
          ],
          pitfall: l.t(
            'A term that tends to 0 is necessary but not sufficient: 1/n tends to 0 while its series diverges.',
            'صفر شدن جمله لازم است ولی کافی نیست: 1/n به صفر میل می‌کند در حالی که سری آن واگراست.',
          ),
        ),
        if (_error != null) ...[
          const SizedBox(height: 16),
          ErrorCard(message: _error!),
        ],
        if (r != null) ...[
          const SizedBox(height: 18),
          Container(key: _resultKey, child: ResultView(result: r, l10n: l)),
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
        label: l10n.t('last partial sum', 'آخرین مجموع جزئی'),
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
            l10n.t('Partial sums S_k', 'مجموع‌های جزئی S_k'),
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
              fontSize: 14,
            ),
          ),
          const SizedBox(height: 12),
          MathPlot(
            spec: PlotSpec(series: series, xLabel: 'k', yLabel: 'S_k'),
            height: 280,
            caption: l10n.t(
              'Each point adds one more term. Flattening suggests convergence; steady growth or oscillation suggests divergence.',
              'هر نقطه یک جملهٔ دیگر اضافه می‌کند. تخت شدن نشانهٔ همگرایی است؛ رشد پیوسته یا نوسان نشانهٔ واگرایی.',
            ),
          ),
        ],
      ),
    );
  }
}
