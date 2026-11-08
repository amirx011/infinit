// Definite-integral applications: area between curves, volume of revolution
// and arc length.
import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../app_state.dart';
import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

enum _IntegralApp { area, volume, arcLength }

class IntegralAppsPage extends StatefulWidget {
  const IntegralAppsPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<IntegralAppsPage> createState() => _IntegralAppsPageState();
}

class _IntegralAppsPageState extends State<IntegralAppsPage> {
  final _f = TextEditingController(text: 'x');
  final _g = TextEditingController(text: 'x^2');
  final _a = TextEditingController(text: '0');
  final _b = TextEditingController(text: '1');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  _IntegralApp _kind = _IntegralApp.area;
  String _axis = 'x';

  static const _areaExamples = <String>['x|x^2', 'sin(x)|0', '4-x^2|x^2', 'x^2|2*x'];
  static const _volumeExamples = <String>['x^2|', 'sqrt(x)|0', 'x|0', '1/x|0'];
  static const _arcExamples = <String>['x^2', 'sin(x)', 'exp(x)', 'ln(x)', 'sqrt(1-x^2)'];

  @override
  void didUpdateWidget(covariant IntegralAppsPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _result = null;
      _error = null;
    }
  }

  @override
  void dispose() {
    _f.dispose();
    _g.dispose();
    _a.dispose();
    _b.dispose();
    super.dispose();
  }

  String get _engineKind => switch (_kind) {
    _IntegralApp.area => 'area',
    _IntegralApp.volume => 'volume',
    _IntegralApp.arcLength => 'arc_length',
  };

  List<String> get _examples => switch (_kind) {
    _IntegralApp.area => _areaExamples,
    _IntegralApp.volume => _volumeExamples,
    _IntegralApp.arcLength => _arcExamples,
  };

  void _pick(String example) {
    if (_kind == _IntegralApp.arcLength) {
      _f.text = example;
    } else {
      final parts = example.split('|');
      _f.text = parts.first;
      _g.text = parts.length > 1 ? parts[1] : '';
    }
    _run();
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
    final f = _f.text.trim();
    final a = double.tryParse(_a.text.trim().replaceAll('−', '-'));
    final b = double.tryParse(_b.text.trim().replaceAll('−', '-'));
    if (f.isEmpty || a == null || b == null) {
      setState(
        () => _error = l.t(
          'Enter the function and numeric bounds a and b.',
          'تابع و کران‌های عددی a و b را وارد کنید.',
        ),
      );
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final r = await engine.integralApplicationAsync(
        _engineKind,
        f,
        g: _kind == _IntegralApp.arcLength ? '' : _g.text.trim(),
        a: a,
        b: b,
        axis: _axis,
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
                icon: Icons.architecture_rounded,
                title: l.t('Integral applications', 'کاربردهای انتگرال'),
                subtitle: l.t(
                  'Area between curves, volume of revolution and arc length, with the integral set up and evaluated.',
                  'مساحت بین دو منحنی، حجم دوران و طول قوس، همراه با تشکیل و محاسبهٔ انتگرال.',
                ),
              ),
              const SizedBox(height: 18),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  SizedBox(
                    width: 200,
                    child: InputDecorator(
                      decoration: InputDecoration(
                        labelText: l.t('Application', 'کاربرد'),
                      ),
                      child: DropdownButtonHideUnderline(
                        child: DropdownButton<_IntegralApp>(
                          value: _kind,
                          isExpanded: true,
                          items: [
                            DropdownMenuItem(
                              value: _IntegralApp.area,
                              child: Text(l.t('Area between curves', 'مساحت بین دو منحنی')),
                            ),
                            DropdownMenuItem(
                              value: _IntegralApp.volume,
                              child: Text(l.t('Volume of revolution', 'حجم دوران')),
                            ),
                            DropdownMenuItem(
                              value: _IntegralApp.arcLength,
                              child: Text(l.t('Arc length', 'طول قوس')),
                            ),
                          ],
                          onChanged: (v) {
                            setState(() {
                              _kind = v ?? _IntegralApp.area;
                              _result = null;
                            });
                          },
                        ),
                      ),
                    ),
                  ),
                  if (_kind == _IntegralApp.volume)
                    SizedBox(
                      width: 140,
                      child: InputDecorator(
                        decoration: InputDecoration(
                          labelText: l.t('Axis', 'محور'),
                        ),
                        child: DropdownButtonHideUnderline(
                          child: DropdownButton<String>(
                            value: _axis,
                            isExpanded: true,
                            items: [
                              DropdownMenuItem(
                                value: 'x',
                                child: Text(l.t('x-axis (disk/washer)', 'محور x (دیسک/واشر)')),
                              ),
                              DropdownMenuItem(
                                value: 'y',
                                child: Text(l.t('y-axis (shells)', 'محور y (پوسته)')),
                              ),
                            ],
                            onChanged: (v) =>
                                setState(() => _axis = v ?? 'x'),
                          ),
                        ),
                      ),
                    ),
                ],
              ),
              const SizedBox(height: 14),
              ExpressionField(
                controller: _f,
                hint: l.t('Upper curve f(x)', 'منحنی بالایی f(x)'),
                onSubmitted: _run,
              ),
              if (_kind != _IntegralApp.arcLength) ...[
                const SizedBox(height: 12),
                ExpressionField(
                  controller: _g,
                  hint: l.t('Lower curve g(x), or empty for 0', 'منحنی پایینی g(x)، یا خالی برای ۰'),
                  onSubmitted: _run,
                ),
              ],
              const SizedBox(height: 12),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  LabelledField(
                    controller: _a,
                    label: l.t('Lower bound a', 'کران پایین a'),
                    width: 150,
                  ),
                  LabelledField(
                    controller: _b,
                    label: l.t('Upper bound b', 'کران بالا b'),
                    width: 150,
                  ),
                ],
              ),
              const SizedBox(height: 14),
              ExampleRow(
                examples: _examples,
                l10n: l,
                onPick: _pick,
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
                    : const Icon(Icons.architecture_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('Computing…', 'در حال محاسبه…')
                      : l.t('Set up and evaluate', 'تشکیل و محاسبه'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('Slice, then add up', 'برش بزن، سپس جمع کن'),
          idea: l.t(
            'Every definite-integral application is a sum of thin pieces. The hard part is writing the height, radius or length of one piece; the integral adds them.',
            'هر کاربرد انتگرال معین مجموعی از قطعه‌های نازک است. بخش سخت نوشتن ارتفاع، شعاع یا طول یک قطعه است؛ انتگرال آن‌ها را جمع می‌کند.',
          ),
          points: [
            l.t(
              'Area: integrate |f − g|, splitting at the intersection points.',
              'مساحت: انتگرال |f − g|، با شکستن در نقاط تقاطع.',
            ),
            l.t(
              'Volume about x: pi ∫ (outer² − inner²) dx. About y: 2 pi ∫ x (f − g) dx.',
              'حجم حول x: π ∫ (شعاع بیرونی² − درونی²) dx. حول y: ۲π ∫ x(f − g) dx.',
            ),
            l.t(
              'Arc length: ∫ sqrt(1 + (f\')²) dx.',
              'طول قوس: ∫ sqrt(1 + (f′)²) dx.',
            ),
          ],
          pitfall: l.t(
            'Check which curve is on top before subtracting; a negative integrand means the order is reversed.',
            'قبل از تفریق بررسی کنید کدام منحنی بالاست؛ انتگرال‌دهٔ منفی یعنی ترتیب برعکس است.',
          ),
        ),
        if (_error != null) ...[
          const SizedBox(height: 16),
          ErrorCard(message: _error!),
        ],
        if (r != null) ...[
          const SizedBox(height: 18),
          Container(key: _resultKey, child: ResultView(result: r, l10n: l)),
          if (showGraphs && r.ok) ...[
            const SizedBox(height: 16),
            if (_kind == _IntegralApp.area)
              MultiCurvePlot(
                engine: widget.engine,
                l10n: l,
                curves: [
                  PlotCurveSpec(
                    expression: _f.text.trim(),
                    color: c.accent,
                    label: l.t('f(x)', 'f(x)'),
                  ),
                  if (_g.text.trim().isNotEmpty)
                    PlotCurveSpec(
                      expression: _g.text.trim(),
                      color: c.accentAlt,
                      label: l.t('g(x)', 'g(x)'),
                    ),
                ],
                xmin: _plotLo(),
                xmax: _plotHi(),
                title: l.t('Curves and the enclosed region', 'منحنی‌ها و ناحیهٔ محصور'),
                caption: l.t(
                  'The area integral adds thin vertical strips of height |f − g| between the bounds, splitting where the curves cross.',
                  'انتگرال مساحت، نوارهای عمودی نازک با ارتفاع |f − g| را بین کران‌ها جمع می‌کند و در محل تقاطع منحنی‌ها تقسیم می‌شود.',
                ),
              )
            else
              CurvePlot(
                engine: widget.engine,
                l10n: l,
                expression: _f.text.trim(),
                xmin: _plotLo(),
                xmax: _plotHi(),
                title: _kind == _IntegralApp.volume
                    ? l.t('Generating curve', 'منحنی مولد')
                    : l.t('Curve whose length is measured', 'منحنی‌ای که طولش اندازه‌گیری می‌شود'),
                caption: _kind == _IntegralApp.volume
                    ? l.t(
                        'Rotating this curve about the chosen axis sweeps the solid; each thin slice contributes πr² or 2πrh.',
                        'دوران این منحنی حول محور انتخابی جسم را جارو می‌کند؛ هر برش نازک πr² یا 2πrh کمک می‌کند.',
                      )
                    : l.t(
                        'The arc length adds the hypotenuses of tiny steps along the curve, ∫ sqrt(1 + (f′)²) dx.',
                        'طول قوس وترهای گام‌های بسیار کوچک روی منحنی را جمع می‌کند، ∫ sqrt(1 + (f′)²) dx.',
                      ),
              ),
          ],
        ],
      ],
    );
  }

  double _plotLo() {
    final a = double.tryParse(_a.text.trim()) ?? 0;
    final b = double.tryParse(_b.text.trim()) ?? 1;
    final lo = math.min(a, b);
    final hi = math.max(a, b);
    final pad = math.max((hi - lo) * 0.25, 0.5);
    return lo - pad;
  }

  double _plotHi() {
    final a = double.tryParse(_a.text.trim()) ?? 0;
    final b = double.tryParse(_b.text.trim()) ?? 1;
    final lo = math.min(a, b);
    final hi = math.max(a, b);
    final pad = math.max((hi - lo) * 0.25, 0.5);
    return hi + pad;
  }
}
