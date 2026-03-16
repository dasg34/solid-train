import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:openclaw_tv_genui/core/platform/app_control_handler.dart';

void main() {
  group('AppControlHandler', () {
    late AppControlHandler handler;

    setUp(() {
      TestWidgetsFlutterBinding.ensureInitialized();
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(
        const MethodChannel('openclaw/a2ui'),
        (call) async => null,
      );
      handler = AppControlHandler();
    });

    tearDown(() {
      handler.dispose();
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(
        const MethodChannel('openclaw/a2ui'),
        null,
      );
    });

    test('emits file path when loadFile is called via MethodChannel', () async {
      final paths = <String>[];
      handler.onFileReceived.listen(paths.add);

      await TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .handlePlatformMessage(
        'openclaw/a2ui',
        const StandardMethodCodec().encodeMethodCall(
          const MethodCall('loadFile', '/tmp/test.jsonl'),
        ),
        (data) {},
      );

      await Future<void>.delayed(Duration.zero);
      expect(paths, ['/tmp/test.jsonl']);
    });

    test('ignores unknown method calls', () async {
      final paths = <String>[];
      handler.onFileReceived.listen(paths.add);

      await TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .handlePlatformMessage(
        'openclaw/a2ui',
        const StandardMethodCodec().encodeMethodCall(
          const MethodCall('unknownMethod', 'data'),
        ),
        (data) {},
      );

      await Future<void>.delayed(Duration.zero);
      expect(paths, isEmpty);
    });
  });
}
