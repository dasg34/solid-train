import 'package:genui/genui.dart';

/// Loads A2UI messages for a given scenario.
///
/// The current app builds these messages from local presentation assets.
/// Future implementations may fetch presentation payloads over HTTP or
/// streaming transports and still satisfy this interface.
abstract class A2uiPayloadSource {
  Future<List<A2uiMessage>> load(String scenarioId);
}
