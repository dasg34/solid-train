import 'dart:io';

import 'package:genui/genui.dart';

import '../logging/app_logger.dart';
import 'parse_ndjson.dart';

/// Loads A2UI messages from a local filesystem NDJSON file.
class FilePayloadSource {
  Future<List<A2uiMessage>> loadFile(String filePath) async {
    AppLogger.debug('payload.file', 'Loading external A2UI file: $filePath');

    try {
      final raw = await File(filePath).readAsString();
      final messages = parseNdjson(raw);
      AppLogger.info(
        'payload.file',
        'Loaded ${messages.length} A2UI messages from $filePath',
      );
      return messages;
    } catch (error, stackTrace) {
      AppLogger.error(
        'payload.file',
        'Failed to load external A2UI file: $filePath',
        error: error,
        stackTrace: stackTrace,
      );
      rethrow;
    }
  }
}
