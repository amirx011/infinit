import 'l10n.dart';
import 'models.dart';
import 'tool_links.dart';

/// The computation attached to an article.  Most sections hand the worked
/// example to the shared equation solver; the conceptual sections get a small
/// tool that computes the object they are about.
enum CurriculumTool {
  solver,
  directionField,
  classification,
  existence,
  difference,
}

/// A short article for a curriculum slot.  Every sentence comes from the
/// catalogue under `cur.<index>.*`; only ids and mathematics live in code.
class CurriculumArticle {
  const CurriculumArticle({
    required this.index,
    required this.standard,
    required this.exampleAnswerMath,
    required this.input,
    this.tool = CurriculumTool.solver,
    this.point = '0,1',
    this.links = const [],
  });

  final int index;
  final ExView standard;
  final ExView exampleAnswerMath;

  /// Seed for the computation: an equation for the solver, a right-hand side
  /// for the direction field, an expression to differentiate, or the
  /// `a, b, y0` triple of a difference equation.
  final String input;
  final CurriculumTool tool;

  /// Evaluation point for the existence tool.
  final String point;

  /// Buttons to the workbench that solves this family.
  final List<ToolLink> links;

  int get chapter => switch (index) {
    98 || 66 || 99 || 100 || 101 || 102 || 103 || 67 || 68 => 2,
    104 || 105 || 106 || 107 || 108 || 69 => 3,
    70 || 109 || 110 || 71 => 4,
    _ => 2,
  };

  String get titleKey => 'nav.$index.label';
  String get subtitleKey => 'cur.ch$chapter.subtitle';
  String get ideaKey => 'cur.$index.idea';
  String get algorithmKey => 'cur.$index.algorithm';
  String get exampleKey => 'cur.$index.example';
  String get answerKey => 'cur.$index.answer';
  String get pitfallKey => 'cur.$index.pitfall';

  String title(L10n l) => l.t(titleKey);
  String subtitle(L10n l) => l.t(subtitleKey);
  String idea(L10n l) => l.t(ideaKey);
  String algorithm(L10n l) => l.t(algorithmKey);
  String example(L10n l) => l.t(exampleKey);
  String answer(L10n l) => l.t(answerKey);
  String pitfall(L10n l) => l.t(pitfallKey);

  /// The long-form, Boyce-style explanation.  Inline math uses `$...$`.
  List<String> theory(L10n l) => l.tList('cur.$index.theory');
}

ExView _math(String plain, String pretty, String latex) =>
    ExView(plain: plain, pretty: pretty, latex: latex);

/// Curriculum slots covered by the chapter 2–4 reader, in reading order.
final kCurriculumArticles = <CurriculumArticle>[
  CurriculumArticle(
    index: 98,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y' = y*(2-y)"),
    ],
    input: "y' = y*(2-y)",
    tool: CurriculumTool.directionField,
    standard: ExView(
      plain: "y' = f(x, y)",
      pretty: 'y′ = f(x, y)',
      latex: r"y'=f(x,y)",
    ),
    exampleAnswerMath: _math("y' = y*(2 - y)", 'y′ = y(2 − y)', r"y'=y(2-y)"),
  ),
  CurriculumArticle(
    index: 66,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y'' + 3*y' + 2*y = exp(x)"),
    ],
    input: "y'' + 3*y' + 2*y = exp(x)",
    tool: CurriculumTool.classification,
    standard: ExView(
      plain: "a_n(x)*y^(n) + ... + a_0(x)*y = g(x)",
      pretty: 'aₙ(x)y⁽ⁿ⁾ + … + a₀(x)y = g(x)',
      latex: r"a_n(x)y^{(n)}+\cdots+a_0(x)y=g(x)",
    ),
    exampleAnswerMath: _math(
      "y'' + 3y' + 2y = exp(x),  y' = y^2",
      'y″ + 3y′ + 2y = eˣ,   y′ = y²',
      r"y''+3y'+2y=e^x,\qquad y'=y^2",
    ),
  ),
  CurriculumArticle(
    index: 99,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y' + y/x = x^2"),
    ],
    input: "y' + y/x = x^2",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "y' + P(x)*y = Q(x),  mu = exp(integral P dx)",
      pretty: 'y′ + P(x)y = Q(x),   μ = exp(∫P dx)',
      latex: r"y'+P(x)y=Q(x),\qquad \mu=e^{\int P\,dx}",
    ),
    exampleAnswerMath: _math(
      "y = x^3/4 + C/x",
      'y = x³/4 + C/x',
      r"y=\frac{x^3}{4}+\frac{C}{x}",
    ),
  ),
  CurriculumArticle(
    index: 100,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y' = 4*x*y/(x^2+1)"),
    ],
    input: "y' = 4*x*y/(x^2+1)",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "y' = f(x)*g(y)",
      pretty: 'y′ = f(x)·g(y)',
      latex: r"y'=f(x)g(y)",
    ),
    exampleAnswerMath: _math(
      "y = C*(x^2 + 1)^2",
      'y = C(x² + 1)²',
      r"y=C(x^2+1)^2",
    ),
  ),
  CurriculumArticle(
    index: 101,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "(2*x+y) + (x+2*y)*y' = 0"),
    ],
    input: "(2*x+y) + (x+2*y)*y' = 0",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "M(x,y) dx + N(x,y) dy = 0,  M_y = N_x",
      pretty: 'M(x, y) dx + N(x, y) dy = 0,   M_y = N_x',
      latex: r"M(x,y)\,dx+N(x,y)\,dy=0,\qquad M_y=N_x",
    ),
    exampleAnswerMath: _math(
      "x^2 + x*y + y^2 = C",
      'x² + xy + y² = C',
      r"x^2+xy+y^2=C",
    ),
  ),
  CurriculumArticle(
    index: 102,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y' = (x^2+y^2)/(x*y)"),
    ],
    input: "y' = (x^2+y^2)/(x*y)",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "y' = f(y/x),  y = v*x",
      pretty: 'y′ = f(y/x),   y = vx',
      latex: r"y'=f\!\left(\frac{y}{x}\right),\qquad y=vx",
    ),
    exampleAnswerMath: _math(
      "y^2 = x^2*(2*ln(abs(x)) + C)",
      'y² = x²(2 ln|x| + C)',
      r"y^2=x^2\left(2\ln|x|+C\right)",
    ),
  ),
  CurriculumArticle(
    index: 103,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y' = y + y^2"),
    ],
    input: "y' = y + y^2",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "y' + P(x)*y = Q(x)*y^n",
      pretty: 'y′ + P(x)y = Q(x)yⁿ',
      latex: r"y'+P(x)y=Q(x)y^n",
    ),
    exampleAnswerMath: _math(
      "y = 1/(C*exp(-x) - 1)",
      'y = 1/(Ce⁻ˣ − 1)',
      r"y=\frac{1}{Ce^{-x}-1}",
    ),
  ),
  CurriculumArticle(
    index: 67,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y' = y^2"),
    ],
    input: "y^2",
    tool: CurriculumTool.existence,
    point: '0,1',
    standard: ExView(
      plain: "y' = f(x,y),  y(x0) = y0",
      pretty: 'y′ = f(x, y),   y(x₀) = y₀',
      latex: r"y'=f(x,y),\qquad y(x_0)=y_0",
    ),
    exampleAnswerMath: _math(
      "y = 1/(1 - x)",
      'y = 1/(1 − x)',
      r"y=\frac{1}{1-x}",
    ),
  ),
  CurriculumArticle(
    index: 68,
    input: "0.5, 3, 1",
    tool: CurriculumTool.difference,
    standard: ExView(
      plain: "y_{n+1} = f(n, y_n)",
      pretty: 'yₙ₊₁ = f(n, yₙ)',
      latex: r"y_{n+1}=f(n,y_n)",
    ),
    exampleAnswerMath: _math(
      "y_n = 6 - 5*(0.5)^n",
      'yₙ = 6 − 5(0.5)ⁿ',
      r"y_n=6-5(0.5)^n",
    ),
  ),
  CurriculumArticle(
    index: 104,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y'' - 3*y' + 2*y = 0"),
    ],
    input: "y'' - 3*y' + 2*y = 0",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "a*y'' + b*y' + c*y = 0,  a*r^2 + b*r + c = 0",
      pretty: 'ay″ + by′ + cy = 0,   ar² + br + c = 0',
      latex: r"ay''+by'+cy=0,\qquad ar^2+br+c=0",
    ),
    exampleAnswerMath: _math(
      "y = C1*exp(x) + C2*exp(2*x)",
      'y = C₁eˣ + C₂e²ˣ',
      r"y=C_1e^x+C_2e^{2x}",
    ),
  ),
  CurriculumArticle(
    index: 105,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y'' + y = 0"),
      ToolLink(labelKey: 'topic.damped_oscillation.title', page: 51),
    ],
    input: "y'' + y = 0",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "r = alpha +/- i*beta",
      pretty: 'r = α ± iβ',
      latex: r"r=\alpha\pm i\beta",
    ),
    exampleAnswerMath: _math(
      "y = C1*cos(x) + C2*sin(x)",
      'y = C₁ cos x + C₂ sin x',
      r"y=C_1\cos x+C_2\sin x",
    ),
  ),
  CurriculumArticle(
    index: 106,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y'' + 4*y' + 4*y = 0"),
    ],
    input: "y'' + 4*y' + 4*y = 0",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "(D - r)^2*y = 0",
      pretty: '(D − r)²y = 0',
      latex: r"(D-r)^2y=0",
    ),
    exampleAnswerMath: _math(
      "y = (C1 + C2*x)*exp(-2*x)",
      'y = (C₁ + C₂x)e⁻²ˣ',
      r"y=(C_1+C_2x)e^{-2x}",
    ),
  ),
  CurriculumArticle(
    index: 107,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y'' - 3*y' + 2*y = exp(x)"),
    ],
    input: "y'' - 3*y' + 2*y = exp(x)",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "a*y'' + b*y' + c*y = g(x)",
      pretty: 'ay″ + by′ + cy = g(x)',
      latex: r"ay''+by'+cy=g(x)",
    ),
    exampleAnswerMath: _math(
      "y = C1*exp(x) + C2*exp(2*x) - x*exp(x)",
      'y = C₁eˣ + C₂e²ˣ − xeˣ',
      r"y=C_1e^x+C_2e^{2x}-xe^x",
    ),
  ),
  CurriculumArticle(
    index: 108,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y'' + y = sec(x)"),
    ],
    input: "y'' + y = sec(x)",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "y = u1(x)*y1(x) + u2(x)*y2(x)",
      pretty: 'y = u₁(x)y₁(x) + u₂(x)y₂(x)',
      latex: r"y=u_1(x)y_1(x)+u_2(x)y_2(x)",
    ),
    exampleAnswerMath: _math(
      "y = C1*cos(x) + C2*sin(x) + cos(x)*ln(abs(cos(x))) + x*sin(x)",
      'y = C₁ cos x + C₂ sin x + cos x ln|cos x| + x sin x',
      r"y=C_1\cos x+C_2\sin x+\cos x\ln|\cos x|+x\sin x",
    ),
  ),
  CurriculumArticle(
    index: 69,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y'' + 9*y = 5*cos(2*x)"),
      ToolLink(labelKey: 'topic.spring_mass.title', page: 50),
    ],
    input: "y'' + 9*y = 5*cos(2*x)",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "m*y'' + gamma*y' + k*y = F0*cos(omega*t)",
      pretty: 'my″ + γy′ + ky = F₀ cos(ωt)',
      latex: r"my''+\gamma y'+ky=F_0\cos(\omega t)",
    ),
    exampleAnswerMath: _math(
      "y = C1*cos(w0*t) + C2*sin(w0*t) + F0*cos(w*t)/(w0^2 - w^2)",
      'y = C₁ cos ω₀t + C₂ sin ω₀t + F₀ cos ωt/(ω₀² − ω²)',
      r"y=C_1\cos(\omega_0t)+C_2\sin(\omega_0t)+\frac{F_0\cos(\omega t)}{\omega_0^2-\omega^2}",
    ),
  ),
  CurriculumArticle(
    index: 70,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y''' - y = 0"),
    ],
    input: "y''' - y = 0",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "y^(n) + p1(x)*y^(n-1) + ... + pn(x)*y = g(x)",
      pretty: 'y⁽ⁿ⁾ + p₁(x)y⁽ⁿ⁻¹⁾ + … + pₙ(x)y = g(x)',
      latex: r"y^{(n)}+p_1(x)y^{(n-1)}+\cdots+p_n(x)y=g(x)",
    ),
    exampleAnswerMath: _math(
      "y = C1*exp(x) + exp(-x/2)*(C2*cos(sqrt(3)*x/2) + C3*sin(sqrt(3)*x/2))",
      'y = C₁eˣ + e⁻ˣᐟ²(C₂ cos(√3x/2) + C₃ sin(√3x/2))',
      r"y=C_1e^x+e^{-x/2}\left(C_2\cos\frac{\sqrt3x}{2}+C_3\sin\frac{\sqrt3x}{2}\right)",
    ),
  ),
  CurriculumArticle(
    index: 109,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y''' - 3*y'' + 3*y' - y = 0"),
    ],
    input: "y''' - 3*y'' + 3*y' - y = 0",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "a_n*y^(n) + ... + a_0*y = 0",
      pretty: 'aₙy⁽ⁿ⁾ + … + a₀y = 0',
      latex: r"a_ny^{(n)}+\cdots+a_0y=0",
    ),
    exampleAnswerMath: _math(
      "y = (C1 + C2*x + C3*x^2)*exp(x)",
      'y = (C₁ + C₂x + C₃x²)eˣ',
      r"y=(C_1+C_2x+C_3x^2)e^x",
    ),
  ),
  CurriculumArticle(
    index: 110,
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y''' - y'' = exp(x)"),
    ],
    input: "y''' - y'' = exp(x)",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "L[y] = g(x)",
      pretty: 'L[y] = g(x)',
      latex: r"L[y]=g(x)",
    ),
    exampleAnswerMath: _math(
      "y = C1 + C2*x + C3*exp(x) + x*exp(x)",
      'y = C₁ + C₂x + C₃eˣ + xeˣ',
      r"y=C_1+C_2x+C_3e^x+xe^x",
    ),
  ),
  CurriculumArticle(
    index: 71,
    links: [
      ToolLink(labelKey: 'ui.wronskian', page: 3),
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y''' - y' = 1"),
    ],
    input: "y''' - y' = 1",
    tool: CurriculumTool.solver,
    standard: ExView(
      plain: "W = det([y1, ..., yn; ...; y1^(n-1), ..., yn^(n-1)])",
      pretty: 'W = det[y₁ … yₙ; …; y₁⁽ⁿ⁻¹⁾ … yₙ⁽ⁿ⁻¹⁾]',
      latex: r"W=\det\begin{pmatrix}y_1&\cdots&y_n\\ \vdots&&\vdots\\ y_1^{(n-1)}&\cdots&y_n^{(n-1)}\end{pmatrix}",
    ),
    exampleAnswerMath: _math(
      "y = C1 + C2*exp(x) + C3*exp(-x) - x",
      'y = C₁ + C₂eˣ + C₃e⁻ˣ − x',
      r"y=C_1+C_2e^x+C_3e^{-x}-x",
    ),
  ),
];
