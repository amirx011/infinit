import 'package:flutter/material.dart';

import '../l10n.dart';

/// A reserved curriculum slot. The page intentionally contains only the
/// coming-soon phrase until the topic is implemented.
class ComingSoonPage extends StatelessWidget {
  const ComingSoonPage({super.key, required this.l10n});

  final L10n l10n;

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 420,
      child: Center(
        child: Text(
          l10n.t('Coming soon', 'به‌زودی'),
          textAlign: TextAlign.center,
          style: Theme.of(context).textTheme.titleMedium
              ?.copyWith(fontWeight: FontWeight.w700),
        ),
      ),
    );
  }
}
