// The camera's confirmation card.
//
// Recognition never writes into a field on its own: the shutter produces a
// reading, this sheet shows it and asks. "Insert" is the only path that hands
// the text back, so a wrong reading costs one tap instead of a silently broken
// expression.
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_math_fork/flutter_math.dart';

import '../l10n.dart';
import '../scanner_engine.dart';
import '../theme.dart';

enum ScanConfirmAction { insert, review, again }

/// A reading the app is willing to offer under a plain "Insert" button. A weak
/// score, a rejected parse or an unresolved "?" makes the card lead with
/// Review instead; the user can still insert, just not by accident.
bool isSureReading(ScanResult result) {
  final text = result.expression.trim();
  if (result.status != 'ok' || text.isEmpty) return false;
  if (result.parseValid == false) return false;
  if (result.confidence < 0.55) return false;
  if (text.contains('?')) return false;
  return true;
}

String readingText(ScanResult result) {
  if (result.kind == 'matrix' && result.matrix.isNotEmpty) return result.matrix;
  return result.expression.trim();
}

Future<ScanConfirmAction?> showScanConfirm(
  BuildContext context, {
  required L10n l10n,
  required ScanResult result,
  required Uint8List image,
}) {
  return showModalBottomSheet<ScanConfirmAction>(
    context: context,
    showDragHandle: true,
    isScrollControlled: true,
    builder: (context) =>
        ScanConfirmSheet(l10n: l10n, result: result, image: image),
  );
}

class ScanConfirmSheet extends StatelessWidget {
  const ScanConfirmSheet({
    super.key,
    required this.l10n,
    required this.result,
    required this.image,
  });

  final L10n l10n;
  final ScanResult result;
  final Uint8List image;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final sure = isSureReading(result);
    final text = readingText(result);
    final hasReading = text.isNotEmpty;
    final theme = Theme.of(context);

    final onInsert = hasReading
        ? () => Navigator.of(context).pop(ScanConfirmAction.insert)
        : null;
    void onReview() => Navigator.of(context).pop(ScanConfirmAction.review);
    // A sure reading leads with insert. A weak one leads with review, so the
    // confident-looking button never hides an uncertain string behind it.
    final Widget insertButton = sure
        ? FilledButton.icon(
            onPressed: onInsert,
            icon: const Icon(Icons.check_rounded, size: 18),
            label: Text(l10n.t('scan.confirm.insert')),
          )
        : OutlinedButton.icon(
            onPressed: onInsert,
            icon: const Icon(Icons.check_rounded, size: 18),
            label: Text(l10n.t('scan.confirm.insert')),
          );
    final Widget reviewButton = sure
        ? OutlinedButton.icon(
            onPressed: onReview,
            icon: const Icon(Icons.tune_rounded, size: 18),
            label: Text(l10n.t('scan.confirm.review')),
          )
        : FilledButton.icon(
            onPressed: onReview,
            icon: const Icon(Icons.tune_rounded, size: 18),
            label: Text(l10n.t('scan.confirm.review')),
          );

    return SafeArea(
      child: Padding(
        padding: const EdgeInsets.fromLTRB(20, 4, 20, 20),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                Icon(
                  hasReading
                      ? Icons.document_scanner_outlined
                      : Icons.search_off,
                  color: c.accent,
                  size: 22,
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: Text(
                    l10n.t(
                      hasReading ? 'scan.confirm.title' : 'scan.confirm.failed',
                    ),
                    style: theme.textTheme.titleMedium,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 4),
            Text(
              l10n.t(
                hasReading ? 'scan.confirm.hint' : 'scan.confirm.failed_hint',
              ),
              style: theme.textTheme.bodySmall?.copyWith(color: c.muted),
            ),
            const SizedBox(height: 14),
            if (hasReading)
              Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  ClipRRect(
                    borderRadius: BorderRadius.circular(10),
                    child: Image.memory(
                      image,
                      width: 92,
                      height: 74,
                      fit: BoxFit.cover,
                      cacheWidth: 240,
                      gaplessPlayback: true,
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Container(
                      height: 74,
                      padding: const EdgeInsets.symmetric(
                        horizontal: 12,
                        vertical: 8,
                      ),
                      decoration: BoxDecoration(
                        color: c.cardAlt,
                        borderRadius: BorderRadius.circular(12),
                        border: Border.all(color: c.line),
                      ),
                      child: Center(
                        child: Directionality(
                          textDirection: TextDirection.ltr,
                          child: SingleChildScrollView(
                            scrollDirection: Axis.horizontal,
                            child: Math.tex(
                              result.latex.isEmpty ? text : result.latex,
                              mathStyle: MathStyle.text,
                              textStyle: TextStyle(fontSize: 18, color: c.text),
                              onErrorFallback: (error) => Text(
                                text,
                                style: TextStyle(
                                  fontSize: 16,
                                  color: c.text,
                                  fontFamily: 'DejaVu Sans Mono',
                                  fontFamilyFallback: const ['monospace'],
                                ),
                              ),
                            ),
                          ),
                        ),
                      ),
                    ),
                  ),
                ],
              ),
            if (!sure && hasReading) ...[
              const SizedBox(height: 12),
              Container(
                padding: const EdgeInsets.symmetric(
                  horizontal: 12,
                  vertical: 10,
                ),
                decoration: BoxDecoration(
                  color: c.warningSoft,
                  borderRadius: BorderRadius.circular(10),
                ),
                child: Row(
                  children: [
                    Icon(Icons.info_outline, size: 16, color: c.warning),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        l10n.t('scan.confirm.unsure'),
                        style: theme.textTheme.bodySmall?.copyWith(
                          color: c.warning,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ],
            if (hasReading) ...[
              const SizedBox(height: 12),
              Directionality(
                textDirection: TextDirection.ltr,
                child: SelectableText(
                  text,
                  maxLines: 3,
                  style: TextStyle(
                    fontSize: 13,
                    color: c.muted,
                    fontFamily: 'DejaVu Sans Mono',
                    fontFamilyFallback: const ['monospace'],
                  ),
                ),
              ),
            ],
            const SizedBox(height: 16),
            Wrap(
              spacing: 10,
              runSpacing: 8,
              alignment: WrapAlignment.end,
              crossAxisAlignment: WrapCrossAlignment.center,
              children: [
                TextButton(
                  onPressed: () =>
                      Navigator.of(context).pop(ScanConfirmAction.again),
                  child: Text(l10n.t('scan.confirm.again')),
                ),
                reviewButton,
                if (hasReading) insertButton,
              ],
            ),
          ],
        ),
      ),
    );
  }
}
