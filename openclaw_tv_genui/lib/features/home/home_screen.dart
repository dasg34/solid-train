import 'dart:async';

import 'package:flutter/material.dart';

import '../../core/a2ui/a2ui_payload_source.dart';
import '../../core/logging/app_logger.dart';
import '../../core/platform/app_control_handler.dart';
import 'models/scenario_entry.dart';
import 'widgets/genui_scenario_surface.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({
    required this.payloadSource,
    this.appControlHandler,
    super.key,
  });

  final A2uiPayloadSource payloadSource;
  final AppControlHandler? appControlHandler;

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  static final ScenarioEntry _fallbackScenario = scenarioCatalog.firstWhere(
    (scenario) => scenario.id == 'daily',
    orElse: () => scenarioCatalog.first,
  );

  StreamSubscription<String>? _appControlSub;
  String? _externalFilePath;

  @override
  void initState() {
    super.initState();
    AppLogger.info(
      'home',
      'Home screen ready. Fallback scenario=${_fallbackScenario.id}',
    );

    _appControlSub = widget.appControlHandler?.onFileReceived.listen((path) {
      AppLogger.info('home', 'Switching to external file mode: $path');
      setState(() {
        _externalFilePath = path;
      });
    });
  }

  @override
  Widget build(BuildContext context) {
    final viewport = _OverlayViewportSpec.resolve(MediaQuery.sizeOf(context));
    final activeSurface = _externalFilePath != null
        ? GenUiScenarioSurface.file(
            key: ValueKey(_externalFilePath),
            filePath: _externalFilePath!,
          )
        : GenUiScenarioSurface.scenario(
            key: const ValueKey('fallback-scenario'),
            scenario: _fallbackScenario,
            payloadSource: widget.payloadSource,
          );

    return Scaffold(
      backgroundColor: Colors.transparent,
      body: SafeArea(
        child: Padding(
          padding: viewport.margin,
          child: Align(
            alignment: viewport.alignment,
            child: SizedBox(
              width: viewport.width,
              height: viewport.height,
              child: AnimatedSwitcher(
                duration: const Duration(milliseconds: 240),
                switchInCurve: Curves.easeOutCubic,
                switchOutCurve: Curves.easeInCubic,
                child: activeSurface,
              ),
            ),
          ),
        ),
      ),
    );
  }

  @override
  void dispose() {
    AppLogger.debug('home', 'Disposing home screen state.');
    _appControlSub?.cancel();
    super.dispose();
  }
}

class _OverlayViewportSpec {
  const _OverlayViewportSpec({
    required this.alignment,
    required this.width,
    required this.height,
    required this.margin,
  });

  final Alignment alignment;
  final double width;
  final double height;
  final EdgeInsets margin;

  static _OverlayViewportSpec resolve(Size size) {
    final phoneLike = size.width < 900;
    final compactTv = !phoneLike && (size.width <= 1280 || size.height <= 720);

    final margin = EdgeInsets.symmetric(
      horizontal: phoneLike ? 16 : (compactTv ? 28 : 40),
      vertical: phoneLike ? 16 : (compactTv ? 24 : 32),
    );

    final width = (size.width * (phoneLike ? 0.90 : (compactTv ? 0.54 : 0.60)))
        .clamp(phoneLike ? 360.0 : 480.0, compactTv ? 620.0 : 920.0);
    final height =
        (size.height * (phoneLike ? 0.82 : (compactTv ? 0.86 : 0.88))).clamp(
          phoneLike ? 320.0 : 420.0,
          compactTv ? 580.0 : 820.0,
        );

    return _OverlayViewportSpec(
      alignment: phoneLike ? Alignment.center : Alignment.centerRight,
      width: width,
      height: height,
      margin: margin,
    );
  }
}
