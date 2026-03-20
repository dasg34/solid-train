import 'app_logger_sink_stub.dart'
    if (dart.library.io) 'app_logger_sink_io.dart'
    as sink;

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

    sink.logToPlatform(tag: _tag, level: level, message: buffer.toString());
  }
}
