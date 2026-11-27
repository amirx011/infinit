// The scan affordance every expression field shares. It opens the source
// sheet, and writes the confirmed reading straight into the field.
//
// An unlabelled icon was easy to miss between the clear and paste icons, so the
// button carries its own word and the accent colour of the active palette. It
// stays an IconButton so the input decoration keeps one semantics node.
import 'package:flutter/material.dart';

import '../l10n.dart';
import '../theme.dart';
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
    final c = context.colors;
    return IconButton(
      tooltip: l10n.t('scan.button.tooltip'),
      iconSize: iconSize,
      style: IconButton.styleFrom(
        foregroundColor: c.accent,
        backgroundColor: c.accentSoft,
        side: BorderSide(color: c.accent.withValues(alpha: 0.45)),
        padding: const EdgeInsets.symmetric(horizontal: 9),
        minimumSize: const Size(0, 34),
        tapTargetSize: MaterialTapTargetSize.shrinkWrap,
        visualDensity: VisualDensity.compact,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
      ),
      icon: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(Icons.document_scanner_outlined, size: iconSize),
          const SizedBox(width: 5),
          Text(
            l10n.t('scan.button.label'),
            maxLines: 1,
            style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600),
          ),
        ],
      ),
      onPressed: () => _scan(context),
    );
  }

  Future<void> _scan(BuildContext context) async {
    final outcome = await scanExpression(context);
    if (outcome == null || !context.mounted) return;
    controller.text = outcome.text;
    controller.selection = TextSelection.collapsed(
      offset: controller.text.length,
    );
    onFilled?.call(outcome);
  }
}
