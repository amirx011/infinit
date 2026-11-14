// Reader for a curriculum slot.
//
// Each article carries a short answer, the standard form, the algorithm, one
// worked example, a real computation and the common pitfall.  The computation
// either sends the example to the shared solver or runs the small tool the
// section is about; it is never a mock.
import 'package:flutter/material.dart';

import '../curriculum_articles.dart';
import '../l10n.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../tool_links.dart';
import '../widgets.dart';
import 'curriculum_tools.dart';

class CurriculumArticlePage extends StatelessWidget {
  const CurriculumArticlePage({
    super.key,
    required this.article,
    required this.l10n,
    required this.engine,
    required this.onSolve,
    this.onOpen,
  });

  final CurriculumArticle article;
  final L10n l10n;
  final NativeEngine? engine;
  final ValueChanged<String> onSolve;

  /// Opens a workbench page from the footer of the article.
  final void Function(int page, String? seed)? onOpen;

  @override
  Widget build(BuildContext context) {
    final l = l10n;
    final c = context.colors;
    final steps = article
        .algorithm(l)
        .split('\n')
        .map((line) => line.trim())
        .where((line) => line.isNotEmpty)
        .toList();
    final theory = article.theory(l);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: PageTitle(
            tone: c.accent,
            icon: Icons.menu_book_rounded,
            title: article.title(l),
            subtitle: article.subtitle(l),
          ),
        ),
        const SizedBox(height: 16),
        CurriculumComputation(
          article: article,
          l10n: l,
          engine: engine,
          onSolve: onSolve,
        ),
        const SizedBox(height: 16),
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              SectionLabel(l.t('ui.short_answer'), icon: Icons.bolt_rounded),
              const SizedBox(height: 10),
              MathText(
                article.idea(l),
                style: Theme.of(context).textTheme.bodyMedium
                    ?.copyWith(height: 1.8),
              ),
              const SizedBox(height: 16),
              SectionLabel(
                l.t('ui.standard_form'),
                icon: Icons.functions_rounded,
              ),
              const SizedBox(height: 10),
              MathBlock(math: article.standard, accent: true),
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
                  l.t('ui.why_it_works'),
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
                l.t('ui.algorithm'),
                icon: Icons.format_list_numbered_rounded,
              ),
              const SizedBox(height: 12),
              for (var i = 0; i < steps.length; i++)
                _AlgorithmStep(
                  number: i + 1,
                  text: steps[i],
                  isLast: i == steps.length - 1,
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
                l.t('ui.worked_example'),
                icon: Icons.edit_note_rounded,
              ),
              const SizedBox(height: 12),
              MathText(
                article.example(l),
                style: Theme.of(context).textTheme.bodyMedium
                    ?.copyWith(height: 1.75, fontWeight: FontWeight.w600),
              ),
              const SizedBox(height: 16),
              SectionLabel(
                l.t('ui.simplest_answer'),
                icon: Icons.check_circle_outline_rounded,
              ),
              const SizedBox(height: 8),
              MathText(
                article.answer(l),
                style: Theme.of(context).textTheme.bodyMedium
                    ?.copyWith(height: 1.8),
              ),
              const SizedBox(height: 12),
              MathBlock(math: article.exampleAnswerMath, accent: true),
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
              MathText(
                article.pitfall(l),
                style: Theme.of(context).textTheme.bodyMedium
                    ?.copyWith(height: 1.8),
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

class _AlgorithmStep extends StatelessWidget {
  const _AlgorithmStep({
    required this.number,
    required this.text,
    required this.isLast,
  });

  final int number;
  final String text;
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
            padding: EdgeInsets.only(bottom: isLast ? 0 : 14),
            child: MathText(
              text,
              style: Theme.of(context).textTheme.bodyMedium
                  ?.copyWith(height: 1.7),
            ),
          ),
        ),
      ],
    );
  }
}
