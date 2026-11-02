import 'package:flutter/material.dart';

import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class OdePage extends StatefulWidget {
  const OdePage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
    this.initialEquation,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  /// Pre-filled equation (used when a form card on the home screen is tapped).
  final String? initialEquation;

  @override
  State<OdePage> createState() => _OdePageState();
}

class _OdePageState extends State<OdePage> {
  late final TextEditingController _input = TextEditingController(
    text: widget.initialEquation ?? "y' = y",
  );
  final _xv = TextEditingController(text: 'x');
  final _yv = TextEditingController(text: 'y');
  final _particular = TextEditingController();
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String? _fieldEquation;
  String _fieldX = 'x';
  String _fieldY = 'y';

  static const _examples = <String>[
    "y' = y",
    "y' + y/x = x^2",
    "y' = x/y",
    "y' = y^2",
    "y' = 1 + y^2",
    "y' = (x+y)^2",
    "y' = x*y + y^2",
    "y' = y^2 - 2/x^2",
    "y = x*y' + (y')^2",
    "y = 2*x*y' + (y')^2",
    "(2x+y) + (x+2y)y' = 0",
    "(x+y)dx + (x+2y)dy = 0",
    "y'' - 3y' + 2y = 0",
    "y'' + y = 0",
    "y'' = 6x",
  ];

  @override
  void initState() {
    super.initState();
    if (const bool.fromEnvironment('INFINIT_DEMO')) {
      WidgetsBinding.instance.addPostFrameCallback((_) => _solve());
    }
  }

  @override
  void didUpdateWidget(covariant dynamic oldWidget) {
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
    _yv.dispose();
    _particular.dispose();
    super.dispose();
  }

  Future<void> _solve() async {
    final engine = widget.engine;
    if (engine == null) {
      setState(
        () => _error =
            widget.engineError ??
            widget.l10n.t(
              'The computation engine is unavailable.',
              'موتور محاسبات در دسترس نیست.',
            ),
      );
      return;
    }
    final text = _input.text.trim();
    if (text.isEmpty) {
      setState(
        () => _error = widget.l10n.t(
          'Enter an equation. Example: y′ = y',
          'یک معادله وارد کنید. نمونه: y′ = y',
        ),
      );
      return;
    }
    final xVar = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
    final yVar = _yv.text.trim().isEmpty ? 'y' : _yv.text.trim();
    setState(() {
      _busy = true;
      _error = null;
      _fieldEquation = text;
      _fieldX = xVar;
      _fieldY = yVar;
    });
    try {
      final result = await engine.solveOdeAsync(
        text,
        x: xVar,
        y: yVar,
        particular: _particular.text.trim(),
        lang: widget.l10n.isFa ? 'fa' : 'en',
      );
      if (!mounted) return;
      setState(() {
        _result = result;
        _busy = false;
      });
      revealWidget(_resultKey);
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _error = widget.l10n.t('Engine error: $e', 'خطا در اجرای موتور: $e');
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
                icon: Icons.functions_rounded,
                title: l.t('Differential equation', 'معادلهٔ دیفرانسیل'),
                subtitle: l.t(
                  'Enter an equation; the exact solution and step-by-step path are shown below.',
                  'معادله را وارد کنید؛ حل دقیق همراه با مسیر گام‌به‌گام ارائه می‌شود.',
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
                  fontSize: 17,
                  height: 1.5,
                ),
                decoration: InputDecoration(
                  hintText: "y' = y    •    y'' - 3y' + 2y = 0",
                  prefixIcon: Icon(
                    Icons.edit_outlined,
                    size: 20,
                    color: c.muted,
                  ),
                ),
                onSubmitted: (_) => _solve(),
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
                      label: ex,
                      onTap: () {
                        _input.text = ex;
                        _solve();
                      },
                    ),
                ],
              ),
              const SizedBox(height: 14),
              Theme(
                data: Theme.of(context)
                    .copyWith(dividerColor: Colors.transparent),
                child: ExpansionTile(
                  tilePadding: EdgeInsets.zero,
                  childrenPadding: const EdgeInsets.only(top: 4, bottom: 4),
                  leading: Icon(Icons.tune_rounded, size: 20, color: c.muted),
                  title: Text(
                    l.t('Advanced options', 'تنظیمات پیشرفته'),
                    style: Theme.of(context).textTheme.bodyMedium,
                  ),
                  children: [
                    Row(
                      children: [
                        Expanded(
                          child: _smallField(
                            _xv,
                            l.t('Independent variable', 'متغیر مستقل'),
                            'x',
                          ),
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: _smallField(
                            _yv,
                            l.t('Dependent variable', 'متغیر وابسته'),
                            'y',
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 12),
                    _smallField(
                      _particular,
                      l.t(
                        'Riccati particular solution (optional)',
                        'جواب خصوصی ریکاتی (اختیاری)',
                      ),
                      l.t('e.g. tan(x)', 'مثلاً tan(x)'),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 14),
              Row(
                children: [
                  FilledButton.icon(
                    onPressed: _busy ? null : _solve,
                    icon: _busy
                        ? const SizedBox(
                            width: 17,
                            height: 17,
                            child: CircularProgressIndicator(
                              strokeWidth: 2,
                              color: Colors.white,
                            ),
                          )
                        : const Icon(Icons.auto_awesome_rounded, size: 19),
                    label: Text(
                      _busy
                          ? l.t('Solving…', 'در حال حل…')
                          : l.t('Solve equation', 'حل معادله'),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      l.t(
                        'Exact symbolic answers; non-elementary integrals are shown honestly.',
                        'حل‌ها نمادین و دقیق‌اند؛ انتگرال‌های غیرمقدماتی صادقانه نشان داده می‌شوند.',
                      ),
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t(
            'What a general solution means',
            'جواب عمومی چه معنایی دارد؟',
          ),
          idea: l.t(
            'A first-order equation describes a whole family of curves. Solving finds the relation that makes the equation true, with one constant C left free until an initial condition fixes it.',
            'معادلهٔ مرتبهٔ اول یک خانوادهٔ کامل از منحنی‌ها را توصیف می‌کند. حل کردن یعنی یافتن رابطه‌ای که معادله را درست کند؛ ثابت C تا وقتی شرط اولیه نداشته باشیم آزاد می‌ماند.',
          ),
          points: [
            l.t(
              'Classify the form first: separable, linear, exact, Bernoulli, or a special first-order form.',
              'اول فرم را تشخیص دهید: جداشدنی، خطی، کامل، برنولی یا یکی از فرم‌های خاص مرتبهٔ اول.',
            ),
            l.t(
              'Differentiate the answer and substitute it back; that is the only reliable check.',
              'از جواب مشتق بگیرید و آن را جایگذاری کنید؛ تنها بررسی قابل‌اعتماد همین است.',
            ),
          ],
          example: const ExView(
            plain: "y' = y  =>  y = C*exp(x)",
            pretty: "y′ = y  ⇒  y = C eˣ",
            latex: r"y' = y \quad\Longrightarrow\quad y = C\,e^{x}",
          ),
          exampleCaption: l.t(
            'Separable: separate, integrate both sides, then solve for y.',
            'جداشدنی: جدا کنید، دو طرف را انتگرال بگیرید و برای y حل کنید.',
          ),
          pitfall: l.t(
            'Dividing by y loses the constant solution y = 0. Check whether it also satisfies the equation.',
            'تقسیم بر y جواب ثابت y = 0 را از دست می‌دهد؛ بررسی کنید آیا آن هم در معادله صدق می‌کند.',
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
        if (_result != null && _fieldEquation != null) ...[
          const SizedBox(height: 16),
          FieldPlot(
            engine: widget.engine,
            expression: _fieldEquation!,
            l10n: l,
            xVariable: _fieldX,
            yVariable: _fieldY,
            title: l.t(
              'Direction field & solution family',
              'میدان جهت و خانوادهٔ جواب',
            ),
            caption: l.t(
              'Each arrow is the local slope of a solution. The coloured curves are RK4 trajectories started from different initial points, so you can compare members of the same family.',
              'هر پیکان شیب محلی یک جواب است. منحنی‌های رنگی مسیرهای RK4 از نقطه‌های اولیهٔ مختلف‌اند تا اعضای یک خانواده را با هم مقایسه کنید.',
            ),
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

  Widget _smallField(
    TextEditingController controller,
    String label,
    String hint,
  ) {
    return TextField(
      controller: controller,
      textDirection: TextDirection.ltr,
      textAlign: TextAlign.left,
      style: const TextStyle(fontFamily: 'monospace', fontSize: 14),
      decoration: InputDecoration(
        labelText: label,
        hintText: hint,
        isDense: true,
      ),
    );
  }
}

