// Partial derivatives, the gradient, the Laplacian and directional derivatives.
import 'package:flutter/material.dart';

import '../app_state.dart';
import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../widgets.dart';

class PartialPage extends StatefulWidget {
  const PartialPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<PartialPage> createState() => _PartialPageState();
}

class _PartialPageState extends State<PartialPage> {
  final _input = TextEditingController(text: 'x^2*y + y^3');
  final _vars = TextEditingController(text: 'x, y');
  final _point = TextEditingController(text: '1, 2');
  final _direction = TextEditingController(text: '1, 1');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;

  static const _examples = <String>[
    'x^2*y + y^3',
    'x^2 + y^2',
    'x*y*exp(x)',
    'sin(x)*cos(y)',
    'ln(x^2 + y^2)',
    'x^3 - 3*x*y + y^3',
  ];

  @override
  void didUpdateWidget(covariant PartialPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _result = null;
      _error = null;
    }
  }

  @override
  void dispose() {
    _input.dispose();
    _vars.dispose();
    _point.dispose();
    _direction.dispose();
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
    if (text.isEmpty) {
      setState(
        () => _error = l.t('Enter a function.', 'یک تابع وارد کنید.'),
      );
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final r = await engine.partialAsync(
        text,
        vars: _vars.text.trim(),
        point: _point.text.trim(),
        direction: _direction.text.trim(),
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
    final showGraphs = SettingsScope.maybeOf(context)?.showGraphs ?? true;
    final vars = _vars.text
        .split(',')
        .map((v) => v.trim())
        .where((v) => v.isNotEmpty)
        .toList();
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                icon: Icons.grid_4x4_rounded,
                title: l.t('Partial derivatives & gradient', 'مشتق جزئی و گرادیان'),
                subtitle: l.t(
                  'Differentiate a multivariable function one variable at a time, then read the gradient at a point.',
                  'تابع چندمتغیره را هر بار نسبت به یک متغیر مشتق بگیرید، سپس گرادیان را در یک نقطه بخوانید.',
                ),
              ),
              const SizedBox(height: 18),
              ExpressionField(
                controller: _input,
                hint: l.t('e.g. x^2*y + y^3', 'مثلاً x^2*y + y^3'),
                onSubmitted: _run,
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  LabelledField(
                    controller: _vars,
                    label: l.t('Variables', 'متغیرها'),
                    width: 140,
                  ),
                  LabelledField(
                    controller: _point,
                    label: l.t('Point (a, b, ...)', 'نقطه (a, b, ...)'),
                    width: 180,
                  ),
                  LabelledField(
                    controller: _direction,
                    label: l.t('Direction (optional)', 'جهت (اختیاری)'),
                    width: 190,
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
                    : const Icon(Icons.grid_4x4_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('Computing…', 'در حال محاسبه…')
                      : l.t('Differentiate and evaluate', 'مشتق‌گیری و ارزیابی'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('One variable moves, the others wait', 'یک متغیر حرکت می‌کند، بقیه می‌مانند'),
          idea: l.t(
            'A partial derivative treats every other variable as a constant. The gradient collects the first partials into one vector pointing in the direction of steepest increase.',
            'مشتق جزئی همهٔ متغیرهای دیگر را ثابت در نظر می‌گیرد. گرادیان مشتق‌های اول را در برداری جمع می‌کند که در جهت بیشترین افزایش است.',
          ),
          points: [
            l.t(
              'f_x means differentiate with respect to x and hold y, z, ... fixed.',
              'f_x یعنی نسبت به x مشتق بگیر و y، z، ... را ثابت نگه دار.',
            ),
            l.t(
              'The directional derivative is the gradient dotted with a unit direction.',
              'مشتق جهت‌دار برابر حاصل‌ضرب داخلی گرادیان با جهت یکه است.',
            ),
            l.t(
              'Clairaut: for a smooth function the mixed partials f_xy and f_yx agree.',
              'کلرو: برای تابع هموار، مشتق‌های مخلوط f_xy و f_yx برابرند.',
            ),
          ],
          pitfall: l.t(
            'Forgetting that y is constant is the usual mistake; the derivative of x*y with respect to x is y, not y + x*y\'.',
            'فراموش کردن ثابت بودن y اشتباه رایج است؛ مشتق x*y نسبت به x برابر y است، نه y + x*y′.',
          ),
        ),
        if (_error != null) ...[
          const SizedBox(height: 16),
          ErrorCard(message: _error!),
        ],
        if (r != null) ...[
          const SizedBox(height: 18),
          Container(key: _resultKey, child: ResultView(result: r, l10n: l)),
          if (showGraphs && r.ok && vars.length >= 2) ...[
            const SizedBox(height: 16),
            SurfacePlot(
              engine: widget.engine,
              expression: _input.text.trim(),
              l10n: l,
              xVariable: vars[0],
              yVariable: vars[1],
              title: l.t(
                'Contour map of the surface',
                'نقشهٔ تراز سطح',
              ),
              caption: l.t(
                'Colour shows the value of f(x, y): warm regions are high and cool regions are low. The partial derivatives are the slopes along the two axes, and the gradient points uphill.',
                'رنگ مقدار f(x, y) را نشان می‌دهد: ناحیه‌های گرم بالا و سرد پایین‌اند. مشتق‌های جزئی شیب در راستای دو محورند و گرادیان به سمت بالا تپه اشاره می‌کند.',
              ),
            ),
          ],
        ],
      ],
    );
  }
}
