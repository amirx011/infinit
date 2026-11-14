// Study article renderer.
//
// The layout is deliberately fixed: short answer first, then the long
// explanation, then the ideas, then one or more worked examples whose answer
// is shown before the numbered steps.  Every article uses the same shape so
// the four educational sections feel like one book rather than four unrelated
// pages.  Prose is rendered with [MathText], so `$...$` inside a sentence is
// typeset as real mathematics instead of staying linear text.
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
    this.onOpen,
  });

  final StudyArticle article;
  final L10n l10n;

  /// Opens a workbench page.  `seed` pre-fills the destination when supported.
  final void Function(int page, String? seed)? onOpen;

  @override
  Widget build(BuildContext context) {
    final l = l10n;
    final c = context.colors;
    final ideas = article.keyIdeas(l);
    final theory = article.theory(l);
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
                title: article.title(l),
                subtitle: article.subtitle(l),
              ),
              const SizedBox(height: 20),
              SectionLabel(l.t('ui.short_answer'), icon: Icons.bolt_rounded),
              const SizedBox(height: 10),
              MathText(
                article.shortAnswer(l),
                style: Theme.of(context).textTheme.bodyMedium
                    ?.copyWith(height: 1.8),
              ),
              const SizedBox(height: 14),
              MathBlock(math: article.shortMath, accent: true),
            ],
          ),
        ),
        if (theory.isNotEmpty) ...[
          const SizedBox(height: 16),
          SoftCard(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                SectionLabel(
                  l.t('ui.the_big_picture'),
                  icon: Icons.account_tree_outlined,
                ),
                const SizedBox(height: 12),
                for (var i = 0; i < theory.length; i++)
                  Padding(
                    padding: EdgeInsets.only(
                      bottom: i == theory.length - 1 ? 0 : 14,
                    ),
                    child: MathText(
                      theory[i],
                      style: Theme.of(context).textTheme.bodyMedium
                          ?.copyWith(height: 1.85),
                    ),
                  ),
              ],
            ),
          ),
        ],
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
              for (var i = 0; i < ideas.length; i++)
                Padding(
                  padding: const EdgeInsets.only(bottom: 10),
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Container(
                        margin: const EdgeInsets.only(top: 7),
                        width: 7,
                        height: 7,
                        decoration: BoxDecoration(
                          color: c.accent,
                          shape: BoxShape.circle,
                        ),
                      ),
                      const SizedBox(width: 11),
                      Expanded(
                        child: MathText(
                          ideas[i],
                          style: Theme.of(context).textTheme.bodyMedium
                              ?.copyWith(height: 1.7),
                        ),
                      ),
                    ],
                  ),
                ),
            ],
          ),
        ),
        for (var i = 0; i < article.examples.length; i++) ...[
          const SizedBox(height: 16),
          _ExampleCard(
            number: i + 1,
            example: article.examples[i],
            l10n: l,
            total: article.examples.length,
          ),
        ],
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
              MathText(
                article.pitfalls(l),
                style: Theme.of(context).textTheme.bodyMedium
                    ?.copyWith(height: 1.75),
              ),
            ],
          ),
        ),
        if (article.links.isNotEmpty && onOpen != null) ...[
          const SizedBox(height: 16),
          SoftCard(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                SectionLabel(
                  l.t('ui.practice_in_the_app'),
                  icon: Icons.open_in_new_rounded,
                ),
                const SizedBox(height: 12),
                Wrap(
                  spacing: 10,
                  runSpacing: 10,
                  children: [
                    for (final link in article.links)
                      FilledButton.tonalIcon(
                        onPressed: () => onOpen!(link.page, link.seed),
                        icon: const Icon(Icons.play_arrow_rounded, size: 18),
                        label: Text(
                          l.t('ui.open_tool', {'tool': l.t(link.labelKey)}),
                        ),
                      ),
                  ],
                ),
              ],
            ),
          ),
        ],
      ],
    );
  }
}

class _ExampleCard extends StatelessWidget {
  const _ExampleCard({
    required this.number,
    required this.example,
    required this.l10n,
    required this.total,
  });

  final int number;
  final StudyExample example;
  final L10n l10n;
  final int total;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return SoftCard(
      color: c.cardAlt,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SectionLabel(
            total > 1
                ? l10n.t('ui.worked_example_n', {'n': number})
                : l10n.t('ui.worked_example_2'),
            icon: Icons.edit_note_rounded,
          ),
          const SizedBox(height: 12),
          MathText(
            example.problem(l10n),
            style: Theme.of(context).textTheme.bodyMedium
                ?.copyWith(height: 1.75, fontWeight: FontWeight.w600),
          ),
          const SizedBox(height: 16),
          SectionLabel(
            l10n.t('ui.simplest_answer'),
            icon: Icons.check_circle_outline_rounded,
          ),
          const SizedBox(height: 8),
          MathText(
            example.answer(l10n),
            style: Theme.of(context).textTheme.bodyMedium
                ?.copyWith(height: 1.75),
          ),
          const SizedBox(height: 12),
          MathBlock(math: example.answerMath, accent: true),
          const SizedBox(height: 22),
          SectionLabel(
            l10n.t('ui.step_by_step'),
            icon: Icons.format_list_numbered_rounded,
          ),
          const SizedBox(height: 12),
          for (var i = 0; i < example.steps.length; i++)
            _StepBlock(
              number: i + 1,
              step: example.steps[i],
              l10n: l10n,
              isLast: i == example.steps.length - 1,
            ),
        ],
      ),
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
                MathText(
                  step.title(l10n),
                  style: Theme.of(context).textTheme.bodyMedium
                      ?.copyWith(fontWeight: FontWeight.w700),
                ),
                const SizedBox(height: 5),
                MathText(
                  step.detail(l10n),
                  style: Theme.of(context).textTheme.bodySmall
                      ?.copyWith(height: 1.7),
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
