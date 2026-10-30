import 'package:flutter/material.dart';

import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class IntegralPage extends StatefulWidget {
  const IntegralPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<IntegralPage> createState() => _IntegralPageState();
}

class _IntegralPageState extends State<IntegralPage> {
  final _input = TextEditingController(text: '2*x*cos(x^2)');
  final _xv = TextEditingController(text: 'x');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;

  static const _examples = <String>[
    '2*x*cos(x^2)',
    'x*exp(x)',
    'x*ln(x)',
    'x^2*sin(x)',
    'exp(x)*sin(x)',
    '1/(x^2-x+1)',
    'sin(x)^2',
    'x/(x^2+1)',
  ];

  @override
  void didUpdateWidget(covariant IntegralPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _result = null;
      _error = null;
    }
  }

  @override
  void dispose() {
    _input.dispose();
    _xv.dispose();
    super.dispose();
  }

  void _solve() {
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
    });
    try {
      final r = engine.integral(
        text,
        x: _xv.text.trim().isEmpty ? 'x' : _xv.text.trim(),
        lang: l.isFa ? 'fa' : 'en',
      );
      if (!mounted) return;
      setState(() {
        _result = r;
        _busy = false;
      });
      revealWidget(_resultKey);
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
                icon: Icons.integration_instructions_rounded,
                title: l.t('Indefinite integral', 'انتگرال نامعین'),
                subtitle: l.t(
                  'The method is detected automatically: substitution, by parts, rational, table, …',
                  'روش به‌صورت خودکار تشخیص داده می‌شود: تغییر متغیر، جزء‌به‌جزء، گویا، جدول، …',
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
                  hintText: l.t('e.g. x*exp(x)', 'مثلاً x*exp(x)'),
                  prefixIcon: Icon(Icons.functions_rounded, size: 20, color: c.muted),
                ),
                onSubmitted: (_) => _solve(),
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
                        _solve();
                      },
                    ),
                ],
              ),
              const SizedBox(height: 16),
              FilledButton.icon(
                onPressed: _busy ? null : _solve,
                icon: _busy
                    ? const SizedBox(
                        width: 17,
                        height: 17,
                        child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
                      )
                    : const Icon(Icons.integration_instructions_rounded, size: 19),
                label: Text(_busy
                    ? l.t('Integrating…', 'در حال انتگرال‌گیری…')
                    : l.t('Compute integral', 'محاسبهٔ انتگرال')),
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
        if (_result != null) ...[
          const SizedBox(height: 18),
          Container(key: _resultKey, child: ResultView(result: _result!, l10n: l)),
        ],
      ],
    );
  }
}
