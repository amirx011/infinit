import 'package:flutter/material.dart';

import 'l10n.dart';
import 'models.dart';
import 'tool_links.dart';

/// Top-level shelf in the explanations library.
enum StudySection { differentialEquations, calculus, standalone }

/// A thematic group inside a section.  The library shows one card grid per
/// group and the drawer mirrors the same headings.
enum StudyGroup {
  firstOrder,
  secondOrder,
  seriesSystems,
  modelling,
  calculusFoundations,
  calculusCore,
  calculusSeries,
  standalone,
}

class StudyGroupDef {
  const StudyGroupDef({
    required this.id,
    required this.titleKey,
    required this.icon,
  });

  final StudyGroup id;
  final String titleKey;
  final IconData icon;
}

class StudySectionDef {
  const StudySectionDef({
    required this.id,
    required this.titleKey,
    required this.subtitleKey,
    required this.icon,
    required this.groups,
  });

  final StudySection id;
  final String titleKey;
  final String subtitleKey;
  final IconData icon;
  final List<StudyGroup> groups;
}

const kStudyGroups = <StudyGroup, StudyGroupDef>{
  StudyGroup.firstOrder: StudyGroupDef(
    id: StudyGroup.firstOrder,
    titleKey: 'study.group.first_order',
    icon: Icons.alt_route_rounded,
  ),
  StudyGroup.secondOrder: StudyGroupDef(
    id: StudyGroup.secondOrder,
    titleKey: 'study.group.second_order',
    icon: Icons.show_chart_rounded,
  ),
  StudyGroup.seriesSystems: StudyGroupDef(
    id: StudyGroup.seriesSystems,
    titleKey: 'study.group.series_systems',
    icon: Icons.hub_outlined,
  ),
  StudyGroup.modelling: StudyGroupDef(
    id: StudyGroup.modelling,
    titleKey: 'study.group.modelling',
    icon: Icons.science_outlined,
  ),
  StudyGroup.calculusFoundations: StudyGroupDef(
    id: StudyGroup.calculusFoundations,
    titleKey: 'study.group.calculus_foundations',
    icon: Icons.linear_scale_rounded,
  ),
  StudyGroup.calculusCore: StudyGroupDef(
    id: StudyGroup.calculusCore,
    titleKey: 'study.group.calculus_core',
    icon: Icons.functions_rounded,
  ),
  StudyGroup.calculusSeries: StudyGroupDef(
    id: StudyGroup.calculusSeries,
    titleKey: 'study.group.calculus_series',
    icon: Icons.polyline_rounded,
  ),
  StudyGroup.standalone: StudyGroupDef(
    id: StudyGroup.standalone,
    titleKey: 'study.group.standalone',
    icon: Icons.grid_4x4_rounded,
  ),
};

const kStudySections = <StudySectionDef>[
  StudySectionDef(
    id: StudySection.differentialEquations,
    titleKey: 'study.section.ode',
    subtitleKey: 'study.section.ode.subtitle',
    icon: Icons.science_rounded,
    groups: [
      StudyGroup.firstOrder,
      StudyGroup.secondOrder,
      StudyGroup.seriesSystems,
      StudyGroup.modelling,
    ],
  ),
  StudySectionDef(
    id: StudySection.calculus,
    titleKey: 'study.section.calculus',
    subtitleKey: 'study.section.calculus.subtitle',
    icon: Icons.calculate_rounded,
    groups: [
      StudyGroup.calculusFoundations,
      StudyGroup.calculusCore,
      StudyGroup.calculusSeries,
    ],
  ),
  StudySectionDef(
    id: StudySection.standalone,
    titleKey: 'study.section.standalone',
    subtitleKey: 'study.section.standalone.subtitle',
    icon: Icons.auto_awesome_rounded,
    groups: [StudyGroup.standalone],
  ),
];

/// Page index that hosts the grouped explanations library.
const int kStudyLibraryPage = 125;

/// Stable page index for every article.  The four original articles keep the
/// routes that older builds persisted in `start_page`; the new ones follow.
const Map<String, int> kStudyArticlePages = <String, int>{
  'derivative': 61,
  'integral': 62,
  'ode_types': 63,
  'ode_applications': 64,
  'first_order': 116,
  'second_order': 117,
  'laplace': 118,
  'series_power': 119,
  'systems': 120,
  'numerical': 121,
  'limits': 122,
  'calculus_series': 123,
  'linear_algebra': 124,
};

StudyArticle? studyArticleForPage(int page) {
  for (final article in kStudyArticles) {
    if (kStudyArticlePages[article.id] == page) return article;
  }
  return null;
}

List<StudyArticle> studyArticlesIn(StudyGroup group) => [
  for (final article in kStudyArticles)
    if (article.group == group) article,
];

List<StudyArticle> studyArticlesInSection(StudySection section) => [
  for (final article in kStudyArticles)
    if (article.section == section) article,
];

StudyArticle studyArticleById(String id) =>
    kStudyArticles.firstWhere((article) => article.id == id);

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
    this.icon = Icons.auto_stories_rounded,
    this.section = StudySection.differentialEquations,
    this.group = StudyGroup.firstOrder,
  });

  final String id;
  final ExView shortMath;
  final List<StudyExample> examples;
  final List<ToolLink> links;
  final IconData icon;
  final StudySection section;
  final StudyGroup group;

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
    icon: Icons.drive_file_rename_outline_rounded,
    section: StudySection.calculus,
    group: StudyGroup.calculusCore,
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
    icon: Icons.integration_instructions_rounded,
    section: StudySection.calculus,
    group: StudyGroup.calculusCore,
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
    icon: Icons.category_rounded,
    section: StudySection.differentialEquations,
    group: StudyGroup.firstOrder,
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
    icon: Icons.science_rounded,
    section: StudySection.differentialEquations,
    group: StudyGroup.modelling,
    shortMath: ExView(
      plain: 'model -> solve -> initial condition -> interpret',
      pretty: 'model → solve → initial condition → interpret',
      latex: r"\text{model}\to\text{solve}\to\text{initial condition}\to\text{interpret}",
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
          latex:
              r"P(t)=100e^{0.02t},\qquad t_{2}=\frac{\ln 2}{0.02}\approx 34.7",
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
  StudyArticle(
    id: 'first_order',
    icon: Icons.alt_route_rounded,
    section: StudySection.differentialEquations,
    group: StudyGroup.firstOrder,
    shortMath: ExView(
      plain: "y' + P(x) y = Q(x),   mu(x) = exp(integral P dx)",
      pretty: 'y′ + P(x)y = Q(x),   μ(x) = exp(∫P dx)',
      latex: r"y'+P(x)y=Q(x),\qquad \mu(x)=e^{\int P(x)\,dx}",
    ),
    examples: [
      StudyExample(
        keyPrefix: 'study.first_order.example.0',
        answerMath: ExView(
          plain: 'y = 2x - 1 + 4 exp(-2x)',
          pretty: 'y = 2x − 1 + 4e⁻²ˣ',
          latex: r"y=2x-1+4e^{-2x}",
        ),
        steps: [
          StudyStep(
            'study.first_order.example.0.step.0',
            math: ExView(
              plain: "y' + 2y = 4x",
              pretty: 'y′ + 2y = 4x',
              latex: r"y'+2y=4x",
            ),
          ),
          StudyStep(
            'study.first_order.example.0.step.1',
            math: ExView(
              plain: 'mu = exp(2x)',
              pretty: 'μ = e²ˣ',
              latex: r"\mu=e^{2x}",
            ),
          ),
          StudyStep(
            'study.first_order.example.0.step.2',
            math: ExView(
              plain: "(exp(2x)*y)' = 4x*exp(2x)",
              pretty: '(e²ˣy)′ = 4x e²ˣ',
              latex: r"(e^{2x}y)'=4xe^{2x}",
            ),
          ),
          StudyStep(
            'study.first_order.example.0.step.3',
            math: ExView(
              plain: 'exp(2x)*y = exp(2x)*(2x - 1) + C',
              pretty: 'e²ˣy = e²ˣ(2x − 1) + C',
              latex: r"e^{2x}y=e^{2x}(2x-1)+C",
            ),
          ),
          StudyStep(
            'study.first_order.example.0.step.4',
            math: ExView(
              plain: 'y(0) = 3  =>  C = 4',
              pretty: 'y(0) = 3  ⇒  C = 4',
              latex: r"y(0)=3\;\Rightarrow\;C=4",
            ),
          ),
        ],
      ),
      StudyExample(
        keyPrefix: 'study.first_order.example.1',
        answerMath: ExView(
          plain: 'y = 1 / (x*(1 - ln(x)))',
          pretty: 'y = 1 / (x(1 − ln x))',
          latex: r"y=\frac{1}{x\left(1-\ln x\right)}",
        ),
        steps: [
          StudyStep(
            'study.first_order.example.1.step.0',
            math: ExView(
              plain: "y' + y/x = y^2",
              pretty: 'y′ + y/x = y²',
              latex: r"y'+\frac1x y=y^2",
            ),
          ),
          StudyStep(
            'study.first_order.example.1.step.1',
            math: ExView(
              plain: "v = 1/y  =>  v' - v/x = -1",
              pretty: 'v = 1/y  ⇒  v′ − v/x = −1',
              latex: r"v=\frac1y\;\Rightarrow\;v'-\frac1x v=-1",
            ),
          ),
          StudyStep(
            'study.first_order.example.1.step.2',
            math: ExView(
              plain: 'mu = 1/x',
              pretty: 'μ = 1/x',
              latex: r"\mu=\frac1x",
            ),
          ),
          StudyStep(
            'study.first_order.example.1.step.3',
            math: ExView(
              plain: 'v/x = -ln(x) + C',
              pretty: 'v/x = −ln x + C',
              latex: r"\frac{v}{x}=-\ln x+C",
            ),
          ),
          StudyStep(
            'study.first_order.example.1.step.4',
            math: ExView(
              plain: 'y(1) = 1  =>  C = 1',
              pretty: 'y(1) = 1  ⇒  C = 1',
              latex: r"y(1)=1\;\Rightarrow\;C=1",
            ),
          ),
        ],
      ),
    ],
    links: [
      ToolLink(labelKey: 'ui.solve_an_equation', page: 2, seed: "y' + 2y = 4x"),
      ToolLink(labelKey: 'topic.mixing.title', page: 49),
      ToolLink(labelKey: 'topic.integration_techniques.title', page: 97),
    ],
  ),
  StudyArticle(
    id: 'second_order',
    icon: Icons.show_chart_rounded,
    section: StudySection.differentialEquations,
    group: StudyGroup.secondOrder,
    shortMath: ExView(
      plain: "a y'' + b y' + c y = g(x)",
      pretty: 'a y″ + b y′ + c y = g(x)',
      latex: r"ay''+by'+cy=g(x)",
    ),
    examples: [
      StudyExample(
        keyPrefix: 'study.second_order.example.0',
        answerMath: ExView(
          plain: 'y = (1/3) exp(2x) + (8/3) exp(-x) - 2x^2 + 2x - 3',
          pretty: 'y = ⅓e²ˣ + ⅓·8e⁻ˣ − 2x² + 2x − 3',
          latex: r"y=\frac13e^{2x}+\frac83e^{-x}-2x^2+2x-3",
        ),
        steps: [
          StudyStep(
            'study.second_order.example.0.step.0',
            math: ExView(
              plain: "r^2 - r - 2 = 0  =>  r = 2, -1",
              pretty: 'r² − r − 2 = 0  ⇒  r = 2, −1',
              latex: r"r^2-r-2=0\;\Rightarrow\;r=2,-1",
            ),
          ),
          StudyStep(
            'study.second_order.example.0.step.1',
            math: ExView(
              plain: 'y_h = C_1 exp(2x) + C_2 exp(-x)',
              pretty: 'yₕ = C₁e²ˣ + C₂e⁻ˣ',
              latex: r"y_h=C_1e^{2x}+C_2e^{-x}",
            ),
          ),
          StudyStep(
            'study.second_order.example.0.step.2',
            math: ExView(
              plain: 'y_p = -2x^2 + 2x - 3',
              pretty: 'yₚ = −2x² + 2x − 3',
              latex: r"y_p=-2x^2+2x-3",
            ),
          ),
          StudyStep(
            'study.second_order.example.0.step.3',
            math: ExView(
              plain: 'y(0) = 0,  y\'(0) = 0',
              pretty: 'y(0) = 0,   y′(0) = 0',
              latex: r"y(0)=0,\qquad y'(0)=0",
            ),
          ),
          StudyStep(
            'study.second_order.example.0.step.4',
            math: ExView(
              plain: 'C_1 = 1/3,  C_2 = 8/3',
              pretty: 'C₁ = 1/3,   C₂ = 8/3',
              latex: r"C_1=\frac13,\qquad C_2=\frac83",
            ),
          ),
        ],
      ),
      StudyExample(
        keyPrefix: 'study.second_order.example.1',
        answerMath: ExView(
          plain: 'y = cos(2x) + (3/4) x sin(2x)',
          pretty: 'y = cos 2x + (3/4)x sin 2x',
          latex: r"y=\cos 2x+\frac34 x\sin 2x",
        ),
        steps: [
          StudyStep(
            'study.second_order.example.1.step.0',
            math: ExView(
              plain: 'r^2 + 4 = 0  =>  r = +/-2i',
              pretty: 'r² + 4 = 0  ⇒  r = ±2i',
              latex: r"r^2+4=0\;\Rightarrow\;r=\pm 2i",
            ),
          ),
          StudyStep(
            'study.second_order.example.1.step.1',
            math: ExView(
              plain: 'y_p = A x sin(2x)',
              pretty: 'yₚ = A x sin 2x',
              latex: r"y_p=Ax\sin 2x",
            ),
          ),
          StudyStep(
            'study.second_order.example.1.step.2',
            math: ExView(
              plain: "y_p'' + 4 y_p = 4A cos(2x) = 3 cos(2x)",
              pretty: 'yₚ″ + 4yₚ = 4A cos 2x = 3 cos 2x',
              latex: r"y_p''+4y_p=4A\cos 2x=3\cos 2x",
            ),
          ),
          StudyStep(
            'study.second_order.example.1.step.3',
            math: ExView(
              plain: 'A = 3/4',
              pretty: 'A = 3/4',
              latex: r"A=\frac34",
            ),
          ),
          StudyStep(
            'study.second_order.example.1.step.4',
            math: ExView(
              plain: 'y(0) = 1,  y\'(0) = 0  =>  C_1 = 1, C_2 = 0',
              pretty: 'y(0) = 1,   y′(0) = 0  ⇒  C₁ = 1, C₂ = 0',
              latex: r"y(0)=1,\;y'(0)=0\;\Rightarrow\;C_1=1,\;C_2=0",
            ),
          ),
        ],
      ),
    ],
    links: [
      ToolLink(
        labelKey: 'ui.solve_an_equation',
        page: 2,
        seed: "y'' + y' - 2y = 4x^2",
      ),
      ToolLink(labelKey: 'ui.wronskian', page: 3),
      ToolLink(labelKey: 'topic.spring_mass.title', page: 50),
    ],
  ),
  StudyArticle(
    id: 'laplace',
    icon: Icons.transform_rounded,
    section: StudySection.differentialEquations,
    group: StudyGroup.secondOrder,
    shortMath: ExView(
      plain: 'L{f}(s) = integral_0^inf exp(-s t) f(t) dt',
      pretty: 'ℒ{f}(s) = ∫₀^∞ e⁻ˢᵗ f(t) dt',
      latex: r"\mathcal{L}\{f\}(s)=\int_0^\infty e^{-st}f(t)\,dt",
    ),
    examples: [
      StudyExample(
        keyPrefix: 'study.laplace.example.0',
        answerMath: ExView(
          plain: 'y = 1 - cos(t) + sin(t)',
          pretty: 'y = 1 − cos t + sin t',
          latex: r"y=1-\cos t+\sin t",
        ),
        steps: [
          StudyStep(
            'study.laplace.example.0.step.0',
            math: ExView(
              plain: "s^2 Y - s*0 - 1 + Y = 1/s",
              pretty: 's²Y − s·0 − 1 + Y = 1/s',
              latex: r"s^2Y-s\,y(0)-y'(0)+Y=\frac1s",
            ),
          ),
          StudyStep(
            'study.laplace.example.0.step.1',
            math: ExView(
              plain: '(s^2 + 1) Y = 1 + 1/s',
              pretty: '(s² + 1)Y = 1 + 1/s',
              latex: r"(s^2+1)Y=1+\frac1s",
            ),
          ),
          StudyStep(
            'study.laplace.example.0.step.2',
            math: ExView(
              plain: 'Y = 1/(s(s^2+1)) + 1/(s^2+1)',
              pretty: 'Y = 1/[s(s²+1)] + 1/(s²+1)',
              latex: r"Y=\frac{1}{s(s^2+1)}+\frac{1}{s^2+1}",
            ),
          ),
          StudyStep(
            'study.laplace.example.0.step.3',
            math: ExView(
              plain: 'Y = 1/s - s/(s^2+1) + 1/(s^2+1)',
              pretty: 'Y = 1/s − s/(s²+1) + 1/(s²+1)',
              latex: r"Y=\frac1s-\frac{s}{s^2+1}+\frac{1}{s^2+1}",
            ),
          ),
          StudyStep(
            'study.laplace.example.0.step.4',
            math: ExView(
              plain: 'y = 1 - cos(t) + sin(t)',
              pretty: 'y = 1 − cos t + sin t',
              latex: r"y=1-\cos t+\sin t",
            ),
          ),
        ],
      ),
      StudyExample(
        keyPrefix: 'study.laplace.example.1',
        answerMath: ExView(
          plain: 'y = (1/2) u_2(t) (1 - exp(-2(t-2)))',
          pretty: 'y = ½u₂(t)(1 − e⁻²⁽ᵗ⁻²⁾)',
          latex: r"y=\frac12 u_2(t)\left(1-e^{-2(t-2)}\right)",
        ),
        steps: [
          StudyStep(
            'study.laplace.example.1.step.0',
            math: ExView(
              plain: 's Y + 2Y = exp(-2s)/s',
              pretty: 'sY + 2Y = e⁻²ˢ/s',
              latex: r"sY+2Y=\frac{e^{-2s}}{s}",
            ),
          ),
          StudyStep(
            'study.laplace.example.1.step.1',
            math: ExView(
              plain: 'Y = exp(-2s) / (s(s+2))',
              pretty: 'Y = e⁻²ˢ / [s(s+2)]',
              latex: r"Y=\frac{e^{-2s}}{s(s+2)}",
            ),
          ),
          StudyStep(
            'study.laplace.example.1.step.2',
            math: ExView(
              plain: '1/(s(s+2)) = (1/2)(1/s - 1/(s+2))',
              pretty: '1/[s(s+2)] = ½(1/s − 1/(s+2))',
              latex:
                  r"\frac{1}{s(s+2)}=\frac12\left(\frac1s-\frac1{s+2}\right)",
            ),
          ),
          StudyStep(
            'study.laplace.example.1.step.3',
            math: ExView(
              plain: 'inverse{exp(-2s)/s} = u_2(t)',
              pretty: 'ℒ⁻¹{e⁻²ˢ/s} = u₂(t)',
              latex: r"\mathcal{L}^{-1}\left\{\frac{e^{-2s}}{s}\right\}=u_2(t)",
            ),
          ),
          StudyStep(
            'study.laplace.example.1.step.4',
            math: ExView(
              plain: 'y = (1/2) u_2(t) (1 - exp(-2(t-2)))',
              pretty: 'y = ½u₂(t)(1 − e⁻²⁽ᵗ⁻²⁾)',
              latex: r"y=\frac12u_2(t)\left(1-e^{-2(t-2)}\right)",
            ),
          ),
        ],
      ),
    ],
    links: [
      ToolLink(labelKey: 'topic.laplace.title', page: 37),
      ToolLink(labelKey: 'topic.inverse_laplace.title', page: 38),
      ToolLink(labelKey: 'topic.transform_derivatives.title', page: 114),
    ],
  ),
  StudyArticle(
    id: 'series_power',
    icon: Icons.waves_rounded,
    section: StudySection.differentialEquations,
    group: StudyGroup.seriesSystems,
    shortMath: ExView(
      plain: 'y = sum_{n=0}^inf a_n (x - x_0)^n',
      pretty: 'y = Σₙ₌₀^∞ aₙ(x − x₀)ⁿ',
      latex: r"y=\sum_{n=0}^\infty a_n(x-x_0)^n",
    ),
    examples: [
      StudyExample(
        keyPrefix: 'study.series_power.example.0',
        answerMath: ExView(
          plain: 'y = a_0 (1 + x^3/6 + x^6/180 + ...) + a_1 (x + x^4/12 + ...)',
          pretty: 'y = a₀(1 + x³/6 + x⁶/180 + …) + a₁(x + x⁴/12 + …)',
          latex: r"y=a_0\left(1+\frac{x^3}{6}+\frac{x^6}{180}+\cdots\right)+a_1\left(x+\frac{x^4}{12}+\cdots\right)",
        ),
        steps: [
          StudyStep(
            'study.series_power.example.0.step.0',
            math: ExView(
              plain: "y'' - x y = 0",
              pretty: 'y″ − xy = 0',
              latex: r"y''-xy=0",
            ),
          ),
          StudyStep(
            'study.series_power.example.0.step.1',
            math: ExView(
              plain: 'sum (n+2)(n+1) a_{n+2} x^n = sum a_n x^{n+1}',
              pretty: 'Σ(n+2)(n+1)aₙ₊₂xⁿ = Σaₙxⁿ⁺¹',
              latex: r"\sum_{n=0}^\infty(n+2)(n+1)a_{n+2}x^n=\sum_{n=0}^\infty a_nx^{n+1}",
            ),
          ),
          StudyStep(
            'study.series_power.example.0.step.2',
            math: ExView(
              plain: '2 a_2 = 0,  6 a_3 = a_0,  12 a_4 = a_1',
              pretty: '2a₂ = 0,   6a₃ = a₀,   12a₄ = a₁',
              latex: r"2a_2=0,\qquad 6a_3=a_0,\qquad 12a_4=a_1",
            ),
          ),
          StudyStep(
            'study.series_power.example.0.step.3',
            math: ExView(
              plain: 'a_5 = 0,  30 a_6 = a_3',
              pretty: 'a₅ = 0,   30a₆ = a₃',
              latex: r"a_5=0,\qquad 30a_6=a_3",
            ),
          ),
        ],
      ),
      StudyExample(
        keyPrefix: 'study.series_power.example.1',
        answerMath: ExView(
          plain: 'y = C_1 x + C_2 x^2',
          pretty: 'y = C₁x + C₂x²',
          latex: r"y=C_1x+C_2x^2",
        ),
        steps: [
          StudyStep(
            'study.series_power.example.1.step.0',
            math: ExView(plain: 'y = x^r', pretty: 'y = xʳ', latex: r"y=x^r"),
          ),
          StudyStep(
            'study.series_power.example.1.step.1',
            math: ExView(
              plain: 'r(r-1) - 2r + 2 = 0',
              pretty: 'r(r−1) − 2r + 2 = 0',
              latex: r"r(r-1)-2r+2=0",
            ),
          ),
          StudyStep(
            'study.series_power.example.1.step.2',
            math: ExView(
              plain: 'r^2 - 3r + 2 = 0  =>  r = 1, 2',
              pretty: 'r² − 3r + 2 = 0  ⇒  r = 1, 2',
              latex: r"r^2-3r+2=0\;\Rightarrow\;r=1,2",
            ),
          ),
          StudyStep(
            'study.series_power.example.1.step.3',
            math: ExView(
              plain: 'y = C_1 x + C_2 x^2',
              pretty: 'y = C₁x + C₂x²',
              latex: r"y=C_1x+C_2x^2",
            ),
          ),
        ],
      ),
    ],
    links: [
      ToolLink(labelKey: 'topic.series_power.title', page: 32),
      ToolLink(labelKey: 'topic.series_frobenius.title', page: 34),
      ToolLink(labelKey: 'topic.special_legendre.title', page: 35),
    ],
  ),
  StudyArticle(
    id: 'systems',
    icon: Icons.hub_outlined,
    section: StudySection.differentialEquations,
    group: StudyGroup.seriesSystems,
    shortMath: ExView(
      plain: "x' = A x",
      pretty: 'x′ = Ax',
      latex: r"\mathbf{x}'=A\mathbf{x}",
    ),
    examples: [
      StudyExample(
        keyPrefix: 'study.systems.example.0',
        answerMath: ExView(
          plain: 'x = C_1 exp(3t) (2, 1) + C_2 exp(-t) (-2, 1)',
          pretty: 'x = C₁e³ᵗ(2, 1)ᵀ + C₂e⁻ᵗ(−2, 1)ᵀ',
          latex: r"\mathbf{x}=C_1e^{3t}\begin{pmatrix}2\\1\end{pmatrix}+C_2e^{-t}\begin{pmatrix}-2\\1\end{pmatrix}",
        ),
        steps: [
          StudyStep(
            'study.systems.example.0.step.0',
            math: ExView(
              plain: 'det(A - lambda I) = (1 - lambda)^2 - 4 = 0',
              pretty: 'det(A − λI) = (1 − λ)² − 4 = 0',
              latex: r"\det(A-\lambda I)=(1-\lambda)^2-4=0",
            ),
          ),
          StudyStep(
            'study.systems.example.0.step.1',
            math: ExView(
              plain: 'lambda = 3, -1',
              pretty: 'λ = 3, −1',
              latex: r"\lambda=3,-1",
            ),
          ),
          StudyStep(
            'study.systems.example.0.step.2',
            math: ExView(
              plain: 'lambda = 3:  v = (2, 1)',
              pretty: 'λ = 3:   v = (2, 1)',
              latex: r"\lambda=3:\quad \mathbf{v}=(2,1)",
            ),
          ),
          StudyStep(
            'study.systems.example.0.step.3',
            math: ExView(
              plain: 'lambda = -1:  v = (-2, 1)',
              pretty: 'λ = −1:   v = (−2, 1)',
              latex: r"\lambda=-1:\quad \mathbf{v}=(-2,1)",
            ),
          ),
        ],
      ),
      StudyExample(
        keyPrefix: 'study.systems.example.1',
        answerMath: ExView(
          plain: 'J = [[0, 1], [1, 0]],  lambda = +/-1  =>  saddle',
          pretty: 'J = [[0, 1], [1, 0]],   λ = ±1  ⇒  saddle',
          latex: r"J=\begin{pmatrix}0&1\\1&0\end{pmatrix},\qquad \lambda=\pm1\;\Rightarrow\;\text{saddle}",
        ),
        steps: [
          StudyStep(
            'study.systems.example.1.step.0',
            math: ExView(
              plain: "x' = y,  y' = -sin(x)",
              pretty: 'x′ = y,   y′ = −sin x',
              latex: r"x'=y,\qquad y'=-\sin x",
            ),
          ),
          StudyStep(
            'study.systems.example.1.step.1',
            math: ExView(
              plain: 'critical point (pi, 0)',
              pretty: 'critical point (π, 0)',
              latex: r"(x,y)=(\pi,0)",
            ),
          ),
          StudyStep(
            'study.systems.example.1.step.2',
            math: ExView(
              plain: 'J = [[0, 1], [1, 0]]',
              pretty: 'J = [[0, 1], [1, 0]]',
              latex: r"J=\begin{pmatrix}0&1\\1&0\end{pmatrix}",
            ),
          ),
          StudyStep(
            'study.systems.example.1.step.3',
            math: ExView(
              plain: 'lambda = +/-1  =>  saddle, unstable',
              pretty: 'λ = ±1  ⇒  saddle (unstable)',
              latex: r"\lambda=\pm1\;\Rightarrow\;\text{saddle (unstable)}",
            ),
          ),
        ],
      ),
    ],
    links: [
      ToolLink(labelKey: 'topic.linear_system.title', page: 27),
      ToolLink(labelKey: 'topic.phase_portrait.title', page: 29),
      ToolLink(labelKey: 'topic.system_fundamental.title', page: 77),
    ],
  ),
  StudyArticle(
    id: 'numerical',
    icon: Icons.calculate_rounded,
    section: StudySection.differentialEquations,
    group: StudyGroup.seriesSystems,
    shortMath: ExView(
      plain: 'y_{n+1} = y_n + h f(t_n, y_n)',
      pretty: 'yₙ₊₁ = yₙ + h f(tₙ, yₙ)',
      latex: r"y_{n+1}=y_n+hf(t_n,y_n)",
    ),
    examples: [
      StudyExample(
        keyPrefix: 'study.numerical.example.0',
        answerMath: ExView(
          plain: 'y_1 = 1.1,   y(0.1) = 1.1103...,  error = 0.0103',
          pretty: 'y₁ = 1.1,   y(0.1) = 1.1103…, error ≈ 0.0103',
          latex: r"y_1=1.1,\qquad y(0.1)\approx1.1103,\qquad \text{error}\approx0.0103",
        ),
        steps: [
          StudyStep(
            'study.numerical.example.0.step.0',
            math: ExView(
              plain: 'f(0, 1) = 0 + 1 = 1',
              pretty: 'f(0, 1) = 0 + 1 = 1',
              latex: r"f(0,1)=0+1=1",
            ),
          ),
          StudyStep(
            'study.numerical.example.0.step.1',
            math: ExView(
              plain: 'y_1 = 1 + 0.1*1',
              pretty: 'y₁ = 1 + 0.1·1',
              latex: r"y_1=1+0.1\cdot1",
            ),
          ),
          StudyStep(
            'study.numerical.example.0.step.2',
            math: ExView(
              plain: 'y_1 = 1.1',
              pretty: 'y₁ = 1.1',
              latex: r"y_1=1.1",
            ),
          ),
          StudyStep(
            'study.numerical.example.0.step.3',
            math: ExView(
              plain: 'y(0.1) = 1.1103...,  error = 0.0103',
              pretty: 'y(0.1) = 1.1103…, error ≈ 0.0103',
              latex: r"y(0.1)\approx1.1103,\qquad \text{error}\approx0.0103",
            ),
          ),
        ],
      ),
      StudyExample(
        keyPrefix: 'study.numerical.example.1',
        answerMath: ExView(
          plain: 'y_1 = 1.64844,   exp(0.5) = 1.64872',
          pretty: 'y₁ = 1.64844,   e⁰·⁵ = 1.64872',
          latex: r"y_1\approx1.64844,\qquad e^{0.5}\approx1.64872",
        ),
        steps: [
          StudyStep(
            'study.numerical.example.1.step.0',
            math: ExView(
              plain: 'k_1 = f(0, 1) = 1',
              pretty: 'k₁ = f(0, 1) = 1',
              latex: r"k_1=f(0,1)=1",
            ),
          ),
          StudyStep(
            'study.numerical.example.1.step.1',
            math: ExView(
              plain: 'k_2 = 1.25,  k_3 = 1.3125',
              pretty: 'k₂ = 1.25,   k₃ = 1.3125',
              latex: r"k_2=1.25,\qquad k_3=1.3125",
            ),
          ),
          StudyStep(
            'study.numerical.example.1.step.2',
            math: ExView(
              plain: 'k_4 = 1.65625',
              pretty: 'k₄ = 1.65625',
              latex: r"k_4=1.65625",
            ),
          ),
          StudyStep(
            'study.numerical.example.1.step.3',
            math: ExView(
              plain: 'y_1 = 1 + (0.5/6)(k_1 + 2k_2 + 2k_3 + k_4)',
              pretty: 'y₁ = 1 + (0.5/6)(k₁ + 2k₂ + 2k₃ + k₄)',
              latex: r"y_1=1+\frac{0.5}{6}\left(k_1+2k_2+2k_3+k_4\right)",
            ),
          ),
        ],
      ),
    ],
    links: [
      ToolLink(labelKey: 'topic.rk4.title', page: 43),
      ToolLink(labelKey: 'topic.multistep.title', page: 44),
      ToolLink(labelKey: 'topic.error_convergence.title', page: 45),
      ToolLink(labelKey: 'topic.heun.title', page: 65),
    ],
  ),
  StudyArticle(
    id: 'limits',
    icon: Icons.linear_scale_rounded,
    section: StudySection.calculus,
    group: StudyGroup.calculusFoundations,
    shortMath: ExView(
      plain: 'lim_{x -> a} f(x) = L',
      pretty: 'limₓ→ₐ f(x) = L',
      latex: r"\lim_{x\to a}f(x)=L",
    ),
    examples: [
      StudyExample(
        keyPrefix: 'study.limits.example.0',
        answerMath: ExView(
          plain: 'lim = 4',
          pretty: 'limit = 4',
          latex: r"\lim_{x\to2}\frac{x^2-4}{x-2}=4",
        ),
        steps: [
          StudyStep(
            'study.limits.example.0.step.0',
            math: ExView(
              plain: '(x^2 - 4)/(x - 2) = x + 2,  x != 2',
              pretty: '(x² − 4)/(x − 2) = x + 2,   x ≠ 2',
              latex: r"\frac{x^2-4}{x-2}=x+2,\qquad x\ne2",
            ),
          ),
          StudyStep(
            'study.limits.example.0.step.1',
            math: ExView(
              plain: 'lim (x + 2) = 4',
              pretty: 'lim (x + 2) = 4',
              latex: r"\lim_{x\to2}(x+2)=4",
            ),
          ),
          StudyStep(
            'study.limits.example.0.step.2',
            math: ExView(
              plain: 'hole at x = 2, limit exists',
              pretty: 'hole at x = 2, limit exists',
              latex: r"\text{hole at }x=2,\ \text{limit exists}",
            ),
          ),
        ],
      ),
      StudyExample(
        keyPrefix: 'study.limits.example.1',
        answerMath: ExView(
          plain: 'lim = 1/2',
          pretty: 'limit = 1/2',
          latex: r"\lim_{x\to0}\frac{\sqrt{1+x}-1}{x}=\frac12",
        ),
        steps: [
          StudyStep(
            'study.limits.example.1.step.0',
            math: ExView(
              plain: 'multiply by (sqrt(1+x) + 1)/(sqrt(1+x) + 1)',
              pretty: 'multiply by (√(1+x) + 1)',
              latex: r"\frac{\sqrt{1+x}-1}{x}\cdot\frac{\sqrt{1+x}+1}{\sqrt{1+x}+1}",
            ),
          ),
          StudyStep(
            'study.limits.example.1.step.1',
            math: ExView(
              plain: '= 1 / (sqrt(1+x) + 1)',
              pretty: '= 1 / (√(1+x) + 1)',
              latex: r"=\frac{1}{\sqrt{1+x}+1}",
            ),
          ),
          StudyStep(
            'study.limits.example.1.step.2',
            math: ExView(
              plain: 'x = 0:  1/(1 + 1) = 1/2',
              pretty: 'x = 0:  1/(1 + 1) = 1/2',
              latex: r"x=0:\quad \frac{1}{1+1}=\frac12",
            ),
          ),
        ],
      ),
    ],
    links: [
      ToolLink(labelKey: 'ui.limit', page: 7),
      ToolLink(labelKey: 'ui.continuity', page: 8),
      ToolLink(labelKey: 'ui.derivative', page: 5),
    ],
  ),
  StudyArticle(
    id: 'calculus_series',
    icon: Icons.polyline_rounded,
    section: StudySection.calculus,
    group: StudyGroup.calculusSeries,
    shortMath: ExView(
      plain: 'f(x) = sum f^(n)(a)/n! (x - a)^n',
      pretty: 'f(x) = Σ f⁽ⁿ⁾(a)/n! (x − a)ⁿ',
      latex: r"f(x)=\sum_{n=0}^\infty\frac{f^{(n)}(a)}{n!}(x-a)^n",
    ),
    examples: [
      StudyExample(
        keyPrefix: 'study.calculus_series.example.0',
        answerMath: ExView(
          plain: 'exp(0.1) ~= 1.105167, |R_3| <= 4.6e-6',
          pretty: 'e⁰·¹ ≈ 1.105167, |R₃| ≤ 4.6×10⁻⁶',
          latex: r"e^{0.1}\approx1.105167,\qquad |R_3|\le4.6\times10^{-6}",
        ),
        steps: [
          StudyStep(
            'study.calculus_series.example.0.step.0',
            math: ExView(
              plain: 'exp(x) = 1 + x + x^2/2 + x^3/6 + ...',
              pretty: 'eˣ = 1 + x + x²/2 + x³/6 + …',
              latex: r"e^x=1+x+\frac{x^2}{2}+\frac{x^3}{6}+\cdots",
            ),
          ),
          StudyStep(
            'study.calculus_series.example.0.step.1',
            math: ExView(
              plain: 'x = 0.1:  1 + 0.1 + 0.005 + 0.0001667',
              pretty: 'x = 0.1:  1 + 0.1 + 0.005 + 0.0001667',
              latex: r"x=0.1:\quad 1+0.1+0.005+0.0001667",
            ),
          ),
          StudyStep(
            'study.calculus_series.example.0.step.2',
            math: ExView(
              plain: '= 1.1051667',
              pretty: '= 1.1051667',
              latex: r"=1.1051667",
            ),
          ),
          StudyStep(
            'study.calculus_series.example.0.step.3',
            math: ExView(
              plain: '|R_3| <= exp(0.1)*0.1^4/24 ~= 4.6e-6',
              pretty: '|R₃| ≤ e⁰·¹·0.1⁴/24 ≈ 4.6×10⁻⁶',
              latex:
                  r"|R_3|\le\frac{e^{0.1}}{24}(0.1)^4\approx4.6\times10^{-6}",
            ),
          ),
        ],
      ),
      StudyExample(
        keyPrefix: 'study.calculus_series.example.1',
        answerMath: ExView(
          plain: 'interval = [-1, 1),  R = 1',
          pretty: 'interval = [−1, 1), R = 1',
          latex: r"\text{interval}=[-1,1),\qquad R=1",
        ),
        steps: [
          StudyStep(
            'study.calculus_series.example.1.step.0',
            math: ExView(
              plain: '|a_{n+1}/a_n| = |x| n/(n+1) -> |x|',
              pretty: '|aₙ₊₁/aₙ| = |x|·n/(n+1) → |x|',
              latex: r"\left|\frac{a_{n+1}}{a_n}\right|=|x|\frac{n}{n+1}\to|x|",
            ),
          ),
          StudyStep(
            'study.calculus_series.example.1.step.1',
            math: ExView(
              plain: '|x| < 1  =>  R = 1',
              pretty: '|x| < 1  ⇒  R = 1',
              latex: r"|x|<1\;\Rightarrow\;R=1",
            ),
          ),
          StudyStep(
            'study.calculus_series.example.1.step.2',
            math: ExView(
              plain: 'x = 1: harmonic series diverges',
              pretty: 'x = 1: harmonic series diverges',
              latex: r"x=1:\quad \sum\frac1n\ \text{diverges}",
            ),
          ),
          StudyStep(
            'study.calculus_series.example.1.step.3',
            math: ExView(
              plain: 'x = -1: alternating series converges',
              pretty: 'x = −1: alternating series converges',
              latex: r"x=-1:\quad \sum\frac{(-1)^n}{n}\ \text{converges}",
            ),
          ),
        ],
      ),
    ],
    links: [
      ToolLink(labelKey: 'ui.taylor_maclaurin', page: 23),
      ToolLink(labelKey: 'ui.series_convergence', page: 26),
      ToolLink(labelKey: 'topic.series_power.title', page: 32),
    ],
  ),
  StudyArticle(
    id: 'linear_algebra',
    icon: Icons.grid_4x4_rounded,
    section: StudySection.standalone,
    group: StudyGroup.standalone,
    shortMath: ExView(
      plain: 'A v = lambda v,  det(A - lambda I) = 0',
      pretty: 'Av = λv,  det(A − λI) = 0',
      latex: r"A\mathbf{v}=\lambda\mathbf{v},\qquad \det(A-\lambda I)=0",
    ),
    examples: [
      StudyExample(
        keyPrefix: 'study.linear_algebra.example.0',
        answerMath: ExView(
          plain: 'lambda = 3, 1;  P = [[1,1],[1,-1]]',
          pretty: 'λ = 3, 1;  P = [[1, 1], [1, −1]]',
          latex: r"\lambda=3,1,\qquad P=\begin{pmatrix}1&1\\1&-1\end{pmatrix}",
        ),
        steps: [
          StudyStep(
            'study.linear_algebra.example.0.step.0',
            math: ExView(
              plain: 'det(A - lambda I) = (2 - lambda)^2 - 1 = 0',
              pretty: 'det(A − λI) = (2 − λ)² − 1 = 0',
              latex: r"\det(A-\lambda I)=(2-\lambda)^2-1=0",
            ),
          ),
          StudyStep(
            'study.linear_algebra.example.0.step.1',
            math: ExView(
              plain: 'lambda = 3, 1',
              pretty: 'λ = 3, 1',
              latex: r"\lambda=3,1",
            ),
          ),
          StudyStep(
            'study.linear_algebra.example.0.step.2',
            math: ExView(
              plain: 'lambda = 3:  v = (1, 1)',
              pretty: 'λ = 3:  v = (1, 1)',
              latex: r"\lambda=3:\quad \mathbf{v}=(1,1)",
            ),
          ),
          StudyStep(
            'study.linear_algebra.example.0.step.3',
            math: ExView(
              plain: 'lambda = 1:  v = (1, -1)',
              pretty: 'λ = 1:  v = (1, −1)',
              latex: r"\lambda=1:\quad \mathbf{v}=(1,-1)",
            ),
          ),
        ],
      ),
      StudyExample(
        keyPrefix: 'study.linear_algebra.example.1',
        answerMath: ExView(
          plain: 'det = 22, invertible',
          pretty: 'det = 22, invertible',
          latex: r"\det A=22,\qquad A\ \text{is invertible}",
        ),
        steps: [
          StudyStep(
            'study.linear_algebra.example.1.step.0',
            math: ExView(
              plain: 'det = 1*(4*6 - 5*0) - 0 + 1*(2*5 - 3*4)',
              pretty: 'det = 1(4·6 − 5·0) − 0 + 1(2·5 − 3·4)',
              latex: r"\det A=1(24-0)-0+1(10-12)",
            ),
          ),
          StudyStep(
            'study.linear_algebra.example.1.step.1',
            math: ExView(
              plain: '= 24 - 2 = 22',
              pretty: '= 24 − 2 = 22',
              latex: r"=24-2=22",
            ),
          ),
          StudyStep(
            'study.linear_algebra.example.1.step.2',
            math: ExView(
              plain: 'det != 0  =>  invertible',
              pretty: 'det ≠ 0  ⇒  invertible',
              latex: r"\det A\ne0\;\Rightarrow\;A\ \text{is invertible}",
            ),
          ),
        ],
      ),
    ],
    links: [
      ToolLink(labelKey: 'ui.linear_algebra', page: 1),
      ToolLink(labelKey: 'ui.wronskian', page: 3),
      ToolLink(labelKey: 'topic.matrix_eigen.title', page: 28),
    ],
  ),
];
