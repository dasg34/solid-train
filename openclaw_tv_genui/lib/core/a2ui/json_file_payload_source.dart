import 'package:flutter/services.dart' show rootBundle;
import 'package:genui/genui.dart';

import 'a2ui_payload_source.dart';
import '../logging/app_logger.dart';
import 'parse_ndjson.dart';

export 'parse_ndjson.dart' show parseNdjson;

/// Loads A2UI messages from pre-generated NDJSON asset files.
class JsonFilePayloadSource implements A2uiPayloadSource {
  const JsonFilePayloadSource();

  @override
  Future<List<A2uiMessage>> load(String scenarioId) async {
    final assetPath = 'assets/a2ui/$scenarioId.json';
    AppLogger.debug('payload.asset', 'Loading A2UI asset: $assetPath');

    try {
      final raw = await rootBundle.loadString(assetPath);
      final messages = parseNdjson(raw);
      AppLogger.info(
        'payload.asset',
        'Loaded ${messages.length} A2UI messages from $assetPath',
      );
      return messages;
    } catch (error, stackTrace) {
      AppLogger.error(
        'payload.asset',
        'Failed to load A2UI asset: $assetPath',
        error: error,
        stackTrace: stackTrace,
      );
      rethrow;
    }
  }
}
