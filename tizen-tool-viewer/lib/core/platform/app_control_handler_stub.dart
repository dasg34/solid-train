import 'dart:async';

import 'received_presentation_payload.dart';

class AppControlHandler {
  final _payloadController =
      StreamController<ReceivedPresentationPayload>.broadcast();

  Stream<ReceivedPresentationPayload> get onPayloadReceived =>
      _payloadController.stream;

  void dispose() {
    _payloadController.close();
  }
}
