// Taylor and Maclaurin polynomials with the Lagrange remainder.
import 'package:flutter/material.dart';

import '../app_state.dart';
import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class TaylorPage extends StatefulWidget {
  const TaylorPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<TaylorPage> createState() => _TaylorPageState();
}

class _TaylorPageState extends State<TaylorPage> {
  final _input = TextEditingController(text: 'exp(x)');
  final _xv = TextEditingController(text: 'x');
  final _center = TextEditingController(text: '0');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  int _order = 4;

  static const _examples = <String>[
    'exp(x)',
    'sin(x)',
    'cos(x)',
    'ln(1+x)',
    '1/(1-x)',
    'sqrt(1+x)',
    'tan(x)',
  ];

  @override
  void didUpdateWidget(covariant TaylorPage oldWidget) {
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
    _center.dispose();
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
    final center = double.tryParse(
      _center.text.trim().replaceAll('−', '-'),
    );
    if (text.isEmpty || center == null) {
      setState(
        () => _error = l.t(
          'Enter a function and a numeric centre a.',
          'یک تابع و مرکز عددی a وارد کنید.',
        ),
      );
      return;
    }
    final xVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final r = await engine.taylorAsync(
        text,
        x: xVar,
        a: center,
        order: _order,
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
                icon: Icons.polyline_rounded,
                title: l.t('Taylor & Maclaurin', 'تیلور و مکلورن'),
                subtitle: l.t(
                  'Approximate a function by a polynomial built from its derivatives at one point.',
                  'تقریب یک تابع با چندجمله‌ای ساخته‌شده از مشتق‌های آن در یک نقطه.',
                ),
              ),
              const SizedBox(height: 18),
              ExpressionField(
                controller: _input,
                hint: l.t('e.g. exp(x)', 'مثلاً exp(x)'),
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
                    controller: _center,
                    label: l.t('Centre a (0 = Maclaurin)', 'مرکز a (۰ = مکلورن)'),
                    width: 180,
                  ),
                  SizedBox(
                    width: 150,
                    child: InputDecorator(
                      decoration: InputDecoration(
                        labelText: l.t('Order n', 'مرتبهٔ n'),
                      ),
                      child: DropdownButtonHideUnderline(
                        child: DropdownButton<int>(
                          value: _order,
                          isDense: true,
                          items: [
                            for (var i = 0; i <= 12; i++)
                              DropdownMenuItem(value: i, child: Text('$i')),
                          ],
                          onChanged: (v) => setState(() => _order = v ?? 4),
                        ),
                      ),
                    ),
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
                    : const Icon(Icons.polyline_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('Computing…', 'در حال محاسبه…')
                      : l.t('Build the polynomial', 'ساخت چندجمله‌ای'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t('Derivatives at one point, reused', 'مشتق‌ها در یک نقطه، دوباره‌به‌کاررفته'),
          idea: l.t(
            'The Taylor polynomial matches the value and the first n derivatives of f at the centre. Each coefficient is f^(k)(a)/k!, so the polynomial is a local copy of the function.',
            'چندجمله‌ای تیلور مقدار و n مشتق اول تابع را در مرکز مطابقت می‌دهد. هر ضریب f^(k)(a)/k! است، پس چندجمله‌ای یک کپی محلی از تابع است.',
          ),
          points: [
            l.t(
              'Maclaurin is Taylor with a = 0.',
              'مکلورن همان تیلور با a = 0 است.',
            ),
            l.t(
              'The Lagrange remainder bounds the error: the next derivative at some xi between a and x controls it.',
              'باقی‌ماندهٔ لاگرانژ خطا را کران می‌دهد: مشتق بعدی در نقطه‌ای xi بین a و x آن را کنترل می‌کند.',
            ),
            l.t(
              'Higher order is not always better; the series must stay inside its radius of convergence.',
              'مرتبهٔ بالاتر همیشه بهتر نیست؛ سری باید در شعاع همگرایی خود بماند.',
            ),
          ],
          pitfall: l.t(
            'Do not forget the factorial in every coefficient, and do not mix the centre a with the variable x.',
            'فاکتوریل هر ضریب را فراموش نکنید و مرکز a را با متغیر x اشتباه نگیرید.',
          ),
        ),
        if (_error != null) ...[
          const SizedBox(height: 16),
          ErrorCard(message: _error!),
        ],
        if (r != null) ...[
          const SizedBox(height: 18),
          Container(key: _resultKey, child: ResultView(result: r, l10n: l)),
          if (showGraphs &&
              r.ok &&
              r.normalized != null &&
              r.polynomial != null) ...[
            const SizedBox(height: 16),
            MultiCurvePlot(
              engine: widget.engine,
              l10n: l,
              variable: r.variable.isEmpty ? 'x' : r.variable,
              curves: [
                PlotCurveSpec(
                  expression: r.normalized!.plain,
                  color: c.accent,
                  label: l.t('f(x)', 'f(x)'),
                ),
                PlotCurveSpec(
                  expression: r.polynomial!.plain,
                  color: c.accentAlt,
                  dashed: true,
                  label: l.t('Taylor polynomial', 'چندجمله‌ای تیلور'),
                ),
              ],
              xmin: _plotCentre() - 5,
              xmax: _plotCentre() + 5,
              title: l.t('Function and Taylor polynomial', 'تابع و چندجمله‌ای تیلور'),
              caption: l.t(
                'The polynomial agrees with f at the centre and matches its first derivatives there. Away from the centre the gap is the remainder; it grows faster outside the radius of convergence.',
                'چندجمله‌ای در مرکز با f و مشتق‌های اولش برابر است. دورتر از مرکز، فاصله همان باقی‌مانده است و بیرون از شعاع همگرایی سریع‌تر بزرگ می‌شود.',
              ),
            ),
          ],
        ],
      ],
    );
  }

  double _plotCentre() => double.tryParse(_center.text.trim()) ?? 0;
}
