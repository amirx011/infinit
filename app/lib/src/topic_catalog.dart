import 'package:flutter/material.dart';

/// One editable input on a topic workbench.
class TopicField {
  const TopicField(
    this.key, {
    required this.labelKey,
    this.hintKey = '',
    this.initial = '',
    this.numeric = false,
  });

  final String key;
  final String labelKey;
  final String hintKey;
  final String initial;
  final bool numeric;
}

/// A curriculum topic that the shared C++/Julia workbench can solve.
class TopicSpec {
  const TopicSpec({
    required this.id,
    required this.icon,
    required this.fields,
    this.examples = const [],
    String? backendId,
    String? keyPrefix,
  }) : backendId = backendId ?? id,
       keyPrefix = keyPrefix ?? id;

  /// The topic id sent to the engine.  Usually the same as [id], but an alias
  /// spec can reuse a solver under a more specific heading.
  final String id;
  final String backendId;

  /// Prefix of the catalogue keys for this spec (`topic.<keyPrefix>.title`).
  final String keyPrefix;
  final IconData icon;
  final List<TopicField> fields;
  final List<String> examples;

  String get titleKey => 'topic.$keyPrefix.title';
  String get blurbKey => 'topic.$keyPrefix.blurb';
}

const _oscillatorFields = <TopicField>[
  TopicField(
    'm',
    labelKey: 'topic.oscillator.field.m.label',
    initial: '1',
    numeric: true,
  ),
  TopicField(
    'c',
    labelKey: 'topic.oscillator.field.c.label',
    initial: '0.5',
    numeric: true,
  ),
  TopicField(
    'k',
    labelKey: 'topic.oscillator.field.k.label',
    initial: '4',
    numeric: true,
  ),
  TopicField(
    'f0',
    labelKey: 'topic.oscillator.field.f0.label',
    initial: '0',
    numeric: true,
  ),
  TopicField(
    'w',
    labelKey: 'topic.oscillator.field.w.label',
    initial: '1',
    numeric: true,
  ),
  TopicField(
    'y0',
    labelKey: 'topic.oscillator.field.y0.label',
    initial: '1',
    numeric: true,
  ),
  TopicField(
    'v0',
    labelKey: 'topic.oscillator.field.v0.label',
    initial: '0',
    numeric: true,
  ),
];

const kTopicCatalog = <String, TopicSpec>{
  'growth_decay': TopicSpec(
    id: 'growth_decay',
    icon: Icons.trending_up_rounded,
    fields: [
      TopicField(
        'k',
        labelKey: 'topic.growth_decay.field.k.label',
        initial: '0.2',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.growth_decay.field.y0.label',
        initial: '100',
        numeric: true,
      ),
      TopicField(
        't',
        labelKey: 'topic.growth_decay.field.t.label',
        initial: '5',
        numeric: true,
      ),
    ],
    examples: ['k = 0.2, y₀ = 100, t = 5'],
  ),
  'cooling': TopicSpec(
    id: 'cooling',
    icon: Icons.thermostat_rounded,
    fields: [
      TopicField(
        'k',
        labelKey: 'topic.cooling.field.k.label',
        initial: '0.1',
        numeric: true,
      ),
      TopicField(
        'room',
        labelKey: 'topic.cooling.field.room.label',
        initial: '20',
        numeric: true,
      ),
      TopicField(
        't0',
        labelKey: 'topic.cooling.field.t0.label',
        initial: '90',
        numeric: true,
      ),
      TopicField(
        't',
        labelKey: 'topic.cooling.field.t.label',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: ['k = 0.1, room = 20, T₀ = 90, t = 10'],
  ),
  'logistic': TopicSpec(
    id: 'logistic',
    icon: Icons.groups_rounded,
    fields: [
      TopicField(
        'r',
        labelKey: 'topic.logistic.field.r.label',
        initial: '0.5',
        numeric: true,
      ),
      TopicField(
        'K',
        labelKey: 'topic.logistic.field.K.label',
        initial: '1000',
        numeric: true,
      ),
      TopicField(
        'p0',
        labelKey: 'topic.logistic.field.p0.label',
        initial: '50',
        numeric: true,
      ),
      TopicField(
        't',
        labelKey: 'topic.logistic.field.t.label',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: ['r = 0.5, K = 1000, P₀ = 50, t = 10'],
  ),
  'mixing': TopicSpec(
    id: 'mixing',
    icon: Icons.science_rounded,
    fields: [
      TopicField(
        'V0',
        labelKey: 'topic.mixing.field.V0.label',
        initial: '100',
        numeric: true,
      ),
      TopicField(
        'q0',
        labelKey: 'topic.mixing.field.q0.label',
        initial: '10',
        numeric: true,
      ),
      TopicField(
        'rin',
        labelKey: 'topic.mixing.field.rin.label',
        initial: '5',
        numeric: true,
      ),
      TopicField(
        'rout',
        labelKey: 'topic.mixing.field.rout.label',
        initial: '5',
        numeric: true,
      ),
      TopicField(
        'cin',
        labelKey: 'topic.mixing.field.cin.label',
        initial: '0.2',
        numeric: true,
      ),
      TopicField(
        't',
        labelKey: 'topic.mixing.field.t.label',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: ['V₀ = 100, Q₀ = 10, r = 5, c = 0.2'],
  ),
  'spring_mass': TopicSpec(
    id: 'spring_mass',
    icon: Icons.architecture_rounded,
    fields: _oscillatorFields,
    examples: ['m = 1, c = 0.5, k = 4, F₀ = 0'],
  ),
  'damped_oscillation': TopicSpec(
    id: 'damped_oscillation',
    icon: Icons.vibration_rounded,
    fields: _oscillatorFields,
    examples: [
      'm = 1, c = 0.5, k = 4  →  underdamped',
      'm = 1, c = 4, k = 4  →  critically damped',
      'm = 1, c = 0, k = 4, F₀ = 1, ω = 1  →  forced',
    ],
    backendId: 'spring_mass',
  ),
  'circuit': TopicSpec(
    id: 'circuit',
    icon: Icons.electrical_services_rounded,
    fields: [
      TopicField(
        'm',
        labelKey: 'topic.circuit.field.m.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'c',
        labelKey: 'topic.circuit.field.c.label',
        initial: '0.5',
        numeric: true,
      ),
      TopicField(
        'k',
        labelKey: 'topic.circuit.field.k.label',
        initial: '4',
        numeric: true,
      ),
      TopicField(
        'f0',
        labelKey: 'topic.circuit.field.f0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'w',
        labelKey: 'topic.circuit.field.w.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.circuit.field.y0.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'v0',
        labelKey: 'topic.circuit.field.v0.label',
        initial: '0',
        numeric: true,
      ),
    ],
    examples: ['L = 1, R = 0.5, 1/C = 4, E₀ = 0'],
  ),
  'falling_body': TopicSpec(
    id: 'falling_body',
    icon: Icons.arrow_downward_rounded,
    fields: [
      TopicField(
        'm',
        labelKey: 'topic.falling_body.field.m.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'k',
        labelKey: 'topic.falling_body.field.k.label',
        initial: '0.5',
        numeric: true,
      ),
      TopicField(
        'g',
        labelKey: 'topic.falling_body.field.g.label',
        initial: '9.81',
        numeric: true,
      ),
      TopicField(
        'v0',
        labelKey: 'topic.falling_body.field.v0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        't',
        labelKey: 'topic.falling_body.field.t.label',
        initial: '5',
        numeric: true,
      ),
    ],
    examples: ['m = 1, k = 0.5, g = 9.81, v₀ = 0'],
  ),
  'euler': TopicSpec(
    id: 'euler',
    icon: Icons.straighten_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.euler.field.f.label',
        hintKey: 'topic.euler.field.f.hint',
        initial: 'x + y',
      ),
      TopicField(
        'x0',
        labelKey: 'topic.euler.field.x0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.euler.field.y0.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'h',
        labelKey: 'topic.euler.field.h.label',
        initial: '0.1',
        numeric: true,
      ),
      TopicField(
        'n',
        labelKey: 'topic.euler.field.n.label',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: ["y' = x + y, y(0) = 1, h = 0.1"],
  ),
  'heun': TopicSpec(
    id: 'heun',
    icon: Icons.linear_scale_rounded,
    fields: [
      TopicField('f', labelKey: 'topic.heun.field.f.label', initial: 'x + y'),
      TopicField(
        'x0',
        labelKey: 'topic.heun.field.x0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.heun.field.y0.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'h',
        labelKey: 'topic.heun.field.h.label',
        initial: '0.1',
        numeric: true,
      ),
      TopicField(
        'n',
        labelKey: 'topic.heun.field.n.label',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: ["y' = x + y, y(0) = 1, h = 0.1"],
  ),
  'rk4': TopicSpec(
    id: 'rk4',
    icon: Icons.stacked_line_chart_rounded,
    fields: [
      TopicField('f', labelKey: 'topic.rk4.field.f.label', initial: 'x + y'),
      TopicField(
        'x0',
        labelKey: 'topic.rk4.field.x0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.rk4.field.y0.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'h',
        labelKey: 'topic.rk4.field.h.label',
        initial: '0.1',
        numeric: true,
      ),
      TopicField(
        'n',
        labelKey: 'topic.rk4.field.n.label',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: ["y' = x + y, y(0) = 1, h = 0.1"],
  ),
  'multistep': TopicSpec(
    id: 'multistep',
    icon: Icons.format_list_numbered_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.multistep.field.f.label',
        hintKey: 'topic.multistep.field.f.hint',
        initial: 'x + y',
      ),
      TopicField(
        'x0',
        labelKey: 'topic.multistep.field.x0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.multistep.field.y0.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'h',
        labelKey: 'topic.multistep.field.h.label',
        initial: '0.1',
        numeric: true,
      ),
      TopicField(
        'n',
        labelKey: 'topic.multistep.field.n.label',
        initial: '10',
        numeric: true,
      ),
      TopicField(
        'order',
        labelKey: 'topic.multistep.field.order.label',
        initial: '4',
        numeric: true,
      ),
    ],
    examples: ["y' = x + y, y(0) = 1, h = 0.1, order 4"],
  ),
  'error_convergence': TopicSpec(
    id: 'error_convergence',
    icon: Icons.error_outline_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.error_convergence.field.f.label',
        hintKey: 'topic.error_convergence.field.f.hint',
        initial: 'x + y',
      ),
      TopicField(
        'x0',
        labelKey: 'topic.error_convergence.field.x0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.error_convergence.field.y0.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'h',
        labelKey: 'topic.error_convergence.field.h.label',
        initial: '0.1',
        numeric: true,
      ),
      TopicField(
        'n',
        labelKey: 'topic.error_convergence.field.n.label',
        initial: '10',
        numeric: true,
      ),
      TopicField(
        'method',
        labelKey: 'topic.error_convergence.field.method.label',
        hintKey: 'topic.error_convergence.field.method.hint',
        initial: '3',
        numeric: true,
      ),
      TopicField(
        'exact',
        labelKey: 'topic.error_convergence.field.exact.label',
        hintKey: 'topic.error_convergence.field.exact.hint',
      ),
    ],
    examples: ["y' = x + y, y(0) = 1, h = 0.1, RK4"],
  ),
  'linear_system': TopicSpec(
    id: 'linear_system',
    icon: Icons.hub_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.linear_system.field.a.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.linear_system.field.b.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'c',
        labelKey: 'topic.linear_system.field.c.label',
        initial: '-1',
        numeric: true,
      ),
      TopicField(
        'd',
        labelKey: 'topic.linear_system.field.d.label',
        initial: '0',
        numeric: true,
      ),
    ],
    examples: ["x' = y, y' = −x (centre)"],
  ),
  'matrix_eigen': TopicSpec(
    id: 'matrix_eigen',
    icon: Icons.grid_4x4_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.matrix_eigen.field.matrix.label',
        hintKey: 'topic.matrix_eigen.field.matrix.hint',
        initial: '4, 1; 2, 3',
      ),
    ],
    examples: [
      'A = [[4, 1], [2, 3]]  →  λ = 2, 5',
      'A = [[2, 1], [0, 2]]  →  λ = 2 (×2), det = 4',
      'A = [[0, −1], [1, 0]]  →  λ = ±i',
    ],
  ),
  'phase_portrait': TopicSpec(
    id: 'phase_portrait',
    icon: Icons.scatter_plot_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.phase_portrait.field.a.label',
        hintKey: 'topic.phase_portrait.field.a.hint',
        initial: '-1',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.phase_portrait.field.b.label',
        hintKey: 'topic.phase_portrait.field.b.hint',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'c',
        labelKey: 'topic.phase_portrait.field.c.label',
        hintKey: 'topic.phase_portrait.field.c.hint',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'd',
        labelKey: 'topic.phase_portrait.field.d.label',
        hintKey: 'topic.phase_portrait.field.d.hint',
        initial: '-2',
        numeric: true,
      ),
    ],
    examples: [
      "a = −1, b = 0, c = 0, d = −2  →  stable node",
      "a = 1, b = 0, c = 0, d = −1  →  saddle",
      "a = 0, b = 1, c = −1, d = 0  →  centre",
    ],
  ),
  'critical_points': TopicSpec(
    id: 'critical_points',
    icon: Icons.place_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.critical_points.field.f.label',
        hintKey: 'topic.critical_points.field.f.hint',
        initial: 'x - x^2 - x*y',
      ),
      TopicField(
        'g',
        labelKey: 'topic.critical_points.field.g.label',
        hintKey: 'topic.critical_points.field.g.hint',
        initial: 'y*x - 0.5*y',
      ),
      TopicField(
        'xmin',
        labelKey: 'topic.critical_points.field.xmin.label',
        initial: '-1',
        numeric: true,
      ),
      TopicField(
        'xmax',
        labelKey: 'topic.critical_points.field.xmax.label',
        initial: '2',
        numeric: true,
      ),
      TopicField(
        'ymin',
        labelKey: 'topic.critical_points.field.ymin.label',
        initial: '-1',
        numeric: true,
      ),
      TopicField(
        'ymax',
        labelKey: 'topic.critical_points.field.ymax.label',
        initial: '2',
        numeric: true,
      ),
    ],
    examples: [
      "x′ = x − x² − xy,  y′ = xy − 0.5y",
      'three critical points: (0, 0), (0.5, 0.5), (1, 0)',
      'the middle one is a stable spiral',
    ],
  ),
  'predator_prey': TopicSpec(
    id: 'predator_prey',
    icon: Icons.pets_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.predator_prey.field.a.label',
        initial: '1.1',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.predator_prey.field.b.label',
        initial: '0.4',
        numeric: true,
      ),
      TopicField(
        'c',
        labelKey: 'topic.predator_prey.field.c.label',
        initial: '0.4',
        numeric: true,
      ),
      TopicField(
        'd',
        labelKey: 'topic.predator_prey.field.d.label',
        initial: '0.1',
        numeric: true,
      ),
      TopicField(
        'x0',
        labelKey: 'topic.predator_prey.field.x0.label',
        initial: '10',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.predator_prey.field.y0.label',
        initial: '5',
        numeric: true,
      ),
      TopicField(
        't',
        labelKey: 'topic.predator_prey.field.t.label',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: [
      "x′ = 1.1x − 0.4xy,  y′ = −0.4y + 0.1xy",
      'coexistence equilibrium: (4, 2.75), a centre',
      'the conserved quantity keeps each orbit closed',
    ],
  ),
  'fourier_series': TopicSpec(
    id: 'fourier_series',
    icon: Icons.multiline_chart_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.fourier_series.field.f.label',
        initial: 'x',
      ),
      TopicField(
        'L',
        labelKey: 'topic.fourier_series.field.L.label',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField(
        'n',
        labelKey: 'topic.fourier_series.field.n.label',
        initial: '5',
        numeric: true,
      ),
    ],
    examples: ['f(x) = x on [−π, π]'],
  ),
  'pde_first_order': TopicSpec(
    id: 'pde_first_order',
    icon: Icons.looks_one_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.pde_first_order.field.a.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.pde_first_order.field.b.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'c',
        labelKey: 'topic.pde_first_order.field.c.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'f',
        labelKey: 'topic.pde_first_order.field.f.label',
        initial: 'sin(x)',
      ),
    ],
    examples: [
      'u_x + u_y = 0, u(x, 0) = sin(x)',
      '2 u_x − 3 u_y = 1, u(x, 0) = x²',
    ],
  ),
  'pde_characteristics': TopicSpec(
    id: 'pde_characteristics',
    icon: Icons.alt_route_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.pde_characteristics.field.a.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.pde_characteristics.field.b.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'p',
        labelKey: 'topic.pde_characteristics.field.p.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'q',
        labelKey: 'topic.pde_characteristics.field.q.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'f',
        labelKey: 'topic.pde_characteristics.field.f.label',
        initial: 'sin(x)',
      ),
    ],
    examples: [
      'u_x + u_y = u, u(x, 0) = sin(x)',
      '2 u_x + u_y = 3u + 1, u(x, 0) = x²',
    ],
  ),
  'pde_classify': TopicSpec(
    id: 'pde_classify',
    icon: Icons.category_rounded,
    fields: [
      TopicField(
        'A',
        labelKey: 'topic.pde_classify.field.A.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'B',
        labelKey: 'topic.pde_classify.field.B.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'C',
        labelKey: 'topic.pde_classify.field.C.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'D',
        labelKey: 'topic.pde_classify.field.D.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'E',
        labelKey: 'topic.pde_classify.field.E.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'F',
        labelKey: 'topic.pde_classify.field.F.label',
        initial: '0',
        numeric: true,
      ),
    ],
    examples: [
      'u_xx + u_yy = 0  →  elliptic',
      'u_xx + 2u_xy + u_yy = 0  →  parabolic',
      'u_xx + 4u_xy + u_yy = 0  →  hyperbolic',
    ],
  ),
  'pde_separation': TopicSpec(
    id: 'pde_separation',
    icon: Icons.call_split_rounded,
    fields: [
      TopicField(
        'A',
        labelKey: 'topic.pde_separation.field.A.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'C',
        labelKey: 'topic.pde_separation.field.C.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'L',
        labelKey: 'topic.pde_separation.field.L.label',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField(
        'n',
        labelKey: 'topic.pde_separation.field.n.label',
        initial: '1',
        numeric: true,
      ),
    ],
    examples: [
      'A u_xx + C u_yy = 0, u(0, y) = u(L, y) = 0',
      'A = 1, C = 1  →  sinh and cosh in y',
      'A = −1, C = 1  →  sine and cosine in y',
    ],
  ),
  'heat': TopicSpec(
    id: 'heat',
    icon: Icons.local_fire_department_rounded,
    fields: [
      TopicField(
        'alpha',
        labelKey: 'topic.heat.field.alpha.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'L',
        labelKey: 'topic.heat.field.L.label',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField(
        'n',
        labelKey: 'topic.heat.field.n.label',
        initial: '3',
        numeric: true,
      ),
      TopicField(
        'f',
        labelKey: 'topic.heat.field.f.label',
        initial: 'sin(pi*x/L)',
      ),
    ],
    examples: ['u_t = u_xx, u(0,t) = u(L,t) = 0'],
  ),
  'wave': TopicSpec(
    id: 'wave',
    icon: Icons.waves_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.wave.field.a.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'L',
        labelKey: 'topic.wave.field.L.label',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField(
        'n',
        labelKey: 'topic.wave.field.n.label',
        initial: '3',
        numeric: true,
      ),
      TopicField(
        'f',
        labelKey: 'topic.wave.field.f.label',
        initial: 'sin(pi*x/L)',
      ),
    ],
    examples: ['u_tt = a² u_xx, fixed ends'],
  ),
  'laplace_pde': TopicSpec(
    id: 'laplace_pde',
    icon: Icons.blur_circular_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.laplace_pde.field.a.label',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.laplace_pde.field.b.label',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField(
        'n',
        labelKey: 'topic.laplace_pde.field.n.label',
        initial: '3',
        numeric: true,
      ),
      TopicField(
        'f',
        labelKey: 'topic.laplace_pde.field.f.label',
        initial: 'sin(pi*x/a)',
      ),
    ],
    examples: ['u_xx + u_yy = 0 on a rectangle'],
  ),
  'laplace': TopicSpec(
    id: 'laplace',
    icon: Icons.transform_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.laplace.field.f.label',
        initial: 'exp(2*t)*sin(3*t)',
      ),
    ],
    examples: ['f(t) = e^{2t} sin(3t)', 'f(t) = t² e^{2t}'],
  ),
  'inverse_laplace': TopicSpec(
    id: 'inverse_laplace',
    icon: Icons.u_turn_left_rounded,
    fields: [
      TopicField(
        'F',
        labelKey: 'topic.inverse_laplace.field.F.label',
        initial: '1/(s^2+4)',
      ),
    ],
    examples: ['F(s) = s/(s² + 4)', 'F(s) = 1/(s − 2)³'],
  ),
  'convolution': TopicSpec(
    id: 'convolution',
    icon: Icons.join_full_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.convolution.field.f.label',
        initial: 't',
      ),
      TopicField(
        'g',
        labelKey: 'topic.convolution.field.g.label',
        initial: 'sin(t)',
      ),
      TopicField(
        't',
        labelKey: 'topic.convolution.field.t.label',
        initial: '1',
        numeric: true,
      ),
    ],
    examples: ['f(t) = t, g(t) = sin(t), t = 1', 'f(t) = e^t, g(t) = cos(t)'],
  ),
  'transform_derivatives': TopicSpec(
    id: 'transform_derivatives',
    icon: Icons.linear_scale_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.transform_derivatives.field.f.label',
        initial: 't',
      ),
      TopicField(
        'y0',
        labelKey: 'topic.transform_derivatives.field.y0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'v0',
        labelKey: 'topic.transform_derivatives.field.v0.label',
        initial: '0',
        numeric: true,
      ),
    ],
    examples: [
      "f(t) = t, y(0) = 0, y'(0) = 0",
      "f(t) = sin(t), y(0) = 0, y'(0) = 1",
    ],
  ),
  'laplace_ivp': TopicSpec(
    id: 'laplace_ivp',
    icon: Icons.rule_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.laplace_ivp.field.a.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.laplace_ivp.field.b.label',
        initial: '3',
        numeric: true,
      ),
      TopicField(
        'c',
        labelKey: 'topic.laplace_ivp.field.c.label',
        initial: '2',
        numeric: true,
      ),
      TopicField(
        'g',
        labelKey: 'topic.laplace_ivp.field.g.label',
        initial: 'exp(t)',
      ),
      TopicField(
        'y0',
        labelKey: 'topic.laplace_ivp.field.y0.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'v0',
        labelKey: 'topic.laplace_ivp.field.v0.label',
        initial: '0',
        numeric: true,
      ),
    ],
    examples: [
      "y'' + 3y' + 2y = e^t, y(0) = 1, y'(0) = 0",
      "y'' + y = sin(t), y(0) = 0, y'(0) = 0",
    ],
  ),
  'step_functions': TopicSpec(
    id: 'step_functions',
    icon: Icons.stairs_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.step_functions.field.f.label',
        initial: 't',
      ),
      TopicField(
        'shift',
        labelKey: 'topic.step_functions.field.shift.label',
        initial: '1',
        numeric: true,
      ),
    ],
    examples: ['f(t) = t, a = 1', 'f(t) = e^{2t}, a = 2'],
  ),
  'discontinuous_forcing': TopicSpec(
    id: 'discontinuous_forcing',
    icon: Icons.bolt_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.discontinuous_forcing.field.a.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.discontinuous_forcing.field.b.label',
        initial: '2',
        numeric: true,
      ),
      TopicField(
        'c',
        labelKey: 'topic.discontinuous_forcing.field.c.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'f',
        labelKey: 'topic.discontinuous_forcing.field.f.label',
        initial: '1',
      ),
      TopicField(
        'shift',
        labelKey: 'topic.discontinuous_forcing.field.shift.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.discontinuous_forcing.field.y0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'v0',
        labelKey: 'topic.discontinuous_forcing.field.v0.label',
        initial: '0',
        numeric: true,
      ),
    ],
    examples: [
      "y'' + 2y' + y = 1 − u(t − 1)",
      "y'' + 3y' + 2y = t − t u(t − 2)",
    ],
  ),
  'impulse': TopicSpec(
    id: 'impulse',
    icon: Icons.flash_on_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.impulse.field.a.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.impulse.field.b.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'c',
        labelKey: 'topic.impulse.field.c.label',
        initial: '4',
        numeric: true,
      ),
      TopicField(
        'k',
        labelKey: 'topic.impulse.field.k.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'shift',
        labelKey: 'topic.impulse.field.shift.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.impulse.field.y0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'v0',
        labelKey: 'topic.impulse.field.v0.label',
        initial: '0',
        numeric: true,
      ),
    ],
    examples: ["y'' + 4y = δ(t − 1)", "y'' + 2y' + y = 2δ(t − 1)"],
  ),
  'fourier_transform': TopicSpec(
    id: 'fourier_transform',
    icon: Icons.ssid_chart_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.fourier_transform.field.f.label',
        initial: 'exp(-abs(x))',
      ),
      TopicField(
        'w',
        labelKey: 'topic.fourier_transform.field.w.label',
        initial: '1',
        numeric: true,
      ),
    ],
    examples: ['f(x) = e^{−|x|} at ω = 1', 'f(x) = e^{−x²} at ω = 0'],
  ),
  'ode_linear': TopicSpec(
    id: 'ode_linear',
    icon: Icons.functions_rounded,
    fields: [
      TopicField(
        'coeffs',
        labelKey: 'topic.ode_linear.field.coeffs.label',
        hintKey: 'topic.ode_linear.field.coeffs.hint',
        initial: '1, 0, 1',
      ),
      TopicField(
        'ics',
        labelKey: 'topic.ode_linear.field.ics.label',
        hintKey: 'topic.ode_linear.field.ics.hint',
        initial: '1, 0',
      ),
    ],
    examples: ["y'' + y = 0, y(0)=1, y'(0)=0", "y'' + 3y' + 2y = 0"],
  ),
  'ode_stiff': TopicSpec(
    id: 'ode_stiff',
    icon: Icons.speed_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.ode_stiff.field.f.label',
        initial: '-50*(y - cos(x))',
      ),
      TopicField(
        'x0',
        labelKey: 'topic.ode_stiff.field.x0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.ode_stiff.field.y0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'xend',
        labelKey: 'topic.ode_stiff.field.xend.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'h',
        labelKey: 'topic.ode_stiff.field.h.label',
        initial: '0.01',
        numeric: true,
      ),
    ],
    examples: ["y' = −50(y − cos x), y(0) = 0"],
  ),
  'series_power': TopicSpec(
    id: 'series_power',
    icon: Icons.auto_graph_rounded,
    fields: [
      TopicField(
        'p',
        labelKey: 'topic.series_power.field.p.label',
        initial: '0',
      ),
      TopicField(
        'q',
        labelKey: 'topic.series_power.field.q.label',
        initial: '1',
      ),
      TopicField(
        'x0',
        labelKey: 'topic.series_power.field.x0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'terms',
        labelKey: 'topic.series_power.field.terms.label',
        initial: '8',
        numeric: true,
      ),
    ],
    examples: [
      'y\'\' − y = 0 about x₀ = 0',
      'y\'\' + x y\' + y = 0 about x₀ = 0',
    ],
  ),
  'series_singular_points': TopicSpec(
    id: 'series_singular_points',
    icon: Icons.adjust_rounded,
    fields: [
      TopicField(
        'p',
        labelKey: 'topic.series_singular_points.field.p.label',
        initial: '-1/(2*x)',
      ),
      TopicField(
        'q',
        labelKey: 'topic.series_singular_points.field.q.label',
        initial: '(1+x)/(2*x^2)',
      ),
      TopicField(
        'x0',
        labelKey: 'topic.series_singular_points.field.x0.label',
        initial: '0',
        numeric: true,
      ),
    ],
    examples: [
      'p = −1/(2x), q = (1 + x)/(2x²) at x₀ = 0',
      'p = 0, q = 1 at x₀ = 0  →  ordinary',
    ],
  ),
  'series_frobenius': TopicSpec(
    id: 'series_frobenius',
    icon: Icons.account_tree_rounded,
    fields: [
      TopicField(
        'p',
        labelKey: 'topic.series_frobenius.field.p.label',
        initial: '-1/(2*x)',
      ),
      TopicField(
        'q',
        labelKey: 'topic.series_frobenius.field.q.label',
        initial: '(1+x)/(2*x^2)',
      ),
      TopicField(
        'x0',
        labelKey: 'topic.series_frobenius.field.x0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'terms',
        labelKey: 'topic.series_frobenius.field.terms.label',
        initial: '8',
        numeric: true,
      ),
    ],
    examples: [
      '2x² y″ − x y′ + (1 + x) y = 0 at x₀ = 0',
      'indicial roots r = 1 and r = 1/2',
    ],
  ),
  'euler_equations': TopicSpec(
    id: 'euler_equations',
    icon: Icons.functions_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.euler_equations.field.a.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.euler_equations.field.b.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'c',
        labelKey: 'topic.euler_equations.field.c.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'd',
        labelKey: 'topic.euler_equations.field.d.label',
        initial: '0',
        numeric: true,
      ),
    ],
    examples: ['x² y″ + x y′ + y = 0', 'x² y″ − 2 y = 1'],
  ),
  'special_legendre': TopicSpec(
    id: 'special_legendre',
    icon: Icons.legend_toggle_rounded,
    fields: [
      TopicField(
        'n',
        labelKey: 'topic.special_legendre.field.n.label',
        initial: '2',
        numeric: true,
      ),
      TopicField(
        'x',
        labelKey: 'topic.special_legendre.field.x.label',
        initial: '0.5',
        numeric: true,
      ),
      TopicField(
        'q',
        labelKey: 'topic.special_legendre.field.q.label',
        initial: '0',
        numeric: true,
      ),
    ],
    examples: ['P₂(0.5) = −0.125', 'n = 3, x = 0.5  →  P₃(0.5)'],
  ),
  'special_bessel': TopicSpec(
    id: 'special_bessel',
    icon: Icons.cyclone_rounded,
    fields: [
      TopicField(
        'nu',
        labelKey: 'topic.special_bessel.field.nu.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'x',
        labelKey: 'topic.special_bessel.field.x.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'terms',
        labelKey: 'topic.special_bessel.field.terms.label',
        initial: '30',
        numeric: true,
      ),
    ],
    examples: ['J₀(1) ≈ 0.7652', 'ν = 1, x = 2'],
  ),
  'system_theory': TopicSpec(
    id: 'system_theory',
    icon: Icons.hub_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.system_theory.field.matrix.label',
        initial: '1, 1; 4, 1',
      ),
    ],
    examples: [
      'A = [[1, 1], [4, 1]]  →  λ = 3, −1',
      'A = [[0, 1], [−1, 0]]  →  λ = ±i',
    ],
  ),
  'system_complex_eigen': TopicSpec(
    id: 'system_complex_eigen',
    icon: Icons.grain_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.system_complex_eigen.field.matrix.label',
        initial: '1, -1; 1, 1',
      ),
    ],
    examples: [
      'A = [[1, −1], [1, 1]]  →  λ = 1 ± i',
      'A = [[0, −4], [1, 0]]  →  pure rotation',
    ],
  ),
  'system_fundamental': TopicSpec(
    id: 'system_fundamental',
    icon: Icons.grid_on_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.system_fundamental.field.matrix.label',
        initial: '0, 1; -2, -3',
      ),
    ],
    examples: ['A = [[0, 1], [−2, −3]]  →  λ = −1, −2', 'A = [[1, 0], [0, 2]]'],
  ),
  'system_repeated': TopicSpec(
    id: 'system_repeated',
    icon: Icons.repeat_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.system_repeated.field.matrix.label',
        initial: '3, 1; 0, 3',
      ),
    ],
    examples: [
      'A = [[3, 1], [0, 3]]  →  λ = 3 (defective)',
      'A = [[2, 0], [0, 2]]  →  diagonalizable',
    ],
  ),
  'system_nonhomogeneous': TopicSpec(
    id: 'system_nonhomogeneous',
    icon: Icons.add_circle_outline_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.system_nonhomogeneous.field.matrix.label',
        initial: '1, 0; 0, 2',
      ),
      TopicField(
        'f',
        labelKey: 'topic.system_nonhomogeneous.field.f.label',
        initial: '1, 0',
      ),
    ],
    examples: [
      'A = [[1, 0], [0, 2]], f = [1, 0]',
      'A = [[0, 1], [−1, 0]], f = [0, eᵗ]',
    ],
  ),
  'system_rk4': TopicSpec(
    id: 'system_rk4',
    icon: Icons.stacked_line_chart_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.system_rk4.field.f.label',
        initial: 'x - 0.5*x*y',
      ),
      TopicField(
        'g',
        labelKey: 'topic.system_rk4.field.g.label',
        initial: '-y + 0.1*x*y',
      ),
      TopicField(
        't0',
        labelKey: 'topic.system_rk4.field.t0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'T',
        labelKey: 'topic.system_rk4.field.T.label',
        initial: '5',
        numeric: true,
      ),
      TopicField(
        'h',
        labelKey: 'topic.system_rk4.field.h.label',
        initial: '0.05',
        numeric: true,
      ),
      TopicField(
        'x0',
        labelKey: 'topic.system_rk4.field.x0.label',
        initial: '2',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.system_rk4.field.y0.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'method',
        labelKey: 'topic.system_rk4.field.method.label',
        initial: 'rk4',
      ),
    ],
    examples: [
      'x′ = x − 0.5xy, y′ = −y + 0.1xy',
      'x₀ = 2, y₀ = 1, T = 5, h = 0.05',
    ],
  ),
  'competing_species': TopicSpec(
    id: 'competing_species',
    icon: Icons.compare_arrows_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.competing_species.field.a.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.competing_species.field.b.label',
        initial: '0.3',
        numeric: true,
      ),
      TopicField(
        'c',
        labelKey: 'topic.competing_species.field.c.label',
        initial: '0.2',
        numeric: true,
      ),
      TopicField(
        'd',
        labelKey: 'topic.competing_species.field.d.label',
        initial: '0.8',
        numeric: true,
      ),
      TopicField(
        'e',
        labelKey: 'topic.competing_species.field.e.label',
        initial: '0.4',
        numeric: true,
      ),
      TopicField(
        'f',
        labelKey: 'topic.competing_species.field.f.label',
        initial: '0.3',
        numeric: true,
      ),
    ],
    examples: [
      'x′ = x(1 − 0.3x − 0.2y), y′ = y(0.8 − 0.4x − 0.3y)',
      'one species is eventually excluded',
    ],
  ),
  'liapunov': TopicSpec(
    id: 'liapunov',
    icon: Icons.trending_down_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.liapunov.field.f.label',
        initial: '-x + 2*x^2*y',
      ),
      TopicField('g', labelKey: 'topic.liapunov.field.g.label', initial: '-y'),
      TopicField(
        'x0',
        labelKey: 'topic.liapunov.field.x0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.liapunov.field.y0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField('V', labelKey: 'topic.liapunov.field.V.label'),
    ],
    examples: ['x′ = −x + 2x²y, y′ = −y', 'V = x² + y²  →  V′ < 0 locally'],
  ),
  'limit_cycle': TopicSpec(
    id: 'limit_cycle',
    icon: Icons.all_inclusive_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.limit_cycle.field.f.label',
        initial: 'y',
      ),
      TopicField(
        'g',
        labelKey: 'topic.limit_cycle.field.g.label',
        initial: '1*(1 - x^2)*y - x',
      ),
      TopicField(
        'x0',
        labelKey: 'topic.limit_cycle.field.x0.label',
        initial: '2',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.limit_cycle.field.y0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'tmax',
        labelKey: 'topic.limit_cycle.field.tmax.label',
        initial: '40',
        numeric: true,
      ),
      TopicField(
        'h',
        labelKey: 'topic.limit_cycle.field.h.label',
        initial: '0.005',
        numeric: true,
      ),
    ],
    examples: [
      'Van der Pol: x′ = y, y′ = (1 − x²)y − x',
      'the orbit settles on a stable limit cycle',
    ],
  ),
  'chaos_attractor': TopicSpec(
    id: 'chaos_attractor',
    icon: Icons.scatter_plot_rounded,
    fields: [
      TopicField(
        'system',
        labelKey: 'topic.chaos_attractor.field.system.label',
        initial: 'lorenz',
      ),
      TopicField(
        'x0',
        labelKey: 'topic.chaos_attractor.field.x0.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'y0',
        labelKey: 'topic.chaos_attractor.field.y0.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'z0',
        labelKey: 'topic.chaos_attractor.field.z0.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'tmax',
        labelKey: 'topic.chaos_attractor.field.tmax.label',
        initial: '40',
        numeric: true,
      ),
      TopicField(
        'h',
        labelKey: 'topic.chaos_attractor.field.h.label',
        initial: '0.005',
        numeric: true,
      ),
    ],
    examples: [
      'Lorenz: x′ = 10(y − x), y′ = x(28 − z) − y, z′ = xy − 8z/3',
      'a positive exponent is numerical evidence of sensitive dependence',
    ],
  ),
  'local_linearization': TopicSpec(
    id: 'local_linearization',
    icon: Icons.linear_scale_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.local_linearization.field.f.label',
        initial: 'x - x^2 - x*y',
      ),
      TopicField(
        'g',
        labelKey: 'topic.local_linearization.field.g.label',
        initial: 'y*x - 0.5*y',
      ),
      TopicField(
        'xmin',
        labelKey: 'topic.local_linearization.field.xmin.label',
        initial: '-1',
        numeric: true,
      ),
      TopicField(
        'xmax',
        labelKey: 'topic.local_linearization.field.xmax.label',
        initial: '2',
        numeric: true,
      ),
      TopicField(
        'ymin',
        labelKey: 'topic.local_linearization.field.ymin.label',
        initial: '-1',
        numeric: true,
      ),
      TopicField(
        'ymax',
        labelKey: 'topic.local_linearization.field.ymax.label',
        initial: '2',
        numeric: true,
      ),
    ],
    examples: [
      'x′ = x − x² − xy, y′ = xy − 0.5y',
      'compare each Jacobian with the full nonlinear field',
    ],
  ),
  'bvp_two_point': TopicSpec(
    id: 'bvp_two_point',
    icon: Icons.straighten_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.bvp_two_point.field.a.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.bvp_two_point.field.b.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'alpha',
        labelKey: 'topic.bvp_two_point.field.alpha.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'beta',
        labelKey: 'topic.bvp_two_point.field.beta.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'N',
        labelKey: 'topic.bvp_two_point.field.N.label',
        initial: '40',
        numeric: true,
      ),
      TopicField(
        'p',
        labelKey: 'topic.bvp_two_point.field.p.label',
        initial: '0',
      ),
      TopicField(
        'q',
        labelKey: 'topic.bvp_two_point.field.q.label',
        initial: '0',
      ),
      TopicField(
        'f',
        labelKey: 'topic.bvp_two_point.field.f.label',
        initial: '1',
      ),
    ],
    examples: ['−y″ = 1, y(0) = y(1) = 0', '−y″ + y = x, y(0) = 0, y(1) = 1'],
  ),
  'bvp_occurrence': TopicSpec(
    id: 'bvp_occurrence',
    icon: Icons.category_rounded,
    fields: [
      TopicField(
        'case',
        labelKey: 'topic.bvp_occurrence.field.case.label',
        initial: 'heat_rod',
      ),
    ],
    examples: [
      'heat_rod  →  steady heat in a rod',
      'string  →  steady hanging string',
      'beam  →  fourth-order Euler–Bernoulli beam',
    ],
  ),
  'sturm_liouville': TopicSpec(
    id: 'sturm_liouville',
    icon: Icons.tune_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.sturm_liouville.field.a.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.sturm_liouville.field.b.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'N',
        labelKey: 'topic.sturm_liouville.field.N.label',
        initial: '60',
        numeric: true,
      ),
      TopicField(
        'K',
        labelKey: 'topic.sturm_liouville.field.K.label',
        initial: '3',
        numeric: true,
      ),
      TopicField(
        'p',
        labelKey: 'topic.sturm_liouville.field.p.label',
        initial: '1',
      ),
      TopicField(
        'q',
        labelKey: 'topic.sturm_liouville.field.q.label',
        initial: '0',
      ),
      TopicField(
        'w',
        labelKey: 'topic.sturm_liouville.field.w.label',
        initial: '1',
      ),
    ],
    examples: ['−y″ = λ y, y(0) = y(1) = 0', '−y″ + x y = λ y on [0, 1]'],
  ),
  'bvp_nonhomogeneous': TopicSpec(
    id: 'bvp_nonhomogeneous',
    icon: Icons.add_chart_rounded,
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.bvp_nonhomogeneous.field.a.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.bvp_nonhomogeneous.field.b.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'alpha',
        labelKey: 'topic.bvp_nonhomogeneous.field.alpha.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'beta',
        labelKey: 'topic.bvp_nonhomogeneous.field.beta.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'N',
        labelKey: 'topic.bvp_nonhomogeneous.field.N.label',
        initial: '60',
        numeric: true,
      ),
      TopicField(
        'p',
        labelKey: 'topic.bvp_nonhomogeneous.field.p.label',
        initial: '1',
      ),
      TopicField(
        'q',
        labelKey: 'topic.bvp_nonhomogeneous.field.q.label',
        initial: '0',
      ),
      TopicField(
        'f',
        labelKey: 'topic.bvp_nonhomogeneous.field.f.label',
        initial: '1',
      ),
    ],
    examples: [
      '−y″ + y = 1, y(0) = y(1) = 0',
      '−y″ = sin(πx), y(0) = y(1) = 0',
    ],
  ),
  'sturm_liouville_singular': TopicSpec(
    id: 'sturm_liouville_singular',
    icon: Icons.adjust_rounded,
    fields: [
      TopicField(
        'family',
        labelKey: 'topic.sturm_liouville_singular.field.family.label',
        initial: 'bessel',
      ),
      TopicField(
        'K',
        labelKey: 'topic.sturm_liouville_singular.field.K.label',
        initial: '4',
        numeric: true,
      ),
      TopicField(
        'nu',
        labelKey: 'topic.sturm_liouville_singular.field.nu.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'L',
        labelKey: 'topic.sturm_liouville_singular.field.L.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'a',
        labelKey: 'topic.sturm_liouville_singular.field.a.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.sturm_liouville_singular.field.b.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'N',
        labelKey: 'topic.sturm_liouville_singular.field.N.label',
        initial: '80',
        numeric: true,
      ),
      TopicField(
        'p',
        labelKey: 'topic.sturm_liouville_singular.field.p.label',
        initial: 'x*(1 - x)',
      ),
      TopicField(
        'q',
        labelKey: 'topic.sturm_liouville_singular.field.q.label',
        initial: '0',
      ),
      TopicField(
        'w',
        labelKey: 'topic.sturm_liouville_singular.field.w.label',
        initial: '1',
      ),
    ],
    examples: [
      'bessel, ν = 0, L = 1  →  J₀ zeros',
      'legendre  →  λ = n(n + 1)',
      'laguerre  →  λ = n',
    ],
  ),
  'bessel_series': TopicSpec(
    id: 'bessel_series',
    icon: Icons.functions_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.bessel_series.field.f.label',
        initial: '1 - x^2',
      ),
      TopicField(
        'nu',
        labelKey: 'topic.bessel_series.field.nu.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'L',
        labelKey: 'topic.bessel_series.field.L.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'x0',
        labelKey: 'topic.bessel_series.field.x0.label',
        initial: '0.5',
        numeric: true,
      ),
      TopicField(
        'N',
        labelKey: 'topic.bessel_series.field.N.label',
        initial: '12',
        numeric: true,
      ),
    ],
    examples: [
      'f(x) = 1 − x², ν = 0, L = 1',
      'the partial sum vanishes at x = L',
    ],
  ),
  'orthogonal_series': TopicSpec(
    id: 'orthogonal_series',
    icon: Icons.multiline_chart_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.orthogonal_series.field.f.label',
        initial: 'x',
      ),
      TopicField(
        'family',
        labelKey: 'topic.orthogonal_series.field.family.label',
        initial: 'legendre',
      ),
      TopicField(
        'a',
        labelKey: 'topic.orthogonal_series.field.a.label',
        initial: '-1',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.orthogonal_series.field.b.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'x0',
        labelKey: 'topic.orthogonal_series.field.x0.label',
        initial: '0.5',
        numeric: true,
      ),
      TopicField(
        'N',
        labelKey: 'topic.orthogonal_series.field.N.label',
        initial: '8',
        numeric: true,
      ),
      TopicField(
        'nu',
        labelKey: 'topic.orthogonal_series.field.nu.label',
        initial: '0',
        numeric: true,
      ),
    ],
    examples: [
      'f(x) = x on [−1, 1]  →  Legendre P₁',
      'chebyshev, sine and cosine families are available',
    ],
  ),
  'fourier_convergence': TopicSpec(
    id: 'fourier_convergence',
    icon: Icons.rule_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.fourier_convergence.field.f.label',
        initial: 'x',
      ),
      TopicField(
        'L',
        labelKey: 'topic.fourier_convergence.field.L.label',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField(
        'x0',
        labelKey: 'topic.fourier_convergence.field.x0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'N',
        labelKey: 'topic.fourier_convergence.field.N.label',
        initial: '20',
        numeric: true,
      ),
    ],
    examples: [
      'f(x) = x on [−π, π]',
      'the jump at the endpoints produces a Gibbs overshoot',
    ],
  ),
  'fourier_even_odd': TopicSpec(
    id: 'fourier_even_odd',
    icon: Icons.balance_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.fourier_even_odd.field.f.label',
        initial: 'x^2',
      ),
      TopicField(
        'L',
        labelKey: 'topic.fourier_even_odd.field.L.label',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField(
        'x0',
        labelKey: 'topic.fourier_even_odd.field.x0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'N',
        labelKey: 'topic.fourier_even_odd.field.N.label',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: [
      'f(x) = x²  →  cosine series only',
      'f(x) = x  →  sine series only',
    ],
  ),
  'heat_derivation': TopicSpec(
    id: 'heat_derivation',
    icon: Icons.local_fire_department_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.heat_derivation.field.f.label',
        initial: 'sin(pi*x/L)',
      ),
      TopicField(
        'L',
        labelKey: 'topic.heat_derivation.field.L.label',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField(
        'alpha',
        labelKey: 'topic.heat_derivation.field.alpha.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'x0',
        labelKey: 'topic.heat_derivation.field.x0.label',
        initial: '0.5',
        numeric: true,
      ),
      TopicField(
        'N',
        labelKey: 'topic.heat_derivation.field.N.label',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: ['u_t = α u_xx, u(0, t) = u(L, t) = 0', 'u(x, 0) = sin(πx/L)'],
  ),
  'wave_derivation': TopicSpec(
    id: 'wave_derivation',
    icon: Icons.waves_rounded,
    fields: [
      TopicField(
        'f',
        labelKey: 'topic.wave_derivation.field.f.label',
        initial: 'sin(pi*x/L)',
      ),
      TopicField(
        'g',
        labelKey: 'topic.wave_derivation.field.g.label',
        initial: '0',
      ),
      TopicField(
        'L',
        labelKey: 'topic.wave_derivation.field.L.label',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField(
        'c',
        labelKey: 'topic.wave_derivation.field.c.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'domain',
        labelKey: 'topic.wave_derivation.field.domain.label',
        initial: 'finite',
      ),
      TopicField(
        'x0',
        labelKey: 'topic.wave_derivation.field.x0.label',
        initial: '0.5',
        numeric: true,
      ),
      TopicField(
        'N',
        labelKey: 'topic.wave_derivation.field.N.label',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: [
      'u_tt = c² u_xx, fixed ends',
      'd\'Alembert solution on the infinite line',
    ],
  ),
  'heat_other_bc': TopicSpec(
    id: 'heat_other_bc',
    icon: Icons.thermostat_rounded,
    fields: [
      TopicField(
        'bc',
        labelKey: 'topic.heat_other_bc.field.bc.label',
        initial: 'insulated',
      ),
      TopicField(
        'f',
        labelKey: 'topic.heat_other_bc.field.f.label',
        initial: 'cos(pi*x/L)',
      ),
      TopicField(
        'L',
        labelKey: 'topic.heat_other_bc.field.L.label',
        initial: '3.141592653589793',
        numeric: true,
      ),
      TopicField(
        'alpha',
        labelKey: 'topic.heat_other_bc.field.alpha.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'h',
        labelKey: 'topic.heat_other_bc.field.h.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'u0',
        labelKey: 'topic.heat_other_bc.field.u0.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'u1',
        labelKey: 'topic.heat_other_bc.field.u1.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'x0',
        labelKey: 'topic.heat_other_bc.field.x0.label',
        initial: '0.5',
        numeric: true,
      ),
      TopicField(
        'N',
        labelKey: 'topic.heat_other_bc.field.N.label',
        initial: '10',
        numeric: true,
      ),
    ],
    examples: [
      'insulated ends  →  cosine modes',
      'robin ends  →  roots of μ tan(μL) = h',
    ],
  ),
  'transcendental': TopicSpec(
    id: 'transcendental',
    icon: Icons.show_chart_rounded,
    fields: [
      TopicField(
        'var',
        labelKey: 'topic.transcendental.field.var.label',
        initial: 'x',
      ),
      TopicField(
        'f',
        labelKey: 'topic.transcendental.field.f.label',
        initial: 'exp(x)*sin(x)',
      ),
      TopicField(
        'a',
        labelKey: 'topic.transcendental.field.a.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'x0',
        labelKey: 'topic.transcendental.field.x0.label',
        initial: '0.5',
        numeric: true,
      ),
      TopicField(
        'N',
        labelKey: 'topic.transcendental.field.N.label',
        initial: '5',
        numeric: true,
      ),
    ],
    examples: [
      'f(x) = eˣ sin x at a = 0',
      'compare the polynomial with the function at x₀',
    ],
  ),
  'integration_techniques': TopicSpec(
    id: 'integration_techniques',
    icon: Icons.calculate_rounded,
    fields: [
      TopicField(
        'var',
        labelKey: 'topic.integration_techniques.field.var.label',
        initial: 'x',
      ),
      TopicField(
        'technique',
        labelKey: 'topic.integration_techniques.field.technique.label',
        initial: 'parts',
      ),
      TopicField(
        'f',
        labelKey: 'topic.integration_techniques.field.f.label',
        initial: 'x*exp(x)',
      ),
      TopicField(
        'a',
        labelKey: 'topic.integration_techniques.field.a.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.integration_techniques.field.b.label',
        initial: '1',
        numeric: true,
      ),
    ],
    examples: [
      '∫ x eˣ dx  →  integration by parts',
      '∫ 1/(x² − 4) dx  →  partial fractions',
    ],
  ),
};
