/// Detects the common "missing denominator parentheses" shape.
///
/// `y' = 4xy/ x^2 +1` is read by standard precedence as
/// `(4xy/x²) + 1`.  Many students mean `4xy/(x²+1)`.  The engine must keep
/// the standard reading, but the UI can offer the parenthesised alternative
/// as a one-click correction.  This helper only builds that suggestion; it
/// never silently changes the expression.
String? ambiguousDenominatorSuggestion(String input) {
  final equals = _firstTopLevel(input, '=');
  if (equals < 0) return null;
  final rhs = input.substring(equals + 1);
  final slash = _firstTopLevel(rhs, '/');
  if (slash < 0) return null;
  // A second top-level division (1/x + 1/y) is not this ambiguity.
  if (_firstTopLevel(rhs.substring(slash + 1), '/') >= 0) return null;

  var start = slash + 1;
  while (start < rhs.length && _isSpace(rhs[start])) {
    start++;
  }
  if (start >= rhs.length || rhs[start] == '(') return null;

  final offset = _firstAdditiveOperator(rhs, start);
  if (offset < 0) return null;

  final termEnd = _endOfAdditiveTerm(rhs, offset + 1);
  if (termEnd <= offset + 1) return null;

  final denominator = rhs.substring(start, termEnd).trim();
  if (denominator.isEmpty) return null;

  final suggestedRhs =
      '${rhs.substring(0, slash + 1)}($denominator)${rhs.substring(termEnd)}';
  return '${input.substring(0, equals + 1)}$suggestedRhs';
}

int _firstAdditiveOperator(String s, int from) {
  var depth = 0;
  for (var i = from; i < s.length; i++) {
    final c = s[i];
    if (c == '(' || c == '[' || c == '{') depth++;
    if (c == ')' || c == ']' || c == '}') depth--;
    if (depth != 0 || (c != '+' && c != '-')) continue;
    var prev = i - 1;
    while (prev >= 0 && _isSpace(s[prev])) {
      prev--;
    }
    if (prev >= 0 && _isUnaryContext(s[prev])) continue;
    return i;
  }
  return -1;
}

int _endOfAdditiveTerm(String s, int from) {
  var depth = 0;
  for (var i = from; i < s.length; i++) {
    final c = s[i];
    if (c == '(' || c == '[' || c == '{') depth++;
    if (c == ')' || c == ']' || c == '}') depth--;
    if (depth != 0 || (c != '+' && c != '-')) continue;
    var prev = i - 1;
    while (prev >= 0 && _isSpace(s[prev])) {
      prev--;
    }
    if (prev >= 0 && _isUnaryContext(s[prev])) continue;
    return i;
  }
  return s.length;
}

int _firstTopLevel(String s, String target) {
  var depth = 0;
  for (var i = 0; i < s.length; i++) {
    final c = s[i];
    if (c == '(' || c == '[' || c == '{') depth++;
    if (c == ')' || c == ']' || c == '}') depth--;
    if (depth == 0 && c == target) return i;
  }
  return -1;
}

bool _isUnaryContext(String c) =>
    c == '^' || c == '*' || c == '/' || c == '(' || c == '+' || c == '-';

bool _isSpace(String c) => c == ' ' || c == '\t' || c == '\n' || c == '\r';
