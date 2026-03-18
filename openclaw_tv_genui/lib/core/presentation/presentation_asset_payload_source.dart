import 'package:flutter/services.dart' show rootBundle;
import 'package:genui/genui.dart';

import '../a2ui/a2ui_payload_source.dart';
import '../a2ui/json_file_payload_source.dart';
import '../logging/app_logger.dart';
import 'presentation_payload_decoder.dart';

typedef AssetStringLoader = Future<String> Function(String assetPath);

class PresentationAssetPayloadSource implements A2uiPayloadSource {
  PresentationAssetPayloadSource({
    Set<String> presentationScenarioIds = const {
      'finance_focus',
      'weather_today',
    },
    A2uiPayloadSource? delegate,
    AssetStringLoader? loadAssetString,
  }) : _presentationScenarioIds = Set.unmodifiable(presentationScenarioIds),
       _delegate = delegate ?? const JsonFilePayloadSource(),
       _loadAssetString = loadAssetString ?? rootBundle.loadString;

  final Set<String> _presentationScenarioIds;
  final A2uiPayloadSource _delegate;
  final AssetStringLoader _loadAssetString;

  @override
  Future<List<A2uiMessage>> load(String scenarioId) async {
    if (!_presentationScenarioIds.contains(scenarioId)) {
      return _delegate.load(scenarioId);
    }

    final assetPath = 'assets/presentation/$scenarioId.json';
    AppLogger.debug(
      'payload.presentation',
      'Loading presentation asset: $assetPath',
    );

    try {
      final raw = await _loadAssetString(assetPath);
      final messages = decodePresentationMessages(raw, sourceLabel: 'asset');
      AppLogger.info(
        'payload.presentation',
        'Built ${messages.length} A2UI messages from $assetPath',
      );
      return messages;
    } catch (error, stackTrace) {
      AppLogger.error(
        'payload.presentation',
        'Failed to load presentation asset: $assetPath',
        error: error,
        stackTrace: stackTrace,
      );
      rethrow;
    }
  }
}
