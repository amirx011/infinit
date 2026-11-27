import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/calc_widgets.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/theme.dart';
import 'package:infinit/src/scanner/matrix_input.dart';

void main() {
  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  group('compact matrix text', () {
    test('parses rows and cells', () {
      expect(parseMatrixCells('2, 1; 1, -1'), [
        ['2', '1'],
        ['1', '-1'],
      ]);
      expect(parseMatrixCells('1; 2; 3'), [
        ['1'],
        ['2'],
        ['3'],
      ]);
      expect(parseMatrixCells(''), [
        ['', ''],
        ['', ''],
      ]);
    });

    test('pads ragged rows and accepts Arabic punctuation', () {
      expect(parseMatrixCells('1\u060C 2\u061B 3'), [
        ['1', '2'],
        ['3', ''],
      ]);
    });

    test('joins back into the form the core parses', () {
      expect(
        joinMatrixCells([
          ['1', '2'],
          ['3', '4'],
        ]),
        '1, 2; 3, 4',
      );
      expect(
        joinMatrixCells(const [
          ['5'],
          ['1'],
        ]),
        '5; 1',
      );
    });
  });

  testWidgets('the grid edits flow back into the field value', (tester) async {
    final controller = TextEditingController(text: '1, 2; 3, 4');
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        home: Scaffold(
          body: SingleChildScrollView(
            child: MatrixInput(
              controller: controller,
              l10n: const L10n(Locale('en')),
            ),
          ),
        ),
      ),
    );
    await tester.pump();

    expect(find.byType(TextField), findsNWidgets(4));
    await tester.enterText(find.byType(TextField).first, '7');
    await tester.pump();
    expect(controller.text, '7, 2; 3, 4');
  });

  testWidgets('an external value reshapes the grid', (tester) async {
    final controller = TextEditingController(text: '1, 2; 3, 4');
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        home: Scaffold(
          body: SingleChildScrollView(
            child: MatrixInput(
              controller: controller,
              l10n: const L10n(Locale('en')),
            ),
          ),
        ),
      ),
    );
    await tester.pump();

    controller.text = '1, 2, 3; 4, 5, 6';
    await tester.pump();
    expect(find.byType(TextField), findsNWidgets(6));
  });

  testWidgets('expression fields expose the scanner, numeric ones do not', (
    tester,
  ) async {
    final controller = TextEditingController(text: 'x');
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        home: Scaffold(
          body: Column(
            children: [
              ExpressionField(controller: controller, hint: 'hint'),
              LabelledField(controller: controller, label: 'a'),
            ],
          ),
        ),
      ),
    );
    await tester.pump();
    expect(find.byIcon(Icons.document_scanner_outlined), findsOneWidget);

    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        home: Scaffold(
          body: LabelledField(controller: controller, label: 'a', scan: true),
        ),
      ),
    );
    await tester.pump();
    expect(find.byIcon(Icons.document_scanner_outlined), findsOneWidget);
  });
}
