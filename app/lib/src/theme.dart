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
    required this.accentAlt,
    required this.accentAltSoft,
    required this.warning,
    required this.warningSoft,
    required this.success,
    required this.successSoft,
    required this.info,
    required this.infoSoft,
    required this.creative,
    required this.creativeSoft,
    required this.glow,
  });

  final Color background;
  final Color backgroundAlt;
  final Color card;
  final Color cardAlt;
  final Color line;
  final Color text;
  final Color muted;

  /// Primary blue accent used for actions, selection and the logo.
  final Color accent;
  final Color accentSoft;

  /// Indigo companion used for study notes and secondary emphasis.
  final Color accentAlt;
  final Color accentAltSoft;

  final Color warning;
  final Color warningSoft;
  final Color success;
  final Color successSoft;

  /// Cyan used for domain/range plots and informational notes.
  final Color info;
  final Color infoSoft;

  /// Rose used for envelopes and secondary highlights.
  final Color creative;
  final Color creativeSoft;

  final Color glow;

  static const dark = OdeColors(
    background: Color(0xFF050810),
    backgroundAlt: Color(0xFF0A1122),
    card: Color(0xFF0E162A),
    cardAlt: Color(0xFF141F38),
    line: Color(0xFF253455),
    text: Color(0xFFEEF3FF),
    muted: Color(0xFF93A2C4),
    accent: Color(0xFF3B82F6),
    accentSoft: Color(0xFF0E1B36),
    accentAlt: Color(0xFF818CF8),
    accentAltSoft: Color(0xFF191C3F),
    warning: Color(0xFFF2B45C),
    warningSoft: Color(0xFF2A2318),
    success: Color(0xFF34D399),
    successSoft: Color(0xFF102A22),
    info: Color(0xFF22D3EE),
    infoSoft: Color(0xFF0B2530),
    creative: Color(0xFFF472B6),
    creativeSoft: Color(0xFF2A1626),
    glow: Color(0xFF1E3A8A),
  );

  static const light = OdeColors(
    background: Color(0xFFF3F6FC),
    backgroundAlt: Color(0xFFE7EEFB),
    card: Color(0xFFFFFFFF),
    cardAlt: Color(0xFFF5F8FF),
    line: Color(0xFFDCE5F5),
    text: Color(0xFF10182B),
    muted: Color(0xFF5D6B8A),
    accent: Color(0xFF1D4ED8),
    accentSoft: Color(0xFFE4EDFF),
    accentAlt: Color(0xFF4338CA),
    accentAltSoft: Color(0xFFECEBFF),
    warning: Color(0xFF9A5C0A),
    warningSoft: Color(0xFFFCF0DC),
    success: Color(0xFF15803D),
    successSoft: Color(0xFFE4F5EA),
    info: Color(0xFF0E7490),
    infoSoft: Color(0xFFE0F5F9),
    creative: Color(0xFFBE185D),
    creativeSoft: Color(0xFFFCE7F1),
    glow: Color(0xFFC7DBFF),
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
    Color? accentAlt,
    Color? accentAltSoft,
    Color? warning,
    Color? warningSoft,
    Color? success,
    Color? successSoft,
    Color? info,
    Color? infoSoft,
    Color? creative,
    Color? creativeSoft,
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
      accentAlt: accentAlt ?? this.accentAlt,
      accentAltSoft: accentAltSoft ?? this.accentAltSoft,
      warning: warning ?? this.warning,
      warningSoft: warningSoft ?? this.warningSoft,
      success: success ?? this.success,
      successSoft: successSoft ?? this.successSoft,
      info: info ?? this.info,
      infoSoft: infoSoft ?? this.infoSoft,
      creative: creative ?? this.creative,
      creativeSoft: creativeSoft ?? this.creativeSoft,
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
      accentAlt: Color.lerp(accentAlt, other.accentAlt, t)!,
      accentAltSoft: Color.lerp(accentAltSoft, other.accentAltSoft, t)!,
      warning: Color.lerp(warning, other.warning, t)!,
      warningSoft: Color.lerp(warningSoft, other.warningSoft, t)!,
      success: Color.lerp(success, other.success, t)!,
      successSoft: Color.lerp(successSoft, other.successSoft, t)!,
      info: Color.lerp(info, other.info, t)!,
      infoSoft: Color.lerp(infoSoft, other.infoSoft, t)!,
      creative: Color.lerp(creative, other.creative, t)!,
      creativeSoft: Color.lerp(creativeSoft, other.creativeSoft, t)!,
      glow: Color.lerp(glow, other.glow, t)!,
    );
  }
}

extension OdeTheme on BuildContext {
  OdeColors get colors => Theme.of(this).extension<OdeColors>()!;
}

ThemeData buildOdeTheme({required bool dark}) {
  final c = dark ? OdeColors.dark : OdeColors.light;
  final scheme =
      ColorScheme.fromSeed(
        brightness: dark ? Brightness.dark : Brightness.light,
        seedColor: c.accent,
      ).copyWith(
        primary: c.accent,
        onPrimary: dark ? const Color(0xFF06101F) : Colors.white,
        secondary: c.accentAlt,
        onSecondary: dark ? const Color(0xFF0B1024) : Colors.white,
        surface: c.card,
        onSurface: c.text,
        outline: c.line,
        error: dark ? const Color(0xFFF08A80) : const Color(0xFFB3261E),
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
    // Vazirmatn covers Persian but lacks some math punctuation (arrows, π).
    // SpaceGrotesk fills those while the system font handles the rest.
    fontFamilyFallback: const ['SpaceGrotesk'],
  );

  // Derive each override from the themed style so the Vazirmatn family is
  // never dropped. Building a bare TextStyle here would silently fall back to
  // the platform font for that role.
  TextStyle? role(
    TextStyle? base, {
    double? fontSize,
    FontWeight? fontWeight,
    double? height,
    double? letterSpacing,
    Color? color,
  }) => base?.copyWith(
    fontSize: fontSize,
    fontWeight: fontWeight,
    height: height,
    letterSpacing: letterSpacing,
    color: color,
  );

  return base.copyWith(
    extensions: [c],
    textTheme: textTheme.copyWith(
      headlineSmall: role(
        textTheme.headlineSmall,
        fontSize: 25,
        fontWeight: FontWeight.w700,
        letterSpacing: -0.3,
        height: 1.35,
        color: c.text,
      ),
      titleLarge: role(
        textTheme.titleLarge,
        fontSize: 19,
        fontWeight: FontWeight.w700,
        height: 1.4,
        color: c.text,
      ),
      titleMedium: role(
        textTheme.titleMedium,
        fontSize: 15.5,
        fontWeight: FontWeight.w600,
        height: 1.5,
        color: c.text,
      ),
      bodyMedium: role(
        textTheme.bodyMedium,
        fontSize: 14,
        height: 1.8,
        color: c.text,
      ),
      bodySmall: role(
        textTheme.bodySmall,
        fontSize: 12.5,
        height: 1.7,
        color: c.muted,
      ),
      labelLarge: role(
        textTheme.labelLarge,
        fontSize: 14,
        fontWeight: FontWeight.w600,
        color: c.text,
      ),
    ),
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: dark ? c.cardAlt : Colors.white,
      contentPadding: const EdgeInsets.symmetric(horizontal: 18, vertical: 16),
      hintStyle: TextStyle(
        fontFamily: 'Vazirmatn',
        color: c.muted,
        height: 1.6,
      ),
      labelStyle: TextStyle(fontFamily: 'Vazirmatn', color: c.muted),
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
      backgroundColor: dark ? c.cardAlt : const Color(0xFF10203A),
      contentTextStyle: const TextStyle(
        fontFamily: 'Vazirmatn',
        color: Colors.white,
      ),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(14)),
    ),
    dividerTheme: DividerThemeData(color: c.line, thickness: 1, space: 1),
    filledButtonTheme: FilledButtonThemeData(
      style: FilledButton.styleFrom(
        backgroundColor: c.accent,
        foregroundColor: dark ? const Color(0xFF06101F) : Colors.white,
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
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
    colors: [c.backgroundAlt, c.background, c.background],
    stops: const [0, 0.5, 1],
  ),
);
