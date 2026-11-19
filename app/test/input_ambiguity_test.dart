import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/input_ambiguity.dart';

void main() {
  test('suggests parentheses for a bare denominator followed by a term', () {
    expect(
      ambiguousDenominatorSuggestion("y' = 4xy/ x^2 +1"),
      "y' = 4xy/(x^2 +1)",
    );
    expect(
      ambiguousDenominatorSuggestion("y' = 4xy/x^2 - 1"),
      "y' = 4xy/(x^2 - 1)",
    );
    expect(
      ambiguousDenominatorSuggestion("y' = 4y/x + 1"),
      "y' = 4y/(x + 1)",
    );
  });

  test('does not suggest when the denominator is already grouped', () {
    expect(ambiguousDenominatorSuggestion("y' = 4xy/(x^2+1)"), isNull);
    expect(ambiguousDenominatorSuggestion("y' = (4xy)/(x^2+1)"), isNull);
  });

  test('ignores equations without this shape', () {
    expect(ambiguousDenominatorSuggestion("y' = 4y/x"), isNull);
    expect(ambiguousDenominatorSuggestion("y' = 1/x + 1/y"), isNull);
    expect(ambiguousDenominatorSuggestion('y = 3x + 1'), isNull);
  });

  test('keeps unary and exponent signs inside the denominator', () {
    expect(
      ambiguousDenominatorSuggestion("y' = 4xy/x^-2 +1"),
      "y' = 4xy/(x^-2 +1)",
    );
  });
}
