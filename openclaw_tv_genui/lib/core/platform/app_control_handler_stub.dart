import 'dart:async';

class AppControlHandler {
  final _filePathController = StreamController<String>.broadcast();

  Stream<String> get onFileReceived => _filePathController.stream;

  void dispose() {
    _filePathController.close();
  }
}
