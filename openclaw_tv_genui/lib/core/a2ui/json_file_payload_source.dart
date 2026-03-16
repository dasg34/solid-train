import 'package:flutter/services.dart' show rootBundle;
import 'package:genui/genui.dart';

import 'a2ui_payload_source.dart';
import 'parse_ndjson.dart';

export 'parse_ndjson.dart' show parseNdjson;

/// Loads A2UI messages from pre-generated NDJSON asset files.
class JsonFilePayloadSource implements A2uiPayloadSource {
  const JsonFilePayloadSource();

  @override
  Future<List<A2uiMessage>> load(String scenarioId) async {
    final raw = await rootBundle.loadString('assets/a2ui/$scenarioId.json');
    return parseNdjson(raw);
  }
}
