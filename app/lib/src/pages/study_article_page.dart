// Study article renderer.
//
// The layout is deliberately fixed: short answer first, then the ideas, then a
// worked example whose answer is shown before its numbered steps.  Every
// article uses the same shape so the four educational sections feel like one
// book rather than four unrelated pages.
import 'package:flutter/material.dart';

import '../l10n.dart';
import '../study_articles.dart';
import '../theme.dart';
import '../widgets.dart';

class StudyArticlePage extends StatelessWidget {
  const StudyArticlePage({
    super.key,
    required this.article,
    required this.l10n,
  });

  final StudyArticle article;
  final L10n l10n;

  @override
  Widget build(BuildContext context) {
    final l = l10n;
    final c = context.colors;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                tone: c.accent,
                icon: Icons.auto_stories_rounded,
                title: l.t(article.titleEn, article.titleFa),
                subtitle: l.t(article.subtitleEn, article.subtitleFa),
              ),
              const SizedBox(height: 20),
              SectionLabel(
                l.t('ui.short_answer'),
                icon: Icons.bolt_rounded,
              ),
              const SizedBox(height: 10),
              Text(
                l.t(article.shortAnswerEn, article.shortAnswerFa),
                style: Theme.of(context).textTheme.bodyMedium
                    ?.copyWith(height: 1.75),
              ),
              const SizedBox(height: 14),
              MathBlock(math: article.shortMath, accent: true),
            ],
          ),
        ),
        const SizedBox(height: 16),
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              SectionLabel(
                l.t('ui.key_ideas'),
                icon: Icons.lightbulb_outline_rounded,
              ),
              const SizedBox(height: 12),
              for (var i = 0; i < article.keyIdeasEn.length; i++)
                Padding(
                  padding: const EdgeInsets.only(bottom: 10),
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Container(
                        margin: const EdgeInsets.only(top: 6),
                        width: 7,
                        height: 7,
                        decoration: BoxDecoration(
                          color: c.accent,
                          shape: BoxShape.circle,
                        ),
                      ),
                      const SizedBox(width: 11),
                      Expanded(
                        child: Text(
                          l.t(article.keyIdeasEn[i], article.keyIdeasFa[i]),
                          style: Theme.of(context).textTheme.bodyMedium
                              ?.copyWith(height: 1.65),
                        ),
                      ),
                    ],
                  ),
                ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        SoftCard(
          color: c.cardAlt,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              SectionLabel(
                l.t('ui.worked_example_2'),
                icon: Icons.edit_note_rounded,
              ),
              const SizedBox(height: 12),
              Text(
                l.t(article.exampleProblemEn, article.exampleProblemFa),
                style: Theme.of(context).textTheme.bodyMedium
                    ?.copyWith(height: 1.7, fontWeight: FontWeight.w600),
              ),
              const SizedBox(height: 16),
              SectionLabel(
                l.t('ui.simplest_answer'),
                icon: Icons.check_circle_outline_rounded,
              ),
              const SizedBox(height: 8),
              Text(
                l.t(article.exampleAnswerEn, article.exampleAnswerFa),
                style: Theme.of(context).textTheme.bodyMedium
                    ?.copyWith(height: 1.7),
              ),
              const SizedBox(height: 12),
              MathBlock(math: article.exampleAnswerMath, accent: true),
              const SizedBox(height: 22),
              SectionLabel(
                l.t('ui.step_by_step'),
                icon: Icons.format_list_numbered_rounded,
              ),
              const SizedBox(height: 12),
              for (var i = 0; i < article.steps.length; i++)
                _StepBlock(
                  number: i + 1,
                  step: article.steps[i],
                  l10n: l,
                  isLast: i == article.steps.length - 1,
                ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        SoftCard(
          color: c.warningSoft,
          borderColor: c.warning.withValues(alpha: 0.35),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              SectionLabel(
                l.t('ui.common_pitfalls'),
                icon: Icons.warning_amber_rounded,
              ),
              const SizedBox(height: 10),
              Text(
                l.t(article.pitfallsEn, article.pitfallsFa),
                style: Theme.of(context).textTheme.bodyMedium
                    ?.copyWith(height: 1.7),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Icon(Icons.menu_book_rounded, size: 17, color: c.muted),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                l.t(article.sourceEn, article.sourceFa),
                style: Theme.of(context).textTheme.bodySmall
                    ?.copyWith(height: 1.6, color: c.muted),
              ),
            ),
          ],
        ),
      ],
    );
  }
}

class _StepBlock extends StatelessWidget {
  const _StepBlock({
    required this.number,
    required this.step,
    required this.l10n,
    required this.isLast,
  });

  final int number;
  final StudyStep step;
  final L10n l10n;
  final bool isLast;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Container(
          width: 28,
          height: 28,
          decoration: BoxDecoration(
            color: c.accentSoft,
            shape: BoxShape.circle,
            border: Border.all(color: c.accent.withValues(alpha: 0.35)),
          ),
          alignment: Alignment.center,
          child: Text(
            '$number',
            style: TextStyle(
              color: c.accent,
              fontWeight: FontWeight.w700,
              fontSize: 13,
            ),
          ),
        ),
        const SizedBox(width: 12),
        Expanded(
          child: Padding(
            padding: EdgeInsets.only(bottom: isLast ? 0 : 18),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  l10n.t(step.titleEn, step.titleFa),
                  style: Theme.of(context).textTheme.bodyMedium
                      ?.copyWith(fontWeight: FontWeight.w700),
                ),
                const SizedBox(height: 5),
                Text(
                  l10n.t(step.detailEn, step.detailFa),
                  style: Theme.of(context).textTheme.bodySmall
                      ?.copyWith(height: 1.65),
                ),
                if (step.math != null) ...[
                  const SizedBox(height: 9),
                  MathBlock(math: step.math!, size: 16),
                ],
              ],
            ),
          ),
        ),
      ],
    );
  }
}
