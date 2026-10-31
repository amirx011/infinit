import 'package:flutter/material.dart';

import '../l10n.dart';
import '../theme.dart';
import '../widgets.dart';

/// Linear algebra is reserved for a later release: the drawer entry exists so
/// the module map is complete, but there is nothing to compute yet.
class LinearAlgebraPage extends StatelessWidget {
  const LinearAlgebraPage({super.key});

  @override
  Widget build(BuildContext context) {
    final l = L10n(Localizations.localeOf(context).languageCode == 'fa');
    final c = context.colors;
    return SoftCard(
      padding: const EdgeInsets.fromLTRB(22, 24, 22, 26),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          PageTitle(
            icon: Icons.view_in_ar_rounded,
            title: l.t('Linear algebra', 'جبر خطی'),
            subtitle: l.t(
              'Matrices, determinants, eigenvalues and vector spaces.',
              'ماتریس‌ها، دترمینان، مقدارهای ویژه و فضاهای برداری.',
            ),
          ),
          const SizedBox(height: 26),
          Center(
            child: Column(
              children: [
                Container(
                  width: 84,
                  height: 84,
                  decoration: BoxDecoration(
                    color: c.accentSoft,
                    borderRadius: BorderRadius.circular(24),
                    border: Border.all(color: c.accent.withValues(alpha: 0.25)),
                  ),
                  child: Center(
                    child: Text(
                      'Aᵀ',
                      textDirection: TextDirection.ltr,
                      style: TextStyle(
                        fontFamily: 'SpaceGrotesk',
                        fontSize: 30,
                        fontWeight: FontWeight.w700,
                        color: c.accent,
                      ),
                    ),
                  ),
                ),
                const SizedBox(height: 18),
                Text(
                  l.t('Coming soon', 'به‌زودی'),
                  style: Theme.of(context)
                      .textTheme
                      .titleMedium
                      ?.copyWith(fontWeight: FontWeight.w700),
                ),
                const SizedBox(height: 6),
                ConstrainedBox(
                  constraints: const BoxConstraints(maxWidth: 420),
                  child: Text(
                    l.t(
                      'This section is intentionally empty for now. Determinants, inverses and eigenvalues will live here.',
                      'این بخش فعلاً خالی است. دترمینان، ماتریس وارون و مقدارهای ویژه در همین صفحه اضافه می‌شوند.',
                    ),
                    textAlign: TextAlign.center,
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(height: 1.6),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
