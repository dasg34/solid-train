import 'dart:async';

import 'package:tizen_app_control/tizen_app_control.dart';

import '../logging/app_logger.dart';
import 'received_presentation_payload.dart';

class AppControlHandler {
  final _payloadController =
      StreamController<ReceivedPresentationPayload>.broadcast();

  Stream<ReceivedPresentationPayload> get onPayloadReceived =>
      _payloadController.stream;

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

          final rawJson = request.extraData['json'];
          if (rawJson is String && rawJson.trim().isNotEmpty) {
            AppLogger.info(
              'app_control',
              'Forwarding presentation JSON from AppControl: ${rawJson.length} chars',
            );
            _payloadController.add(ReceivedPresentationPayload(rawJson: rawJson));
            return;
          }

          final filePath = request.extraData['file'];
          if (filePath is String && filePath.isNotEmpty) {
            AppLogger.info(
              'app_control',
              'Forwarding presentation payload file from AppControl: $filePath',
            );
            _payloadController.add(
              ReceivedPresentationPayload(filePath: filePath),
            );
            return;
          }

          AppLogger.warn(
            'app_control',
            'Received AppControl without a usable presentation "json" or "file" extra.',
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
    _payloadController.close();
  }
}
