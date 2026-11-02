import 'package:flutter/material.dart';

import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

/// Limits: value at a point from both sides (or one side), with the limit laws,
/// L'Hôpital and squeeze recorded as steps.
class LimitPage extends StatefulWidget {
  const LimitPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<LimitPage> createState() => _LimitPageState();
}

class _LimitPageState extends State<LimitPage> {
  final _input = TextEditingController(text: 'sin(x)/x');
  final _xv = TextEditingController(text: 'x');
  final _point = TextEditingController(text: '0');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String _side = 'both';
  String? _plottedExpr;
  String _plottedVar = 'x';
  String _plottedPoint = '0';

  static const _examples = <(String, String)>[
    ('sin(x)/x', '0'),
    ('(1-cos(x))/x^2', '0'),
    ('(1+1/x)^x', 'inf'),
    ('x*ln(x)', '0'),
    ('(x^2-4)/(x-2)', '2'),
    ('(3*x^2+2)/(x^2-1)', 'inf'),
    ('tan(x)/x', '0'),
    ('1/x', '0'),
  ];

  @override
  void didUpdateWidget(covariant LimitPage oldWidget) {
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
    _point.dispose();
    super.dispose();
  }

  double? get _guideX {
    final raw = _plottedPoint
        .replaceAll('−', '-')
        .replaceAll(' ', '')
        .toLowerCase();
    if (raw == 'inf' || raw == '+inf' || raw == 'infinity') return null;
    return double.tryParse(raw);
  }

  (double, double) get _plotWindow {
    final x = _guideX;
    if (x == null) return (-12, 12);
    return (x - 4, x + 4);
  }

  Future<void> _compute() async {
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
    final variable = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    final point = _point.text.trim().isEmpty ? '0' : _point.text.trim();
    setState(() {
      _busy = true;
      _error = null;
      _plottedExpr = text;
      _plottedVar = variable;
      _plottedPoint = point;
    });
    try {
      final r = await engine.limitAsync(
        text,
        x: variable,
        point: point,
        side: _side,
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
    final c = context.colors;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                tone: c.warning,
                icon: Icons.linear_scale_rounded,
                title: l.t('Limits', 'حد'),
                subtitle: l.t(
                  'Limit laws on the expression tree, L\'Hôpital for indeterminate forms, and one-sided limits.',
                  'قوانین حد روی درخت عبارت، قاعدهٔ هوپیتال برای فرم‌های مبهم و حدهای یک‌طرفه.',
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
                  hintText: l.t('e.g. sin(x)/x', 'مثلاً sin(x)/x'),
                  prefixIcon: Icon(
                    Icons.edit_outlined,
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
                    width: 100,
                    child: TextField(
                      controller: _xv,
                      textDirection: TextDirection.ltr,
                      textAlign: TextAlign.left,
                      style: const TextStyle(
                        fontFamily: 'monospace',
                        fontSize: 14,
                      ),
                      decoration: InputDecoration(
                        labelText: l.t('Variable', 'متغیر'),
                        isDense: true,
                      ),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: TextField(
                      controller: _point,
                      textDirection: TextDirection.ltr,
                      textAlign: TextAlign.left,
                      style: const TextStyle(
                        fontFamily: 'monospace',
                        fontSize: 14,
                      ),
                      decoration: InputDecoration(
                        labelText: l.t('Approaches', 'به چه چیزی میل می‌کند'),
                        hintText: l.t('0, pi/2, inf', '۰، pi/2، inf'),
                        isDense: true,
                      ),
                      onSubmitted: (_) => _compute(),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              SegmentedButton<String>(
                segments: [
                  ButtonSegment(
                    value: 'both',
                    label: Text(l.t('Two-sided', 'دوطرفه')),
                    icon: const Icon(Icons.compare_arrows_rounded, size: 17),
                  ),
                  ButtonSegment(
                    value: 'left',
                    label: Text(l.t('Left', 'از چپ')),
                    icon: const Icon(Icons.west_rounded, size: 17),
                  ),
                  ButtonSegment(
                    value: 'right',
                    label: Text(l.t('Right', 'از راست')),
                    icon: const Icon(Icons.east_rounded, size: 17),
                  ),
                ],
                selected: {_side},
                onSelectionChanged: (v) => setState(() => _side = v.first),
              ),
              const SizedBox(height: 14),
              Text(
                l.t('Examples', 'نمونه‌ها'),
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
                        _point.text = ex.$2;
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
                    : const Icon(Icons.linear_scale_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('Computing…', 'در حال محاسبه…')
                      : l.t('Compute the limit', 'محاسبهٔ حد'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t(
            'A limit describes approach',
            'حد، نزدیک شدن را توصیف می‌کند',
          ),
          idea: l.t(
            'The limit at a point depends on nearby values, not on the value at the point itself. That is why a function can have a limit where it is undefined.',
            'حد در یک نقطه به مقادیر نزدیک آن بستگی دارد، نه به مقدار خود تابع در آن نقطه. به همین دلیل تابع می‌تواند در نقطه‌ای تعریف‌نشده هم حد داشته باشد.',
          ),
          points: [
            l.t(
              'Try direct substitution first; 0/0 or ∞/∞ signals that another method is needed.',
              'اول جایگذاری مستقیم را امتحان کنید؛ 0/0 یا ∞/∞ نشان می‌دهد روش دیگری لازم است.',
            ),
            l.t(
              'For jumps, absolute values and pieces, compare the left and right limits separately.',
              'برای پرش‌ها، قدر مطلق و توابع تکه‌ای، حد چپ و راست را جداگانه مقایسه کنید.',
            ),
          ],
          example: const ExView(
            plain: 'lim x->0 sin(x)/x = 1',
            pretty: 'lim x→0 sin x / x = 1',
            latex: r'\lim_{x\to 0}\frac{\sin x}{x} = 1',
          ),
          exampleCaption: l.t(
            'The expression is 0/0 at x = 0; the limit exists even though the quotient is undefined there.',
            'عبارت در x = 0 به 0/0 می‌رسد؛ حد وجود دارد هرچند کسر آنجا تعریف نشده است.',
          ),
          pitfall: l.t(
            "L'Hôpital applies to indeterminate quotients only; differentiating numerator and denominator separately is not a quotient rule.",
            'قاعدهٔ هوپیتال فقط برای کسرهای مبهم است؛ مشتق گرفتن جداگانه از صورت و مخرج، قاعدهٔ تقسیم نیست.',
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
        if (_result != null && _plottedExpr != null) ...[
          const SizedBox(height: 16),
          CurvePlot(
            engine: widget.engine,
            expression: _plottedExpr!,
            variable: _plottedVar,
            l10n: l,
            xmin: _plotWindow.$1,
            xmax: _plotWindow.$2,
            height: 280,
            color: c.accent,
            title: l.t(
              'Near $_plottedVar → $_plottedPoint',
              'نزدیک $_plottedVar → $_plottedPoint',
            ),
            caption: l.t(
              'The dashed guide marks the point being approached. The curve shows why the two sides either agree or disagree.',
              'خط‌چین راهنما نقطه‌ای را نشان می‌دهد که به آن نزدیک می‌شویم. منحنی نشان می‌دهد چرا دو طرف یا هم‌خوان‌اند یا ناهم‌خوان.',
            ),
            guides: [
              if (_guideX != null && !(_result!.limitInfinite)) _guideX!,
            ],
            asymptotes: [
              if (_guideX != null && _result!.limitInfinite) _guideX!,
            ],
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
}
