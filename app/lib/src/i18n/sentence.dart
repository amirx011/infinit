import 'package:flutter/material.dart';

import '../l10n.dart';

/// A translated sentence with optional math siblings.
///
/// The sentence direction comes from the selected language. Every math child is
/// wrapped in its own left-to-right [Directionality], so a formula never picks
/// up the sentence direction and its punctuation/factorials stay in place.
class LocalizedSentence extends StatelessWidget {
  const LocalizedSentence({
    super.key,
    required this.l10n,
    required this.messageKey,
    this.params = const {},
    this.math = const [],
    this.style,
    this.textAlign,
    this.spacing = 6,
  });

  final L10n l10n;
  final String messageKey;
  final Map<String, Object?> params;

  /// Formula widgets rendered next to the sentence, never inside it.
  final List<Widget> math;
  final TextStyle? style;
  final TextAlign? textAlign;
  final double spacing;

  @override
  Widget build(BuildContext context) {
    final direction = l10n.direction;
    final text = Text(
      l10n.t(messageKey, params),
      style: style,
      textAlign: textAlign,
      textDirection: direction,
    );
    if (math.isEmpty) return text;

    return Directionality(
      textDirection: direction,
      child: Wrap(
        spacing: spacing,
        runSpacing: 4,
        crossAxisAlignment: WrapCrossAlignment.center,
        children: [
          text,
          for (final widget in math)
            Directionality(textDirection: TextDirection.ltr, child: widget),
        ],
      ),
    );
  }
}
