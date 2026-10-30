import 'package:flutter/material.dart';

import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class EnvelopePage extends StatefulWidget {
  const EnvelopePage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  @override
  State<EnvelopePage> createState() => _EnvelopePageState();
}

class _EnvelopePageState extends State<EnvelopePage> {
  final _input = TextEditingController(text: 'y = c*x + c^2');
  final _cv = TextEditingController(text: 'c');
  final _xv = TextEditingController(text: 'x');
  final _yv = TextEditingController(text: 'y');
  final _resultKey = GlobalKey();
  bool _orthogonal = false;
  SolveResult? _result;
  String? _error;
  bool _busy = false;

  static const _examples = <String>[
    'y = c*x + c^2',
    'y = c*x - c^2',
    'x^2 + y^2 = c^2',
    'y = c/x',
    '(y - c)^2 = x',
  ];

  @override
  void didUpdateWidget(covariant dynamic oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _result = null;
      _error = null;
    }
  }

  @override
  void dispose() {
    _input.dispose();
    _cv.dispose();
    _xv.dispose();
    _yv.dispose();
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
      setState(() => _error = l.t('Enter a family of curves.', 'معادلهٔ خانواده را وارد کنید.'));
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final r = engine.envelope(
        text,
        c: _cv.text.trim().isEmpty ? 'c' : _cv.text.trim(),
        orthogonal: _orthogonal,
        x: _xv.text.trim().isEmpty ? 'x' : _xv.text.trim(),
        y: _yv.text.trim().isEmpty ? 'y' : _yv.text.trim(),
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
                icon: Icons.join_inner_rounded,
                title: l.t('Envelope & orthogonal trajectories', 'پوش و مسیرهای متعامد'),
                subtitle: l.t(
                  'Eliminate the parameter to find the envelope, and optionally the orthogonal trajectories.',
                  'با حذف پارامتر، پوش خانواده و در صورت نیاز مسیرهای متعامد را بیابید.',
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
                  hintText: l.t('e.g. y = c*x + c^2', 'مثلاً y = c*x + c^2'),
                  prefixIcon: Icon(Icons.timeline_rounded, size: 20, color: c.muted),
                ),
                onSubmitted: (_) => _solve(),
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
              const SizedBox(height: 10),
              SwitchListTile(
                value: _orthogonal,
                onChanged: (v) => setState(() => _orthogonal = v),
                contentPadding: EdgeInsets.zero,
                activeThumbColor: c.accent,
                title: Text(l.t('Orthogonal trajectories', 'مسیرهای متعامد'),
                    style: Theme.of(context).textTheme.bodyMedium),
                subtitle: Text(
                  l.t(
                    'Curves that intersect the family at right angles.',
                    'خانوادهٔ منحنی‌هایی که خانوادهٔ اصلی را با زاویهٔ قائمه قطع می‌کنند.',
                  ),
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ),
              Theme(
                data: Theme.of(context).copyWith(dividerColor: Colors.transparent),
                child: ExpansionTile(
                  tilePadding: EdgeInsets.zero,
                  childrenPadding: const EdgeInsets.only(top: 4, bottom: 4),
                  leading: Icon(Icons.tune_rounded, size: 20, color: c.muted),
                  title: Text(l.t('Advanced options', 'تنظیمات پیشرفته'),
                      style: Theme.of(context).textTheme.bodyMedium),
                  children: [
                    Row(
                      children: [
                        Expanded(child: _smallField(_cv, l.t('Parameter', 'پارامتر'), 'c')),
                        const SizedBox(width: 10),
                        Expanded(child: _smallField(_xv, l.t('Variable x', 'متغیر x'), 'x')),
                        const SizedBox(width: 10),
                        Expanded(child: _smallField(_yv, l.t('Variable y', 'متغیر y'), 'y')),
                      ],
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 10),
              FilledButton.icon(
                onPressed: _busy ? null : _solve,
                icon: _busy
                    ? const SizedBox(
                        width: 17,
                        height: 17,
                        child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
                      )
                    : const Icon(Icons.auto_fix_high_rounded, size: 19),
                label: Text(_busy
                    ? l.t('Analysing…', 'در حال تحلیل…')
                    : l.t('Analyse family', 'تحلیل خانواده')),
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
                  child: SelectableText(
                    _error!,
                    style: Theme.of(context).textTheme.bodyMedium,
                  ),
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

  Widget _smallField(TextEditingController controller, String label, String hint) {
    return TextField(
      controller: controller,
      textDirection: TextDirection.ltr,
      textAlign: TextAlign.left,
      style: const TextStyle(fontFamily: 'monospace', fontSize: 14),
      decoration: InputDecoration(labelText: label, hintText: hint, isDense: true),
    );
  }
}
