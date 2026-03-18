import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:genui/genui.dart';
import 'package:openclaw_tv_genui/core/a2ui/a2ui_payload_source.dart';
import 'package:openclaw_tv_genui/core/presentation/presentation_asset_payload_source.dart';

void main() {
  group('PresentationAssetPayloadSource', () {
    test('builds A2UI messages from structured presentation assets', () async {
      final loadedPaths = <String>[];
      final source = PresentationAssetPayloadSource(
        presentationScenarioIds: const {'finance_focus'},
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

    test(
      'delegates unsupported scenarios to the legacy payload source',
      () async {
        final delegate = _FakePayloadSource();
        final source = PresentationAssetPayloadSource(
          presentationScenarioIds: const {'finance_focus'},
          delegate: delegate,
          loadAssetString: (_) async => throw UnimplementedError(),
        );

        final messages = await source.load('weather');

        expect(delegate.loadedScenarioIds, ['weather']);
        expect(messages, isEmpty);
      },
    );
  });
}

class _FakePayloadSource implements A2uiPayloadSource {
  final List<String> loadedScenarioIds = [];

  @override
  Future<List<A2uiMessage>> load(String scenarioId) async {
    loadedScenarioIds.add(scenarioId);
    return <A2uiMessage>[];
  }
}
