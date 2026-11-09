import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_math_fork/flutter_math.dart';

import 'app_state.dart';
import 'calc_widgets.dart';
import 'i18n/sentence.dart';
import 'l10n.dart';
import 'models.dart';
import 'theme.dart';

class PageTitle extends StatelessWidget {
  const PageTitle({
    super.key,
    required this.icon,
    required this.title,
    required this.subtitle,
    this.tone,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final Color? tone;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final accent = tone ?? c.accent;
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Container(
          width: 40,
          height: 40,
          decoration: BoxDecoration(
            color: accent.withValues(alpha: 0.12),
            borderRadius: BorderRadius.circular(12),
            border: Border.all(color: accent.withValues(alpha: 0.28)),
          ),
          child: Icon(icon, color: accent, size: 21),
        ),
        const SizedBox(width: 12),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                title,
                style: Theme.of(context).textTheme.titleLarge?.copyWith(
                  fontWeight: FontWeight.w700,
                  letterSpacing: -0.1,
                ),
              ),
              const SizedBox(height: 1),
              Text(subtitle, style: Theme.of(context).textTheme.bodySmall),
            ],
          ),
        ),
      ],
    );
  }
}

class SoftCard extends StatelessWidget {
  const SoftCard({
    super.key,
    required this.child,
    this.padding = const EdgeInsets.all(22),
    this.color,
    this.borderColor,
    this.elevated = true,
  });

  final Widget child;
  final EdgeInsetsGeometry padding;
  final Color? color;
  final Color? borderColor;
  final bool elevated;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final dark = Theme.of(context).brightness == Brightness.dark;
    return Container(
      decoration: BoxDecoration(
        color: color ?? c.card,
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: borderColor ?? c.line),
        boxShadow: elevated && !dark
            ? [
                BoxShadow(
                  color: Colors.black.withValues(alpha: 0.05),
                  blurRadius: 18,
                  offset: const Offset(0, 8),
                ),
              ]
            : null,
      ),
      padding: padding,
      child: Material(type: MaterialType.transparency, child: child),
    );
  }
}

class SectionLabel extends StatelessWidget {
  const SectionLabel(this.text, {super.key, this.trailing, this.icon});

  final String text;
  final Widget? trailing;
  final IconData? icon;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Row(
      children: [
        if (icon != null) ...[
          Icon(icon, size: 16, color: c.accent),
          const SizedBox(width: 7),
        ],
        Expanded(
          child: Text(
            text,
            style: Theme.of(context).textTheme.titleMedium
                ?.copyWith(fontSize: 14.5),
          ),
        ),
        ?trailing,
      ],
    );
  }
}

class MathBlock extends StatelessWidget {
  const MathBlock({
    super.key,
    required this.math,
    this.size = 19,
    this.accent = false,
    this.onCopy,
    this.tooltip,
  });

  final ExView math;
  final double size;
  final bool accent;
  final VoidCallback? onCopy;
  final String? tooltip;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 15),
      decoration: BoxDecoration(
        color: accent ? c.accentSoft.withValues(alpha: 0.72) : c.cardAlt,
        borderRadius: BorderRadius.circular(15),
        border: Border.all(
          color: accent ? c.accent.withValues(alpha: 0.28) : c.line,
        ),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Expanded(
            child: Directionality(
              textDirection: TextDirection.ltr,
              child: SingleChildScrollView(
                scrollDirection: Axis.horizontal,
                child: Math.tex(
                  math.latex,
                  mathStyle: MathStyle.display,
                  textStyle: TextStyle(
                    fontSize: size,
                    color: accent ? c.text : c.text.withValues(alpha: 0.92),
                  ),
                  onErrorFallback: (error) => Text(
                    math.pretty,
                    style: TextStyle(
                      fontFamily: 'DejaVu Sans Mono',
                      fontFamilyFallback: const ['monospace', 'Vazirmatn'],
                      fontSize: size,
                      color: accent ? c.text : c.text.withValues(alpha: 0.92),
                    ),
                  ),
                ),
              ),
            ),
          ),
          if (onCopy != null)
            IconButton(
              tooltip: tooltip,
              visualDensity: VisualDensity.compact,
              icon: Icon(Icons.copy_rounded, size: 19, color: c.muted),
              onPressed: onCopy,
            ),
        ],
      ),
    );
  }
}

/// A formula rendered on the text baseline, for rows and tables where a full
/// [MathBlock] would be too heavy.  Fractions and roots still use the real
/// mathematical layout instead of the linear `.pretty` fallback.
class MathInline extends StatelessWidget {
  const MathInline({super.key, required this.math, this.size = 15, this.color});

  final ExView math;
  final double size;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final style = TextStyle(fontSize: size, color: color ?? c.text);
    return Directionality(
      textDirection: TextDirection.ltr,
      child: SingleChildScrollView(
        scrollDirection: Axis.horizontal,
        child: Math.tex(
          math.latex,
          mathStyle: MathStyle.text,
          textStyle: style,
          onErrorFallback: (error) => Text(
            math.pretty,
            style: style.copyWith(
              fontFamily: 'DejaVu Sans Mono',
              fontFamilyFallback: const ['monospace', 'Vazirmatn'],
            ),
          ),
        ),
      ),
    );
  }
}

class TypeBadge extends StatelessWidget {
  const TypeBadge(this.text, {super.key, this.icon, this.tone});

  final String text;
  final IconData? icon;
  final Color? tone;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final color = tone ?? c.accent;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.06),
        borderRadius: BorderRadius.circular(100),
        border: Border.all(color: color.withValues(alpha: 0.28)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          if (icon != null) ...[
            Icon(icon, size: 14, color: color),
            const SizedBox(width: 5),
          ],
          Text(
            text,
            style: TextStyle(
              fontFamily: 'Vazirmatn',
              fontSize: 12,
              fontWeight: FontWeight.w600,
              color: color,
            ),
          ),
        ],
      ),
    );
  }
}

/// A compact study note attached to a module. The key idea stays visible so the
/// page teaches while it computes; the worked example and pitfalls open on
/// demand so the workspace is not pushed off screen.
class LearningCard extends StatefulWidget {
  const LearningCard({
    super.key,
    required this.l10n,
    required this.title,
    required this.idea,
    this.points = const [],
    this.example,
    this.exampleCaption,
    this.pitfall,
    this.expandLabel,
    this.initiallyExpanded = false,
  });

  final L10n l10n;
  final String title;
  final String idea;
  final List<String> points;
  final ExView? example;
  final String? exampleCaption;
  final String? pitfall;
  final String? expandLabel;
  final bool initiallyExpanded;

  @override
  State<LearningCard> createState() => _LearningCardState();
}

class _LearningCardState extends State<LearningCard> {
  late bool _expanded = widget.initiallyExpanded;

  bool get _hasMore =>
      widget.points.isNotEmpty ||
      widget.example != null ||
      widget.pitfall != null;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final l = widget.l10n;
    final theme = Theme.of(context);
    return Container(
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: AlignmentDirectional.topStart,
          end: AlignmentDirectional.bottomEnd,
          colors: [c.accentAltSoft, c.card],
        ),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: c.accentAlt.withValues(alpha: 0.28)),
      ),
      padding: const EdgeInsets.fromLTRB(16, 14, 16, 6),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Container(
                width: 34,
                height: 34,
                decoration: BoxDecoration(
                  color: c.accentAlt.withValues(alpha: 0.14),
                  borderRadius: BorderRadius.circular(10),
                ),
                child: Icon(
                  Icons.school_outlined,
                  size: 19,
                  color: c.accentAlt,
                ),
              ),
              const SizedBox(width: 11),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      l.t('STUDY NOTE', 'نکتهٔ آموزشی'),
                      style: TextStyle(
                        fontFamily: 'Vazirmatn',
                        fontSize: 12,
                        letterSpacing: l.isRtl ? 0 : 0.8,
                        fontWeight: FontWeight.w700,
                        color: c.accentAlt,
                      ),
                    ),
                    const SizedBox(height: 1),
                    Text(
                      widget.title,
                      style: theme.textTheme.titleMedium?.copyWith(
                        fontSize: 14.5,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          Text(
            widget.idea,
            style: theme.textTheme.bodyMedium?.copyWith(height: 1.7),
          ),
          if (_hasMore) ...[
            AnimatedCrossFade(
              duration: const Duration(milliseconds: 200),
              sizeCurve: Curves.easeOutCubic,
              crossFadeState: _expanded
                  ? CrossFadeState.showSecond
                  : CrossFadeState.showFirst,
              firstChild: const SizedBox(width: double.infinity, height: 0),
              secondChild: _detail(context),
            ),
            Align(
              alignment: AlignmentDirectional.centerStart,
              child: TextButton.icon(
                onPressed: () => setState(() => _expanded = !_expanded),
                style: TextButton.styleFrom(
                  foregroundColor: c.accentAlt,
                  padding: const EdgeInsets.symmetric(horizontal: 8),
                  minimumSize: const Size(0, 36),
                  textStyle: const TextStyle(
                    fontFamily: 'Vazirmatn',
                    fontSize: 12,
                    fontWeight: FontWeight.w600,
                  ),
                ),
                icon: Icon(
                  _expanded
                      ? Icons.expand_less_rounded
                      : Icons.expand_more_rounded,
                  size: 18,
                ),
                label: Text(
                  _expanded
                      ? l.t('Hide the detail', 'بستن توضیح')
                      : widget.expandLabel ??
                            l.t('Worked example & pitfalls', 'مثال و دام‌ها'),
                ),
              ),
            ),
          ] else
            const SizedBox(height: 8),
        ],
      ),
    );
  }

  Widget _detail(BuildContext context) {
    final c = context.colors;
    final l = widget.l10n;
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.only(top: 4),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          if (widget.points.isNotEmpty) ...[
            const SizedBox(height: 8),
            for (final point in widget.points)
              Padding(
                padding: const EdgeInsets.only(bottom: 7),
                child: Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Padding(
                      padding: const EdgeInsets.only(top: 8),
                      child: Container(
                        width: 5,
                        height: 5,
                        decoration: BoxDecoration(
                          color: c.accentAlt,
                          shape: BoxShape.circle,
                        ),
                      ),
                    ),
                    const SizedBox(width: 9),
                    Expanded(
                      child: Text(
                        point,
                        style: theme.textTheme.bodySmall?.copyWith(
                          height: 1.65,
                          color: c.text.withValues(alpha: 0.86),
                        ),
                      ),
                    ),
                  ],
                ),
              ),
          ],
          if (widget.example != null) ...[
            const SizedBox(height: 8),
            Text(
              widget.exampleCaption ??
                  l.t('A worked example', 'یک مثال حل‌شده'),
              style: theme.textTheme.bodySmall?.copyWith(
                fontWeight: FontWeight.w600,
                color: c.muted,
              ),
            ),
            const SizedBox(height: 7),
            MathBlock(math: widget.example!, size: 17),
          ],
          if (widget.pitfall != null) ...[
            const SizedBox(height: 10),
            Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Icon(
                  Icons.report_gmailerrorred_rounded,
                  size: 17,
                  color: c.warning,
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    '${l.t('Common pitfall', 'دام رایج')}: ${widget.pitfall}',
                    style: theme.textTheme.bodySmall?.copyWith(
                      height: 1.65,
                      color: c.muted,
                    ),
                  ),
                ),
              ],
            ),
          ],
          const SizedBox(height: 2),
        ],
      ),
    );
  }
}

class ResultView extends StatelessWidget {
  const ResultView({
    super.key,
    required this.result,
    required this.l10n,
    this.beforeSteps,
  });

  final SolveResult result;
  final L10n l10n;

  /// Optional visualisation (for example a direction field) shown after the
  /// answer card and before the step-by-step solution.
  final Widget? beforeSteps;

  Future<void> _copyMath(BuildContext context, ExView? math) async {
    if (math == null) return;
    // Copy the same linear notation the user types, never the internal LaTeX.
    final text = math.plain.isNotEmpty ? math.plain : math.pretty;
    await Clipboard.setData(ClipboardData(text: text));
    if (!context.mounted) return;
    ScaffoldMessenger.of(context)
        .showSnackBar(SnackBar(content: Text(l10n.t('Copied.', 'کپی شد.'))));
  }

  @override
  Widget build(BuildContext context) {
    if (!result.ok) {
      final scheme = Theme.of(context).colorScheme;
      return SoftCard(
        borderColor: scheme.error.withValues(alpha: 0.5),
        color: scheme.errorContainer,
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Icon(Icons.error_outline_rounded, color: scheme.error),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    l10n.t('Not solved', 'حل نشد'),
                    style: Theme.of(context).textTheme.titleMedium
                        ?.copyWith(color: scheme.error),
                  ),
                  const SizedBox(height: 6),
                  Text(
                    _errorMessage(),
                    style: Theme.of(context).textTheme.bodyMedium,
                  ),
                ],
              ),
            ),
          ],
        ),
      );
    }

    final settings = SettingsScope.maybeOf(context);
    final showSteps = settings?.showSteps ?? true;
    final showChecks = settings?.showChecks ?? true;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        if (result.interpretedAs.isNotEmpty) ...[
          _interpretationCard(context),
          const SizedBox(height: 18),
        ],
        _answerCard(context),
        if (beforeSteps != null) ...[const SizedBox(height: 18), beforeSteps!],
        if (showSteps && result.steps.isNotEmpty) ...[
          const SizedBox(height: 18),
          _stepsCard(context),
        ],
        if (showChecks &&
            (result.checks.isNotEmpty ||
                result.warnings.isNotEmpty ||
                result.assumptions.isNotEmpty)) ...[
          const SizedBox(height: 18),
          _checksCard(context),
        ],
      ],
    );
  }

  String _errorMessage() {
    final code = result.errorCode;
    if (code.isNotEmpty) {
      final params = <String, Object?>{};
      if (result.hasErrorPoint && result.errorPoint != null) {
        params['point'] = _formatNumber(result.errorPoint!);
      }
      final key = 'error.$code';
      final text = l10n.t(key, params);
      if (text != key) return text;
    }
    if (result.error.isNotEmpty) return result.error;
    return l10n.t('error.unavailable');
  }

  Widget _interpretationCard(BuildContext context) {
    final theme = Theme.of(context);
    final c = context.colors;
    return SoftCard(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      elevated: false,
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(Icons.info_outline_rounded, size: 18, color: c.accent),
          const SizedBox(width: 10),
          Expanded(
            child: LocalizedSentence(
              l10n: l10n,
              messageKey: 'note.interpreted_as',
              math: [
                MathInline(
                  math: ExView(
                    plain: result.interpretedAs,
                    pretty: result.interpretedAs,
                    latex: result.interpretedAs,
                  ),
                ),
              ],
              style: theme.textTheme.bodySmall?.copyWith(height: 1.6),
            ),
          ),
        ],
      ),
    );
  }

  String _typeName() {
    if (result.mode == 'derivative') {
      if (result.implicit) return l10n.t('Implicit derivative', 'مشتق ضمنی');
      if (result.order > 3) {
        return l10n.t(
          'Derivative of order ${result.order}',
          'مشتق مرتبهٔ ${result.order}',
        );
      }
      switch (result.order) {
        case 3:
          return l10n.t('Third derivative', 'مشتق مرتبهٔ سوم');
        case 2:
          return l10n.t('Second derivative', 'مشتق مرتبهٔ دوم');
        default:
          return l10n.t('First derivative', 'مشتق مرتبهٔ اول');
      }
    }
    if (result.mode == 'limit') {
      if (result.side == 'left') return l10n.t('Left-hand limit', 'حد از چپ');
      if (result.side == 'right') {
        return l10n.t('Right-hand limit', 'حد از راست');
      }
      return l10n.t('Limit', 'حد');
    }
    if (result.mode == 'continuity') {
      return l10n.t('Continuity', 'پیوستگی');
    }
    if (result.mode == 'integral') {
      final def = result.definite;
      switch (result.dimension) {
        case 3:
          return def
              ? l10n.t('Triple integral (definite)', 'انتگرال سه‌گانهٔ معین')
              : l10n.t('Triple integral', 'انتگرال سه‌گانه');
        case 2:
          return def
              ? l10n.t('Double integral (definite)', 'انتگرال دوگانهٔ معین')
              : l10n.t('Double integral', 'انتگرال دوگانه');
        default:
          return def
              ? l10n.t('Definite integral', 'انتگرال معین')
              : l10n.t('Indefinite integral', 'انتگرال نامعین');
      }
    }
    return l10n.isFa ? result.typeFa : result.typeEn;
  }

  String _angleText(double angle) => angle == angle.roundToDouble()
      ? '${angle.toStringAsFixed(0)}°'
      : '${angle.toStringAsFixed(1)}°';

  String _seriesTestLabel(String test) => switch (test) {
    'geometric' => l10n.t('Geometric series', 'سری هندسی'),
    'p-series' => l10n.t('p-series', 'سری p'),
    'ratio' => l10n.t('Ratio test', 'آزمون نسبت'),
    'root' => l10n.t('Root test', 'آزمون ریشه'),
    'alternating' => l10n.t('Alternating series', 'سری متناوب'),
    'nth-term' => l10n.t('nth-term test', 'آزمون جملهٔ nام'),
    _ => l10n.t('Numeric evidence', 'شواهد عددی'),
  };

  int _decimals(BuildContext context) =>
      SettingsScope.maybeOf(context)?.decimalPlaces ?? 0;

  String _formatNumber(double v, {int decimals = 0}) {
    if (v.isNaN) return '?';
    if (!v.isFinite) return v.sign < 0 ? '−∞' : '∞';
    if (v.abs() < 1e-12) return '0';
    if (decimals > 0) {
      var fixed = v.toStringAsFixed(decimals);
      if (fixed.contains('.')) {
        fixed = fixed.replaceFirst(RegExp(r'0+$'), '');
        if (fixed.endsWith('.')) fixed = fixed.substring(0, fixed.length - 1);
      }
      return fixed;
    }
    var s = v.toStringAsPrecision(10);
    if (s.contains('e')) return s;
    if (s.contains('.')) s = s.replaceFirst(RegExp(r'0+$'), '');
    if (s.endsWith('.')) s = s.substring(0, s.length - 1);
    return s;
  }

  String _limitSideText(ExView? value, bool infinite, int sign) {
    if (value != null) return value.pretty;
    if (infinite) return sign < 0 ? '−∞' : '∞';
    return l10n.t('does not exist', 'وجود ندارد');
  }

  String _limitSideLatex(ExView? value, bool infinite, int sign) {
    if (value != null) return value.latex;
    if (infinite) return sign < 0 ? '-\\infty' : '\\infty';
    return '\\text{DNE}';
  }

  String _lawName(String id) {
    switch (id) {
      case 'constant':
        return l10n.t('constant law', 'قانون ثابت');
      case 'identity':
        return l10n.t('identity law', 'قانون متغیر');
      case 'sum':
        return l10n.t('sum law', 'قانون جمع');
      case 'product':
        return l10n.t('product law', 'قانون ضرب');
      case 'quotient':
        return l10n.t('quotient law', 'قانون تقسیم');
      case 'power':
        return l10n.t('power law', 'قانون توان');
      case 'composite':
        return l10n.t('composition', 'ترکیب');
      case 'lhopital':
        return l10n.t("L'Hôpital", 'هوپیتال');
      case 'algebraic_simplification':
        return l10n.t('algebraic simplification', 'ساده‌سازی جبری');
      case 'exponential_rewrite':
        return l10n.t('exponential rewrite', 'بازنویسی نمایی');
      case 'squeeze':
        return l10n.t('squeeze law', 'قانون فشردگی');
      case 'two_sided':
        return l10n.t('two-sided check', 'بررسی دوطرفه');
      default:
        return id;
    }
  }

  String _discontinuityName(String kind) {
    switch (kind) {
      case 'removable':
        return l10n.t('Removable discontinuity', 'انقطاع رفع‌شدنی');
      case 'jump':
        return l10n.t('Jump discontinuity', 'انقطاع جهشی');
      case 'infinite':
        return l10n.t('Infinite discontinuity', 'انقطاع نامتناهی');
      case 'oscillatory':
        return l10n.t('The limit does not exist', 'حد وجود ندارد');
      default:
        return l10n.t('Not continuous', 'پیوسته نیست');
    }
  }

  List<String> get _integralVars =>
      result.vars.isEmpty ? <String>[result.variable] : result.vars;

  ExView? _integralFormula(int decimals) {
    final f = result.normalized;
    if (f == null) return null;
    final vars = _integralVars;
    final latexSigns = vars.length >= 3
        ? '\\iiint'
        : (vars.length == 2 ? '\\iint' : '\\int');
    final prettySigns = vars.length >= 3
        ? '∫∫∫'
        : (vars.length == 2 ? '∫∫' : '∫');
    final diffs = vars.map((v) => '\\,d$v').join();
    final prettyDiffs = vars.map((v) => ' d$v').join();
    final solutionLatex = result.solution?.latex;
    final solutionPretty = result.solution?.pretty;
    final valueText = _formatNumber(result.numericValue, decimals: decimals);
    final rhsLatex = result.divergent
        ? '\\text{diverges}'
        : (solutionLatex ?? (result.hasNumeric ? valueText : '\\text{?}'));
    final rhsPretty = result.divergent
        ? l10n.t('diverges', 'واگرا')
        : (solutionPretty ?? (result.hasNumeric ? valueText : '?'));

    String latex;
    String pretty;
    if (result.definite) {
      final buffer = StringBuffer();
      for (var i = vars.length - 1; i >= 0; i--) {
        final lo = i < result.lower.length ? result.lower[i] : '0';
        final hi = i < result.upper.length ? result.upper[i] : '1';
        buffer.write('\\int_{$lo}^{$hi}');
      }
      latex = '$buffer ${f.latex}$diffs';
      latex += ' = $rhsLatex';
      pretty =
          '$prettySigns${result.lower.isEmpty ? '' : ' '}${f.pretty}$prettyDiffs';
      if (result.lower.length == vars.length &&
          result.upper.length == vars.length) {
        final bounds = <String>[];
        for (var i = 0; i < vars.length; i++) {
          bounds.add('${vars[i]}: ${result.lower[i]} → ${result.upper[i]}');
        }
        pretty = '${bounds.join(', ')}:  $pretty';
      }
      pretty += ' = $rhsPretty';
    } else {
      latex = '$latexSigns ${f.latex}$diffs = ${solutionLatex ?? ''} + C';
      pretty =
          '$prettySigns ${f.pretty}$prettyDiffs = ${solutionPretty ?? ''} + C';
    }
    return ExView(plain: pretty, pretty: pretty, latex: latex);
  }

  ExView? _derivativeFormula() {
    final d = result.solution;
    if (d == null) return null;
    final v = result.variable.isEmpty ? 'x' : result.variable;
    final order = result.order <= 0 ? 1 : result.order;
    if (result.implicit) {
      final y = result.vars.isNotEmpty ? result.vars.first : 'y';
      final symbol = order == 1 ? 'd$y/d$v' : 'd^$order$y/d$v^$order';
      final latex = order == 1
          ? '\\frac{d$y}{d$v}'
          : '\\frac{d^{$order}$y}{d$v^{$order}}';
      return ExView(
        plain: '$symbol = ${d.plain}',
        pretty: '$symbol = ${d.pretty}',
        latex: '$latex = ${d.latex}',
      );
    }
    final f = result.normalized;
    if (f == null) return null;
    final symbol = order == 1 ? 'd' : 'd^$order';
    final latexOp = order == 1
        ? '\\frac{d}{d$v}'
        : '\\frac{d^{$order}}{d$v^{$order}}';
    return ExView(
      plain: '$symbol/d$v [${f.plain}] = ${d.plain}',
      pretty: '$symbol/d$v [${f.pretty}] = ${d.pretty}',
      latex: '$latexOp\\left(${f.latex}\\right) = ${d.latex}',
    );
  }

  Widget _answerCard(BuildContext context) {
    final c = context.colors;
    final badges = <Widget>[];
    if (_typeName().isNotEmpty) {
      badges.add(TypeBadge(_typeName(), icon: Icons.category_outlined));
    }
    if (result.order > 0) {
      badges.add(
        TypeBadge(l10n.t('Order ${result.order}', 'مرتبه ${result.order}')),
      );
    }
    if (result.mode == 'oblique') {
      final angle = result.trajectoryAngle;
      final text = angle == angle.roundToDouble()
          ? 'α = ${angle.toStringAsFixed(0)}°'
          : 'α = ${angle.toStringAsFixed(1)}°';
      badges.add(TypeBadge(text, icon: Icons.straighten_rounded));
    }
    if (!result.closedForm) {
      badges.add(
        TypeBadge(
          l10n.t('Quadrature answer', 'جواب کوادراتوری'),
          icon: Icons.functions_rounded,
          tone: c.warning,
        ),
      );
    }
    if (result.divergent) {
      badges.add(
        TypeBadge(
          l10n.t('Divergent', 'واگرا'),
          icon: Icons.trending_up_rounded,
          tone: c.warning,
        ),
      );
    } else if (result.improper) {
      badges.add(
        TypeBadge(
          l10n.t('Improper integral', 'انتگرال ناسره'),
          icon: Icons.warning_amber_rounded,
          tone: c.warning,
        ),
      );
    }
    if (result.ok && !result.verified) {
      badges.add(
        TypeBadge(
          l10n.t('Not verified', 'تأییدنشده'),
          icon: Icons.help_outline_rounded,
          tone: c.warning,
        ),
      );
    }
    if (result.timeMs > 0) {
      badges.add(
        TypeBadge(
          '${result.timeMs} ms',
          icon: Icons.bolt_rounded,
          tone: c.muted,
        ),
      );
    }

    final children = <Widget>[];
    children.add(Wrap(spacing: 8, runSpacing: 8, children: badges));
    children.add(const SizedBox(height: 16));

    if (result.numericOnly) {
      children.add(SectionLabel(l10n.t('Numerical solution', 'جواب عددی')));
      children.add(const SizedBox(height: 10));
      final rhs = result.numericRhs.isEmpty ? 'f(x, y, y′)' : result.numericRhs;
      children.add(
        MathBlock(
          math: ExView(
            plain: "y'' = $rhs",
            pretty: "y″ = $rhs",
            latex: "y'' = ${rhs.replaceAll('−', '-')}",
          ),
          accent: true,
          size: 22,
          tooltip: l10n.t('Copy', 'کپی'),
        ),
      );
      children.add(const SizedBox(height: 12));
      children.add(
        Text(
          result.numericNote.isEmpty
              ? l10n.t(
                  'No closed form was found. The curves below are approximate RK4 trajectories.',
                  'فرم بسته‌ای پیدا نشد. منحنی‌های زیر مسیرهای تقریبی RK4 هستند.',
                )
              : result.numericNote,
          style: Theme.of(context).textTheme.bodyMedium,
        ),
      );
      children.add(const SizedBox(height: 10));
      children.add(
        TypeBadge(
          l10n.t('Approximate', 'تقریبی'),
          icon: Icons.straighten_rounded,
          tone: c.warning,
        ),
      );
    } else if (result.mode == 'wronskian') {
      children.add(
        SectionLabel(l10n.t('Wronskian determinant', 'دترمینان رونسکی')),
      );
      children.add(const SizedBox(height: 10));
      if (result.wronskian != null) {
        children.add(
          MathBlock(
            math: result.wronskian!,
            accent: true,
            size: 22,
            tooltip: l10n.t('Copy', 'کپی'),
            onCopy: () => _copyMath(context, result.wronskian),
          ),
        );
      }
      children.add(const SizedBox(height: 12));
      children.add(_independenceBanner(context));
    } else if (result.mode == 'integral') {
      final isDefinite = result.definite;
      children.add(
        SectionLabel(
          isDefinite
              ? l10n.t('Definite integral', 'انتگرال معین')
              : l10n.t('Indefinite integral', 'انتگرال نامعین'),
        ),
      );
      children.add(const SizedBox(height: 10));
      final formula = _integralFormula(_decimals(context));
      if (formula != null) {
        children.add(
          MathBlock(
            math: formula,
            accent: true,
            size: 21,
            tooltip: l10n.t('Copy', 'کپی'),
            onCopy: () => _copyMath(context, formula),
          ),
        );
      }
      if (isDefinite && result.solution != null && result.hasNumeric) {
        children.add(const SizedBox(height: 10));
        children.add(
          Text(
            '≈ ${_formatNumber(result.numericValue, decimals: _decimals(context))}',
            style: Theme.of(context).textTheme.bodySmall
                ?.copyWith(fontFamily: 'DejaVu Sans Mono', letterSpacing: 0.2),
          ),
        );
      }
      if (result.divergent) {
        children.add(const SizedBox(height: 12));
        children.add(
          _NoteRow(
            icon: Icons.trending_up_rounded,
            text: l10n.t(
              'The integral diverges: the integrand is singular or the tail does not vanish.',
              'انتگرال واگرا است: انتگرال‌ده تکین است یا دنبالهٔ بی‌نهایت به صفر نمی‌رسد.',
            ),
            color: c.warning,
          ),
        );
      } else if (result.improper && result.singularities.isNotEmpty) {
        children.add(const SizedBox(height: 12));
        children.add(
          _NoteRow(
            icon: Icons.warning_amber_rounded,
            text: l10n.t(
              'Improper integral at ${result.singularities.join(', ')}; one-sided limits were used.',
              'انتگرال ناسره در ${result.singularities.join('، ')}؛ از حدهای یک‌طرفه استفاده شد.',
            ),
            color: c.muted,
          ),
        );
      }
      if (result.method.isNotEmpty) {
        children.add(const SizedBox(height: 12));
        children.add(
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              TypeBadge(
                _methodName(result.method, l10n),
                icon: Icons.route_outlined,
              ),
            ],
          ),
        );
      }
    } else if (result.mode == 'limit') {
      children.add(SectionLabel(l10n.t('Limit', 'حد')));
      children.add(const SizedBox(height: 10));
      final f = result.normalized;
      final v = result.variable.isEmpty ? 'x' : result.variable;
      final a = result.point.isEmpty ? '0' : result.point;
      final sub = result.side == 'left'
          ? '$a^-'
          : (result.side == 'right' ? '$a^+' : a);
      String rhsLatex;
      String rhsPretty;
      if (result.solution != null) {
        rhsLatex = result.solution!.latex;
        rhsPretty = result.solution!.pretty;
      } else if (result.limitInfinite) {
        rhsLatex = result.limitSign < 0 ? '-\\infty' : '\\infty';
        rhsPretty = result.limitSign < 0 ? '−∞' : '∞';
      } else if (result.hasNumeric) {
        rhsLatex =
            '\\approx ${_formatNumber(result.numericValue, decimals: _decimals(context))}';
        rhsPretty =
            '≈ ${_formatNumber(result.numericValue, decimals: _decimals(context))}';
      } else {
        rhsLatex = '\\text{does not exist}';
        rhsPretty = l10n.t('does not exist', 'وجود ندارد');
      }
      final bodyLatex = f == null ? '' : f.latex;
      final bodyPretty = f == null ? '' : f.pretty;
      final formula = ExView(
        plain: 'lim $v -> $sub  $bodyPretty  = $rhsPretty',
        pretty: 'lim $v → $sub  ($bodyPretty) = $rhsPretty',
        latex: '\\lim_{$v \\to $sub} $bodyLatex = $rhsLatex',
      );
      children.add(
        MathBlock(
          math: formula,
          accent: true,
          size: 21,
          tooltip: l10n.t('Copy', 'کپی'),
          onCopy: () => _copyMath(context, formula),
        ),
      );
      if (result.side == 'both' &&
          (result.leftExists ||
              result.rightExists ||
              result.leftInfinite ||
              result.rightInfinite)) {
        children.add(const SizedBox(height: 12));
        children.add(
          Wrap(
            spacing: 10,
            runSpacing: 8,
            children: [
              TypeBadge(
                '${l10n.t('Left', 'چپ')}: ${_limitSideText(result.leftValue, result.leftInfinite, result.leftSign)}',
                icon: Icons.west_rounded,
                tone: c.muted,
              ),
              TypeBadge(
                '${l10n.t('Right', 'راست')}: ${_limitSideText(result.rightValue, result.rightInfinite, result.rightSign)}',
                icon: Icons.east_rounded,
                tone: c.muted,
              ),
            ],
          ),
        );
      }
      if (result.laws.isNotEmpty) {
        children.add(const SizedBox(height: 12));
        children.add(
          Text(
            '${l10n.t('Rules', 'قوانین')}: ${result.laws.map((e) => _lawName(e)).join(' · ')}',
            style: Theme.of(context).textTheme.bodySmall,
          ),
        );
      }
    } else if (result.mode == 'continuity') {
      final v = result.variable.isEmpty ? 'x' : result.variable;
      final a = result.point.isEmpty ? '0' : result.point;
      final verdict = result.continuous
          ? l10n.t('Continuous at this point', 'در این نقطه پیوسته است')
          : _discontinuityName(result.discontinuity);
      children.add(SectionLabel(l10n.t('Continuity', 'پیوستگی')));
      children.add(const SizedBox(height: 10));
      children.add(
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 13),
          decoration: BoxDecoration(
            color: result.continuous ? c.accentSoft : c.warningSoft,
            borderRadius: BorderRadius.circular(14),
            border: Border.all(
              color: (result.continuous ? c.accent : c.warning).withValues(
                alpha: 0.3,
              ),
            ),
          ),
          child: Row(
            children: [
              Icon(
                result.continuous
                    ? Icons.check_circle_rounded
                    : Icons.info_rounded,
                size: 19,
                color: result.continuous ? c.success : c.warning,
              ),
              const SizedBox(width: 9),
              Expanded(
                child: Text(
                  verdict,
                  style: Theme.of(context).textTheme.bodyMedium,
                ),
              ),
            ],
          ),
        ),
      );
      children.add(const SizedBox(height: 12));
      final lines = <ExView>[
        ExView(
          plain:
              'lim $v -> $a-  f = ${_limitSideText(result.leftValue, result.leftInfinite, result.leftSign)}',
          pretty:
              'lim $v → $a⁻  f($v) = ${_limitSideText(result.leftValue, result.leftInfinite, result.leftSign)}',
          latex:
              '\\lim_{$v \\to $a^-} f($v) = ${_limitSideLatex(result.leftValue, result.leftInfinite, result.leftSign)}',
        ),
        ExView(
          plain:
              'lim $v -> $a+  f = ${_limitSideText(result.rightValue, result.rightInfinite, result.rightSign)}',
          pretty:
              'lim $v → $a⁺  f($v) = ${_limitSideText(result.rightValue, result.rightInfinite, result.rightSign)}',
          latex:
              '\\lim_{$v \\to $a^+} f($v) = ${_limitSideLatex(result.rightValue, result.rightInfinite, result.rightSign)}',
        ),
        ExView(
          plain: 'f($a) = ${result.valueAt?.plain ?? 'undefined'}',
          pretty:
              'f($a) = ${result.valueAt?.pretty ?? l10n.t('undefined', 'تعریف‌نشده')}',
          latex: 'f($a) = ${result.valueAt?.latex ?? '\\text{undefined}'}',
        ),
      ];
      for (final line in lines) {
        children.add(
          Padding(
            padding: const EdgeInsets.only(bottom: 8),
            child: MathBlock(math: line, size: 18),
          ),
        );
      }
    } else if (result.mode == 'derivative') {
      children.add(SectionLabel(l10n.t('Derivative', 'مشتق')));
      children.add(const SizedBox(height: 10));
      final formula = _derivativeFormula();
      if (formula != null) {
        children.add(
          MathBlock(
            math: formula,
            accent: true,
            size: 21,
            tooltip: l10n.t('Copy', 'کپی'),
            onCopy: () => _copyMath(context, formula),
          ),
        );
      }
    } else if (result.mode == 'behaviour') {
      children.addAll(_behaviourAnswer(context));
    } else if (result.mode == 'tangent') {
      children.addAll(_tangentAnswer(context));
    } else if (result.mode == 'optimize') {
      children.addAll(_optimiseAnswer(context));
    } else if (result.mode == 'related_rates') {
      children.addAll(_relatedRatesAnswer(context));
    } else if (result.mode == 'mvt' ||
        result.mode == 'cauchy' ||
        result.mode == 'lhopital') {
      children.addAll(_theoremAnswer(context));
    } else if (result.mode == 'envelope') {
      children.add(
        SectionLabel(l10n.t('Envelope of the family', 'پوش خانواده')),
      );
      children.add(const SizedBox(height: 10));
      if (result.envelope.isEmpty) {
        children.add(
          Container(
            padding: const EdgeInsets.all(16),
            decoration: BoxDecoration(
              color: c.cardAlt,
              borderRadius: BorderRadius.circular(14),
              border: Border.all(color: c.line),
            ),
            child: Text(
              l10n.t('This family has no envelope.', 'این خانواده پوش ندارد.'),
            ),
          ),
        );
      } else {
        for (var i = 0; i < result.envelope.length; i++) {
          final curve = result.envelope[i];
          children.add(
            Padding(
              padding: EdgeInsets.only(
                bottom: i == result.envelope.length - 1 ? 0 : 8,
              ),
              child: MathBlock(
                math: ExView(
                  plain: '${curve.plain} = 0',
                  pretty: '${curve.pretty} = 0',
                  latex: '${curve.latex} = 0',
                ),
                accent: true,
                size: 21,
                tooltip: l10n.t('Copy', 'کپی'),
                onCopy: () => _copyMath(context, curve),
              ),
            ),
          );
        }
      }
      if (result.parametricAvailable) {
        children.add(const SizedBox(height: 14));
        children.add(
          SectionLabel(l10n.t('Parametric solution', 'جواب پارامتری')),
        );
        if (result.paramX != null) {
          children.add(const SizedBox(height: 8));
          children.add(
            _labelledMath('x(${result.paramName})', result.paramX!, context),
          );
        }
        if (result.paramY != null) {
          children.add(const SizedBox(height: 8));
          children.add(
            _labelledMath('y(${result.paramName})', result.paramY!, context),
          );
        }
      }
      if (result.orthogonal != null) {
        final o = result.orthogonal!;
        final expression = result.orthogonalExplicit
            ? 'y = ${o.pretty}'
            : '${o.pretty} = C';
        children.add(const SizedBox(height: 16));
        children.add(
          SectionLabel(l10n.t('Orthogonal trajectories', 'مسیرهای قائم')),
        );
        children.add(const SizedBox(height: 10));
        children.add(
          MathBlock(
            math: ExView(
              plain: result.orthogonalExplicit
                  ? 'y = ${o.plain}'
                  : '${o.plain} = C',
              pretty: expression,
              latex: result.orthogonalExplicit
                  ? 'y = ${o.latex}'
                  : '${o.latex} = C',
            ),
            accent: true,
            size: 20,
            tooltip: l10n.t('Copy', 'کپی'),
            onCopy: () => _copyMath(context, o),
          ),
        );
      }
    } else if (result.mode == 'orthogonal' || result.mode == 'oblique') {
      final t = result.trajectorySolution;
      if (t != null) {
        final explicit = result.trajectoryIsExplicit;
        final oblique = result.mode == 'oblique';
        children.add(
          SectionLabel(
            oblique
                ? l10n.t('Oblique trajectories', 'مسیرهای مایل')
                : l10n.t('Orthogonal trajectories', 'مسیرهای قائم'),
          ),
        );
        children.add(const SizedBox(height: 10));
        final latex = explicit ? 'y = ${t.latex}' : '${t.latex} = C';
        final pretty = explicit ? 'y = ${t.pretty}' : '${t.pretty} = C';
        final plain = explicit ? 'y = ${t.plain}' : '${t.plain} = C';
        children.add(
          MathBlock(
            math: ExView(plain: plain, pretty: pretty, latex: latex),
            accent: true,
            size: 21,
            tooltip: l10n.t('Copy', 'کپی'),
            onCopy: () => _copyMath(
              context,
              ExView(plain: plain, pretty: pretty, latex: latex),
            ),
          ),
        );
        if (oblique) {
          children.add(const SizedBox(height: 12));
          children.add(
            _NoteRow(
              icon: Icons.straighten_rounded,
              text: l10n.t(
                'The family crosses every member at a constant angle α = ${_angleText(result.trajectoryAngle)}.',
                'خانواده هر عضو را با زاویهٔ ثابت α = ${_angleText(result.trajectoryAngle)} قطع می‌کند.',
              ),
              color: c.muted,
            ),
          );
        }
      }
    } else if (result.mode == 'taylor') {
      children.add(
        SectionLabel(
          result.application == 'maclaurin'
              ? l10n.t('Maclaurin polynomial', 'چندجمله‌ای مکلورن')
              : l10n.t('Taylor polynomial', 'چندجمله‌ای تیلور'),
        ),
      );
      children.add(const SizedBox(height: 10));
      if (result.polynomial != null) {
        children.add(
          MathBlock(
            math: result.polynomial!,
            accent: true,
            size: 21,
            tooltip: l10n.t('Copy', 'کپی'),
            onCopy: () => _copyMath(context, result.polynomial),
          ),
        );
      }
      if (result.coefficients.isNotEmpty) {
        children.add(const SizedBox(height: 14));
        children.add(SectionLabel(l10n.t('Coefficients', 'ضرایب')));
        children.add(const SizedBox(height: 8));
        children.add(
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              for (var i = 0; i < result.coefficients.length; i++)
                TypeBadge(
                  'c$i = ${result.coefficients[i].pretty}',
                  icon: Icons.tag_rounded,
                ),
            ],
          ),
        );
      }
      if (result.remainder != null) {
        children.add(const SizedBox(height: 14));
        children.add(
          SectionLabel(l10n.t('Lagrange remainder', 'باقی‌ماندهٔ لاگرانژ')),
        );
        children.add(const SizedBox(height: 8));
        children.add(MathBlock(math: result.remainder!, size: 17));
      }
    } else if (result.mode == 'integral_apps') {
      final label = switch (result.application) {
        'volume' => l10n.t('Volume of revolution', 'حجم دوران'),
        'arc_length' => l10n.t('Arc length', 'طول قوس'),
        _ => l10n.t('Area between curves', 'مساحت بین دو منحنی'),
      };
      children.add(SectionLabel(label));
      children.add(const SizedBox(height: 10));
      final answer = result.value ?? result.solution;
      if (answer != null) {
        children.add(
          MathBlock(
            math: answer,
            accent: true,
            size: 21,
            tooltip: l10n.t('Copy', 'کپی'),
            onCopy: () => _copyMath(context, answer),
          ),
        );
      }
      if (result.hasNumeric) {
        children.add(const SizedBox(height: 10));
        children.add(
          Text(
            '≈ ${_formatNumber(result.numericValue, decimals: _decimals(context))}',
            style: Theme.of(context).textTheme.bodySmall
                ?.copyWith(fontFamily: 'DejaVu Sans Mono', letterSpacing: 0.2),
          ),
        );
      }
      if (result.integrand != null) {
        children.add(const SizedBox(height: 14));
        children.add(SectionLabel(l10n.t('Integrand', 'انتگرال‌ده')));
        children.add(const SizedBox(height: 8));
        children.add(MathBlock(math: result.integrand!, size: 17));
      }
    } else if (result.mode == 'partial') {
      if (result.functionValue != null) {
        children.add(
          SectionLabel(
            l10n.t('Value at the point', 'مقدار در نقطه') +
                (result.pointLabel.isEmpty ? '' : ' ${result.pointLabel}'),
          ),
        );
        children.add(const SizedBox(height: 8));
        children.add(MathBlock(math: result.functionValue!, size: 18));
        children.add(const SizedBox(height: 14));
      }
      if (result.gradient.isNotEmpty) {
        children.add(SectionLabel(l10n.t('Gradient', 'گرادیان')));
        children.add(const SizedBox(height: 10));
        children.add(
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              for (var i = 0; i < result.gradient.length; i++)
                TypeBadge(
                  '∂${i + 1} = ${result.gradient[i].pretty}',
                  icon: Icons.north_east_rounded,
                ),
            ],
          ),
        );
        children.add(const SizedBox(height: 14));
      }
      if (result.partials.isNotEmpty) {
        children.add(
          SectionLabel(l10n.t('Partial derivatives', 'مشتق‌های جزئی')),
        );
        children.add(const SizedBox(height: 8));
        for (final p in result.partials) {
          if (p.value == null) continue;
          children.add(
            Padding(
              padding: const EdgeInsets.only(bottom: 8),
              child: Row(
                children: [
                  SizedBox(
                    width: 64,
                    child: Text(
                      p.label,
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                        fontFamily: 'DejaVu Sans Mono',
                        color: c.muted,
                      ),
                    ),
                  ),
                  Expanded(child: MathBlock(math: p.value!, size: 17)),
                ],
              ),
            ),
          );
        }
      }
      if (result.laplacian != null) {
        children.add(const SizedBox(height: 8));
        children.add(SectionLabel(l10n.t('Laplacian', 'لاپلاسیان')));
        children.add(const SizedBox(height: 8));
        children.add(MathBlock(math: result.laplacian!, size: 17));
      }
      if (result.directional != null) {
        children.add(const SizedBox(height: 14));
        children.add(
          SectionLabel(l10n.t('Directional derivative', 'مشتق جهت‌دار')),
        );
        children.add(const SizedBox(height: 8));
        children.add(
          MathBlock(math: result.directional!, accent: true, size: 19),
        );
      }
    } else if (result.mode == 'series') {
      final converges = result.seriesVerdict == 'converges';
      final inconclusive = result.seriesVerdict == 'inconclusive';
      children.add(
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            TypeBadge(
              inconclusive
                  ? l10n.t('Inconclusive', 'بی‌نتیجه')
                  : converges
                  ? l10n.t('Converges', 'همگرا')
                  : l10n.t('Diverges', 'واگرا'),
              icon: inconclusive
                  ? Icons.help_outline_rounded
                  : converges
                  ? Icons.check_circle_outline_rounded
                  : Icons.cancel_outlined,
              tone: inconclusive
                  ? c.warning
                  : converges
                  ? c.success
                  : Theme.of(context).colorScheme.error,
            ),
            if (result.seriesTest.isNotEmpty)
              TypeBadge(
                _seriesTestLabel(result.seriesTest),
                icon: Icons.rule_rounded,
              ),
          ],
        ),
      );
      children.add(const SizedBox(height: 14));
      if (result.seriesSum != null) {
        children.add(SectionLabel(l10n.t('Exact sum', 'مجموع دقیق')));
        children.add(const SizedBox(height: 10));
        children.add(
          MathBlock(
            math: result.seriesSum!,
            accent: true,
            size: 21,
            tooltip: l10n.t('Copy', 'کپی'),
            onCopy: () => _copyMath(context, result.seriesSum),
          ),
        );
      } else {
        children.add(
          SectionLabel(l10n.t('Numeric partial sum', 'مجموع جزئی عددی')),
        );
        children.add(const SizedBox(height: 8));
        children.add(
          Text(
            _formatNumber(result.partialSum, decimals: _decimals(context)),
            style: Theme.of(context).textTheme.titleMedium
                ?.copyWith(fontFamily: 'DejaVu Sans Mono'),
          ),
        );
      }
    } else if (result.mode == 'topic') {
      // Curriculum topics already contain the finished answer.  They are not
      // ODE families, so never append the implicit `= C` suffix here.
      final primary = result.primary;
      children.add(
        SectionLabel(
          l10n.t('Simplest answer', 'ساده‌ترین جواب'),
          icon: Icons.bolt_rounded,
        ),
      );
      if (result.typeEn.isNotEmpty && result.typeEn != 'topic') {
        children.add(const SizedBox(height: 8));
        children.add(
          TypeBadge(
            l10n.t(result.typeEn, result.typeFa),
            icon: Icons.sell_outlined,
          ),
        );
      }
      children.add(const SizedBox(height: 10));
      if (primary != null) {
        children.add(
          MathBlock(
            math: primary,
            accent: true,
            size: 22,
            tooltip: l10n.t('Copy', 'کپی'),
            onCopy: () => _copyMath(context, primary),
          ),
        );
      }
      final numericText = _formatNumber(
        result.numericValue,
        decimals: _decimals(context),
      );
      final answerIsNumeric =
          primary != null &&
          double.tryParse(primary.plain.replaceAll('−', '-')) != null;
      if (result.hasNumeric && result.point.isNotEmpty && !answerIsNumeric) {
        children.add(const SizedBox(height: 12));
        children.add(
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 11),
            decoration: BoxDecoration(
              color: c.cardAlt,
              borderRadius: BorderRadius.circular(12),
              border: Border.all(color: c.line),
            ),
            child: Row(
              children: [
                Icon(Icons.pin_rounded, size: 17, color: c.accent),
                const SizedBox(width: 9),
                Expanded(
                  child: Text(
                    '${l10n.t('At', 'در')} ${result.point}: $numericText',
                    style: Theme.of(context).textTheme.bodyMedium,
                  ),
                ),
              ],
            ),
          ),
        );
      }
    } else {
      final primary = result.primary;
      if (primary != null) {
        children.add(
          SectionLabel(
            result.isExplicit
                ? l10n.t('Explicit solution', 'جواب صریح')
                : l10n.t('General solution', 'جواب عمومی'),
          ),
        );
        children.add(const SizedBox(height: 10));
        final expression = result.isExplicit
            ? 'y = ${primary.pretty}'
            : '${primary.pretty} = C';
        final plain = result.isExplicit
            ? 'y = ${primary.plain}'
            : '${primary.plain} = C';
        final latex = result.isExplicit
            ? 'y = ${primary.latex}'
            : '${primary.latex} = C';
        children.add(
          MathBlock(
            math: ExView(plain: plain, pretty: expression, latex: latex),
            accent: true,
            size: 22,
            tooltip: l10n.t('Copy', 'کپی'),
            onCopy: () => _copyMath(
              context,
              ExView(plain: plain, pretty: expression, latex: latex),
            ),
          ),
        );
      }
      if (result.parametricAvailable) {
        children.add(const SizedBox(height: 14));
        children.add(
          SectionLabel(l10n.t('Parametric solution', 'جواب پارامتری')),
        );
        if (result.paramX != null) {
          children.add(const SizedBox(height: 8));
          children.add(
            _labelledMath('x(${result.paramName})', result.paramX!, context),
          );
        }
        if (result.paramY != null) {
          children.add(const SizedBox(height: 8));
          children.add(
            _labelledMath('y(${result.paramName})', result.paramY!, context),
          );
        }
      }
      if (result.singular.isNotEmpty) {
        children.add(const SizedBox(height: 14));
        children.add(
          SectionLabel(l10n.t('Singular solutions', 'جواب‌های منفرد')),
        );
        for (final s in result.singular) {
          children.add(
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: MathBlock(math: s, size: 16),
            ),
          );
        }
      }
    }

    return SoftCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: children,
      ),
    );
  }

  /// Behaviour analysis: the two derivatives plus the point tables.
  List<Widget> _behaviourAnswer(BuildContext context) {
    final out = <Widget>[];
    if (result.derivative != null) {
      out.add(SectionLabel(l10n.t("First derivative f'", 'مشتق اول f′')));
      out.add(const SizedBox(height: 10));
      out.add(
        MathBlock(
          math: result.derivative!,
          accent: true,
          size: 20,
          tooltip: l10n.t('Copy', 'کپی'),
          onCopy: () => _copyMath(context, result.derivative),
        ),
      );
      out.add(const SizedBox(height: 14));
    }
    if (result.secondDerivative != null) {
      out.add(SectionLabel(l10n.t("Second derivative f''", 'مشتق دوم f″')));
      out.add(const SizedBox(height: 10));
      out.add(
        MathBlock(
          math: result.secondDerivative!,
          size: 18,
          tooltip: l10n.t('Copy', 'کپی'),
          onCopy: () => _copyMath(context, result.secondDerivative),
        ),
      );
      out.add(const SizedBox(height: 14));
    }
    if (result.critical.isNotEmpty) {
      out.add(SectionLabel(l10n.t('Critical points', 'نقطه‌های بحرانی')));
      out.add(const SizedBox(height: 10));
      out.add(
        PointTable(
          points: result.critical,
          l10n: l10n,
          kindLabels: {
            'maximum': l10n.t('maximum', 'بیشینه'),
            'minimum': l10n.t('minimum', 'کمینه'),
            'inflection': l10n.t('flat point', 'نقطهٔ تخت'),
            'undecided': l10n.t('undecided', 'نامشخص'),
          },
        ),
      );
      out.add(const SizedBox(height: 14));
    }
    if (result.inflections.isNotEmpty) {
      out.add(SectionLabel(l10n.t('Inflection points', 'نقطه‌های عطف')));
      out.add(const SizedBox(height: 10));
      out.add(
        PointTable(
          points: result.inflections,
          l10n: l10n,
          kindLabels: {'inflection': l10n.t('inflection', 'عطف')},
        ),
      );
      out.add(const SizedBox(height: 14));
    }
    if (result.pieces.isNotEmpty) {
      out.add(SectionLabel(l10n.t('Monotonicity', 'یکنوایی')));
      out.add(const SizedBox(height: 10));
      out.add(MonotoneStrip(pieces: result.pieces, l10n: l10n));
    }
    return out;
  }

  /// Tangent and normal: both line equations, side by side with the slopes.
  List<Widget> _tangentAnswer(BuildContext context) {
    final out = <Widget>[];
    if (result.value != null) {
      out.add(
        _valueLine(
          context,
          l10n.t('Value at the point f(a)', 'مقدار در نقطه f(a)'),
          result.value!,
        ),
      );
      out.add(const SizedBox(height: 8));
    }
    if (result.slope != null) {
      out.add(
        _valueLine(
          context,
          l10n.t("Tangent slope f'(a)", 'شیب مماس f′(a)'),
          result.slope!,
        ),
      );
      out.add(const SizedBox(height: 14));
    }
    if (result.tangent != null) {
      out.add(SectionLabel(l10n.t('Tangent line', 'خط مماس')));
      out.add(const SizedBox(height: 10));
      final formula = ExView(
        plain: 'y = ${result.tangent!.plain}',
        pretty: 'y = ${result.tangent!.pretty}',
        latex: 'y = ${result.tangent!.latex}',
      );
      out.add(
        MathBlock(
          math: formula,
          accent: true,
          size: 21,
          tooltip: l10n.t('Copy', 'کپی'),
          onCopy: () => _copyMath(context, formula),
        ),
      );
      out.add(const SizedBox(height: 14));
    }
    if (result.normal != null) {
      out.add(SectionLabel(l10n.t('Normal line', 'خط عمود')));
      out.add(const SizedBox(height: 10));
      final vertical = result.normalSlope == null;
      final latex = vertical
          ? 'x = ${result.normal!.latex}'
          : 'y = ${result.normal!.latex}';
      final pretty = vertical
          ? 'x = ${result.normal!.pretty}'
          : 'y = ${result.normal!.pretty}';
      final plain = vertical
          ? 'x = ${result.normal!.plain}'
          : 'y = ${result.normal!.plain}';
      final formula = ExView(plain: plain, pretty: pretty, latex: latex);
      out.add(
        MathBlock(
          math: formula,
          accent: true,
          size: 21,
          tooltip: l10n.t('Copy', 'کپی'),
          onCopy: () => _copyMath(context, formula),
        ),
      );
    }
    return out;
  }

  /// Optimisation: the extreme values and where they occur.
  List<Widget> _optimiseAnswer(BuildContext context) {
    final out = <Widget>[];
    out.add(SectionLabel(l10n.t('Extreme values', 'مقادیر اکسترمم')));
    out.add(const SizedBox(height: 12));
    if (result.maximum != null && result.maximumAt != null) {
      out.add(
        _labelledMath(
          l10n.t('Maximum', 'بیشینه'),
          ExView(
            plain: 'f(${result.maximumAt!.plain}) = ${result.maximum!.plain}',
            pretty:
                'f(${result.maximumAt!.pretty}) = ${result.maximum!.pretty}',
            latex: 'f(${result.maximumAt!.latex}) = ${result.maximum!.latex}',
          ),
          context,
        ),
      );
      out.add(const SizedBox(height: 8));
    }
    if (result.minimum != null && result.minimumAt != null) {
      out.add(
        _labelledMath(
          l10n.t('Minimum', 'کمینه'),
          ExView(
            plain: 'f(${result.minimumAt!.plain}) = ${result.minimum!.plain}',
            pretty:
                'f(${result.minimumAt!.pretty}) = ${result.minimum!.pretty}',
            latex: 'f(${result.minimumAt!.latex}) = ${result.minimum!.latex}',
          ),
          context,
        ),
      );
      out.add(const SizedBox(height: 8));
    }
    if (result.candidates.isNotEmpty) {
      out.add(const SizedBox(height: 8));
      out.add(
        SectionLabel(l10n.t('Candidates compared', 'کاندیدهای مقایسه‌شده')),
      );
      out.add(const SizedBox(height: 10));
      out.add(
        PointTable(
          points: result.candidates,
          l10n: l10n,
          kindLabels: {
            'stationary': l10n.t('stationary', 'سکون'),
            'endpoint': l10n.t('endpoint', 'سر بازه'),
          },
        ),
      );
    }
    return out;
  }

  /// Related rates: the isolated rate and, when known, its numeric value.
  List<Widget> _relatedRatesAnswer(BuildContext context) {
    final out = <Widget>[];
    if (result.solution != null) {
      out.add(SectionLabel(l10n.t('Isolated rate', 'نرخ جداشده')));
      out.add(const SizedBox(height: 10));
      out.add(
        MathBlock(
          math: result.solution!,
          accent: true,
          size: 21,
          tooltip: l10n.t('Copy', 'کپی'),
          onCopy: () => _copyMath(context, result.solution),
        ),
      );
      out.add(const SizedBox(height: 14));
    }
    if (result.hasNumeric) {
      out.add(
        _valueLine(
          context,
          l10n.t('At this instant', 'در این لحظه'),
          ExView(
            plain: _formatNumber(
              result.numericValue,
              decimals: _decimals(context),
            ),
            pretty: _formatNumber(
              result.numericValue,
              decimals: _decimals(context),
            ),
            latex:
                '\\approx ${_formatNumber(result.numericValue, decimals: _decimals(context)).replaceAll('−', '-')}',
          ),
        ),
      );
    }
    return out;
  }

  /// Mean value and L'Hôpital: the promised point or the limit.
  List<Widget> _theoremAnswer(BuildContext context) {
    final out = <Widget>[];
    if (result.mode == 'lhopital') {
      out.add(SectionLabel(l10n.t('Limit', 'حد')));
      out.add(const SizedBox(height: 10));
      String latex;
      String pretty;
      if (result.solution != null) {
        latex = result.solution!.latex;
        pretty = result.solution!.pretty;
      } else if (result.limitInfinite) {
        latex = result.limitSign < 0 ? '-\\infty' : '\\infty';
        pretty = result.limitSign < 0 ? '−∞' : '∞';
      } else {
        latex = '\\text{does not exist}';
        pretty = l10n.t('does not exist', 'وجود ندارد');
      }
      out.add(
        MathBlock(
          math: ExView(plain: pretty, pretty: pretty, latex: latex),
          accent: true,
          size: 22,
        ),
      );
      return out;
    }
    if (result.slope != null) {
      out.add(
        _valueLine(
          context,
          l10n.t('Secant slope', 'شیب خط قاطع'),
          result.slope!,
        ),
      );
      out.add(const SizedBox(height: 10));
    }
    if (result.points.isNotEmpty) {
      out.add(
        SectionLabel(
          l10n.t('Point guaranteed by the theorem', 'نقطهٔ تضمین‌شدهٔ قضیه'),
        ),
      );
      out.add(const SizedBox(height: 10));
      for (var i = 0; i < result.points.length; i++) {
        final point = result.points[i];
        final value = i < result.pointValues.length
            ? result.pointValues[i]
            : null;
        final pretty = value == null
            ? 'c = ${point.pretty}'
            : 'c = ${point.pretty},  f(c) = ${value.pretty}';
        final latex = value == null
            ? 'c = ${point.latex}'
            : 'c = ${point.latex}, \\quad f(c) = ${value.latex}';
        out.add(
          Padding(
            padding: EdgeInsets.only(
              bottom: i == result.points.length - 1 ? 0 : 8,
            ),
            child: MathBlock(
              math: ExView(plain: pretty, pretty: pretty, latex: latex),
              accent: true,
              size: 20,
            ),
          ),
        );
      }
    }
    return out;
  }

  Widget _valueLine(BuildContext context, String label, ExView math) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.center,
      children: [
        SizedBox(
          width: 160,
          child: Text(label, style: Theme.of(context).textTheme.bodySmall),
        ),
        Expanded(child: MathInline(math: math, size: 16)),
      ],
    );
  }

  Widget _labelledMath(String label, ExView math, BuildContext context) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        SizedBox(
          width: 74,
          child: Padding(
            padding: const EdgeInsets.only(top: 12),
            child: Text(label, style: Theme.of(context).textTheme.bodySmall),
          ),
        ),
        Expanded(child: MathBlock(math: math, size: 18)),
      ],
    );
  }

  Widget _independenceBanner(BuildContext context) {
    final c = context.colors;
    final ok = result.independent;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      decoration: BoxDecoration(
        color: ok ? c.accentSoft : c.warningSoft,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(
          color: (ok ? c.accent : c.warning).withValues(alpha: 0.25),
        ),
      ),
      child: Row(
        children: [
          Icon(
            ok ? Icons.check_circle_rounded : Icons.info_rounded,
            size: 19,
            color: ok ? c.success : c.warning,
          ),
          const SizedBox(width: 9),
          Expanded(
            child: Text(
              ok
                  ? l10n.t(
                      'The functions are linearly independent on the interval.',
                      'توابع روی بازهٔ تعریف، مستقل خطی هستند.',
                    )
                  : l10n.t(
                      'The Wronskian is zero; the functions are linearly dependent (or have a degenerate point).',
                      'دترمینان رونسکی صفر است؛ توابع وابستهٔ خطی‌اند (یا نقطهٔ تبهگن دارند).',
                    ),
              style: Theme.of(context).textTheme.bodyMedium,
            ),
          ),
        ],
      ),
    );
  }

  Widget _stepsCard(BuildContext context) {
    return SoftCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SectionLabel(
            l10n.t('Solution path', 'مسیر حل'),
            icon: Icons.route_outlined,
          ),
          const SizedBox(height: 16),
          for (var i = 0; i < result.steps.length; i++)
            _StepRow(
              index: i + 1,
              step: result.steps[i],
              last: i == result.steps.length - 1,
              l10n: l10n,
              onCopy: (m) => _copyMath(context, m),
            ),
        ],
      ),
    );
  }

  Widget _checksCard(BuildContext context) {
    final c = context.colors;
    return SoftCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SectionLabel(
            l10n.t('Validation', 'اعتبارسنجی'),
            icon: Icons.verified_outlined,
          ),
          if (result.checks.isNotEmpty) const SizedBox(height: 12),
          for (final check in result.checks)
            Padding(
              padding: const EdgeInsets.only(bottom: 8),
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Icon(
                    check.ok
                        ? Icons.verified_rounded
                        : Icons.error_outline_rounded,
                    size: 19,
                    color: check.ok ? c.success : c.warning,
                  ),
                  const SizedBox(width: 9),
                  Expanded(
                    child: Text(
                      '${check.name}: ${check.detail}',
                      style: Theme.of(context).textTheme.bodyMedium,
                    ),
                  ),
                ],
              ),
            ),
          if (result.warnings.isNotEmpty) ...[
            const SizedBox(height: 8),
            for (final w in result.warnings)
              _NoteRow(
                icon: Icons.info_outline_rounded,
                text: w,
                color: c.warning,
              ),
          ],
          if (result.assumptions.isNotEmpty) ...[
            const SizedBox(height: 8),
            for (final a in result.assumptions)
              _NoteRow(icon: Icons.rule_rounded, text: a, color: c.muted),
          ],
        ],
      ),
    );
  }
}

String _methodName(String id, L10n l10n) {
  final en = <String, String>{
    'substitution': 'Substitution',
    'parts': 'Integration by parts',
    'rational': 'Rational functions',
    'trig': 'Trigonometric powers',
    'expand': 'Polynomial expansion',
    'sum': 'Term-by-term',
    'constant': 'Constant',
    'table': 'Standard table',
  };
  final fa = <String, String>{
    'substitution': 'تغییر متغیر',
    'parts': 'جزء‌به‌جزء',
    'rational': 'توابع گویا',
    'trig': 'توان‌های مثلثاتی',
    'expand': 'بسط چندجمله‌ای',
    'sum': 'جمله‌به‌جمله',
    'constant': 'تابع ثابت',
    'table': 'جدول استاندارد',
  };
  final map = l10n.isFa ? fa : en;
  return map[id] ?? id;
}

class _StepRow extends StatelessWidget {
  const _StepRow({
    required this.index,
    required this.step,
    required this.last,
    required this.l10n,
    this.onCopy,
  });

  final int index;
  final StepView step;
  final bool last;
  final L10n l10n;
  final ValueChanged<ExView?>? onCopy;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Column(
          children: [
            Container(
              width: 27,
              height: 27,
              alignment: Alignment.center,
              decoration: BoxDecoration(
                color: c.accentSoft,
                shape: BoxShape.circle,
                border: Border.all(color: c.accent.withValues(alpha: 0.35)),
              ),
              child: Text(
                '$index',
                style: TextStyle(
                  fontFamily: 'Vazirmatn',
                  fontSize: 12,
                  fontWeight: FontWeight.w700,
                  color: c.accent,
                ),
              ),
            ),
            if (!last)
              Container(
                width: 1.4,
                height: 22,
                margin: const EdgeInsets.symmetric(vertical: 4),
                color: c.line,
              ),
          ],
        ),
        const SizedBox(width: 12),
        Expanded(
          child: Padding(
            padding: EdgeInsets.only(bottom: last ? 0 : 18),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  step.title,
                  style: Theme.of(context).textTheme.titleMedium
                      ?.copyWith(fontSize: 14, fontWeight: FontWeight.w700),
                ),
                if (step.math != null) ...[
                  const SizedBox(height: 8),
                  MathBlock(
                    math: step.math!,
                    size: 16,
                    tooltip: l10n.t('Copy', 'کپی'),
                    onCopy: onCopy == null ? null : () => onCopy!(step.math),
                  ),
                ],
                if (step.note.isNotEmpty) ...[
                  const SizedBox(height: 7),
                  Text(step.note, style: Theme.of(context).textTheme.bodySmall),
                ],
              ],
            ),
          ),
        ),
      ],
    );
  }
}

class _NoteRow extends StatelessWidget {
  const _NoteRow({required this.icon, required this.text, required this.color});

  final IconData icon;
  final String text;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 7),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(icon, size: 17, color: color),
          const SizedBox(width: 8),
          Expanded(
            child: Text(text, style: Theme.of(context).textTheme.bodySmall),
          ),
        ],
      ),
    );
  }
}

class ExampleChip extends StatelessWidget {
  const ExampleChip({super.key, required this.label, required this.onTap});

  final String label;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return ActionChip(
      label: Text(label),
      onPressed: onTap,
      side: BorderSide(color: c.line),
      backgroundColor: c.cardAlt,
      padding: const EdgeInsets.symmetric(horizontal: 4),
    );
  }
}

/// Scrolls the nearest scrollable so the widget identified by [key] starts at
/// the top of the viewport. Unlike [Scrollable.ensureVisible], this works even
/// when the target is taller than the viewport (the result column usually is).
void revealWidget(GlobalKey key) {
  WidgetsBinding.instance.addPostFrameCallback((_) {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        final ctx = key.currentContext;
        if (ctx == null) return;
        final box = ctx.findRenderObject();
        if (box is! RenderBox) return;
        final viewport = RenderAbstractViewport.of(box);
        final offset = viewport.getOffsetToReveal(box, 0.0).offset;
        final scrollable = Scrollable.maybeOf(ctx);
        if (scrollable == null) return;
        final position = scrollable.position;
        position.animateTo(
          offset.clamp(position.minScrollExtent, position.maxScrollExtent),
          duration: const Duration(milliseconds: 340),
          curve: Curves.easeOutCubic,
        );
      });
    });
  });
}
