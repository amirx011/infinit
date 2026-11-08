import 'package:flutter/material.dart';

import '../l10n.dart';
import '../native_engine.dart';
import '../topic_catalog.dart';
import 'topic_workbench_page.dart';

/// Linear algebra workbench: a 2×2 system is the smallest object where trace,
/// determinant, eigenvalues and eigenvectors all matter at once.
class LinearAlgebraPage extends StatelessWidget {
  const LinearAlgebraPage({
    super.key,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final NativeEngine? engine;
  final String? engineError;
  final L10n l10n;

  static const _spec = TopicSpec(
    id: 'linear_system',
    titleEn: 'Linear algebra',
    titleFa: 'جبر خطی',
    icon: Icons.view_in_ar_rounded,
    blurbEn:
        'Read a 2×2 system through its matrix: trace, determinant and eigenvalues decide whether the origin is a node, saddle, spiral or centre.',
    blurbFa:
        'یک دستگاه ۲×۲ را از ماتریسش بخوانید: رد، دترمینان و مقدارهای ویژه تعیین می‌کنند مبدأ گره، زین، مارپیچ یا مرکز باشد.',
    fields: [
      TopicField('a', 'a (row 1, col 1)', 'a (سطر ۱، ستون ۱)', initial: '0', numeric: true),
      TopicField('b', 'b (row 1, col 2)', 'b (سطر ۱، ستون ۲)', initial: '1', numeric: true),
      TopicField('c', 'c (row 2, col 1)', 'c (سطر ۲، ستون ۱)', initial: '-1', numeric: true),
      TopicField('d', 'd (row 2, col 2)', 'd (سطر ۲، ستون ۲)', initial: '0', numeric: true),
    ],
    examples: [
      "x' = y, y' = −x  →  centre",
      "x' = x, y' = −y  →  saddle",
      "x' = −x − y, y' = x − y  →  stable spiral",
    ],
  );

  @override
  Widget build(BuildContext context) {
    return TopicWorkbenchPage(
      spec: _spec,
      engine: engine,
      l10n: l10n,
      engineError: engineError,
    );
  }
}
