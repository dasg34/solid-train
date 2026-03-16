import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:genui/genui.dart';
import 'package:openclaw_tv_genui/core/a2ui/file_payload_source.dart';

void main() {
  group('FilePayloadSource', () {
    late Directory tempDir;

    setUp(() {
      tempDir = Directory.systemTemp.createTempSync('file_payload_source_test');
    });

    tearDown(() {
      tempDir.deleteSync(recursive: true);
    });

    test('loads NDJSON from filesystem path', () async {
      final file = File('${tempDir.path}/test.jsonl');
      file.writeAsStringSync([
        jsonEncode({
          'version': 'v0.9',
          'createSurface': {
            'surfaceId': 'test_main',
            'catalogId': 'test_catalog',
          },
        }),
        jsonEncode({
          'version': 'v0.9',
          'updateDataModel': {
            'surfaceId': 'test_main',
            'value': {'title': 'Hello'},
          },
        }),
        jsonEncode({
          'version': 'v0.9',
          'updateComponents': {
            'surfaceId': 'test_main',
            'components': [
              {'id': 'root', 'component': 'Column', 'children': []},
            ],
          },
        }),
      ].join('\n'));

      final source = FilePayloadSource();
      final messages = await source.loadFile(file.path);

      expect(messages, hasLength(3));
      expect(messages[0], isA<CreateSurface>());
      expect(messages[1], isA<UpdateDataModel>());
      expect(messages[2], isA<UpdateComponents>());
    });

    test('throws when file does not exist', () async {
      final source = FilePayloadSource();
      expect(
        () => source.loadFile('${tempDir.path}/nonexistent.jsonl'),
        throwsA(isA<FileSystemException>()),
      );
    });
  });
}
