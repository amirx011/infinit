import 'package:flutter/material.dart';

import '../charts.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

enum _TrajectoryMode { envelope, orthogonal, oblique }

class EnvelopePage extends StatefulWidget {
  const EnvelopePage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
    this.orthogonalOnly = false,
    this.obliqueOnly = false,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  /// Dedicated orthogonal-trajectory entry: the family still has a parameter,
  /// but the page only asks for the perpendicular family and hides the envelope
  /// switch so the two workflows do not blur together.
  final bool orthogonalOnly;

  /// Dedicated oblique-trajectory entry: the crossing angle is a first-class
  /// input and the envelope switch is hidden.
  final bool obliqueOnly;

  @override
  State<EnvelopePage> createState() => _EnvelopePageState();
}

class _EnvelopePageState extends State<EnvelopePage> {
  final _input = TextEditingController(text: 'y = c*x + c^2');
  final _cv = TextEditingController(text: 'c');
  final _xv = TextEditingController(text: 'x');
  final _yv = TextEditingController(text: 'y');
  final _angle = TextEditingController(text: '45');
  final _resultKey = GlobalKey();
  bool _orthogonal = false;
  SolveResult? _result;
  String? _error;
  bool _busy = false;

  static const _envelopeExamples = <String>[
    'y = c*x + c^2',
    'y = c*x - c^2',
    'x^2 + y^2 = c^2',
    'y = c/x',
    '(y - c)^2 = x',
  ];

  static const _orthogonalExamples = <String>[
    'x^2 + y^2 = c^2',
    'y = c*x',
    'y = c/x',
    'x = c',
    'y = c',
  ];

  static const _obliqueExamples = <String>[
    'y = c*x',
    'x^2 + y^2 = c^2',
    'y = c/x',
    'y = c',
    'x = c',
  ];

  _TrajectoryMode get _mode {
    if (widget.obliqueOnly) return _TrajectoryMode.oblique;
    if (widget.orthogonalOnly) return _TrajectoryMode.orthogonal;
    return _orthogonal ? _TrajectoryMode.orthogonal : _TrajectoryMode.envelope;
  }

  List<String> get _examples {
    switch (_mode) {
      case _TrajectoryMode.oblique:
        return _obliqueExamples;
      case _TrajectoryMode.orthogonal:
        return _orthogonalExamples;
      case _TrajectoryMode.envelope:
        return _envelopeExamples;
    }
  }

  @override
  void initState() {
    super.initState();
    _orthogonal = widget.orthogonalOnly;
    if (widget.orthogonalOnly) _input.text = 'x^2 + y^2 = c^2';
    if (widget.obliqueOnly) _input.text = 'y = c*x';
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
    _cv.dispose();
    _xv.dispose();
    _yv.dispose();
    _angle.dispose();
    super.dispose();
  }

  double? _angleValue() {
    final raw = _angle.text.trim().replaceAll('°', '').replaceAll('−', '-');
    final value = double.tryParse(raw);
    if (value == null || !value.isFinite || value <= 0 || value > 90) {
      return null;
    }
    return value;
  }

  Future<void> _solve() async {
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
        () => _error = l.t(
          'Enter a family of curves.',
          'معادلهٔ خانواده را وارد کنید.',
        ),
      );
      return;
    }
    final mode = _mode;
    final angle = _angleValue();
    if (mode == _TrajectoryMode.oblique && angle == null) {
      setState(
        () => _error = l.t(
          'The crossing angle must be a number between 0 and 90 degrees.',
          'زاویهٔ تقاطع باید عددی بین ۰ و ۹۰ درجه باشد.',
        ),
      );
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final c = _cv.text.trim().isEmpty ? 'c' : _cv.text.trim();
      final x = _xv.text.trim().isEmpty ? 'x' : _xv.text.trim();
      final y = _yv.text.trim().isEmpty ? 'y' : _yv.text.trim();
      final lang = l.isFa ? 'fa' : 'en';
      final r = mode == _TrajectoryMode.oblique
          ? await engine.obliqueAsync(
              text,
              c: c,
              angle: angle!,
              x: x,
              y: y,
              lang: lang,
            )
          : await engine.envelopeAsync(
              text,
              c: c,
              orthogonal: mode == _TrajectoryMode.orthogonal,
              x: x,
              y: y,
              lang: lang,
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
    final mode = _mode;
    final oblique = mode == _TrajectoryMode.oblique;
    final orthogonal = mode == _TrajectoryMode.orthogonal;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                tone: c.creative,
                icon: oblique
                    ? Icons.straighten_rounded
                    : orthogonal
                    ? Icons.swap_calls_rounded
                    : Icons.join_inner_rounded,
                title: oblique
                    ? l.t('Oblique trajectories', 'مسیرهای مایل')
                    : orthogonal
                    ? l.t('Orthogonal trajectories', 'مسیرهای قائم')
                    : l.t(
                        'Envelope & orthogonal trajectories',
                        'پوش و مسیرهای قائم',
                      ),
                subtitle: oblique
                    ? l.t(
                        'Differentiate the family, eliminate its parameter, replace the slope using the constant crossing angle α, then solve the resulting equation.',
                        'از خانواده مشتق بگیرید، پارامتر را حذف کنید، شیب را با استفاده از زاویهٔ ثابت α جایگزین کنید و معادلهٔ حاصل را حل کنید.',
                      )
                    : orthogonal
                    ? l.t(
                        'Differentiate the family, eliminate its parameter, replace y′ by −1/y′, then solve the resulting equation.',
                        'از خانواده مشتق بگیرید، پارامتر را حذف کنید، y′ را با −1/y′ جایگزین کنید و معادلهٔ حاصل را حل کنید.',
                      )
                    : l.t(
                        'Eliminate the parameter to find the envelope, and optionally the orthogonal trajectories.',
                        'با حذف پارامتر، پوش خانواده و در صورت نیاز مسیرهای قائم را بیابید.',
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
                  hintText: l.t('e.g. y = c*x + c^2', 'مثلاً y = c*x + c^2'),
                  prefixIcon: Icon(
                    Icons.timeline_rounded,
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
              if (oblique) ...[
                const SizedBox(height: 14),
                Row(
                  crossAxisAlignment: CrossAxisAlignment.center,
                  children: [
                    Expanded(
                      flex: 2,
                      child: _smallField(
                        _angle,
                        l.t('Crossing angle α (degrees)', 'زاویهٔ تقاطع α (درجه)'),
                        '45',
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      flex: 3,
                      child: Text(
                        l.t(
                          'The trajectories meet every family member at this constant angle. 90° gives the orthogonal case.',
                          'مسیرها هر عضو خانواده را با این زاویهٔ ثابت قطع می‌کنند. زاویهٔ ۹۰ درجه حالت قائم است.',
                        ),
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ),
                  ],
                ),
              ],
              const SizedBox(height: 10),
              if (!widget.orthogonalOnly && !widget.obliqueOnly)
                SwitchListTile(
                  value: _orthogonal,
                  onChanged: (v) => setState(() {
                    _orthogonal = v;
                    _result = null;
                  }),
                  contentPadding: EdgeInsets.zero,
                  activeThumbColor: c.accent,
                  title: Text(
                    l.t('Orthogonal trajectories', 'مسیرهای قائم'),
                    style: Theme.of(context).textTheme.bodyMedium,
                  ),
                  subtitle: Text(
                    l.t(
                      'Curves that intersect the family at right angles.',
                      'خانوادهٔ منحنی‌هایی که خانوادهٔ اصلی را با زاویهٔ قائمه قطع می‌کنند.',
                    ),
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                ),
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
                            _cv,
                            l.t('Parameter', 'پارامتر'),
                            'c',
                          ),
                        ),
                        const SizedBox(width: 10),
                        Expanded(
                          child: _smallField(
                            _xv,
                            l.t('Variable x', 'متغیر x'),
                            'x',
                          ),
                        ),
                        const SizedBox(width: 10),
                        Expanded(
                          child: _smallField(
                            _yv,
                            l.t('Variable y', 'متغیر y'),
                            'y',
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 10),
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
                    : const Icon(Icons.auto_fix_high_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('Analysing…', 'در حال تحلیل…')
                      : oblique
                      ? l.t('Find oblique trajectories', 'یافتن مسیرهای مایل')
                      : orthogonal
                      ? l.t(
                          'Find orthogonal trajectories',
                          'یافتن مسیرهای قائم',
                        )
                      : l.t('Analyse family', 'تحلیل خانواده'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        _learningCard(l, mode),
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
        if (_result != null) ...[
          const SizedBox(height: 18),
          Container(
            key: _resultKey,
            child: ResultView(
              result: _result!,
              l10n: l,
              beforeSteps: _fieldPlot(l, c),
            ),
          ),
        ],
      ],
    );
  }

  Widget _learningCard(L10n l, _TrajectoryMode mode) {
    if (mode == _TrajectoryMode.oblique) {
      return LearningCard(
        l10n: l,
        title: l.t(
          'How oblique trajectories are found',
          'مسیرهای مایل چگونه پیدا می‌شوند',
        ),
        idea: l.t(
          'A curve crosses the family at the constant angle α when tan α = (m_t − m)/(1 + m_t m). Solving for the trajectory slope gives m_t = (m + tan α)/(1 − m tan α), the differential equation of the oblique family.',
          'منحنی وقتی خانواده را با زاویهٔ ثابت α قطع می‌کند که tan α = (m_t − m)/(1 + m_t m) باشد. با حل برای شیب مسیر، m_t = (m + tan α)/(1 − m tan α) به دست می‌آید که معادلهٔ دیفرانسیل خانوادهٔ مایل است.',
        ),
        points: [
          l.t(
            'Differentiate F(x, y, c) = 0 implicitly to get the family slope m.',
            'از F(x, y, c) = 0 به‌صورت ضمنی مشتق بگیرید تا شیب خانواده m به دست آید.',
          ),
          l.t(
            'Eliminate c, substitute m_t = (m + tan α)/(1 − m tan α), and solve the resulting ODE.',
            'c را حذف کنید، m_t = (m + tan α)/(1 − m tan α) را جایگذاری کنید و معادلهٔ دیفرانسیل حاصل را حل کنید.',
          ),
          l.t(
            'At α = 90° the formula reduces to y′ = −1/y′, the orthogonal family.',
            'در α = ۹۰ درجه فرمول به y′ = −1/y′ تقلیل می‌یابد که همان خانوادهٔ قائم است.',
          ),
        ],
        example: const ExView(
          plain: 'y = c,  alpha = 45  =>  y = x + C',
          pretty: 'y = c، α = 45° ⇒ y = x + C',
          latex: r'y = c,\quad \alpha = 45^{\circ} \quad\Longrightarrow\quad y = x + C',
        ),
        exampleCaption: l.t(
          'A horizontal family has slope 0; the oblique trajectory slope is tan α = 1.',
          'شیب خانوادهٔ افقی صفر است؛ شیب مسیر مایل tan α = ۱ می‌شود.',
        ),
        pitfall: l.t(
          'The formula fails when the trajectory is vertical; the solver reports x = C instead of dividing by zero.',
          'وقتی مسیر قائم باشد فرمول کار نمی‌کند؛ حل‌کننده به‌جای تقسیم بر صفر، x = C را گزارش می‌کند.',
        ),
      );
    }
    if (mode == _TrajectoryMode.orthogonal) {
      return LearningCard(
        l10n: l,
        title: l.t(
          'How orthogonal trajectories are found',
          'مسیرهای قائم چگونه پیدا می‌شوند',
        ),
        idea: l.t(
          'Two curves cross at a right angle when the product of their slopes is −1. Replacing the family slope y′ by −1/y′ therefore gives the differential equation of the perpendicular family.',
          'دو منحنی وقتی با زاویهٔ قائمه یکدیگر را قطع می‌کنند که حاصل‌ضرب شیب‌هایشان −1 باشد. پس جایگزینی شیب خانواده y′ با −1/y′ معادلهٔ دیفرانسیل خانوادهٔ قائم را می‌دهد.',
        ),
        points: [
          l.t(
            'Differentiate F(x, y, c) = 0 implicitly to get the family slope.',
            'از F(x, y, c) = 0 به‌صورت ضمنی مشتق بگیرید تا شیب خانواده به دست آید.',
          ),
          l.t(
            'Eliminate c using the family equation, then set y′ = −1/y′ and solve.',
            'با استفاده از معادلهٔ خانواده c را حذف کنید، سپس y′ = −1/y′ را قرار دهید و حل کنید.',
          ),
        ],
        example: const ExView(
          plain: 'x^2 + y^2 = c^2  =>  y = C*x',
          pretty: 'x² + y² = c²  ⇒  y = Cx',
          latex: r'x^{2} + y^{2} = c^{2} \quad\Longrightarrow\quad y = Cx',
        ),
        exampleCaption: l.t(
          'The circles have slope −x/y; the perpendicular slope is y/x, whose solutions are the lines y = Cx.',
          'شیب دایره‌ها −x/y است؛ شیب عمود y/x است و جواب‌های آن خطوط y = Cx هستند.',
        ),
        pitfall: l.t(
          'A vertical tangent has no finite slope. The solver treats that case separately instead of dividing by zero.',
          'مماس قائم شیب متناهی ندارد. حل‌کننده این حالت را جداگانه بررسی می‌کند و بر صفر تقسیم نمی‌کند.',
        ),
      );
    }
    return LearningCard(
      l10n: l,
      title: l.t(
        'Envelopes and orthogonal families',
        'پوش‌ها و خانواده‌های قائم',
      ),
      idea: l.t(
        'An envelope is a curve tangent to every member of a family. Orthogonal trajectories are the family that crosses every member at a right angle.',
        'پوش منحنی‌ای است که بر هر عضو یک خانواده مماس است. مسیرهای قائم خانواده‌ای‌اند که هر عضو را با زاویهٔ قائمه قطع می‌کنند.',
      ),
      points: [
        l.t(
          'To find an envelope, eliminate the parameter between F(x, y, c) = 0 and ∂F/∂c = 0.',
          'برای یافتن پوش، پارامتر را بین F(x, y, c) = 0 و ∂F/∂c = 0 حذف کنید.',
        ),
        l.t(
          'For orthogonal trajectories, replace y′ by −1/y′ in the differential equation of the family.',
          'برای مسیرهای قائم، در معادلهٔ دیفرانسیل خانواده y′ را با −1/y′ جایگزین کنید.',
        ),
      ],
      example: const ExView(
        plain: 'y = c*x + c^2  =>  envelope y = -x^2/4',
        pretty: 'y = cx + c²  ⇒  y = −x²/4',
        latex: r'y = cx + c^{2} \quad\Longrightarrow\quad y = -\frac{x^{2}}{4}',
      ),
      exampleCaption: l.t(
        'Differentiate with respect to c, solve for c = −x/2, and substitute back.',
        'نسبت به c مشتق بگیرید، c = −x/2 را به دست آورید و جایگذاری کنید.',
      ),
      pitfall: l.t(
        'Not every family has a real envelope; a formal elimination can return only a complex or degenerate curve.',
        'هر خانواده پوش حقیقی ندارد؛ حذف صوری ممکن است فقط یک منحنی مختلط یا تبهگن بدهد.',
      ),
    );
  }

  Widget? _fieldPlot(L10n l, OdeColors c) {
    final result = _result;
    if (result == null) return null;
    final slope = result.trajectorySlope ?? result.orthogonalSlope;
    if (slope == null) return null;
    final oblique = _mode == _TrajectoryMode.oblique;
    return FieldPlot(
      engine: widget.engine,
      // The field reader expects an equation, not a bare right-hand side, so
      // build y' = f(x, y) from the solved slope.
      expression:
          "${_yv.text.trim().isEmpty ? 'y' : _yv.text.trim()}' = "
          '${slope.plain}',
      l10n: l,
      xVariable: _xv.text.trim().isEmpty ? 'x' : _xv.text.trim(),
      yVariable: _yv.text.trim().isEmpty ? 'y' : _yv.text.trim(),
      xmin: -5,
      xmax: 5,
      ymin: -5,
      ymax: 5,
      title: oblique
          ? l.t(
              'Oblique trajectories — direction field',
              'مسیرهای مایل — میدان جهت',
            )
          : l.t(
              'Orthogonal trajectories — direction field',
              'مسیرهای قائم — میدان جهت',
            ),
      caption: oblique
          ? l.t(
              'The arrows are the slope m_t = (m + tan α)/(1 − m tan α) after eliminating c. The solid curves are oblique trajectories; the dashed curves are members of the original family.',
              'پیکان‌ها شیب m_t = (m + tan α)/(1 − m tan α) پس از حذف c هستند. منحنی‌های پیوسته مسیرهای مایل و منحنی‌های خط‌چین اعضای خانوادهٔ اصلی‌اند.',
            )
          : l.t(
              'The arrows are the slope y′ = −1/f(x,y) after eliminating c. The solid curves are orthogonal trajectories; the dashed curves are members of the original family.',
              'پیکان‌ها شیب y′ = −1/f(x,y) پس از حذف c هستند. منحنی‌های پیوسته مسیرهای قائم و منحنی‌های خط‌چین اعضای خانوادهٔ اصلی‌اند.',
            ),
      extraCurves: _familyOverlay(c),
    );
  }

  /// Overlays a few members of the original family on the trajectory field.
  /// The native sampler substitutes c, so each branch keeps its own constant.
  List<PlotCurveSpec> _familyOverlay(OdeColors c) {
    final result = _result;
    if (result == null || result.familyBranches.isEmpty) return const [];
    final param = _cv.text.trim().isEmpty ? 'c' : _cv.text.trim();
    const values = <double>[-2, -1, 1, 2];
    final palette = <Color>[c.info, c.success, c.warning, c.accentAlt];
    final curves = <PlotCurveSpec>[];
    for (final branch in result.familyBranches) {
      for (var i = 0; i < values.length; i++) {
        curves.add(
          PlotCurveSpec(
            expression: branch.plain,
            constants: {param: values[i]},
            color: palette[i % palette.length].withValues(alpha: 0.55),
            label: '$param = ${values[i].toStringAsFixed(0)}',
            dashed: true,
            width: 1.5,
          ),
        );
      }
    }
    return curves;
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
