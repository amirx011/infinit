import 'package:flutter/material.dart';

/// One editable input on a topic workbench.
class TopicField {
  const TopicField(
    this.key,
    this.labelEn,
    this.labelFa, {
    this.initial = '',
    this.numeric = false,
    this.hintEn = '',
    this.hintFa = '',
  });

  final String key;
  final String labelEn;
  final String labelFa;
  final String initial;
  final bool numeric;
  final String hintEn;
  final String hintFa;
}

/// A curriculum topic that the shared C++/Julia workbench can solve.
class TopicSpec {
  const TopicSpec({
    required this.id,
    required this.titleEn,
    required this.titleFa,
    required this.icon,
    required this.blurbEn,
    required this.blurbFa,
    required this.fields,
    this.examples = const [],
    String? backendId,
  }) : backendId = backendId ?? id;

  /// The topic id sent to the engine.  Usually the same as [id], but an alias
  /// spec can reuse a solver under a more specific heading.
  final String id;
  final String backendId;
  final String titleEn;
  final String titleFa;
  final IconData icon;
  final String blurbEn;
  final String blurbFa;
  final List<TopicField> fields;
  final List<String> examples;
}

const _oscillatorFields = <TopicField>[
  TopicField('m', 'Mass m', 'جرم m', initial: '1', numeric: true),
  TopicField('c', 'Damping c', 'میرایی c', initial: '0.5', numeric: true),
  TopicField(
    'k',
    'Spring constant k',
    'ثابت فنر k',
    initial: '4',
    numeric: true,
  ),
  TopicField(
    'f0',
    'Forcing amplitude F₀',
    'دامنهٔ نیرو F₀',
    initial: '0',
    numeric: true,
  ),
  TopicField(
    'w',
    'Forcing frequency ω',
    'بسامد نیرو ω',
    initial: '1',
    numeric: true,
  ),
  TopicField(
    'y0',
    'Initial displacement',
    'جابه‌جایی اولیه',
    initial: '1',
    numeric: true,
  ),
  TopicField(
    'v0',
    'Initial velocity',
    'سرعت اولیه',
    initial: '0',
    numeric: true,
  ),
];

const kTopicCatalog = <String, TopicSpec>{
  'growth_decay': TopicSpec(
    id: 'growth_decay',
    titleEn: 'Growth & decay',
    titleFa: 'رشد و واپاشی',
    icon: Icons.trending_up_rounded,
    blurbEn: 'A quantity whose rate of change is proportional to its size. The answer is a single exponential.',
    blurbFa: 'کمیتی که آهنگ تغییرش با اندازهٔ خودش متناسب است. جواب یک تابع نمایی است.',
    fields: [
      TopicField(
        'k',
        'Rate constant k',
        'ثابت آهنگ k',
        initial: '0.2',
        numeric: true,
      ),
      TopicField(
        'y0',
        'Initial amount y₀',
        'مقدار اولیه y₀',
        initial: '100',
        numeric: true,
      ),
      TopicField('t', 'Time t', 'زمان t', initial: '5', numeric: true),
    ],
    examples: ['k = 0.2, y₀ = 100, t = 5'],
  ),
  'cooling': TopicSpec(
    id: 'cooling',
    titleEn: "Newton's law of cooling",
    titleFa: 'قانون سرد شدن نیوتن',
    icon: Icons.thermostat_rounded,
    blurbEn: 'An object cools toward the room temperature at a rate proportional to the temperature difference.',
    blurbFa: 'جسم با آهنگی متناسب با اختلاف دما به دمای محیط نزدیک می‌شود.',
    fields: [
      TopicField(
        'k',
        'Cooling constant k',
        'ثابت سرد شدن k',
        initial: '0.1',
        numeric: true,
      ),
      TopicField(
        'room',
        'Room temperature',
        'دمای محیط',
        initial: '20',
        numeric: true,
      ),
      TopicField(
        't0',
        'Initial temperature',
        'دمای اولیه',
        initial: '90',
        numeric: true,
      ),
      TopicField('t', 'Time t', 'زمان t', initial: '10', numeric: true),
    ],
    examples: ['k = 0.1, room = 20, T₀ = 90, t = 10'],
  ),
  'logistic': TopicSpec(
    id: 'logistic',
    titleEn: 'Logistic population',
    titleFa: 'جمعیت لجستیک',
    icon: Icons.groups_rounded,
    blurbEn: 'Growth slows as the population approaches the carrying capacity, giving an S-shaped curve.',
    blurbFa: 'با نزدیک شدن جمعیت به ظرفیت حمل، رشد کند می‌شود و منحنی S شکل می‌گیرد.',
    fields: [
      TopicField(
        'r',
        'Growth rate r',
        'آهنگ رشد r',
        initial: '0.5',
        numeric: true,
      ),
      TopicField(
        'K',
        'Carrying capacity K',
        'ظرفیت حمل K',
        initial: '1000',
        numeric: true,
      ),
      TopicField(
        'p0',
        'Initial population P₀',
        'جمعیت اولیه P₀',
        initial: '50',
        numeric: true,
      ),
      TopicField('t', 'Time t', 'زمان t', initial: '10', numeric: true),
    ],
    examples: ['r = 0.5, K = 1000, P₀ = 50, t = 10'],
  ),
  'mixing': TopicSpec(
    id: 'mixing',
    titleEn: 'Mixing problems',
    titleFa: 'مسائل اختلاط',
    icon: Icons.science_rounded,
    blurbEn: 'Salt flows into a tank and the mixture leaves. Track the amount Q(t) with a balance law.',
    blurbFa: 'نمک وارد مخزن می‌شود و مخلوط خارج می‌شود. مقدار Q(t) را با قانون موازنه دنبال کنید.',
    fields: [
      TopicField(
        'V0',
        'Initial volume V₀',
        'حجم اولیه V₀',
        initial: '100',
        numeric: true,
      ),
      TopicField(
        'q0',
        'Initial amount Q₀',
        'مقدار اولیه Q₀',
        initial: '10',
        numeric: true,
      ),
      TopicField(
        'rin',
        'Inflow rate',
        'دبی ورودی',
        initial: '5',
        numeric: true,
      ),
      TopicField(
        'rout',
        'Outflow rate',
        'دبی خروجی',
        initial: '5',
        numeric: true,
      ),
      TopicField(
        'cin',
        'Inflow concentration',
        'غلظت ورودی',
        initial: '0.2',
        numeric: true,
      ),
      TopicField('t', 'Time t', 'زمان t', initial: '10', numeric: true),
    ],
    examples: ['V₀ = 100, Q₀ = 10, r = 5, c = 0.2'],
  ),
  'spring_mass': TopicSpec(
    id: 'spring_mass',
    titleEn: 'Spring–mass systems',
    titleFa: 'سیستم جرم و فنر',
    icon: Icons.architecture_rounded,
    blurbEn: 'A mass on a spring with damping and optional forcing. The answer is the full transient plus steady-state response.',
    blurbFa: 'جرمی روی فنر با میرایی و نیروی اختیاری. جواب شامل پاسخ گذرا و حالت پایدار است.',
    fields: _oscillatorFields,
    examples: ['m = 1, c = 0.5, k = 4, F₀ = 0'],
  ),
  'damped_oscillation': TopicSpec(
    id: 'damped_oscillation',
    backendId: 'spring_mass',
    titleEn: 'Damped & forced oscillations',
    titleFa: 'نوسان‌های میرا و اجباری',
    icon: Icons.vibration_rounded,
    blurbEn: 'The same oscillator seen through damping: underdamped, critically damped and overdamped, plus a sinusoidal forcing term.',
    blurbFa: 'همان نوسانگر از دید میرایی: زیرمیرا، میرای بحرانی و فرامیرا، به‌همراه یک نیروی سینوسی.',
    fields: _oscillatorFields,
    examples: [
      'm = 1, c = 0.5, k = 4  →  underdamped',
      'm = 1, c = 4, k = 4  →  critically damped',
      'm = 1, c = 0, k = 4, F₀ = 1, ω = 1  →  forced',
    ],
  ),
  'circuit': TopicSpec(
    id: 'circuit',
    titleEn: 'Electrical circuits',
    titleFa: 'مدارهای الکتریکی',
    icon: Icons.electrical_services_rounded,
    blurbEn: 'A series LRC circuit obeys the same second-order equation as a spring–mass system.',
    blurbFa:
        'مدار سری LRC همان معادلهٔ مرتبهٔ دوم سیستم جرم و فنر را پیروی می‌کند.',
    fields: [
      TopicField('m', 'Inductance L', 'القای L', initial: '1', numeric: true),
      TopicField(
        'c',
        'Resistance R',
        'مقاومت R',
        initial: '0.5',
        numeric: true,
      ),
      TopicField(
        'k',
        'Inverse capacitance 1/C',
        'وارون ظرفیت 1/C',
        initial: '4',
        numeric: true,
      ),
      TopicField(
        'f0',
        'Source amplitude E₀',
        'دامنهٔ منبع E₀',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'w',
        'Source frequency ω',
        'بسامد منبع ω',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'y0',
        'Initial charge',
        'بار اولیه',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'v0',
        'Initial current',
        'جریان اولیه',
        initial: '0',
        numeric: true,
      ),
    ],
    examples: ['L = 1, R = 0.5, 1/C = 4, E₀ = 0'],
  ),
  'falling_body': TopicSpec(
    id: 'falling_body',
    titleEn: 'Falling bodies with drag',
    titleFa: 'سقوط با مقاومت هوا',
    icon: Icons.arrow_downward_rounded,
    blurbEn: 'Gravity pulls down while linear drag opposes motion, so the velocity approaches a terminal value.',
    blurbFa: 'گرانش به پایین می‌کشد و مقاومت خطی مخالف حرکت است، پس سرعت به مقدار حدی می‌رسد.',
    fields: [
      TopicField('m', 'Mass m', 'جرم m', initial: '1', numeric: true),
      TopicField(
        'k',
        'Drag coefficient k',
        'ضریب مقاومت k',
        initial: '0.5',
        numeric: true,
      ),
      TopicField('g', 'Gravity g', 'گرانش g', initial: '9.81', numeric: true),
      TopicField(
        'v0',
        'Initial velocity',
        'سرعت اولیه',
        initial: '0',
        numeric: true,
      ),
      TopicField('t', 'Time t', 'زمان t', initial: '5', numeric: true),
    ],
    examples: ['m = 1, k = 0.5, g = 9.81, v₀ = 0'],
  ),
  'euler': TopicSpec(
    id: 'euler',
    titleEn: 'Euler method',
    titleFa: 'روش اویلر',
    icon: Icons.straighten_rounded,
    blurbEn: 'The simplest numerical step: follow the tangent line for a fixed step h.',
    blurbFa: 'ساده‌ترین گام عددی: به اندازهٔ گام h روی خط مماس پیش بروید.',
    fields: [
      TopicField(
        'f',
        'Right side f(x, y)',
        'سمت راست f(x, y)',
        initial: 'x + y',
        hintEn: 'e.g. x + y',
        hintFa: 'مثلاً x + y',
      ),
      TopicField('x0', 'Start x₀', 'شروع x₀', initial: '0', numeric: true),
      TopicField('y0', 'Start y₀', 'شروع y₀', initial: '1', numeric: true),
      TopicField('h', 'Step h', 'گام h', initial: '0.1', numeric: true),
      TopicField(
        'n',
        'Number of steps',
        'تعداد گام',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: ["y' = x + y, y(0) = 1, h = 0.1"],
  ),
  'heun': TopicSpec(
    id: 'heun',
    titleEn: 'Improved Euler (Heun)',
    titleFa: 'اویلر بهبودیافته (هیون)',
    icon: Icons.linear_scale_rounded,
    blurbEn: 'Average the slope at the start and the predicted end of each step for second-order accuracy.',
    blurbFa: 'شیب ابتدا و انتهای پیش‌بینی‌شدهٔ هر گام را میانگین بگیرید تا دقت مرتبهٔ دوم به‌دست آید.',
    fields: [
      TopicField(
        'f',
        'Right side f(x, y)',
        'سمت راست f(x, y)',
        initial: 'x + y',
      ),
      TopicField('x0', 'Start x₀', 'شروع x₀', initial: '0', numeric: true),
      TopicField('y0', 'Start y₀', 'شروع y₀', initial: '1', numeric: true),
      TopicField('h', 'Step h', 'گام h', initial: '0.1', numeric: true),
      TopicField(
        'n',
        'Number of steps',
        'تعداد گام',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: ["y' = x + y, y(0) = 1, h = 0.1"],
  ),
  'rk4': TopicSpec(
    id: 'rk4',
    titleEn: 'Classical Runge–Kutta (RK4)',
    titleFa: 'رانگ-کوتای کلاسیک (RK4)',
    icon: Icons.stacked_line_chart_rounded,
    blurbEn: 'Four slope samples per step give fourth-order accuracy, the workhorse of numerical ODE solving.',
    blurbFa: 'چهار نمونهٔ شیب در هر گام دقت مرتبهٔ چهارم می‌دهد؛ روش استاندارد حل عددی معادلات.',
    fields: [
      TopicField(
        'f',
        'Right side f(x, y)',
        'سمت راست f(x, y)',
        initial: 'x + y',
      ),
      TopicField('x0', 'Start x₀', 'شروع x₀', initial: '0', numeric: true),
      TopicField('y0', 'Start y₀', 'شروع y₀', initial: '1', numeric: true),
      TopicField('h', 'Step h', 'گام h', initial: '0.1', numeric: true),
      TopicField(
        'n',
        'Number of steps',
        'تعداد گام',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: ["y' = x + y, y(0) = 1, h = 0.1"],
  ),
  'linear_system': TopicSpec(
    id: 'linear_system',
    titleEn: 'Systems of ODEs',
    titleFa: 'دستگاه‌های معادلات دیفرانسیل',
    icon: Icons.hub_rounded,
    blurbEn: 'A 2×2 constant matrix system. Eigenvalues classify the origin as a node, saddle, spiral or centre.',
    blurbFa: 'دستگاه ۲×۲ با ماتریس ثابت. مقدارهای ویژه مبدأ را به گره، زین، مارپیچ یا مرکز طبقه‌بندی می‌کنند.',
    fields: [
      TopicField('a', 'a', 'a', initial: '0', numeric: true),
      TopicField('b', 'b', 'b', initial: '1', numeric: true),
      TopicField('c', 'c', 'c', initial: '-1', numeric: true),
      TopicField('d', 'd', 'd', initial: '0', numeric: true),
    ],
    examples: ["x' = y, y' = −x (centre)"],
  ),
  'fourier_series': TopicSpec(
    id: 'fourier_series',
    titleEn: 'Fourier series',
    titleFa: 'سری فوریه',
    icon: Icons.multiline_chart_rounded,
    blurbEn: 'Expand a periodic function into sines and cosines over the interval [−L, L].',
    blurbFa: 'یک تابع متناوب را روی بازهٔ [−L, L] به سینوس و کسینوس بسط دهید.',
    fields: [
      TopicField('f', 'Function f(x)', 'تابع f(x)', initial: 'x'),
      TopicField(
        'L',
        'Half-period L',
        'نیم‌دوره L',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField(
        'n',
        'Harmonics',
        'تعداد هارمونیک',
        initial: '5',
        numeric: true,
      ),
    ],
    examples: ['f(x) = x on [−π, π]'],
  ),
  'heat': TopicSpec(
    id: 'heat',
    titleEn: 'Heat equation',
    titleFa: 'معادلهٔ گرما',
    icon: Icons.local_fire_department_rounded,
    blurbEn: 'Separation of variables turns the heat equation into decaying sine modes.',
    blurbFa: 'جداسازی متغیرها معادلهٔ گرما را به مودهای سینوسی میراشونده تبدیل می‌کند.',
    fields: [
      TopicField(
        'alpha',
        'Diffusivity α',
        'ضریب نفوذ α',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'L',
        'Length L',
        'طول L',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField('n', 'Terms', 'تعداد جمله', initial: '3', numeric: true),
      TopicField(
        'f',
        'Initial profile',
        'پروفایل اولیه',
        initial: 'sin(pi*x/L)',
      ),
    ],
    examples: ['u_t = u_xx, u(0,t) = u(L,t) = 0'],
  ),
  'wave': TopicSpec(
    id: 'wave',
    titleEn: 'Wave equation',
    titleFa: 'معادلهٔ موج',
    icon: Icons.waves_rounded,
    blurbEn: 'Fixed ends give standing sine modes whose time part is harmonic.',
    blurbFa: 'دو انتهای ثابت مودهای ایستادهٔ سینوسی می‌دهند که بخش زمانی آن‌ها هارمونیک است.',
    fields: [
      TopicField(
        'a',
        'Wave speed a',
        'سرعت موج a',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'L',
        'Length L',
        'طول L',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField('n', 'Terms', 'تعداد جمله', initial: '3', numeric: true),
      TopicField(
        'f',
        'Initial displacement',
        'جابه‌جایی اولیه',
        initial: 'sin(pi*x/L)',
      ),
    ],
    examples: ['u_tt = a² u_xx, fixed ends'],
  ),
  'laplace_pde': TopicSpec(
    id: 'laplace_pde',
    titleEn: 'Laplace equation',
    titleFa: 'معادلهٔ لاپلاس',
    icon: Icons.blur_circular_rounded,
    blurbEn: 'A steady-state problem on a rectangle. Three zero sides and one prescribed side give a sinh series.',
    blurbFa: 'مسئلهٔ حالت پایدار روی مستطیل. سه ضلع صفر و یک ضلع معلوم سری سینوس هیپربولیک می‌دهد.',
    fields: [
      TopicField(
        'a',
        'Width a',
        'عرض a',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField(
        'b',
        'Height b',
        'ارتفاع b',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField('n', 'Terms', 'تعداد جمله', initial: '3', numeric: true),
      TopicField('f', 'Top boundary', 'مرز بالایی', initial: 'sin(pi*x/a)'),
    ],
    examples: ['u_xx + u_yy = 0 on a rectangle'],
  ),
  'laplace': TopicSpec(
    id: 'laplace',
    titleEn: 'Laplace transform',
    titleFa: 'تبدیل لاپلاس',
    icon: Icons.transform_rounded,
    blurbEn:
        'Turn a function of t into a function of s using the standard table.',
    blurbFa: 'یک تابع از t را با جدول استاندارد به تابعی از s تبدیل کنید.',
    fields: [
      TopicField(
        'f',
        'Function f(t)',
        'تابع f(t)',
        initial: 'exp(2*t)*sin(3*t)',
      ),
    ],
    examples: ['f(t) = e^{2t} sin(3t)', 'f(t) = t² e^{2t}'],
  ),
  'inverse_laplace': TopicSpec(
    id: 'inverse_laplace',
    titleEn: 'Inverse Laplace transform',
    titleFa: 'تبدیل معکوس لاپلاس',
    icon: Icons.u_turn_left_rounded,
    blurbEn: 'Recover f(t) from F(s) by matching a table entry.',
    blurbFa: 'با تطبیق یک سطر جدول، f(t) را از F(s) بازیابید.',
    fields: [
      TopicField('F', 'Function F(s)', 'تابع F(s)', initial: '1/(s^2+4)'),
    ],
    examples: ['F(s) = s/(s² + 4)', 'F(s) = 1/(s − 2)³'],
  ),
  'ode_linear': TopicSpec(
    id: 'ode_linear',
    titleEn: 'Linear ODE, constant coefficients',
    titleFa: 'معادلهٔ خطی با ضرایب ثابت',
    icon: Icons.functions_rounded,
    blurbEn: 'Enter coefficients from the highest derivative down. Optional initial conditions fix the constants.',
    blurbFa: 'ضرایب را از بالاترین مشتق به پایین وارد کنید. شرایط اولیهٔ اختیاری ثابت‌ها را تعیین می‌کند.',
    fields: [
      TopicField(
        'coeffs',
        'Coefficients (highest first)',
        'ضرایب (از بالا به پایین)',
        initial: '1, 0, 1',
        hintEn: 'e.g. 1, 3, 2 for y″ + 3y′ + 2y',
        hintFa: 'مثلاً 1, 3, 2 برای y″ + 3y′ + 2y',
      ),
      TopicField(
        'ics',
        'Initial conditions (optional)',
        'شرایط اولیه (اختیاری)',
        initial: '1, 0',
        hintEn: 'e.g. 1, 0 for y(0)=1, y′(0)=0',
        hintFa: 'مثلاً 1, 0 برای y(0)=1 و y′(0)=0',
      ),
    ],
    examples: ["y'' + y = 0, y(0)=1, y'(0)=0", "y'' + 3y' + 2y = 0"],
  ),
  'ode_stiff': TopicSpec(
    id: 'ode_stiff',
    titleEn: 'Stiff initial-value problem',
    titleFa: 'مسئلهٔ مقدار اولیهٔ سخت',
    icon: Icons.speed_rounded,
    blurbEn: 'An implicit Euler march for equations with a fast-decaying mode. Each step is solved with Newton iteration.',
    blurbFa: 'گام‌برداری اویلر ضمنی برای معادلات با مود سریع‌میرا. هر گام با تکرار نیوتن حل می‌شود.',
    fields: [
      TopicField(
        'f',
        'Right side f(x, y)',
        'سمت راست f(x, y)',
        initial: '-50*(y - cos(x))',
      ),
      TopicField('x0', 'Start x₀', 'شروع x₀', initial: '0', numeric: true),
      TopicField('y0', 'Start y₀', 'شروع y₀', initial: '0', numeric: true),
      TopicField('xend', 'End x', 'پایان x', initial: '1', numeric: true),
      TopicField('h', 'Step h', 'گام h', initial: '0.01', numeric: true),
    ],
    examples: ["y' = −50(y − cos x), y(0) = 0"],
  ),
};
