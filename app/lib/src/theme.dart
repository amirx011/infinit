import 'package:flutter/material.dart';

@immutable
class OdeColors extends ThemeExtension<OdeColors> {
  const OdeColors({
    required this.background,
    required this.backgroundAlt,
    required this.card,
    required this.cardAlt,
    required this.line,
    required this.text,
    required this.muted,
    required this.accent,
    required this.accentSoft,
    required this.warning,
    required this.warningSoft,
    required this.success,
    required this.glow,
  });

  final Color background;
  final Color backgroundAlt;
  final Color card;
  final Color cardAlt;
  final Color line;
  final Color text;
  final Color muted;
  final Color accent;
  final Color accentSoft;
  final Color warning;
  final Color warningSoft;
  final Color success;
  final Color glow;

  static const dark = OdeColors(
    background: Color(0xFF050509),
    backgroundAlt: Color(0xFF0B0A14),
    card: Color(0xFF101019),
    cardAlt: Color(0xFF171525),
    line: Color(0xFF2A2640),
    text: Color(0xFFF2EFFF),
    muted: Color(0xFF9C96BB),
    accent: Color(0xFF7C5CFF),
    accentSoft: Color(0xFF1D173B),
    warning: Color(0xFFF0B35A),
    warningSoft: Color(0xFF2A2117),
    success: Color(0xFF8B7CFF),
    glow: Color(0xFF3B2E8A),
  );

  static const light = OdeColors(
    background: Color(0xFFF4F1FB),
    backgroundAlt: Color(0xFFECE7FA),
    card: Color(0xFFFFFFFF),
    cardAlt: Color(0xFFF7F3FF),
    line: Color(0xFFE2DCF0),
    text: Color(0xFF171322),
    muted: Color(0xFF6F6885),
    accent: Color(0xFF5B45D6),
    accentSoft: Color(0xFFECE7FF),
    warning: Color(0xFFB07A1E),
    warningSoft: Color(0xFFFBF0DC),
    success: Color(0xFF5B45D6),
    glow: Color(0xFFC9BFFF),
  );

  @override
  OdeColors copyWith({
    Color? background,
    Color? backgroundAlt,
    Color? card,
    Color? cardAlt,
    Color? line,
    Color? text,
    Color? muted,
    Color? accent,
    Color? accentSoft,
    Color? warning,
    Color? warningSoft,
    Color? success,
    Color? glow,
  }) {
    return OdeColors(
      background: background ?? this.background,
      backgroundAlt: backgroundAlt ?? this.backgroundAlt,
      card: card ?? this.card,
      cardAlt: cardAlt ?? this.cardAlt,
      line: line ?? this.line,
      text: text ?? this.text,
      muted: muted ?? this.muted,
      accent: accent ?? this.accent,
      accentSoft: accentSoft ?? this.accentSoft,
      warning: warning ?? this.warning,
      warningSoft: warningSoft ?? this.warningSoft,
      success: success ?? this.success,
      glow: glow ?? this.glow,
    );
  }

  @override
  OdeColors lerp(OdeColors? other, double t) {
    if (other == null) return this;
    return OdeColors(
      background: Color.lerp(background, other.background, t)!,
      backgroundAlt: Color.lerp(backgroundAlt, other.backgroundAlt, t)!,
      card: Color.lerp(card, other.card, t)!,
      cardAlt: Color.lerp(cardAlt, other.cardAlt, t)!,
      line: Color.lerp(line, other.line, t)!,
      text: Color.lerp(text, other.text, t)!,
      muted: Color.lerp(muted, other.muted, t)!,
      accent: Color.lerp(accent, other.accent, t)!,
      accentSoft: Color.lerp(accentSoft, other.accentSoft, t)!,
      warning: Color.lerp(warning, other.warning, t)!,
      warningSoft: Color.lerp(warningSoft, other.warningSoft, t)!,
      success: Color.lerp(success, other.success, t)!,
      glow: Color.lerp(glow, other.glow, t)!,
    );
  }
}

extension OdeTheme on BuildContext {
  OdeColors get colors => Theme.of(this).extension<OdeColors>()!;
}

ThemeData buildOdeTheme({required bool dark}) {
  final c = dark ? OdeColors.dark : OdeColors.light;
  final scheme = ColorScheme.fromSeed(
    brightness: dark ? Brightness.dark : Brightness.light,
    seedColor: c.accent,
  ).copyWith(
    primary: c.accent,
    onPrimary: dark ? const Color(0xFF0A0714) : Colors.white,
    secondary: c.warning,
    surface: c.card,
    onSurface: c.text,
    outline: c.line,
    error: const Color(0xFFE08978),
  );

  final base = ThemeData(
    useMaterial3: true,
    brightness: dark ? Brightness.dark : Brightness.light,
    colorScheme: scheme,
    fontFamily: 'Vazirmatn',
    scaffoldBackgroundColor: c.background,
  );

  final textTheme = base.textTheme.apply(
    bodyColor: c.text,
    displayColor: c.text,
  );

  return base.copyWith(
    extensions: [c],
    textTheme: textTheme.copyWith(
      headlineSmall: TextStyle(
        fontSize: 25,
        fontWeight: FontWeight.w700,
        letterSpacing: -0.3,
        height: 1.35,
        color: c.text,
      ),
      titleLarge: TextStyle(
        fontSize: 19,
        fontWeight: FontWeight.w700,
        height: 1.4,
        color: c.text,
      ),
      titleMedium: TextStyle(
        fontSize: 15.5,
        fontWeight: FontWeight.w600,
        height: 1.5,
        color: c.text,
      ),
      bodyMedium: TextStyle(fontSize: 14, height: 1.8, color: c.text),
      bodySmall: TextStyle(fontSize: 12.5, height: 1.7, color: c.muted),
      labelLarge: TextStyle(
        fontSize: 14,
        fontWeight: FontWeight.w600,
        color: c.text,
      ),
    ),
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: dark ? c.cardAlt : Colors.white,
      contentPadding: const EdgeInsets.symmetric(horizontal: 18, vertical: 16),
      hintStyle: TextStyle(color: c.muted.withValues(alpha: 0.75), height: 1.6),
      labelStyle: TextStyle(color: c.muted),
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(16),
        borderSide: BorderSide(color: c.line),
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(16),
        borderSide: BorderSide(color: c.line),
      ),
      focusedBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(16),
        borderSide: BorderSide(color: c.accent, width: 1.5),
      ),
    ),
    snackBarTheme: SnackBarThemeData(
      behavior: SnackBarBehavior.floating,
      backgroundColor: dark ? c.cardAlt : const Color(0xFF17211E),
      contentTextStyle: const TextStyle(fontFamily: 'Vazirmatn', color: Colors.white),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(14)),
    ),
    dividerTheme: DividerThemeData(color: c.line, thickness: 1, space: 1),
    filledButtonTheme: FilledButtonThemeData(
      style: FilledButton.styleFrom(
        backgroundColor: c.accent,
        foregroundColor: dark ? const Color(0xFF0A0714) : Colors.white,
        padding: const EdgeInsets.symmetric(horizontal: 26, vertical: 18),
        textStyle: const TextStyle(
          fontFamily: 'Vazirmatn',
          fontSize: 15,
          fontWeight: FontWeight.w700,
        ),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      ),
    ),
    chipTheme: base.chipTheme.copyWith(
      backgroundColor: dark ? c.cardAlt : Colors.white,
      side: BorderSide(color: c.line),
      labelStyle: TextStyle(
        fontFamily: 'Vazirmatn',
        color: c.muted,
        fontSize: 12.5,
      ),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(11)),
      padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
    ),
    iconTheme: IconThemeData(color: c.muted),
    dialogTheme: DialogThemeData(
      backgroundColor: c.card,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(22)),
    ),
    bottomSheetTheme: BottomSheetThemeData(
      backgroundColor: c.card,
      modalBackgroundColor: c.card,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(26)),
      ),
    ),
  );
}

BoxDecoration pageBackground(OdeColors c) => BoxDecoration(
      gradient: LinearGradient(
        begin: Alignment.topRight,
        end: Alignment.bottomLeft,
        colors: [c.background, c.backgroundAlt],
      ),
    );
