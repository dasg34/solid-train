import 'dart:async';

import 'package:flutter/services.dart';

/// Listens for file paths sent from the Tizen C# layer via MethodChannel.
///
/// Sends a "ready" signal on construction so the C# side can flush any
/// pending path that arrived before Flutter was initialised.
class AppControlHandler {
  static const _channel = MethodChannel('openclaw/a2ui');

  final _filePathController = StreamController<String>.broadcast();

  Stream<String> get onFileReceived => _filePathController.stream;

  AppControlHandler() {
    _channel.setMethodCallHandler((call) async {
      if (call.method == 'loadFile') {
        final path = call.arguments as String;
        _filePathController.add(path);
      }
    });

    // Signal the native side that Flutter is ready to receive paths.
    _channel.invokeMethod<void>('ready', null);
  }

  void dispose() => _filePathController.close();
}
