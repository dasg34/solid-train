import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:genui/genui.dart';

import '../../../core/a2ui/a2ui_payload_source.dart';
import '../../../core/a2ui/chart_catalog_items.dart';
import '../../../core/a2ui/layout_catalog_items.dart';
import '../../../core/a2ui/surface_style.dart';
import '../../../core/logging/app_logger.dart';
import '../../../core/presentation/presentation_file_payload_source.dart';
import '../../../core/presentation/presentation_payload_decoder.dart';
import '../models/scenario_entry.dart';

// Skills emit this catalogId in their createSurface messages.
const _catalogId = 'https://a2ui.org/specification/v0_9/standard_catalog.json';
const _keyboardScrollStepMin = 88.0;
const _keyboardScrollStepMax = 168.0;
const _keyboardScrollStepViewportFactor = 0.22;
const _keyboardRepeatStepFactor = 0.28;
const _keyboardScrollDuration = Duration(milliseconds: 240);

class GenUiScenarioSurface extends StatefulWidget {
  const GenUiScenarioSurface.scenario({
    required A2uiPayloadSource payloadSource,
    required ScenarioEntry scenario,
    super.key,
  }) : _payloadSource = payloadSource,
       _scenario = scenario,
       presentationFilePath = null,
       presentationJson = null;

  const GenUiScenarioSurface.presentationFile({
    required this.presentationFilePath,
    super.key,
  })
    : _payloadSource = null,
      _scenario = null,
      presentationJson = null;

  const GenUiScenarioSurface.presentationRaw({
    required this.presentationJson,
    super.key,
  })
    : _payloadSource = null,
      _scenario = null,
      presentationFilePath = null;

  final A2uiPayloadSource? _payloadSource;
  final ScenarioEntry? _scenario;
  final String? presentationFilePath;
  final String? presentationJson;

  @override
  State<GenUiScenarioSurface> createState() => _GenUiScenarioSurfaceState();
}

class _GenUiScenarioSurfaceState extends State<GenUiScenarioSurface> {
  late SurfaceController _controller;
  final ScrollController _scrollController = ScrollController();
  final FocusNode _scrollFocusNode = FocusNode(
    debugLabel: 'genui_surface_scroll',
  );
  int _loadId = 0;
  String _surfaceId = 'surface';
  SurfaceStyle _style = SurfaceStyle.standard;
  TvSurfacePattern _pattern = TvSurfacePattern.immersive;
  TvSurfaceScale _scale = TvSurfaceScale.standard;
  Size? _measuredContentSize;
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
    _scheduleScrollFocus();
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
        inset,
        balancedWrap,
        masonryFlow,
        flowingWrap,
        lineChart,
        barChart,
      ], catalogId: _catalogId),
    ],
  );

  @override
  void didUpdateWidget(covariant GenUiScenarioSurface oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.presentationFilePath != widget.presentationFilePath ||
        oldWidget.presentationJson != widget.presentationJson ||
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

      if (widget.presentationJson != null) {
        messages = decodePresentationMessages(
          widget.presentationJson!,
          sourceLabel: 'app control json',
        );
      } else if (widget.presentationFilePath != null) {
        messages = await PresentationFilePayloadSource().loadFile(
          widget.presentationFilePath!,
        );
      } else {
        messages = await widget._payloadSource!.load(widget._scenario!.id);
      }

      if (!mounted || loadId != _loadId) {
        AppLogger.debug('surface', 'Ignoring stale load #$loadId for $target');
        return;
      }

      // Resolve surfaceId, domain, and pattern from the CreateSurface message.
      final createMsg = messages.whereType<CreateSurface>().firstOrNull;
      if (createMsg == null) {
        AppLogger.warn(
          'surface',
          'No createSurface message found for $target. Falling back to scenario metadata.',
        );
      }
      final surfaceId =
          createMsg?.surfaceId ??
          widget._scenario?.surfaceId ??
          widget._scenario?.id ??
          'surface';
      final presentation = _resolvePresentation(createMsg, messages);

      setState(() {
        _surfaceId = surfaceId;
        _style = presentation.style;
        _pattern = presentation.pattern;
        _scale = presentation.scale;
        _measuredContentSize = null;
        _hasError = false;
        _errorDetail = null;
      });

      AppLogger.info(
        'surface',
        'Applying ${messages.length} messages to surfaceId=$surfaceId '
            'domain=${presentation.domain} '
            'pattern=${presentation.pattern.name} '
            'scale=${presentation.scale.name} '
            'style=${presentation.style.name} for $target',
      );
      for (final message in messages) {
        _controller.handleMessage(message);
      }
      _scheduleScrollFocus();
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
    if (widget.presentationJson != null) {
      return 'presentation-json:${widget.presentationJson!.length}chars';
    }
    if (widget.presentationFilePath != null) {
      return 'presentation-file:${widget.presentationFilePath}';
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
      return error.message;
    }
    return '로그에서 상세 오류를 확인해 주세요.';
  }

  void _scheduleScrollFocus() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted || !_scrollFocusNode.canRequestFocus) {
        return;
      }
      if (_scrollFocusNode.hasFocus) {
        return;
      }
      _scrollFocusNode.requestFocus();
    });
  }

  KeyEventResult _handleScrollKey(FocusNode node, KeyEvent event) {
    if (event is! KeyDownEvent && event is! KeyRepeatEvent) {
      return KeyEventResult.ignored;
    }
    final isRepeat = event is KeyRepeatEvent;

    final baseDelta = switch (event.logicalKey) {
      LogicalKeyboardKey.arrowDown => _scrollStep(),
      LogicalKeyboardKey.arrowUp => -_scrollStep(),
      LogicalKeyboardKey.pageDown => _scrollStep() * 1.4,
      LogicalKeyboardKey.pageUp => -_scrollStep() * 1.4,
      _ => null,
    };
    if (baseDelta == null) {
      return KeyEventResult.ignored;
    }
    final delta = isRepeat ? baseDelta * _keyboardRepeatStepFactor : baseDelta;

    _scrollBy(delta, animate: !isRepeat);
    return KeyEventResult.handled;
  }

  double _scrollStep() {
    if (!_scrollController.hasClients) {
      return 240;
    }
    final viewport = _scrollController.position.viewportDimension;
    return (viewport * _keyboardScrollStepViewportFactor).clamp(
      _keyboardScrollStepMin,
      _keyboardScrollStepMax,
    );
  }

  void _scrollBy(double delta, {required bool animate}) {
    if (!_scrollController.hasClients) {
      return;
    }
    final position = _scrollController.position;
    if (position.maxScrollExtent <= position.minScrollExtent) {
      return;
    }

    final target = (position.pixels + delta).clamp(
      position.minScrollExtent,
      position.maxScrollExtent,
    );
    if ((target - position.pixels).abs() < 1) {
      return;
    }

    if (!animate) {
      _scrollController.jumpTo(target);
      return;
    }

    _scrollController.animateTo(
      target,
      duration: _keyboardScrollDuration,
      curve: Curves.easeInOutCubicEmphasized,
    );
  }

  @override
  Widget build(BuildContext context) {
    if (_hasError) {
      final title = widget.presentationJson != null
          ? '외부 presentation JSON을 불러올 수 없습니다.'
          : widget.presentationFilePath != null
          ? '외부 presentation 파일을 불러올 수 없습니다.'
          : '시나리오를 불러올 수 없습니다.';
      final detail = widget.presentationJson != null
          ? _errorDetail ?? '전달한 App Control JSON이 presentation 객체인지 확인해 주세요.'
          : widget.presentationFilePath != null
          ? _errorDetail ?? '전달한 파일이 presentation JSON인지 확인해 주세요.'
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

    final contentPadding = EdgeInsets.fromLTRB(
      _scale == TvSurfaceScale.compact ? 24 : 32,
      _style == SurfaceStyle.atmosphericWeather
          ? (_scale == TvSurfaceScale.compact ? 30 : 36)
          : (_scale == TvSurfaceScale.compact ? 24 : 28),
      _scale == TvSurfaceScale.compact ? 24 : 32,
      _scale == TvSurfaceScale.compact ? 24 : 32,
    );
    final contentAlignment = _surfaceContentAlignment(
      pattern: _pattern,
      scale: _scale,
    );
    final spec = _OverlayLayoutSpec.resolve(
      size: MediaQuery.sizeOf(context),
      pattern: _pattern,
      scale: _scale,
    );
    final fillContentWidth = !spec.fitWidthToContent;
    final content = LayoutBuilder(
      builder: (context, constraints) {
        final contentWidth =
            (constraints.maxWidth - contentPadding.horizontal).clamp(
              0.0,
              4000.0,
            );
        final contentHeight =
            (constraints.maxHeight - contentPadding.vertical).clamp(
              0.0,
              4000.0,
            );

        return SingleChildScrollView(
          controller: _scrollController,
          padding: contentPadding,
          child: ConstrainedBox(
            constraints: BoxConstraints(
              minWidth: contentWidth,
              minHeight: contentHeight,
            ),
            child: Align(
              alignment: contentAlignment,
              child: ConstrainedBox(
                constraints: BoxConstraints(
                  minWidth: fillContentWidth ? contentWidth : 0,
                  maxWidth: contentWidth,
                ),
                child: Surface(
                  surfaceContext: _controller.contextFor(_surfaceId),
                ),
              ),
            ),
          ),
        );
      },
    );
    final overlayContent = Stack(
      children: [
        _OverlaySurfaceFrame(
          spec: spec,
          style: _style,
          contentPadding: contentPadding,
          measuredContentSize: _measuredContentSize,
          child: content,
        ),
        Positioned.fill(
          child: IgnorePointer(
            child: _OffstageSurfaceMeasurer(
              spec: spec,
              contentPadding: contentPadding,
              contentAlignment: contentAlignment,
              onMeasured: _handleMeasuredContentSize,
              child: Surface(
                surfaceContext: _controller.contextFor(_surfaceId),
              ),
            ),
          ),
        ),
      ],
    );

    return switch (_style) {
      SurfaceStyle.atmosphericWeather => Focus(
        autofocus: true,
        focusNode: _scrollFocusNode,
        onKeyEvent: _handleScrollKey,
        child: _WeatherSurfaceShell(child: overlayContent),
      ),
      SurfaceStyle.newsPanel => Focus(
        autofocus: true,
        focusNode: _scrollFocusNode,
        onKeyEvent: _handleScrollKey,
        child: _NewsSurfaceShell(child: overlayContent),
      ),
      SurfaceStyle.schedulePanel => Focus(
        autofocus: true,
        focusNode: _scrollFocusNode,
        onKeyEvent: _handleScrollKey,
        child: _ScheduleSurfaceShell(child: overlayContent),
      ),
      SurfaceStyle.standard => Focus(
        autofocus: true,
        focusNode: _scrollFocusNode,
        onKeyEvent: _handleScrollKey,
        child: _StandardSurfaceShell(child: overlayContent),
      ),
    };
  }

  void _handleMeasuredContentSize(Size size) {
    if (!mounted) {
      return;
    }
    final next = Size(
      size.width.clamp(0.0, 4000.0),
      size.height.clamp(0.0, 4000.0),
    );
    final current = _measuredContentSize;
    if (current != null &&
        (current.width - next.width).abs() < 1 &&
        (current.height - next.height).abs() < 1) {
      return;
    }
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) {
        return;
      }
      setState(() {
        _measuredContentSize = next;
      });
    });
  }

  _SurfacePresentation _resolvePresentation(
    CreateSurface? createMsg,
    List<A2uiMessage> messages,
  ) {
    final JsonMap? theme = createMsg?.theme;
    final String? domain =
        _themeString(theme, 'domain') ?? widget._scenario?.domain;
    final TvSurfacePattern? pattern =
        _themePattern(theme?['pattern']) ?? widget._scenario?.pattern;
    final TvSurfaceScale? themeScale =
        _themeScale(theme?['scale']) ?? _themeScale(theme?['size']);

    if (domain == null || domain.isEmpty) {
      throw const FormatException('createSurface.theme.domain 이 필요합니다.');
    }
    if (pattern == null) {
      throw const FormatException('createSurface.theme.pattern 이 필요합니다.');
    }

    final componentCount = _componentCount(messages);
    final scale =
        themeScale ??
        _autoSurfaceScale(pattern: pattern, componentCount: componentCount);

    return _SurfacePresentation(
      domain: domain,
      pattern: pattern,
      scale: scale,
      style: resolveSurfaceStyle(domain),
    );
  }

  @override
  void dispose() {
    AppLogger.debug(
      'surface',
      'Disposing surface controller for surfaceId=$_surfaceId',
    );
    _scrollController.dispose();
    _scrollFocusNode.dispose();
    _controller.dispose();
    super.dispose();
  }
}

String? _themeString(JsonMap? theme, String key) {
  final value = theme?[key];
  if (value is String && value.trim().isNotEmpty) {
    return value.trim();
  }
  return null;
}

TvSurfacePattern? _themePattern(Object? rawPattern) {
  return switch (rawPattern) {
    'immersive' || 'full' || 'fullscreen' => TvSurfacePattern.immersive,
    'sidePanel' || 'side_panel' || 'side-panel' => TvSurfacePattern.sidePanel,
    'centerCard' ||
    'center_card' ||
    'center-card' => TvSurfacePattern.centerCard,
    'topBanner' || 'top_banner' || 'top-banner' => TvSurfacePattern.topBanner,
    'bottomRibbon' ||
    'bottom_ribbon' ||
    'bottom-ribbon' => TvSurfacePattern.bottomRibbon,
    _ => null,
  };
}

class _SurfacePresentation {
  const _SurfacePresentation({
    required this.domain,
    required this.pattern,
    required this.scale,
    required this.style,
  });

  final String domain;
  final TvSurfacePattern pattern;
  final TvSurfaceScale scale;
  final SurfaceStyle style;
}

class _OverlaySurfaceFrame extends StatelessWidget {
  const _OverlaySurfaceFrame({
    required this.spec,
    required this.style,
    required this.contentPadding,
    required this.measuredContentSize,
    required this.child,
  });

  final _OverlayLayoutSpec spec;
  final SurfaceStyle style;
  final EdgeInsets contentPadding;
  final Size? measuredContentSize;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    final fittedWidth = !spec.fitWidthToContent || measuredContentSize == null
        ? spec.width
        : (measuredContentSize!.width + contentPadding.horizontal).clamp(
            spec.minWidth,
            spec.maxWidth,
          );
    final fittedHeight = measuredContentSize == null
        ? spec.height
        : (measuredContentSize!.height + contentPadding.vertical).clamp(
            spec.minHeight,
            spec.maxHeight,
          );

    return Stack(
      children: [
        SafeArea(
          child: Padding(
            padding: spec.margin,
            child: Align(
              alignment: spec.alignment,
              child: AnimatedContainer(
                duration: const Duration(milliseconds: 220),
                curve: Curves.easeOutCubic,
                width: fittedWidth,
                height: fittedHeight,
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
    required this.fitWidthToContent,
    required this.minWidth,
    required this.maxWidth,
    required this.minHeight,
    required this.maxHeight,
    required this.margin,
  });

  final Alignment alignment;
  final double width;
  final double height;
  final bool fitWidthToContent;
  final double minWidth;
  final double maxWidth;
  final double minHeight;
  final double maxHeight;
  final EdgeInsets margin;

  static _OverlayLayoutSpec resolve({
    required Size size,
    required TvSurfacePattern pattern,
    required TvSurfaceScale scale,
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
    late final double minWidth;
    late final double maxWidth;
    late final double heightFactor;
    late final double minHeight;
    late final bool fitWidthToContent;

    switch (pattern) {
      case TvSurfacePattern.immersive:
        alignment = Alignment.center;
        fitWidthToContent = true;
        switch (scale) {
          case TvSurfaceScale.compact:
            widthFactor = smallScreen ? 0.88 : (compactTv ? 0.50 : 0.56);
            minWidth = 420;
            maxWidth = compactTv ? 620 : 860;
            heightFactor = smallScreen ? 0.78 : (compactTv ? 0.82 : 0.80);
            minHeight = 280;
          case TvSurfaceScale.standard:
            widthFactor = smallScreen ? 0.94 : (compactTv ? 0.62 : 0.68);
            minWidth = 520;
            maxWidth = compactTv ? 720 : 1040;
            heightFactor = smallScreen ? 0.90 : (compactTv ? 0.96 : 0.94);
            minHeight = 320;
          case TvSurfaceScale.expanded:
            widthFactor = smallScreen ? 0.96 : (compactTv ? 0.72 : 0.78);
            minWidth = 620;
            maxWidth = compactTv ? 900 : 1280;
            heightFactor = smallScreen ? 0.92 : (compactTv ? 0.98 : 0.96);
            minHeight = 360;
        }
      case TvSurfacePattern.sidePanel:
        alignment = smallScreen ? Alignment.center : Alignment.centerRight;
        fitWidthToContent = false;
        switch (scale) {
          case TvSurfaceScale.compact:
            widthFactor = smallScreen ? 0.78 : (compactTv ? 0.30 : 0.34);
            minWidth = 280;
            maxWidth = compactTv ? 360 : 540;
            heightFactor = smallScreen ? 0.74 : (compactTv ? 0.74 : 0.72);
            minHeight = 260;
          case TvSurfaceScale.standard:
            widthFactor = smallScreen ? 0.88 : 0.42;
            minWidth = 320;
            maxWidth = compactTv ? 520 : 700;
            heightFactor = smallScreen ? 0.88 : (compactTv ? 0.94 : 0.94);
            minHeight = 320;
          case TvSurfaceScale.expanded:
            widthFactor = smallScreen ? 0.92 : (compactTv ? 0.44 : 0.48);
            minWidth = 360;
            maxWidth = compactTv ? 520 : 780;
            heightFactor = smallScreen ? 0.92 : (compactTv ? 0.96 : 0.96);
            minHeight = 360;
        }
      case TvSurfacePattern.centerCard:
        alignment = Alignment.center;
        fitWidthToContent = true;
        switch (scale) {
          case TvSurfaceScale.compact:
            widthFactor = smallScreen ? 0.72 : (compactTv ? 0.28 : 0.32);
            minWidth = 260;
            maxWidth = compactTv ? 340 : 500;
            heightFactor = smallScreen ? 0.58 : (compactTv ? 0.58 : 0.56);
            minHeight = 220;
          case TvSurfaceScale.standard:
            widthFactor = smallScreen ? 0.82 : (compactTv ? 0.36 : 0.40);
            minWidth = 320;
            maxWidth = compactTv ? 440 : 640;
            heightFactor = smallScreen ? 0.76 : (compactTv ? 0.80 : 0.80);
            minHeight = 320;
          case TvSurfaceScale.expanded:
            widthFactor = smallScreen ? 0.88 : (compactTv ? 0.42 : 0.46);
            minWidth = 360;
            maxWidth = compactTv ? 520 : 760;
            heightFactor = smallScreen ? 0.84 : (compactTv ? 0.86 : 0.84);
            minHeight = 360;
        }
      case TvSurfacePattern.topBanner:
        alignment = Alignment.topCenter;
        fitWidthToContent = true;
        switch (scale) {
          case TvSurfaceScale.compact:
            widthFactor = smallScreen ? 0.88 : (compactTv ? 0.82 : 0.84);
            minWidth = 520;
            maxWidth = compactTv ? 900 : 1120;
            heightFactor = smallScreen ? 0.22 : (compactTv ? 0.18 : 0.18);
            minHeight = 140;
          case TvSurfaceScale.standard:
            widthFactor = smallScreen ? 0.94 : (compactTv ? 0.90 : 0.92);
            minWidth = 640;
            maxWidth = compactTv ? 1080 : 1400;
            heightFactor = smallScreen ? 0.30 : (compactTv ? 0.26 : 0.24);
            minHeight = 170;
          case TvSurfaceScale.expanded:
            widthFactor = smallScreen ? 0.96 : (compactTv ? 0.94 : 0.96);
            minWidth = 760;
            maxWidth = compactTv ? 1180 : 1520;
            heightFactor = smallScreen ? 0.34 : (compactTv ? 0.30 : 0.28);
            minHeight = 200;
        }
      case TvSurfacePattern.bottomRibbon:
        alignment = Alignment.bottomCenter;
        fitWidthToContent = true;
        switch (scale) {
          case TvSurfaceScale.compact:
            widthFactor = smallScreen ? 0.88 : (compactTv ? 0.84 : 0.86);
            minWidth = 560;
            maxWidth = compactTv ? 920 : 1180;
            heightFactor = smallScreen ? 0.24 : (compactTv ? 0.24 : 0.22);
            minHeight = 150;
          case TvSurfaceScale.standard:
            widthFactor = smallScreen ? 0.94 : (compactTv ? 0.90 : 0.92);
            minWidth = 720;
            maxWidth = compactTv ? 1080 : 1440;
            heightFactor = smallScreen ? 0.34 : (compactTv ? 0.34 : 0.32);
            minHeight = 200;
          case TvSurfaceScale.expanded:
            widthFactor = smallScreen ? 0.96 : (compactTv ? 0.94 : 0.96);
            minWidth = 820;
            maxWidth = compactTv ? 1180 : 1560;
            heightFactor = smallScreen ? 0.40 : (compactTv ? 0.38 : 0.36);
            minHeight = 230;
        }
    }

    final width = (availableWidth * widthFactor).clamp(minWidth, maxWidth);
    final height = (availableHeight * heightFactor).clamp(
      minHeight,
      availableHeight,
    );

    return _OverlayLayoutSpec(
      alignment: alignment,
      width: width,
      height: height,
      fitWidthToContent: fitWidthToContent,
      minWidth: minWidth,
      maxWidth: maxWidth,
      minHeight: minHeight,
      maxHeight: availableHeight,
      margin: margin,
    );
  }
}

class _OffstageSurfaceMeasurer extends StatelessWidget {
  const _OffstageSurfaceMeasurer({
    required this.spec,
    required this.contentPadding,
    required this.contentAlignment,
    required this.onMeasured,
    required this.child,
  });

  final _OverlayLayoutSpec spec;
  final EdgeInsets contentPadding;
  final Alignment contentAlignment;
  final ValueChanged<Size> onMeasured;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    final measuredPanelWidth = spec.fitWidthToContent ? spec.maxWidth : spec.width;
    final maxContentWidth = (measuredPanelWidth - contentPadding.horizontal).clamp(
      0.0,
      measuredPanelWidth,
    );
    final maxContentHeight = (spec.maxHeight - contentPadding.vertical).clamp(
      0.0,
      spec.maxHeight,
    );

    return Offstage(
      child: Align(
        alignment: spec.alignment,
        child: ConstrainedBox(
          constraints: BoxConstraints(
            minWidth: spec.fitWidthToContent ? 0 : maxContentWidth,
            maxWidth: maxContentWidth,
            maxHeight: maxContentHeight,
          ),
          child: Align(
            alignment: contentAlignment,
            widthFactor: 1,
            heightFactor: 1,
            child: _MeasureSize(onChange: onMeasured, child: child),
          ),
        ),
      ),
    );
  }
}

enum TvSurfaceScale { compact, standard, expanded }

TvSurfaceScale? _themeScale(Object? rawScale) {
  return switch (rawScale) {
    'compact' || 'small' || 'sm' => TvSurfaceScale.compact,
    'expanded' || 'large' || 'lg' => TvSurfaceScale.expanded,
    'standard' || 'default' || 'md' => TvSurfaceScale.standard,
    _ => null,
  };
}

int _componentCount(List<A2uiMessage> messages) {
  var maxCount = 0;
  for (final message in messages.whereType<UpdateComponents>()) {
    if (message.components.length > maxCount) {
      maxCount = message.components.length;
    }
  }
  return maxCount;
}

TvSurfaceScale _autoSurfaceScale({
  required TvSurfacePattern pattern,
  required int componentCount,
}) {
  if (componentCount <= 0) {
    return TvSurfaceScale.standard;
  }

  return switch (pattern) {
    TvSurfacePattern.centerCard =>
      componentCount <= 8
          ? TvSurfaceScale.compact
          : componentCount >= 18
          ? TvSurfaceScale.expanded
          : TvSurfaceScale.standard,
    TvSurfacePattern.sidePanel =>
      componentCount <= 10
          ? TvSurfaceScale.compact
          : componentCount >= 22
          ? TvSurfaceScale.expanded
          : TvSurfaceScale.standard,
    TvSurfacePattern.topBanner =>
      componentCount <= 6
          ? TvSurfaceScale.compact
          : componentCount >= 12
          ? TvSurfaceScale.expanded
          : TvSurfaceScale.standard,
    TvSurfacePattern.bottomRibbon =>
      componentCount <= 8
          ? TvSurfaceScale.compact
          : componentCount >= 16
          ? TvSurfaceScale.expanded
          : TvSurfaceScale.standard,
    TvSurfacePattern.immersive =>
      componentCount <= 10
          ? TvSurfaceScale.compact
          : componentCount >= 24
          ? TvSurfaceScale.expanded
          : TvSurfaceScale.standard,
  };
}

Alignment _surfaceContentAlignment({
  required TvSurfacePattern pattern,
  required TvSurfaceScale scale,
}) {
  return switch (pattern) {
    TvSurfacePattern.centerCard => Alignment.center,
    TvSurfacePattern.sidePanel =>
      scale == TvSurfaceScale.compact ? Alignment.center : Alignment.topCenter,
    TvSurfacePattern.topBanner ||
    TvSurfacePattern.bottomRibbon => Alignment.centerLeft,
    TvSurfacePattern.immersive =>
      scale == TvSurfaceScale.compact ? Alignment.center : Alignment.topCenter,
  };
}

class _MeasureSize extends SingleChildRenderObjectWidget {
  const _MeasureSize({required this.onChange, required super.child});

  final ValueChanged<Size> onChange;

  @override
  RenderObject createRenderObject(BuildContext context) {
    return _RenderMeasureSize(onChange);
  }

  @override
  void updateRenderObject(
    BuildContext context,
    covariant _RenderMeasureSize renderObject,
  ) {
    renderObject.onChange = onChange;
  }
}

class _RenderMeasureSize extends RenderProxyBox {
  _RenderMeasureSize(this.onChange);

  ValueChanged<Size> onChange;
  Size? _oldSize;

  @override
  void performLayout() {
    super.performLayout();
    final newSize = child?.size;
    if (newSize == null || newSize == _oldSize) {
      return;
    }
    _oldSize = newSize;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      onChange(newSize);
    });
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
