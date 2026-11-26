// The handwriting board is the recommended input path, so its controls are part
// of the contract: undo/redo, a fix-symbols action that only lights up after a
// reading, the personal-sample menu, and the board settings (stylus-only, guide
// lines) plus Fit.
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/scanner/board.dart';
import 'package:infinit/src/theme.dart';

void main() {
  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  Future<void> pumpBoard(WidgetTester tester, L10n l10n) async {
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        home: HandwritingBoardPage(l10n: l10n),
      ),
    );
    await tester.pump();
  }

  testWidgets('the board exposes its toolbar actions', (tester) async {
    final l10n = const L10n(Locale('en'));
    await pumpBoard(tester, l10n);

    expect(find.byTooltip(l10n.t('scan.board_undo')), findsOneWidget);
    expect(find.byTooltip(l10n.t('scan.board_redo')), findsOneWidget);
    expect(find.byTooltip(l10n.t('scan.board_clear')), findsOneWidget);
    expect(find.byTooltip(l10n.t('scan.board.fit')), findsOneWidget);
    expect(find.byTooltip(l10n.t('scan.board.settings')), findsOneWidget);
    expect(find.byTooltip(l10n.t('scan.samples.title')), findsOneWidget);
  });

  testWidgets('fixing a symbol needs a reading first', (tester) async {
    final l10n = const L10n(Locale('en'));
    await pumpBoard(tester, l10n);

    final button = tester.widget<IconButton>(
      find.ancestor(
        of: find.byIcon(Icons.spellcheck_rounded),
        matching: find.byType(IconButton),
      ).first,
    );
    expect(button.onPressed, isNull);
  });

  testWidgets('board settings toggle stylus-only and guide lines', (
    tester,
  ) async {
    final l10n = const L10n(Locale('en'));
    await pumpBoard(tester, l10n);

    await tester.tap(find.byTooltip(l10n.t('scan.board.settings')));
    await tester.pumpAndSettle();
    expect(find.text(l10n.t('scan.board.stylus_only')), findsOneWidget);
    expect(find.text(l10n.t('scan.board.guides')), findsOneWidget);

    await tester.tap(find.text(l10n.t('scan.board.stylus_only')));
    await tester.pumpAndSettle();

    await tester.tap(find.byTooltip(l10n.t('scan.board.settings')));
    await tester.pumpAndSettle();
    final stylus = tester.widget<CheckedPopupMenuItem<String>>(
      find.widgetWithText(
        CheckedPopupMenuItem<String>,
        l10n.t('scan.board.stylus_only'),
      ),
    );
    expect(stylus.checked, isTrue);
  });

  testWidgets('the board is localized in Persian', (tester) async {
    final l10n = const L10n(Locale('fa'));
    await pumpBoard(tester, l10n);
    expect(find.text(l10n.t('scan.board_hint')), findsOneWidget);
  });
}
