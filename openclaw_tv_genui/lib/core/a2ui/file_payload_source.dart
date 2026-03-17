import 'dart:io';

import 'package:genui/genui.dart';

import '../logging/app_logger.dart';
import 'parse_ndjson.dart';

/// Loads A2UI messages from a local filesystem NDJSON file.
class FilePayloadSource {
  Future<List<A2uiMessage>> loadFile(String filePath) async {
    AppLogger.debug('payload.file', 'Loading external A2UI file: $filePath');

    try {
      final file = File(filePath);
      final exists = await file.exists();
      AppLogger.info(
        'payload.file',
        'External file check path=$filePath exists=$exists',
      );
      if (!exists) {
        AppLogger.error(
          'payload.file',
          'External A2UI file does not exist: $filePath',
        );
      }

      final raw = await file.readAsString();
      final preview = _buildContentPreview(raw);
      AppLogger.debug(
        'payload.file',
        'Read ${raw.length} characters from $filePath preview="$preview"',
      );
      final messages = parseNdjson(raw);
      if (messages.isEmpty) {
        AppLogger.warn(
          'payload.file',
          'External A2UI file contains zero messages: $filePath',
        );
      }
      AppLogger.info(
        'payload.file',
        'Loaded ${messages.length} A2UI messages from $filePath',
      );
      return messages;
    } on FormatException catch (error, stackTrace) {
      AppLogger.error(
        'payload.file',
        'External A2UI file has invalid message format: $filePath. '
            'This usually means the file is normalized JSON, not renderable A2UI NDJSON.',
        error: error,
        stackTrace: stackTrace,
      );
      rethrow;
    } on FileSystemException catch (error, stackTrace) {
      AppLogger.error(
        'payload.file',
        'Filesystem error while loading external A2UI file: $filePath',
        error: error,
        stackTrace: stackTrace,
      );
      rethrow;
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

String _buildContentPreview(String raw) {
  final compact = raw.replaceAll('\r', r'\r').replaceAll('\n', r'\n').trim();
  if (compact.length <= 180) {
    return compact;
  }
  return '${compact.substring(0, 180)}...';
}
