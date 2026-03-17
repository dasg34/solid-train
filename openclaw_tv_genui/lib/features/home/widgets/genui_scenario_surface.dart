import 'package:flutter/material.dart';
import 'package:genui/genui.dart';

import '../../../core/a2ui/a2ui_payload_source.dart';
import '../../../core/a2ui/file_payload_source.dart';
import '../../../core/a2ui/surface_style.dart';
import '../../../core/logging/app_logger.dart';
import '../models/scenario_entry.dart';

// Skills emit this catalogId in their createSurface messages.
const _catalogId = 'https://a2ui.org/specification/v0_9/standard_catalog.json';
const _tvSurfaceTextScale = 0.65;

class GenUiScenarioSurface extends StatefulWidget {
  const GenUiScenarioSurface.scenario({
    required A2uiPayloadSource payloadSource,
    required ScenarioEntry scenario,
    super.key,
  }) : _payloadSource = payloadSource,
       _scenario = scenario,
       filePath = null;

  const GenUiScenarioSurface.file({required this.filePath, super.key})
    : _payloadSource = null,
      _scenario = null;

  final A2uiPayloadSource? _payloadSource;
  final ScenarioEntry? _scenario;
  final String? filePath;

  @override
  State<GenUiScenarioSurface> createState() => _GenUiScenarioSurfaceState();
}

class _GenUiScenarioSurfaceState extends State<GenUiScenarioSurface> {
  late SurfaceController _controller;
  int _loadId = 0;
  String _surfaceId = 'main';
  SurfaceStyle _style = SurfaceStyle.standard;
  bool _hasError = false;
  String? _errorDetail;

  @override
  void initState() {
    super.initState();
    _controller = _createController();
    AppLogger.debug(
      'surface',
      'Initializing surface for ${_activeTargetDescription()}',
    );
    _loadScenario();
  }

  SurfaceController _createController() => SurfaceController(
    catalogs: [
      Catalog([
        BasicCatalogItems.button,
        BasicCatalogItems.card,
        BasicCatalogItems.column,
        BasicCatalogItems.divider,
        BasicCatalogItems.icon,
        BasicCatalogItems.row,
        BasicCatalogItems.text,
      ], catalogId: _catalogId),
    ],
  );

  @override
  void didUpdateWidget(covariant GenUiScenarioSurface oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.filePath != widget.filePath ||
        oldWidget._scenario?.id != widget._scenario?.id) {
      AppLogger.debug(
        'surface',
        'Surface target changed to ${_activeTargetDescription()}',
      );
      _loadScenario();
    }
  }

  Future<void> _loadScenario() async {
    final loadId = ++_loadId;
    final target = _activeTargetDescription();
    AppLogger.debug('surface', 'Starting load #$loadId for $target');

    // Dispose old controller and create a fresh one to avoid accumulating
    // surfaces from previous scenarios.
    _controller.dispose();
    _controller = _createController();

    try {
      final List<A2uiMessage> messages;

      if (widget.filePath != null) {
        messages = await FilePayloadSource().loadFile(widget.filePath!);
      } else {
        messages = await widget._payloadSource!.load(widget._scenario!.id);
      }

      if (!mounted || loadId != _loadId) {
        AppLogger.debug('surface', 'Ignoring stale load #$loadId for $target');
        return;
      }

      // Resolve surfaceId and style from the CreateSurface message.
      final createMsg = messages.whereType<CreateSurface>().firstOrNull;
      if (createMsg == null) {
        AppLogger.warn(
          'surface',
          'No createSurface message found for $target. Falling back to surfaceId resolution.',
        );
      }
      final surfaceId =
          createMsg?.surfaceId ?? widget._scenario?.surfaceId ?? 'main';
      final newStyle = resolveSurfaceStyle(surfaceId);

      setState(() {
        _surfaceId = surfaceId;
        _style = newStyle;
        _hasError = false;
        _errorDetail = null;
      });

      AppLogger.info(
        'surface',
        'Applying ${messages.length} messages to surfaceId=$surfaceId '
            'style=${newStyle.name} for $target',
      );
      for (final message in messages) {
        _controller.handleMessage(message);
      }
    } catch (error, stackTrace) {
      if (!mounted || loadId != _loadId) {
        AppLogger.debug(
          'surface',
          'Error arrived for stale load #$loadId for $target',
          error: error,
          stackTrace: stackTrace,
        );
        return;
      }
      AppLogger.error(
        'surface',
        'Failed to load surface content for $target. '
            'reason=${_summarizeLoadError(error)}',
        error: error,
        stackTrace: stackTrace,
      );
      setState(() {
        _hasError = true;
        _errorDetail = _userFacingErrorDetail(error);
      });
    }
  }

  String _activeTargetDescription() {
    if (widget.filePath != null) {
      return 'file:${widget.filePath}';
    }
    return 'scenario:${widget._scenario?.id ?? "unknown"}';
  }

  String _summarizeLoadError(Object error) {
    if (error is FormatException) {
      return 'invalid_a2ui_format:${error.message}';
    }
    return '${error.runtimeType}:$error';
  }

  String _userFacingErrorDetail(Object error) {
    if (error is FormatException) {
      return 'A2UI NDJSON 포맷이 아니거나 메시지 구조가 올바르지 않습니다.';
    }
    return '로그에서 상세 오류를 확인해 주세요.';
  }

  @override
  Widget build(BuildContext context) {
    if (_hasError) {
      final title = widget.filePath != null
          ? '외부 A2UI 파일을 불러올 수 없습니다.'
          : '시나리오를 불러올 수 없습니다.';
      final detail = widget.filePath != null
          ? _errorDetail ?? '전달한 파일이 A2UI NDJSON인지 확인해 주세요.'
          : '데이터 또는 템플릿 상태를 확인해 주세요.';

      return Center(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Text(title, textAlign: TextAlign.center),
              const SizedBox(height: 12),
              Text(
                detail,
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.bodyMedium,
              ),
            ],
          ),
        ),
      );
    }

    final content = MediaQuery(
      data: MediaQuery.of(
        context,
      ).copyWith(textScaler: const TextScaler.linear(_tvSurfaceTextScale)),
      child: SingleChildScrollView(
        padding: EdgeInsets.fromLTRB(
          32,
          _style == SurfaceStyle.atmosphericWeather ? 36 : 28,
          32,
          32,
        ),
        child: Surface(surfaceContext: _controller.contextFor(_surfaceId)),
      ),
    );
    final overlayContent = _OverlaySurfaceFrame(
      pattern: _resolvedPattern(),
      style: _style,
      child: content,
    );

    return switch (_style) {
      SurfaceStyle.atmosphericWeather => _WeatherSurfaceShell(
        child: overlayContent,
      ),
      SurfaceStyle.newsPanel => _NewsSurfaceShell(child: overlayContent),
      SurfaceStyle.schedulePanel => _ScheduleSurfaceShell(
        child: overlayContent,
      ),
      SurfaceStyle.standard => _StandardSurfaceShell(child: overlayContent),
    };
  }

  TvSurfacePattern _resolvedPattern() {
    return widget._scenario?.pattern ?? _patternForSurfaceId(_surfaceId);
  }

  @override
  void dispose() {
    AppLogger.debug(
      'surface',
      'Disposing surface controller for surfaceId=$_surfaceId',
    );
    _controller.dispose();
    super.dispose();
  }
}

TvSurfacePattern _patternForSurfaceId(String surfaceId) {
  if (surfaceId.startsWith('news') ||
      surfaceId.startsWith('commute') ||
      surfaceId.startsWith('smart_home') ||
      surfaceId.startsWith('media')) {
    return TvSurfacePattern.sidePanel;
  }
  if (surfaceId.startsWith('schedule') ||
      surfaceId.startsWith('finance') ||
      surfaceId.startsWith('delivery') ||
      surfaceId.startsWith('wellness') ||
      surfaceId.startsWith('family') ||
      surfaceId.startsWith('shopping')) {
    return TvSurfacePattern.centerCard;
  }
  return TvSurfacePattern.immersive;
}

class _OverlaySurfaceFrame extends StatelessWidget {
  const _OverlaySurfaceFrame({
    required this.pattern,
    required this.style,
    required this.child,
  });

  final TvSurfacePattern pattern;
  final SurfaceStyle style;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    final spec = _OverlayLayoutSpec.resolve(
      size: MediaQuery.sizeOf(context),
      pattern: pattern,
    );

    return Stack(
      children: [
        SafeArea(
          child: Padding(
            padding: spec.margin,
            child: Align(
              alignment: spec.alignment,
              child: SizedBox(
                width: spec.width,
                height: spec.height,
                child: _GlassOverlayPanel(style: style, child: child),
              ),
            ),
          ),
        ),
      ],
    );
  }
}

class _OverlayLayoutSpec {
  const _OverlayLayoutSpec({
    required this.alignment,
    required this.width,
    required this.height,
    required this.margin,
  });

  final Alignment alignment;
  final double width;
  final double height;
  final EdgeInsets margin;

  static _OverlayLayoutSpec resolve({
    required Size size,
    required TvSurfacePattern pattern,
  }) {
    final smallScreen = size.width < 900;
    final compactTv =
        !smallScreen && (size.width <= 1280 || size.height <= 720);
    final margin = EdgeInsets.symmetric(
      horizontal: smallScreen ? 16 : (compactTv ? 20 : 28),
      vertical: smallScreen ? 16 : (compactTv ? 18 : 24),
    );
    final availableWidth = (size.width - margin.horizontal).clamp(
      320.0,
      4000.0,
    );
    final availableHeight = (size.height - margin.vertical).clamp(
      280.0,
      4000.0,
    );

    late final Alignment alignment;
    late final double widthFactor;
    late final double maxWidth;
    late final double heightFactor;

    switch (pattern) {
      case TvSurfacePattern.immersive:
        alignment = Alignment.center;
        widthFactor = smallScreen ? 0.92 : (compactTv ? 0.52 : 0.56);
        maxWidth = compactTv ? 620 : 920;
        heightFactor = smallScreen ? 0.84 : (compactTv ? 0.80 : 0.84);
      case TvSurfacePattern.sidePanel:
        alignment = smallScreen ? Alignment.center : Alignment.centerRight;
        widthFactor = smallScreen ? 0.88 : (compactTv ? 0.38 : 0.42);
        maxWidth = compactTv ? 440 : 700;
        heightFactor = smallScreen ? 0.82 : (compactTv ? 0.84 : 0.88);
      case TvSurfacePattern.centerCard:
        alignment = Alignment.center;
        widthFactor = smallScreen ? 0.86 : (compactTv ? 0.44 : 0.48);
        maxWidth = compactTv ? 520 : 780;
        heightFactor = smallScreen ? 0.74 : (compactTv ? 0.68 : 0.72);
    }

    final width = (availableWidth * widthFactor).clamp(360.0, maxWidth);
    final height = (availableHeight * heightFactor).clamp(
      320.0,
      availableHeight,
    );

    return _OverlayLayoutSpec(
      alignment: alignment,
      width: width,
      height: height,
      margin: margin,
    );
  }
}

class _GlassOverlayPanel extends StatelessWidget {
  const _GlassOverlayPanel({required this.style, required this.child});

  final SurfaceStyle style;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    return ClipRRect(
      borderRadius: BorderRadius.circular(34),
      child: DecoratedBox(
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(34),
          border: Border.all(color: Colors.white.withValues(alpha: 0.10)),
          gradient: LinearGradient(
            colors: [
              Colors.black.withValues(alpha: 0.88),
              Colors.black.withValues(alpha: 0.94),
            ],
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
          ),
          boxShadow: [
            BoxShadow(
              color: Colors.black.withValues(alpha: 0.34),
              blurRadius: 34,
              offset: const Offset(0, 18),
            ),
          ],
        ),
        child: Stack(
          children: [
            Positioned.fill(child: _PanelBackdrop(style: style)),
            Positioned.fill(
              child: DecoratedBox(
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(34),
                  gradient: LinearGradient(
                    colors: [
                      Colors.white.withValues(alpha: 0.02),
                      Colors.transparent,
                      Colors.black.withValues(alpha: 0.16),
                    ],
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                  ),
                ),
              ),
            ),
            Positioned.fill(child: child),
          ],
        ),
      ),
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
        child: Material(color: Colors.transparent, child: child),
      ),
    );
  }
}

class _StandardSurfaceShell extends StatelessWidget {
  const _StandardSurfaceShell({required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) {
    final baseTheme = Theme.of(context);

    return Theme(
      data: baseTheme,
      child: DefaultTextStyle(
        style: baseTheme.textTheme.bodyMedium!,
        child: Material(color: Colors.transparent, child: child),
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
        child: Material(color: Colors.transparent, child: child),
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
        child: Material(color: Colors.transparent, child: child),
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
      headlineLarge: baseTextTheme.headlineLarge?.copyWith(
        color: colorScheme.onSurface,
        fontWeight: FontWeight.w700,
        height: 1.15,
        letterSpacing: -1.2,
      ),
      headlineMedium: baseTextTheme.headlineMedium?.copyWith(
        color: colorScheme.onSurface,
        fontWeight: FontWeight.w700,
        height: 1.2,
        letterSpacing: -0.8,
      ),
      headlineSmall: baseTextTheme.headlineSmall?.copyWith(
        color: colorScheme.onSurface,
        fontWeight: FontWeight.w700,
        height: 1.22,
        letterSpacing: -0.5,
      ),
      titleLarge: baseTextTheme.titleLarge?.copyWith(
        color: colorScheme.onSurface,
        fontWeight: FontWeight.w700,
        height: 1.28,
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
      headlineMedium: baseTextTheme.headlineMedium?.copyWith(
        color: colorScheme.onSurface,
        fontWeight: FontWeight.w700,
        height: 1.18,
        letterSpacing: -0.8,
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

class _PanelBackdrop extends StatelessWidget {
  const _PanelBackdrop({required this.style});

  final SurfaceStyle style;

  @override
  Widget build(BuildContext context) {
    final config = switch (style) {
      SurfaceStyle.atmosphericWeather => const _PanelBackdropConfig(
        topGlowColor: Color(0xFFF1C67B),
        topGlowOpacity: 0.18,
        topGlowSize: 220,
        bottomGlowColor: Color(0xFF4A98B9),
        bottomGlowOpacity: 0.14,
        bottomGlowSize: 280,
        washStart: Color(0x0DF3C980),
        washEnd: Color(0x1285B6D7),
        lineOpacity: 0.20,
        innerStartOpacity: 0.02,
        innerEndOpacity: 0.005,
      ),
      SurfaceStyle.newsPanel => const _PanelBackdropConfig(
        topGlowColor: Color(0xFFE0BE83),
        topGlowOpacity: 0.10,
        topGlowSize: 180,
        bottomGlowColor: Color(0xFF4F93B2),
        bottomGlowOpacity: 0.08,
        bottomGlowSize: 220,
        washStart: Color(0x08D9B884),
        washEnd: Color(0x0D7CA0BD),
        lineOpacity: 0.18,
        innerStartOpacity: 0.02,
        innerEndOpacity: 0.004,
      ),
      SurfaceStyle.schedulePanel => const _PanelBackdropConfig(
        topGlowColor: Color(0xFFDFC189),
        topGlowOpacity: 0.08,
        topGlowSize: 170,
        bottomGlowColor: Color(0xFF4E8DAA),
        bottomGlowOpacity: 0.07,
        bottomGlowSize: 220,
        washStart: Color(0x08E0BE85),
        washEnd: Color(0x0A7CA0B9),
        lineOpacity: 0.16,
        innerStartOpacity: 0.015,
        innerEndOpacity: 0.004,
      ),
      SurfaceStyle.standard => const _PanelBackdropConfig(
        topGlowColor: Color(0xFF6CB7D6),
        topGlowOpacity: 0.08,
        topGlowSize: 170,
        bottomGlowColor: Color(0xFFE3C388),
        bottomGlowOpacity: 0.07,
        bottomGlowSize: 220,
        washStart: Color(0x08203446),
        washEnd: Color(0x08E3C388),
        lineOpacity: 0.14,
        innerStartOpacity: 0.012,
        innerEndOpacity: 0.004,
      ),
    };

    return IgnorePointer(
      child: Stack(
        children: [
          Positioned.fill(
            child: DecoratedBox(
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  colors: [
                    config.washStart,
                    Colors.transparent,
                    config.washEnd,
                  ],
                  begin: Alignment.topRight,
                  end: Alignment.bottomLeft,
                ),
              ),
            ),
          ),
          Positioned(
            top: -28,
            right: 68,
            child: _WeatherGlow(
              size: config.topGlowSize,
              color: config.topGlowColor,
              opacity: config.topGlowOpacity,
            ),
          ),
          Positioned(
            left: -52,
            bottom: 24,
            child: _WeatherGlow(
              size: config.bottomGlowSize,
              color: config.bottomGlowColor,
              opacity: config.bottomGlowOpacity,
            ),
          ),
          Positioned(
            top: 24,
            left: 28,
            right: 28,
            child: Container(
              height: 1,
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  colors: [
                    Colors.transparent,
                    Colors.white.withValues(alpha: config.lineOpacity),
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
                  gradient: LinearGradient(
                    colors: [
                      Colors.white.withValues(alpha: config.innerStartOpacity),
                      Colors.white.withValues(alpha: config.innerEndOpacity),
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

class _PanelBackdropConfig {
  const _PanelBackdropConfig({
    required this.topGlowColor,
    required this.topGlowOpacity,
    required this.topGlowSize,
    required this.bottomGlowColor,
    required this.bottomGlowOpacity,
    required this.bottomGlowSize,
    required this.washStart,
    required this.washEnd,
    required this.lineOpacity,
    required this.innerStartOpacity,
    required this.innerEndOpacity,
  });

  final Color topGlowColor;
  final double topGlowOpacity;
  final double topGlowSize;
  final Color bottomGlowColor;
  final double bottomGlowOpacity;
  final double bottomGlowSize;
  final Color washStart;
  final Color washEnd;
  final double lineOpacity;
  final double innerStartOpacity;
  final double innerEndOpacity;
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
