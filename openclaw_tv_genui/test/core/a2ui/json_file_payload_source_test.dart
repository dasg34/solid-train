import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:genui/genui.dart';
import 'package:openclaw_tv_genui/core/a2ui/json_file_payload_source.dart';

void main() {
  group('JsonFilePayloadSource', () {
    test('parses NDJSON string into A2uiMessage list', () {
      final ndjson = [
        jsonEncode({
          'version': 'v0.9',
          'createSurface': {
            'surfaceId': 'test_surface',
            'catalogId': 'test_catalog',
          },
        }),
        jsonEncode({
          'version': 'v0.9',
          'updateDataModel': {
            'surfaceId': 'test_surface',
            'value': {'title': 'Test'},
          },
        }),
        jsonEncode({
          'version': 'v0.9',
          'updateComponents': {
            'surfaceId': 'test_surface',
            'components': [
              {'id': 'root', 'component': 'Column', 'children': []},
            ],
          },
        }),
      ].join('\n');

      final messages = parseNdjson(ndjson);

      expect(messages, hasLength(3));
      expect(messages[0], isA<CreateSurface>());
      expect(messages[1], isA<UpdateDataModel>());
      expect(messages[2], isA<UpdateComponents>());
    });

    test('skips empty lines in NDJSON', () {
      final ndjson = [
        jsonEncode({
          'version': 'v0.9',
          'createSurface': {
            'surfaceId': 's',
            'catalogId': 'c',
          },
        }),
        '',
        '',
      ].join('\n');

      final messages = parseNdjson(ndjson);
      expect(messages, hasLength(1));
    });
  });
}
