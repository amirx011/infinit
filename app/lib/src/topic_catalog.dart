import 'package:flutter/material.dart';

/// One editable input on a topic workbench.
/// One option of a small method selector (for example Gauss vs Cramer).
class TopicChoice {
  const TopicChoice(this.value, this.labelKey);

  final String value;
  final String labelKey;
}

class TopicField {
  const TopicField(
    this.key, {
    required this.labelKey,
    this.hintKey = '',
    this.initial = '',
    this.numeric = false,
    this.choices = const [],
  });

  final String key;
  final String labelKey;
  final String hintKey;
  final String initial;
  final bool numeric;

  /// When non-empty the field renders as a segmented selector instead of a
  /// free-text box.
  final List<TopicChoice> choices;
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
  'linear_solve': TopicSpec(
    id: 'linear_solve',
    icon: Icons.calculate_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linear_solve.field.matrix.label',
        hintKey: 'topic.linear_solve.field.matrix.hint',
        initial: '2, 1; 1, -1',
      ),
      TopicField(
        'rhs',
        labelKey: 'topic.linear_solve.field.rhs.label',
        hintKey: 'topic.linear_solve.field.rhs.hint',
        initial: '5; 1',
      ),
      TopicField(
        'method',
        labelKey: 'topic.linear_solve.field.method.label',
        initial: 'gauss',
        choices: [
          TopicChoice('gauss', 'topic.linear_solve.method.gauss'),
          TopicChoice('cramer', 'topic.linear_solve.method.cramer'),
        ],
      ),
    ],
    examples: [
      'A = [[2, 1], [1, −1]], b = [5, 1]  →  Gauss',
      'A = [[1, 2], [3, 4]], b = [5, 6]  →  Cramer',
      'A = [[1, 1], [1, 1]], b = [1, 2]  →  no solution',
    ],
  ),
  'linalg_rank': TopicSpec(
    id: 'linalg_rank',
    icon: Icons.table_rows_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_rank.field.matrix.label',
        hintKey: 'topic.linalg_rank.field.matrix.hint',
        initial: '1, 2, 3; 2, 4, 6; 1, 1, 1',
      ),
    ],
    examples: [
      'A = [[1, 2, 3], [2, 4, 6], [1, 1, 1]]  →  rank 2',
      'A = [[1, 2], [2, 4]]  →  rank 1, nullity 1',
      'A = [[a, b], [2a, 2b]]  →  symbolic rank 1',
    ],
  ),
  'linalg_det': TopicSpec(
    id: 'linalg_det',
    icon: Icons.calculate_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_det.field.matrix.label',
        hintKey: 'topic.linalg_det.field.matrix.hint',
        initial: '1, 2; 3, 4',
      ),
    ],
    examples: [
      'A = [[1, 2], [3, 4]]  →  det = −2',
      'A = [[a, b], [c, d]]  →  det = ad − bc',
      'A = [[1, 2], [2, 4]]  →  det = 0',
    ],
  ),
  'linalg_inverse': TopicSpec(
    id: 'linalg_inverse',
    icon: Icons.flip_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_inverse.field.matrix.label',
        hintKey: 'topic.linalg_inverse.field.matrix.hint',
        initial: '2, 1; 1, 1',
      ),
    ],
    examples: [
      'A = [[2, 1], [1, 1]]  →  A⁻¹ = [[1, −1], [−1, 2]]',
      'A = [[a, b], [c, d]]  →  adj(A) / det(A)',
      'A = [[1, 1], [1, 1]]  →  no inverse',
    ],
  ),
  'linalg_lu': TopicSpec(
    id: 'linalg_lu',
    icon: Icons.call_split_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_lu.field.matrix.label',
        hintKey: 'topic.linalg_lu.field.matrix.hint',
        initial: '4, 3; 6, 3',
      ),
    ],
    examples: [
      'A = [[4, 3], [6, 3]]  →  PA = LU',
      'A = [[0, 1], [1, 0]]  →  a row swap',
      'A = [[a, b], [c, d]]  →  symbolic factors',
    ],
  ),
  'linalg_span': TopicSpec(
    id: 'linalg_span',
    icon: Icons.blur_on_rounded,
    fields: [
      TopicField(
        'vectors',
        labelKey: 'topic.linalg_span.field.vectors.label',
        hintKey: 'topic.linalg_span.field.vectors.hint',
        initial: '1, 0; 0, 1',
      ),
      TopicField(
        'target',
        labelKey: 'topic.linalg_span.field.target.label',
        hintKey: 'topic.linalg_span.field.target.hint',
        initial: '3, 5',
      ),
    ],
    examples: [
      'v₁ = (1, 0), v₂ = (0, 1), b = (3, 5)  →  b = 3v₁ + 5v₂',
      'v₁ = (1, 2), v₂ = (2, 4), b = (1, 1)  →  not in the span',
      'v₁ = (1, 2, 3), v₂ = (2, 4, 6)  →  dim span = 1',
    ],
  ),
  'linalg_independence': TopicSpec(
    id: 'linalg_independence',
    icon: Icons.rule_rounded,
    fields: [
      TopicField(
        'vectors',
        labelKey: 'topic.linalg_independence.field.vectors.label',
        hintKey: 'topic.linalg_independence.field.vectors.hint',
        initial: '1, 2; 2, 4',
      ),
    ],
    examples: [
      'v₁ = (1, 2), v₂ = (2, 4)  →  −2v₁ + v₂ = 0',
      'v₁ = (1, 0), v₂ = (0, 1)  →  independent',
      'v₁ = (a, b), v₂ = (c, d)  →  generic answer',
    ],
  ),
  'linalg_basis': TopicSpec(
    id: 'linalg_basis',
    icon: Icons.account_tree_rounded,
    fields: [
      TopicField(
        'vectors',
        labelKey: 'topic.linalg_basis.field.vectors.label',
        hintKey: 'topic.linalg_basis.field.vectors.hint',
        initial: '1, 2, 3; 2, 4, 6; 1, 1, 1',
      ),
    ],
    examples: [
      'v₁ = (1, 2, 3), v₂ = (2, 4, 6), v₃ = (1, 1, 1)  →  dim = 2',
      'v₁ = (1, 0), v₂ = (0, 1), v₃ = (1, 1)  →  dim = 2',
      'v₁ = (0, 0), v₂ = (0, 0)  →  dim = 0',
    ],
  ),
  'linalg_coordinates': TopicSpec(
    id: 'linalg_coordinates',
    icon: Icons.my_location_rounded,
    fields: [
      TopicField(
        'basis',
        labelKey: 'topic.linalg_coordinates.field.basis.label',
        hintKey: 'topic.linalg_coordinates.field.basis.hint',
        initial: '1, 1; 0, 1',
      ),
      TopicField(
        'vector',
        labelKey: 'topic.linalg_coordinates.field.vector.label',
        hintKey: 'topic.linalg_coordinates.field.vector.hint',
        initial: '3, 5',
      ),
    ],
    examples: [
      'B = {(1, 1), (0, 1)}, v = (3, 5)  →  [v]ᴮ = (3, 2)',
      'B = {(1, 0), (0, 1)}, v = (3, 5)  →  [v]ᴮ = (3, 5)',
      'B = {(1, 1), (2, 2)}  →  not a basis',
    ],
  ),
  'linalg_change_basis': TopicSpec(
    id: 'linalg_change_basis',
    icon: Icons.swap_horiz_rounded,
    fields: [
      TopicField(
        'basis_b',
        labelKey: 'topic.linalg_change_basis.field.basis_b.label',
        hintKey: 'topic.linalg_change_basis.field.basis_b.hint',
        initial: '1, 0; 0, 1',
      ),
      TopicField(
        'basis_c',
        labelKey: 'topic.linalg_change_basis.field.basis_c.label',
        hintKey: 'topic.linalg_change_basis.field.basis_c.hint',
        initial: '1, 1; 0, 1',
      ),
    ],
    examples: [
      'B = {(1, 0), (0, 1)}, C = {(1, 1), (0, 1)}  →  P = [[1, 0], [−1, 1]]',
      'B = {(1, 1), (0, 1)}, C = {(1, 0), (0, 1)}  →  the inverse matrix',
      'C = {(1, 1), (2, 2)}  →  not a basis',
    ],
  ),
  'linalg_subspace': TopicSpec(
    id: 'linalg_subspace',
    icon: Icons.checklist_rounded,
    fields: [
      TopicField(
        'vectors',
        labelKey: 'topic.linalg_subspace.field.vectors.label',
        hintKey: 'topic.linalg_subspace.field.vectors.hint',
        initial: '1, 0; 0, 1; 1, 1',
      ),
    ],
    examples: [
      'v₁ = (1, 0), v₂ = (0, 1), v₃ = (1, 1)  →  a plane, dim = 2',
      'v₁ = (1, 2), v₂ = (2, 4)  →  a line, dim = 1',
      'v₁ = (0, 0), v₂ = (0, 0)  →  {0}, dim = 0',
    ],
  ),
  'linalg_projection': TopicSpec(
    id: 'linalg_projection',
    icon: Icons.vertical_align_center_rounded,
    fields: [
      TopicField(
        'basis',
        labelKey: 'topic.linalg_projection.field.basis.label',
        hintKey: 'topic.linalg_projection.field.basis.hint',
        initial: '1, 0; 0, 1',
      ),
      TopicField(
        'vector',
        labelKey: 'topic.linalg_projection.field.vector.label',
        hintKey: 'topic.linalg_projection.field.vector.hint',
        initial: '3, 4',
      ),
    ],
    examples: [
      'W = span{(1, 0), (0, 1)}, v = (3, 4)  →  proj = (3, 4)',
      'W = span{(1, 1, 0), (0, 1, 1)}, v = (1, 2, 3)  →  proj = (1/3, 8/3, 7/3)',
      'W = span{(1, 2), (2, 4)}  →  the basis is dependent',
    ],
  ),
  'linalg_gram_schmidt': TopicSpec(
    id: 'linalg_gram_schmidt',
    icon: Icons.auto_fix_high_rounded,
    fields: [
      TopicField(
        'vectors',
        labelKey: 'topic.linalg_gram_schmidt.field.vectors.label',
        hintKey: 'topic.linalg_gram_schmidt.field.vectors.hint',
        initial: '3, 4; 1, 0',
      ),
    ],
    examples: [
      'v₁ = (3, 4), v₂ = (1, 0)  →  q₁ = (3/5, 4/5), q₂ = (4/5, −3/5)',
      'v₁ = (1, 2), v₂ = (2, 4)  →  v₂ is dropped',
      'v₁ = (1, 0), v₂ = (0, 1)  →  already orthonormal',
    ],
  ),
  'linalg_qr': TopicSpec(
    id: 'linalg_qr',
    icon: Icons.qr_code_2_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_qr.field.matrix.label',
        hintKey: 'topic.linalg_qr.field.matrix.hint',
        initial: '1, 1; 1, 0; 0, 1',
      ),
    ],
    examples: [
      'A = [[1, 1], [1, 0], [0, 1]]  →  A = QR',
      'A = [[1, 2], [3, 4]]  →  Q with radicals, R upper triangular',
      'A = [[1, 2], [2, 4]]  →  the columns are dependent',
    ],
  ),
  'linalg_least_squares': TopicSpec(
    id: 'linalg_least_squares',
    icon: Icons.show_chart_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_least_squares.field.matrix.label',
        hintKey: 'topic.linalg_least_squares.field.matrix.hint',
        initial: '1, 0; 0, 1; 1, 1',
      ),
      TopicField(
        'rhs',
        labelKey: 'topic.linalg_least_squares.field.rhs.label',
        hintKey: 'topic.linalg_least_squares.field.rhs.hint',
        initial: '1; 2; 3',
      ),
    ],
    examples: [
      'A = [[1, 0], [0, 1], [1, 1]], b = (1, 2, 3)  →  exact fit',
      'A = [[1, 0], [0, 1], [1, 1]], b = (1, 2, 4)  →  x = (4/3, 7/3)',
      'A = [[1, 1], [1, 1]], b = (1, 3)  →  not unique',
    ],
  ),
  'linalg_diagonalize': TopicSpec(
    id: 'linalg_diagonalize',
    icon: Icons.filter_center_focus_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_diagonalize.field.matrix.label',
        hintKey: 'topic.linalg_diagonalize.field.matrix.hint',
        initial: '2, 1; 1, 2',
      ),
    ],
    examples: [
      'A = [[2, 1], [1, 2]]  →  A = P D P⁻¹, D = diag(1, 3)',
      'A = [[4, 1], [2, 3]]  →  D = diag(2, 5)',
      'A = [[1, 1], [0, 1]]  →  not diagonalizable',
    ],
  ),
  'linalg_power': TopicSpec(
    id: 'linalg_power',
    icon: Icons.bolt_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_power.field.matrix.label',
        hintKey: 'topic.linalg_power.field.matrix.hint',
        initial: '2, 1; 1, 2',
      ),
      TopicField(
        'power',
        labelKey: 'topic.linalg_power.field.power.label',
        hintKey: 'topic.linalg_power.field.power.hint',
        initial: '3',
      ),
    ],
    examples: [
      'A = [[2, 1], [1, 2]], k = 3  →  A³ = [[14, 13], [13, 14]]',
      'A = [[2, 0], [0, 3]], k = −1  →  A⁻¹ = [[1/2, 0], [0, 1/3]]',
      'A = [[1, 1], [0, 1]], k = 3  →  direct multiplication',
    ],
  ),
  'linalg_orthogonal_complement': TopicSpec(
    id: 'linalg_orthogonal_complement',
    icon: Icons.blur_circular_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_rank.field.matrix.label',
        hintKey: 'topic.linalg_rank.field.matrix.hint',
        initial: '1, 2; 2, 4; 3, 6',
      ),
    ],
    examples: [
      'A = [[1, 2], [2, 4], [3, 6]]  →  A^⊥ = span{(−2, 1, 0), (−3, 0, 1)}',
      'A = [[1, 0], [0, 1]]  →  A^⊥ = {0}',
      'A = [[1, 1, 1]]  →  a plane orthogonal to (1, 1, 1)',
    ],
  ),
  'linalg_graph': TopicSpec(
    id: 'linalg_graph',
    icon: Icons.hub_rounded,
    fields: [
      TopicField(
        'nodes',
        labelKey: 'topic.linalg_graph.field.nodes.label',
        hintKey: 'topic.linalg_graph.field.nodes.hint',
        initial: '3',
      ),
      TopicField(
        'edges',
        labelKey: 'topic.linalg_graph.field.edges.label',
        hintKey: 'topic.linalg_graph.field.edges.hint',
        initial: '1-2, 2-3, 3-1',
      ),
    ],
    examples: [
      '3 nodes, edges 1-2, 2-3, 3-1  →  one cycle',
      '4 nodes, edges 1-2, 3-4  →  two components',
      '3 nodes, edges 1-2, 2-3  →  a tree, no cycle',
    ],
  ),
  'linalg_fft': TopicSpec(
    id: 'linalg_fft',
    icon: Icons.graphic_eq_rounded,
    fields: [
      TopicField(
        'signal',
        labelKey: 'topic.linalg_fft.field.signal.label',
        hintKey: 'topic.linalg_fft.field.signal.hint',
        initial: '1, 2, 3, 4',
      ),
    ],
    examples: [
      'x = (1, 2, 3, 4)  →  X = (10, −2 + 2i, −2, −2 − 2i)',
      'x = (1, 0, 0, 0)  →  a flat spectrum',
      'x = (1, 1, 1, 1)  →  X = (4, 0, 0, 0)',
    ],
  ),
  'linalg_similarity': TopicSpec(
    id: 'linalg_similarity',
    icon: Icons.compare_arrows_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_rank.field.matrix.label',
        hintKey: 'topic.linalg_rank.field.matrix.hint',
        initial: '1, 2; 3, 4',
      ),
      TopicField(
        'change',
        labelKey: 'topic.linalg_similarity.field.change.label',
        hintKey: 'topic.linalg_similarity.field.change.hint',
        initial: '1, 1; 0, 1',
      ),
    ],
    examples: [
      'A = [[1, 2], [3, 4]], P = [[1, 1], [0, 1]]  →  B = P⁻¹AP',
      'A = [[2, 0], [0, 3]], P = [[1, 1], [0, 1]]  →  the same eigenvalues',
      'P singular  →  not a change of basis',
    ],
  ),
  'linalg_matrix_exp': TopicSpec(
    id: 'linalg_matrix_exp',
    icon: Icons.timeline_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_rank.field.matrix.label',
        hintKey: 'topic.linalg_rank.field.matrix.hint',
        initial: '0, 1; -1, 0',
      ),
    ],
    examples: [
      'A = [[0, 1], [−1, 0]]  →  a rotation',
      'A = [[1, 1], [0, 1]]  →  e^{At} = e^t [[1, t], [0, 1]]',
      'A = [[0, 1], [0, 0]]  →  e^{At} = [[1, t], [0, 1]]',
    ],
  ),
  'linalg_positive_definite': TopicSpec(
    id: 'linalg_positive_definite',
    icon: Icons.check_circle_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_rank.field.matrix.label',
        hintKey: 'topic.linalg_rank.field.matrix.hint',
        initial: '4, 2; 2, 3',
      ),
    ],
    examples: [
      'A = [[4, 2], [2, 3]]  →  A = LLᵀ',
      'A = [[1, 2], [2, 1]]  →  not positive definite',
      'A = [[2, −1], [−1, 2]]  →  a positive definite tridiagonal matrix',
    ],
  ),
  'linalg_svd': TopicSpec(
    id: 'linalg_svd',
    icon: Icons.auto_graph_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_rank.field.matrix.label',
        hintKey: 'topic.linalg_rank.field.matrix.hint',
        initial: '3, 0; 0, 2',
      ),
    ],
    examples: [
      'A = [[3, 0], [0, 2]]  →  σ = 3, 2',
      'A = [[1, 0], [0, 0]]  →  rank 1, σ = 1, 0',
      'A = [[1, 2, 3], [4, 5, 6]]  →  a rectangular SVD',
    ],
  ),
  'linalg_norm': TopicSpec(
    id: 'linalg_norm',
    icon: Icons.straighten_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_rank.field.matrix.label',
        hintKey: 'topic.linalg_rank.field.matrix.hint',
        initial: '1, -2; 3, 4',
      ),
    ],
    examples: [
      'A = [[1, −2], [3, 4]]  →  ‖A‖₁ = 6, ‖A‖∞ = 7',
      'A = [[2, 0], [0, 1]]  →  cond₂ = 2',
      'A = [[1, 1], [1, 1]]  →  cond₂ = ∞',
    ],
  ),
  'linalg_power_iteration': TopicSpec(
    id: 'linalg_power_iteration',
    icon: Icons.trending_up_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_rank.field.matrix.label',
        hintKey: 'topic.linalg_rank.field.matrix.hint',
        initial: '2, 1; 1, 2',
      ),
      TopicField(
        'start',
        labelKey: 'topic.linalg_power_iteration.field.start.label',
        hintKey: 'topic.linalg_power_iteration.field.start.hint',
        initial: '1, 0',
      ),
    ],
    examples: [
      'A = [[2, 1], [1, 2]], x₀ = (1, 0)  →  λ = 3',
      'A = [[4, 1], [2, 3]], x₀ = (1, 1)  →  λ = 5',
      'A = [[0, 1], [1, 0]]  →  the eigenvalues tie',
    ],
  ),
  'linalg_qr_algorithm': TopicSpec(
    id: 'linalg_qr_algorithm',
    icon: Icons.qr_code_2_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_rank.field.matrix.label',
        hintKey: 'topic.linalg_rank.field.matrix.hint',
        initial: '2, 1; 1, 2',
      ),
    ],
    examples: [
      'A = [[2, 1], [1, 2]]  →  λ = 3, 1',
      'A = [[4, 1], [1, 4]]  →  λ = 5, 3',
      'A = [[1, 2], [3, 4]]  →  symmetric input required',
    ],
  ),
  'linalg_iterative': TopicSpec(
    id: 'linalg_iterative',
    icon: Icons.sync_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_rank.field.matrix.label',
        hintKey: 'topic.linalg_rank.field.matrix.hint',
        initial: '4, 1; 1, 3',
      ),
      TopicField(
        'rhs',
        labelKey: 'topic.linalg_least_squares.field.rhs.label',
        hintKey: 'topic.linalg_least_squares.field.rhs.hint',
        initial: '1, 2',
      ),
      TopicField(
        'method',
        labelKey: 'topic.linalg_iterative.field.method.label',
        initial: 'jacobi',
        choices: [
          TopicChoice('jacobi', 'topic.linalg_iterative.method.jacobi'),
          TopicChoice(
            'gauss_seidel',
            'topic.linalg_iterative.method.gauss_seidel',
          ),
        ],
      ),
    ],
    examples: [
      'A = [[4, 1], [1, 3]], b = (1, 2)  →  Jacobi',
      'A = [[4, 1], [1, 3]], b = (1, 2)  →  Gauss–Seidel',
      'A = [[1, 2], [2, 1]]  →  the iteration diverges',
    ],
  ),
  'linalg_lp': TopicSpec(
    id: 'linalg_lp',
    icon: Icons.linear_scale_rounded,
    fields: [
      TopicField(
        'objective',
        labelKey: 'topic.linalg_lp.field.objective.label',
        hintKey: 'topic.linalg_lp.field.objective.hint',
        initial: '3, 2',
      ),
      TopicField(
        'constraints',
        labelKey: 'topic.linalg_lp.field.constraints.label',
        hintKey: 'topic.linalg_lp.field.constraints.hint',
        initial: '1, 1; 1, 0; 0, 1',
      ),
      TopicField(
        'bounds',
        labelKey: 'topic.linalg_lp.field.bounds.label',
        hintKey: 'topic.linalg_lp.field.bounds.hint',
        initial: '4, 2, 3',
      ),
    ],
    examples: [
      'max 3x + 2y, x + y ≤ 4, x ≤ 2, y ≤ 3  →  (2, 2), z = 10',
      'max x + y, x + y ≤ 5  →  a whole edge of optima',
      'max x + y, x ≥ 0, y ≥ 0  →  unbounded',
    ],
  ),
  'linalg_game': TopicSpec(
    id: 'linalg_game',
    icon: Icons.sports_esports_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_rank.field.matrix.label',
        hintKey: 'topic.linalg_rank.field.matrix.hint',
        initial: '3, 0; 0, 2',
      ),
    ],
    examples: [
      'A = [[3, 0], [0, 2]]  →  mixed value 6/5',
      'A = [[3, 1], [2, 1]]  →  a saddle point',
      'A = [[1, −1], [−1, 1]]  →  value 0',
    ],
  ),
  'linalg_symmetric': TopicSpec(
    id: 'linalg_symmetric',
    icon: Icons.balance_rounded,
    fields: [
      TopicField(
        'matrix',
        labelKey: 'topic.linalg_rank.field.matrix.label',
        hintKey: 'topic.linalg_rank.field.matrix.hint',
        initial: '2, 1; 1, 2',
      ),
    ],
    examples: [
      'A = [[2, 1], [1, 2]]  →  Q D Qᵀ',
      'A = [[4, 1], [1, 4]]  →  λ = 5, 3',
      'A = [[1, 2], [3, 4]]  →  symmetric input required',
    ],
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
        initial: 'e^(2*t)*sin(3*t)',
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
        initial: 'e^t',
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
        initial: 'e^(-abs(x))',
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
        initial: 'e^x*sin(x)',
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
        initial: 'x*e^x',
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
