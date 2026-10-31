import 'package:flutter/material.dart';

import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

/// Limits: value at a point from both sides (or one side), with the limit laws,
/// L'Hôpital and squeeze recorded as steps.
class LimitPage extends StatefulWidget {
  const LimitPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<LimitPage> createState() => _LimitPageState();
}

class _LimitPageState extends State<LimitPage> {
  final _input = TextEditingController(text: 'sin(x)/x');
  final _xv = TextEditingController(text: 'x');
  final _point = TextEditingController(text: '0');
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;
  String _side = 'both';

  static const _examples = <(String, String)>[
    ('sin(x)/x', '0'),
    ('(1-cos(x))/x^2', '0'),
    ('(1+1/x)^x', 'inf'),
    ('x*ln(x)', '0'),
    ('(x^2-4)/(x-2)', '2'),
    ('(3*x^2+2)/(x^2-1)', 'inf'),
    ('tan(x)/x', '0'),
    ('1/x', '0'),
  ];

  @override
  void didUpdateWidget(covariant LimitPage oldWidget) {
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
    _point.dispose();
    super.dispose();
  }

  void _compute() {
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
      final r = engine.limit(
        text,
        x: _xv.text.trim().isEmpty ? 'x' : _xv.text.trim(),
        point: _point.text.trim().isEmpty ? '0' : _point.text.trim(),
        side: _side,
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
                icon: Icons.linear_scale_rounded,
                title: l.t('Limits', 'حد'),
                subtitle: l.t(
                  'Limit laws on the expression tree, L\'Hôpital for indeterminate forms, and one-sided limits.',
                  'قوانین حد روی درخت عبارت، قاعدهٔ هوپیتال برای فرم‌های مبهم و حدهای یک‌طرفه.',
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
                  hintText: l.t('e.g. sin(x)/x', 'مثلاً sin(x)/x'),
                  prefixIcon: Icon(Icons.edit_outlined, size: 20, color: c.muted),
                ),
                onSubmitted: (_) => _compute(),
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  SizedBox(
                    width: 100,
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
                  const SizedBox(width: 10),
                  Expanded(
                    child: TextField(
                      controller: _point,
                      textDirection: TextDirection.ltr,
                      textAlign: TextAlign.left,
                      style: const TextStyle(fontFamily: 'monospace', fontSize: 14),
                      decoration: InputDecoration(
                        labelText: l.t('Approaches', 'به چه چیزی میل می‌کند'),
                        hintText: l.t('0, pi/2, inf', '۰، pi/2، inf'),
                        isDense: true,
                      ),
                      onSubmitted: (_) => _compute(),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              SegmentedButton<String>(
                segments: [
                  ButtonSegment(
                    value: 'both',
                    label: Text(l.t('Two-sided', 'دوطرفه')),
                    icon: const Icon(Icons.compare_arrows_rounded, size: 17),
                  ),
                  ButtonSegment(
                    value: 'left',
                    label: Text(l.t('Left', 'از چپ')),
                    icon: const Icon(Icons.west_rounded, size: 17),
                  ),
                  ButtonSegment(
                    value: 'right',
                    label: Text(l.t('Right', 'از راست')),
                    icon: const Icon(Icons.east_rounded, size: 17),
                  ),
                ],
                selected: {_side},
                onSelectionChanged: (v) => setState(() => _side = v.first),
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
                      label: ex.$1,
                      onTap: () {
                        _input.text = ex.$1;
                        _point.text = ex.$2;
                        _compute();
                      },
                    ),
                ],
              ),
              const SizedBox(height: 16),
              FilledButton.icon(
                onPressed: _busy ? null : _compute,
                icon: _busy
                    ? const SizedBox(
                        width: 17,
                        height: 17,
                        child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
                      )
                    : const Icon(Icons.linear_scale_rounded, size: 19),
                label: Text(_busy
                    ? l.t('Computing…', 'در حال محاسبه…')
                    : l.t('Compute the limit', 'محاسبهٔ حد')),
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
