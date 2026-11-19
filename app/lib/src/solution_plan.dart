// The compact "how this is solved" summary shown above the step list.
//
// The engine reports the family (`type_id`) and, for calculus, the technique
// (`method`).  This catalogue turns those ids into the formulas a reader needs
// to follow the steps: the standard shape of the problem, the substitution the
// method depends on, and the two or three key formulas in order.
//
// Everything here is language-free LaTeX; the labels around it come from the
// translation catalogue, so the same plan reads correctly in all 12 locales.
import 'models.dart';

class SolutionPlan {
  const SolutionPlan({this.form, this.substitution, this.steps = const []});

  /// Canonical shape of the problem, e.g. `y' + P(x)y = Q(x)`.
  final String? form;

  /// The change of variable the method is built on, e.g. `u = y/x`.
  final String? substitution;

  /// The key formulas in the order the solution uses them.
  final List<String> steps;

  bool get isEmpty => form == null && substitution == null && steps.isEmpty;
}

SolutionPlan? solutionPlanFor(SolveResult result) {
  if (!result.ok) return null;
  final type = result.typeId;
  final byType = _plans[type];
  if (byType != null) return byType;

  if (result.mode == 'integral') {
    return _integralPlan(result.method);
  }
  if (result.mode == 'derivative') {
    return _plans[result.typeId] ?? _plans['derivative'];
  }
  if (result.method.isNotEmpty) {
    final byMethod = _plans['method:${result.method}'];
    if (byMethod != null) return byMethod;
  }
  // Phase portraits report the classification, not the system, as the type.
  if (_portraitTypes.contains(type)) return _plans['phase_portrait'];
  return null;
}

const Set<String> _portraitTypes = <String>{
  'stable_node',
  'unstable_node',
  'saddle',
  'center',
  'stable_spiral',
  'unstable_spiral',
  'stable_degenerate_node',
  'unstable_degenerate_node',
  'degenerate',
};

SolutionPlan _integralPlan(String method) =>
    _plans['integral:$method'] ?? _plans['integral']!;

const Map<String, SolutionPlan> _plans = <String, SolutionPlan>{
  // ---------------------------------------------------------------- first order
  'separable': SolutionPlan(
    form: r"y' = f(x)\,g(y)",
    steps: [
      r"\frac{dy}{g(y)} = f(x)\,dx",
      r"\int \frac{dy}{g(y)} = \int f(x)\,dx + C",
    ],
  ),
  'exact': SolutionPlan(
    form: r"M(x,y)\,dx + N(x,y)\,dy = 0",
    steps: [
      r"\frac{\partial M}{\partial y} = \frac{\partial N}{\partial x}",
      r"u_x = M,\qquad u_y = N",
      r"u(x,y) = C",
    ],
  ),
  'linear': SolutionPlan(
    form: r"y' + P(x)\,y = Q(x)",
    substitution: r"\mu(x) = e^{\int P(x)\,dx}",
    steps: [
      r"(\mu y)' = \mu Q",
      r"y = \frac{1}{\mu}\int \mu Q\,dx + \frac{C}{\mu}",
    ],
  ),
  'bernoulli': SolutionPlan(
    form: r"y' + P(x)\,y = Q(x)\,y^{n}",
    substitution: r"v = y^{1-n}",
    steps: [r"v' + (1-n)P\,v = (1-n)Q", r"y = v^{\frac{1}{1-n}}"],
  ),
  'homogeneous': SolutionPlan(
    form: r"y' = f\left(\frac{y}{x}\right)",
    substitution: r"y = ux \;\Rightarrow\; y' = u + x\,u'",
    steps: [
      r"x\,\frac{du}{dx} = f(u) - u",
      r"\int \frac{du}{f(u)-u} = \int \frac{dx}{x} + C",
    ],
  ),
  'integrating_factor': SolutionPlan(
    form: r"M\,dx + N\,dy = 0",
    substitution: r"\mu(x,y)",
    steps: [
      r"\frac{\partial(\mu M)}{\partial y} = \frac{\partial(\mu N)}{\partial x}",
      r"\mu M\,dx + \mu N\,dy = du",
      r"u(x,y) = C",
    ],
  ),
  'riccati': SolutionPlan(
    form: r"y' = P(x) + Q(x)\,y + R(x)\,y^{2}",
    substitution: r"y = y_1 + \frac{1}{z}",
    steps: [r"z' + (Q + 2R\,y_1)\,z + R = 0", r"y = y_1 + \frac{1}{z}"],
  ),
  'clairaut': SolutionPlan(
    form: r"y = x\,y' + f(y')",
    substitution: r"p = y'",
    steps: [r"y = Cx + f(C)", r"x = -f'(p),\qquad y = f(p) - p\,f'(p)"],
  ),
  'lagrange': SolutionPlan(
    form: r"y = x\,g(y') + f(y')",
    substitution: r"p = y'",
    steps: [
      r"p = g(p) + (x\,g'(p) + f'(p))\frac{dp}{dx}",
      r"\frac{dx}{dp} - \frac{g'(p)}{p - g(p)}\,x = \frac{f'(p)}{p - g(p)}",
    ],
  ),
  'linear_sub': SolutionPlan(
    form: r"y' = f(ax + by + c)",
    substitution: r"u = ax + by + c",
    steps: [r"u' = a + b\,f(u)", r"\int \frac{du}{a + b\,f(u)} = x + C"],
  ),
  'quadrature': SolutionPlan(
    form: r"y' = f(x)",
    steps: [r"y = \int f(x)\,dx + C"],
  ),
  'direct_integration': SolutionPlan(
    form: r"y'' = f(x)",
    steps: [
      r"y' = \int f(x)\,dx + C_1",
      r"y = \int\left(\int f(x)\,dx\right)dx + C_1x + C_2",
    ],
  ),

  // ---------------------------------------------------------------- higher order
  'linear_cc': SolutionPlan(
    form: r"y'' + p\,y' + q\,y = f(x)",
    substitution: r"y = e^{rx}",
    steps: [r"r^{2} + pr + q = 0", r"y = y_h + y_p"],
  ),
  'linear_higher': SolutionPlan(
    form: r"x^{2}y'' + a\,x\,y' + b\,y = 0",
    substitution: r"x = e^{t},\qquad \frac{d}{dx} = \frac{1}{x}\frac{d}{dt}",
    steps: [r"r(r-1) + ar + b = 0", r"y = x^{r}"],
  ),
  'reduction_missing_y': SolutionPlan(
    form: r"F(x,\,y',\,y'') = 0",
    substitution: r"p = y',\qquad y'' = p'",
    steps: [r"F(x,\,p,\,p') = 0"],
  ),
  'reduction_missing_x': SolutionPlan(
    form: r"F(y,\,y',\,y'') = 0",
    substitution: r"p = y'(y)",
    steps: [
      r"y'' = p\,\frac{dp}{dy}",
      r"F\left(y,\,p,\,p\frac{dp}{dy}\right) = 0",
    ],
  ),
  'higher_order': SolutionPlan(
    form: r"y'' = f(x,\,y,\,y')",
    substitution: r"y' = v",
    steps: [r"y' = v,\qquad v' = f(x,\,y,\,v)", r"\text{RK4 on } (y,v)"],
  ),
  'numeric_second_order': SolutionPlan(
    form: r"y'' = f(x,\,y,\,y')",
    substitution: r"y' = v",
    steps: [
      r"y' = v,\qquad v' = f(x,\,y,\,v)",
      r"y_{n+1} = y_n + \frac{h}{6}(k_1 + 2k_2 + 2k_3 + k_4)",
    ],
  ),
  'numeric_first_order': SolutionPlan(
    form: r"y' = f(x,y),\qquad y(x_0) = y_0",
    steps: [r"y_{n+1} = y_n + h\,\Phi(x_n,\,y_n;\,h)", r"e_h \approx C\,h^{p}"],
  ),
  'numeric_higher_order': SolutionPlan(
    form: r"\mathbf{y}' = \mathbf{F}(x,\mathbf{y})",
    substitution: r"y_1 = y,\quad y_2 = y',\ \dots",
    steps: [
      r"\mathbf{y}_{n+1} = \mathbf{y}_n + h\,\Phi(x_n,\,\mathbf{y}_n;\,h)",
    ],
  ),

  // ---------------------------------------------------------------- calculus
  'derivative': SolutionPlan(
    form: r"\frac{df}{dx} = \lim_{h\to 0}\frac{f(x+h)-f(x)}{h}",
    steps: [
      r"(uv)' = u'v + uv'",
      r"\left(\frac{u}{v}\right)' = \frac{u'v - uv'}{v^{2}}",
      r"\big(f(g(x))\big)' = f'(g(x))\,g'(x)",
    ],
  ),
  'derivative_implicit': SolutionPlan(
    form: r"F(x,y) = 0",
    steps: [
      r"\frac{d}{dx}F(x,y) = F_x + F_y\,y' = 0",
      r"y' = -\frac{F_x}{F_y}",
    ],
  ),
  'integral': SolutionPlan(
    form: r"\int f(x)\,dx = F(x) + C",
    steps: [r"\frac{d}{dx}F(x) = f(x)"],
  ),
  'integral:table': SolutionPlan(
    form: r"\int f(x)\,dx = F(x) + C",
    steps: [r"\frac{d}{dx}F(x) = f(x)"],
  ),
  'integral:substitution': SolutionPlan(
    form: r"\int f(g(x))\,g'(x)\,dx",
    substitution: r"u = g(x),\qquad du = g'(x)\,dx",
    steps: [r"\int f(g(x))\,g'(x)\,dx = \int f(u)\,du"],
  ),
  'integral:parts': SolutionPlan(
    form: r"\int u\,dv",
    steps: [r"\int u\,dv = uv - \int v\,du"],
  ),
  'integral:rational': SolutionPlan(
    form: r"\int \frac{P(x)}{Q(x)}\,dx",
    substitution:
        r"\frac{P(x)}{Q(x)} = \sum_k \frac{A_k}{(x-a_k)^{m}} + \cdots",
    steps: [r"\int \frac{dx}{x-a} = \ln|x-a|"],
  ),
  'integral:trig': SolutionPlan(
    form: r"\int \sin^{m}x\,\cos^{n}x\,dx",
    steps: [
      r"\sin^{2}x = \frac{1-\cos 2x}{2}",
      r"\cos^{2}x = \frac{1+\cos 2x}{2}",
    ],
  ),
  'integral:expand': SolutionPlan(
    form: r"\int (af + bg)\,dx",
    steps: [r"\int (af + bg) = a\int f + b\int g"],
  ),
  'integral:constant': SolutionPlan(form: r"\int c\,dx = cx + C"),
  'integral:sum': SolutionPlan(form: r"\int \sum_k f_k = \sum_k \int f_k"),
  'integral_definite': SolutionPlan(
    form: r"\int_a^b f(x)\,dx = F(b) - F(a)",
    steps: [r"F'(x) = f(x)"],
  ),
  'integral_multiple': SolutionPlan(
    form: r"\int\!\!\int_D f(x,y)\,dA = \int\!\!\int f(x,y)\,dx\,dy",
    steps: [r"\text{integrate one variable at a time, then undo } +C"],
  ),
  'integral_multiple_definite': SolutionPlan(
    form: r"\int\!\!\int_D f(x,y)\,dA",
    steps: [r"\int_{c}^{d}\!\!\int_{a}^{b} f(x,y)\,dx\,dy"],
  ),
  'limit': SolutionPlan(
    form: r"\lim_{x\to a} f(x) = L",
    steps: [
      r"\lim\frac{f}{g} = \frac{\lim f}{\lim g}",
      r"\text{0/0 or }\infty/\infty:\quad \lim\frac{f}{g} = \lim\frac{f'}{g'}",
    ],
  ),
  'continuity': SolutionPlan(
    form: r"\lim_{x\to a} f(x) = f(a)",
    steps: [
      r"f(a)\ \text{defined},\quad \lim_{x\to a^-} f = \lim_{x\to a^+} f",
    ],
  ),
  'tangent': SolutionPlan(
    form: r"y - y_0 = m\,(x - x_0)",
    steps: [r"m = f'(x_0)", r"m_{\perp} = -\frac{1}{m}"],
  ),
  'optimize': SolutionPlan(
    form: r"f'(x) = 0",
    steps: [
      r"f''(x) > 0 \Rightarrow \text{minimum},\qquad f''(x) < 0 \Rightarrow \text{maximum}",
      r"\text{compare with } f(a), f(b)",
    ],
  ),
  'behaviour': SolutionPlan(
    form: r"f'(x),\ f''(x)",
    steps: [
      r"f' > 0 \Rightarrow \text{increasing}",
      r"f'' > 0 \Rightarrow \text{convex}",
    ],
  ),
  'maclaurin': SolutionPlan(
    form: r"f(x) = \sum_{k=0}^{n}\frac{f^{(k)}(0)}{k!}\,x^{k} + R_n",
  ),
  'taylor': SolutionPlan(
    form: r"f(x) = \sum_{k=0}^{n}\frac{f^{(k)}(a)}{k!}\,(x-a)^{k} + R_n",
  ),
  'series': SolutionPlan(
    form: r"\sum_{n=1}^{\infty} a_n",
    steps: [
      r"L = \lim_{n\to\infty}\left|\frac{a_{n+1}}{a_n}\right|",
      r"\sum ar^{n} = \frac{a}{1-r}\quad (|r|<1)",
      r"\sum \frac{1}{n^{p}}\ \text{converges} \iff p > 1",
    ],
  ),
  'partial': SolutionPlan(
    form: r"\nabla f = (f_x,\,f_y)",
    steps: [r"D_{\hat u}f = \nabla f \cdot \hat u", r"f_{xy} = f_{yx}"],
  ),
  'mvt': SolutionPlan(form: r"f'(c) = \frac{f(b)-f(a)}{b-a}"),
  'cauchy': SolutionPlan(
    form: r"\frac{f'(c)}{g'(c)} = \frac{f(b)-f(a)}{g(b)-g(a)}",
  ),
  'lhopital': SolutionPlan(form: r"\lim\frac{f}{g} = \lim\frac{f'}{g'}"),
  'area': SolutionPlan(form: r"A = \int_a^b \big(f(x) - g(x)\big)\,dx"),
  'volume': SolutionPlan(
    form: r"V = \pi\int_a^b \big(f^{2} - g^{2}\big)\,dx",
    steps: [r"V = 2\pi\int_a^b x\,f(x)\,dx\ \text{(shells)}"],
  ),
  'arc_length': SolutionPlan(form: r"L = \int_a^b \sqrt{1 + (f'(x))^{2}}\,dx"),
  'related_rates': SolutionPlan(
    form: r"\frac{dA}{dt} = \frac{dA}{dx}\cdot\frac{dx}{dt}",
  ),
  'domain_range': SolutionPlan(
    form: r"D = \{x : f(x)\ \text{is defined}\}",
    steps: [r"R = \{f(x) : x \in D\}"],
  ),
  'wronskian': SolutionPlan(
    form: r"W(f,g) = f g' - f' g",
    steps: [r"W \neq 0 \Rightarrow \{f,g\}\ \text{independent}"],
  ),
  'envelope': SolutionPlan(
    form: r"F(x,y,c) = 0",
    steps: [r"F_c(x,y,c) = 0", r"\text{eliminate } c"],
  ),
  'orthogonal': SolutionPlan(form: r"y' \;\mapsto\; -\frac{1}{y'}"),
  'oblique': SolutionPlan(
    form: r"m_t = \frac{m + \tan\alpha}{1 - m\tan\alpha}",
  ),

  // ---------------------------------------------------------------- systems
  'linear_system': SolutionPlan(
    form: r"\mathbf{x}' = A\mathbf{x}",
    steps: [
      r"|A - \lambda I| = 0",
      r"\mathbf{x} = c_1 e^{\lambda_1 t}v_1 + c_2 e^{\lambda_2 t}v_2",
    ],
  ),
  'linear_solve': SolutionPlan(
    form: r"A\mathbf{x} = \mathbf{b}",
    steps: [
      r"\text{Gauss: } [A \,|\, \mathbf{b}] \to \text{row echelon}",
      r"\text{Cramer: } x_i = \det A_i / \det A",
    ],
  ),
  'matrix_eigen': SolutionPlan(
    form: r"A v = \lambda v",
    steps: [r"|A - \lambda I| = 0", r"(A - \lambda I)v = 0"],
  ),
  'phase_portrait': SolutionPlan(
    form: r"x' = ax + by,\qquad y' = cx + dy",
    steps: [
      r"\tau = a + d,\qquad \Delta = ad - bc",
      r"\tau^{2} - 4\Delta > 0 \Rightarrow \text{node}",
    ],
  ),
  'critical_points': SolutionPlan(
    form: r"F(x,y) = 0,\qquad G(x,y) = 0",
    steps: [r"J = (F_x, F_y;\ G_x, G_y)"],
  ),
  'predator_prey': SolutionPlan(
    form: r"x' = x(a - by),\qquad y' = y(-c + dx)",
    steps: [
      r"\text{equilibria: } (0,0),\ \left(\frac{c}{d},\frac{a}{b}\right)",
    ],
  ),

  // ---------------------------------------------------------------- transforms
  'laplace': SolutionPlan(
    form: r"F(s) = \int_0^{\infty} e^{-st}f(t)\,dt",
    steps: [
      r"\mathcal{L}\{y'\} = sY(s) - y(0)",
      r"\mathcal{L}\{y''\} = s^{2}Y(s) - sy(0) - y'(0)",
    ],
  ),
  'transform_derivatives': SolutionPlan(
    form: r"\mathcal{L}\{y^{(n)}\} = s^{n}Y - s^{n-1}y(0) - \cdots - y^{(n-1)}(0)",
  ),
  'laplace_ivp': SolutionPlan(
    form: r"a y'' + b y' + c y = f(t)",
    steps: [
      r"(as^{2} + bs + c)Y = F(s) + \text{initial terms}",
      r"y(t) = \mathcal{L}^{-1}\{Y(s)\}",
    ],
  ),
  'inverse_laplace': SolutionPlan(
    form: r"y(t) = \mathcal{L}^{-1}\{F(s)\}",
    steps: [r"\text{partial fractions} \;\to\; \text{table}"],
  ),
  'convolution': SolutionPlan(
    form: r"(f * g)(t) = \int_0^{t} f(\tau)\,g(t-\tau)\,d\tau",
    steps: [r"\mathcal{L}\{f * g\} = F(s)\,G(s)"],
  ),
  'step_functions': SolutionPlan(
    form: r"\mathcal{L}\{u(t-a)\} = \frac{e^{-as}}{s}",
    steps: [r"\mathcal{L}\{f(t-a)u(t-a)\} = e^{-as}F(s)"],
  ),
  'discontinuous_forcing': SolutionPlan(
    form: r"\mathcal{L}\{u(t-a)\} = \frac{e^{-as}}{s}",
  ),
  'impulse': SolutionPlan(form: r"\mathcal{L}\{\delta(t-a)\} = e^{-as}"),
  'fourier_series': SolutionPlan(
    form: r"f(x) \sim \frac{a_0}{2} + \sum_{n\ge 1} a_n\cos\frac{n\pi x}{L} + b_n\sin\frac{n\pi x}{L}",
    steps: [
      r"a_n = \frac{1}{L}\int_{-L}^{L} f(x)\cos\frac{n\pi x}{L}\,dx",
      r"b_n = \frac{1}{L}\int_{-L}^{L} f(x)\sin\frac{n\pi x}{L}\,dx",
    ],
  ),
  'fourier_transform': SolutionPlan(
    form: r"F(\omega) = \int_{-\infty}^{\infty} f(x)\,e^{-i\omega x}\,dx",
  ),
  'fourier_convergence': SolutionPlan(
    form: r"\text{piecewise smooth} \Rightarrow \text{series} \to \frac{f(x^+)+f(x^-)}{2}",
  ),
  'fourier_even_odd': SolutionPlan(
    form: r"f\ \text{even} \Rightarrow b_n = 0,\qquad f\ \text{odd} \Rightarrow a_n = 0",
  ),

  // ---------------------------------------------------------------- numerical
  'euler': SolutionPlan(
    form: r"y' = f(x,y)",
    steps: [r"y_{n+1} = y_n + h\,f(x_n,y_n)"],
  ),
  'heun': SolutionPlan(
    form: r"y' = f(x,y)",
    steps: [
      r"\tilde y_{n+1} = y_n + h f(x_n,y_n)",
      r"y_{n+1} = y_n + \frac{h}{2}\big(f(x_n,y_n) + f(x_{n+1},\tilde y_{n+1})\big)",
    ],
  ),
  'rk4': SolutionPlan(
    form: r"y' = f(x,y)",
    steps: [
      r"k_1 = f(x_n,y_n)",
      r"k_2 = f(x_n + \frac{h}{2},\, y_n + \frac{h}{2}k_1)",
      r"y_{n+1} = y_n + \frac{h}{6}(k_1 + 2k_2 + 2k_3 + k_4)",
    ],
  ),
  'multistep': SolutionPlan(
    form: r"y_{n+1} = y_n + h\sum_{j=0}^{s} \beta_j f_{n-j}",
  ),
  'error_convergence': SolutionPlan(
    form: r"e_h \approx C\,h^{p}",
    steps: [r"p = \log_2\frac{e_h}{e_{h/2}}"],
  ),
  'ode_stiff': SolutionPlan(
    form: r"y' = f(x,y)\ \text{with a fast decaying mode}",
    steps: [r"y_{n+1} = y_n + h f(x_{n+1}, y_{n+1})\ \text{(implicit)}"],
  ),

  // ---------------------------------------------------------------- applications
  'growth_decay': SolutionPlan(form: r"y' = k\,y", steps: [r"y = y_0\,e^{kt}"]),
  'cooling': SolutionPlan(
    form: r"T' = -k\,(T - T_m)",
    steps: [r"T(t) = T_m + (T_0 - T_m)e^{-kt}"],
  ),
  'logistic': SolutionPlan(
    form: r"P' = kP\left(1 - \frac{P}{K}\right)",
    steps: [r"P(t) = \frac{K}{1 + \left(\frac{K}{P_0}-1\right)e^{-kt}}"],
  ),
  'mixing': SolutionPlan(form: r"Q' = r\,c_{in} - \frac{r}{V}\,Q"),
  'spring_mass': SolutionPlan(form: r"m\,y'' + c\,y' + k\,y = F(t)"),
  'damped_oscillation': SolutionPlan(
    form: r"m\,y'' + c\,y' + k\,y = 0",
    steps: [r"c^{2} - 4mk > 0 \Rightarrow \text{overdamped}"],
  ),
  'circuit': SolutionPlan(form: r"L\,Q'' + R\,Q' + \frac{1}{C}\,Q = E(t)"),
  'falling_body': SolutionPlan(
    form: r"m\,v' = mg - kv",
    steps: [r"v(t) = \frac{mg}{k} + \left(v_0 - \frac{mg}{k}\right)e^{-kt/m}"],
  ),
  'ode_linear': SolutionPlan(form: r"a\,y'' + b\,y' + c\,y = f(t)"),

  // ---------------------------------------------------------------- PDE
  'pde_first_order': SolutionPlan(
    form: r"u_t + c\,u_x = 0",
    steps: [r"\frac{dt}{1} = \frac{dx}{c} \Rightarrow x - ct = \text{const}"],
  ),
  'pde_classify': SolutionPlan(
    form: r"A u_{xx} + 2B u_{xy} + C u_{yy} = 0",
    steps: [r"B^{2} - AC > 0 \Rightarrow \text{hyperbolic}"],
  ),
  'pde_characteristics': SolutionPlan(
    form: r"u_{xx} = f(u_{xy},u_{yy})",
    steps: [r"\frac{dy}{dx} = \frac{B \pm \sqrt{B^{2}-AC}}{A}"],
  ),
  'pde_separation': SolutionPlan(
    form: r"u(x,t) = X(x)\,T(t)",
    steps: [
      r"\frac{X''}{X} = \frac{T'}{aT} = -\lambda",
      r"X'' + \lambda X = 0,\qquad T' + a\lambda T = 0",
    ],
  ),
  'heat': SolutionPlan(
    form: r"u_t = \alpha\,u_{xx}",
    steps: [r"u = \sum b_n e^{-\alpha (n\pi/L)^{2}t}\sin\frac{n\pi x}{L}"],
  ),
  'wave': SolutionPlan(
    form: r"u_{tt} = c^{2}u_{xx}",
    steps: [
      r"u = \frac{f(x-ct)+f(x+ct)}{2} + \frac{1}{2c}\int_{x-ct}^{x+ct} g(s)\,ds",
    ],
  ),
  'laplace_pde': SolutionPlan(
    form: r"u_{xx} + u_{yy} = 0",
    steps: [r"u = \sum b_n \sin\frac{n\pi x}{L}\,\sinh\frac{n\pi (H-y)}{L}"],
  ),

  // ---------------------------------------------------------------- series
  'series_power': SolutionPlan(
    form: r"y = \sum_{n=0}^{\infty} a_n (x - x_0)^{n}",
  ),
  'series_singular_points': SolutionPlan(
    form: r"(x-x_0)^{2}\frac{q}{p},\ (x-x_0)\frac{r}{p}\ \text{finite}",
    steps: [r"\text{ordinary / regular singular / irregular singular}"],
  ),
  'series_frobenius': SolutionPlan(
    form: r"y = x^{r}\sum_{n=0}^{\infty} a_n x^{n}",
    steps: [r"r(r-1) + p_0 r + q_0 = 0"],
  ),
  'special_legendre': SolutionPlan(
    form: r"(1-x^{2})y'' - 2xy' + n(n+1)y = 0",
    steps: [r"(n+1)P_{n+1} = (2n+1)xP_n - nP_{n-1}"],
  ),
  'special_bessel': SolutionPlan(
    form: r"x^{2}y'' + xy' + (x^{2} - \nu^{2})y = 0",
    steps: [
      r"J_\nu(x) = \sum_{m\ge0}\frac{(-1)^{m}}{m!\,\Gamma(m+\nu+1)}\left(\frac{x}{2}\right)^{2m+\nu}",
    ],
  ),
  'bessel_series': SolutionPlan(
    form: r"f(x) = \sum_{n} c_n J_\nu(\lambda_n x)",
  ),
  'orthogonal_series': SolutionPlan(
    form: r"f = \sum_n \frac{\langle f,\phi_n\rangle}{\langle \phi_n,\phi_n\rangle}\,\phi_n",
  ),
  'euler_equations': SolutionPlan(
    form: r"x^{2}y'' + axy' + by = 0",
    substitution: r"x = e^{t}",
    steps: [r"r(r-1) + ar + b = 0"],
  ),

  // ---------------------------------------------------------------- boundary problems
  'sturm_liouville': SolutionPlan(
    form: r"-(p\,y')' + q\,y = \lambda\,w\,y",
    steps: [
      r"\lambda = \frac{\int (p y'^{2} + q y^{2})}{\int w y^{2}}",
      r"\int_a^b w\,\phi_m\phi_n = 0\ (m \neq n)",
    ],
  ),
  'sturm_liouville_singular': SolutionPlan(
    form: r"-(p\,y')' + q\,y = \lambda\,w\,y,\quad p(a)=0",
    steps: [
      r"\text{bounded } y \text{ at the singular end fixes the spectrum}",
    ],
  ),
  'bvp_two_point': SolutionPlan(
    form: r"-y'' + q(x)y = f(x),\quad y(a)=\alpha,\ y(b)=\beta",
  ),
  'bvp_nonhomogeneous': SolutionPlan(
    form: r"-y'' + q(x)y = f(x),\quad y(a)=\alpha,\ y(b)=\beta",
  ),
  'bvp_occurrence': SolutionPlan(
    form: r"L[y] = f,\quad B_a[y] = \alpha,\ B_b[y] = \beta",
  ),

  // ---------------------------------------------------------------- systems, deeper
  'system_theory': SolutionPlan(
    form: r"\mathbf{x}' = A\mathbf{x} + \mathbf{f}(t)",
    steps: [
      r"\Phi(t) = e^{At}",
      r"\mathbf{x} = \Phi(t)\,\mathbf{c} + \Phi(t)\int \Phi^{-1}(s)\mathbf{f}(s)\,ds",
    ],
  ),
  'system_complex_eigen': SolutionPlan(
    form: r"\lambda = \alpha \pm i\beta",
    steps: [r"e^{\alpha t}\big(\cos\beta t \cdot u - \sin\beta t \cdot v\big)"],
  ),
  'system_fundamental': SolutionPlan(
    form: r"W(t) = |\Phi(t)|",
    steps: [r"\Phi' = A\Phi,\qquad W(t) = W(0)e^{\int tr(A)\,dt}"],
  ),
  'system_repeated': SolutionPlan(
    form: r"(A - \lambda I)v = 0,\quad (A - \lambda I)w = v",
    steps: [r"\mathbf{x} = e^{\lambda t}(c_1 v + c_2(t v + w))"],
  ),
  'system_nonhomogeneous': SolutionPlan(
    form: r"\mathbf{x}' = A\mathbf{x} + \mathbf{f}(t)",
    steps: [r"\mathbf{x}_p = \Phi(t)\int \Phi^{-1}(s)\mathbf{f}(s)\,ds"],
  ),
  'system_rk4': SolutionPlan(
    form: r"\mathbf{x}' = \mathbf{F}(t,\mathbf{x})",
    steps: [
      r"\mathbf{x}_{n+1} = \mathbf{x}_n + \frac{h}{6}(k_1 + 2k_2 + 2k_3 + k_4)",
    ],
  ),
  'competing_species': SolutionPlan(
    form: r"x' = x(a - bx - cy),\qquad y' = y(d - ex - fy)",
  ),
  'liapunov': SolutionPlan(form: r"V(x) > 0,\qquad \dot V(x) < 0"),
  'local_linearization': SolutionPlan(
    form: r"\mathbf{u}' = J\,\mathbf{u},\qquad J = \frac{\partial \mathbf{F}}{\partial \mathbf{x}}",
  ),
  'limit_cycle': SolutionPlan(
    form: r"\mathbf{x}' = \mathbf{F}(\mathbf{x})",
    steps: [
      r"\text{closed orbit } \Gamma:\ \text{trajectories spiral onto it}",
    ],
  ),
  'chaos_attractor': SolutionPlan(
    form:
        r"\mathbf{x}' = \mathbf{F}(\mathbf{x}),\quad \nabla\cdot\mathbf{F} < 0",
    steps: [r"\text{bounded, non-periodic, sensitive to } \mathbf{x}_0"],
  ),

  // ---------------------------------------------------------------- misc topics
  'transcendental': SolutionPlan(
    form: r"f(x) = 0",
    steps: [r"x_{n+1} = x_n - \frac{f(x_n)}{f'(x_n)}"],
  ),
  'integration_techniques': SolutionPlan(
    form: r"\int u\,dv = uv - \int v\,du",
    steps: [r"\int f(g(x))g'(x)\,dx = \int f(u)\,du"],
  ),
  'heat_derivation': SolutionPlan(form: r"c\rho\,u_t = k\,u_{xx}"),
  'wave_derivation': SolutionPlan(form: r"\rho\,u_{tt} = T\,u_{xx}"),
  'heat_other_bc': SolutionPlan(
    form: r"\text{Dirichlet / Neumann / Robin boundary data}",
  ),
};
