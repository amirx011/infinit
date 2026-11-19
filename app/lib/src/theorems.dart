// Reference catalogue of the theorems of the derivative.
//
// The content is data, not logic: the theorems page renders these entries and
// each one carries its own bilingual statement, LaTeX and pitfalls. Keeping it
// here means adding a theorem never touches a widget.
import 'l10n.dart';

/// One theorem of the derivative.  All human-language text lives in the
/// catalogue under `theorem.<id>.*`; only the id, the LaTeX statement and the
/// list lengths stay in code.
class TheoremEntry {
  const TheoremEntry({required this.id, required this.latex});

  final String id;
  final String latex;

  String title(L10n l) => l.t('theorem.$id.title');
  String statement(L10n l) => l.t('theorem.$id.statement');
  String why(L10n l) => l.t('theorem.$id.why');
  List<String> hypotheses(L10n l) => l.tList('theorem.$id.hypothesis');
  List<String> pitfalls(L10n l) => l.tList('theorem.$id.pitfall');
}

/// The derivative theorems, in teaching order.
const theoremCatalog = <TheoremEntry>[
  TheoremEntry(
    id: 'fermat',
    latex: r"f'(c) = 0 \quad \text{at a local extremum where } f \text{ is differentiable}",
  ),
  TheoremEntry(
    id: 'rolle',
    latex: r"f(a)=f(b) \;\Rightarrow\; \exists\, c \in (a,b):\; f'(c)=0",
  ),
  TheoremEntry(
    id: 'mvt',
    latex: r"f'(c) = \frac{f(b)-f(a)}{b-a} \quad \text{for some } c \in (a,b)",
  ),
  TheoremEntry(
    id: 'cauchy',
    latex: r"\frac{f'(c)}{g'(c)} = \frac{f(b)-f(a)}{g(b)-g(a)} \quad \text{for some } c \in (a,b)",
  ),
  TheoremEntry(
    id: 'lhopital',
    latex: r"\lim_{x\to a}\frac{f(x)}{g(x)} = \lim_{x\to a}\frac{f'(x)}{g'(x)}",
  ),
  TheoremEntry(
    id: 'zero_derivative',
    latex: r"f'(x)=0 \;\forall x \in I \;\Rightarrow\; f \text{ is constant on } I",
  ),
  TheoremEntry(
    id: 'monotonicity',
    latex: r"f'(x) > 0 \Rightarrow f \nearrow \qquad f'(x) < 0 \Rightarrow f \searrow",
  ),
  TheoremEntry(
    id: 'first_derivative_test',
    latex: r"f': + \to - \Rightarrow \text{max} \qquad f': - \to + \Rightarrow \text{min}",
  ),
  TheoremEntry(
    id: 'second_derivative_test',
    latex: r"f'(c)=0,\; f''(c)>0 \Rightarrow \text{min} \qquad f'(c)=0,\; f''(c)<0 \Rightarrow \text{max}",
  ),
  TheoremEntry(
    id: 'inflection',
    latex: r"f'' \text{ changes sign at } c \;\Rightarrow\; (c, f(c)) \text{ is an inflection point}",
  ),
  TheoremEntry(
    id: 'evt',
    latex: r"f \in C[a,b] \;\Rightarrow\; \exists\, x_1,x_2 \in [a,b]:\; f(x_1) \le f(x) \le f(x_2)",
  ),
  TheoremEntry(
    id: 'differentiable_implies_continuous',
    latex: r"f \text{ differentiable at } c \;\Rightarrow\; f \text{ continuous at } c",
  ),
  TheoremEntry(
    id: 'darboux',
    latex: r"f'(a) < k < f'(b) \;\Rightarrow\; \exists\, c:\; f'(c)=k",
  ),
  TheoremEntry(
    id: 'taylor',
    latex: r"f(x)=\sum_{k=0}^{n}\frac{f^{(k)}(a)}{k!}(x-a)^k+\frac{f^{(n+1)}(\xi)}{(n+1)!}(x-a)^{n+1}",
  ),
  TheoremEntry(
    id: 'linear_approximation',
    latex: r"f(x) \approx f(a) + f'(a)(x-a) \qquad \Delta y \approx f'(a)\,\Delta x",
  ),
  TheoremEntry(
    id: 'differentiation_rules',
    latex: r"(fg)'=f'g+fg' \qquad \left(\frac{f}{g}\right)'=\frac{f'g-fg'}{g^{2}} \qquad \frac{dy}{dx}=\frac{dy}{du}\frac{du}{dx}",
  ),
];
