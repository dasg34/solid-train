import 'dart:async';

import 'package:tizen_app_control/tizen_app_control.dart';

import '../logging/app_logger.dart';

class AppControlHandler {
  final _filePathController = StreamController<String>.broadcast();

  Stream<String> get onFileReceived => _filePathController.stream;

  StreamSubscription<ReceivedAppControl>? _sub;

  AppControlHandler() {
    try {
      AppLogger.info('app_control', 'Listening for AppControl events.');
      _sub = AppControl.onAppControl.listen(
        (request) {
          AppLogger.debug(
            'app_control',
            'Received AppControl '
                'operation=${request.operation ?? "-"} '
                'uri=${request.uri ?? "-"} '
                'extraKeys=${request.extraData.keys.toList()}',
          );

          final filePath = request.extraData['file'];
          if (filePath is String && filePath.isNotEmpty) {
            AppLogger.info(
              'app_control',
              'Forwarding payload file from AppControl: $filePath',
            );
            _filePathController.add(filePath);
            return;
          }

          AppLogger.warn(
            'app_control',
            'Received AppControl without a usable "file" extra.',
          );
        },
        onError: (Object error, StackTrace stackTrace) {
          AppLogger.error(
            'app_control',
            'AppControl stream reported an error.',
            error: error,
            stackTrace: stackTrace,
          );
        },
      );
    } catch (error, stackTrace) {
      AppLogger.warn(
        'app_control',
        'AppControl is unavailable on this runtime. Falling back to no-op handler.',
        error: error,
        stackTrace: stackTrace,
      );
    }
  }

  void dispose() {
    AppLogger.info('app_control', 'Disposing AppControl handler.');
    _sub?.cancel();
    _filePathController.close();
  }
}
