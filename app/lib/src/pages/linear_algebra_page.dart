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
    icon: Icons.view_in_ar_rounded,
    keyPrefix: 'linear_algebra',
    fields: [
      TopicField(
        'a',
        labelKey: 'topic.linear_algebra.field.a.label',
        initial: '0',
        numeric: true,
      ),
      TopicField(
        'b',
        labelKey: 'topic.linear_algebra.field.b.label',
        initial: '1',
        numeric: true,
      ),
      TopicField(
        'c',
        labelKey: 'topic.linear_algebra.field.c.label',
        initial: '-1',
        numeric: true,
      ),
      TopicField(
        'd',
        labelKey: 'topic.linear_algebra.field.d.label',
        initial: '0',
        numeric: true,
      ),
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
