import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:openclaw_tv_genui/core/platform/app_control_handler.dart';

// AppControlHandler depends on tizen_app_control which uses EventChannel
// internally. Tests need binding initialised and the EventChannel mocked.

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  group('AppControlHandler', () {
    test('onPayloadReceived is a broadcast stream', () {
      final handler = AppControlHandler();
      final sub1 = handler.onPayloadReceived.listen((_) {});
      final sub2 = handler.onPayloadReceived.listen((_) {});

      // Both subscriptions active = broadcast stream works.
      sub1.cancel();
      sub2.cancel();
      handler.dispose();
    });

    test('dispose closes the stream', () async {
      final handler = AppControlHandler();
      handler.dispose();

      final completer = Completer<bool>();
      handler.onPayloadReceived.listen(
        (_) {},
        onDone: () => completer.complete(true),
      );

      expect(await completer.future, isTrue);
    });
  });
}
