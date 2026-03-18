import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:genui/genui.dart';
import 'package:openclaw_tv_genui/core/presentation/presentation_a2ui_builder.dart';
import 'package:openclaw_tv_genui/core/presentation/presentation_surface.dart';
import 'package:openclaw_tv_genui/features/home/models/scenario_entry.dart';

void main() {
  test('all built-in scenarios have valid presentation assets', () {
    for (final scenario in scenarioCatalog) {
      final file = File(
        '${Directory.current.path}/assets/presentation/${scenario.id}.json',
      );

      expect(file.existsSync(), isTrue, reason: 'missing ${scenario.id}.json');

      final decoded = jsonDecode(file.readAsStringSync());
      expect(decoded, isA<Map>(), reason: 'invalid JSON in ${scenario.id}.json');

      final surface = PresentationSurface.fromJson(
        Map<String, Object?>.from(decoded as Map),
      );
      final messages = buildPresentationMessages(surface);

      expect(messages, hasLength(3), reason: 'unexpected message count for ${scenario.id}');
      expect(messages[0], isA<CreateSurface>());
      expect(messages[1], isA<UpdateDataModel>());
      expect(messages[2], isA<UpdateComponents>());
      expect(surface.surfaceId, scenario.surfaceId);
      expect(surface.theme.domain, scenario.domain);
      expect(surface.theme.pattern, scenario.pattern.name);
    }
  });
}
