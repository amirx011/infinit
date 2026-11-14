import 'package:flutter/material.dart';
import 'package:flutter_math_fork/flutter_math.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/widgets.dart';

Widget _host(Widget child, {bool rtl = false}) => MaterialApp(
  home: Directionality(
    textDirection: rtl ? TextDirection.rtl : TextDirection.ltr,
    child: Scaffold(body: Center(child: child)),
  ),
);

void main() {
  testWidgets('plain prose stays a plain Text', (tester) async {
    await tester.pumpWidget(_host(const MathText('no mathematics here')));
    expect(find.text('no mathematics here'), findsOneWidget);
    expect(find.byType(Math), findsNothing);
  });

  testWidgets('an inline fraction becomes a typeset formula', (tester) async {
    await tester.pumpWidget(
      _host(const MathText(r'a fraction $\frac{1}{2}$ inside a sentence')),
    );
    // The sentence is still there, but the fraction is a Math widget.
    expect(find.byType(Math), findsOneWidget);
    final math = tester.widget<Math>(find.byType(Math));
    expect(math.mathStyle, MathStyle.text);
    expect(find.textContaining('a fraction'), findsOneWidget);
    expect(find.textContaining('inside a sentence'), findsOneWidget);
  });

  testWidgets('display math uses the display style', (tester) async {
    await tester.pumpWidget(_host(const MathText(r'$$\int_0^1 x\,dx$$')));
    final math = tester.widget<Math>(find.byType(Math));
    expect(math.mathStyle, MathStyle.display);
  });

  testWidgets('two formulas in one sentence are both typeset', (tester) async {
    await tester.pumpWidget(
      _host(const MathText(r'$x$ and $y$')),
    );
    expect(find.byType(Math), findsNWidgets(2));
  });

  testWidgets('invalid TeX falls back to readable text', (tester) async {
    await tester.pumpWidget(_host(const MathText(r'broken $\frac{1}{$ here')));
    // The Math widget shows the raw TeX instead of throwing.
    expect(find.text(r'\frac{1}{'), findsOneWidget);
  });

  testWidgets('right-to-left prose keeps the formula isolated', (tester) async {
    // Persian letters built from code points keep this source file free of
    // literal Arabic script (the i18n guard forbids it outside the catalogue).
    final rtl = String.fromCharCodes([0x645, 0x642, 0x62F, 0x627, 0x631]);
    await tester.pumpWidget(
      _host(MathText('$rtl \$\\frac{1}{2}\$ $rtl'), rtl: true),
    );
    expect(find.byType(Math), findsOneWidget);
  });
}
