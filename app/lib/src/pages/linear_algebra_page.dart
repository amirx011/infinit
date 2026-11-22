import 'package:flutter/material.dart';

import '../l10n.dart';
import '../theme.dart';
import '../widgets.dart';

/// The linear-algebra section home.
///
/// The section is large enough that a single workbench is a poor landing page:
/// this map shows the six shelves, how many workbenches each holds, and the
/// article shelf, so the reader picks a chapter instead of scrolling a drawer.
class LinearAlgebraPage extends StatelessWidget {
  const LinearAlgebraPage({
    super.key,
    required this.l10n,
    required this.onOpen,
  });

  final L10n l10n;

  /// Opens one workbench or article by page index.
  final void Function(int page) onOpen;

  static const _shelves = <_Shelf>[
    _Shelf(
      icon: Icons.table_rows_rounded,
      titleKey: 'nav.group.linalg.matrices',
      entries: [
        _Entry(127, 'topic.linalg_rank.title', Icons.table_rows_rounded),
        _Entry(128, 'topic.linalg_det.title', Icons.calculate_rounded),
        _Entry(129, 'topic.linalg_inverse.title', Icons.flip_rounded),
        _Entry(130, 'topic.linalg_lu.title', Icons.call_split_rounded),
        _Entry(126, 'topic.linear_solve.title', Icons.functions_rounded),
      ],
    ),
    _Shelf(
      icon: Icons.blur_on_rounded,
      titleKey: 'nav.group.linalg.vector_spaces',
      entries: [
        _Entry(131, 'topic.linalg_span.title', Icons.blur_on_rounded),
        _Entry(132, 'topic.linalg_independence.title', Icons.rule_rounded),
        _Entry(133, 'topic.linalg_basis.title', Icons.account_tree_rounded),
        _Entry(134, 'topic.linalg_coordinates.title', Icons.my_location_rounded),
        _Entry(135, 'topic.linalg_change_basis.title', Icons.swap_horiz_rounded),
        _Entry(136, 'topic.linalg_subspace.title', Icons.checklist_rounded),
        _Entry(144, 'topic.linalg_graph.title', Icons.hub_rounded),
      ],
    ),
    _Shelf(
      icon: Icons.vertical_align_center_rounded,
      titleKey: 'nav.group.linalg.orthogonality',
      entries: [
        _Entry(
          137,
          'topic.linalg_projection.title',
          Icons.vertical_align_center_rounded,
        ),
        _Entry(
          138,
          'topic.linalg_gram_schmidt.title',
          Icons.auto_fix_high_rounded,
        ),
        _Entry(139, 'topic.linalg_qr.title', Icons.qr_code_2_rounded),
        _Entry(
          140,
          'topic.linalg_least_squares.title',
          Icons.show_chart_rounded,
        ),
        _Entry(
          143,
          'topic.linalg_orthogonal_complement.title',
          Icons.blur_circular_rounded,
        ),
        _Entry(145, 'topic.linalg_fft.title', Icons.graphic_eq_rounded),
      ],
    ),
    _Shelf(
      icon: Icons.filter_center_focus_rounded,
      titleKey: 'nav.group.linalg.eigen',
      entries: [
        _Entry(28, 'topic.matrix_eigen.title', Icons.grid_4x4_rounded),
        _Entry(
          141,
          'topic.linalg_diagonalize.title',
          Icons.filter_center_focus_rounded,
        ),
        _Entry(142, 'topic.linalg_power.title', Icons.bolt_rounded),
        _Entry(146, 'topic.linalg_similarity.title', Icons.compare_arrows_rounded),
        _Entry(147, 'topic.linalg_matrix_exp.title', Icons.timeline_rounded),
        _Entry(156, 'topic.linalg_symmetric.title', Icons.balance_rounded),
      ],
    ),
    _Shelf(
      icon: Icons.check_circle_rounded,
      titleKey: 'nav.group.linalg.positive',
      entries: [
        _Entry(
          148,
          'topic.linalg_positive_definite.title',
          Icons.check_circle_rounded,
        ),
        _Entry(149, 'topic.linalg_svd.title', Icons.auto_graph_rounded),
      ],
    ),
    _Shelf(
      icon: Icons.straighten_rounded,
      titleKey: 'nav.group.linalg.numerical',
      entries: [
        _Entry(150, 'topic.linalg_norm.title', Icons.straighten_rounded),
        _Entry(
          151,
          'topic.linalg_power_iteration.title',
          Icons.trending_up_rounded,
        ),
        _Entry(
          152,
          'topic.linalg_qr_algorithm.title',
          Icons.qr_code_2_rounded,
        ),
        _Entry(153, 'topic.linalg_iterative.title', Icons.sync_rounded),
      ],
    ),
    _Shelf(
      icon: Icons.linear_scale_rounded,
      titleKey: 'nav.group.linalg.optimization',
      entries: [
        _Entry(154, 'topic.linalg_lp.title', Icons.linear_scale_rounded),
        _Entry(155, 'topic.linalg_game.title', Icons.sports_esports_rounded),
      ],
    ),
  ];

  static const _articles = <_Entry>[
    _Entry(157, 'study.la_fields.title', Icons.tag_rounded),
    _Entry(158, 'study.la_vector_spaces.title', Icons.blur_on_rounded),
    _Entry(159, 'study.la_transformations.title', Icons.swap_horiz_rounded),
    _Entry(160, 'study.la_determinants.title', Icons.calculate_rounded),
    _Entry(161, 'study.la_canonical.title', Icons.account_tree_rounded),
    _Entry(162, 'study.la_inner_products.title', Icons.straighten_rounded),
  ];

  static const _articlesShelf = _Shelf(
    icon: Icons.auto_stories_rounded,
    titleKey: 'nav.group.linalg.articles',
    entries: _articles,
    leading: _Entry(125, 'study.library.title', Icons.local_library_rounded),
    reading: true,
  );

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final total = _shelves.fold<int>(0, (sum, s) => sum + s.entries.length);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                tone: c.accent,
                icon: Icons.grid_on_rounded,
                title: l10n.t('ui.linear_algebra'),
                subtitle: l10n.t('linalg.home.subtitle'),
              ),
              const SizedBox(height: 16),
              MathText(
                l10n.t('linalg.home.intro'),
                style: Theme.of(context).textTheme.bodyMedium
                    ?.copyWith(color: c.muted, height: 1.7),
              ),
              const SizedBox(height: 16),
              // Counts are meta: one quiet line, not a row of badges.
              Wrap(
                spacing: 16,
                runSpacing: 6,
                children: [
                  _Meta(
                    icon: Icons.grid_on_rounded,
                    label: l10n.t('linalg.home.topics', {'count': total}),
                  ),
                  _Meta(
                    icon: Icons.auto_stories_rounded,
                    label: l10n.t('linalg.home.articles', {
                      'count': _articles.length,
                    }),
                  ),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LayoutBuilder(
          builder: (context, constraints) {
            final shelves = <_Shelf>[..._shelves, _articlesShelf];
            if (constraints.maxWidth < 1040) {
              return Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  for (var i = 0; i < shelves.length; i++) ...[
                    if (i > 0) const SizedBox(height: 16),
                    _ShelfCard(
                      shelf: shelves[i],
                      l10n: l10n,
                      onOpen: onOpen,
                    ),
                  ],
                ],
              );
            }
            // Two independent columns: a shelf is a card, so letting each
            // column flow avoids the empty holes a row-aligned grid leaves
            // under the shorter shelf.
            final left = <_Shelf>[];
            final right = <_Shelf>[];
            var leftRows = 0;
            var rightRows = 0;
            for (final shelf in shelves) {
              final rows =
                  shelf.entries.length + (shelf.leading == null ? 0 : 1);
              if (leftRows <= rightRows) {
                left.add(shelf);
                leftRows += rows;
              } else {
                right.add(shelf);
                rightRows += rows;
              }
            }
            Widget column(List<_Shelf> items) => Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                for (var i = 0; i < items.length; i++) ...[
                  if (i > 0) const SizedBox(height: 16),
                  _ShelfCard(shelf: items[i], l10n: l10n, onOpen: onOpen),
                ],
              ],
            );
            return Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Expanded(child: column(left)),
                const SizedBox(width: 16),
                Expanded(child: column(right)),
              ],
            );
          },
        ),
      ],
    );
  }
}

class _Shelf {
  const _Shelf({
    required this.icon,
    required this.titleKey,
    required this.entries,
    this.leading,
    this.reading = false,
  });

  final IconData icon;
  final String titleKey;
  final List<_Entry> entries;

  /// An optional first row that is not a workbench (the library link).
  final _Entry? leading;

  /// The article shelf is a reading list, so it sits on the alternate surface.
  final bool reading;
}

class _Entry {
  const _Entry(this.page, this.titleKey, this.icon);

  final int page;
  final String titleKey;
  final IconData icon;
}

class _ShelfCard extends StatelessWidget {
  const _ShelfCard({
    required this.shelf,
    required this.l10n,
    required this.onOpen,
  });

  final _Shelf shelf;
  final L10n l10n;
  final void Function(int page) onOpen;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return SoftCard(
      padding: const EdgeInsets.fromLTRB(20, 20, 20, 20),
      color: shelf.reading ? c.cardAlt : null,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisSize: MainAxisSize.min,
        children: [
          Row(
            children: [
              Container(
                width: 32,
                height: 32,
                decoration: BoxDecoration(
                  color: c.accent.withValues(alpha: 0.1),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Icon(shelf.icon, size: 18, color: c.accent),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Text(
                  l10n.t(shelf.titleKey),
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                    fontSize: 15,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          if (shelf.leading != null) ...[
            _EntryRow(
              entry: shelf.leading!,
              l10n: l10n,
              onOpen: onOpen,
              emphasized: true,
            ),
            const SizedBox(height: 4),
          ],
          for (final entry in shelf.entries)
            _EntryRow(entry: entry, l10n: l10n, onOpen: onOpen),
        ],
      ),
    );
  }
}

class _EntryRow extends StatelessWidget {
  const _EntryRow({
    required this.entry,
    required this.l10n,
    required this.onOpen,
    this.emphasized = false,
  });

  final _Entry entry;
  final L10n l10n;
  final void Function(int page) onOpen;
  final bool emphasized;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final tone = emphasized ? c.accent : c.muted;
    return Material(
      color: Colors.transparent,
      child: InkWell(
        borderRadius: BorderRadius.circular(8),
        onTap: () => onOpen(entry.page),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 8),
          child: Row(
            children: [
              Icon(entry.icon, size: 16, color: tone),
              const SizedBox(width: 10),
              Expanded(
                child: Text(
                  l10n.t(entry.titleKey),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                    color: emphasized ? c.text : null,
                    fontWeight: emphasized ? FontWeight.w600 : null,
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _Meta extends StatelessWidget {
  const _Meta({required this.icon, required this.label});

  final IconData icon;
  final String label;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(icon, size: 14, color: c.muted),
        const SizedBox(width: 6),
        Text(
          label,
          style: Theme.of(context).textTheme.bodySmall?.copyWith(
            fontSize: 12,
            color: c.muted,
            fontFeatures: const [FontFeature.tabularFigures()],
          ),
        ),
      ],
    );
  }
}
