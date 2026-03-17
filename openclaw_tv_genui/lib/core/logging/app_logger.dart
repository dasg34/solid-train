import 'package:tizen_log/tizen_log.dart';

class AppLogger {
  AppLogger._();

  static const String _tag = 'OpenclawTv';

  static void debug(
    String scope,
    String message, {
    Object? error,
    StackTrace? stackTrace,
  }) {
    _log('DEBUG', scope, message, error: error, stackTrace: stackTrace);
  }

  static void info(
    String scope,
    String message, {
    Object? error,
    StackTrace? stackTrace,
  }) {
    _log('INFO', scope, message, error: error, stackTrace: stackTrace);
  }

  static void warn(
    String scope,
    String message, {
    Object? error,
    StackTrace? stackTrace,
  }) {
    _log('WARN', scope, message, error: error, stackTrace: stackTrace);
  }

  static void error(
    String scope,
    String message, {
    Object? error,
    StackTrace? stackTrace,
  }) {
    _log('ERROR', scope, message, error: error, stackTrace: stackTrace);
  }

  static void _log(
    String level,
    String scope,
    String message, {
    Object? error,
    StackTrace? stackTrace,
  }) {
    final buffer = StringBuffer('[$scope] $message');
    if (error != null) {
      buffer.write(' | error=$error');
    }
    if (stackTrace != null) {
      buffer.write('\n$stackTrace');
    }

    final messageText = buffer.toString();
    switch (level) {
      case 'DEBUG':
        Log.debug(_tag, messageText);
        return;
      case 'INFO':
        Log.info(_tag, messageText);
        return;
      case 'WARN':
        Log.warn(_tag, messageText);
        return;
      case 'ERROR':
        Log.error(_tag, messageText);
        return;
      default:
        Log.verbose(_tag, messageText);
        return;
    }
  }
}
