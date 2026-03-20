import 'dart:io';

import 'package:genui/genui.dart';

import '../logging/app_logger.dart';
import 'presentation_payload_decoder.dart';

/// Loads presentation JSON from a local filesystem path and converts it
/// into deterministic A2UI messages.
class PresentationFilePayloadSource {
  Future<List<A2uiMessage>> loadFile(String filePath) async {
    AppLogger.debug(
      'payload.presentation.file',
      'Loading external presentation file: $filePath',
    );

    try {
      final file = File(filePath);
      final exists = await file.exists();
      AppLogger.info(
        'payload.presentation.file',
        'External file check path=$filePath exists=$exists',
      );
      if (!exists) {
        AppLogger.error(
          'payload.presentation.file',
          'External presentation file does not exist: $filePath',
        );
      }

      final raw = await file.readAsString();
      final preview = _buildContentPreview(raw);
      AppLogger.debug(
        'payload.presentation.file',
        'Read ${raw.length} characters from $filePath preview="$preview"',
      );

      final messages = decodePresentationMessages(
        raw,
        sourceLabel: 'file',
      );
      AppLogger.info(
        'payload.presentation.file',
        'Built ${messages.length} A2UI messages from $filePath',
      );
      return messages;
    } on FormatException catch (error, stackTrace) {
      AppLogger.error(
        'payload.presentation.file',
        'External presentation file has invalid format: $filePath. '
            'This usually means the file is raw A2UI NDJSON, not presentation JSON.',
        error: error,
        stackTrace: stackTrace,
      );
      rethrow;
    } on FileSystemException catch (error, stackTrace) {
      AppLogger.error(
        'payload.presentation.file',
        'Filesystem error while loading external presentation file: $filePath',
        error: error,
        stackTrace: stackTrace,
      );
      rethrow;
    } catch (error, stackTrace) {
      AppLogger.error(
        'payload.presentation.file',
        'Failed to load external presentation file: $filePath',
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
