import 'package:flutter/material.dart';
import 'package:genui/genui.dart';
import 'package:google_fonts/google_fonts.dart';

import '../models/template_registry.dart';
import '../models/template_scenario.dart';
import '../models/template_surface.dart';

class GenUiScenarioSurface extends StatefulWidget {
  const GenUiScenarioSurface({
    required this.scenario,
    required this.templateRegistry,
    super.key,
  });

  final TemplateScenario scenario;
  final TemplateRegistry templateRegistry;

  @override
  State<GenUiScenarioSurface> createState() => _GenUiScenarioSurfaceState();
}

class _GenUiScenarioSurfaceState extends State<GenUiScenarioSurface> {
  late final A2uiMessageProcessor _processor;
  int _renderLoadId = 0;
  TemplateSurfaceStyle _surfaceStyle = TemplateSurfaceStyle.standard;

  @override
  void initState() {
    super.initState();
    _processor = A2uiMessageProcessor(
      catalogs: [
        Catalog([
          CoreCatalogItems.card,
          CoreCatalogItems.column,
          CoreCatalogItems.divider,
          CoreCatalogItems.icon,
          CoreCatalogItems.row,
          CoreCatalogItems.text,
        ], catalogId: tvPocCatalogId),
      ],
    );
    _renderScenario();
  }

  @override
  void didUpdateWidget(covariant GenUiScenarioSurface oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.scenario.id != widget.scenario.id) {
      _renderScenario();
    }
  }

  void _renderScenario() {
    _renderScenarioAsync();
  }

  Future<void> _renderScenarioAsync() async {
    final presenter = widget.templateRegistry.presenterFor(widget.scenario.id);
    final loadId = ++_renderLoadId;
    final loadingPayload = presenter.buildLoading();
    if (loadingPayload != null) {
      _renderPayload(loadingPayload);
    }

    try {
      final payload = await presenter.load();
      if (!mounted || loadId != _renderLoadId) {
        return;
      }
      _renderPayload(payload);
    } catch (error) {
      if (!mounted || loadId != _renderLoadId) {
        return;
      }
      _renderPayload(presenter.buildError(error));
    }
  }

  void _renderPayload(TemplateSurfacePayload payload) {
    if (mounted && _surfaceStyle != payload.surfaceStyle) {
      setState(() {
        _surfaceStyle = payload.surfaceStyle;
      });
    } else {
      _surfaceStyle = payload.surfaceStyle;
    }

    _processor.handleMessage(
      SurfaceUpdate(
        surfaceId: previewSurfaceId,
        components: payload.components,
      ),
    );
    if (payload.dataModel.isNotEmpty) {
      _processor.handleMessage(
        DataModelUpdate(
          surfaceId: previewSurfaceId,
          contents: payload.dataModel,
        ),
      );
    }
    _processor.handleMessage(
      const BeginRendering(
        surfaceId: previewSurfaceId,
        root: 'root',
        catalogId: tvPocCatalogId,
      ),
    );
  }

  @override
  void dispose() {
    _processor.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final content = SingleChildScrollView(
      padding: EdgeInsets.fromLTRB(
        32,
        _surfaceStyle == TemplateSurfaceStyle.atmosphericWeather ? 36 : 28,
        32,
        32,
      ),
      child: GenUiSurface(host: _processor, surfaceId: previewSurfaceId),
    );

    if (_surfaceStyle == TemplateSurfaceStyle.atmosphericWeather) {
      return _WeatherSurfaceShell(child: content);
    }

    if (_surfaceStyle == TemplateSurfaceStyle.newsPanel) {
      return _NewsSurfaceShell(child: content);
    }

    if (_surfaceStyle == TemplateSurfaceStyle.schedulePanel) {
      return _ScheduleSurfaceShell(child: content);
    }

    return Material(
      color: const Color(0xFF12212D),
      borderRadius: BorderRadius.circular(32),
      child: content,
    );
  }
}

class _WeatherSurfaceShell extends StatelessWidget {
  const _WeatherSurfaceShell({required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) {
    final baseTheme = Theme.of(context);
    final weatherTheme = _buildWeatherTheme(baseTheme);

    return Theme(
      data: weatherTheme,
      child: DefaultTextStyle(
        style: weatherTheme.textTheme.bodyMedium!,
        child: Material(
          color: Colors.transparent,
          child: ClipRRect(
            borderRadius: BorderRadius.circular(32),
            child: DecoratedBox(
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(32),
                border: Border.all(color: Colors.white.withValues(alpha: 0.08)),
                gradient: const LinearGradient(
                  colors: [
                    Color(0xFF0D1621),
                    Color(0xFF132231),
                    Color(0xFF1A2938),
                  ],
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                ),
              ),
              child: Stack(
                children: [
                  const Positioned.fill(child: _WeatherBackdrop()),
                  Positioned.fill(child: child),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _NewsSurfaceShell extends StatelessWidget {
  const _NewsSurfaceShell({required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) {
    final baseTheme = Theme.of(context);
    final newsTheme = _buildNewsTheme(baseTheme);

    return Theme(
      data: newsTheme,
      child: DefaultTextStyle(
        style: newsTheme.textTheme.bodyMedium!,
        child: Material(
          color: Colors.transparent,
          child: ClipRRect(
            borderRadius: BorderRadius.circular(32),
            child: DecoratedBox(
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(32),
                border: Border.all(color: Colors.white.withValues(alpha: 0.08)),
                gradient: const LinearGradient(
                  colors: [
                    Color(0xFF0C1620),
                    Color(0xFF12202E),
                    Color(0xFF172635),
                  ],
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                ),
              ),
              child: Stack(
                children: [
                  const Positioned.fill(child: _NewsBackdrop()),
                  Positioned.fill(child: child),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _ScheduleSurfaceShell extends StatelessWidget {
  const _ScheduleSurfaceShell({required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) {
    final baseTheme = Theme.of(context);
    final scheduleTheme = _buildScheduleTheme(baseTheme);

    return Theme(
      data: scheduleTheme,
      child: DefaultTextStyle(
        style: scheduleTheme.textTheme.bodyMedium!,
        child: Material(
          color: Colors.transparent,
          child: ClipRRect(
            borderRadius: BorderRadius.circular(32),
            child: DecoratedBox(
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(32),
                border: Border.all(color: Colors.white.withValues(alpha: 0.08)),
                gradient: const LinearGradient(
                  colors: [
                    Color(0xFF0D1620),
                    Color(0xFF13202C),
                    Color(0xFF1A2934),
                  ],
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                ),
              ),
              child: Stack(
                children: [
                  const Positioned.fill(child: _ScheduleBackdrop()),
                  Positioned.fill(child: child),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

ThemeData _buildWeatherTheme(ThemeData baseTheme) {
  final baseTextTheme = baseTheme.textTheme;
  final colorScheme = baseTheme.colorScheme.copyWith(
    surface: const Color(0x662A3946),
    onSurface: const Color(0xFFF5F3EF),
    secondary: const Color(0xFFF0C782),
    outline: const Color(0xFF5A6D7E),
  );

  return baseTheme.copyWith(
    colorScheme: colorScheme,
    dividerColor: Colors.white.withValues(alpha: 0.08),
    cardTheme: baseTheme.cardTheme.copyWith(
      color: const Color(0x5A273543),
      margin: const EdgeInsets.symmetric(horizontal: 6, vertical: 8),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(26)),
    ),
    textTheme: baseTextTheme.copyWith(
      headlineLarge: GoogleFonts.nanumMyeongjo(
        textStyle: baseTextTheme.headlineLarge?.copyWith(
          color: colorScheme.onSurface,
          fontWeight: FontWeight.w700,
          height: 1.15,
          letterSpacing: -1.2,
        ),
      ),
      headlineMedium: GoogleFonts.nanumMyeongjo(
        textStyle: baseTextTheme.headlineMedium?.copyWith(
          color: colorScheme.onSurface,
          fontWeight: FontWeight.w700,
          height: 1.2,
          letterSpacing: -0.8,
        ),
      ),
      headlineSmall: GoogleFonts.nanumMyeongjo(
        textStyle: baseTextTheme.headlineSmall?.copyWith(
          color: colorScheme.onSurface,
          fontWeight: FontWeight.w700,
          height: 1.22,
          letterSpacing: -0.5,
        ),
      ),
      titleLarge: GoogleFonts.nanumMyeongjo(
        textStyle: baseTextTheme.titleLarge?.copyWith(
          color: colorScheme.onSurface,
          fontWeight: FontWeight.w700,
          height: 1.28,
        ),
      ),
      titleMedium: baseTextTheme.titleMedium?.copyWith(
        color: const Color(0xFFE1E7ED),
        fontWeight: FontWeight.w600,
        height: 1.35,
      ),
      bodyMedium: baseTextTheme.bodyMedium?.copyWith(
        color: const Color(0xDDEBF1F6),
        height: 1.68,
      ),
      bodySmall: baseTextTheme.bodySmall?.copyWith(
        color: const Color(0x99E7EDF3),
        letterSpacing: 0.3,
      ),
    ),
  );
}

ThemeData _buildNewsTheme(ThemeData baseTheme) {
  final baseTextTheme = baseTheme.textTheme;
  final colorScheme = baseTheme.colorScheme.copyWith(
    surface: const Color(0x66283846),
    onSurface: const Color(0xFFF4F2EE),
    outline: const Color(0xFF5C7082),
  );

  return baseTheme.copyWith(
    colorScheme: colorScheme,
    dividerColor: Colors.white.withValues(alpha: 0.08),
    cardTheme: baseTheme.cardTheme.copyWith(
      color: const Color(0x5A283846),
      margin: const EdgeInsets.symmetric(horizontal: 6, vertical: 8),
      elevation: 0,
      shadowColor: Colors.transparent,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(26)),
    ),
    textTheme: baseTextTheme.copyWith(
      headlineMedium: GoogleFonts.nanumMyeongjo(
        textStyle: baseTextTheme.headlineMedium?.copyWith(
          color: colorScheme.onSurface,
          fontWeight: FontWeight.w700,
          height: 1.18,
          letterSpacing: -0.8,
        ),
      ),
      headlineSmall: baseTextTheme.headlineSmall?.copyWith(
        color: colorScheme.onSurface,
        fontWeight: FontWeight.w700,
        height: 1.2,
      ),
      titleLarge: baseTextTheme.titleLarge?.copyWith(
        color: const Color(0xFFE7EDF3),
        fontWeight: FontWeight.w700,
        height: 1.26,
      ),
      titleMedium: baseTextTheme.titleMedium?.copyWith(
        color: const Color(0xFFE0E6EC),
        fontWeight: FontWeight.w600,
        height: 1.34,
      ),
      bodyMedium: baseTextTheme.bodyMedium?.copyWith(
        color: const Color(0xDCEAF0F5),
        height: 1.62,
      ),
      bodySmall: baseTextTheme.bodySmall?.copyWith(
        color: const Color(0x99E6ECF2),
        letterSpacing: 0.3,
      ),
    ),
  );
}

ThemeData _buildScheduleTheme(ThemeData baseTheme) {
  final baseTextTheme = baseTheme.textTheme;
  final colorScheme = baseTheme.colorScheme.copyWith(
    surface: const Color(0x6630404C),
    onSurface: const Color(0xFFF2F5F7),
    outline: const Color(0xFF5C7181),
  );

  return baseTheme.copyWith(
    colorScheme: colorScheme,
    dividerColor: Colors.white.withValues(alpha: 0.08),
    cardTheme: baseTheme.cardTheme.copyWith(
      color: const Color(0x5D2E3F4C),
      margin: const EdgeInsets.symmetric(horizontal: 6, vertical: 8),
      elevation: 0,
      shadowColor: Colors.transparent,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(26),
        side: BorderSide(color: Colors.white.withValues(alpha: 0.05)),
      ),
    ),
    textTheme: baseTextTheme.copyWith(
      headlineSmall: baseTextTheme.headlineSmall?.copyWith(
        color: colorScheme.onSurface,
        fontWeight: FontWeight.w700,
        height: 1.2,
        letterSpacing: -0.4,
      ),
      titleLarge: baseTextTheme.titleLarge?.copyWith(
        color: const Color(0xFFE8EDF1),
        fontWeight: FontWeight.w700,
        height: 1.28,
      ),
      titleMedium: baseTextTheme.titleMedium?.copyWith(
        color: const Color(0xFFDEE5EA),
        fontWeight: FontWeight.w600,
        height: 1.34,
      ),
      bodyMedium: baseTextTheme.bodyMedium?.copyWith(
        color: const Color(0xDCE9EFF3),
        height: 1.58,
      ),
      bodySmall: baseTextTheme.bodySmall?.copyWith(
        color: const Color(0x99E3EAF0),
        letterSpacing: 0.3,
      ),
    ),
  );
}

class _NewsBackdrop extends StatelessWidget {
  const _NewsBackdrop();

  @override
  Widget build(BuildContext context) {
    return IgnorePointer(
      child: Stack(
        children: [
          Positioned.fill(
            child: DecoratedBox(
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  colors: [
                    const Color(0xFFD9B884).withValues(alpha: 0.02),
                    Colors.transparent,
                    const Color(0xFF7CA0BD).withValues(alpha: 0.05),
                  ],
                  begin: Alignment.topRight,
                  end: Alignment.bottomLeft,
                ),
              ),
            ),
          ),
          Positioned(
            top: -22,
            right: 80,
            child: _WeatherGlow(
              size: 180,
              color: const Color(0xFFE0BE83),
              opacity: 0.10,
            ),
          ),
          Positioned(
            left: -48,
            bottom: 18,
            child: _WeatherGlow(
              size: 220,
              color: const Color(0xFF4F93B2),
              opacity: 0.08,
            ),
          ),
          Positioned(
            top: 22,
            left: 28,
            right: 28,
            child: Container(
              height: 1,
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  colors: [
                    Colors.transparent,
                    Colors.white.withValues(alpha: 0.18),
                    Colors.transparent,
                  ],
                ),
              ),
            ),
          ),
          Positioned.fill(
            child: Padding(
              padding: const EdgeInsets.all(18),
              child: DecoratedBox(
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(26),
                  border: Border.all(
                    color: Colors.white.withValues(alpha: 0.04),
                  ),
                  gradient: LinearGradient(
                    colors: [
                      Colors.white.withValues(alpha: 0.02),
                      Colors.white.withValues(alpha: 0.004),
                    ],
                    begin: Alignment.topCenter,
                    end: Alignment.bottomCenter,
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _ScheduleBackdrop extends StatelessWidget {
  const _ScheduleBackdrop();

  @override
  Widget build(BuildContext context) {
    return IgnorePointer(
      child: Stack(
        children: [
          Positioned.fill(
            child: DecoratedBox(
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  colors: [
                    const Color(0xFFE0BE85).withValues(alpha: 0.02),
                    Colors.transparent,
                    const Color(0xFF7CA0B9).withValues(alpha: 0.04),
                  ],
                  begin: Alignment.topRight,
                  end: Alignment.bottomLeft,
                ),
              ),
            ),
          ),
          Positioned(
            top: -18,
            right: 76,
            child: _WeatherGlow(
              size: 170,
              color: const Color(0xFFDFC189),
              opacity: 0.08,
            ),
          ),
          Positioned(
            left: -44,
            bottom: 22,
            child: _WeatherGlow(
              size: 220,
              color: const Color(0xFF4E8DAA),
              opacity: 0.07,
            ),
          ),
          Positioned(
            left: 56,
            right: 56,
            top: 28,
            child: Container(
              height: 1,
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  colors: [
                    Colors.transparent,
                    Colors.white.withValues(alpha: 0.16),
                    Colors.transparent,
                  ],
                ),
              ),
            ),
          ),
          Positioned.fill(
            child: Padding(
              padding: const EdgeInsets.all(18),
              child: DecoratedBox(
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(26),
                  border: Border.all(
                    color: Colors.white.withValues(alpha: 0.04),
                  ),
                  gradient: LinearGradient(
                    colors: [
                      Colors.white.withValues(alpha: 0.015),
                      Colors.white.withValues(alpha: 0.004),
                    ],
                    begin: Alignment.topCenter,
                    end: Alignment.bottomCenter,
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _WeatherBackdrop extends StatelessWidget {
  const _WeatherBackdrop();

  @override
  Widget build(BuildContext context) {
    return IgnorePointer(
      child: Stack(
        children: [
          Positioned.fill(
            child: DecoratedBox(
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  colors: [
                    const Color(0xFFF3C980).withValues(alpha: 0.05),
                    Colors.transparent,
                    const Color(0xFF85B6D7).withValues(alpha: 0.07),
                  ],
                  begin: Alignment.topRight,
                  end: Alignment.bottomLeft,
                ),
              ),
            ),
          ),
          Positioned(
            top: -40,
            right: 72,
            child: _WeatherGlow(
              size: 220,
              color: const Color(0xFFF1C67B),
              opacity: 0.18,
            ),
          ),
          Positioned(
            left: -60,
            bottom: 26,
            child: _WeatherGlow(
              size: 280,
              color: const Color(0xFF4A98B9),
              opacity: 0.14,
            ),
          ),
          Positioned(
            left: 56,
            right: 56,
            top: 28,
            child: Container(
              height: 1,
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  colors: [
                    Colors.transparent,
                    Colors.white.withValues(alpha: 0.20),
                    Colors.transparent,
                  ],
                ),
              ),
            ),
          ),
          Positioned.fill(
            child: Padding(
              padding: const EdgeInsets.all(18),
              child: DecoratedBox(
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(26),
                  border: Border.all(
                    color: Colors.white.withValues(alpha: 0.04),
                  ),
                  gradient: LinearGradient(
                    colors: [
                      Colors.white.withValues(alpha: 0.02),
                      Colors.white.withValues(alpha: 0.005),
                    ],
                    begin: Alignment.topCenter,
                    end: Alignment.bottomCenter,
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _WeatherGlow extends StatelessWidget {
  const _WeatherGlow({
    required this.size,
    required this.color,
    required this.opacity,
  });

  final double size;
  final Color color;
  final double opacity;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: size,
      height: size,
      decoration: BoxDecoration(
        shape: BoxShape.circle,
        gradient: RadialGradient(
          colors: [
            color.withValues(alpha: opacity),
            color.withValues(alpha: opacity * 0.35),
            Colors.transparent,
          ],
        ),
      ),
    );
  }
}
