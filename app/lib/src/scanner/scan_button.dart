// The scan affordance every expression field shares. It opens the source
// sheet, and writes the confirmed reading straight into the field.
import 'package:flutter/material.dart';

import '../l10n.dart';
import 'scan_flow.dart';

class ScanFieldButton extends StatelessWidget {
  const ScanFieldButton({
    super.key,
    required this.controller,
    this.onFilled,
    this.iconSize = 20,
  });

  final TextEditingController controller;

  /// Called after the text is written, so a page can refresh its preview.
  final void Function(ScanOutcome outcome)? onFilled;
  final double iconSize;

  @override
  Widget build(BuildContext context) {
    final l10n = L10n(Localizations.localeOf(context));
    return IconButton(
      tooltip: l10n.t('scan.button.tooltip'),
      iconSize: iconSize,
      icon: const Icon(Icons.document_scanner_outlined),
      onPressed: () async {
        final outcome = await scanExpression(context);
        if (outcome == null || !context.mounted) return;
        controller.text = outcome.text;
        controller.selection = TextSelection.collapsed(
          offset: controller.text.length,
        );
        onFilled?.call(outcome);
      },
    );
  }
}
