import 'package:genui/genui.dart';

/// Loads A2UI messages for a given scenario.
///
/// Phase 1: reads from local asset files.
/// Phase 2: fetches from HTTP endpoint (drop-in replacement).
abstract class A2uiPayloadSource {
  Future<List<A2uiMessage>> load(String scenarioId);
}
