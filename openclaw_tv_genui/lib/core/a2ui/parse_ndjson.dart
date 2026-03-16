import 'dart:convert';

import 'package:genui/genui.dart';

/// Parses an NDJSON string into a list of [A2uiMessage].
List<A2uiMessage> parseNdjson(String ndjson) {
  return ndjson
      .split('\n')
      .where((line) => line.trim().isNotEmpty)
      .map((line) =>
          A2uiMessage.fromJson(jsonDecode(line) as Map<String, Object?>))
      .toList();
}
