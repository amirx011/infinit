import 'dart:math' as math;

import 'models.dart';

/// Formats a real number for the interface: compact decimals, scientific
/// notation for extreme magnitudes and a typographic minus sign.
String formatReal(double v) {
  if (v.isNaN) return '?';
  if (v.isInfinite) return v < 0 ? '−∞' : '∞';
  if (v == 0) return '0';
  final magnitude = v.abs();
  String text;
  if (magnitude >= 1e6 || magnitude < 1e-4) {
    var exponent = (math.log(magnitude) / math.ln10).floor();
    var mantissa = v / math.pow(10, exponent);
    if (mantissa.abs() >= 9.99995) {
      mantissa /= 10;
      exponent += 1;
    }
    text = '${_trim(mantissa)}×10^$exponent';
  } else {
    text = _trim(v);
  }
  return text.replaceAll('-', '−');
}

String _trim(double v) {
  var s = v.toStringAsPrecision(6);
  if (s.contains('e')) return v.toString();
  if (s.contains('.')) {
    s = s.replaceFirst(RegExp(r'0+$'), '');
    s = s.replaceFirst(RegExp(r'\.$'), '');
  }
  return s;
}

/// Standard interval notation, e.g. `(−∞, −1] ∪ [1, ∞)`.
String intervalText(IntervalView iv) {
  final left = iv.loOpen ? '(' : '[';
  final right = iv.hiOpen ? ')' : ']';
  final lo = iv.loInfinite ? '−∞' : formatReal(iv.lo);
  final hi = iv.hiInfinite ? '∞' : formatReal(iv.hi);
  return '$left$lo, $hi$right';
}

String intervalsText(List<IntervalView> intervals) {
  if (intervals.isEmpty) return '∅';
  return intervals.map(intervalText).join(' ∪ ');
}

/// A compact set description such as `ℝ \ {2}` for a single puncture.
String puncturedText(List<double> holes, {required String emptySymbol}) {
  if (holes.isEmpty) return emptySymbol;
  return holes.map(formatReal).join(', ');
}
