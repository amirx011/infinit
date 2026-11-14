// Educational explanations.
//
// The section is split into four articles: derivative, integral, types of
// differential equations and applications of differential equations.  Each
// page is rendered by the shared StudyArticlePage so the reading order is the
// same everywhere: short answer, long explanation, ideas, worked examples,
// steps, pitfalls and links to the matching workbench.
import 'package:flutter/material.dart';

import '../l10n.dart';
import '../study_articles.dart';
import 'study_article_page.dart';

class EducationalPage extends StatelessWidget {
  const EducationalPage({
    super.key,
    required this.l10n,
    this.articleId = 'derivative',
    this.onOpen,
  });

  final L10n l10n;
  final String articleId;

  /// Opens a workbench page from the article footer.
  final void Function(int page, String? seed)? onOpen;

  @override
  Widget build(BuildContext context) {
    final article = kStudyArticles.firstWhere(
      (a) => a.id == articleId,
      orElse: () => kStudyArticles.first,
    );
    return StudyArticlePage(
      key: ValueKey('study-${article.id}'),
      article: article,
      l10n: l10n,
      onOpen: onOpen,
    );
  }
}
