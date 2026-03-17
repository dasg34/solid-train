import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

const _tvTextScaleFactor = 0.65;

ThemeData buildAppTheme() {
  final colorScheme =
      ColorScheme.fromSeed(
        seedColor: const Color(0xFF56D6C2),
        brightness: Brightness.dark,
      ).copyWith(
        primary: const Color(0xFF7DE8D5),
        onPrimary: const Color(0xFF04201B),
        secondary: const Color(0xFFFFC26B),
        onSecondary: const Color(0xFF2F1A00),
        surface: const Color(0xFF12212D),
        onSurface: const Color(0xFFF4F7FA),
        outline: const Color(0xFF345165),
      );

  final baseTheme = ThemeData(
    useMaterial3: true,
    brightness: Brightness.dark,
    colorScheme: colorScheme,
  );

  final textTheme = GoogleFonts.notoSansKrTextTheme(baseTheme.textTheme).apply(
    bodyColor: colorScheme.onSurface,
    displayColor: colorScheme.onSurface,
    fontSizeFactor: _tvTextScaleFactor,
  );

  return baseTheme.copyWith(
    scaffoldBackgroundColor: const Color(0xFF07131C),
    textTheme: textTheme,
    chipTheme: baseTheme.chipTheme.copyWith(
      color: WidgetStateProperty.resolveWith((states) {
        if (states.contains(WidgetState.selected)) {
          return colorScheme.primary.withValues(alpha: 0.18);
        }
        return Colors.white.withValues(alpha: 0.05);
      }),
      side: BorderSide(color: colorScheme.outline),
      labelStyle: textTheme.labelMedium,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(999)),
    ),
    cardTheme: CardThemeData(
      color: const Color(0xFF1A2D3B),
      elevation: 0,
      margin: const EdgeInsets.symmetric(vertical: 8),
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(24),
        side: BorderSide(color: Colors.white.withValues(alpha: 0.06)),
      ),
    ),
  );
}
