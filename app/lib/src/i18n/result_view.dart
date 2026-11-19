import 'package:flutter/material.dart';

import '../l10n.dart';
import '../models.dart';
import '../widgets.dart';
import 'result.dart';
import 'sentence.dart';

typedef LocalizedMathBuilder = Widget Function(ExView math);

/// One generic renderer for every section's [LocalizedResult].
///
/// All text comes from translation keys; math is rendered by [mathBuilder] in
/// its own left-to-right widget. Sections only need to produce the data model.
class LocalizedResultView extends StatelessWidget {
  const LocalizedResultView({
    super.key,
    required this.l10n,
    required this.result,
    this.mathBuilder,
    this.showSteps = true,
    this.showChecks = true,
  });

  final L10n l10n;
  final LocalizedResult result;
  final LocalizedMathBuilder? mathBuilder;
  final bool showSteps;
  final bool showChecks;

  Widget _inlineMath(ExView math) =>
      mathBuilder?.call(math) ?? MathInline(math: math);

  Widget _blockMath(ExView math) =>
      mathBuilder?.call(math) ?? MathBlock(math: math);

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final direction = l10n.direction;

    if (result.error != null) {
      final scheme = theme.colorScheme;
      return Directionality(
        textDirection: direction,
        child: Container(
          width: double.infinity,
          padding: const EdgeInsets.all(14),
          decoration: BoxDecoration(
            color: scheme.errorContainer.withValues(alpha: 0.45),
            borderRadius: BorderRadius.circular(12),
            border: Border.all(color: scheme.error.withValues(alpha: 0.35)),
          ),
          child: LocalizedSentence(
            l10n: l10n,
            messageKey: result.error!.key,
            params: result.error!.params,
            style: theme.textTheme.bodyMedium?.copyWith(
              color: scheme.onErrorContainer,
            ),
          ),
        ),
      );
    }

    final children = <Widget>[];
    if (result.answerMath != null) {
      children.add(_blockMath(result.answerMath!));
    }
    if (showSteps) {
      for (final step in result.steps) {
        children.add(_step(context, step));
      }
    }
    if (showChecks) {
      for (final validation in result.validations) {
        children.add(_validation(context, validation));
      }
      for (final notice in result.notices) {
        children.add(_notice(context, notice));
      }
    }

    return Directionality(
      textDirection: direction,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: children,
      ),
    );
  }

  Widget _step(BuildContext context, LocalizedStep step) {
    final theme = Theme.of(context);
    final rows = <Widget>[
      LocalizedSentence(
        l10n: l10n,
        messageKey: step.key,
        params: step.params,
        math: step.math == null ? const [] : [_inlineMath(step.math!)],
        style: theme.textTheme.bodyMedium,
      ),
    ];
    if (step.noteKey != null) {
      rows.add(const SizedBox(height: 4));
      rows.add(
        LocalizedSentence(
          l10n: l10n,
          messageKey: step.noteKey!,
          params: step.params,
          style: theme.textTheme.bodySmall?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
      );
    }
    return Padding(
      padding: const EdgeInsets.only(bottom: 10),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: rows,
      ),
    );
  }

  Widget _validation(BuildContext context, LocalizedValidation validation) {
    final scheme = Theme.of(context).colorScheme;
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: Row(
        children: [
          Icon(
            validation.ok ? Icons.check_circle_outline : Icons.error_outline,
            size: 18,
            color: validation.ok ? scheme.primary : scheme.error,
          ),
          const SizedBox(width: 8),
          Expanded(
            child: LocalizedSentence(
              l10n: l10n,
              messageKey: validation.key,
              params: validation.params,
            ),
          ),
        ],
      ),
    );
  }

  Widget _notice(BuildContext context, LocalizedNotice notice) {
    final scheme = Theme.of(context).colorScheme;
    final color = switch (notice.level) {
      NoticeLevel.error => scheme.error,
      NoticeLevel.warning => scheme.tertiary,
      NoticeLevel.info => scheme.onSurfaceVariant,
    };
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: LocalizedSentence(
        l10n: l10n,
        messageKey: notice.key,
        params: notice.params,
        style: Theme.of(context).textTheme.bodySmall?.copyWith(color: color),
      ),
    );
  }
}
