import 'l10n.dart';
import 'models.dart';
import 'tool_links.dart';

/// One numbered step in a worked example.  The title and detail live in the
/// catalogue under `study.<article>.example.<n>.step.<k>.*`.
class StudyStep {
  const StudyStep(this.keyPrefix, {this.math});

  final String keyPrefix;
  final ExView? math;

  String title(L10n l) => l.t('$keyPrefix.title');
  String detail(L10n l) => l.t('$keyPrefix.detail');
}

/// One fully solved example.  Its sentences come from
/// `study.<article>.example.<n>.*`, its mathematics from code.
class StudyExample {
  const StudyExample({
    required this.keyPrefix,
    required this.answerMath,
    required this.steps,
  });

  final String keyPrefix;
  final ExView answerMath;
  final List<StudyStep> steps;

  String problem(L10n l) => l.t('$keyPrefix.problem');
  String answer(L10n l) => l.t('$keyPrefix.answer');
}

/// A self-contained study article.  All human-language text comes from the
/// catalogue under `study.<id>.*`; only ids and mathematics stay in code.
class StudyArticle {
  const StudyArticle({
    required this.id,
    required this.shortMath,
    required this.examples,
    this.links = const [],
  });

  final String id;
  final ExView shortMath;
  final List<StudyExample> examples;
  final List<ToolLink> links;

  String title(L10n l) => l.t('study.$id.title');
  String subtitle(L10n l) => l.t('study.$id.subtitle');
  String shortAnswer(L10n l) => l.t('study.$id.short_answer');
  String pitfalls(L10n l) => l.t('study.$id.pitfalls');
  List<String> keyIdeas(L10n l) => l.tList('study.$id.key_idea');

  /// The long-form explanation, one paragraph per entry.  Inline mathematics
  /// uses `$...$`; [MathText] turns it into typeset formulas.
  List<String> theory(L10n l) => l.tList('study.$id.theory');
}

const _lhs = r"f'(x)=\lim_{h\to 0}\frac{f(x+h)-f(x)}{h}";

final kStudyArticles = <StudyArticle>[
  StudyArticle(
    id: 'derivative',
    shortMath: ExView(
      plain: "f'(x) = lim(h->0) [f(x+h) - f(x)] / h",
      pretty: 'f′(x) = lim(h→0) [f(x+h) − f(x)] / h',
      latex: _lhs,
    ),
    examples: [
      StudyExample(
        keyPrefix: 'study.derivative.example.0',
        answerMath: ExView(
          plain: "f'(x) = 3x^2 + 2,   y = 5x - 2",
          pretty: 'f′(x) = 3x² + 2,   y = 5x − 2',
          latex: r"f'(x)=3x^2+2,\qquad y=5x-2",
        ),
        steps: [
          StudyStep(
            'study.derivative.example.0.step.0',
            math: ExView(
              plain: 'd/dx[x^3] = 3x^2,   d/dx[2x] = 2',
              pretty: 'd/dx[x³] = 3x²,   d/dx[2x] = 2',
              latex: r"\frac{d}{dx}x^3=3x^2,\qquad \frac{d}{dx}2x=2",
            ),
          ),
          StudyStep(
            'study.derivative.example.0.step.1',
            math: ExView(
              plain: "f'(x) = 3x^2 + 2",
              pretty: 'f′(x) = 3x² + 2',
              latex: r"f'(x)=3x^2+2",
            ),
          ),
          StudyStep(
            'study.derivative.example.0.step.2',
            math: ExView(
              plain: "f'(1) = 3(1)^2 + 2 = 5",
              pretty: 'f′(1) = 3(1)² + 2 = 5',
              latex: r"f'(1)=3(1)^2+2=5",
            ),
          ),
          StudyStep(
            'study.derivative.example.0.step.3',
            math: ExView(
              plain: 'f(1) = 1 + 2 = 3,   (1, 3)',
              pretty: 'f(1) = 1 + 2 = 3,   (1, 3)',
              latex: r"f(1)=1+2=3,\qquad (1,3)",
            ),
          ),
          StudyStep(
            'study.derivative.example.0.step.4',
            math: ExView(
              plain: 'y - 3 = 5(x - 1)  =>  y = 5x - 2',
              pretty: 'y − 3 = 5(x − 1)  ⇒  y = 5x − 2',
              latex: r"y-3=5(x-1)\;\Rightarrow\;y=5x-2",
            ),
          ),
        ],
      ),
      StudyExample(
        keyPrefix: 'study.derivative.example.1',
        answerMath: ExView(
          plain: "y' = -x/y = -3/4,   y = 4 - (3/4)(x - 3)",
          pretty: 'y′ = −x/y = −3/4,   y = 4 − (3/4)(x − 3)',
          latex: r"y'=-\frac{x}{y}=-\frac34,\qquad y=4-\frac34(x-3)",
        ),
        steps: [
          StudyStep(
            'study.derivative.example.1.step.0',
            math: ExView(
              plain: 'x^2 + y^2 = 25',
              pretty: 'x² + y² = 25',
              latex: r"x^2+y^2=25",
            ),
          ),
          StudyStep(
            'study.derivative.example.1.step.1',
            math: ExView(
              plain: "2x + 2y*y' = 0",
              pretty: '2x + 2y·y′ = 0',
              latex: r"2x+2yy'=0",
            ),
          ),
          StudyStep(
            'study.derivative.example.1.step.2',
            math: ExView(
              plain: "y' = -x/y",
              pretty: 'y′ = −x/y',
              latex: r"y'=-\frac{x}{y}",
            ),
          ),
          StudyStep(
            'study.derivative.example.1.step.3',
            math: ExView(
              plain: "(3, 4):  y' = -3/4",
              pretty: '(3, 4):  y′ = −3/4',
              latex: r"(3,4):\quad y'=-\frac34",
            ),
          ),
          StudyStep(
            'study.derivative.example.1.step.4',
            math: ExView(
              plain: 'y = 4 - (3/4)(x - 3)',
              pretty: 'y = 4 − (3/4)(x − 3)',
              latex: r"y=4-\frac34(x-3)",
            ),
          ),
        ],
      ),
    ],
    links: [
      ToolLink(labelKey: 'ui.derivative', page: 5, seed: null),
      ToolLink(labelKey: 'ui.tangent_normal', page: 11),
      ToolLink(labelKey: 'ui.function_behaviour', page: 10),
    ],
  ),
  StudyArticle(
    id: 'integral',
    shortMath: ExView(
      plain: 'integral_a^b f(x) dx = F(b) - F(a),   F\' = f',
      pretty: '∫ₐᵇ f(x) dx = F(b) − F(a),   F′ = f',
      latex: r"\int_a^b f(x)\,dx=F(b)-F(a),\qquad F'=f",
    ),
    examples: [
      StudyExample(
        keyPrefix: 'study.integral.example.0',
        answerMath: ExView(plain: '12', pretty: '12', latex: r"12"),
        steps: [
          StudyStep(
            'study.integral.example.0.step.0',
            math: ExView(
              plain: 'F(x) = x^3 + x^2',
              pretty: 'F(x) = x³ + x²',
              latex: r"F(x)=x^3+x^2",
            ),
          ),
          StudyStep(
            'study.integral.example.0.step.1',
            math: ExView(
              plain: 'F(2) - F(0) = (8 + 4) - (0 + 0)',
              pretty: 'F(2) − F(0) = (8 + 4) − (0 + 0)',
              latex: r"F(2)-F(0)=(8+4)-(0+0)",
            ),
          ),
          StudyStep(
            'study.integral.example.0.step.2',
            math: ExView(plain: '12', pretty: '12', latex: r"12"),
          ),
          StudyStep(
            'study.integral.example.0.step.3',
            math: ExView(
              plain: 'd/dx[x^3 + x^2] = 3x^2 + 2x',
              pretty: 'd/dx[x³ + x²] = 3x² + 2x',
              latex: r"\frac{d}{dx}\left(x^3+x^2\right)=3x^2+2x",
            ),
          ),
        ],
      ),
      StudyExample(
        keyPrefix: 'study.integral.example.1',
        answerMath: ExView(
          plain: 'integral x*exp(x) dx = exp(x)*(x - 1) + C',
          pretty: '∫ x eˣ dx = eˣ(x − 1) + C',
          latex: r"\int x e^x\,dx=e^x(x-1)+C",
        ),
        steps: [
          StudyStep(
            'study.integral.example.1.step.0',
            math: ExView(
              plain: 'u = x,  dv = exp(x) dx',
              pretty: 'u = x,   dv = eˣ dx',
              latex: r"u=x,\qquad dv=e^x\,dx",
            ),
          ),
          StudyStep(
            'study.integral.example.1.step.1',
            math: ExView(
              plain: 'du = dx,  v = exp(x)',
              pretty: 'du = dx,   v = eˣ',
              latex: r"du=dx,\qquad v=e^x",
            ),
          ),
          StudyStep(
            'study.integral.example.1.step.2',
            math: ExView(
              plain: 'integral x*exp(x) dx = x*exp(x) - integral exp(x) dx',
              pretty: '∫ x eˣ dx = x eˣ − ∫ eˣ dx',
              latex: r"\int x e^x\,dx=xe^x-\int e^x\,dx",
            ),
          ),
          StudyStep(
            'study.integral.example.1.step.3',
            math: ExView(
              plain: 'exp(x)*(x - 1) + C',
              pretty: 'eˣ(x − 1) + C',
              latex: r"e^x(x-1)+C",
            ),
          ),
        ],
      ),
    ],
    links: [
      ToolLink(labelKey: 'ui.integral', page: 6),
      ToolLink(labelKey: 'ui.integral_applications', page: 24),
      ToolLink(labelKey: 'topic.integration_techniques.title', page: 97),
    ],
  ),
  StudyArticle(
    id: 'ode_types',
    shortMath: ExView(
      plain: "a_n(x)*y^(n) + ... + a_1(x)*y' + a_0(x)*y = g(x)",
      pretty: 'aₙ(x)y⁽ⁿ⁾ + … + a₁(x)y′ + a₀(x)y = g(x)',
      latex: r"a_n(x)y^{(n)}+\cdots+a_1(x)y'+a_0(x)y=g(x)",
    ),
    examples: [
      StudyExample(
        keyPrefix: 'study.ode_types.example.0',
        answerMath: ExView(
          plain: 'y = C_1 e^(-x) + C_2 e^(-2x) + e^x/6',
          pretty: 'y = C₁e⁻ˣ + C₂e⁻²ˣ + eˣ/6',
          latex: r"y=C_1e^{-x}+C_2e^{-2x}+\frac{e^x}{6}",
        ),
        steps: [
          StudyStep(
            'study.ode_types.example.0.step.0',
            math: ExView(
              plain: 'order = 2',
              pretty: 'order = 2',
              latex: r"\text{order}=2",
            ),
          ),
          StudyStep(
            'study.ode_types.example.0.step.1',
            math: ExView(
              plain: 'linear, nonhomogeneous, constant coefficients',
              pretty: 'linear, nonhomogeneous, constant coefficients',
              latex: r"\text{linear, nonhomogeneous, constant coefficients}",
            ),
          ),
          StudyStep(
            'study.ode_types.example.0.step.2',
            math: ExView(
              plain: 'r = -1, -2  =>  y_h = C_1 e^(-x) + C_2 e^(-2x)',
              pretty: 'r = −1, −2  ⇒  yₕ = C₁e⁻ˣ + C₂e⁻²ˣ',
              latex: r"r=-1,-2\;\Rightarrow\;y_h=C_1e^{-x}+C_2e^{-2x}",
            ),
          ),
          StudyStep(
            'study.ode_types.example.0.step.3',
            math: ExView(
              plain: 'A = 1/6  =>  y_p = e^x/6',
              pretty: 'A = 1/6  ⇒  yₚ = eˣ/6',
              latex: r"A=\frac16\;\Rightarrow\;y_p=\frac{e^x}{6}",
            ),
          ),
          StudyStep(
            'study.ode_types.example.0.step.4',
            math: ExView(
              plain: 'y = C_1 e^(-x) + C_2 e^(-2x) + e^x/6',
              pretty: 'y = C₁e⁻ˣ + C₂e⁻²ˣ + eˣ/6',
              latex: r"y=C_1e^{-x}+C_2e^{-2x}+\frac{e^x}{6}",
            ),
          ),
        ],
      ),
      StudyExample(
        keyPrefix: 'study.ode_types.example.1',
        answerMath: ExView(
          plain: "y = 2/(2 - x^2)",
          pretty: 'y = 2/(2 − x²)',
          latex: r"y=\frac{2}{2-x^2}",
        ),
        steps: [
          StudyStep(
            'study.ode_types.example.1.step.0',
            math: ExView(
              plain: "y' = x*y^2",
              pretty: 'y′ = x·y²',
              latex: r"y'=xy^2",
            ),
          ),
          StudyStep(
            'study.ode_types.example.1.step.1',
            math: ExView(
              plain: 'dy/y^2 = x dx',
              pretty: 'dy/y² = x dx',
              latex: r"\frac{dy}{y^2}=x\,dx",
            ),
          ),
          StudyStep(
            'study.ode_types.example.1.step.2',
            math: ExView(
              plain: '-1/y = x^2/2 + C',
              pretty: '−1/y = x²/2 + C',
              latex: r"-\frac1y=\frac{x^2}{2}+C",
            ),
          ),
          StudyStep(
            'study.ode_types.example.1.step.3',
            math: ExView(
              plain: 'y(0) = 1  =>  C = -1',
              pretty: 'y(0) = 1  ⇒  C = −1',
              latex: r"y(0)=1\;\Rightarrow\;C=-1",
            ),
          ),
          StudyStep(
            'study.ode_types.example.1.step.4',
            math: ExView(
              plain: 'y = 2/(2 - x^2)',
              pretty: 'y = 2/(2 − x²)',
              latex: r"y=\frac{2}{2-x^2}",
            ),
          ),
        ],
      ),
    ],
    links: [
      ToolLink(
        labelKey: 'ui.solve_an_equation',
        page: 2,
        seed: "y'' + 3*y' + 2*y = exp(x)",
      ),
      ToolLink(labelKey: 'ui.wronskian', page: 3),
      ToolLink(labelKey: 'topic.laplace.title', page: 37),
      ToolLink(labelKey: 'topic.series_power.title', page: 32),
      ToolLink(labelKey: 'topic.rk4.title', page: 43),
    ],
  ),
  StudyArticle(
    id: 'ode_applications',
    shortMath: ExView(
      plain: 'model -> solve -> initial condition -> interpret',
      pretty: 'model → solve → initial condition → interpret',
      latex:
          r"\text{model}\to\text{solve}\to\text{initial condition}\to\text{interpret}",
    ),
    examples: [
      StudyExample(
        keyPrefix: 'study.ode_applications.example.0',
        answerMath: ExView(
          plain: 'T(10) = 20 + 70 e^(-1) ~= 45.8',
          pretty: 'T(10) = 20 + 70e⁻¹ ≈ 45.8',
          latex: r"T(10)=20+70e^{-1}\approx 45.8",
        ),
        steps: [
          StudyStep(
            'study.ode_applications.example.0.step.0',
            math: ExView(
              plain: "T' = -k(T - T_m)",
              pretty: 'T′ = −k(T − Tₘ)',
              latex: r"T'=-k(T-T_m)",
            ),
          ),
          StudyStep(
            'study.ode_applications.example.0.step.1',
            math: ExView(
              plain: 'T(t) = T_m + C e^(-kt)',
              pretty: 'T(t) = Tₘ + C e⁻ᵏᵗ',
              latex: r"T(t)=T_m+Ce^{-kt}",
            ),
          ),
          StudyStep(
            'study.ode_applications.example.0.step.2',
            math: ExView(
              plain: 'T(t) = 20 + 70 e^(-0.1 t)',
              pretty: 'T(t) = 20 + 70e⁻⁰·¹ᵗ',
              latex: r"T(t)=20+70e^{-0.1t}",
            ),
          ),
          StudyStep(
            'study.ode_applications.example.0.step.3',
            math: ExView(
              plain: 'T(10) = 20 + 70 e^(-1) ~= 45.8',
              pretty: 'T(10) = 20 + 70e⁻¹ ≈ 45.8',
              latex: r"T(10)=20+70e^{-1}\approx 45.8",
            ),
          ),
          StudyStep(
            'study.ode_applications.example.0.step.4',
            math: ExView(
              plain: 'T -> 20,  t -> inf',
              pretty: 'T → 20,   t → ∞',
              latex: r"T\to 20,\quad t\to\infty",
            ),
          ),
        ],
      ),
      StudyExample(
        keyPrefix: 'study.ode_applications.example.1',
        answerMath: ExView(
          plain: 'P(t) = 100 e^(0.02 t),  t_double = ln(2)/0.02 ~= 34.7',
          pretty: 'P(t) = 100e⁰·⁰²ᵗ,   t₂ ≈ 34.7',
          latex: r"P(t)=100e^{0.02t},\qquad t_{2}=\frac{\ln 2}{0.02}\approx 34.7",
        ),
        steps: [
          StudyStep(
            'study.ode_applications.example.1.step.0',
            math: ExView(
              plain: "P' = 0.02 P,  P(0) = 100",
              pretty: 'P′ = 0.02P,   P(0) = 100',
              latex: r"P'=0.02P,\qquad P(0)=100",
            ),
          ),
          StudyStep(
            'study.ode_applications.example.1.step.1',
            math: ExView(
              plain: 'P(t) = 100 e^(0.02 t)',
              pretty: 'P(t) = 100e⁰·⁰²ᵗ',
              latex: r"P(t)=100e^{0.02t}",
            ),
          ),
          StudyStep(
            'study.ode_applications.example.1.step.2',
            math: ExView(
              plain: '200 = 100 e^(0.02 t)',
              pretty: '200 = 100e⁰·⁰²ᵗ',
              latex: r"200=100e^{0.02t}",
            ),
          ),
          StudyStep(
            'study.ode_applications.example.1.step.3',
            math: ExView(
              plain: 't = ln(2)/0.02 ~= 34.7',
              pretty: 't = ln2 / 0.02 ≈ 34.7',
              latex: r"t=\frac{\ln 2}{0.02}\approx 34.7",
            ),
          ),
        ],
      ),
    ],
    links: [
      ToolLink(labelKey: 'topic.growth_decay.title', page: 46),
      ToolLink(labelKey: 'topic.cooling.title', page: 47),
      ToolLink(labelKey: 'topic.logistic.title', page: 48),
      ToolLink(labelKey: 'topic.mixing.title', page: 49),
      ToolLink(labelKey: 'topic.spring_mass.title', page: 50),
      ToolLink(labelKey: 'topic.circuit.title', page: 52),
    ],
  ),
];
