import 'dart:async';

import 'package:tizen_app_control/tizen_app_control.dart';

/// Listens for incoming Tizen AppControl events and extracts file paths.
///
/// Uses the `tizen_app_control` Dart package directly — no C# bridge needed.
class AppControlHandler {
  final _filePathController = StreamController<String>.broadcast();

  Stream<String> get onFileReceived => _filePathController.stream;

  StreamSubscription<ReceivedAppControl>? _sub;

  AppControlHandler() {
    _sub = AppControl.onAppControl.listen((request) {
      final filePath = request.extraData['file'];
      if (filePath is String && filePath.isNotEmpty) {
        _filePathController.add(filePath);
      }
    });
  }

  void dispose() {
    _sub?.cancel();
    _filePathController.close();
  }
}
