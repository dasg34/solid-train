import 'package:flutter/services.dart' show rootBundle;
import 'package:genui/genui.dart';

import '../a2ui/a2ui_payload_source.dart';
import '../logging/app_logger.dart';
import 'presentation_payload_decoder.dart';

typedef AssetStringLoader = Future<String> Function(String assetPath);

class PresentationAssetPayloadSource implements A2uiPayloadSource {
  PresentationAssetPayloadSource({AssetStringLoader? loadAssetString})
    : _loadAssetString = loadAssetString ?? rootBundle.loadString;

  final AssetStringLoader _loadAssetString;

  @override
  Future<List<A2uiMessage>> load(String scenarioId) async {
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
