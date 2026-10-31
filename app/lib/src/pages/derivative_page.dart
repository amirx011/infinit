import 'package:flutter/material.dart';

import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

/// Derivative workspace: order 1, then (below it) order 2, then order 3.
/// Equations such as `x^2+y^2=4` are differentiated implicitly.
class DerivativePage extends StatefulWidget {
  const DerivativePage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<DerivativePage> createState() => _DerivativePageState();
}

class _DerivativePageState extends State<DerivativePage> {
  final _input = TextEditingController(text: 'x^3*sin(x)');
  final _xv = TextEditingController(text: 'x');
  final _firstKey = GlobalKey();
  final _secondKey = GlobalKey();
  final _thirdKey = GlobalKey();
  SolveResult? _first;
  SolveResult? _second;
  SolveResult? _third;
  String? _error;
  bool _busy = false;

  static const _examples = <String>[
    'x^3*sin(x)',
    'exp(x)*cos(x)',
    'ln(x)/x',
    'x*sin(x)',
    'tan(x)',
    'x^2+y^2=4',
  ];

  @override
  void didUpdateWidget(covariant DerivativePage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _first = null;
      _second = null;
      _third = null;
      _error = null;
    }
  }

  @override
  void dispose() {
    _input.dispose();
    _xv.dispose();
    super.dispose();
  }

  GlobalKey _keyFor(int order) =>
      order == 1 ? _firstKey : (order == 2 ? _secondKey : _thirdKey);

  void _compute(int order) {
    final engine = widget.engine;
    final l = widget.l10n;
    if (engine == null) {
      setState(() => _error = widget.engineError ??
          l.t('The computation engine is unavailable.', 'موتور محاسبات در دسترس نیست.'));
      return;
    }
    final text = _input.text.trim();
    if (text.isEmpty) {
      setState(() => _error = l.t('Enter an expression.', 'یک عبارت وارد کنید.'));
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
      // A new computation invalidates the higher orders below it.
      if (order == 1) {
        _second = null;
        _third = null;
      } else if (order == 2) {
        _third = null;
      }
    });
    try {
      final r = engine.derivative(
        text,
        x: _xv.text.trim().isEmpty ? 'x' : _xv.text.trim(),
        order: order,
        lang: l.isFa ? 'fa' : 'en',
      );
      if (!mounted) return;
      setState(() {
        if (order == 1) {
          _first = r;
        } else if (order == 2) {
          _second = r;
        } else {
          _third = r;
        }
        _busy = false;
      });
      revealWidget(_keyFor(order));
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _error = l.t('Engine error: $e', 'خطا در اجرای موتور: $e');
        _busy = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final l = widget.l10n;
    final c = context.colors;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                icon: Icons.drive_file_rename_outline_rounded,
                title: l.t('Derivatives', 'مشتق'),
                subtitle: l.t(
                  'First, second and third derivatives — explicit (AST walk, chain and product rules) or implicit from an equation.',
                  'مشتق مرتبهٔ اول، دوم و سوم — صریح (پیمایش درخت AST، قواعد زنجیره‌ای و ضرب) یا ضمنی از یک معادله.',
                ),
              ),
              const SizedBox(height: 18),
              TextField(
                controller: _input,
                textDirection: TextDirection.ltr,
                textAlign: TextAlign.left,
                style: const TextStyle(
                  fontFamily: 'DejaVu Sans Mono',
                  fontFamilyFallback: ['monospace', 'Vazirmatn'],
                  fontSize: 16,
                  height: 1.5,
                ),
                decoration: InputDecoration(
                  hintText: l.t('e.g. x^3*sin(x)  or  x^2+y^2=4',
                      'مثلاً x^3*sin(x)  یا  x^2+y^2=4'),
                  prefixIcon: Icon(Icons.edit_outlined, size: 20, color: c.muted),
                ),
                onSubmitted: (_) => _compute(1),
              ),
              const SizedBox(height: 12),
              SizedBox(
                width: 110,
                child: TextField(
                  controller: _xv,
                  textDirection: TextDirection.ltr,
                  textAlign: TextAlign.left,
                  style: const TextStyle(fontFamily: 'monospace', fontSize: 14),
                  decoration: InputDecoration(
                    labelText: l.t('Variable', 'متغیر'),
                    isDense: true,
                  ),
                ),
              ),
              const SizedBox(height: 14),
              Text(l.t('Examples', 'نمونه‌ها'),
                  style: Theme.of(context).textTheme.bodySmall),
              const SizedBox(height: 8),
              Wrap(
                spacing: 7,
                runSpacing: 7,
                children: [
                  for (final ex in _examples)
                    ExampleChip(
                      label: ex,
                      onTap: () {
                        _input.text = ex;
                        _compute(1);
                      },
                    ),
                ],
              ),
              const SizedBox(height: 16),
              FilledButton.icon(
                onPressed: _busy ? null : () => _compute(1),
                icon: _busy
                    ? const SizedBox(
                        width: 17,
                        height: 17,
                        child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
                      )
                    : const Icon(Icons.drive_file_rename_outline_rounded, size: 19),
                label: Text(_busy
                    ? l.t('Differentiating…', 'در حال مشتق‌گیری…')
                    : l.t('Compute first derivative', 'محاسبهٔ مشتق اول')),
              ),
            ],
          ),
        ),
        if (_error != null) ...[
          const SizedBox(height: 16),
          SoftCard(
            color: c.warningSoft,
            borderColor: c.warning.withValues(alpha: 0.4),
            padding: const EdgeInsets.all(16),
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Icon(Icons.error_outline_rounded, color: c.warning, size: 20),
                const SizedBox(width: 10),
                Expanded(
                  child: SelectableText(_error!,
                      style: Theme.of(context).textTheme.bodyMedium),
                ),
              ],
            ),
          ),
        ],
        if (_first != null) ...[
          const SizedBox(height: 18),
          Container(key: _firstKey, child: ResultView(result: _first!, l10n: l)),
          const SizedBox(height: 14),
          Align(
            alignment: AlignmentDirectional.centerStart,
            child: OutlinedButton.icon(
              onPressed: _busy ? null : () => _compute(2),
              icon: const Icon(Icons.exposure_plus_2_rounded, size: 18),
              label: Text(l.t('Compute second derivative', 'محاسبهٔ مشتق مرتبهٔ دوم')),
            ),
          ),
        ],
        if (_second != null) ...[
          const SizedBox(height: 18),
          SectionLabel(l.t('Second derivative', 'مشتق مرتبهٔ دوم'),
              icon: Icons.exposure_plus_2_rounded),
          const SizedBox(height: 10),
          Container(key: _secondKey, child: ResultView(result: _second!, l10n: l)),
          const SizedBox(height: 14),
          Align(
            alignment: AlignmentDirectional.centerStart,
            child: OutlinedButton.icon(
              onPressed: _busy ? null : () => _compute(3),
              icon: const Icon(Icons.looks_3_rounded, size: 18),
              label: Text(l.t('Compute third derivative', 'محاسبهٔ مشتق مرتبهٔ سوم')),
            ),
          ),
        ],
        if (_third != null) ...[
          const SizedBox(height: 18),
          SectionLabel(l.t('Third derivative', 'مشتق مرتبهٔ سوم'),
              icon: Icons.looks_3_rounded),
          const SizedBox(height: 10),
          Container(key: _thirdKey, child: ResultView(result: _third!, l10n: l)),
        ],
      ],
    );
  }
}
