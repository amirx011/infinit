import 'l10n.dart';
import 'models.dart';

/// One numbered step in a worked study example.  The title and detail live in
/// the catalogue under `study.<article>.step.<n>.*`.
class StudyStep {
  const StudyStep(this.keyPrefix, {this.math});

  final String keyPrefix;
  final ExView? math;

  String title(L10n l) => l.t('$keyPrefix.title');
  String detail(L10n l) => l.t('$keyPrefix.detail');
}

/// A self-contained study article.  All human-language text comes from the
/// catalogue under `study.<id>.*`; only ids and math stay in code.
class StudyArticle {
  const StudyArticle({
    required this.id,
    required this.shortMath,
    required this.exampleAnswerMath,
    required this.steps,
  });

  final String id;
  final ExView shortMath;
  final ExView exampleAnswerMath;
  final List<StudyStep> steps;

  String title(L10n l) => l.t('study.$id.title');
  String subtitle(L10n l) => l.t('study.$id.subtitle');
  String shortAnswer(L10n l) => l.t('study.$id.short_answer');
  String exampleProblem(L10n l) => l.t('study.$id.example_problem');
  String exampleAnswer(L10n l) => l.t('study.$id.example_answer');
  String pitfalls(L10n l) => l.t('study.$id.pitfalls');
  String source(L10n l) => l.t('study.$id.source');
  List<String> keyIdeas(L10n l) => l.tList('study.$id.key_idea');
}

const kStudyArticles = <StudyArticle>[
  StudyArticle(
    id: 'derivative',
    shortMath: ExView(
      plain: "f'(x) = lim_{h->0} (f(x+h) - f(x)) / h",
      pretty: "f′(x) = lim(h→0) [f(x+h) − f(x)] / h",
      latex: r"f'(x)=\lim_{h\to 0}\frac{f(x+h)-f(x)}{h}",
    ),
    exampleAnswerMath: ExView(
      plain: "f'(x) = 3x^2 + 2,  y = 5x - 2",
      pretty: 'f′(x) = 3x² + 2,   y = 5x − 2',
      latex: r"f'(x)=3x^2+2,\qquad y=5x-2",
    ),
    steps: [
      StudyStep(
        'study.derivative.step.0',
        math: ExView(
          plain: "d/dx[x^3] = 3x^2,  d/dx[2x] = 2",
          pretty: 'd/dx[x³] = 3x²,   d/dx[2x] = 2',
          latex: r"\frac{d}{dx}x^3=3x^2,\qquad \frac{d}{dx}2x=2",
        ),
      ),
      StudyStep(
        'study.derivative.step.1',
        math: ExView(
          plain: "f'(x) = 3x^2 + 2",
          pretty: 'f′(x) = 3x² + 2',
          latex: r"f'(x)=3x^2+2",
        ),
      ),
      StudyStep(
        'study.derivative.step.2',
        math: ExView(
          plain: "f'(1) = 3(1)^2 + 2 = 5",
          pretty: 'f′(1) = 3(1)² + 2 = 5',
          latex: r"f'(1)=3(1)^2+2=5",
        ),
      ),
      StudyStep(
        'study.derivative.step.3',
        math: ExView(
          plain: 'f(1) = 1 + 2 = 3,  (1, 3)',
          pretty: 'f(1) = 1 + 2 = 3,   (1, 3)',
          latex: r"f(1)=1+2=3,\qquad (1,3)",
        ),
      ),
      StudyStep(
        'study.derivative.step.4',
        math: ExView(
          plain: 'y - 3 = 5(x - 1)  =>  y = 5x - 2',
          pretty: 'y − 3 = 5(x − 1)  ⇒  y = 5x − 2',
          latex: r"y-3=5(x-1)\;\Longrightarrow\;y=5x-2",
        ),
      ),
    ],
  ),
  StudyArticle(
    id: 'integral',
    shortMath: ExView(
      plain: 'integral_a^b f(x) dx = F(b) - F(a),  F\' = f',
      pretty: '∫ₐᵇ f(x) dx = F(b) − F(a),   F′ = f',
      latex: r"\int_a^b f(x)\,dx=F(b)-F(a),\qquad F'=f",
    ),
    exampleAnswerMath: ExView(
      plain: 'integral_0^2 (3x^2 + 2x) dx = 12',
      pretty: '∫₀² (3x² + 2x) dx = 12',
      latex: r"\int_0^2 (3x^2+2x)\,dx=12",
    ),
    steps: [
      StudyStep(
        'study.integral.step.0',
        math: ExView(
          plain: 'F(x) = x^3 + x^2',
          pretty: 'F(x) = x³ + x²',
          latex: r"F(x)=x^3+x^2",
        ),
      ),
      StudyStep(
        'study.integral.step.1',
        math: ExView(
          plain: 'F(2) - F(0) = (8 + 4) - (0 + 0)',
          pretty: 'F(2) − F(0) = (8 + 4) − (0 + 0)',
          latex: r"F(2)-F(0)=(8+4)-(0+0)",
        ),
      ),
      StudyStep(
        'study.integral.step.2',
        math: ExView(plain: '12', pretty: '12', latex: r"12"),
      ),
      StudyStep(
        'study.integral.step.3',
        math: ExView(
          plain: "d/dx[x^3 + x^2] = 3x^2 + 2x",
          pretty: 'd/dx[x³ + x²] = 3x² + 2x',
          latex: r"\frac{d}{dx}(x^3+x^2)=3x^2+2x",
        ),
      ),
    ],
  ),
  StudyArticle(
    id: 'ode_types',
    shortMath: ExView(
      plain: 'a_n(x) y^(n) + ... + a_1(x) y\' + a_0(x) y = g(x)',
      pretty: 'aₙ(x)y⁽ⁿ⁾ + … + a₁(x)y′ + a₀(x)y = g(x)',
      latex: r"a_n(x)y^{(n)}+\cdots+a_1(x)y'+a_0(x)y=g(x)",
    ),
    exampleAnswerMath: ExView(
      plain: 'y = C_1 e^(-x) + C_2 e^(-2x) + e^x/6',
      pretty: 'y = C₁e⁻ˣ + C₂e⁻²ˣ + eˣ/6',
      latex: r"y=C_1e^{-x}+C_2e^{-2x}+\frac{e^x}{6}",
    ),
    steps: [
      StudyStep(
        'study.ode_types.step.0',
        math: ExView(
          plain: 'order = 2',
          pretty: 'order = 2',
          latex: r"\text{order}=2",
        ),
      ),
      StudyStep(
        'study.ode_types.step.1',
        math: ExView(
          plain: 'linear, nonhomogeneous, constant coefficients',
          pretty: 'linear, nonhomogeneous, constant coefficients',
          latex: r"\text{linear, nonhomogeneous, constant coefficients}",
        ),
      ),
      StudyStep(
        'study.ode_types.step.2',
        math: ExView(
          plain: 'r = -1, -2  =>  y_h = C_1 e^(-x) + C_2 e^(-2x)',
          pretty: 'r = −1, −2  ⇒  yₕ = C₁e⁻ˣ + C₂e⁻²ˣ',
          latex: r"r=-1,-2\;\Rightarrow\;y_h=C_1e^{-x}+C_2e^{-2x}",
        ),
      ),
      StudyStep(
        'study.ode_types.step.3',
        math: ExView(
          plain: 'A = 1/6  =>  y_p = e^x/6',
          pretty: 'A = 1/6  ⇒  yₚ = eˣ/6',
          latex: r"A=\frac16\;\Rightarrow\;y_p=\frac{e^x}{6}",
        ),
      ),
      StudyStep(
        'study.ode_types.step.4',
        math: ExView(
          plain: 'y = C_1 e^(-x) + C_2 e^(-2x) + e^x/6',
          pretty: 'y = C₁e⁻ˣ + C₂e⁻²ˣ + eˣ/6',
          latex: r"y=C_1e^{-x}+C_2e^{-2x}+\frac{e^x}{6}",
        ),
      ),
    ],
  ),
  StudyArticle(
    id: 'ode_applications',
    shortMath: ExView(
      plain: "model -> solve -> apply initial condition -> interpret",
      pretty: 'model → solve → apply initial condition → interpret',
      latex: r"\text{model}\to\text{solve}\to\text{initial condition}\to\text{interpret}",
    ),
    exampleAnswerMath: ExView(
      plain: 'T(10) = 20 + 70 e^(-1) ~= 45.8',
      pretty: 'T(10) = 20 + 70e⁻¹ ≈ 45.8',
      latex: r"T(10)=20+70e^{-1}\approx 45.8",
    ),
    steps: [
      StudyStep(
        'study.ode_applications.step.0',
        math: ExView(
          plain: "T' = -k(T - T_m)",
          pretty: 'T′ = −k(T − Tₘ)',
          latex: r"T'=-k(T-T_m)",
        ),
      ),
      StudyStep(
        'study.ode_applications.step.1',
        math: ExView(
          plain: 'T(t) = T_m + C e^(-kt)',
          pretty: 'T(t) = Tₘ + C e⁻ᵏᵗ',
          latex: r"T(t)=T_m+Ce^{-kt}",
        ),
      ),
      StudyStep(
        'study.ode_applications.step.2',
        math: ExView(
          plain: 'T(t) = 20 + 70 e^(-0.1 t)',
          pretty: 'T(t) = 20 + 70e⁻⁰·¹ᵗ',
          latex: r"T(t)=20+70e^{-0.1t}",
        ),
      ),
      StudyStep(
        'study.ode_applications.step.3',
        math: ExView(
          plain: 'T(10) = 20 + 70 e^(-1) ~= 45.8',
          pretty: 'T(10) = 20 + 70e⁻¹ ≈ 45.8',
          latex: r"T(10)=20+70e^{-1}\approx 45.8",
        ),
      ),
      StudyStep(
        'study.ode_applications.step.4',
        math: ExView(
          plain: 'T -> 20,  t -> inf',
          pretty: 'T → 20,   t → ∞',
          latex: r"T\to 20,\quad t\to\infty",
        ),
      ),
    ],
  ),
];
