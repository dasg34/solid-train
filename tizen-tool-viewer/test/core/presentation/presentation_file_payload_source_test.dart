import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:genui/genui.dart';
import 'package:openclaw_tv_genui/core/presentation/presentation_file_payload_source.dart';

void main() {
  group('PresentationFilePayloadSource', () {
    late Directory tempDir;

    setUp(() {
      tempDir = Directory.systemTemp.createTempSync(
        'presentation_file_payload_source_test',
      );
    });

    tearDown(() {
      tempDir.deleteSync(recursive: true);
    });

    test('loads presentation JSON from filesystem path', () async {
      final file = File('${tempDir.path}/presentation.json');
      file.writeAsStringSync(
        '''
{
  "surfaceId": "finance_focus",
  "theme": {"domain": "finance", "pattern": "centerCard"},
  "title": "삼성전자",
  "hero": {"label": "현재가", "value": "74,300원"}
}
''',
      );

      final source = PresentationFilePayloadSource();
      final messages = await source.loadFile(file.path);

      expect(messages, hasLength(3));
      expect(messages[0], isA<CreateSurface>());
      expect(messages[1], isA<UpdateDataModel>());
      expect(messages[2], isA<UpdateComponents>());
    });

    test('throws when file does not exist', () async {
      final source = PresentationFilePayloadSource();
      expect(
        () => source.loadFile('${tempDir.path}/nonexistent.json'),
        throwsA(isA<FileSystemException>()),
      );
    });
  });
}
