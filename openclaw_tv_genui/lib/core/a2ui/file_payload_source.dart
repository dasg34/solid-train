import 'dart:io';

import 'package:genui/genui.dart';

import 'parse_ndjson.dart';

/// Loads A2UI messages from a local filesystem NDJSON file.
class FilePayloadSource {
  Future<List<A2uiMessage>> loadFile(String filePath) async {
    final raw = await File(filePath).readAsString();
    return parseNdjson(raw);
  }
}
