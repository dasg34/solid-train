import 'dart:async';

import 'received_a2ui_payload.dart';

class AppControlHandler {
  final _payloadController = StreamController<ReceivedA2uiPayload>.broadcast();

  Stream<ReceivedA2uiPayload> get onPayloadReceived => _payloadController.stream;

  void dispose() {
    _payloadController.close();
  }
}
