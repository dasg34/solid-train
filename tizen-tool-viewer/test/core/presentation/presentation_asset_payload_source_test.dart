import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:genui/genui.dart';
import 'package:openclaw_tv_genui/core/presentation/presentation_asset_payload_source.dart';

void main() {
  group('PresentationAssetPayloadSource', () {
    test('builds A2UI messages from structured presentation assets', () async {
      final loadedPaths = <String>[];
      final source = PresentationAssetPayloadSource(
        loadAssetString: (assetPath) async {
          loadedPaths.add(assetPath);
          return jsonEncode({
            'surfaceId': 'finance_focus',
            'theme': {'domain': 'finance', 'pattern': 'centerCard'},
            'title': '삼성전자',
            'hero': {'label': '현재가', 'value': '74,300원'},
          });
        },
      );

      final messages = await source.load('finance_focus');

      expect(loadedPaths, ['assets/presentation/finance_focus.json']);
      expect(messages, hasLength(3));
      expect(messages[0], isA<CreateSurface>());
      expect(messages[1], isA<UpdateDataModel>());
      expect(messages[2], isA<UpdateComponents>());
    });
  });
}
