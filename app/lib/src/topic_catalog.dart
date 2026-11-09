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
};
