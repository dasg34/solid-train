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
  static const String _defaultScenarioId = String.fromEnvironment(
    'OPENCLAW_DEFAULT_SCENARIO',
    defaultValue: 'daily',
  );

  static final ScenarioEntry _fallbackScenario = scenarioCatalog.firstWhere(
    (scenario) => scenario.id == _defaultScenarioId,
    orElse: () => scenarioCatalog.first,
  );

  StreamSubscription<ReceivedA2uiPayload>? _appControlSub;
  String? _externalFilePath;
  String? _externalRawJson;

  @override
  void initState() {
    super.initState();
    AppLogger.info(
      'home',
      'Home screen ready. Fallback scenario=${_fallbackScenario.id}',
    );

    _appControlSub = widget.appControlHandler?.onPayloadReceived.listen((
      payload,
    ) {
      if (payload.hasRawJson) {
        AppLogger.info(
          'home',
          'Switching to external raw JSON mode: ${payload.rawJson!.length} chars',
        );
        setState(() {
          _externalRawJson = payload.rawJson;
          _externalFilePath = null;
        });
        return;
      }

      if (payload.hasFilePath) {
        AppLogger.info(
          'home',
          'Switching to external file mode: ${payload.filePath}',
        );
        setState(() {
          _externalFilePath = payload.filePath;
          _externalRawJson = null;
        });
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    final activeSurface = _externalRawJson != null
        ? GenUiScenarioSurface.raw(
            key: ValueKey(
              'raw:${_externalRawJson.hashCode}:${_externalRawJson!.length}',
            ),
            rawJson: _externalRawJson!,
          )
        : _externalFilePath != null
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
      body: AnimatedSwitcher(
        duration: const Duration(milliseconds: 240),
        switchInCurve: Curves.easeOutCubic,
        switchOutCurve: Curves.easeInCubic,
        child: activeSurface,
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
