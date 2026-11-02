// The important theorems of the derivative.
//
// One page serves every theorem: each mode states the theorem in full, verifies
// its hypotheses on the supplied interval, finds the point the theorem promises,
// and draws the picture. The catalogue mode renders the complete reference.
import 'package:flutter/material.dart';

import '../calc_widgets.dart';
import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../theorems.dart';
import '../widgets.dart';

/// Which theorem the page presents.
enum TheoremMode { mvt, rolle, cauchy, lhopital, catalogue }

class TheoremsPage extends StatefulWidget {
  const TheoremsPage({
    super.key,
    required this.mode,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final TheoremMode mode;
  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<TheoremsPage> createState() => _TheoremsPageState();
}

class _TheoremsPageState extends State<TheoremsPage> {
  final _f = TextEditingController();
  final _g = TextEditingController();
  final _xv = TextEditingController(text: 'x');
  final _a = TextEditingController(text: '0');
  final _b = TextEditingController(text: '1');
  final _point = TextEditingController(text: '0');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String? _fieldF;
  String? _fieldG;
  double _fieldA = 0;
  double _fieldB = 1;
  String? _fieldPoint;

  @override
  void initState() {
    super.initState();
    _applyDefaults();
  }

  @override
  void didUpdateWidget(covariant TheoremsPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.mode != widget.mode) {
      _applyDefaults();
      _result = null;
      _error = null;
    }
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _result = null;
      _error = null;
    }
  }

  void _applyDefaults() {
    switch (widget.mode) {
      case TheoremMode.mvt:
        _f.text = 'x^2';
        _a.text = '0';
        _b.text = '2';
      case TheoremMode.rolle:
        _f.text = 'x^2 - 4*x + 3';
        _a.text = '1';
        _b.text = '3';
      case TheoremMode.cauchy:
        _f.text = 'x^2';
        _g.text = 'x^3';
        _a.text = '0';
        _b.text = '1';
      case TheoremMode.lhopital:
        _f.text = 'sin(x)';
        _g.text = 'x';
        _point.text = '0';
      case TheoremMode.catalogue:
        break;
    }
  }

  @override
  void dispose() {
    _f.dispose();
    _g.dispose();
    _xv.dispose();
    _a.dispose();
    _b.dispose();
    _point.dispose();
    super.dispose();
  }

  String get _theoremId => switch (widget.mode) {
    TheoremMode.mvt => 'mvt',
    TheoremMode.rolle => 'rolle',
    TheoremMode.cauchy => 'cauchy',
    TheoremMode.lhopital => 'lhopital',
    TheoremMode.catalogue => 'mvt',
  };

  TheoremEntry get _entry =>
      theoremCatalog.firstWhere((t) => t.id == _theoremId);

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
    final f = _f.text.trim();
    if (f.isEmpty) {
      setState(
        () => _error = l.t('Enter a function.', 'یک تابع وارد کنید.'),
      );
      return;
    }
    final xVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    final a = _bound(_a, 0);
    final b = _bound(_b, 1);
    setState(() {
      _busy = true;
      _error = null;
      _fieldF = f;
      _fieldG = _g.text.trim().isEmpty ? null : _g.text.trim();
      _fieldA = a;
      _fieldB = b;
      _fieldPoint = _point.text.trim();
    });
    try {
      final lang = l.isFa ? 'fa' : 'en';
      final SolveResult r;
      switch (widget.mode) {
        case TheoremMode.cauchy:
          final g = _g.text.trim();
          if (g.isEmpty) {
            setState(() {
              _busy = false;
              _error = l.t(
                'Cauchy needs a second function g.',
                'قضیهٔ کوشی به تابع دوم g نیاز دارد.',
              );
            });
            return;
          }
          r = await engine.cauchyAsync(f, g, x: xVar, a: a, b: b, lang: lang);
        case TheoremMode.lhopital:
          final g = _g.text.trim();
          if (g.isEmpty) {
            setState(() {
              _busy = false;
              _error = l.t(
                'L\'Hôpital needs a denominator g.',
                'قاعدهٔ هوپیتال به مخرج g نیاز دارد.',
              );
            });
            return;
          }
          r = await engine.lhopitalAsync(
            f,
            g,
            x: xVar,
            point: _point.text.trim(),
            lang: lang,
          );
        case TheoremMode.mvt:
        case TheoremMode.rolle:
          if (!(a < b)) {
            setState(() {
              _busy = false;
              _error = l.t(
                'The interval must satisfy a < b.',
                'باید a < b باشد.',
              );
            });
            return;
          }
          r = await engine.meanValueAsync(
            f,
            x: xVar,
            a: a,
            b: b,
            lang: lang,
          );
        case TheoremMode.catalogue:
          return;
      }
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
    if (widget.mode == TheoremMode.catalogue) return _catalogue();
    return _interactive();
  }

  // ------------------------------------------------------------- interactive
  Widget _interactive() {
    final l = widget.l10n;
    final entry = _entry;
    final result = _result;
    final isLhopital = widget.mode == TheoremMode.lhopital;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                icon: isLhopital
                    ? Icons.compare_arrows_rounded
                    : Icons.rule_rounded,
                title: entry.title(l),
                subtitle: l.t(
                  'State the theorem, verify its hypotheses, then find the point it promises.',
                  'قضیه را بیان کنید، فرض‌هایش را بررسی کنید و نقطه‌ای را که تضمین می‌کند بیابید.',
                ),
              ),
              const SizedBox(height: 18),
              _statementBlock(entry),
              const SizedBox(height: 16),
              _hypothesesBlock(entry),
              const SizedBox(height: 20),
              if (!isLhopital)
                ExpressionField(
                  controller: _f,
                  hint: l.t('e.g. x^2', 'مثلاً x^2'),
                  onSubmitted: _run,
                )
              else
                Row(
                  children: [
                    Expanded(
                      child: ExpressionField(
                        controller: _f,
                        hint: l.t('numerator f', 'صورت f'),
                        onSubmitted: _run,
                      ),
                    ),
                    const SizedBox(width: 10),
                    Expanded(
                      child: ExpressionField(
                        controller: _g,
                        hint: l.t('denominator g', 'مخرج g'),
                        onSubmitted: _run,
                      ),
                    ),
                  ],
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
                  if (isLhopital)
                    LabelledField(
                      controller: _point,
                      label: l.t('x →', 'x ←'),
                      width: 100,
                    )
                  else ...[
                    LabelledField(
                      controller: _a,
                      label: l.t('From a', 'از a'),
                      width: 110,
                    ),
                    LabelledField(
                      controller: _b,
                      label: l.t('To b', 'تا b'),
                      width: 110,
                    ),
                  ],
                ],
              ),
              if (widget.mode == TheoremMode.cauchy) ...[
                const SizedBox(height: 12),
                ExpressionField(
                  controller: _g,
                  hint: l.t('second function g', 'تابع دوم g'),
                  onSubmitted: _run,
                ),
              ],
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
                    : const Icon(Icons.verified_outlined, size: 19),
                label: Text(
                  _busy
                      ? l.t('Checking…', 'در حال بررسی…')
                      : l.t('Check the theorem', 'بررسی قضیه'),
                ),
              ),
            ],
          ),
        ),
        if (_error != null) ...[
          const SizedBox(height: 16),
          ErrorCard(message: _error!),
        ],
        if (result != null) ...[
          const SizedBox(height: 16),
          _verificationCard(result),
        ],
        if (result != null && _fieldF != null) ...[
          const SizedBox(height: 16),
          _plot(result),
        ],
        if (result != null) ...[
          const SizedBox(height: 18),
          Container(key: _resultKey, child: ResultView(result: result, l10n: l)),
        ],
      ],
    );
  }

  Widget _statementBlock(TheoremEntry entry) {
    final c = context.colors;
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.fromLTRB(16, 14, 16, 14),
      decoration: BoxDecoration(
        color: c.accentAltSoft,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: c.accentAlt.withValues(alpha: 0.25)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            entry.statement(widget.l10n),
            style: Theme.of(
              context,
            ).textTheme.bodyMedium?.copyWith(height: 1.85),
          ),
          const SizedBox(height: 12),
          MathBlock(
            math: ExView(
              plain: entry.statementEn,
              pretty: entry.statementEn,
              latex: entry.latex,
            ),
            size: 17,
          ),
        ],
      ),
    );
  }

  Widget _hypothesesBlock(TheoremEntry entry) {
    final c = context.colors;
    final items = entry.hypotheses(widget.l10n);
    if (items.isEmpty) return const SizedBox.shrink();
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          widget.l10n.t('Hypotheses to check', 'فرض‌هایی که باید بررسی شوند'),
          style: Theme.of(context).textTheme.bodySmall,
        ),
        const SizedBox(height: 8),
        for (final item in items)
          Padding(
            padding: const EdgeInsets.only(bottom: 6),
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Icon(
                  Icons.circle,
                  size: 6,
                  color: c.accent.withValues(alpha: 0.7),
                ),
                const SizedBox(width: 9),
                Expanded(
                  child: Text(
                    item,
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                ),
              ],
            ),
          ),
      ],
    );
  }

  Widget _verificationCard(SolveResult r) {
    final l = widget.l10n;
    final c = context.colors;
    final checks = r.checks;
    final points = r.points;
    final isLhopital = widget.mode == TheoremMode.lhopital;
    return SoftCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SectionLabel(
            l.t('Theorem check', 'بررسی قضیه'),
            icon: Icons.fact_check_outlined,
          ),
          const SizedBox(height: 14),
          for (final check in checks)
            HypothesisRow(check: check, l10n: l),
          if (!isLhopital && r.rolleApplies && widget.mode != TheoremMode.rolle)
            Padding(
              padding: const EdgeInsets.only(bottom: 10),
              child: Row(
                children: [
                  Icon(Icons.info_outline_rounded, size: 18, color: c.info),
                  const SizedBox(width: 9),
                  Expanded(
                    child: Text(
                      l.t(
                        'Since f(a) = f(b), Rolle\'s theorem applies here as a special case.',
                        'چون f(a) = f(b) است، قضیهٔ رول به‌عنوان حالت خاص اینجا هم برقرار می‌شود.',
                      ),
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ),
                ],
              ),
            ),
          if (!isLhopital) ...[
            const SizedBox(height: 6),
            _valueRow(
              l.t('Secant slope', 'شیب خط قاطع'),
              r.slope?.pretty ?? '?',
            ),
          ],
          if (!isLhopital && points.isNotEmpty) ...[
            const SizedBox(height: 6),
            _valueRow(
              l.t('Promised point c', 'نقطهٔ تضمین‌شده c'),
              points.map((p) => p.pretty).join(', '),
            ),
          ],
          if (isLhopital && r.solution != null) ...[
            const SizedBox(height: 6),
            _valueRow(l.t('Limit', 'حد'), r.solution!.pretty),
          ],
          if (isLhopital && r.limitInfinite) ...[
            const SizedBox(height: 6),
            _valueRow(
              l.t('Limit', 'حد'),
              r.limitSign < 0 ? '−∞' : '∞',
            ),
          ],
          if (isLhopital && !r.limitExists && !r.limitInfinite) ...[
            const SizedBox(height: 6),
            _valueRow(
              l.t('Limit', 'حد'),
              l.t('does not exist', 'وجود ندارد'),
            ),
          ],
          if (points.isEmpty &&
              !isLhopital &&
              r.warnings.isNotEmpty) ...[
            const SizedBox(height: 8),
            for (final w in r.warnings)
              Padding(
                padding: const EdgeInsets.only(bottom: 6),
                child: Text(
                  w,
                  style: Theme.of(
                    context,
                  ).textTheme.bodySmall?.copyWith(color: c.warning),
                ),
              ),
          ],
        ],
      ),
    );
  }

  Widget _valueRow(String label, String value) {
    final c = context.colors;
    return Padding(
      padding: const EdgeInsets.only(bottom: 7),
      child: Row(
        children: [
          SizedBox(
            width: 150,
            child: Text(label, style: Theme.of(context).textTheme.bodySmall),
          ),
          Expanded(
            child: Text(
              value,
              textDirection: TextDirection.ltr,
              textAlign: TextAlign.left,
              style: TextStyle(
                fontFamily: 'DejaVu Sans Mono',
                fontFamilyFallback: const ['monospace', 'Vazirmatn'],
                fontSize: 13.5,
                color: c.text,
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _plot(SolveResult r) {
    final l = widget.l10n;
    final c = context.colors;
    final f = _fieldF!;
    final xVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();

    if (widget.mode == TheoremMode.lhopital) {
      final point = double.tryParse(
        (_fieldPoint ?? '0').replaceAll('−', '-'),
      );
      final window = point == null
          ? (-3.0, 3.0)
          : (point - 3, point + 3);
      return MultiCurvePlot(
        engine: widget.engine,
        l10n: l,
        xmin: window.$1,
        xmax: window.$2,
        variable: xVar,
        height: 320,
        title: l.t(
          'Numerator and denominator near the point',
          'صورت و مخرج نزدیک نقطه',
        ),
        curves: [
          PlotCurveSpec(
            expression: f,
            color: c.accent,
            label: 'f($xVar)',
          ),
          PlotCurveSpec(
            expression: _fieldG ?? 'x',
            color: c.creative,
            label: 'g($xVar)',
            dashed: true,
          ),
        ],
        guides: [?point],
        caption: l.t(
          'The dashed guide is the point being approached. When both curves meet the axis there, the quotient is indeterminate and differentiating is justified.',
          'راهنمای چین همان نقطه‌ای است که به آن نزدیک می‌شویم. وقتی هر دو منحنی آنجا به محور برسند، کسر مبهم است و مشتق‌گیری توجیه دارد.',
        ),
      );
    }

    final a = _fieldA;
    final b = _fieldB;
    final fa = exToDouble(r.valueA);
    final fb = exToDouble(r.valueB);
    final secant = exToDouble(r.slope);
    final window = plotWindowAround([a, b], fallback: 4);

    final series = <PlotSeries>[];
    if (secant != null && fa != null) {
      series.add(
        sampleLine(
          slope: secant,
          intercept: fa - secant * a,
          xmin: window.$1,
          xmax: window.$2,
          color: c.warning,
          label: l.t('Secant', 'خط قاطع'),
          dashed: true,
        ),
      );
    }

    final markers = <PlotMarker>[
      if (fa != null)
        PlotMarker(x: a, y: fa, color: c.muted, kind: PlotMarkerKind.point),
      if (fb != null)
        PlotMarker(x: b, y: fb, color: c.muted, kind: PlotMarkerKind.point),
    ];
    final curves = <PlotCurveSpec>[
      PlotCurveSpec(expression: f, color: c.accent, label: 'f($xVar)'),
    ];

    if (widget.mode == TheoremMode.cauchy) {
      curves.add(
        PlotCurveSpec(
          expression: _fieldG ?? 'x',
          color: c.creative,
          label: 'g($xVar)',
          dashed: true,
        ),
      );
    }

    // A tangent at each promised point makes the conclusion visible: the whole
    // claim is that this tangent is parallel to the secant.
    final tangents = <PlotSeries>[];
    for (var i = 0; i < r.points.length; i++) {
      final cv = exToDouble(r.points[i]);
      final fv = i < r.pointValues.length
          ? exToDouble(r.pointValues[i])
          : null;
      if (cv == null) continue;
      if (fv != null) {
        markers.add(
          PlotMarker(
            x: cv,
            y: fv,
            color: c.success,
            kind: PlotMarkerKind.point,
          ),
        );
      }
      if (fv != null && secant != null) {
        tangents.add(
          sampleLine(
            slope: secant,
            intercept: fv - secant * cv,
            xmin: window.$1,
            xmax: window.$2,
            color: c.success,
            label: l.t('Tangent at c', 'مماس در c'),
          ),
        );
      }
    }

    return MultiCurvePlot(
      engine: widget.engine,
      l10n: l,
      xmin: window.$1,
      xmax: window.$2,
      variable: xVar,
      height: 330,
      title: l.t(
        'Curve, secant and the promised point',
        'منحنی، خط قاطع و نقطهٔ تضمین‌شده',
      ),
      curves: curves,
      extraSeries: tangents,
      markers: markers,
      guides: [a, b],
      caption: l.t(
        'The dashed line joins the two endpoints. The point marker sits at the x the theorem guarantees, where the tangent is parallel to that secant.',
        'خط چین دو سر بازه را به هم وصل می‌کند. نشانگر نقطه در همان x است که قضیه تضمین می‌کند و مماس آنجا موازی خط قاطع است.',
      ),
    );
  }

  // --------------------------------------------------------------- catalogue
  Widget _catalogue() {
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
                icon: Icons.menu_book_rounded,
                title: l.t('Theorems of the derivative', 'قضایای مشتق'),
                subtitle: l.t(
                  'Every theorem stated in full: hypotheses, conclusion, meaning and the mistakes to avoid.',
                  'همهٔ قضایا به‌طور کامل: فرض‌ها، نتیجه، معنا و اشتباه‌های رایج.',
                ),
              ),
              const SizedBox(height: 14),
              Text(
                l.t(
                  'These are the results the rest of the calculus rests on. Each one is a promise about existence, and each one has a hypothesis that cannot be dropped.',
                  'این‌ها نتایجی هستند که بقیهٔ حساب دیفرانسیل بر آن‌ها استوار است. هر کدام وعده‌ای دربارهٔ وجود است و هر کدام فرضی دارد که نمی‌توان حذفش کرد.',
                ),
                style: Theme.of(context).textTheme.bodySmall?.copyWith(height: 1.8),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        for (var i = 0; i < theoremCatalog.length; i++) ...[
          if (i > 0) const SizedBox(height: 12),
          _catalogueCard(theoremCatalog[i], i + 1),
        ],
        const SizedBox(height: 16),
        SoftCard(
          color: c.accentAltSoft,
          borderColor: c.accentAlt.withValues(alpha: 0.25),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Icon(Icons.lightbulb_outline_rounded, size: 19, color: c.accentAlt),
                  const SizedBox(width: 9),
                  Text(
                    l.t('The mistakes that cost marks', 'اشتباه‌هایی که نمره می‌سوزانند'),
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                ],
              ),
              const SizedBox(height: 12),
              for (final pitfall in _commonPitfalls(l))
                Padding(
                  padding: const EdgeInsets.only(bottom: 7),
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Icon(Icons.remove_rounded, size: 14, color: c.warning),
                      const SizedBox(width: 8),
                      Expanded(
                        child: Text(
                          pitfall,
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ),
                    ],
                  ),
                ),
            ],
          ),
        ),
      ],
    );
  }

  List<String> _commonPitfalls(L10n l) => [
    l.t(
      'Rolle and the MVT need continuity on the CLOSED interval and differentiability on the OPEN one.',
      'قضیهٔ رول و مقدار میانگین به پیوستگی روی بازهٔ بسته و مشتق‌پذیری روی بازهٔ باز نیاز دارند.',
    ),
    l.t(
      "f'(c) = 0 never proves an extremum on its own; the sign change is the proof.",
      'صفر بودن f′(c) به‌تنهایی اکسترمم را ثابت نمی‌کند؛ تغییر علامت اثبات است.',
    ),
    l.t(
      "L'Hôpital requires a genuinely indeterminate form before differentiating.",
      'قاعدهٔ هوپیتال پیش از مشتق‌گیری به شکل مبهم واقعی نیاز دارد.',
    ),
    l.t(
      'Cauchy needs g\'(x) ≠ 0 on the open interval, which is what makes the ratio legal.',
      'قضیهٔ کوشی به g′(x) ≠ 0 روی بازهٔ باز نیاز دارد؛ همان چیزی که نسبت را مجاز می‌کند.',
    ),
    l.t(
      'The promised point c is guaranteed to exist, not to be unique or easy to find.',
      'نقطهٔ تضمین‌شدهٔ c وجودش تضمین است، نه یگانگی یا یافت‌شدن آسان آن.',
    ),
  ];

  Widget _catalogueCard(TheoremEntry entry, int index) {
    final l = widget.l10n;
    final c = context.colors;
    return SoftCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Container(
                width: 26,
                height: 26,
                alignment: Alignment.center,
                decoration: BoxDecoration(
                  color: c.accentSoft,
                  shape: BoxShape.circle,
                  border: Border.all(color: c.accent.withValues(alpha: 0.35)),
                ),
                child: Text(
                  '$index',
                  style: TextStyle(
                    fontFamily: 'Vazirmatn',
                    fontSize: 12,
                    fontWeight: FontWeight.w700,
                    color: c.accent,
                  ),
                ),
              ),
              const SizedBox(width: 11),
              Expanded(
                child: Text(
                  entry.title(l),
                  style: Theme.of(
                    context,
                  ).textTheme.titleMedium?.copyWith(fontSize: 15),
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          Text(
            entry.statement(l),
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(height: 1.85),
          ),
          const SizedBox(height: 12),
          MathBlock(
            math: ExView(
              plain: entry.statementEn,
              pretty: entry.statementEn,
              latex: entry.latex,
            ),
            size: 15.5,
          ),
          const SizedBox(height: 12),
          Text(
            entry.why(l),
            style: Theme.of(context).textTheme.bodySmall?.copyWith(height: 1.75),
          ),
          if (entry.hypotheses(l).isNotEmpty) ...[
            const SizedBox(height: 12),
            Text(
              l.t('Hypotheses', 'فرض‌ها'),
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                fontWeight: FontWeight.w700,
                color: c.text,
              ),
            ),
            const SizedBox(height: 6),
            for (final h in entry.hypotheses(l))
              Padding(
                padding: const EdgeInsets.only(bottom: 4),
                child: Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Icon(Icons.circle, size: 5, color: c.accent),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        h,
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ),
                  ],
                ),
              ),
          ],
          if (entry.pitfalls(l).isNotEmpty) ...[
            const SizedBox(height: 10),
            for (final p in entry.pitfalls(l))
              Padding(
                padding: const EdgeInsets.only(bottom: 5),
                child: Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Icon(
                      Icons.error_outline_rounded,
                      size: 14,
                      color: c.warning,
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        p,
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ),
                  ],
                ),
              ),
          ],
        ],
      ),
    );
  }
}
