import 'dart:developer' as developer;

import 'package:tizen_log/tizen_log.dart';

void logToPlatform({
  required String tag,
  required String level,
  required String message,
}) {
  try {
    switch (level) {
      case 'DEBUG':
        Log.debug(tag, message);
        return;
      case 'INFO':
        Log.info(tag, message);
        return;
      case 'WARN':
        Log.warn(tag, message);
        return;
      case 'ERROR':
        Log.error(tag, message);
        return;
      default:
        Log.verbose(tag, message);
        return;
    }
  } catch (error, stackTrace) {
    developer.log(
      message,
      name: '$tag/$level',
      error: error,
      stackTrace: stackTrace,
    );
  }
}
