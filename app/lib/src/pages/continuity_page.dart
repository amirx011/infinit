import 'package:flutter/material.dart';

import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

/// Continuity at a point, following the definition itself:
/// the left limit and the right limit must both exist and be equal, and the
/// value of the function must equal that common limit.
class ContinuityPage extends StatefulWidget {
  const ContinuityPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<ContinuityPage> createState() => _ContinuityPageState();
}

class _ContinuityPageState extends State<ContinuityPage> {
  final _input = TextEditingController(text: '(x^2-4)/(x-2)');
  final _xv = TextEditingController(text: 'x');
  final _point = TextEditingController(text: '2');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String? _plottedExpr;
  String _plottedVar = 'x';
  String _plottedPoint = '0';

  static const _examples = <(String, String)>[
    ('x^2', '2'),
    ('(x^2-4)/(x-2)', '2'),
    ('1/x', '0'),
    ('abs(x)', '0'),
    ('x/abs(x)', '0'),
    ('sin(x)/x', '0'),
    ('sin(1/x)', '0'),
    ('(x^2-1)/(x-1)', '1'),
  ];

  @override
  void didUpdateWidget(covariant ContinuityPage oldWidget) {
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
    if (x == null) return (-8, 8);
    return (x - 3, x + 3);
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
      setState(() => _error = l.t('Enter a function.', 'یک تابع وارد کنید.'));
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
      final r = await engine.continuityAsync(
        text,
        x: variable,
        point: point,
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
                tone: c.accentAlt,
                icon: Icons.timeline_rounded,
                title: l.t('Continuity', 'پیوستگی'),
                subtitle: l.t(
                  'A function is continuous at a point when the left limit and the right limit exist, are equal, and equal the value of the function.',
                  'تابع در یک نقطه پیوسته است هرگاه حد چپ و حد راست هر دو موجود و برابر باشند و مقدار تابع هم برابر همان حد مشترک باشد.',
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
                  hintText: l.t('e.g. (x^2-4)/(x-2)', 'مثلاً (x^2-4)/(x-2)'),
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
                        labelText: l.t('Point', 'نقطه'),
                        hintText: '0, 1, pi/2',
                        isDense: true,
                      ),
                      onSubmitted: (_) => _compute(),
                    ),
                  ),
                ],
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
                    : const Icon(Icons.timeline_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('Checking…', 'در حال بررسی…')
                      : l.t('Check continuity', 'بررسی پیوستگی'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('Three conditions for continuity', 'سه شرط پیوستگی'),
          idea: l.t(
            'A function is continuous at a point when it is defined there, its two-sided limit exists, and that limit equals the function value. All three must hold together.',
            'تابع در یک نقطه پیوسته است وقتی آنجا تعریف شده باشد، حد دوطرفه وجود داشته باشد و آن حد با مقدار تابع برابر باشد. هر سه شرط باید هم‌زمان برقرار باشند.',
          ),
          points: [
            l.t(
              'Compute the left and right limits independently; equality is what makes the two-sided limit exist.',
              'حد چپ و راست را جدا حساب کنید؛ برابری آن‌ها وجود حد دوطرفه را می‌سازد.',
            ),
            l.t(
              'A removable gap can be repaired by redefining one point; a jump or an infinite break cannot.',
              'شکاف رفع‌شدنی با بازتعریف یک نقطه درست می‌شود؛ پرش یا انقطاع نامتناهی نه.',
            ),
          ],
          example: const ExView(
            plain: 'f(x) = (x^2-4)/(x-2)  has a removable gap at x = 2',
            pretty: 'f(x) = (x² − 4)/(x − 2)  ⇒  gap at x = 2',
            latex:
                r'f(x)=\frac{x^{2}-4}{x-2}\ \text{has a removable gap at }x=2',
          ),
          exampleCaption: l.t(
            'The limit is 4, but f(2) is undefined, so the function is not continuous at 2 until the point is filled in.',
            'حد برابر ۴ است اما f(2) تعریف نشده؛ پس تابع در ۲ پیوسته نیست تا وقتی این نقطه پر شود.',
          ),
          pitfall: l.t(
            'An existing limit is not enough: the value f(a) must also be defined and match it.',
            'وجود حد کافی نیست: مقدار f(a) هم باید تعریف شده و برابر آن باشد.',
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
            height: 270,
            color: c.accent,
            title: l.t(
              'Around $_plottedVar = $_plottedPoint',
              'حول $_plottedVar = $_plottedPoint',
            ),
            caption: l.t(
              'Continuity is a visual agreement: the curve must arrive at the same height from both sides and meet the printed value.',
              'پیوستگی یعنی توافق دیداری: منحنی باید از دو طرف به یک ارتفاع برسد و همان مقدار تعریف‌شده را قطع کند.',
            ),
            guides: [?_guideX],
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
