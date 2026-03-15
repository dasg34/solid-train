import 'dart:convert';

import 'package:flutter/services.dart' show rootBundle;
import 'package:genui/genui.dart';

import 'a2ui_payload_source.dart';

/// Parses an NDJSON string into a list of [A2uiMessage].
List<A2uiMessage> parseNdjson(String ndjson) {
  return ndjson
      .split('\n')
      .where((line) => line.trim().isNotEmpty)
      .map((line) =>
          A2uiMessage.fromJson(jsonDecode(line) as Map<String, Object?>))
      .toList();
}

/// Loads A2UI messages from pre-generated NDJSON asset files.
class JsonFilePayloadSource implements A2uiPayloadSource {
  const JsonFilePayloadSource();

  @override
  Future<List<A2uiMessage>> load(String scenarioId) async {
    final raw = await rootBundle.loadString('assets/a2ui/$scenarioId.json');
    return parseNdjson(raw);
  }
}
