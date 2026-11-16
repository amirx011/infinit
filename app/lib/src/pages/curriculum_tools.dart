// Computation cards for the curriculum articles.
//
// Most sections defer to the shared equation solver so the article, the steps
// and the answer all come from one engine.  The four conceptual sections get a
// small tool that computes the object they actually study: a direction field,
// a classification, an existence check and a difference-equation table.
import 'package:flutter/material.dart';

import '../charts.dart';
import '../curriculum_articles.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class CurriculumComputation extends StatelessWidget {
  const CurriculumComputation({
    super.key,
    required this.article,
    required this.l10n,
    required this.engine,
    required this.onSolve,
  });

  final CurriculumArticle article;
  final L10n l10n;
  final NativeEngine? engine;
  final ValueChanged<String> onSolve;

  @override
  Widget build(BuildContext context) {
    return switch (article.tool) {
      CurriculumTool.solver => _SolverLink(
        article: article,
        l10n: l10n,
        onSolve: onSolve,
      ),
      CurriculumTool.directionField => _DirectionField(
        article: article,
        l10n: l10n,
        engine: engine,
      ),
      CurriculumTool.classification => _Classification(
        article: article,
        l10n: l10n,
        engine: engine,
      ),
      CurriculumTool.existence => _Existence(
        article: article,
        l10n: l10n,
        engine: engine,
      ),
      CurriculumTool.difference => _Difference(article: article, l10n: l10n),
    };
  }
}

class _SolverLink extends StatelessWidget {
  const _SolverLink({
    required this.article,
    required this.l10n,
    required this.onSolve,
  });

  final CurriculumArticle article;
  final L10n l10n;
  final ValueChanged<String> onSolve;

  @override
  Widget build(BuildContext context) {
    return SoftCard(
      color: context.colors.accentSoft.withValues(alpha: 0.45),
      borderColor: context.colors.accent.withValues(alpha: 0.25),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SectionLabel(
            l10n.t('ui.step_by_step_solution'),
            icon: Icons.calculate_rounded,
          ),
          const SizedBox(height: 10),
          Text(
            l10n.t('ui.enter_an_equation_the_exact_solution_and_step_by'),
            style: Theme.of(context).textTheme.bodyMedium
                ?.copyWith(height: 1.7),
          ),
          const SizedBox(height: 8),
          MathBlock(math: article.standard),
          const SizedBox(height: 14),
          FilledButton.tonalIcon(
            onPressed: () => onSolve(article.input),
            icon: const Icon(Icons.open_in_new_rounded, size: 18),
            label: Text(l10n.t('ui.solve_an_equation')),
          ),
        ],
      ),
    );
  }
}

class _DirectionField extends StatelessWidget {
  const _DirectionField({
    required this.article,
    required this.l10n,
    required this.engine,
  });

  final CurriculumArticle article;
  final L10n l10n;
  final NativeEngine? engine;

  @override
  Widget build(BuildContext context) {
    return SoftCard(
      color: context.colors.cardAlt,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SectionLabel(
            l10n.t('ui.set_up_and_evaluate'),
            icon: Icons.calculate_rounded,
          ),
          const SizedBox(height: 12),
          FieldPlot(
            engine: engine,
            expression: article.input,
            l10n: l10n,
            title: l10n.t('ui.direction_field_solution_family'),
            caption: l10n.t(
              'ui.each_arrow_is_the_local_slope_of_a_solution_the',
            ),
          ),
        ],
      ),
    );
  }
}

class _Classification extends StatefulWidget {
  const _Classification({
    required this.article,
    required this.l10n,
    required this.engine,
  });

  final CurriculumArticle article;
  final L10n l10n;
  final NativeEngine? engine;

  @override
  State<_Classification> createState() => _ClassificationState();
}

class _ClassificationState extends State<_Classification> {
  late final TextEditingController _input = TextEditingController(
    text: widget.article.input,
  );
  SolveResult? _result;
  bool _busy = false;
  String? _error;

  @override
  void dispose() {
    _input.dispose();
    super.dispose();
  }

  Future<void> _classify() async {
    final engine = widget.engine;
    if (engine == null) {
      setState(
        () =>
            _error = widget.l10n.t('ui.the_computation_engine_is_unavailable'),
      );
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
      _result = null;
    });
    try {
      final result = await engine.solveOdeAsync(
        _input.text.trim(),
        lang: widget.l10n.code,
      );
      if (!mounted) return;
      setState(() {
        _busy = false;
        if (result.ok) {
          _result = result;
        } else {
          _error = result.error.isEmpty
              ? widget.l10n.t('error.unknown_function')
              : result.error;
        }
      });
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _busy = false;
        _error = e.toString();
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final l = widget.l10n;
    return SoftCard(
      color: context.colors.cardAlt,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SectionLabel(
            l.t('ui.set_up_and_evaluate'),
            icon: Icons.calculate_rounded,
          ),
          const SizedBox(height: 12),
          TextField(
            controller: _input,
            textDirection: TextDirection.ltr,
            decoration: InputDecoration(
              labelText: l.t('ui.differential_equation'),
            ),
          ),
          const SizedBox(height: 12),
          FilledButton.tonalIcon(
            onPressed: _busy ? null : _classify,
            icon: const Icon(Icons.rule_rounded, size: 18),
            label: Text(l.t('step.title.classify_the_equation')),
          ),
          if (_error != null) ...[
            const SizedBox(height: 10),
            Text(
              _error!,
              style: Theme.of(context).textTheme.bodySmall
                  ?.copyWith(color: context.colors.warning),
            ),
          ],
          if (_result != null) ...[
            const SizedBox(height: 12),
            Wrap(
              spacing: 10,
              runSpacing: 10,
              children: [
                _Chip(text: l.t('ui.order', {'order': _result!.order})),
                _Chip(text: _typeLabel(l, _result!)),
              ],
            ),
          ],
        ],
      ),
    );
  }
}

class _Existence extends StatefulWidget {
  const _Existence({
    required this.article,
    required this.l10n,
    required this.engine,
  });

  final CurriculumArticle article;
  final L10n l10n;
  final NativeEngine? engine;

  @override
  State<_Existence> createState() => _ExistenceState();
}

class _ExistenceState extends State<_Existence> {
  late final TextEditingController _f = TextEditingController(
    text: widget.article.input,
  );
  late final TextEditingController _x0 = TextEditingController(
    text: widget.article.point.split(',').first.trim(),
  );
  late final TextEditingController _y0 = TextEditingController(
    text: widget.article.point.split(',').last.trim(),
  );
  SolveResult? _result;
  bool _busy = false;
  String? _error;

  @override
  void dispose() {
    _f.dispose();
    _x0.dispose();
    _y0.dispose();
    super.dispose();
  }

  Future<void> _check() async {
    final engine = widget.engine;
    if (engine == null) {
      setState(
        () =>
            _error = widget.l10n.t('ui.the_computation_engine_is_unavailable'),
      );
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
      _result = null;
    });
    try {
      final result = await engine.partialAsync(
        _f.text.trim(),
        vars: 'x,y',
        point: '${_x0.text.trim()},${_y0.text.trim()}',
        lang: widget.l10n.code,
      );
      if (!mounted) return;
      setState(() {
        _busy = false;
        if (result.ok) {
          _result = result;
        } else {
          _error = result.error.isEmpty
              ? widget.l10n.t('error.unknown_function')
              : result.error;
        }
      });
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _busy = false;
        _error = e.toString();
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final l = widget.l10n;
    final result = _result;
    final value = result?.value ?? result?.valueAt;
    return SoftCard(
      color: context.colors.cardAlt,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SectionLabel(
            l.t('ui.set_up_and_evaluate'),
            icon: Icons.calculate_rounded,
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                flex: 3,
                child: TextField(
                  controller: _f,
                  textDirection: TextDirection.ltr,
                  decoration: const InputDecoration(labelText: 'f(x, y)'),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: TextField(
                  controller: _x0,
                  textDirection: TextDirection.ltr,
                  decoration: const InputDecoration(labelText: 'x₀'),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: TextField(
                  controller: _y0,
                  textDirection: TextDirection.ltr,
                  decoration: const InputDecoration(labelText: 'y₀'),
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          FilledButton.tonalIcon(
            onPressed: _busy ? null : _check,
            icon: const Icon(Icons.fact_check_outlined, size: 18),
            label: Text(l.t('ui.analyse_the_function')),
          ),
          if (_error != null) ...[
            const SizedBox(height: 10),
            Text(
              _error!,
              style: Theme.of(context).textTheme.bodySmall
                  ?.copyWith(color: context.colors.warning),
            ),
          ],
          if (value != null) ...[
            const SizedBox(height: 14),
            Text(
              l.t('ui.value_at_the_point'),
              style: Theme.of(context).textTheme.bodySmall
                  ?.copyWith(fontWeight: FontWeight.w600),
            ),
            const SizedBox(height: 6),
            MathBlock(math: value, accent: true),
          ],
          if (result != null) ...[
            if (result.gradient.length > 1) ...[
              const SizedBox(height: 12),
              Text(
                l.t('ui.partial_derivatives'),
                style: Theme.of(context).textTheme.bodySmall
                    ?.copyWith(fontWeight: FontWeight.w600),
              ),
              const SizedBox(height: 6),
              MathBlock(math: result.gradient[1]),
            ],
            const SizedBox(height: 12),
            Text(
              l.t('ui.a_unique_solution_passes_through_this_point'),
              style: Theme.of(context).textTheme.bodySmall
                  ?.copyWith(height: 1.6),
            ),
          ],
        ],
      ),
    );
  }
}

class _Difference extends StatefulWidget {
  const _Difference({required this.article, required this.l10n});

  final CurriculumArticle article;
  final L10n l10n;

  @override
  State<_Difference> createState() => _DifferenceState();
}

class _DifferenceState extends State<_Difference> {
  late final List<String> _seed = widget.article.input.split(',');

  String _seedAt(int index, String fallback) =>
      index < _seed.length ? _seed[index].trim() : fallback;

  late final TextEditingController _a = TextEditingController(
    text: _seedAt(0, '0.5'),
  );
  late final TextEditingController _b = TextEditingController(
    text: _seedAt(1, '3'),
  );
  late final TextEditingController _y0 = TextEditingController(
    text: _seedAt(2, '1'),
  );
  List<double>? _values;
  String? _error;

  @override
  void dispose() {
    _a.dispose();
    _b.dispose();
    _y0.dispose();
    super.dispose();
  }

  void _build() {
    final a = double.tryParse(_a.text.trim());
    final b = double.tryParse(_b.text.trim());
    final y0 = double.tryParse(_y0.text.trim());
    if (a == null || b == null || y0 == null) {
      setState(() {
        _error = widget.l10n.t('ui.enter_a_function');
        _values = null;
      });
      return;
    }
    var current = y0;
    final values = <double>[current];
    for (var n = 0; n < 10; n++) {
      current = a * current + b;
      values.add(current);
    }
    setState(() {
      _error = null;
      _values = values;
    });
  }

  String _number(double value) {
    if (!value.isFinite) return value.sign < 0 ? '−∞' : '∞';
    if (value.abs() < 1e-12) return '0';
    final fixed = value.toStringAsFixed(6);
    return fixed
        .replaceFirst(RegExp(r'0+$'), '')
        .replaceFirst(RegExp(r'\.$'), '');
  }

  @override
  Widget build(BuildContext context) {
    final l = widget.l10n;
    final c = context.colors;
    final a = double.tryParse(_a.text.trim());
    final b = double.tryParse(_b.text.trim());
    final values = _values;
    final hasFixedPoint = a != null && b != null && (a - 1).abs() > 1e-12;
    return SoftCard(
      color: c.cardAlt,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SectionLabel(
            l.t('ui.set_up_and_evaluate'),
            icon: Icons.calculate_rounded,
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: TextField(
                  controller: _a,
                  textDirection: TextDirection.ltr,
                  decoration: const InputDecoration(labelText: 'a'),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: TextField(
                  controller: _b,
                  textDirection: TextDirection.ltr,
                  decoration: const InputDecoration(labelText: 'b'),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: TextField(
                  controller: _y0,
                  textDirection: TextDirection.ltr,
                  decoration: const InputDecoration(labelText: 'y₀'),
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          FilledButton.tonalIcon(
            onPressed: _build,
            icon: const Icon(Icons.table_rows_rounded, size: 18),
            label: Text(l.t('ui.show_the_answer_and_steps')),
          ),
          if (_error != null) ...[
            const SizedBox(height: 10),
            Text(
              _error!,
              style: Theme.of(context).textTheme.bodySmall
                  ?.copyWith(color: c.warning),
            ),
          ],
          if (values != null) ...[
            const SizedBox(height: 14),
            Table(
              border: TableBorder(
                horizontalInside: BorderSide(color: c.line, width: 0.6),
              ),
              children: [
                TableRow(
                  children: [_cell('n', bold: true), _cell('yₙ', bold: true)],
                ),
                for (var n = 0; n < values.length; n++)
                  TableRow(children: [_cell('$n'), _cell(_number(values[n]))]),
              ],
            ),
            if (hasFixedPoint) ...[
              const SizedBox(height: 10),
              MathText(
                '\$y^{*} = ${_number(b / (a - 1))}\$',
                key: const ValueKey('difference-fixed-point'),
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ],
            const SizedBox(height: 10),
            Text(
              a != null && a.abs() < 1
                  ? l.t('ui.converges')
                  : l.t('ui.diverges_2'),
              style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                fontWeight: FontWeight.w700,
                color: a != null && a.abs() < 1 ? c.accent : c.warning,
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _cell(String text, {bool bold = false}) => Padding(
    padding: const EdgeInsets.symmetric(vertical: 4, horizontal: 8),
    child: Text(
      text,
      textDirection: TextDirection.ltr,
      style: TextStyle(
        fontWeight: bold ? FontWeight.w700 : FontWeight.w400,
        fontFamily: 'DejaVu Sans Mono',
        fontFamilyFallback: const ['monospace'],
        fontSize: 13,
      ),
    ),
  );
}

class _Chip extends StatelessWidget {
  const _Chip({required this.text});

  final String text;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
      decoration: BoxDecoration(
        color: c.accentSoft,
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: c.accent.withValues(alpha: 0.3)),
      ),
      child: Text(
        text,
        style: Theme.of(context).textTheme.bodySmall
            ?.copyWith(fontWeight: FontWeight.w600),
      ),
    );
  }
}

String _typeLabel(L10n l, SolveResult result) {
  final key = 'ui.type.${result.typeId}';
  if (l.has(key)) return l.t(key);
  return l.isFa ? result.typeFa : result.typeEn;
}
