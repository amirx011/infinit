import 'package:flutter/material.dart';

import '../l10n.dart';
import '../models.dart';
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
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          padding: const EdgeInsets.fromLTRB(22, 24, 22, 26),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                tone: c.info,
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
                        border: Border.all(
                          color: c.accent.withValues(alpha: 0.25),
                        ),
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
                      style: Theme.of(context).textTheme.titleMedium
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
                        style: Theme.of(context).textTheme.bodySmall
                            ?.copyWith(height: 1.6),
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        LearningCard(
          l10n: l,
          title: l.t(
            'What linear algebra adds',
            'جبر خطی چه چیزی اضافه می‌کند؟',
          ),
          idea: l.t(
            'Linear algebra turns equations into vectors and matrices, so a whole system can be read from its determinant and eigenvalues.',
            'جبر خطی معادله‌ها را به بردار و ماتریس تبدیل می‌کند تا یک دستگاه کامل را از دترمینان و مقدارهای ویژه‌اش بخوانیم.',
          ),
          points: [
            l.t(
              'A nonzero determinant means the matrix is invertible and the system has a unique solution.',
              'دترمینان ناصفر یعنی ماتریس وارون‌پذیر است و دستگاه جواب یکتا دارد.',
            ),
            l.t(
              'Eigenvalues describe the directions a transformation only stretches or shrinks.',
              'مقدارهای ویژه جهت‌هایی را توصیف می‌کنند که تبدیل فقط آن‌ها را کشیده یا فشرده می‌کند.',
            ),
          ],
          example: const ExView(
            plain: 'det([[2,1],[1,3]]) = 5 != 0',
            pretty: 'det [[2, 1], [1, 3]] = 5 ≠ 0',
            latex: r'\det\begin{pmatrix}2&1\\1&3\end{pmatrix} = 5 \neq 0',
          ),
          exampleCaption: l.t(
            'A nonzero determinant confirms that this 2×2 system is uniquely solvable.',
            'دترمینان ناصفر تأیید می‌کند که این دستگاه ۲×۲ جواب یکتا دارد.',
          ),
        ),
      ],
    );
  }
}
