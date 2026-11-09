import 'package:flutter/material.dart';

/// A named accent palette. Backgrounds and typography stay consistent; only
/// the accents (and the very subtle tint they cast on surfaces) change, so the
/// app keeps one visual identity while users can make it their own.
@immutable
class OdePalette {
  const OdePalette({
    required this.id,
    required this.labelKey,
    required this.darkAccent,
    required this.lightAccent,
    required this.darkAlt,
    required this.lightAlt,
    required this.darkInfo,
    required this.lightInfo,
    required this.darkCreative,
    required this.lightCreative,
  });

  final String id;
  final String labelKey;
  final Color darkAccent;
  final Color lightAccent;
  final Color darkAlt;
  final Color lightAlt;
  final Color darkInfo;
  final Color lightInfo;
  final Color darkCreative;
  final Color lightCreative;

  static const ocean = OdePalette(
    id: 'ocean',
    labelKey: 'ui.palette.ocean',
    darkAccent: Color(0xFF5CC8FF),
    lightAccent: Color(0xFF0B5C8A),
    darkAlt: Color(0xFF5EEAD4),
    lightAlt: Color(0xFF115E59),
    darkInfo: Color(0xFF38BDF8),
    lightInfo: Color(0xFF0369A1),
    darkCreative: Color(0xFFFBBF24),
    lightCreative: Color(0xFFB45309),
  );

  static const indigo = OdePalette(
    id: 'indigo',
    labelKey: 'ui.palette.indigo',
    darkAccent: Color(0xFF818CF8),
    lightAccent: Color(0xFF4F46E5),
    darkAlt: Color(0xFFC084FC),
    lightAlt: Color(0xFF7E22CE),
    darkInfo: Color(0xFF38BDF8),
    lightInfo: Color(0xFF0369A1),
    darkCreative: Color(0xFFF0ABFC),
    lightCreative: Color(0xFFA21CAF),
  );

  static const emerald = OdePalette(
    id: 'emerald',
    labelKey: 'ui.palette.emerald',
    darkAccent: Color(0xFF34D399),
    lightAccent: Color(0xFF046C4E),
    darkAlt: Color(0xFF2DD4BF),
    lightAlt: Color(0xFF115E59),
    darkInfo: Color(0xFF38BDF8),
    lightInfo: Color(0xFF0369A1),
    darkCreative: Color(0xFFFBBF24),
    lightCreative: Color(0xFFB45309),
  );

  static const amber = OdePalette(
    id: 'amber',
    labelKey: 'ui.palette.amber',
    darkAccent: Color(0xFFFBBF24),
    lightAccent: Color(0xFF92400E),
    darkAlt: Color(0xFFFB923C),
    lightAlt: Color(0xFF9A3412),
    darkInfo: Color(0xFF38BDF8),
    lightInfo: Color(0xFF0369A1),
    darkCreative: Color(0xFFF472B6),
    lightCreative: Color(0xFFBE185D),
  );

  static const rose = OdePalette(
    id: 'rose',
    labelKey: 'ui.palette.rose',
    darkAccent: Color(0xFFFB7185),
    lightAccent: Color(0xFFBE123C),
    darkAlt: Color(0xFFF0ABFC),
    lightAlt: Color(0xFFA21CAF),
    darkInfo: Color(0xFF38BDF8),
    lightInfo: Color(0xFF0369A1),
    darkCreative: Color(0xFFFBBF24),
    lightCreative: Color(0xFFB45309),
  );

  static const graphite = OdePalette(
    id: 'graphite',
    labelKey: 'ui.palette.graphite',
    darkAccent: Color(0xFFA8B6CC),
    lightAccent: Color(0xFF475569),
    darkAlt: Color(0xFF7DD3FC),
    lightAlt: Color(0xFF0369A1),
    darkInfo: Color(0xFF38BDF8),
    lightInfo: Color(0xFF0369A1),
    darkCreative: Color(0xFFF0ABFC),
    lightCreative: Color(0xFFA21CAF),
  );

  static const all = <OdePalette>[
    ocean,
    indigo,
    emerald,
    amber,
    rose,
    graphite,
  ];

  static OdePalette byId(String id) =>
      all.firstWhere((p) => p.id == id, orElse: () => rose);
}

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

  /// Primary accent used for actions, selection and the logo.
  final Color accent;
  final Color accentSoft;

  /// Companion accent used for study notes and secondary emphasis.
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
    background: Color(0xFF070B12),
    backgroundAlt: Color(0xFF0A111C),
    card: Color(0xFF0E1624),
    cardAlt: Color(0xFF141F31),
    line: Color(0xFF24324A),
    text: Color(0xFFF2F6FF),
    muted: Color(0xFF96A6C0),
    accent: Color(0xFF4F8DF7),
    accentSoft: Color(0xFF102447),
    accentAlt: Color(0xFF8B8CF9),
    accentAltSoft: Color(0xFF1A1D43),
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
    background: Color(0xFFF5F7FA),
    backgroundAlt: Color(0xFFEBEFF5),
    card: Color(0xFFFFFFFF),
    cardAlt: Color(0xFFF7F9FC),
    line: Color(0xFFE0E6EF),
    text: Color(0xFF0B1220),
    muted: Color(0xFF58657A),
    accent: Color(0xFF2563EB),
    accentSoft: Color(0xFFE7EEFF),
    accentAlt: Color(0xFF4338CA),
    accentAltSoft: Color(0xFFECEBFF),
    warning: Color(0xFF9A5C0A),
    warningSoft: Color(0xFFFCF0DC),
    success: Color(0xFF166534),
    successSoft: Color(0xFFE4F5EA),
    info: Color(0xFF0E7490),
    infoSoft: Color(0xFFE0F5F9),
    creative: Color(0xFFBE185D),
    creativeSoft: Color(0xFFFCE7F1),
    glow: Color(0xFFC7DBFF),
  );

  /// Builds the palette-aware colour set. The base surfaces keep their tested
  /// contrast; the accent tint is deliberately small so charts and formulas
  /// stay readable on every preset.
  static OdeColors themed({
    required OdePalette palette,
    required bool dark,
    bool highContrast = false,
  }) {
    final base = dark ? OdeColors.dark : OdeColors.light;
    final accent = dark ? palette.darkAccent : palette.lightAccent;
    final alt = dark ? palette.darkAlt : palette.lightAlt;
    final info = dark ? palette.darkInfo : palette.lightInfo;
    final creative = dark ? palette.darkCreative : palette.lightCreative;

    Color tint(Color color, double amount) =>
        Color.alphaBlend(accent.withValues(alpha: amount), color);

    final background = highContrast
        ? (dark ? const Color(0xFF04070E) : const Color(0xFFF8FAFE))
        : tint(base.background, dark ? 0.022 : 0.010);
    final backgroundAlt = highContrast
        ? (dark ? const Color(0xFF070D18) : const Color(0xFFEEF3FB))
        : tint(base.backgroundAlt, dark ? 0.018 : 0.008);
    final card = highContrast
        ? (dark ? const Color(0xFF0B1322) : Colors.white)
        : tint(base.card, dark ? 0.010 : 0.004);
    final cardAlt = highContrast
        ? (dark ? const Color(0xFF111C31) : const Color(0xFFF7FAFF))
        : tint(base.cardAlt, dark ? 0.016 : 0.006);

    return base.copyWith(
      background: background,
      backgroundAlt: backgroundAlt,
      card: card,
      cardAlt: cardAlt,
      line: highContrast
          ? (dark ? const Color(0xFF42567A) : const Color(0xFFB7C5DC))
          : tint(base.line, dark ? 0.05 : 0.02),
      text: highContrast
          ? (dark ? Colors.white : const Color(0xFF050A14))
          : base.text,
      muted: highContrast
          ? (dark ? const Color(0xFFC7D3E8) : const Color(0xFF3B4964))
          : base.muted,
      accent: accent,
      accentSoft: Color.alphaBlend(
        accent.withValues(alpha: dark ? (highContrast ? 0.22 : 0.17) : 0.11),
        background,
      ),
      accentAlt: alt,
      accentAltSoft: Color.alphaBlend(
        alt.withValues(alpha: dark ? (highContrast ? 0.22 : 0.16) : 0.10),
        background,
      ),
      info: info,
      infoSoft: Color.alphaBlend(
        info.withValues(alpha: dark ? 0.16 : 0.10),
        background,
      ),
      creative: creative,
      creativeSoft: Color.alphaBlend(
        creative.withValues(alpha: dark ? 0.16 : 0.10),
        background,
      ),
      glow: accent.withValues(alpha: dark ? 0.34 : 0.26),
    );
  }

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

ThemeData buildOdeTheme({
  required bool dark,
  OdePalette palette = OdePalette.ocean,
  bool highContrast = false,
  bool compact = false,
}) {
  final c = OdeColors.themed(
    palette: palette,
    dark: dark,
    highContrast: highContrast,
  );
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
        outlineVariant: c.line.withValues(alpha: 0.65),
        error: dark ? const Color(0xFFF08A80) : const Color(0xFFB3261E),
      );

  final base = ThemeData(
    useMaterial3: true,
    brightness: dark ? Brightness.dark : Brightness.light,
    colorScheme: scheme,
    fontFamily: 'Vazirmatn',
    scaffoldBackgroundColor: c.background,
    focusColor: c.accent.withValues(alpha: 0.22),
    visualDensity: compact ? VisualDensity.compact : VisualDensity.standard,
    materialTapTargetSize: compact
        ? MaterialTapTargetSize.shrinkWrap
        : MaterialTapTargetSize.padded,
  );

  final textTheme = base.textTheme.apply(
    bodyColor: c.text,
    displayColor: c.text,
    // Vazirmatn covers Latin, Persian and Arabic. The remaining families let
    // Cyrillic, Devanagari and CJK fall back to a readable system face.
    fontFamilyFallback: const [
      'SpaceGrotesk',
      'Noto Sans',
      'Noto Sans Arabic',
      'Noto Sans Devanagari',
      'Noto Sans CJK SC',
      'Noto Sans CJK JP',
      'Noto Sans CJK KR',
    ],
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

  final controlRadius = BorderRadius.circular(compact ? 12 : 14);

  return base.copyWith(
    extensions: [c],
    textTheme: textTheme.copyWith(
      headlineSmall: role(
        textTheme.headlineSmall,
        fontSize: 25,
        fontWeight: FontWeight.w700,
        letterSpacing: -0.4,
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
        height: 1.7,
        color: c.text,
      ),
      bodySmall: role(
        textTheme.bodySmall,
        fontSize: 12.5,
        height: 1.6,
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
      contentPadding: EdgeInsets.symmetric(
        horizontal: 18,
        vertical: compact ? 13 : 16,
      ),
      hintStyle: TextStyle(
        fontFamily: 'Vazirmatn',
        color: c.muted,
        height: 1.6,
      ),
      labelStyle: TextStyle(fontFamily: 'Vazirmatn', color: c.muted),
      floatingLabelStyle: TextStyle(
        fontFamily: 'Vazirmatn',
        color: c.accent,
        fontWeight: FontWeight.w600,
      ),
      prefixIconColor: c.muted,
      suffixIconColor: c.muted,
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(compact ? 13 : 16),
        borderSide: BorderSide(color: c.line),
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(compact ? 13 : 16),
        borderSide: BorderSide(color: c.line),
      ),
      focusedBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(compact ? 13 : 16),
        borderSide: BorderSide(color: c.accent, width: 1.6),
      ),
      errorBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(compact ? 13 : 16),
        borderSide: BorderSide(color: scheme.error.withValues(alpha: 0.7)),
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
        padding: EdgeInsets.symmetric(
          horizontal: 26,
          vertical: compact ? 15 : 18,
        ),
        textStyle: const TextStyle(
          fontFamily: 'Vazirmatn',
          fontSize: 15,
          fontWeight: FontWeight.w700,
        ),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(compact ? 13 : 16),
        ),
      ),
    ),
    outlinedButtonTheme: OutlinedButtonThemeData(
      style: OutlinedButton.styleFrom(
        foregroundColor: c.text,
        side: BorderSide(color: c.line),
        padding: EdgeInsets.symmetric(
          horizontal: 20,
          vertical: compact ? 13 : 16,
        ),
        textStyle: const TextStyle(
          fontFamily: 'Vazirmatn',
          fontSize: 14,
          fontWeight: FontWeight.w600,
        ),
        shape: RoundedRectangleBorder(borderRadius: controlRadius),
      ),
    ),
    textButtonTheme: TextButtonThemeData(
      style: TextButton.styleFrom(
        foregroundColor: c.accent,
        textStyle: const TextStyle(
          fontFamily: 'Vazirmatn',
          fontSize: 13,
          fontWeight: FontWeight.w600,
        ),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
      ),
    ),
    segmentedButtonTheme: SegmentedButtonThemeData(
      style: ButtonStyle(
        visualDensity: compact ? VisualDensity.compact : VisualDensity.standard,
        side: WidgetStatePropertyAll(BorderSide(color: c.line)),
        backgroundColor: WidgetStateProperty.resolveWith((states) {
          if (states.contains(WidgetState.selected)) return c.accentSoft;
          return Colors.transparent;
        }),
        foregroundColor: WidgetStateProperty.resolveWith((states) {
          if (states.contains(WidgetState.selected)) return c.accent;
          return c.muted;
        }),
        textStyle: const WidgetStatePropertyAll(
          TextStyle(
            fontFamily: 'Vazirmatn',
            fontSize: 13,
            fontWeight: FontWeight.w600,
          ),
        ),
        shape: WidgetStatePropertyAll(
          RoundedRectangleBorder(borderRadius: controlRadius),
        ),
      ),
    ),
    switchTheme: SwitchThemeData(
      thumbColor: WidgetStateProperty.resolveWith(
        (states) => states.contains(WidgetState.selected)
            ? (dark ? const Color(0xFF06101F) : Colors.white)
            : c.muted,
      ),
      trackColor: WidgetStateProperty.resolveWith(
        (states) => states.contains(WidgetState.selected)
            ? c.accent
            : c.line.withValues(alpha: 0.6),
      ),
      trackOutlineColor: WidgetStatePropertyAll(c.line),
    ),
    sliderTheme: base.sliderTheme.copyWith(
      activeTrackColor: c.accent,
      inactiveTrackColor: c.line,
      thumbColor: c.accent,
      overlayColor: c.accent.withValues(alpha: 0.12),
      valueIndicatorColor: c.accent,
    ),
    progressIndicatorTheme: ProgressIndicatorThemeData(
      color: c.accent,
      linearTrackColor: c.line,
      circularTrackColor: c.line,
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
    tooltipTheme: TooltipThemeData(
      decoration: BoxDecoration(
        color: dark ? c.cardAlt : const Color(0xFF10203A),
        borderRadius: BorderRadius.circular(9),
        border: Border.all(color: c.line.withValues(alpha: 0.7)),
      ),
      textStyle: const TextStyle(
        fontFamily: 'Vazirmatn',
        fontSize: 12,
        color: Colors.white,
      ),
      waitDuration: const Duration(milliseconds: 500),
    ),
    scrollbarTheme: ScrollbarThemeData(
      thumbColor: WidgetStateProperty.resolveWith(
        (states) => c.muted.withValues(
          alpha: states.contains(WidgetState.hovered) ? 0.65 : 0.38,
        ),
      ),
      radius: const Radius.circular(8),
      thickness: WidgetStateProperty.resolveWith(
        (states) => states.contains(WidgetState.hovered) ? 7 : 5,
      ),
      crossAxisMargin: 3,
    ),
    textSelectionTheme: TextSelectionThemeData(
      cursorColor: c.accent,
      selectionColor: c.accent.withValues(alpha: 0.24),
      selectionHandleColor: c.accent,
    ),
    dialogTheme: DialogThemeData(
      backgroundColor: c.card,
      surfaceTintColor: Colors.transparent,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(22)),
    ),
    bottomSheetTheme: BottomSheetThemeData(
      backgroundColor: c.card,
      modalBackgroundColor: c.card,
      surfaceTintColor: Colors.transparent,
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
