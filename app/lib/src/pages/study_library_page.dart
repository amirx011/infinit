// The explanations library.
//
// One page gathers every study article so the reader picks a topic instead of
// walking a long drawer.  Sections are the big shelves (differential
// equations, calculus, standalone topics); inside each shelf the articles are
// grouped by theme.  The drawer mirrors the same two levels.
import 'package:flutter/material.dart';

import '../l10n.dart';
import '../study_articles.dart';
import '../theme.dart';
import '../widgets.dart';

class StudyLibraryPage extends StatelessWidget {
  const StudyLibraryPage({super.key, required this.l10n, required this.onOpen});

  final L10n l10n;

  /// Opens one article by page index.
  final void Function(int page) onOpen;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final total = kStudyArticles.length;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                tone: c.accent,
                icon: Icons.local_library_rounded,
                title: l10n.t('study.library.title'),
                subtitle: l10n.t('study.library.subtitle'),
              ),
              const SizedBox(height: 18),
              Wrap(
                spacing: 10,
                runSpacing: 10,
                children: [
                  _Stat(
                    icon: Icons.menu_book_rounded,
                    label: l10n.t('study.library.count', {'count': total}),
                    tone: c.accent,
                  ),
                  _Stat(
                    icon: Icons.edit_note_rounded,
                    label: l10n.t('study.card.examples', {
                      'count': kStudyArticles.fold<int>(
                        0,
                        (sum, article) => sum + article.examples.length,
                      ),
                    }),
                    tone: c.info,
                  ),
                ],
              ),
            ],
          ),
        ),
        for (final section in kStudySections) ...[
          const SizedBox(height: 18),
          _SectionShelf(section: section, l10n: l10n, onOpen: onOpen),
        ],
      ],
    );
  }
}

class _SectionShelf extends StatelessWidget {
  const _SectionShelf({
    required this.section,
    required this.l10n,
    required this.onOpen,
  });

  final StudySectionDef section;
  final L10n l10n;
  final void Function(int page) onOpen;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return SoftCard(
      padding: const EdgeInsets.fromLTRB(20, 18, 20, 22),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Container(
                width: 36,
                height: 36,
                decoration: BoxDecoration(
                  color: c.accent.withValues(alpha: 0.12),
                  borderRadius: BorderRadius.circular(11),
                  border: Border.all(color: c.accent.withValues(alpha: 0.25)),
                ),
                child: Icon(section.icon, size: 19, color: c.accent),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Text(
                      l10n.t(section.titleKey),
                      style: Theme.of(context).textTheme.titleLarge
                          ?.copyWith(fontSize: 18),
                    ),
                    const SizedBox(height: 2),
                    Text(
                      l10n.t(section.subtitleKey),
                      style: Theme.of(context).textTheme.bodySmall
                          ?.copyWith(color: c.muted, height: 1.5),
                    ),
                  ],
                ),
              ),
            ],
          ),
          for (final group in section.groups)
            if (studyArticlesIn(group).isNotEmpty) ...[
              const SizedBox(height: 18),
              _GroupHeading(
                label: l10n.t(kStudyGroups[group]!.titleKey),
                icon: kStudyGroups[group]!.icon,
                count: studyArticlesIn(group).length,
              ),
              const SizedBox(height: 10),
              LayoutBuilder(
                builder: (context, constraints) {
                  final columns = constraints.maxWidth >= 980
                      ? 3
                      : constraints.maxWidth >= 620
                      ? 2
                      : 1;
                  const spacing = 12.0;
                  final width =
                      (constraints.maxWidth - spacing * (columns - 1)) /
                      columns;
                  return Wrap(
                    spacing: spacing,
                    runSpacing: spacing,
                    children: [
                      for (final article in studyArticlesIn(group))
                        SizedBox(
                          width: width,
                          child: _ArticleCard(
                            article: article,
                            l10n: l10n,
                            onTap: () =>
                                onOpen(kStudyArticlePages[article.id]!),
                          ),
                        ),
                    ],
                  );
                },
              ),
            ],
        ],
      ),
    );
  }
}

class _GroupHeading extends StatelessWidget {
  const _GroupHeading({
    required this.label,
    required this.icon,
    required this.count,
  });

  final String label;
  final IconData icon;
  final int count;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Row(
      children: [
        Icon(icon, size: 15, color: c.muted),
        const SizedBox(width: 7),
        Expanded(
          child: Text(
            label,
            style: Theme.of(context).textTheme.labelLarge
                ?.copyWith(color: c.muted, fontWeight: FontWeight.w600),
          ),
        ),
        Text(
          '$count',
          style: Theme.of(context).textTheme.labelMedium
              ?.copyWith(color: c.muted),
        ),
      ],
    );
  }
}

class _ArticleCard extends StatelessWidget {
  const _ArticleCard({
    required this.article,
    required this.l10n,
    required this.onTap,
  });

  final StudyArticle article;
  final L10n l10n;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final count = article.examples.length;
    return Material(
      color: c.cardAlt,
      borderRadius: BorderRadius.circular(14),
      child: InkWell(
        borderRadius: BorderRadius.circular(14),
        onTap: onTap,
        child: Container(
          padding: const EdgeInsets.all(14),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: c.line.withValues(alpha: 0.7)),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              Row(
                children: [
                  Icon(article.icon, size: 18, color: c.accent),
                  const SizedBox(width: 9),
                  Expanded(
                    child: Text(
                      article.title(l10n),
                      style: Theme.of(context).textTheme.titleSmall
                          ?.copyWith(fontWeight: FontWeight.w700),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 7),
              Text(
                article.subtitle(l10n),
                maxLines: 3,
                overflow: TextOverflow.ellipsis,
                style: Theme.of(context).textTheme.bodySmall
                    ?.copyWith(color: c.muted, height: 1.55),
              ),
              const SizedBox(height: 10),
              Row(
                children: [
                  Text(
                    l10n.t('study.card.examples', {'count': count}),
                    style: Theme.of(context).textTheme.labelSmall
                        ?.copyWith(color: c.muted),
                  ),
                  const Spacer(),
                  Icon(Icons.arrow_forward_rounded, size: 15, color: c.accent),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _Stat extends StatelessWidget {
  const _Stat({required this.icon, required this.label, required this.tone});

  final IconData icon;
  final String label;
  final Color tone;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 11, vertical: 7),
      decoration: BoxDecoration(
        color: tone.withValues(alpha: 0.1),
        borderRadius: BorderRadius.circular(9),
        border: Border.all(color: tone.withValues(alpha: 0.25)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, size: 14, color: tone),
          const SizedBox(width: 6),
          Text(
            label,
            style: Theme.of(context).textTheme.labelMedium
                ?.copyWith(color: c.text),
          ),
        ],
      ),
    );
  }
}
